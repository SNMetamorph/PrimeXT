/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// dirtmap.c	// quake3 dirt-mapping (fake Ambient Occlusion)

#include "qrad.h"

#define DIRT_CONE_ANGLE		115  // in degrees
#define DIRT_NUM_ELEVATION_STEPS	3
#define DIRT_NUM_ANGLE_STEPS		16
#define DIRT_NUM_VECTORS		( DIRT_NUM_ANGLE_STEPS * DIRT_NUM_ELEVATION_STEPS )

static vec3_t	g_dirtvecs[DIRT_NUM_VECTORS];
static int	g_num_dirtvecs = 0;

/*
============
SetupDirt

sets up dirtmap (ambient occlusion)
============
*/
void SetupDirt( void )
{
	// check if needed
	if( !g_dirtmapping )
		return;

	// calculate angular steps
	vec_t	angleStep = DEG2RAD( 360.0f / DIRT_NUM_ANGLE_STEPS );
	vec_t	elevationStep = DEG2RAD( DIRT_CONE_ANGLE / DIRT_NUM_ELEVATION_STEPS );
	vec_t	angle = 0.0f;

	for( int i = 0; i < DIRT_NUM_ANGLE_STEPS; i++, angle += angleStep )
	{
		vec_t	elevation = elevationStep * 0.5;

		for( int j = 0; j < DIRT_NUM_ELEVATION_STEPS; j++, elevation += elevationStep )
		{
			g_dirtvecs[g_num_dirtvecs][0] = sin( elevation ) * cos( angle );
			g_dirtvecs[g_num_dirtvecs][1] = sin( elevation ) * sin( angle );
			g_dirtvecs[g_num_dirtvecs][2] = cos( elevation );
			g_num_dirtvecs++;
		}
	}

	MsgDev( D_REPORT, "%d dirtmap vectors\n", g_num_dirtvecs );
}

float GatherSampleDirt( int threadnum, int fn, const vec3_t pos, const vec3_t normal, entity_t *ignoreent )
{
	vec3_t	tangent, binormal, direction;
	vec_t	gatherDirt, outDirt;
	vec_t	dirtDepth = 128.0;
	vec_t	dirtScale = 2.0;
	vec_t	dirtGain = 2.0;
	vec3_t	vecSrc, vecEnd;
	trace_t	trace;

	if( !g_dirtmapping ) return 1.0f; // early out

	VectorCopy( pos, vecSrc );
	gatherDirt = 0.0f;

	if( fn >= 0 )
	{
		dface_t		*f = &g_dfaces[fn];
		dtexinfo_t	*tx = &g_texinfo[f->texinfo];
		float		lmvecs[2][4];

		if( FBitSet( tx->flags, TEX_NODIRT ))
			return 1.0f; // disabled by user

		LightMatrixFromTexMatrix( tx, lmvecs );
		VectorCopy( lmvecs[1], binormal ); 
		VectorCopy( lmvecs[0], tangent ); 
	}
	else
	{
		// compute tangent space from phong-shaded normal
		VectorVectors( normal, tangent, binormal );
	}

	for( int i = 0; i < g_num_dirtvecs; i++ )
	{
		// transform vector into tangent space
		direction[0] = tangent[0] * g_dirtvecs[i][0] + binormal[0] * g_dirtvecs[i][1] + normal[0] * g_dirtvecs[i][2];
		direction[1] = tangent[1] * g_dirtvecs[i][0] + binormal[1] * g_dirtvecs[i][1] + normal[1] * g_dirtvecs[i][2];
		direction[2] = tangent[2] * g_dirtvecs[i][0] + binormal[2] * g_dirtvecs[i][1] + normal[2] * g_dirtvecs[i][2];

		VectorMA( pos, dirtDepth, direction, vecEnd );
		TestLine( threadnum, vecSrc, vecEnd, &trace );

		if( trace.contents == CONTENTS_SOLID )
			gatherDirt += 1.0f - trace.fraction;
	}

	// direct ray
	VectorMA( pos, dirtDepth, normal, vecEnd );
	TestLine( threadnum, vecSrc, vecEnd, &trace );

	if( trace.contents == CONTENTS_SOLID )
		gatherDirt += 1.0f - trace.fraction;

	// early out
	if( gatherDirt <= 0.0f )
		return 1.0f;

	// apply gain (does this even do much? heh)
	outDirt = pow( gatherDirt / ( g_num_dirtvecs + 1 ), dirtGain ) * dirtScale;
	if( outDirt > 1.0f ) outDirt = 1.0f;

	return 1.0f - outDirt;
}