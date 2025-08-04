#include "qrad.h"

#ifdef HLRAD_AMBIENTCUBES

// the angle between consecutive g_anorms[] vectors is ~14.55 degrees
#define MIN_LOCAL_SAMPLES	4
#define MAX_LOCAL_SAMPLES	64		// unsigned byte limit
#define MAX_SAMPLES		32		// enough
#define MAX_LEAF_PLANES	512		// QuArK cylinder :-)
#define AMBIENT_SCALE	128.0		// ambient clamp at 128
#define GAMMA			( 2.2f )		// Valve Software gamma
#define INVGAMMA		( 1.0f / 2.2f )	// back to 1.0

static vec3_t g_BoxDirections[6] = 
{
	{  1,  0,  0 }, 
	{ -1,  0,  0 },
	{  0,  1,  0 }, 
	{  0, -1,  0 }, 
	{  0,  0,  1 }, 
	{  0,  0, -1 },
};

// this stores each sample of the ambient lighting
struct ambientsample_t
{
	vec3_t		cube[6];
	vec3_t		pos;
};

struct ambientlist_t
{
	int		numSamples;
	ambientsample_t	*samples;
};

struct ambientlocallist_t
{
	ambientsample_t	samples[MAX_LOCAL_SAMPLES];
	int		numSamples;
};

struct leafplanes_t
{
	dplane_t		planes[MAX_LEAF_PLANES];
	int		numLeafPlanes;
};

static int	leafparents[MAX_MAP_LEAFS];
static int	nodeparents[MAX_MAP_NODES];
ambientlist_t	g_leaf_samples[MAX_MAP_LEAFS];

static void MakeParents( const int nodenum, const int parent )
{
	dnode_t	*node;
	int	i, j;

	nodeparents[nodenum] = parent;
	node = g_dnodes + nodenum;

	for( i = 0; i < 2; i++ )
	{
		j = node->children[i];
		if( j < 0 ) leafparents[-j - 1] = nodenum;
		else MakeParents(j, nodenum);
	}
}

static vec_t LightAngle( const dworldlight_t *wl, const vec3_t lnormal, const vec3_t snormal, const vec3_t delta, float dist )
{
	vec_t	dot, dot2;

	ASSERT( wl->emittype == emit_surface );

	dot = DotProduct( snormal, delta );
	if( dot <= NORMAL_EPSILON )
		return 0; // behind light surface

	dot2 = -DotProduct( delta, lnormal );
	if( dot2 * dist <= MINIMUM_PATCH_DISTANCE )
		return 0; // behind light surface

	// variable power falloff (1 = inverse linear, 2 = inverse square)
	vec_t	denominator = dist * wl->fade;

	if( wl->falloff == 2 )
		denominator *= dist;

	return dot * dot2 / denominator;
}

static vec_t LightDistanceFalloff( const dworldlight_t *wl, const vec3_t delta )
{
	vec_t radius = DotProduct( delta, delta );

	ASSERT( wl->emittype == emit_surface );

	// cull out stuff that's too far
	if( wl->radius != 0 )
	{
		if( radius > ( wl->radius * wl->radius ))
			return 0.0f;
	}

	if( radius < 1.0 )
		return 1.0;
	return 1.0 / radius;
}

static void AddEmitSurfaceLights( int threadnum, const vec3_t vStart, vec3_t lightBoxColor[6] )
{
	vec3_t	wlOrigin;
	trace_t	trace;

	for( int iLight = 0; iLight < g_numworldlights; iLight++ )
	{
		dworldlight_t *wl = &g_dworldlights[iLight];

		// Should this light even go in the ambient cubes?
		if( !FBitSet( wl->flags, DWL_FLAGS_INAMBIENTCUBE ))
			continue;

		ASSERT( wl->emittype == emit_surface );

		VectorCopy( wl->origin, wlOrigin );	// short to float

		VectorMA( wlOrigin, 1.5f, wl->normal, wlOrigin );

		// Can this light see the point?
		TestLine( threadnum, vStart, wlOrigin, &trace, false );

		if( trace.contents != CONTENTS_EMPTY )
			continue;

		// add this light's contribution.
		vec3_t vDelta;
		VectorSubtract( wlOrigin, vStart, vDelta );
		float flDistanceScale = LightDistanceFalloff( wl, vDelta );

		VectorNormalize( vDelta );
		float ratio = - flDistanceScale * DotProduct( wl->normal, vDelta );

		if( ratio <= 0.0 ) continue;

		for( int i = 0; i < 6; i++ )
		{
			vec_t t = DotProduct( g_BoxDirections[i], vDelta );
			if( t > 0.0 ) VectorMA( lightBoxColor[i], (t * ratio), wl->intensity, lightBoxColor[i] );
		}
	}	
}

bool R_GetDirectLightFromSurface( dface_t *surf, const vec3_t point, lightpoint_t *info, bool no_texlight )
{
	faceneighbor_t	*fn = &g_faceneighbor[surf - g_dfaces];
	int		texture_step = GetTextureStep( surf );
	dtexinfo_t	*tex = g_texinfo + surf->texinfo;
	int		map, size, smax, tmax;
	float		lmvecs[2][4];
	vec_t		s, t;
	byte		*lm;

	LightMatrixFromTexMatrix( tex, lmvecs );

	// recalc face extents here
	s = DotProduct( point, lmvecs[0] ) + lmvecs[0][3] - fn->lightmapmins[0];
	t = DotProduct( point, lmvecs[1] ) + lmvecs[1][3] - fn->lightmapmins[1];

	if(( s < 0 || s > fn->lightextents[0] ) || ( t < 0 || t > fn->lightextents[1] ))
		return false;

	if( FBitSet( tex->flags, TEX_SPECIAL ))
	{
		const char *texname = GetTextureByTexinfo( surf->texinfo );

		if( !Q_strnicmp( texname, "sky", 3 ))
			info->hitsky = true;
		return false; // no lightmaps
	}

	if( surf->lightofs == -1 )
		return false;

	smax = (fn->lightextents[0] / texture_step) + 1;
	tmax = (fn->lightextents[1] / texture_step) + 1;
	s /= (vec_t)texture_step;
	t /= (vec_t)texture_step;

	facelight_t	*fl = &g_facelight[surf - g_dfaces];
	sample_t	*samp = fl->samples;
	int			offset = Q_rint( t ) * smax + Q_rint( s );

	for( map = 0; map < MAXLIGHTMAPS; map++ )
	{
		info->styles[map] = surf->styles[map];

		VectorCopy( samp[offset].light[map], info->diffuse[map] );
		VectorCopy( fl->average[map], info->average[map] );

		if( no_texlight )
		{
			VectorSubtract( info->diffuse[map], fl->texlight[map], info->diffuse[map] );
			VectorSubtract( info->average[map], fl->texlight[map], info->average[map] );
		}

		VectorMultiply( info->diffuse[map], g_reflectivity[tex->miptex], info->diffuse[map] );
		VectorMultiply( info->average[map], g_reflectivity[tex->miptex], info->average[map] );
	}
	
	info->surf = surf;

	return true;
}
	


//-----------------------------------------------------------------------------
// Finds ambient sky lights
//-----------------------------------------------------------------------------
static dworldlight_t *FindAmbientSkyLight( void )
{
	static dworldlight_t *s_pCachedSkylight = NULL;

	// Don't keep searching for the same light.
	if( !s_pCachedSkylight )
	{
		// find any ambient lights
		for( int iLight = 0; iLight < g_numworldlights; iLight++ )
		{
			dworldlight_t *wl = &g_dworldlights[iLight];

			if( wl->emittype == emit_skylight )
			{
				s_pCachedSkylight = wl;
				break;
			}
		}
	}

	return s_pCachedSkylight;
}



//-----------------------------------------------------------------------------
// Computes ambient lighting along a specified ray.  
// Ray represents a cone, tanTheta is the tan of the inner cone angle
//-----------------------------------------------------------------------------
static void CalcRayAmbientLighting( int threadnum, const vec3_t vStart, const vec3_t vEnd, dworldlight_t *pSkyLight, float tanTheta, vec3_t radcolor )
{
	lightpoint_t info;
	vec3_t vDelta;

	memset( &info, 0, sizeof( lightpoint_t ));
	info.fraction = 1.0f;
	VectorClear( radcolor );


	trace_t trace;
	TestLine( threadnum, vStart, vEnd, &trace, false );

	if( trace.contents == CONTENTS_SKY )
	{
		VectorSubtract( vEnd, vStart, vDelta );
		VectorNormalize( vDelta );
		GetSkyColor( vDelta, radcolor );
		VectorScale( radcolor, g_indirect_sun, radcolor );
		return;
	}

	//studio gi
	if( trace.surface == STUDIO_SURFACE_HIT )
	{
		for (int i = 0; i < MAXLIGHTMAPS; i++ )
			if( (trace.styles[i] == LS_NORMAL)||(trace.styles[i] == LS_SKY)||(trace.styles[i] == g_skystyle))	//only sun, sky and default style
				VectorAdd( radcolor, trace.light[i], radcolor );	
		//Msg( "model hit, color %f %f %f\n", radcolor[0], radcolor[1], radcolor[2] );
		return;
	}

	if( trace.surface == -1 )
		return;

	vec3_t	hitpoint, temp_color;
	vec_t	scaleAvg;
	dface_t *face = g_dfaces + trace.surface;
	VectorLerp( vStart, trace.fraction, vEnd, hitpoint );

	if( !R_GetDirectLightFromSurface( face, hitpoint, &info, true ))
		return;

	scaleAvg = CalcFaceSolidAngle( face, vStart );
	//Msg( "solid angle %f\n", scaleAvg );
	if( scaleAvg <= 0.0f )
		return;
	scaleAvg = 4.0f * M_PI / ((float)NUMVERTEXNORMALS * scaleAvg);	//ratio of ray cone and face solid angles
	scaleAvg = bound( 0.0f, scaleAvg, 1.0f );
	for (int i = 0; i < MAXLIGHTMAPS; i++ )
		if( (info.styles[i] == LS_NORMAL)||(info.styles[i] == LS_SKY)||(info.styles[i] == g_skystyle))	//only sun, sky and default style
		{	
			VectorLerp( info.diffuse[i], scaleAvg, info.average[i], temp_color );	
			VectorAdd( radcolor, temp_color, radcolor );	
		}
		
}

static void ComputeAmbientFromSphericalSamples( int threadnum, const vec3_t p1, vec3_t lightBoxColor[6] )
{
	// Figure out the color that rays hit when shot out from this position.
	float tanTheta = tan( VERTEXNORMAL_CONE_INNER_ANGLE );
	dworldlight_t *pSkyLight = FindAmbientSkyLight();
	vec3_t radcolor[NUMVERTEXNORMALS], p2;

	for( int i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		VectorMA( p1, (65536.0f * 1.74f), g_anorms[i], p2 );

		// Now that we've got a ray, see what surface we've hit
		CalcRayAmbientLighting( threadnum, p1, p2, pSkyLight, tanTheta, radcolor[i] );
	}

	// accumulate samples into radiant box
	for ( int j = 0; j < 6; j++ )
	{
		float t = 0.0f;

		VectorClear( lightBoxColor[j] );

		for( int i = 0; i < NUMVERTEXNORMALS; i++ )
		{
			float c = DotProduct( g_anorms[i], g_BoxDirections[j] );

			if( c > 0.0f )
			{
				VectorMA( lightBoxColor[j], c, radcolor[i], lightBoxColor[j] );
				t += c;
			}
		}
		
		VectorScale( lightBoxColor[j], ( 1.0 / t ), lightBoxColor[j] );
	}

	// Now add direct light from the emit_surface lights. These go in the ambient cube because
	// there are a ton of them and they are often so dim that they get filtered out by r_worldlightmin.
	AddEmitSurfaceLights( threadnum, p1, lightBoxColor );
}

bool IsLeafAmbientSurfaceLight( dworldlight_t *wl )
{
	const float g_flWorldLightMinEmitSurfaceDistanceRatio = 0.000003f;
	const float g_flWorldLightMinEmitSurface = 0.005f;

	if( wl->emittype != emit_surface )
		return false;

	if( wl->style != 0 )
		return false;

	return true;
}



bool CheckSamplePositionInLeaf( vec3_t samplePosition, const leafplanes_t *leafPlanes )
{
	for( int k = 0; k < leafPlanes->numLeafPlanes; k++ )
		if( PlaneDiff2( samplePosition, &leafPlanes->planes[k] ) < DIST_EPSILON )
			return false;	
	return true;
}

// gets a list of the planes pointing into a leaf
void GetLeafBoundaryPlanes( leafplanes_t *list, int leafIndex )
{
	int nodeIndex = leafparents[leafIndex];
	int child = -(leafIndex + 1);

	while( nodeIndex >= 0 )
	{
		dnode_t *pNode = g_dnodes + nodeIndex;
		dplane_t *pNodePlane = g_dplanes + pNode->planenum;

		if( pNode->children[0] == child )
		{
			// front side
			list->planes[list->numLeafPlanes] = *pNodePlane;
			list->numLeafPlanes++;
		}
		else
		{
			// back side
			int plane = list->numLeafPlanes;
			list->planes[plane].dist = -pNodePlane->dist;
			list->planes[plane].normal[0] = -pNodePlane->normal[0];
			list->planes[plane].normal[1] = -pNodePlane->normal[1];
			list->planes[plane].normal[2] = -pNodePlane->normal[2];
			list->planes[plane].type = pNodePlane->type;
			list->numLeafPlanes++;
		}

		if( list->numLeafPlanes >= MAX_LEAF_PLANES )
			break; // there were too many planes

		child = nodeIndex;
		nodeIndex = nodeparents[child];
	}
}


// max number of units in linear space of per-side delta
float CubeDeltaColor( vec3_t pCube0[6], vec3_t pCube1[6] )
{
	float maxDelta = 0.0f;
	
	for( int i = 0; i < 6; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			float val0 = pCube0[i][j];
			float val1 = pCube1[i][j];
			float delta = fabsf( val0 - val1 );

			if( delta > maxDelta )
				maxDelta = delta;
		}
	}

	return maxDelta;
}
// reconstruct the ambient lighting for a leaf at the given position in worldspace
// optionally skip one of the entries in the list
void Mod_LeafAmbientColorAtPos( vec3_t pOut[6], const vec3_t pos, ambientlocallist_t *list, int skipIndex )
{
	vec3_t vDelta;
	int i;

	for( i = 0; i < 6; i++ )
	{
		VectorClear( pOut[i] );
	}

	float totalFactor = 0.0f;

	for( i = 0; i < list->numSamples; i++ )
	{
		if ( i == skipIndex )
			continue;

		// do an inverse squared distance weighted average of the samples to reconstruct 
		// the original function
		VectorSubtract( list->samples[i].pos, pos, vDelta ); 
		float dist = DotProduct( vDelta, vDelta );
		float factor = 1.0f / (dist + 1.0f);
		totalFactor += factor;

		for( int j = 0; j < 6; j++ )
		{
			VectorMA( pOut[j], factor, list->samples[i].cube[j], pOut[j] );
		}
	}

	for( i = 0; i < 6; i++ )
	{
		VectorScale( pOut[i], (1.0f / totalFactor), pOut[i] );
	}
}

// this samples the lighting at each sample and removes any unnecessary samples
void CompressAmbientSampleList( ambientlocallist_t *list )
{
	ambientlocallist_t	oldlist;
	vec3_t		testCube[6];

	//this algorithm still sucks 
	int i = 0;
	oldlist.numSamples = 0;
	while ( i < list->numSamples )
	{
		Mod_LeafAmbientColorAtPos( testCube, list->samples[i].pos, list, i );

		if( CubeDeltaColor( testCube, list->samples[i].cube ) >= g_lightprobeepsilon )
			i++;
		else
		{
			//the current probe can be reconstructed, but can all previously removed probes be reconstructed without it?
			bool remove_current = true;
			for( int j = 0; j < oldlist.numSamples; j++ )
			{
				Mod_LeafAmbientColorAtPos( testCube, oldlist.samples[j].pos, list, i );
				if( CubeDeltaColor( testCube, oldlist.samples[j].cube ) >= g_lightprobeepsilon )
				{
					i++;
					remove_current = false;
					break;
				}
			}

			if( remove_current )
			{
				memcpy( &oldlist.samples[oldlist.numSamples], &list->samples[i], sizeof( ambientsample_t ));
				oldlist.numSamples++;

				list->numSamples--;
				memcpy( &list->samples[i], &list->samples[list->numSamples], sizeof( ambientsample_t ));
			}
		}
	}

	if( list->numSamples == 0 )
	{
		list->numSamples = 1;
		memcpy( &list->samples[0], &oldlist.samples[0], sizeof( ambientsample_t ));	
	}
}

void ComputeAmbientForLeaf( int threadnum, int leafID, ambientlocallist_t *list )
{
	leafplanes_t	leafPlanes;

	if( g_dleafs[leafID].contents == CONTENTS_SOLID )
	{
		// don't generate any samples in solid leaves
		// NOTE: We copy the nearest non-solid leaf 
		// sample pointers into this leaf at the end
		return;
	}

	if( g_dleafs[leafID].contents == CONTENTS_SKY )
		return;

	leafPlanes.numLeafPlanes = 0;

	GetLeafBoundaryPlanes( &leafPlanes, leafID );

	// this heuristic tries to generate at least one sample per volume (chosen to be similar to the size of a player) in the space
	int xSize = (g_dleafs[leafID].maxs[0] - g_dleafs[leafID].mins[0]) / 64;
	int ySize = (g_dleafs[leafID].maxs[1] - g_dleafs[leafID].mins[1]) / 64;
	int zSize = (g_dleafs[leafID].maxs[2] - g_dleafs[leafID].mins[2]) / 64;

	xSize = Q_max( xSize, 1 );
	ySize = Q_max( ySize, 1 );
	zSize = Q_max( zSize, 1 );
	

	xSize = Q_min( xSize, MAX_LOCAL_SAMPLES );
	ySize = Q_min( ySize, MAX_LOCAL_SAMPLES );
	zSize = Q_min( zSize, MAX_LOCAL_SAMPLES );

	while( xSize * ySize * zSize > MAX_LOCAL_SAMPLES )	//lazy way
	{
		xSize = Q_max( xSize - 1, 1 );
		ySize = Q_max( ySize - 1, 1 );
		zSize = Q_max( zSize - 1, 1 );	
	}
	
	vec3_t cube[6];
	vec3_t samplePosition;
	vec3_t leafSize;
	vec3_t leafMins;
	VectorCopy( g_dleafs[leafID].mins, leafMins );
	VectorSubtract( g_dleafs[leafID].maxs, leafMins, leafSize );

	for( int i = 0; i < xSize; i++ )
		for( int j = 0; j < ySize; j++ )
			for( int k = 0; k < zSize; k++ )
			{
				samplePosition[0] = ((float)i + 0.5f) / (float)xSize;
				samplePosition[1] = ((float)j + 0.5f) / (float)ySize;
				samplePosition[2] = ((float)k + 0.5f) / (float)zSize;

				VectorMultiply( samplePosition, leafSize, samplePosition );
				VectorAdd( samplePosition, leafMins, samplePosition );

				if( !CheckSamplePositionInLeaf( samplePosition, &leafPlanes ) )
					continue;

				ComputeAmbientFromSphericalSamples( threadnum, samplePosition, cube );

				memcpy( list->samples[list->numSamples].cube, &cube[0], sizeof(vec3_t) * 6 );
				VectorCopy( samplePosition, list->samples[list->numSamples].pos );
				list->numSamples++;
			}

	if( list->numSamples == 0 )
	{
		VectorSet( samplePosition, 0.5f, 0.5f, 0.5f );
		VectorMultiply( samplePosition, leafSize, samplePosition );
		VectorAdd( samplePosition, leafMins, samplePosition );

		ComputeAmbientFromSphericalSamples( threadnum, samplePosition, cube );

		memcpy( list->samples[list->numSamples].cube, &cube[0], sizeof(vec3_t) * 6 );
		VectorCopy( samplePosition, list->samples[list->numSamples].pos );
		list->numSamples++;	
	}

	// remove any samples that can be reconstructed with the remaining data
	CompressAmbientSampleList( list );
}

static void LeafAmbientLighting( int threadnum )
{
	ambientlocallist_t	list;

	while( 1 )
	{
		int leafID = GetThreadWork ();
		if( leafID == -1 ) break;

		list.numSamples = 0;

		ComputeAmbientForLeaf( threadnum, leafID, &list );

		// copy to the output array
		g_leaf_samples[leafID].numSamples = list.numSamples;
		g_leaf_samples[leafID].samples = (ambientsample_t *)Mem_Alloc( sizeof( ambientsample_t ) * list.numSamples );
		memcpy( g_leaf_samples[leafID].samples, list.samples, sizeof( ambientsample_t ) * list.numSamples );
	}
}

void ComputeLeafAmbientLighting( void )
{
	// Figure out which lights should go in the per-leaf ambient cubes.
	int nInAmbientCube = 0;
	int nSurfaceLights = 0;
	int i;

	// always matched
	memset( g_leaf_samples, 0, sizeof( g_leaf_samples ));

	for( i = 0; i < g_numworldlights; i++ )
	{
		dworldlight_t *wl = &g_dworldlights[i];
		
		if( IsLeafAmbientSurfaceLight( wl ))
			SetBits( wl->flags, DWL_FLAGS_INAMBIENTCUBE );
		else ClearBits( wl->flags, DWL_FLAGS_INAMBIENTCUBE );
	
		if( wl->emittype == emit_surface )
			nSurfaceLights++;

		if( FBitSet( wl->flags, DWL_FLAGS_INAMBIENTCUBE ))
			nInAmbientCube++;
	}

	MakeParents( 0, -1 );

	MsgDev( D_REPORT, "%d of %d (%d%% of) surface lights went in leaf ambient cubes.\n",
	nInAmbientCube, nSurfaceLights, nSurfaceLights ? ((nInAmbientCube*100) / nSurfaceLights) : 0 );

	RunThreadsOn( g_dmodels[0].visleafs + 1, true, LeafAmbientLighting );

	// clear old samples
	g_numleaflights = 0;

	for ( int leafID = 0; leafID < g_dmodels[0].visleafs + 1; leafID++ )
	{
		ambientlist_t *list = &g_leaf_samples[leafID];

		ASSERT( list->numSamples <= 255 );

		if( !list->numSamples ) continue;

		// compute the samples in disk format.  Encode the positions in 8-bits using leaf bounds fractions
		for ( int i = 0; i < list->numSamples; i++ )
		{
			if( g_numleaflights == MAX_MAP_LEAFLIGHTS )
				COM_FatalError( "MAX_MAP_LEAFLIGHTS limit exceeded\n" );

			dleafsample_t *sample = &g_dleaflights[g_numleaflights];

			for( int j = 0; j < 3; j++ )
				sample->origin[j] = (short)bound( -32767, (int)list->samples[i].pos[j], 32767 );
			sample->leafnum = leafID;
			
			for( int side = 0; side < 6; side++ )
			{
				ApplyLightPostprocess( list->samples[i].cube[side] ); 
				for( int j = 0; j < 3; j++ )
				{					
					sample->ambient.color[side][j] = Q_rint( list->samples[i].cube[side][j] );
				}
			}

			g_numleaflights++;
		}

		Mem_Free( list->samples );	// release src
		list->samples = NULL;
	}

	MsgDev( D_REPORT, "%i ambient samples stored\n", g_numleaflights );
}

#endif
