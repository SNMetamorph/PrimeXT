/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "qrad.h"
#include "model_trace.h"
#include "../../engine/studio.h"

#ifdef HLRAD_VERTEXLIGHTING

#define MAX_INDIRECT_DIST	1024.0f

typedef struct
{
	unsigned int modelnum : 32;
	unsigned int vertexnum : 32;
} vertremap_t;

static entity_t	*g_vertexlight[MAX_MAP_MODELS];
static int	g_vertexlight_modnum;
static vertremap_t	*g_vertexlight_indexes;
static uint	g_vertexlight_numindexes;

static vec3_t g_box_directions[6] = 
{
{ 1.0f,  0.0f,  0.0f }, 
{-1.0f,  0.0f,  0.0f },
{ 0.0f,  1.0f,  0.0f }, 
{ 0.0f, -1.0f,  0.0f }, 
{ 0.0f,  0.0f,  1.0f }, 
{ 0.0f,  0.0f, -1.0f },
};

void NudgeVertexPosition( vec3_t position )
{
	vec3_t	test;
	int	x;

	VectorCopy( position, test );

	if( PointInLeaf( test ) != g_dleafs )
		return;

	for( x = 0; x < 6; x++ )
	{
		VectorMA( position, 2.0f, g_box_directions[x], test );
		if( PointInLeaf( test )->contents != CONTENTS_SOLID )
		{
			VectorCopy( test, position );
			return;
		}
	}

	for( x = 0; x < 6; x++ )
	{
		VectorMA( position, 3.0f, g_box_directions[x], test );
		if( PointInLeaf( test )->contents != CONTENTS_SOLID )
		{
			VectorCopy( test, position );
			return;
		}
	}
}

void GetStylesFromMesh( byte newstyles[4], const tmesh_t *mesh )
{
	ThreadLock();
	memcpy(newstyles, mesh->styles, sizeof(mesh->styles));
	ThreadUnlock();
}

void AddStylesToMesh( tmesh_t *mesh, byte newstyles[4] )
{
	int	i, j;

	// store styles back to the mesh
	ThreadLock();

	for( i = 0; i < MAXLIGHTMAPS && newstyles[i] != 255; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS; j++ )
		{
			if( mesh->styles[j] == newstyles[i] || mesh->styles[j] == 255 )
				break;
		}

		if( j == MAXLIGHTMAPS )
			continue;

		// allocate a new one					
		if( mesh->styles[j] == 255 )
			mesh->styles[j] = newstyles[i];
	}

	ThreadUnlock();
}

void SmoothModelNormals( int modelnum, int threadnum = -1 )
{
	int hashSize;
	entity_t	*mapent = g_vertexlight[modelnum];
	tmesh_t	*mesh;

	if( g_smoothing_threshold == 0 )
		return;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return;

	// build the smoothed normals
	if( FBitSet( mesh->flags, FMESH_DONT_SMOOTH ))
		return;

	vec3_t	*normals = (vec3_t *)Mem_Alloc( mesh->numverts * sizeof( vec3_t ));

	for( hashSize = 1; hashSize < mesh->numverts; hashSize <<= 1 );
	hashSize = hashSize >> 2;

	// build a map from vertex to a list of triangles that share the vert.
	CUtlArray<CIntVector> vertHashMap;

	vertHashMap.AddMultipleToTail( hashSize );

	for( int vertID = 0; vertID < mesh->numverts; vertID++ )
	{
		tvert_t *tv = &mesh->verts[vertID];
		uint hash = VertexHashKey( tv->point, hashSize );
		vertHashMap[hash].AddToTail( vertID );
	}

	for( int hashID = 0; hashID < hashSize; hashID++ )
	{
		for( int i = 0; i < vertHashMap[hashID].Size(); i++ )
		{
			int vertID = vertHashMap[hashID][i];
			tvert_t *tv0 = &mesh->verts[vertID];

			for( int j = 0; j < vertHashMap[hashID].Size(); j++ )
			{
				tvert_t *tv1 = &mesh->verts[vertHashMap[hashID][j]];

				if( !VectorCompareEpsilon( tv0->point, tv1->point, ON_EPSILON ))
					continue;

				if( DotProduct( tv0->normal, tv1->normal ) >= g_smoothing_threshold )
					VectorAdd( normals[vertID], tv1->normal, normals[vertID] );
			}
		}
	}

	// copy smoothed normals back
	for( int j = 0; j < mesh->numverts; j++ )
	{
		VectorCopy( normals[j], mesh->verts[j].normal );
		VectorNormalize2( mesh->verts[j].normal );
	}

	Mem_Free( normals );
}

void BuildVertexLights( int indexnum, int thread = -1 )
{
	int	modelnum = g_vertexlight_indexes[indexnum].modelnum;
	int	vertexnum = g_vertexlight_indexes[indexnum].vertexnum;
	entity_t	*mapent = g_vertexlight[modelnum];
	float	shadow[MAXLIGHTMAPS];
	vec3_t	light[MAXLIGHTMAPS];
	vec3_t	delux[MAXLIGHTMAPS];
	entity_t	*ignoreent = NULL;
	byte	*vislight = NULL;
	byte	styles[4];
	vec3_t	normal;
	tmesh_t	*mesh;
	int	j;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return; 

	if( !FBitSet( mesh->flags, FMESH_SELF_SHADOW ))
		ignoreent = mapent;

	GetStylesFromMesh( styles, mesh );

	// stats
	g_direct_luxels[thread]++;

#ifdef HLRAD_COMPUTE_VISLIGHTMATRIX
	vislight = mesh->vislight;
#endif
	// vertexlighting is easy. We don't needs to find valid points etc
	tvert_t	*tv = &mesh->verts[vertexnum];
	vec3_t	point;

	// not supposed for vertex lighting?
	if( !tv->light ) return;

	// nudge position from ground
	NudgeVertexPosition( tv->light->pos ); // nudged vertexes will be used on indirect lighting too
	VectorCopy( tv->normal, normal );

	// calculate visibility for the sample
	int	leaf = PointInLeaf( tv->light->pos ) - g_dleafs;

	if( FBitSet( mesh->flags, FMESH_SELF_SHADOW ))
		VectorMA( tv->light->pos, DEFAULT_HUNT_OFFSET, tv->normal, point );
	else VectorCopy( tv->light->pos, point );

	memset( light, 0, sizeof( light ));
	memset( delux, 0, sizeof( delux ));
	memset( shadow, 0, sizeof( shadow ));

	// gather direct lighting for our vertex
	GatherSampleLight( thread, -1, point, leaf, normal, light, delux, shadow, styles, vislight, 0, ignoreent );

	// add an ambient term if desired
	if( g_ambient[0] || g_ambient[1] || g_ambient[2] )
	{
		for( j = 0; j < MAXLIGHTMAPS && styles[j] == 255; j++ );
		if( j == MAXLIGHTMAPS ) styles[0] = 0; // adding style

		for( j = 0; j < MAXLIGHTMAPS && styles[j] != 255; j++ )
		{
			if( styles[j] == 0 )
			{
				VectorAdd( light[j], g_ambient, light[j] );
#ifdef HLRAD_DELUXEMAPPING
				vec_t avg = VectorAvg( g_ambient );
				VectorMA( delux[j], -DIFFUSE_DIRECTION_SCALE * avg, normal, delux[j] );
#endif
				break;
			}
		}
	}

	if( !g_lightbalance )
	{
		for( int k = 0; k < MAXLIGHTMAPS; k++ )
		{
			VectorScale( light[k], g_direct_scale, light[k] );
			VectorScale( delux[k], g_direct_scale, delux[k] );
		}
	}

	// grab indirect lighting for vertex from light_environment or lighting vertex in -fast mode
	GatherSampleLight( thread, -1, point, leaf, normal, light, delux, shadow, styles, NULL, 1, ignoreent );

	// store results back into the vertex
	for( j = 0; j < MAXLIGHTMAPS && styles[j] != 255; j++ )
	{
		VectorCopy( light[j], tv->light->light[j] );
		VectorCopy( delux[j], tv->light->deluxe[j] );
		tv->light->shadow[j] = shadow[j];
	}

	AddStylesToMesh( mesh, styles );
}

bool AddPatchStyleToMesh( trace_t *trace, tmesh_t *mesh, tvert_t *tv, vec3_t *s_light, vec3_t *s_dir, byte newstyles[4] )
{
	vec3_t	sampled_light[MAXLIGHTMAPS];
	vec3_t	sampled_dir[MAXLIGHTMAPS];
	int	i, lightstyles;
	vec_t	dot, total;
	int	numpatches;
	const int	*patches;
	vec3_t	v, dir;

	if( trace->surface < 0 || trace->surface >= g_numfaces )
		return false;

	GetTriangulationPatches( trace->surface, &numpatches, &patches ); // collect patches and their neighbors

	memset( sampled_light, 0, sizeof( sampled_light ));
	memset( sampled_dir, 0, sizeof( sampled_dir ));
	total = 0.0f;

	for( i = 0; i < numpatches; i++ )
	{
		patch_t	*p = &g_patches[patches[i]];

		if( FBitSet( p->flags, PATCH_OUTSIDE ))
			continue;

		for( int j = 0; j < MAXLIGHTMAPS && p->totalstyle[j] != 255; j++ )
		{
			if( p->samples[j] <= 0.0 )
				continue;

			for( lightstyles = 0; lightstyles < MAXLIGHTMAPS && newstyles[lightstyles] != 255; lightstyles++ )
			{
				if( newstyles[lightstyles] == p->totalstyle[j] )
					break;
			}

			if( lightstyles == MAXLIGHTMAPS )
			{
				// overflowed
				continue;
			}
			else if( newstyles[lightstyles] == 255 )
			{
				newstyles[lightstyles] = p->totalstyle[j];
			}

			const vec_t *b = GetTotalLight( p, p->totalstyle[j] );
			VectorScale( b, (1.0), v );
			VectorMultiply( v, p->reflectivity, v );
			VectorAdd( sampled_light[lightstyles], v, sampled_light[lightstyles] );

			const vec_t *d = GetTotalDirection(p, p->totalstyle[j] );
			VectorScale( d, (1.0), v );

			VectorCopy( v, dir );
			VectorNormalize( dir );
			dot = DotProduct( dir, tv->normal );

			if(( -dot ) > 0 )
			{
				// reflect the direction back (this is not ideal!)
				VectorMA( v, -(-dot) * 2.0f, tv->normal, v );
			}


			VectorAdd( sampled_dir[lightstyles], v, sampled_dir[lightstyles] );
			total++;
		}
	}

	// add light to vertex
	for( int k = 0; k < MAXLIGHTMAPS && newstyles[k] != 255; k++ )
	{
		VectorScale( sampled_light[k], 1.0f / total, sampled_light[k] );
		VectorScale( sampled_dir[k], 1.0f / total, sampled_dir[k] );

		if( VectorMaximum( sampled_light[k] ) >= EQUAL_EPSILON )
		{
			if( s_light ) VectorAdd( s_light[k], sampled_light[k], s_light[k] );
			if( s_dir ) VectorAdd( s_dir[k], sampled_dir[k], s_dir[k] );
		}
	}

	return true;
}

/*
============
VertexPatchLights

This function is run multithreaded
============
*/
void VertexPatchLights( int indexnum, int threadnum = -1 )
{
	int	modelnum = g_vertexlight_indexes[indexnum].modelnum;
	int	vertexnum = g_vertexlight_indexes[indexnum].vertexnum;
	vec3_t	*skynormals = g_skynormals[SKYLEVEL_SOFTSKYOFF];
	entity_t	*mapent = g_vertexlight[modelnum];
	vec3_t	sampled_light[MAXLIGHTMAPS];
	vec3_t	sampled_dir[MAXLIGHTMAPS];
	byte	newstyles[4];
	trace_t	besttrace;
	vec_t	total;
	trace_t	trace;
	tmesh_t	*mesh;
	vec3_t	delta;
	vec_t	dot;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return; 

	besttrace.surface = -1;
	besttrace.fraction = 1.0f;
	besttrace.contents = CONTENTS_EMPTY;
	GetStylesFromMesh( newstyles, mesh );

	tvert_t	*tv = &mesh->verts[vertexnum];

	// not supposed for vertex lighting?
	if( !tv->light ) return;

	memset( sampled_light, 0, sizeof( sampled_light ));
	memset( sampled_dir, 0, sizeof( sampled_dir ));
	total = 0.0f;

	// NOTE: we can't using patches directly because they linked to faces
	// we should find nearest face with matched normal and just nearest face
	// then lerp between them
	for( int j = 0; j < g_numskynormals[SKYLEVEL_SOFTSKYOFF]; j++ )
	{
		dot = -DotProduct( skynormals[j], tv->normal );
//		if( dot <= NORMAL_EPSILON ) continue;

		VectorScale( skynormals[j], -MAX_INDIRECT_DIST, delta );
		VectorAdd( tv->light->pos, delta, delta );

		TestLine( threadnum, tv->light->pos, delta, &trace );

		if( trace.surface == -1 )
			continue;

		if( trace.fraction < besttrace.fraction )
			besttrace = trace;

		AddPatchStyleToMesh( &trace, mesh, tv, sampled_light, sampled_dir, newstyles );
		total++;
	}

	if( total <= 0 ) return;

	// add light to vertex
	for( int k = 0; k < MAXLIGHTMAPS && mesh->styles[k] != 255; k++ )
	{
		VectorScale( sampled_light[k], 1.0f / total, sampled_light[k] );
		VectorScale( sampled_dir[k], 1.0f / total, sampled_dir[k] );
		VectorScale( sampled_light[k], g_indirect_scale, sampled_light[k] );
		VectorScale( sampled_dir[k], g_indirect_scale, sampled_dir[k] );

		if( VectorMaximum( sampled_light[k] ) >= EQUAL_EPSILON )
		{
			VectorAdd( tv->light->light[k], sampled_light[k], tv->light->light[k] );
			VectorAdd( tv->light->deluxe[k], sampled_dir[k], tv->light->deluxe[k] );
		}
	}

	AddStylesToMesh( mesh, newstyles );
}

void FinalLightVertex( int modelnum, int threadnum = -1 )
{
	entity_t		*mapent = g_vertexlight[modelnum];
	int		usedstyles[MAXLIGHTMAPS];
	vec3_t		lb, v, direction;
	int		lightstyles;
	vec_t		minlight;
	tmesh_t		*mesh;
	dmodelvertlight_t	*dml;
	dvlightlump_t	*l;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return; 

	for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
	{
		if( mesh->styles[lightstyles] == 255 )
			break;
	}

	// completely black model
	if( !lightstyles ) return;

	memset( usedstyles, 0, sizeof( usedstyles ));

	l = (dvlightlump_t *)g_dvlightdata;
	ASSERT( l->dataofs[modelnum] != -1 );

	dml = (dmodelvertlight_t *)((byte *)g_dvlightdata + l->dataofs[modelnum]);
	ASSERT( dml->numverts == mesh->numverts );

	minlight = FloatForKey( mapent, "_minlight" );
	if( minlight < 1.0 ) minlight *= 128.0f; // GoldSrc
	else minlight *= 0.5f; // Quake

	if( g_lightbalance )
		minlight *= g_direct_scale;
	if( g_numbounce > 0 ) minlight = 0.0f; // ignore for radiosity

	// vertexlighting is easy. We don't needs to find valid points etc
	for( int i = 0; i < mesh->numverts; i++ )
	{
		tvert_t		*tv = &mesh->verts[i];
		dvertlight_t	*dvl = &dml->verts[i];
		int		j;

		if( !tv->light ) continue;

		for( int k = 0; k < lightstyles; k++ )
		{
			VectorCopy( tv->light->light[k], lb ); 
			VectorCopy( tv->light->deluxe[k], direction );

			if( VectorMax( lb ) > EQUAL_EPSILON ) 
				usedstyles[k]++;

			if( g_lightbalance )
			{
				VectorScale( lb, g_direct_scale, lb );
				VectorScale( direction, g_direct_scale, direction );
			}

			vec_t avg = VectorAvg( lb );
			VectorScale( direction, 1.0 / Q_max( 1.0, avg ), direction );

			// clip from the bottom first
			lb[0] = Q_max( lb[0], minlight );
			lb[1] = Q_max( lb[1], minlight );
			lb[2] = Q_max( lb[2], minlight );

			// clip from the top
			if( lb[0] > g_maxlight || lb[1] > g_maxlight || lb[2] > g_maxlight )
			{
				// find max value and scale the whole color down;
				float	max = VectorMax( lb );

				for( j = 0; j < 3; j++ )
					lb[j] = ( lb[j] * g_maxlight ) / max;
			}

			// do gamma adjust
			lb[0] = (float)pow( lb[0] / 256.0f, g_gamma ) * 256.0f;
			lb[1] = (float)pow( lb[1] / 256.0f, g_gamma ) * 256.0f;
			lb[2] = (float)pow( lb[2] / 256.0f, g_gamma ) * 256.0f;
#ifdef HLRAD_RIGHTROUND
			dvl->light[k].r = Q_rint( lb[0] );
			dvl->light[k].g = Q_rint( lb[1] );
			dvl->light[k].b = Q_rint( lb[2] );
#else
			dvl->light[k].r = (byte)lb[0];
			dvl->light[k].g = (byte)lb[1];
			dvl->light[k].b = (byte)lb[2];
#endif
			VectorScale( direction, 0.225, v ); // the scale is calculated such that length( v ) < 1

			if( DotProduct( v, v ) > ( 1.0 - NORMAL_EPSILON ))
				VectorNormalize( v );

			VectorNegate( v, v ); // let the direction point from face sample to light source

			// keep deluxe vectors in modelspace for vertex lighting
			for( int x = 0; x < 3; x++ )
			{
				lb[x] = v[x] * 127.0f + 128.0f;
				lb[x] = bound( 0, lb[x], 255.0 );
			}

			dvl->deluxe[k].r = (byte)lb[0];
			dvl->deluxe[k].g = (byte)lb[1];
			dvl->deluxe[k].b = (byte)lb[2];

			// shadows are simple
			dvl->shadow[k] = tv->light->shadow[k] * 255;
		}
	}

	for( int k = 0; k < lightstyles; k++ )
	{
		if( usedstyles[k] == 0 )
			mesh->styles[k] = 255;
	}
}

static void GenerateLightCacheNumbers( void )
{
	char	string[32];
	tmesh_t	*mesh;
	int	i, j;

	for( i = 1; i < g_numentities; i++ )
	{
		entity_t	*mapent = &g_entities[i];

		// no cache - no lighting
		if( !mapent->cache ) continue;

		mesh = (tmesh_t *)mapent->cache;

		if( mapent->modtype != mod_alias && mapent->modtype != mod_studio )
			continue;

		if( !mesh->verts || mesh->numverts <= 0 )
			continue; 

		if( !FBitSet( mesh->flags, FMESH_VERTEX_LIGHTING ))
			continue;

		short lightid = g_vertexlight_modnum++;
		// at this point we have valid target for vertex lighting
		Q_snprintf( string, sizeof( string ), "%i", lightid + 1 );
		SetKeyValue( mapent, "vlight_cache", string );
		g_vertexlight[lightid] = mapent;
	}

	g_vertexlight_numindexes = 0;

	// generate remapping table for more effective CPU utilize
	for( i = 0; i < g_vertexlight_modnum; i++ )
	{
		entity_t	*mapent = g_vertexlight[i];
		mesh = (tmesh_t *)mapent->cache;
		g_vertexlight_numindexes += mesh->numverts;
	}

	g_vertexlight_indexes = (vertremap_t *)Mem_Alloc( g_vertexlight_numindexes * sizeof( vertremap_t ));
	uint curIndex = 0;

	for( i = 0; i < g_vertexlight_modnum; i++ )
	{
		entity_t	*mapent = g_vertexlight[i];
		mesh = (tmesh_t *)mapent->cache;

		// encode model as lowpart and vertexnum as highpart
		for( j = 0; j < mesh->numverts; j++ )
		{
			g_vertexlight_indexes[curIndex].modelnum = i;
			g_vertexlight_indexes[curIndex].vertexnum = j;
			curIndex++;
		}
		ASSERT( curIndex <= g_vertexlight_numindexes );
	}
}

static int ModelSize( tmesh_t *mesh )
{
	int lightstyles;
	if( !mesh ) return 0;

	if( !mesh->verts || mesh->numverts <= 0 )
		return 0; 

	for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
	{
		if( mesh->styles[lightstyles] == 255 )
			break;
	}

	// model is valid but completely not lighted by direct
	if( !lightstyles ) return 0;

	return sizeof( dmodelvertlight_t ) - ( sizeof( dvertlight_t ) * 3 ) + sizeof( dvertlight_t ) * mesh->numverts + ((g_numworldlights + 7) / 8);
}

static int WriteModelLight( tmesh_t *mesh, byte *out )
{
	int		size = ModelSize( mesh );
	dmodelvertlight_t	*dml;

	if( !size ) return 0;

	dml = (dmodelvertlight_t *)out;
	out += sizeof( dmodelvertlight_t ) - ( sizeof( dvertlight_t ) * 3 ) + sizeof( dvertlight_t ) * mesh->numverts;

	dml->modelCRC = mesh->modelCRC;
	dml->numverts = mesh->numverts;

	memcpy( dml->styles, mesh->styles, sizeof( dml->styles ));
	memcpy( dml->submodels, mesh->vsubmodels, sizeof( dml->submodels ));
	memcpy( out, mesh->vislight, ((g_numworldlights + 7) / 8));

	return size;
}

static void AllocVertexLighting( void )
{
	int		totaldatasize = ( sizeof( int ) * 3 ) + ( sizeof( int ) * g_vertexlight_modnum );
	int		i, len;
	byte		*data;
	dvlightlump_t	*l;

	for( i = 0; i < g_vertexlight_modnum; i++ )
	{
		entity_t	*mapent = g_vertexlight[i];

		// sanity check
		if( !mapent ) continue;

		len = ModelSize( (tmesh_t *)mapent->cache );
		totaldatasize += len;
	}

	Msg( "total vertexlight data: %s\n", Q_memprint( totaldatasize ));
	g_dvlightdata = (byte *)Mem_Realloc( g_dvlightdata, totaldatasize );

	// now setup to get the miptex data (or just the headers if using -wadtextures) from the wadfile
	l = (dvlightlump_t *)g_dvlightdata;
	data = (byte *)&l->dataofs[g_vertexlight_modnum];

	l->ident = VLIGHTIDENT;
	l->version = VLIGHT_VERSION;
	l->nummodels = g_vertexlight_modnum;

	for( i = 0; i < g_vertexlight_modnum; i++ )
	{
		entity_t	*mapent = g_vertexlight[i];

		l->dataofs[i] = data - (byte *)l;
		len = WriteModelLight( (tmesh_t *)mapent->cache, data );
		if( !len ) l->dataofs[i] = -1; // completely black model
		data += len;
	}

	g_vlightdatasize = data - g_dvlightdata;

	if( totaldatasize != g_vlightdatasize )
		COM_FatalError( "WriteVertexLighting: memory corrupted\n" );

	// vertex cache acesss
	// const char *id = ValueForKey( ent, "vlight_cache" );
	// int cacheID = atoi( id ) - 1;
	// if( cacheID < 0 || cacheID > num_map_entities ) return;	// bad cache num
	// if( l->dataofs[cacheID] == -1 ) return;		// cache missed
	// otherwise it's valid
}

void BuildVertexLights( void )
{
	GenerateLightCacheNumbers();

	// new code is very fast, so no reason to show progress
	RunThreadsOnIndividual( g_vertexlight_modnum, false, SmoothModelNormals );

	if( !g_vertexlight_numindexes ) return;

	RunThreadsOnIndividual( g_vertexlight_numindexes, true, BuildVertexLights );
}

void VertexPatchLights( void )
{
	if( !g_vertexlight_numindexes ) return;

	RunThreadsOnIndividual( g_vertexlight_numindexes, true, VertexPatchLights );
}

void FinalLightVertex( void )
{
	if( !g_vertexlight_modnum ) return;

	AllocVertexLighting();

	RunThreadsOnIndividual( g_vertexlight_modnum, true, FinalLightVertex );

	Mem_Free( g_vertexlight_indexes );
	g_vertexlight_indexes = NULL;
	g_vertexlight_numindexes = 0;
}
#endif
