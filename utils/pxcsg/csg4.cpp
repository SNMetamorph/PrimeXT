/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "csg.h"

int	c_outfaces;
int	g_firstbrush;

/*
===========
AllocFace
===========
*/
bface_t *AllocFace( void )
{
	bface_t	*f;

	f = (bface_t *)Mem_Alloc( sizeof( bface_t ), C_SURFACE );
//	ClearBounds( f->mins, f->maxs );
	f->planenum = -1;
//	f->texinfo = -1;

	return f;
}

/*
==================
FreeFace
==================
*/
void FreeFace( bface_t *f )
{
	if( !f ) return;

	if( f->w ) FreeWinding( f->w );
	Mem_Free( f, C_SURFACE );
}

/*
==================
NewFaceFromFace

Duplicates the non point information of a face, used by SplitFace
==================
*/
bface_t *NewFaceFromFace( const bface_t *in )
{
	bface_t	*newf;
	
	newf = AllocFace();
	newf->contents[0] = in->contents[0];
	newf->contents[1] = in->contents[1];
	newf->planenum = in->planenum;
	newf->texinfo = in->texinfo;
	newf->plane = in->plane;
	newf->flags = in->flags;

	return newf;
}

/*
==================
CopyFace
==================
*/
bface_t *CopyFace( const bface_t *f )
{
	bface_t	*n;

	n = NewFaceFromFace( f );
	n->w = CopyWinding( f->w );
	VectorCopy( f->mins, n->mins );
	VectorCopy( f->maxs, n->maxs );

	return n;
}

/*
==================
CopyFacesToOutside

Make a copy of all the faces of the brush, so they
can be chewed up by other brushes.

All of the faces start on the outside list.
As other brushes take bites out of the faces, the
fragments are moved to the inside list, so they
can be freed when they are determined to be
completely enclosed in solid.
==================
*/
bface_t *CopyFacesToOutside( brush_t *b, int num )
{
	bface_t	*f, *newf;
	bface_t	*outside;

	outside = NULL;

	for( f = b->hull[num].faces; f != NULL; f = f->next )
	{
		newf = CopyFace( f );
		WindingBounds( newf->w, newf->mins, newf->maxs );
		newf->contents[1] = b->contents;
		newf->next = outside;
		outside = newf;
	}

	return outside;
}

/*
==================
UnlinkFaces

release specified face or purge all chain
==================
*/
void UnlinkFaces( bface_t **head, bface_t *face )
{
	bface_t	**prev = head;
	bface_t	*cur;

	while( 1 )
	{
		cur = *prev;
		if( !cur ) break;

		if( face != NULL && face != cur )
		{
			prev = &cur->next;
			continue;
		}

		// unlink face from list
		*prev = cur->next;
		FreeFace( cur );
	}
}

//============================================================
/*
==================
SaveOutside

The faces remaining on the outside list are final
polygons.  Write them to the output file.

Passable contents (water, lava, etc) will generate
a mirrored copy of the face to be seen from the inside.
==================
*/
static void SaveOutside( brush_t *b, int hull, bface_t *outside )
{
	bface_t	*f, *f2, *next;

	for( f = outside; f != NULL; f = next )
	{
		int	frontcontents = f->contents[0];
		int	texinfo = f->texinfo;
		bool	frontnull = false;
		bool	backnull = false;
		int	backcontents;

		next = f->next;

		if( b->contents == CONTENTS_EMPTY )
			backcontents = f->contents[1];
		else backcontents = b->contents;

		if( b->contents == CONTENTS_EMPTY )
		{
			// SKIP and HINT are special textures for hlbsp
			if( !FBitSet( f->flags, FSIDE_HINT|FSIDE_SKIP|FSIDE_SOLIDHINT ))
				backnull = true;
		}

		if( FBitSet( f->flags, FSIDE_SOLIDHINT ))
		{
			if( frontcontents != backcontents )
				frontnull = backnull = true; // not discardable, so remove "SOLIDHINT" texture name and behave like NULL
		}

		if( b->entitynum != 0 && IsLiquidContents( f->contents[0] ))
			backnull = true; // strip water face on one side

		if( WindingArea( f->w ) <= 0.0 )
		{
			MsgDev( D_WARN, "Entity %i, Brush %i: tiny fragment\n", b->originalentitynum, b->originalbrushnum );
			FreeFace( f );
			continue;
		}

		if( hull == HULL_VISIBLE )
		{
			// count unique faces for visible hull
			for( f2 = b->hull[hull].faces; f2 != NULL; f2 = f2->next )
			{
				if( f2->planenum == f->planenum )
				{
					if( !FBitSet( f2->flags, FSIDE_USED ))
					{
						SetBits( f2->flags, FSIDE_USED );
						c_outfaces++;
					}
					break;
				}
			}
		}

		f->contents[0] = frontcontents;
		f->texinfo = frontnull ? -1 : texinfo;

		// save front face
		if( hull == HULL_VISIBLE )
			EmitFace( hull, f, b->detaillevel );
		else EmitFace( hull, f, 0 );

		// filp face
		f->planenum ^= 1;	// reverse side
		f->plane = &g_mapplanes[f->planenum];
		f->contents[0] = backcontents;
		f->texinfo = backnull ? -1 : texinfo;
		ReverseWinding( &f->w );

		// save back face
		if( hull == HULL_VISIBLE )
			EmitFace( hull, f, b->detaillevel );
		else EmitFace( hull, f, 0 );

		FreeFace( f );
	}
}

//==========================================================================
/*
===========
CSGBrush
===========
*/
static void CSGBrush( int brushnum, int threadnum = -1 )
{
	bface_t		*f, *f2, *next;
	brushhull_t	*bh1, *bh2;
	bool		overwrite;
	brush_t		*b1, *b2;
	bface_t		*outside;
	vec_t		area;
	mapent_t		*e;

	brushnum = g_firstbrush + brushnum;
	b1 = &g_mapbrushes[brushnum];
	e = &g_mapentities[b1->entitynum];

	// for each of the hulls
	for( int hull = 0; hull < MAX_MAP_HULLS; hull++ )
	{
		bh1 = &b1->hull[hull];

		// g-cont. if brush detaillevel is 0 it's not a detailbrush!
		if( bh1->faces && b1->csg_detaillevel( hull ) > 0 )
		{
			switch( b1->contents )
			{
			case CONTENTS_EMPTY:
				break;
			case CONTENTS_SOLID:
				EmitDetailBrush( hull, bh1->faces );
				break;
			default:	// g-cont. detail brushes can't be a liquid
				COM_FatalError( "Entity %i, Brush %i: not allowed in detail\n", b1->originalentitynum, b1->originalbrushnum );
				break;
			}
		}

		// set outside to a copy of the brush's faces
		outside = CopyFacesToOutside( b1, hull );
		overwrite = false;

		if( b1->contents == CONTENTS_EMPTY )
		{
			for( f = outside; f != NULL; f = f->next )
			{
				f->contents[0] = CONTENTS_EMPTY;
				f->contents[1] = CONTENTS_EMPTY;
			}
		}

		// for each brush in entity e
		for( int bn = 0; bn < e->numbrushes; bn++ )
		{
			// see if b2 needs to clip a chunk out of b1
			if( e->firstbrush + bn == brushnum )
				continue;

			overwrite = (e->firstbrush + bn > brushnum) ? true : false;
			b2 = &g_mapbrushes[e->firstbrush + bn];
			bh2 = &b2->hull[hull];

			if( b2->contents == CONTENTS_EMPTY )
				continue;

			if( b2->csg_detaillevel( hull ) > b1->csg_detaillevel( hull ))
				continue; // you can't chop

			// check if brush1 can overwrite brush2
			if( b2->contents == b1->contents )
			{
				if( b1->csg_detaillevel( hull ) != b2->csg_detaillevel( hull ))
				{
					overwrite = (b2->csg_detaillevel( hull ) < b1->csg_detaillevel( hull )) ? true : false;
				}
			}

			if(( !bh2->faces ) || ( hull == 0 && FBitSet( b2->flags, FBRUSH_NOCSG )))
				continue; // brush isn't in this hull

			if( !BoundsIntersect( bh1->mins, bh1->maxs, bh2->mins, bh2->maxs ))
				continue;

			// divide faces by the planes of the b2 to find which
			// fragments are inside
			for( f = outside, outside = NULL; f != NULL; f = next )
			{
				next = f->next;

				if( !BoundsIntersect( bh2->mins, bh2->maxs, f->mins, f->maxs ))
				{
					// this face doesn't intersect brush2's bbox
					f->next = outside;
					outside = f;
					continue;
				}

				if( b2->csg_detaillevel( hull ) > b1->csg_detaillevel( hull ))
				{
					if( FBitSet( f->flags, FSIDE_NODRAW|FSIDE_SKIP|FSIDE_HINT|FSIDE_SOLIDHINT ))
					{
						// should not nullify the fragment inside detail brush
						f->next = outside;
						outside = f;
						continue;
					}
				}

				// throw pieces on the front sides of the planes
				// into the outside list, return the remains on the inside

				// find the fragment inside brush2
				winding_t *w = CopyWinding( f->w );

				for( f2 = bh2->faces; f2 != NULL; f2 = f2->next )
				{
					if( f->planenum == f2->planenum )
					{
						if( !overwrite || FBitSet( f2->flags, FSIDE_NODRAW ))
						{
							// face plane is outside brush2
							FreeWinding( w );
							w = NULL;
							break;
						}
						else
						{
							continue;
						}
					}

					if( f->planenum == ( f2->planenum ^ 1 ))
					{
#if 0
						if( FBitSet( f2->flags, FSIDE_NODRAW ))
						{
							// face plane is outside brush2
							FreeWinding( w );
							w = NULL;
							break;
						}
						else
#endif
						{
							continue;
						}
					}

					winding_t	*fw, *bw;

					ClipWindingEpsilon( w, f2->plane->normal, f2->plane->dist, g_csgepsilon, &fw, &bw );

					if( fw )
					{
						FreeWinding( fw );
					}

					if( bw )
					{
						FreeWinding( w );
						w = bw;
					}
					else
					{
						FreeWinding( w );
						w = NULL;
						break;
					}
				}

				// do real split
				if( w != NULL )
				{
					for( f2 = bh2->faces; f2 != NULL; f2 = f2->next )
					{
						if( f->planenum == f2->planenum || f->planenum == ( f2->planenum ^ 1 ))
						{
							continue;
						}

						int valid = 0;

						for( int x = 0; x < w->numpoints; x++ )
						{
							vec_t dist = DotProduct( w->p[x], f2->plane->normal ) - f2->plane->dist;

							if( dist >= -g_csgepsilon * 4 ) // only estimate
								valid++;
						}

						if( valid >= 2 )
						{
							// this splitplane forms an edge
							winding_t	*fw, *bw;

							ClipWindingEpsilon( f->w, f2->plane->normal, f2->plane->dist, g_csgepsilon, &fw, &bw );

							if( fw )
							{
								bface_t *front = NewFaceFromFace( f );
								front->w = fw;
								WindingBounds( fw, front->mins, front->maxs );
								front->next = outside;
								outside = front;
							}

							if( bw )
							{
								FreeWinding( f->w );
								WindingBounds( bw, f->mins, f->maxs );
								f->w = bw;
							}
							else
							{
								FreeFace( f );
								f = NULL;
								break;
							}
						}
					}

					FreeWinding( w );
				}
				else
				{
					f->next = outside;
					outside = f;
					f = NULL;
				}

				area = f ? WindingArea( f->w ) : 0.0;

				if( f && area <= 0.0 )
				{
					MsgDev( D_WARN, "Entity %i, Brush %i: tiny penetration\n", b1->originalentitynum, b1->originalbrushnum );
					FreeFace( f );
					f = NULL;
				}

				if( f )
				{
					// there is one convex fragment of the original
					// face left inside brush2
					if( b2->csg_detaillevel( hull ) > b1->csg_detaillevel( hull ))
					{
						// don't chop or set contents, only nullify
						f->next = outside;
						outside = f;
						SetBits( f->flags, FSIDE_NODRAW );
						f->texinfo = -1;
						continue;
					}

					if( b2->csg_detaillevel( hull ) < b1->csg_detaillevel( hull ) && b2->contents == CONTENTS_SOLID )
					{
						// real solid
						FreeFace( f );
						continue;
					}

					if( b1->contents == CONTENTS_EMPTY )
					{
						bool	onfront = true;
						bool	onback = true;

						for( f2 = bh2->faces; f2 != NULL; f2 = f2->next )
						{
							if( f->planenum == ( f2->planenum ^ 1 ))
								onback = false;
							if( f->planenum == f2->planenum )
								onfront = false;
						}

						if( onfront && f->contents[0] < b2->contents )
							f->contents[0] = b2->contents;

						if( onback && f->contents[1] < b2->contents )
							f->contents[1] = b2->contents;

						if( f->contents[0] == CONTENTS_SOLID && f->contents[1] == CONTENTS_SOLID && FBitSet( f->flags, FSIDE_SOLIDHINT ))
						{
							FreeFace( f );
						}
						else
						{
							f->next = outside;
							outside = f;
						}
						continue;
					}

					if( b1->contents > b2->contents || ( b1->contents == b2->contents && FBitSet( f->flags, FSIDE_SOLIDHINT )))
					{	
						// inside a water brush
						f->contents[0] = b2->contents;
						f->next = outside;
						outside = f;
					}
					else
					{
						// inside a solid brush
						FreeFace( f ); // throw it away
					}
				}
			}

		}

		if( hull == HULL_VISIBLE )
			WriteMapBrushes( b1, outside );

		// all of the faces left in outside are real surface faces
		SaveOutside( b1, hull, outside );
	}
}

/*
==================
ChopEntityBrushes

Chop brushes for a gived entity
and dump result faces into text-format files
that called mapname.p0-3
==================
*/
void ChopEntityBrushes( mapent_t *mapent )
{
	ASSERT( mapent->numbrushes > 0 );

	// sort the contents down so stone bites water, etc
	g_firstbrush = mapent->firstbrush;

	// csg them in order
	if( mapent == &g_mapentities[0] )
	{
		RunThreadsOnIndividual( mapent->numbrushes, true, CSGBrush );
	}
	else
	{
		// brushmodels use silent threads
		RunThreadsOnIndividual( mapent->numbrushes, false, CSGBrush );
	}
}