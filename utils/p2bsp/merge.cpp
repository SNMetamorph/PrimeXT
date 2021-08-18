/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// merge.c

#include "bsp5.h"

/*
================
CheckColinear

================
*/
void CheckColinear( winding_t *w )
{
	vec3_t	v1, v2;
	int	i, j;
	
	for( i = 0; i < w->numpoints; i++ )
	{
		// skip the point if the vector from the previous point is the same
		// as the vector to the next point
		j = (i - 1 < 0) ? w->numpoints - 1 : i - 1;
		VectorSubtract( w->p[i], w->p[j], v1 );
		VectorNormalize( v1 );
		
		j = (i + 1 == w->numpoints) ? 0 : i + 1;
		VectorSubtract( w->p[j], w->p[i], v2 );
		VectorNormalize( v2 );
		
		if( VectorCompareEpsilon( v1, v2, EQUAL_EPSILON ))
			COM_FatalError( "colinear edge\n" );			
	}
}

/*
=============
FacesCanMerge

check if two faces can't be merged into one
for some reasons
=============
*/
bool FacesCanMerge( face_t *f1, face_t *f2 )
{
	// one of faces is not valid
	if( !f1->w || !f2->w )
		return false;

	// different texinfo
	if( f1->texturenum != f2->texturenum )
		return false;

	// different contents (in clipping hulls)
	if( f1->contents != f2->contents )
		return false;

	// different plane
	if( f1->planenum != f2->planenum )
		return false;

	// unmatched detail level
	if( f1->detaillevel != f2->detaillevel )
		return false;

	// different facestyle (g-cont. how this possible?)
	if( f1->facestyle != f2->facestyle )
		return false;

	return true;
}

/*
=============
TryMerge

If two polygons share a common edge and the edges that meet at the
common points are both inside the other polygons, merge them

Returns NULL if the faces couldn't be merged, or the new face.
The originals will NOT be freed.
=============
*/
face_t *TryMerge( face_t *f1, face_t *f2 )
{
	face_t	*newf;
	winding_t	*mergew;
	plane_t	*plane;

	if( !FacesCanMerge( f1, f2 ))
		return NULL;

	plane = &g_mapplanes[f1->planenum];
	mergew = TryMergeWindingEpsilon( f1->w, f2->w, plane->normal, ON_EPSILON );

	if( !mergew ) return NULL; // faces can't be merged

	newf = NewFaceFromFace( f1 );
	newf->w = mergew;

	return newf;
}

/*
=============
TryBite

=============
*/
bool TryBite( face_t *f1, face_t *f2, int maxbites )
{
	plane_t	*plane;

	// perform general checks
	if( f2->bite >= maxbites )
		return false;

	if( !FacesCanMerge( f1, f2 ))
		return false;

	plane = &g_mapplanes[f1->planenum];

	if( !BiteWindingEpsilon( &f1->w, &f2->w, plane->normal, ON_EPSILON ))
		return false;

	f2->bite++;

	return true;
}

/*
===============
MergeFaceToList
===============
*/
face_t *MergeFaceToList( face_t *face, face_t *list )
{	
	face_t	*newf, *f;
	
	for( f = list; f != NULL; f = f->next )
	{
		// CheckColinear( f->w );		
		newf = TryMerge( face, f );
		if( !newf ) continue;

		FreeFace( face );
		FreeWinding( f->w );
		f->w = NULL; // merged out

		return MergeFaceToList( newf, list );
	}
	
	// didn't merge, so add at start
	face->next = list;
	return face;
}

face_t *SplitAndMergeFaceToList( face_t **list, int maxbites )
{
	face_t	*next, *next2;
	face_t	*merge = NULL;
	face_t	*f1, *f2;

	for( f1 = *list; f1 != NULL; f1 = next )
	{
		bool	bite = false;

		// try to bite faces in merged list
		for( f2 = merge; f2 != NULL; f2 = next2 )
		{
			next2 = f2->next;

			if( !TryBite( f1, f2, maxbites ))
				continue;

			UnlinkFace( &merge, f2 );

			merge = MergeFaceToList( f2, merge );
			bite = true;
			break;
		}

		if( !bite )
		{
			// try to bite faces in face list
			for( f2 = *list; f2 != NULL; f2 = next2 )
			{
				next2 = f2->next;
				if( f1 == f2 ) continue;

				if( !TryBite( f1, f2, maxbites ))
					continue;

				UnlinkFace( list, f2 );

				merge = MergeFaceToList( f2, merge );
				bite = true;
				break;
			}
		}

		next = f1->next;

		if( bite )
		{
			UnlinkFace( list, f1 );
			merge = MergeFaceToList( f1, merge );
		}
	}

	// add all remaining faces
	for( f1 = *list; f1 != NULL; f1 = next )
	{
		next = f1->next;
		merge = MergeFaceToList( f1, merge );
	}

	return merge;
}

/*
===============
FreeMergeListScraps
===============
*/
face_t *FreeMergeListScraps( face_t *merged )
{
	face_t	*head, *next;
	
	for( head = NULL; merged != NULL; merged = next )
	{
		next = merged->next;

		if( merged->w == NULL )
		{
			FreeFace( merged );
		}
		else
		{
			merged->next = head;
			head = merged;
		}
	}

	return head;
}

/*
===============
MergePlaneFaces
===============
*/
void MergePlaneFaces( surface_t *plane, int mergelevel )
{
	face_t	*f1, *f2, *next;
	face_t	*merged, *backup;

	if( mergelevel <= 0 )
		return;
	
	merged = backup = NULL;
	
	for( f1 = plane->faces; f1 != NULL; f1 = next )
	{
		next = f1->next;
		merged = MergeFaceToList( f1, merged );
	}

	if( mergelevel > 1 )
	{
		// chain all of the non-empty faces to the plane
		plane->faces = FreeMergeListScraps( merged );

		// try split-n-merge approach
		// make copy of all faces
		for( f1 = plane->faces; f1 != NULL; f1 = f1->next )
		{
			f2 = NewFaceFromFace( f1 );
			f2->w = CopyWinding( f1->w );
			f2->original = f1;

			f2->next = backup;
			backup = f2;
		}

		merged = SplitAndMergeFaceToList( &plane->faces, mergelevel * 2 );

		// sanity check
		if( CountListFaces( merged ) >= CountListFaces( backup ))
		{
			// free mergelist
			for( f1 = merged; f1 != NULL; f1 = next )
			{
				next = f1->next;
				FreeFace( f1 );
			}

			// assign original chain
			plane->faces = FreeMergeListScraps( backup );
		}
		else
		{
			// free originallist
			for( f1 = backup; f1 != NULL; f1 = next )
			{
				next = f1->next;
				FreeFace( f1 );
			}

			// assign merged chain
			plane->faces = FreeMergeListScraps( merged );
		}
	}
	else
	{
		// chain all of the non-empty faces to the plane
		plane->faces = FreeMergeListScraps( merged );
	}
}

/*
============
MergeTreeFaces
============
*/
void MergeTreeFaces( tree_t *tree )
{
	surface_t	*surf;
	int	mergefaces;
	int	origfaces;
	int	reduced;

	MsgDev( D_REPORT, "---- MergeTreeFaces ----\n" );

	mergefaces = origfaces = 0;

	for( surf = tree->surfaces; surf; surf = surf->next )
	{
		origfaces += CountListFaces( surf->faces );
		MergePlaneFaces( surf, 1 );
		mergefaces += CountListFaces( surf->faces );
	}

	reduced = origfaces - mergefaces;
	if( reduced ) MsgDev( D_REPORT, "%i faces reduced\n", reduced );
}