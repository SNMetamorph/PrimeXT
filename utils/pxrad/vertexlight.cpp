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

#define MAX_INDIRECT_DIST	131072.0f

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

void VertexDirectLighting( int indexnum, int thread = -1 )
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



	// two-sided verts pos will be adjusted later
	if( tv->twosided )
		VectorCopy( tv->light->pos, point );
	else
	{
		VectorMA( tv->light->pos, DEFAULT_HUNT_OFFSET, tv->normal, point );
		VectorCopy( point, tv->light->pos );

		//check overlapping by back faces
		if( FBitSet( mesh->flags, FMESH_SELF_SHADOW )&&( !g_studiolegacy ) )
		{
			vec3_t	trace_end;
			vec3_t	trace_dir;
			trace_t	trace;		

			VectorSubtract( point, tv->point, trace_dir );
			VectorAdd( trace_dir, tv->normal, trace_dir );
			VectorNormalize2( trace_dir );

			VectorMA( point, 8.0f, trace_dir, trace_end );

			for( int i = 0; i < 8; i++ )
			{
				trace.contents = CONTENTS_EMPTY;
				mesh->rayBVH.TraceRay( point, trace_end, &trace, false );
				if( (trace.contents == CONTENTS_SOLID)&&(trace.surface == -1) )
				{	
					VectorLerp( point, trace.fraction, trace_end, point );
					VectorMA( point, DEFAULT_HUNT_OFFSET, trace_dir, point );
				}
				else
					break;
			}		

			VectorCopy( point, tv->light->pos );
		}
	}
	
	// calculate visibility for the sample
	int	leaf = PointInLeaf( tv->light->pos ) - g_dleafs;

	memset( light, 0, sizeof( light ));
	memset( delux, 0, sizeof( delux ));
	memset( shadow, 0, sizeof( shadow ));

	// gather direct lighting for our vertex
	if( tv->twosided )
		GatherSampleLight( thread, -2, &point, leaf, normal, light, delux, shadow, styles, vislight, 0, ignoreent );
	else
		GatherSampleLight( thread, -1, &point, leaf, normal, light, delux, shadow, styles, vislight, 0, ignoreent );

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

	// store results back into the vertex
	for( j = 0; j < MAXLIGHTMAPS && styles[j] != 255; j++ )
	{
		VectorCopy( light[j], tv->light->light[j] );
		VectorCopy( delux[j], tv->light->deluxe[j] );
		tv->light->shadow[j] = shadow[j];
	}

	AddStylesToMesh( mesh, styles );
}



/*
============
VertexIndirectGather

This function is run multithreaded
============
*/
void VertexIndirectGather( int indexnum, int threadnum = -1 )
{
	int	modelnum = g_vertexlight_indexes[indexnum].modelnum;
	int	vertexnum = g_vertexlight_indexes[indexnum].vertexnum;
	vec3_t	*skynormals = &g_skynormals_random[STUDIO_SAMPLES_PER_PASS * (g_studiogipasscounter - 1)];
	int		skynormalscount = (g_numstudiobounce > 0) ? STUDIO_SAMPLES_PER_PASS : STUDIO_SAMPLES_SKY;
	entity_t	*mapent = g_vertexlight[modelnum];
	entity_t	*ignoreent = NULL;	
	vec3_t	sampled_light[MAXLIGHTMAPS];
	vec3_t	sampled_dir[MAXLIGHTMAPS];
	byte	newstyles[4];
	int		total;
	trace_t	trace;
	tmesh_t	*mesh;
	vec3_t	delta;
	vec_t	dot, avg;
	int		lightstyles;
	vec3_t	temp_color;
	vec3_t	trace_pos;	
	directlight_t *dl;

	// sanity check
	if( !mapent || !mapent->cache )
		return;

	mesh = (tmesh_t *)mapent->cache;
	if( !mesh->verts || mesh->numverts <= 0 )
		return; 

	ignoreent = mapent;		

	GetStylesFromMesh( newstyles, mesh );

	tvert_t	*tv = &mesh->verts[vertexnum];

	// not supposed for vertex lighting?
	if( !tv->light ) return;

	memset( sampled_light, 0, sizeof( sampled_light ));
	memset( sampled_dir, 0, sizeof( sampled_dir ));
	total = 0;

	for( int j = 0; j < skynormalscount; j++, skynormals++ )
	{
		dot = DotProduct( *skynormals, tv->normal );

		if( tv->twosided )
			dot = 1.0f;

		if( dot <= NORMAL_EPSILON ) continue;

		VectorScale( *skynormals, MAX_INDIRECT_DIST, delta );
		VectorAdd( tv->light->pos, delta, delta );

		VectorMA( tv->light->pos, 0.5f, *skynormals,  trace_pos );

		if( !tv->twosided )
		{		
			TestLine( threadnum, tv->light->pos, trace_pos, &trace, false, ignoreent );
			if( trace.contents != CONTENTS_EMPTY )
				VectorCopy( tv->light->pos, trace_pos );
		}

		TestLine( threadnum, trace_pos, delta, &trace, false, ignoreent );

		if( trace.contents == CONTENTS_SKY )
		{
			if((g_indirect_sun <= 0.0) || (g_numskylights <= 0 && !g_envsky))
				continue;

			for( lightstyles = 0; lightstyles < MAXLIGHTMAPS && newstyles[lightstyles] != 255; lightstyles++ )
				if( newstyles[lightstyles] == g_skystyle )
					break;
			if( lightstyles == MAXLIGHTMAPS )	// overflowed
				continue;
			else if( newstyles[lightstyles] == 255 )
				newstyles[lightstyles] = g_skystyle;

			GetSkyColor( *skynormals, temp_color );

			VectorScale( temp_color, dot * g_indirect_sun, temp_color );				

			VectorAdd( sampled_light[lightstyles], temp_color, sampled_light[lightstyles] ); 
			avg = VectorAvg(temp_color);
			VectorMA( sampled_dir[lightstyles], avg, *skynormals, sampled_dir[lightstyles]);
		}
		else if( trace.surface == -1 )
			continue;
		else if( g_numstudiobounce > 0 && trace.surface == STUDIO_SURFACE_HIT )	//studio gi
		{
			for (int i = 0; i < MAXLIGHTMAPS; i++ )
			{	
				for( lightstyles = 0; lightstyles < MAXLIGHTMAPS && newstyles[lightstyles] != 255; lightstyles++ )
					if( newstyles[lightstyles] == trace.styles[i] )
						break;
				if( lightstyles == MAXLIGHTMAPS )	// overflowed
					continue;
				else if( newstyles[lightstyles] == 255 )
					newstyles[lightstyles] = trace.styles[i];	

				VectorScale( trace.light[i], dot, temp_color );

				VectorAdd( sampled_light[lightstyles], temp_color, sampled_light[lightstyles] ); 
				avg = VectorAvg(temp_color);
				VectorMA( sampled_dir[lightstyles], avg, *skynormals, sampled_dir[lightstyles]);	
			}
		}
		else if( g_numbounce > 0 )
		{
			if( !g_facelight[trace.surface].samples ) continue;	//there are no facelights in the prepass

			lightpoint_t info;
			vec3_t	hitpoint;
			vec_t	scaleAvg;
			dface_t *face = g_dfaces + trace.surface;

			VectorLerp( trace_pos, trace.fraction, delta, hitpoint );

			if( !R_GetDirectLightFromSurface( face, hitpoint, &info, true ))
				continue;

			scaleAvg = CalcFaceSolidAngle( face, trace_pos );

			if( scaleAvg <= 0.0f )
				continue;
			scaleAvg = 4.0f * M_PI / ((float)skynormalscount * scaleAvg);	//ratio of ray cone and face solid angles
			scaleAvg = bound( 0.0f, scaleAvg, 1.0f );

			for (int i = 0; i < MAXLIGHTMAPS; i++ )
			{	
				for( lightstyles = 0; lightstyles < MAXLIGHTMAPS && newstyles[lightstyles] != 255; lightstyles++ )
					if( newstyles[lightstyles] == info.styles[i] )
						break;
				if( lightstyles == MAXLIGHTMAPS )	// overflowed
					continue;
				else if( newstyles[lightstyles] == 255 )
					newstyles[lightstyles] = info.styles[i];

				VectorLerp( info.diffuse[i], scaleAvg, info.average[i], temp_color );	

				VectorScale( temp_color, dot, temp_color );

				VectorAdd( sampled_light[lightstyles], temp_color, sampled_light[lightstyles] ); 
				avg = VectorAvg(temp_color);
				VectorMA( sampled_dir[lightstyles], avg, *skynormals, sampled_dir[lightstyles]);	
			}
		}
		
		total++;
	}

	if( total <= 0 ) 
	{
		//Msg( "sheeeit %f %f %f\n", tv->normal[0], tv->normal[1], tv->normal[2]);
		return;
	}

	// add light to vertex
	for( int k = 0; k < MAXLIGHTMAPS && mesh->styles[k] != 255; k++ )
	{
		VectorScale( sampled_light[k], 4.0f / (float)skynormalscount, sampled_light[k] );
		VectorScale( sampled_dir[k], 4.0f / (float)skynormalscount, sampled_dir[k] );

		VectorLerp( tv->light->gi[k], 1.0f / (float)g_studiogipasscounter, sampled_light[k], tv->light->gi[k] );
		VectorLerp( tv->light->gi_dlx[k], 1.0f / (float)g_studiogipasscounter, sampled_dir[k], tv->light->gi_dlx[k]);
	}

	AddStylesToMesh( mesh, newstyles );
}

void VertexIndirectBlend( int modelnum, int threadnum = -1 )
{
	entity_t	*mapent = g_vertexlight[modelnum];
	tmesh_t		*mesh;
	int			lightstyles;	

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

	for( int i = 0; i < mesh->numverts; i++ )
	{
		tvert_t		*tv = &mesh->verts[i];
		if( !tv->light ) continue;

		for( int k = 0; k < lightstyles; k++ )
		{
			VectorSubtract( tv->light->light[k], tv->light->gi_prev[k], tv->light->light[k] );
			VectorAdd( tv->light->light[k], tv->light->gi[k], tv->light->light[k] );
			VectorCopy( tv->light->gi[k], tv->light->gi_prev[k] );

			if( g_studiogipasscounter >= g_numstudiobounce )
				VectorAdd( tv->light->deluxe[k], tv->light->gi_dlx[k], tv->light->deluxe[k] );	
		}
	}
}

//for vertex sorting and blur
int CompareVertexPointers(const void* a,const void* b) 
{
	tvert_t	*v1 = *(tvert_t**)a;
	tvert_t	*v2 = *(tvert_t**)b;

	if( v1->point[0] > v2->point[0] ) return 1;
	if( v1->point[0] < v2->point[0] ) return -1;
	if( v1->point[1] > v2->point[1] ) return 1;
	if( v1->point[1] < v2->point[1] ) return -1;
	if( v1->point[2] > v2->point[2] ) return 1;
	if( v1->point[2] < v2->point[2] ) return -1;

	if( v1->normal[0] > v2->normal[0] ) return 1;
	if( v1->normal[0] < v2->normal[0] ) return -1;
	if( v1->normal[1] > v2->normal[1] ) return 1;
	if( v1->normal[1] < v2->normal[1] ) return -1;
	if( v1->normal[2] > v2->normal[2] ) return 1;
	if( v1->normal[2] < v2->normal[2] ) return -1;

	return 0;
}

void FinalLightVertex( int modelnum, int threadnum = -1 )
{
	entity_t	*mapent = g_vertexlight[modelnum];
	int			usedstyles[MAXLIGHTMAPS];
	vec3_t		lb, v, direction;
	int			lightstyles;
	vec_t		minlight;
	bool		vertexblur;
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

	vertexblur = BoolForKey( mapent, "zhlt_vertexblur" );
	vertexblur = vertexblur || g_vertexblur;

	if( vertexblur )
	{
		for( int i = 0; i < mesh->numverts; i++ )
			mesh->verts[i].light->face_counter = 0;

		for( int i = 0; i < mesh->numfaces; i++ )	
		{
			tface_t	*face = &mesh->faces[i];
			tvert_t	*tv1 = &mesh->verts[face->a];
			tvert_t	*tv2 = &mesh->verts[face->b];
			tvert_t	*tv3 = &mesh->verts[face->c];
			
			tv1->light->face_counter++;
			tv2->light->face_counter++;
			tv3->light->face_counter++;

			for( int k = 0; k < lightstyles; k++ )
			{
				vec3_t	face_light = {0,0,0};
				vec3_t	face_deluxe = {0,0,0};

				//reuse gi and gi_dlx for blurred lighting and deluxe

				VectorMA( face_light, 0.333333f, tv1->light->light[k] , face_light );
				VectorMA( face_light, 0.333333f, tv2->light->light[k] , face_light );
				VectorMA( face_light, 0.333334f, tv3->light->light[k] , face_light );
				VectorLerp( tv1->light->gi[k],  1.0f / (float)tv1->light->face_counter, face_light, tv1->light->gi[k] );
				VectorLerp( tv2->light->gi[k],  1.0f / (float)tv2->light->face_counter, face_light, tv2->light->gi[k] );
				VectorLerp( tv3->light->gi[k],  1.0f / (float)tv3->light->face_counter, face_light, tv3->light->gi[k] );			

				VectorMA( face_deluxe, 0.333333f, tv1->light->deluxe[k] , face_deluxe );
				VectorMA( face_deluxe, 0.333333f, tv2->light->deluxe[k] , face_deluxe );
				VectorMA( face_deluxe, 0.333334f, tv3->light->deluxe[k] , face_deluxe );
				VectorLerp( tv1->light->gi_dlx[k],  1.0f / (float)tv1->light->face_counter, face_deluxe, tv1->light->gi_dlx[k] );
				VectorLerp( tv2->light->gi_dlx[k],  1.0f / (float)tv2->light->face_counter, face_deluxe, tv2->light->gi_dlx[k] );
				VectorLerp( tv3->light->gi_dlx[k],  1.0f / (float)tv3->light->face_counter, face_deluxe, tv3->light->gi_dlx[k] );				
			}
		}
		for( int i = 0; i < mesh->numverts; i++ )
		{
			tvert_t	*tv = &mesh->verts[i];
			for( int k = 0; k < lightstyles; k++ )	
				if( (DotProduct( tv->light->gi_dlx[k], tv->normal ) > 0.0f)||tv->twosided )	//do not blur if blurred deluxe goes negative
				{
					VectorCopy( tv->light->gi[k], tv->light->light[k] );
					VectorCopy( tv->light->gi_dlx[k], tv->light->deluxe[k] );
				}
		}
	}
	
	//combine lighting on coincident vertices
	tvert_t		**vert_ids;
	vert_ids = new tvert_t*[mesh->numverts];
	for( int i = 0; i < mesh->numverts; i++ )
		vert_ids[i] = &mesh->verts[i];

	qsort( &vert_ids[0], mesh->numverts, sizeof(tvert_t*), CompareVertexPointers );

	int counter = 1;
	for( int i = 1; i < mesh->numverts; i++ )
	{
		if( CompareVertexPointers( &vert_ids[i-1], &vert_ids[i] ) == 0 )
		{
			counter += 1;
			for( int k = 0; k < lightstyles; k++ )
			{
				VectorLerp( vert_ids[i-1]->light->light[k], 1.0f / (float)counter, vert_ids[i]->light->light[k], vert_ids[i]->light->light[k] );
				VectorLerp( vert_ids[i-1]->light->deluxe[k], 1.0f / (float)counter, vert_ids[i]->light->deluxe[k], vert_ids[i]->light->deluxe[k] );
			}
		}
		else if( counter > 1 )
		{
			for( int j = i - counter; j < i - 1; j++ )
				for( int k = 0; k < lightstyles; k++ )
				{
					VectorCopy( vert_ids[i-1]->light->light[k], vert_ids[j]->light->light[k] );
					VectorCopy( vert_ids[i-1]->light->deluxe[k], vert_ids[j]->light->deluxe[k] );
				}
			counter = 1;
		}
	}
	if( counter > 1 )
	{
		for( int j = mesh->numverts - counter; j < mesh->numverts - 1; j++ )		
			for( int k = 0; k < lightstyles; k++ )
			{
				VectorCopy( vert_ids[mesh->numverts-1]->light->light[k], vert_ids[j]->light->light[k] );
				VectorCopy( vert_ids[mesh->numverts-1]->light->deluxe[k], vert_ids[j]->light->deluxe[k] );
			}
	}

	delete[] vert_ids;	





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


			if( VectorMax( lb ) > 0.0f )
				usedstyles[k]++;

			if( g_lightbalance )
			{
				VectorScale( lb, g_direct_scale, lb );
				VectorScale( direction, g_direct_scale, direction );
			}

			vec_t avg = VectorAvg( lb );
			if (avg > 0.0f)
				VectorScale( direction, 1.0f / avg, direction );		

			if( g_delambert )
			{
				VectorCopy( direction, v );
				VectorNormalize( v );
				vec_t lambert = DotProduct( v, tv->normal );

				if( lambert > NORMAL_EPSILON )
					VectorScale( lb, 1.0f / lambert, lb );					
			}	

			ApplyLightPostprocess( lb, minlight );
			

#ifdef HLRAD_RIGHTROUND
			dvl->light[k].r = Q_rint( lb[0] );
			dvl->light[k].g = Q_rint( lb[1] );
			dvl->light[k].b = Q_rint( lb[2] );
#else
			dvl->light[k].r = (byte)lb[0];
			dvl->light[k].g = (byte)lb[1];
			dvl->light[k].b = (byte)lb[2];
#endif

			VectorCopy( direction, v );

			if( DotProduct( v, v ) > ( 1.0 - NORMAL_EPSILON ))
				VectorNormalize( v );

			if( VectorIsNull( v ) )
				VectorCopy( tv->normal, v );

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

void VertexDirectLighting( void )
{
	GenerateLightCacheNumbers();

	if( !g_vertexlight_numindexes ) return;

	RunThreadsOnIndividual( g_vertexlight_numindexes, true, VertexDirectLighting );
}

void VertexIndirectGather( void )
{
	if( !g_vertexlight_numindexes ) return;

	RunThreadsOnIndividual( g_vertexlight_numindexes, true, VertexIndirectGather );
}

void VertexIndirectBlend( void )
{
	if( !g_vertexlight_modnum ) return;

	RunThreadsOnIndividual( g_vertexlight_modnum, true, VertexIndirectBlend );
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
