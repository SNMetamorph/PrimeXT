/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "qvis.h"
#include "threads.h"

/*
Some textures (sky, water, slime, lava) are considered ambient sound emiters.
Find an aproximate distance to the nearest emiter of each class for each leaf.
*/

/*
====================
SurfaceBBox
====================
*/
void SurfaceBBox( dface_t *s, vec3_t mins, vec3_t maxs )
{
	int	e, vi;
	vec3_t	v;

	ClearBounds( mins, maxs );

	for( int i = 0; i < s->numedges; i++ )
	{
		e = g_dsurfedges[s->firstedge + i];

		if( e >= 0 ) vi = g_dedges[e].v[0];
		else vi = g_dedges[-e].v[1];

		VectorCopy( g_dvertexes[vi].point, v );
		AddPointToBounds( v, mins, maxs );
	}
}

/*
====================
CalcAmbientSounds

====================
*/
static void CalcAmbientSounds( int leafnum, int threadnum = -1 )
{
	int	j, k, l;
	int	ambient_type;
	dleaf_t	*leaf, *hit;
	byte	vis[(MAX_MAP_LEAFS + 7) / 8];
	vec_t	dists[NUM_AMBIENTS];
	vec_t	d, maxd, vol;
	vec3_t	mins, maxs;
	dface_t	*surf;

 	leaf = &g_dleafs[leafnum+1];

	// clear ambients
	for( j = 0; j < NUM_AMBIENTS; j++ )
		dists[j] = BOGUS_RANGE;

	DecompressVis( &g_dvisdata[leaf->visofs], vis );
		
	for( j = 0; j < g_numvisleafs; j++ )
	{
		if( !CHECKVISBIT( vis, j ))
			continue;
		
		// check this leaf for sound textures
		hit = &g_dleafs[j+1];

		for( k = 0; k < hit->nummarksurfaces; k++ )
		{
			surf = &g_dfaces[g_dmarksurfaces[hit->firstmarksurface + k]];
			const char *texname = GetTextureByTexinfo( surf->texinfo );

			if( !Q_strnicmp( texname, "sky", 3 ))
			{
				ambient_type = AMBIENT_SKY;
			}
			else if( texname[0] == '!' )
			{
				if( !Q_strnicmp( texname, "!water", 6 ))
				{
					ambient_type = AMBIENT_WATER;
				}
				else if( !Q_strnicmp( texname, "!04water", 8 ))
				{
					ambient_type = AMBIENT_WATER;
				}
				else if( !Q_strnicmp( texname, "!slime", 6 ))
				{
					ambient_type = AMBIENT_SLIME;
				}
				else if( !Q_strnicmp( texname, "!lava", 6 ))
				{
					ambient_type = AMBIENT_LAVA;
				}
				else 
				{
					continue;
				}
			}
			else
			{
				continue;
			}

			// find distance from source leaf to polygon
			SurfaceBBox( surf, mins, maxs );
			maxd = 0;

			for( l = 0; l < 3; l++ )
			{
				if( mins[l] > leaf->maxs[l] )
					d = mins[l] - leaf->maxs[l];
				else if( maxs[l] < leaf->mins[l] )
					d = leaf->mins[l] - mins[l];
				else d = 0;

				maxd = Q_max( d, maxd );
			}

			dists[ambient_type] = Q_min( maxd, dists[ambient_type] );
		}
	}

	for( j = 0; j < NUM_AMBIENTS; j++ )
	{
		// remap to quake range
		dists[j] = RemapVal( dists[j], 0.0, 65536.0, 0.0f, 512.0f );

		if( dists[j] < 100 )
		{
			vol = 1.0;
		}
		else
		{
			vol = 1.0 - dists[j] * 0.002;
			vol = bound( 0.0, vol, 1.0 );
		}
		leaf->ambient_level[j] = vol * 255;
	}
}

/*
====================
CalcAmbientSounds

====================
*/
void CalcAmbientSounds( void )
{
	MsgDev( D_REPORT, "---- Calc Ambient Sounds ----\n" );

	RunThreadsOnIndividual( g_numvisleafs, true, CalcAmbientSounds );
}