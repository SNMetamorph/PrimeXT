/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "bsp5.h"

//===========================================================================

/*
===========
AllocTree
===========
*/
tree_t *AllocTree( void )
{
	tree_t	*t;

	t = (tree_t *)Mem_Alloc( sizeof( tree_t ), C_BSPTREE );
	ClearBounds( t->mins, t->maxs );

	return t;
}

/*
===========
FreeTree
===========
*/
void FreeTree( tree_t *t )
{
	Mem_Free( t, C_BSPTREE );
}

// =====================================================================================
//  CheckFaceForHint
//      Returns true if the passed face is facetype hint
// =====================================================================================
bool CheckFaceForHint( const face_t *f )
{
	const char *name = GetTextureByTexinfo( f->texturenum );

	if( !Q_strnicmp( name, "HINT", 4 ))
		return true;
	return false;
}

// =====================================================================================
//  CheckFaceForHint
//      Returns true if the passed face is facetype solidhint
// =====================================================================================
bool CheckFaceForDiscardable( const face_t *f )
{
	const char *name = GetTextureByTexinfo( f->texturenum );

	if( !Q_strnicmp( name, "SOLIDHINT", 9 ))
		return true;
	return false;
}

static facestyle_e SetFaceType( face_t *f )
{
	if( CheckFaceForHint( f ))
		f->facestyle = face_hint;
	else if( CheckFaceForDiscardable( f ))
		f->facestyle = face_discardable;
	return f->facestyle;
}

/*
===============
SurflistFromValidFaces
===============
*/
void MakeSurflistFromValidFaces( tree_t *tree )
{
	face_t	*f, *next;
	surface_t	*n;

	tree->surfaces = NULL;	
	ClearBounds( tree->mins, tree->maxs );
	tree->numfaces = 0;

	// grab planes from both sides
	for( int i = 0; i < g_nummapplanes; i += 2 )
	{
		if( !tree->validfaces[i+0] && !tree->validfaces[i+1] )
			continue;

		n = AllocSurface();
		n->next = tree->surfaces;
		n->detaillevel = -1;
		tree->surfaces = n;
		tree->numsurfaces++;
		n->planenum = i;
		n->faces = NULL;

		for( f = tree->validfaces[i+0]; f != NULL; f = next )
		{
			AddFaceToBounds( f, n->mins, n->maxs );

			if( n->detaillevel == -1 || f->detaillevel < n->detaillevel )
				n->detaillevel = f->detaillevel;

			next = f->next;
			f->next = n->faces;
			n->faces = f;
			tree->numfaces++;
		}

		for( f = tree->validfaces[i+1]; f != NULL; f = next )
		{
			AddFaceToBounds( f, n->mins, n->maxs );

			if( n->detaillevel == -1 || f->detaillevel < n->detaillevel )
				n->detaillevel = f->detaillevel;

			next = f->next;
			f->next = n->faces;
			n->faces = f;
			tree->numfaces++;
		}

		// update tree size
		AddPointToBounds( n->mins, tree->mins, tree->maxs );
		AddPointToBounds( n->maxs, tree->mins, tree->maxs );

		tree->validfaces[i+0] = NULL;
		tree->validfaces[i+1] = NULL;
	}

	// merge polygons
	MergeTreeFaces( tree );
}

/*
===============
MakeTreeFromHullFaces
===============
*/
tree_t *MakeTreeFromHullFaces( FILE *polyfile, FILE *brushfile )
{
	int	texinfo, contents, numpoints;
	int	r, planenum, detaillevel;
	tree_t	*tree = NULL;
	bool	skipface;
	int	line = 0;
	double	v[3];
	face_t	*f;

	// read in the polygons
	while( 1 )
	{
		r = fscanf( polyfile, "%i %i %i %i %i\n", &detaillevel, &planenum, &texinfo, &contents, &numpoints );
		skipface = false;
		line++;

		if( r == 0 || r == -1 )
			return NULL;

		// alloc a new tree for model
		if( !tree ) tree = AllocTree();

		if( planenum == -1 ) // end of model
			break;

		if( r != 5 )
			COM_FatalError( "ReadSurfs (line %i): scanf failure %d != 5\n", line, r );

		if( planenum > g_nummapplanes )
			COM_FatalError( "ReadSurfs (line %i): %i > numplanes\n", line, planenum );

		if( texinfo > g_numtexinfo )
			COM_FatalError( "ReadSurfs (line %i): %i > numtexinfo\n", line, texinfo );

		if( detaillevel < 0 )
			COM_FatalError( "ReadSurfs (line %i): detaillevel %i < 0", line, detaillevel);

		if( !Q_stricmp( GetTextureByTexinfo( texinfo ), "SKIP" ))
			skipface = true;

		if( !skipface )
		{
			f = AllocFace ();
			f->planenum = planenum;
			f->texturenum = texinfo;
			f->contents = contents;
			f->detaillevel = detaillevel;
			f->w = AllocWinding( numpoints );
			f->w->numpoints = numpoints;
			f->next = tree->validfaces[planenum];
			tree->validfaces[planenum] = f;
			SetFaceType( f );
		}

		// restore winding
		for( int i = 0; i < numpoints; i++ )
		{
			r = fscanf( polyfile, "%lf %lf %lf\n", &v[0], &v[1], &v[2] );
			if( !skipface ) VectorCopy( v, f->w->p[i] );
			line++;
		}

		fscanf( polyfile, "\n" );
		line++;
	}

	MakeSurflistFromValidFaces( tree );

	// time to read detailbrushes
	tree->detailbrushes = ReadBrushes( brushfile );

	return tree;
}

/*
===============
TreeProcessModel
===============
*/
tree_t *TreeProcessModel( tree_t *tree, int modnum, int hullnum )
{
	bool	worldmodel = (modnum == 0);

	if( hullnum == 0 )
	{
		// if not the world, make a good tree first
		// the world is just going to make a bad tree
		// because the outside filling will force a regeneration later
		SolidBSP( tree, modnum, hullnum ); // SolidBSP generates a node tree

		if( worldmodel && !g_nofill )
		{
			FillInside( tree->headnode );
			FillOutside( tree, hullnum, true );
		}

		FreeTreePortals( tree );

		// fix tjunctions
		tjunc( tree->headnode, worldmodel );

		MakeFaceEdges();
	}
	else
	{
		// clipping tree are simply
		SolidBSP( tree, modnum, hullnum );

		// assume non-world bmodels are simple
		if( worldmodel && !g_nofill && !g_noclip )
			FillOutside( tree, hullnum, false );

		FreeTreePortals( tree );
	}

	return tree;
}