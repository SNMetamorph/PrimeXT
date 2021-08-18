#include "qrad.h"

#ifdef HLRAD_AMBIENTCUBES

// the angle between consecutive g_anorms[] vectors is ~14.55 degrees
#define MIN_LOCAL_SAMPLES	4
#define MAX_LOCAL_SAMPLES	16		// unsigned byte limit
#define MAX_SAMPLES		32		// enough
#define MAX_LEAF_PLANES	512		// QuArK cylinder :-)
#define AMBIENT_SCALE	128.0		// ambient clamp at 128
#define GAMMA		( 2.2f )		// Valve Software gamma
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

typedef struct
{
	vec3_t		diffuse;
	vec3_t		average;
	float		fraction;
	dface_t		*surf;
	bool		hitsky;
} lightpoint_t;

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

		// Can this light see the point?
		TestLine( threadnum, vStart, wlOrigin, &trace );

		if( trace.contents != CONTENTS_EMPTY )
			continue;

		// add this light's contribution.
		vec3_t vDelta, vDeltaNorm;
		VectorSubtract( wlOrigin, vStart, vDelta );
		float flDistanceScale = LightDistanceFalloff( wl, vDelta );

		VectorCopy( vDelta, vDeltaNorm );
		VectorMA( vDeltaNorm, -DEFAULT_HUNT_OFFSET * 0.5, wl->normal, vDeltaNorm );
		float dist = VectorNormalize( vDeltaNorm );
		dist = Q_max( dist, 1.0 );

		float flAngleScale = LightAngle( wl, wl->normal, vDeltaNorm, vDeltaNorm, dist );

		float ratio = flDistanceScale * flAngleScale * trace.fraction;
		if( ratio == 0.0 ) continue;

		for( int i = 0; i < 6; i++ )
		{
			vec_t t = DotProduct( g_BoxDirections[i], vDeltaNorm );
			if( t > 0.0 ) VectorMA( lightBoxColor[i], (t * ratio), wl->intensity, lightBoxColor[i] );
		}
	}	
}

static bool R_GetDirectLightFromSurface( dface_t *surf, const vec3_t point, lightpoint_t *info )
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
		return true;

	smax = (fn->lightextents[0] / texture_step) + 1;
	tmax = (fn->lightextents[1] / texture_step) + 1;
	s /= (vec_t)texture_step;
	t /= (vec_t)texture_step;

	byte *samples = g_dlightdata + (unsigned int)surf->lightofs;
	lm = samples + (unsigned int)Q_rint( t ) * smax + Q_rint( s );
	size = smax * tmax;

	VectorClear( info->diffuse );

	for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
	{
		info->diffuse[0] += (float)lm[0] * 264.0f;
		info->diffuse[1] += (float)lm[1] * 264.0f;
		info->diffuse[2] += (float)lm[2] * 264.0f;
		lm += size; // skip to next lightmap
	}

	// same as shift >> 7 in quake
	info->diffuse[0] = Q_min( info->diffuse[0] * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	info->diffuse[1] = Q_min( info->diffuse[1] * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	info->diffuse[2] = Q_min( info->diffuse[2] * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	VectorClear( info->average );
	lm = samples;

	// also collect the average value
	for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
	{
		for( int i = 0; i < size; i++, lm += 3 )
		{
			info->average[0] += (float)lm[0] * 264.0f;
			info->average[1] += (float)lm[1] * 264.0f;
			info->average[2] += (float)lm[2] * 264.0f;
		}
		VectorScale( info->average, ( 1.0f / (float)size ), info->average );
	}

	// same as shift >> 7 in quake
	info->average[0] = Q_min( info->average[0] * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	info->average[1] = Q_min( info->average[1] * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	info->average[2] = Q_min( info->average[2] * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	info->surf = surf;

	return true;
}
	
/*
=================
R_RecursiveLightPoint
=================
*/
static bool R_RecursiveLightPoint( const int nodenum, float p1f, float p2f, const vec3_t start, const vec3_t end, lightpoint_t *info )
{
	vec3_t	mid;

	// hit a leaf
	if( nodenum < 0 ) return false;
	dnode_t *node = g_dnodes + nodenum;
	dplane_t *plane = g_dplanes + node->planenum;

	// calculate mid point
	float front = PlaneDiff( start, plane );
	float back = PlaneDiff( end, plane );

	int side = front < 0.0f;
	if(( back < 0.0f ) == side )
		return R_RecursiveLightPoint( node->children[side], p1f, p2f, start, end, info );

	float frac = front / ( front - back );

	float midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( R_RecursiveLightPoint( node->children[side], p1f, midf, start, mid, info ))
		return true; // hit something

	if(( back < 0.0f ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	for( int i = 0; i < node->numfaces; i++ )
	{
		dface_t *surf = g_dfaces + node->firstface + i;

		if( R_GetDirectLightFromSurface( surf, mid, info ))
		{
			info->fraction = midf;
			return true;
		}
	}

	// go down back side
	return R_RecursiveLightPoint( node->children[!side], midf, p2f, mid, end, info );
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

static void ComputeLightmapColorFromPoint( lightpoint_t *info, dworldlight_t* pSkylight, float scale, vec3_t radcolor, bool average )
{
	vec3_t	color;

	if( !info->surf && info->hitsky )
	{
		if( pSkylight )
		{
			VectorScale( pSkylight->intensity, scale * 0.5, color );
			VectorScale( pSkylight->intensity, (1.0 / 255.0), color );
			VectorAdd( radcolor, color, radcolor );
		}
		return;
	}

	if( info->surf != NULL )
	{
		if( average ) VectorScale( info->average, scale, color );
		else VectorScale( info->diffuse, scale, color );
#if 0
		vec3_t	light, reflectivity;

		BaseLightForFace( info->surf, light, reflectivity );

		if( !VectorCompare( reflectivity, vec3_origin ))
			VectorMultiply( color, reflectivity, color );
#endif
		VectorAdd( radcolor, color, radcolor );
	}
}

//-----------------------------------------------------------------------------
// Computes ambient lighting along a specified ray.  
// Ray represents a cone, tanTheta is the tan of the inner cone angle
//-----------------------------------------------------------------------------
static void CalcRayAmbientLighting( const vec3_t vStart, const vec3_t vEnd, dworldlight_t *pSkyLight, float tanTheta, vec3_t radcolor )
{
	lightpoint_t info;
	vec3_t vDelta;

	memset( &info, 0, sizeof( lightpoint_t ));
	info.fraction = 1.0f;
	VectorClear( radcolor );

	// Now that we've got a ray, see what surface we've hit
	R_RecursiveLightPoint( 0, 0.0f, 1.0f, vStart, vEnd, &info );

	VectorSubtract( vEnd, vStart, vDelta );

	// compute the approximate radius of a circle centered around the intersection point
	float dist = VectorLength( vDelta ) * tanTheta * info.fraction;

	// until 20" we use the point sample, then blend in the average until we're covering 40"
	// This is attempting to model the ray as a cone - in the ideal case we'd simply sample all
	// luxels in the intersection of the cone with the surface.  Since we don't have surface 
	// neighbor information computed we'll just approximate that sampling with a blend between
	// a point sample and the face average.
	// This yields results that are similar in that aliasing is reduced at distance while 
	// point samples provide accuracy for intersections with near geometry
	float scaleAvg = RemapValClamped( dist, 20, 40, 0.0f, 1.0f );

	if( !info.surf )
	{
		// don't have luxel UV, so just use average sample
		scaleAvg = 1.0;
	}

	float scaleSample = 1.0f - scaleAvg;

	if( scaleAvg != 0 )
	{
		ComputeLightmapColorFromPoint( &info, pSkyLight, scaleAvg, radcolor, true );
	}

	if( scaleSample != 0 )
	{
		ComputeLightmapColorFromPoint( &info, pSkyLight, scaleSample, radcolor, false );
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
		VectorMA( p1, (65536.0 * 1.74), g_anorms[i], p2 );

		// Now that we've got a ray, see what surface we've hit
		CalcRayAmbientLighting( p1, p2, pSkyLight, tanTheta, radcolor[i] );
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

	float intensity = VectorMax( wl->intensity );

	return ( intensity * g_flWorldLightMinEmitSurfaceDistanceRatio ) < g_flWorldLightMinEmitSurface;
}

// Generate a random point in the leaf's bounding volume
// reject any points that aren't actually in the leaf
// do a couple of tracing heuristics to eliminate points that are inside detail brushes 
// or underneath displacement surfaces in the leaf
// return once we have a valid point, use the center if one can't be computed quickly
void GenerateLeafSamplePosition( int leafIndex, ambientlocallist_t *list, const leafplanes_t *leafPlanes, vec3_t samplePosition )
{
	dleaf_t *pLeaf = g_dleafs + leafIndex;
	vec3_t vCenter, leafMins, leafMaxs;

	VectorCopy( pLeaf->mins, leafMins );
	VectorCopy( pLeaf->maxs, leafMaxs );
	bool bValid = false;

	VectorAverage( leafMins, leafMaxs, vCenter );

	// place first sample always at center to leaf
	if( list->numSamples == 0 )
	{
		VectorCopy( vCenter, samplePosition );
		return;
	}

	// lock so random float will working properly
	ThreadLock();

	for( int i = 0; i < 1024 && !bValid; i++ )
	{
		VectorLerp( leafMins, RandomFloat( 0.01f, 0.99f ), leafMaxs, samplePosition );
		vec3_t vDiff;

		int l;
		for( l = 0; l < list->numSamples; l++ )
		{
			VectorSubtract( samplePosition, list->samples[l].pos, vDiff );
			float flLength = VectorLength( vDiff );
			if( flLength < 32.0f ) break;	// too tight
		}

		if( l != list->numSamples )
			continue;

		bValid = true;

		for( int k = leafPlanes->numLeafPlanes - 1; --k >= 0 && bValid; )
		{
			float d = PlaneDiff( samplePosition, leafPlanes->planes + k );

			if( d < DIST_EPSILON )
			{
				// not inside the leaf, try again 
				bValid = false;
				break;
			}
		}
	}

	ThreadUnlock();

	if( !bValid )
	{
		// didn't generate a valid sample point, just use the center of the leaf bbox
		VectorCopy( vCenter, samplePosition );
	}
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
			break; // there was a too many planes

		child = nodeIndex;
		nodeIndex = nodeparents[child];
	}
}

// add the sample to the list.  If we exceed the maximum number of samples, the worst sample will
// be discarded.  This has the effect of converging on the best samples when enough are added.
void AddSampleToList( ambientlocallist_t *list, const vec3_t samplePosition, vec3_t pCube[6] )
{
	int i, index = list->numSamples++;

	VectorCopy( samplePosition, list->samples[index].pos );

	for( int k = 0; k < 6; k++ )
	{
		VectorCopy( pCube[k], list->samples[index].cube[k] );
	}

	if( list->numSamples <= MAX_SAMPLES )
		return;

	ambientlocallist_t oldlist;
	int nearestNeighborIndex = 0;
	float nearestNeighborDist = FLT_MAX;
	float nearestNeighborTotal = 0;

	// do a copy of current list
	memcpy( &oldlist, list, sizeof( ambientlocallist_t ));

	for( i = 0; i < oldlist.numSamples; i++ )
	{
		int closestIndex = 0;
		float closestDist = FLT_MAX;
		float totalDC = 0;

		for( int j = 0; j < oldlist.numSamples; j++ )
		{
			if( j == i ) continue;

			vec3_t vDelta;

			VectorSubtract( oldlist.samples[i].pos, oldlist.samples[j].pos, vDelta );

			float dist = VectorLength( vDelta );
			float maxDC = 0;

			for( int k = 0; k < 6; k++ )
			{
				// color delta is computed per-component, per cube side
				for( int s = 0; s < 3; s++ )
				{
					float dc = fabs( oldlist.samples[i].cube[k][s] - oldlist.samples[j].cube[k][s] );
					maxDC = Q_max( maxDC, dc );
				}

				totalDC += maxDC;
			}

			// need a measurable difference in color or we'll just rely on position
			if( maxDC < 1e-4f )
			{
				maxDC = 0;
			}
			else if( maxDC > 1.0f )
			{
				maxDC = 1.0f;
			}

			// selection criteria is 10% distance, 90% color difference
			// choose samples that fill the space (large distance from each other)
			// and have largest color variation
			float distanceFactor = 0.1f + (maxDC * 0.9f);
			dist *= distanceFactor;

			// find the "closest" sample to this one
			if( dist < closestDist )
			{
				closestDist = dist;
				closestIndex = j;
			}
		}

		// the sample with the "closest" neighbor is rejected
		if( closestDist < nearestNeighborDist || ( closestDist == nearestNeighborDist && totalDC < nearestNeighborTotal ))
		{
			nearestNeighborDist = closestDist;
			nearestNeighborIndex = i;
		}
	}

	list->numSamples = 0;

	// copy the entries back but skip nearestNeighborIndex
	for( i = 0; i < oldlist.numSamples; i++ )
	{
		if( i != nearestNeighborIndex )
		{
			memcpy( &list->samples[list->numSamples], &oldlist.samples[i], sizeof( ambientsample_t ));
			list->numSamples++;
		}
	}
}

// max number of units in gamma space of per-side delta
int CubeDeltaColor( vec3_t pCube0[6], vec3_t pCube1[6] )
{
	int maxDelta = 0;

	// do this comparison in gamma space to try and get a perceptual basis for the compare
	for( int i = 0; i < 6; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			int val0 = pCube0[i][j];
			int val1 = pCube1[i][j];
			int delta = abs( val0 - val1 );

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

	// do a copy of current list
	memcpy( &oldlist, list, sizeof( ambientlocallist_t ));
	list->numSamples = 0;

	for( int i = 0; i < oldlist.numSamples; i++ )
	{
		Mod_LeafAmbientColorAtPos( testCube, oldlist.samples[i].pos, &oldlist, i );

		// at least one sample must be included in the list
		if( i == 0 || CubeDeltaColor( testCube, oldlist.samples[i].cube ) >= 10 )
		{
			memcpy( &list->samples[list->numSamples], &oldlist.samples[i], sizeof( ambientsample_t ));
			list->numSamples++;
		}
	}
}

void ComputeAmbientForLeaf( int threadnum, int leafID, ambientlocallist_t *list )
{
	leafplanes_t	leafPlanes;

	leafPlanes.numLeafPlanes = 0;

	GetLeafBoundaryPlanes( &leafPlanes, leafID );

	// this heuristic tries to generate at least one sample per volume (chosen to be similar to the size of a player) in the space
	int xSize = (g_dleafs[leafID].maxs[0] - g_dleafs[leafID].mins[0]) / 64;
	int ySize = (g_dleafs[leafID].maxs[1] - g_dleafs[leafID].mins[1]) / 64;
	int zSize = (g_dleafs[leafID].maxs[2] - g_dleafs[leafID].mins[2]) / 64;

	xSize = Q_max( xSize, 1 );
	ySize = Q_max( ySize, 1 );
	zSize = Q_max( zSize, 1 );

	// generate update 128 candidate samples, always at least one sample
	int volumeCount = xSize * ySize * zSize;

	if( g_fastmode )
		volumeCount *= 0.01;
	else if( !g_extra )
		volumeCount *= 0.05;
	else volumeCount *= 0.1;

	int sampleCount = bound( MIN_LOCAL_SAMPLES, volumeCount, MAX_LOCAL_SAMPLES );

	if( g_dleafs[leafID].contents == CONTENTS_SOLID )
	{
		// don't generate any samples in solid leaves
		// NOTE: We copy the nearest non-solid leaf 
		// sample pointers into this leaf at the end
		return;
	}

	vec3_t cube[6];

	for( int i = 0; i < sampleCount; i++ )
	{
		// compute each candidate sample and add to the list
		vec3_t samplePosition;
		GenerateLeafSamplePosition( leafID, list, &leafPlanes, samplePosition );
		ComputeAmbientFromSphericalSamples( threadnum, samplePosition, cube );
		// note this will remove the least valuable sample once the limit is reached
		AddSampleToList( list, samplePosition, cube );
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

	srand( time( NULL ));	// init random generator
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
				sample->ambient.color[side][0] = bound( 0, list->samples[i].cube[side][0] * 255, 255 );
				sample->ambient.color[side][1] = bound( 0, list->samples[i].cube[side][1] * 255, 255 );
				sample->ambient.color[side][2] = bound( 0, list->samples[i].cube[side][2] * 255, 255 );
			}
			g_numleaflights++;
		}

		Mem_Free( list->samples );	// release src
		list->samples = NULL;
	}

	MsgDev( D_REPORT, "%i ambient samples stored\n", g_numleaflights );
}

#endif