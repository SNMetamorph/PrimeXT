/*
gl_slight.cpp - static lighting
Copyright (C) 2019 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include <stringlib.h>
#include "utils.h"
#include "pm_defs.h"
#include "event_api.h"
#include "gl_local.h"
#include "gl_studio.h"
#include "gl_world.h"
#include "gl_cvars.h"

#define NUMVERTEXNORMALS	162
#define AMBIENT_CUBE_SCALE	1.0f

// lightpoint flags
#define LIGHTINFO_HITSKY	(1<<0)
#define LIGHTINFO_AVERAGE	(1<<1)	// compute average color from lightmap
#define LIGHTINFO_STYLES	(1<<2)

typedef struct
{
	vec3_t		diffuse;
	vec3_t		average;
	colorVec		lightmap;
	vec3_t		normal;	
	msurface_t	*surf;	// may be NULL
	float		fraction;	// hit fraction
	vec3_t		origin;
	int		status;
} lightpoint_t;
 
static vec_t g_anorms[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

static Vector g_BoxDirections[6] = 
{
Vector( 1.0f,  0.0f,  0.0f ), 
Vector(-1.0f,  0.0f,  0.0f ),
Vector( 0.0f,  1.0f,  0.0f ), 
Vector( 0.0f, -1.0f,  0.0f ), 
Vector( 0.0f,  0.0f,  1.0f ), 
Vector( 0.0f,  0.0f, -1.0f ),
};

/*
=======================================================================

	AMBIENT & DIFFUSE LIGHTING

=======================================================================
*/
/*
=================
R_FindAmbientSkyLight
=================
*/
static mworldlight_t *R_FindAmbientSkyLight( void )
{
	// find any ambient lights
	for( int i = 0; i < world->numworldlights; i++ )
	{
		if( world->worldlights[i].emittype == emit_skylight )
			return &world->worldlights[i];
	}

	return NULL;
}

/*
=================
R_LightTraceFilter
=================
*/
static int R_LightTraceFilter( physent_t *pe )
{
	if( !pe || !pe->model )
		return 1;
	return 0;
}

/*
=================
R_GetDirectLightFromSurface
=================
*/
static bool R_GetDirectLightFromSurface( msurface_t *surf, const Vector &point, lightpoint_t *info )
{
	mextrasurf_t *es = surf->info;
	float ds, dt, s, t;
	color24 *lm, *dm;
	mtexinfo_t *tex;
	int map, size;

	tex = surf->texinfo;

	if( FBitSet( surf->flags, SURF_DRAWSKY ))
	{
		info->status |= LIGHTINFO_HITSKY;
		return false; // no lightmaps
	}

	if( FBitSet( surf->flags, SURF_DRAWTILED ))
		return false; // no lightmaps

	s = DotProduct( point, es->lmvecs[0] ) + es->lmvecs[0][3];
	t = DotProduct( point, es->lmvecs[1] ) + es->lmvecs[1][3];

	if( s < es->lightmapmins[0] || t < es->lightmapmins[1] )
		return false;

	ds = s - es->lightmapmins[0];
	dt = t - es->lightmapmins[1];

	if( ds > es->lightextents[0] || dt > es->lightextents[1] )
		return false;

	if( !surf->samples )
		return true;

	int sample_size = Mod_SampleSizeForFace( surf );
	int smax = (es->lightextents[0] / sample_size) + 1;
	int tmax = (es->lightextents[1] / sample_size) + 1;
	ds /= sample_size;
	dt /= sample_size;

	lm = surf->samples + Q_rint( dt ) * smax + Q_rint( ds );
	colorVec localDiffuse = { 0, 0, 0, 0 };
	size = smax * tmax;

	if( es->normals )
	{
		Vector vec_x, vec_y, vec_z;
		matrix3x3	mat;

		dm = es->normals + Q_rint( dt ) * smax + Q_rint( ds );

		// flat TBN for better results
		vec_x = Vector( surf->info->lmvecs[0] );
		vec_y = Vector( surf->info->lmvecs[1] );
		if( FBitSet( surf->flags, SURF_PLANEBACK ))
			vec_z = -surf->plane->normal;
		else vec_z = surf->plane->normal;

		// create tangent space rotational matrix
		mat.SetForward( vec_x.Normalize( ));
		mat.SetRight( -vec_y.Normalize( ));
		mat.SetUp( vec_z.Normalize( ));

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			float f = (1.0f / 128.0f);
			Vector normal = Vector(((float)dm->r - 128.0f) * f, ((float)dm->g - 128.0f) * f, ((float)dm->b - 128.0f) * f);
			uint scale = tr.lightstyle[surf->styles[map]]; // style modifier
			mworldlight_t *pSkyLight = NULL;
			Vector localDir = g_vecZero;
			Vector sky_color = g_vecZero;

			// rotate from tangent to model space
			localDir = mat.VectorRotate( normal );

			// add local dir into main
			info->normal += localDir * (float)scale; // direction factor

			// compute diffuse color
			localDiffuse.r += TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
			localDiffuse.g += TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
			localDiffuse.b += TEXTURE_TO_TEXGAMMA( lm->b ) * scale;

			lm += size; // skip to next lightmap
			dm += size; // skip to next deluxemap
		}

		info->lightmap.r = Q_min(( localDiffuse.r >> 7 ), 255 );
		info->lightmap.g = Q_min(( localDiffuse.g >> 7 ), 255 );
		info->lightmap.b = Q_min(( localDiffuse.b >> 7 ), 255 );
		info->diffuse.x = (float)info->lightmap.r * (1.0f / 255.0f);
		info->diffuse.y = (float)info->lightmap.g * (1.0f / 255.0f);
		info->diffuse.z = (float)info->lightmap.b * (1.0f / 255.0f);
		if( info->normal == g_vecZero ) info->normal = vec_z; // fixup some cases
		info->normal = info->normal.Normalize();
	}
	else
	{
		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			uint	scale = tr.lightstyle[surf->styles[map]];

			// compute diffuse color
			localDiffuse.r += TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
			localDiffuse.g += TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
			localDiffuse.b += TEXTURE_TO_TEXGAMMA( lm->b ) * scale;
			lm += size; // skip to next lightmap
		}

		info->lightmap.r = Q_min(( localDiffuse.r >> 7 ), 255 );
		info->lightmap.g = Q_min(( localDiffuse.g >> 7 ), 255 );
		info->lightmap.b = Q_min(( localDiffuse.b >> 7 ), 255 );
		info->diffuse.x = (float)info->lightmap.r * (1.0f / 255.0f);
		info->diffuse.y = (float)info->lightmap.g * (1.0f / 255.0f);
		info->diffuse.z = (float)info->lightmap.b * (1.0f / 255.0f);
	}

	if(( surf->styles[0] != 0 && surf->styles[0] != LS_SKY ) || surf->styles[1] != 255 )
		SetBits( info->status, LIGHTINFO_STYLES );

	// also collect the average value
	if( FBitSet( info->status, LIGHTINFO_AVERAGE ))
	{
		Vector localAverage = g_vecZero;
		lm = surf->samples;

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			float	scale = tr.lightstyle[surf->styles[map]];

			for( int i = 0; i < size; i++, lm++ )
			{
				localAverage.x += (float)TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
				localAverage.y += (float)TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
				localAverage.z += (float)TEXTURE_TO_TEXGAMMA( lm->b ) * scale;
			}

			localAverage *= (1.0f / (float)size );
		}

		info->average.x = Q_min( localAverage.x * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
		info->average.y = Q_min( localAverage.y * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
		info->average.z = Q_min( localAverage.z * (1.0f / 128.0f), 255.0f ) * (1.0f / 255.0f);
	}

	info->surf = surf;

	return true;
}

/*
=================
R_RecursiveLightPoint
=================
*/
static bool R_RecursiveLightPoint( model_t *model, mnode_t *node, float p1f, float p2f, const Vector &start, const Vector &end, lightpoint_t *info )
{
	float		front, back;
	float		frac, midf;
	int		side;
	msurface_t	*surf;
	Vector		mid;

	// didn't hit anything
	if( !node || node->contents < 0 )
		return false;

	// calculate mid point
	front = PlaneDiff( start, node->plane );
	back = PlaneDiff( end, node->plane );

	side = front < 0.0f;
	if(( back < 0.0f ) == side )
		return R_RecursiveLightPoint( model, node->children[side], p1f, p2f, start, end, info );

	frac = front / ( front - back );

	midf = p1f + ( p2f - p1f ) * frac;
	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( R_RecursiveLightPoint( model, node->children[side], p1f, midf, start, mid, info ))
		return true; // hit something

	if(( back < 0.0f ) == side )
		return false;// didn't hit anything

	// check for impact on this node
	surf = model->surfaces + node->firstsurface;

	for( int i = 0; i < node->numsurfaces; i++, surf++ )
	{
		if( R_GetDirectLightFromSurface( surf, mid, info ))
		{
			info->fraction = midf;
			return true;
		}
	}

	// go down back side
	return R_RecursiveLightPoint( model, node->children[!side], midf, p2f, mid, end, info );
}

/*
=================
R_LightPoint
=================
*/
static bool R_LightPoint( model_t *model, mnode_t *node, const Vector &start, const Vector &end, mstudiolight_t *light, bool ambient, float *fraction )
{
	float factor = (ambient) ? 0.6f : 0.9f;
	lightpoint_t info;

	memset( &info, 0, sizeof( lightpoint_t ));
	info.fraction = *fraction = 1.0f;
	info.origin = start;

	if( R_RecursiveLightPoint( model, node, 0.0f, 1.0f, start, end, &info ))
	{
		float total = Q_max( Q_max( info.lightmap.r, info.lightmap.g ), info.lightmap.b );
		if( total == 0.0f ) total = 1.0f;

		info.normal *= (total * factor);
		light->shadelight = info.normal.Length();
		light->ambientlight = total - light->shadelight;

		if( total > 0.0f )
		{
			light->diffuse.x = (float)info.lightmap.r * ( 1.0f / total );
			light->diffuse.y = (float)info.lightmap.g * ( 1.0f / total );
			light->diffuse.z = (float)info.lightmap.b * ( 1.0f / total );
		}
		else light->diffuse = Vector( 1.0f, 1.0f, 1.0f );

		if( light->ambientlight > 128 )
			light->ambientlight = 128;

		if( light->ambientlight + light->shadelight > 255 )
			light->shadelight = 255 - light->ambientlight;
		light->normal = info.normal.Normalize();
		*fraction = info.fraction;
		light->nointerp = FBitSet( info.status, LIGHTINFO_STYLES ) ? true : false;

		return true;
	}

	return false;
}

/*
=================
ComputeLightmapColorFromPoint
=================
*/
static void ComputeLightmapColorFromPoint( lightpoint_t *info, mworldlight_t* pSkylight, float scale, Vector &radcolor, bool average )
{
	if( !info->surf && FBitSet( info->status, LIGHTINFO_HITSKY ))
	{
		if( pSkylight ) 
		{
			radcolor += pSkylight->intensity * scale * 0.5f * (1.0f / 255.0f);
		}
		else if( world->numworldlights <= 0 ) // old bsp format?
		{
			Vector skyAmbient = tr.sky_ambient * (1.0f / 128.0f) * tr.diffuseFactor;
			radcolor += skyAmbient * scale * 0.5f * (1.0f / 255.0f);
		}
		return;
	}

	if( info->surf != NULL )
	{
		Vector	reflectivity;
		byte	rgba[4];
		Vector	color;

		GET_EXTRA_PARAMS( info->surf->texinfo->texture->gl_texturenum, &rgba[0], &rgba[1], &rgba[2], &rgba[3] );
		reflectivity.x = TextureToLinear( rgba[0] );
		reflectivity.y = TextureToLinear( rgba[1] );
		reflectivity.z = TextureToLinear( rgba[2] );

		if( average ) color = info->average * scale;
		else color = info->diffuse * scale;
#if 0
		if( reflectivity != g_vecZero )
			color *= reflectivity;
#endif
		radcolor += color;
	}
}

//-----------------------------------------------------------------------------
// Computes ambient lighting along a specified ray.  
// Ray represents a cone, tanTheta is the tan of the inner cone angle
//-----------------------------------------------------------------------------
static void R_CalcRayAmbientLighting( const Vector &vStart, const Vector &vEnd, mworldlight_t *pSkyLight, float tanTheta, Vector &color )
{
	cl_entity_t *ent = NULL;
	model_t *model = worldmodel;
	lightpoint_t info;
	pmtrace_t	tr0;

	memset( &info, 0, sizeof( lightpoint_t ));
	SetBits( info.status, LIGHTINFO_AVERAGE );
	info.fraction = 1.0f;
	info.origin = vStart;
	color = g_vecZero;

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTraceExt( (float *)&vStart, (float *)&vEnd, PM_WORLD_ONLY, R_LightTraceFilter, &tr0 );

	if( tr0.allsolid || tr0.startsolid || tr0.fraction == 1.0f )
		return; // doesn't hit anything

	if( gEngfuncs.pEventAPI->EV_IndexFromTrace( &tr0 ) != -1 )
		ent = GET_ENTITY( gEngfuncs.pEventAPI->EV_IndexFromTrace( &tr0 ));

	if( ent ) model = ent->model;

	// Now that we've got a ray, see what surface we've hit
	R_RecursiveLightPoint( model, &model->nodes[model->hulls[0].firstclipnode], 0.0f, 1.0f, vStart, vEnd, &info );

	Vector delta = vEnd - vStart;

	// compute the approximate radius of a circle centered around the intersection point
	float dist = delta.Length() * tanTheta * info.fraction;

	// until 20" we use the point sample, then blend in the average until we're covering 40"
	// This is attempting to model the ray as a cone - in the ideal case we'd simply sample all
	// luxels in the intersection of the cone with the surface.  Since we don't have surface 
	// neighbor information computed we'll just approximate that sampling with a blend between
	// a point sample and the face average.
	// This yields results that are similar in that aliasing is reduced at distance while 
	// point samples provide accuracy for intersections with near geometry
	float scaleAvg = RemapValClamped( dist, 20, 40, 0.0f, 1.0f );

	// don't have luxel UV, so just use average sample
	if( !info.surf ) scaleAvg = 1.0f;

	float scaleSample = 1.0f - scaleAvg;

	if( scaleAvg != 0.0f )
	{
		ComputeLightmapColorFromPoint( &info, pSkyLight, scaleAvg, color, true );
	}

	if( scaleSample != 0.0f )
	{
		ComputeLightmapColorFromPoint( &info, pSkyLight, scaleSample, color, false );
	}
}

/*
=================
R_PointAmbientFromAxisAlignedSamples

fast ambient comptation
=================
*/
static bool R_PointAmbientFromAxisAlignedSamples( const Vector &p1, mstudiolight_t *light )
{
	// use lightprobes instead
	if( world->numleaflights && world->leafs )
		return false;

	mworldlight_t *pSkyLight = R_FindAmbientSkyLight();
	Vector radcolor[NUMVERTEXNORMALS];
	Vector lightBoxColor[6], p2;
	lightpoint_t info;

	// sample world only along cardinal axes
	for( int i = 0; i < 6; i++ )
	{
		memset( &info, 0, sizeof( lightpoint_t ));
		SetBits( info.status, LIGHTINFO_AVERAGE );
		info.fraction = 1.0f;
		info.origin = p1;

		VectorMA( p1, 65536.0f * 1.74f, g_BoxDirections[i], p2 );

		// now that we've got a ray, see what surface we've hit
		if( !R_RecursiveLightPoint( worldmodel, worldmodel->nodes, 0.0f, 1.0f, p1, p2, &info ))
			continue;

		ComputeLightmapColorFromPoint( &info, pSkyLight, 1.0f, light->ambient[i], true );
		light->ambient[i] *= AMBIENT_CUBE_SCALE;
	}

	return true;
}

/*
=================
R_PointAmbientFromSphericalSamples

reconstruct the ambient lighting for a leaf
at the given position in worldspace
=================
*/
static bool R_PointAmbientFromSphericalSamples( const Vector &p1, mstudiolight_t *light )
{
	// use lightprobes instead
	if( world->numleaflights && world->leafs )
		return false;

	// Figure out the color that rays hit when shot out from this position.
	float tanTheta = tan( VERTEXNORMAL_CONE_INNER_ANGLE );
	mworldlight_t *pSkylight = R_FindAmbientSkyLight();
	Vector radcolor[NUMVERTEXNORMALS];
	Vector lightBoxColor[6], p2;

	// sample world by casting N rays distributed across a sphere
	for( int i = 0; i < NUMVERTEXNORMALS; i++ )
	{
		// FIXME: a good optimization would be to scale this per leaf
		VectorMA( p1, 65536.0f * 1.74f, g_anorms[i], p2 );

		R_CalcRayAmbientLighting( p1, p2, pSkylight, tanTheta, radcolor[i] );
	}

	// accumulate samples into radiant box
	for( int j = 0; j < 6; j++ )
	{
		float t = 0.0f;

		//VectorClear( light->ambient[j] );
		light->ambient[j].Assign(0.f);

		for( int i = 0; i < NUMVERTEXNORMALS; i++ )
		{
			float c = DotProduct( g_anorms[i], g_BoxDirections[j] );

			if( c > 0.0f )
			{
				VectorMA( light->ambient[j], c, radcolor[i], light->ambient[j] );
				t += c;
			}
		}
		
		//VectorScale( light->ambient[j], ( 1.0 / t ) * AMBIENT_CUBE_SCALE, light->ambient[j] );
		light->ambient[j] *= (1.0 / t) * AMBIENT_CUBE_SCALE;
	}

	return true;
}

/*
=================
R_PointAmbientFromLeaf

reconstruct the ambient lighting for a leaf
at the given position in worldspace
=================
*/
void R_PointAmbientFromLeaf( const Vector &point, mstudiolight_t *light )
{
	memset( light->ambient, 0, sizeof( light->ambient ));

	if( r_lighting_extended->value > 1.0f )
	{
		if( R_PointAmbientFromSphericalSamples( point, light ))
			return;
	}
	else
	{
		if( R_PointAmbientFromAxisAlignedSamples( point, light ))
			return;
	}

	mleaf_t *leaf = Mod_PointInLeaf( point, worldmodel->nodes );
	mextraleaf_t *info = LEAF_INFO( leaf, worldmodel );
	mlightprobe_t *pAmbientProbe = info->ambient_light;
	float totalFactor = 0.0f;
	int i;

	if( info->num_lightprobes > 0 )
	{
		for( i = 0; i < info->num_lightprobes; i++, pAmbientProbe++ )
		{
			// do an inverse squared distance weighted average of the samples
			// to reconstruct  the original function
			float dist = (pAmbientProbe->origin - point).LengthSqr();
			float factor = 1.0f / (dist + 1.0f);
			totalFactor += factor;

			for( int j = 0; j < 6; j++ )
			{
				Vector v;

				v.x = (float)pAmbientProbe->cube.color[j][0] * (1.0f / 255.0f);
				v.y = (float)pAmbientProbe->cube.color[j][1] * (1.0f / 255.0f);
				v.z = (float)pAmbientProbe->cube.color[j][2] * (1.0f / 255.0f);

				light->ambient[j] += v * factor;
			}
		}

		for( i = 0; i < 6; i++ )
		{
			light->ambient[i] *= (1.0f / totalFactor) * AMBIENT_CUBE_SCALE;
		}
	}
}

/*
=================
R_LightIdentity
=================
*/
static void R_LightIdentity( mstudiolight_t *light )
{
	// get default values
	light->diffuse.x = (float)TEXTURE_TO_TEXGAMMA( 255 ) / 255.0f;
	light->diffuse.y = (float)TEXTURE_TO_TEXGAMMA( 255 ) / 255.0f;
	light->diffuse.z = (float)TEXTURE_TO_TEXGAMMA( 255 ) / 255.0f;
	light->normal = Vector( 0.0f, 0.0f, 0.0f );
}

/*
=================
R_LightMin
=================
*/
static void R_LightMin( mstudiolight_t *light )
{
	// get minlight values
	light->diffuse.x = (float)TEXTURE_TO_TEXGAMMA( 10 ) / 255.0f;
	light->diffuse.y = (float)TEXTURE_TO_TEXGAMMA( 10 ) / 255.0f;
	light->diffuse.z = (float)TEXTURE_TO_TEXGAMMA( 10 ) / 255.0f;
	light->normal = Vector( 0.0f, 0.0f, 0.0f );
}

/*
=================
R_LightVec

check bspmodels to get light from
=================
*/
void R_LightVec( const Vector &point, mstudiolight_t *light, bool ambient )
{
	memset( light, 0 , sizeof( *light ));

	if( worldmodel && worldmodel->lightdata )
	{
		Vector p0 = point + Vector( 0.0f, 0.0f, 8.0f );
		Vector p1 = point + Vector( 0.0f, 0.0f, -512.0f );
		float last_fraction = 1.0f, fraction;
		Vector start = point;
		mstudiolight_t probe;

		for( int i = 0; i < MAX_PHYSENTS; i++ )
		{
			physent_t	*pe = gEngfuncs.pEventAPI->EV_GetPhysent( i );
			Vector offset, start_l, end_l;
			matrix3x4	matrix;
			mnode_t *pnodes;

			if( !pe || !pe->model || pe->model->type != mod_brush )
				continue; // skip non-bsp models

			pnodes = &pe->model->nodes[pe->model->hulls[0].firstclipnode];
			// rotate start and end into the models frame of reference
			if( pe->angles != g_vecZero )
			{
				matrix = matrix3x4( pe->origin, pe->angles );
				start_l = matrix.VectorITransform( p0 );
				end_l = matrix.VectorITransform( p1 );
			}
			else
			{
				start_l = p0 - pe->origin;
				end_l = p1 - pe->origin;
			}

			memset( &probe, 0 , sizeof( probe ));
			if( !R_LightPoint( pe->model, pnodes, start_l, end_l, &probe, ambient, &fraction ))
				continue;	// didn't hit anything

			if( fraction < last_fraction )
			{
				last_fraction = fraction;
				*light = probe;

				if( light->diffuse != g_vecZero )
					return; // we get light now
			}

			// get light from bmodels too
			if( !CVAR_TO_BOOL( r_lighting_extended ))
				return; // get light from world and out
		}

		R_LightMin( light );
	}
	else
	{
		R_LightIdentity( light );
	}
}

/*
=================
R_LightForSky
=================
*/
void R_LightForSky( const Vector &point, mstudiolight_t *light )
{
	if( tr.sun_light_enabled )
	{
		light->diffuse = tr.sun_diffuse;
	}
	else
	{
		light->diffuse = tr.sky_ambient * ( 1.0f / 128.0f ) * tr.diffuseFactor;
	}

	// FIXME: this is correct?
	light->normal = -tr.sky_normal;
	light->ambientlight = 128;
	light->shadelight = 192;
}

/*
=================
R_LightForStudio

given light for a studiomodel
=================
*/
void R_LightForStudio( const Vector &point, mstudiolight_t *light, bool ambient )
{
	R_LightVec( point, light, ambient );
	R_PointAmbientFromLeaf( point, light );
}