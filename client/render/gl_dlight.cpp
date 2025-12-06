/*
gl_dlight.cpp - dynamic lighting
Copyright (C) 2014 Uncle Mike

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
#include "gl_grass.h"
#include "gl_cvars.h"

/*
=============================================================================

PROJECTED LIGHTS

=============================================================================
*/
/*
===============
CL_ClearDlights
===============
*/
void CL_ClearDlights( void )
{
	memset( tr.dlights, 0, sizeof( tr.dlights ));
}

/*
===============
CL_AllocDlight
===============
*/
CDynLight *CL_AllocDlight( int key )
{
	CDynLight *dl;

	// first look for an exact key match
	if( key )
	{
		dl = tr.dlights;
		for( int i = 0; i < MAX_DLIGHTS; i++, dl++ )
		{
			if( dl->key == key )
			{
				// reuse this light
				return dl;
			}
		}
	}

	float time = GET_CLIENT_TIME();

	// then look for anything else
	dl = tr.dlights;
	for( int i = 0; i < MAX_DLIGHTS; i++, dl++ )
	{
		if( dl->die < time && dl->key == 0 )
		{
			memset( dl, 0, sizeof( *dl ));
			dl->key = key;
			return dl;
		}
	}

	dl = &tr.dlights[0];
	memset( dl, 0, sizeof( *dl ));
	dl->key = key;

	return dl;
}

/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights( void )
{
	float time = GET_CLIENT_TIME();
	CDynLight *pl = tr.dlights;

	for (int i = 0; i < MAX_DLIGHTS; i++, pl++)
	{
		if (!pl->radius)
			continue;

		if (FBitSet(pl->flags, DLF_MOVIE))
			continue; // don't clean up dlights that used for video projecting

		pl->radius -= tr.frametime * pl->decay;
		if (pl->radius < 0.0f) pl->radius = 0.0f;

		if (pl->Expired())
			memset(pl, 0, sizeof(*pl));
	}
}

//=====================================
// HasDynamicLights
//
// Return count dynamic lights for current frame
//=====================================
int HasDynamicLights( void )
{
	int numDlights = 0;

	if( !worldmodel->lightdata || !CVAR_TO_BOOL( cv_dynamiclight ))
		return numDlights;

	for( int i = 0; i < MAX_DLIGHTS; i++ )
	{
		CDynLight *pl = &tr.dlights[i];
		if( pl->Expired( )) continue;
		numDlights++;
	}

	return numDlights;
}

//=====================================
// HasStaticLights
//
// Return count static lights for current frame
//=====================================
int HasStaticLights( void )
{
	int		i, numLights = 0;
	mworldlight_t	*wl;

	for( i = 0, wl = world->worldlights; i < world->numworldlights; i++, wl++ )
	{
		if( wl->emittype == emit_ignored )
			continue;

		if( !CHECKVISBIT( RI->view.vislight, i ))
			continue;
		numLights++;
	}

	return numLights;
}


/*
===============
R_UseSkyLightstyle

===============
*/
bool R_UseSkyLightstyle(int lightstyleIndex)
{
	if (tr.sun_light_enabled && lightstyleIndex == LS_SKY)
		return false;
	return true;
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/
/*
==================
R_AnimateLight

==================
*/
void R_AnimateLight( void )
{
	int		i, k, flight, clight;
	float		l, lerpfrac, backlerp;
	float		scale = 1.0f;
	lightstyle_t	*ls;

	scale = tr.diffuseFactor;

	if( tr.realframecount != 1 && !CVAR_TO_BOOL( r_lightstyles ))
		return;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		ls = GET_LIGHTSTYLE( i );

		if( !worldmodel->lightdata )
		{
			tr.lightstyle[i] = 256.0f * scale;
			continue;
		}

		flight = (int)Q_floor( ls->time * 10.0f );
		clight = (int)Q_ceil( ls->time * 10.0f );
		lerpfrac = ( ls->time * 10 ) - flight;
		backlerp = 1.0f - lerpfrac;

		if( !ls->length )
		{
			tr.lightstyle[i] = 256.0f * scale;
			continue;
		}
		else if( ls->length == 1 )
		{
			float	value = ls->map[0];

			// turn off the baked sunlight
			if (!R_UseSkyLightstyle(i)) {
				value = 0.0f;
			}

			// single length style so don't bother interpolating
			tr.lightstyle[i] = value * 22.0f * scale;
			continue;
		}
		else if( !ls->interp )
		{
			tr.lightstyle[i] = ls->map[flight%ls->length] * 22.0f * scale;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22.0f ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22.0f ) * lerpfrac;

		tr.lightstyle[i] = l * scale;
	}
}

/*
=======================================================================

	GATHER DYNAMIC LIGHTING

=======================================================================
*/
/*
=================
R_LightsForPoint
=================
*/
Vector R_LightsForPoint( const Vector &point, float radius )
{
	Vector	lightColor;
	const bool skipCulling = CVAR_TO_BOOL(r_nocull);

	if( radius <= 0.0f )
		radius = 1.0f;

	lightColor = g_vecZero;

	for( int lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		CDynLight *dl = &tr.dlights[lnum];
		float atten = 1.0f;

		if( dl->type == LIGHT_DIRECTIONAL )
			continue;

		if( dl->die < GET_CLIENT_TIME() || !dl->radius )
			continue;

		Vector dir = (dl->origin - point);
		float dist = dir.Length();

		if( !dist || dist > ( dl->radius + radius ))
			continue;

		if( !skipCulling && dl->frustum.CullSphere( point, radius ))
			continue;

		atten = 1.0 - saturate( pow( dist * ( 1.0f / dl->radius ), 2.2f ));
		if( atten <= 0.0 ) continue; // fast reject

		if( dl->type == LIGHT_SPOT )
		{
			Vector lightDir = dl->frustum.GetPlane( FRUSTUM_FAR )->normal.Normalize();
			float fov_x, fov_y;

			// BUGBUG: we use 5:4 aspect not an 4:3 
			if( dl->flags & DLF_ASPECT3X4 )
				fov_y = dl->fov * (5.0f / 4.0f); 
			else if( dl->flags & DLF_ASPECT4X3 )
				fov_y = dl->fov * (4.0f / 5.0f);
			else fov_y = dl->fov;
			fov_x = dl->fov;

			// spot attenuation
			float spotDot = DotProduct( dir.Normalize(), lightDir );
			fov_x = DEG2RAD( fov_x * 0.25f );
			fov_y = DEG2RAD( fov_y * 0.25f );
			float spotCos = cos( fov_x + fov_y );
			if( spotDot < spotCos ) continue;
		}

		lightColor += (dl->color * 0.5f * atten);
	}

	return lightColor;
}

/*
================
R_SetupLightTexture

Must be called after R_SetupLightParams
================
*/
void R_SetupLightTexture( CDynLight *pl, TextureHandle texture )
{
	ASSERT( pl != NULL );

	pl->spotlightTexture = texture;	
}

/*
================
R_SetupLightParams
================
*/
void R_SetupLightParams( CDynLight *pl, const Vector &origin, const Vector &angles, float radius, float fov, int type, int flags )
{
	pl->type = bound( LIGHT_SPOT, type, LIGHT_DIRECTIONAL );

	if( pl->type == LIGHT_OMNI )
	{
		for( int i = 0; i < MAX_SHADOWMAPS; i++ )
			pl->shadowTexture[i] = tr.depthCubemap;	// stub

		if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
			flags |= DLF_NOSHADOWS;
	}
	else
	{
		for( int i = 0; i < MAX_SHADOWMAPS; i++ )
			pl->shadowTexture[i] = tr.depthTexture;	// stub
	}

	if( pl->origin != origin || pl->angles != angles || pl->fov != fov || pl->radius != radius || pl->flags != flags )
	{
		pl->origin = origin;
		pl->angles = angles;
		pl->radius = radius;
		pl->flags = flags;
		pl->update = true;
		pl->fov = fov;
	}
}

/*
================
R_SetupLightProjection

General setup light projections.
Calling only once per frame
================
*/
void R_SetupLightProjection( CDynLight *pl )
{
	const bool frustumLocked = CVAR_TO_BOOL(r_lockfrustum);
	if (!pl->update || frustumLocked)
	{
		if( pl->type != LIGHT_DIRECTIONAL )
		{
			if( !R_ScissorForFrustum( &pl->frustum, &pl->x, &pl->y, &pl->w, &pl->h ))
				SetBits( pl->flags, DLF_CULLED );
			else ClearBits( pl->flags, DLF_CULLED );
		}
		return;
	}

	if( pl->type == LIGHT_SPOT )
	{
		float	fov_x, fov_y;

		// BUGBUG: we use 5:4 aspect not an 4:3 
		if( pl->flags & DLF_ASPECT3X4 )
			fov_y = pl->fov * (5.0f / 4.0f); 
		else if( pl->flags & DLF_ASPECT4X3 )
			fov_y = pl->fov * (4.0f / 5.0f);
		else fov_y = pl->fov;

		// e.g. for fake cinema projectors
		if( FBitSet( pl->flags, DLF_FLIPTEXTURE ))
			fov_x = -pl->fov;
		else fov_x = pl->fov;

		pl->projectionMatrix.CreateProjection( fov_x, fov_y, Z_NEAR_LIGHT, pl->radius );
		pl->modelviewMatrix.CreateModelview(); // init quake world orientation
		pl->viewMatrix = matrix4x4( pl->origin, pl->angles );

		// transform projector by position and angles
		pl->modelviewMatrix.ConcatRotate( -pl->angles.z, 1, 0, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.x, 0, 1, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.y, 0, 0, 1 );
		pl->modelviewMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );

		pl->frustum.InitProjection( pl->viewMatrix, Z_NEAR_LIGHT, pl->radius, pl->fov, fov_y );
		pl->frustum.ComputeFrustumBounds( pl->absmin, pl->absmax );
		pl->frustum.DisablePlane( FRUSTUM_FAR ); // only use plane.normal
	}
	else if( pl->type == LIGHT_OMNI )
	{
		pl->modelviewMatrix.CreateModelview();
		pl->viewMatrix = matrix4x4( pl->origin, g_vecZero );
		pl->projectionMatrix.CreateProjection( 90.0f, 90.0f, Z_NEAR_LIGHT, pl->radius );

		// transform omnilight by position
		pl->modelviewMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );

		pl->frustum.InitBoxFrustum( pl->origin, pl->radius );
		pl->frustum.ComputeFrustumBounds( pl->absmin, pl->absmax );
	}
	else if( pl->type == LIGHT_DIRECTIONAL )
	{
		Vector skyDir, angles, up;

		skyDir = tr.sky_normal.Normalize();
		VectorAngles( skyDir, angles );
		AngleVectors( angles, NULL, NULL, up );

		pl->viewMatrix.LookAt( tr.cached_vieworigin, skyDir, up );
		pl->modelviewMatrix = pl->viewMatrix;

		ClearBounds( pl->absmin, pl->absmax );
	}
	else
	{
		HOST_ERROR( "R_SetupLightProjection: unknown light type %i\n", pl->type );
	}

	if( pl->type != LIGHT_DIRECTIONAL )
	{
		matrix4x4 projectionView, s1;

		projectionView = pl->projectionMatrix.Concat( pl->modelviewMatrix );
		pl->lightviewProjMatrix = projectionView;

		s1.CreateTranslate( 0.5f, 0.5f, 0.5f );
		s1.ConcatScale( 0.5f, 0.5f, 0.5f );

		pl->textureMatrix[0] = projectionView;
		pl->shadowMatrix[0] = s1.Concat( projectionView );

		if( !R_ScissorForFrustum( &pl->frustum, &pl->x, &pl->y, &pl->w, &pl->h ))
			SetBits( pl->flags, DLF_CULLED );
		else ClearBits( pl->flags, DLF_CULLED );
	}

	pl->update = false;
}

/*
================
R_SetupDynamicLights
================
*/
void R_SetupDynamicLights( void )
{
	CDynLight *pl;
	dlight_t *dl;

	for( int lnum = 0; lnum < MAX_ENGINE_DLIGHTS; lnum++ )
	{
		dl = GET_DYNAMIC_LIGHT( lnum );
		pl = &tr.dlights[MAX_ENGINE_DLIGHTS+lnum];

		// NOTE: here we copies dlight settings 'as-is'
		// without reallocating by key because key may
		// be set indirectly without call CL_AllocDlight
		if( dl->die < GET_CLIENT_TIME() || !dl->radius )
		{
			// light is expired. Clear it
			memset( pl, 0, sizeof( *pl ));
			continue;
		}

		pl->key = dl->key;
		pl->die = dl->die + tr.frametime;
		pl->decay = dl->decay;
		pl->color.x = dl->color.r * (1.0 / 255.0f);
		pl->color.y = dl->color.g * (1.0 / 255.0f);
		pl->color.z = dl->color.b * (1.0 / 255.0f);
		pl->origin = dl->origin;
		pl->radius = dl->radius;
		pl->type = LIGHT_OMNI;
		pl->update = true;
			
		R_SetupLightProjection( pl );
	}

	pl = tr.dlights;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		if( pl->Expired( )) continue;
		R_SetupLightProjection( pl );
	}
}

/*
=======================================================================

	WORLDLIGHTS PROCESSING

=======================================================================
*/
static vec_t LightDistanceFalloff( const mworldlight_t *wl, const Vector &direction )
{
	Vector	delta = direction;
	float	dist = delta.NormalizeLength();
	float	dot = 1.0f; // assume dot is 1.0f

	dist = Q_max( dist, 1.0 );
	switch( wl->emittype )
	{
	case emit_surface:
		return dot / (dist * dist);
	case emit_skylight:
		return dot;
		break;
	case emit_spotlight: // directional & positional
	case emit_point:
		// cull out stuff that's too far
		if( dist > wl->radius )
			return 0.0f;
		return dot / (dist * dist);
		break;
	}

	return 1.0f;
}

//-----------------------------------------------------------------------------
// This method returns the effective intensity of a light as seen from
// a particular point. PVS is used to speed up the task.
//-----------------------------------------------------------------------------
static float LightIntensityAtPoint( mworldlight_t *wl, const Vector &mid, bool skipZ ) 
{
	lightzbuffer_t *pZBuf = world->shadowzbuffers;
	Vector direction;

	// special case lights
	if( wl->emittype == emit_skylight )
	{
		// check to see if you can hit the sky texture
		Vector end = mid + wl->normal * -BOGUS_RANGE; // max_range * sqrt(3)
		msurface_t *surf = gEngfuncs.pEventAPI->EV_TraceSurface( 0, (float *)&mid, (float *)end );

		// here, we didn't hit the sky, so we must be in shadow
		if( !surf || !FBitSet( surf->flags, SURF_DRAWSKY ))
			return 0.0f;
		return 1.0f;
	}

	// all other lights

	// check distance
	direction = wl->origin - mid;
	float ratio = LightDistanceFalloff( wl, direction );

	// Early out for really low-intensity lights
	// That way we don't need to ray-cast or normalize
	float intensity = wl->intensity.MaxCoord();

	if( intensity * ratio < 4e-3 )
		return 0.0f;

	if( skipZ ) return ratio;
	float dist = direction.NormalizeLength();

	float flTraceDistance = dist;

	// check if we are so close to the light that we shouldn't use our coarse z buf
	if( dist < 8 * SHADOW_ZBUF_RES )
		pZBuf = NULL;

	LightShadowZBufferSample_t *pSample = NULL;
	Vector epnt = mid;

	if( pZBuf )
	{
		pSample = &( pZBuf->GetSample( direction ));

		if(( pSample->m_flHitDistance < pSample->m_flTraceDistance ) || ( pSample->m_flTraceDistance >= dist ))
		{
			// hit!
			if( dist > pSample->m_flHitDistance + 8 ) // shadow hit
				return 0;
			return ratio;
		}

		// cache miss
		flTraceDistance = Q_max( 100.0f, 2.0f * dist ); // trace a little further for better caching
		epnt += direction * ( dist - flTraceDistance );
	}

	pmtrace_t pm;

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( wl->origin, epnt, PM_WORLD_ONLY, -1, &pm );
//	pm.fraction = 1.0f - pm.fraction;

	float flHitDistance = ( pm.startsolid ) ? FLT_EPSILON : ( pm.fraction ) * flTraceDistance;
	
	if( pSample )
	{
		pSample->m_flTraceDistance = flTraceDistance;
		pSample->m_flHitDistance = ( pm.fraction >= 1.0 ) ? 1.0e23 : flHitDistance;
	}

	if( dist > flHitDistance + 8 )
		return 0.0f;

	return ratio;
}

static float LightIntensityInBox( mworldlight_t *wl, const Vector &mid, const Vector &mins, const Vector &maxs, bool skipZ ) 
{
	float	sphereRadius;
	float	angle, sinAngle;
	float	dist, distSqr;

	// Choose the point closest on the box to the light to get max intensity
	// within the box....
	switch( wl->emittype )
	{
	case emit_spotlight: // directional & positional
		sphereRadius = (maxs - mid).Length();
		// first do a sphere/sphere check
		dist = (wl->origin - mid).Length();
		if( dist > (sphereRadius + wl->radius ))
			return 0;
		// PERFORMANCE: precalc this and store in the light?
		angle = acos( wl->stopdot2 );
		sinAngle = sin( angle );
		if( !IsSphereIntersectingCone( mid, sphereRadius, wl->origin, wl->normal, sinAngle, wl->stopdot2 ))
			return 0;
		// NOTE: fall through to radius check in point case
	case emit_point:
		distSqr = CalcSqrDistanceToAABB( mins, maxs, wl->origin );
		if( distSqr > wl->radius * wl->radius )
			return 0;
		break;
	case emit_surface:	// directional & positional, fixed cone size
		sphereRadius = (maxs - mid).Length();
		// first do a sphere/sphere check
		dist = (wl->origin - mid).Length();
		if( dist > ( sphereRadius + wl->radius ))
			return 0;
		// PERFORMANCE: precalc this and store in the light?
		if( !IsSphereIntersectingCone( mid, sphereRadius, wl->origin, wl->normal, 1.0f, 0.0f ))
			return 0;
		break;
	}

	return LightIntensityAtPoint( wl, mid, skipZ );
}

/*
=================
R_FindWorldLights

search for lights that potentially can lit bbox
=================
*/
void R_FindWorldLights( const Vector &origin, const Vector &mins, const Vector &maxs, byte lights[MAXDYNLIGHTS], bool skipZ )
{
	mworldlight_t *wl = world->worldlights;
	Vector absmin = origin + mins;
	Vector absmax = origin + maxs;
	vec2_t indexes[32];
	int count = 0;

	for( int i = 0; i < world->numworldlights; i++, wl++ )
	{
		if( !Mod_BoxVisible( absmin, absmax, wl->pvs ))
			continue;

		float ratio = LightIntensityInBox( wl, origin, mins, maxs, skipZ );

		// no light contribution?
		if( ratio <= 0.0f ) continue;

		Vector add = wl->intensity * ratio;
		float illum = add.MaxCoord();
//Msg( "#%i, type %d, illum %.5f, intensity %g %g %g\n", i, wl->emittype, illum, wl->intensity[0], wl->intensity[1], wl->intensity[2] );
		if( illum <= 4e-3 )
			continue;

		if( count >= ARRAYSIZE( indexes ) / 2 )
			break;

		indexes[count][0] = i;
		indexes[count][1] = illum;
		count++;
	}

	memset( lights, 255, sizeof( byte ) * MAXDYNLIGHTS );
get_next_light:
	float maxIllum = 0.0;
	int ignored = -1;
	int light = 255;

	for( int i = 0; i < count; i++ )
	{
		if( indexes[i][0] == -1.0f )
			continue;

		// skylight has a maximum priority
		if( indexes[i][1] > maxIllum )
		{
			maxIllum = indexes[i][1];
			light = indexes[i][0];
			ignored = i;
		}
	}

	if( ignored == -1 )
		return;

	int i;
	for( i = 0; i < MAXDYNLIGHTS && lights[i] != 255; i++ );

	if( i < MAXDYNLIGHTS )
		lights[i] = light;	// nearest light for surf

	indexes[ignored][0] = -1;	// this light is handled

//	if( count > (int)cv_deferred_maxlights->value && i == (int)cv_deferred_maxlights->value )
//		Msg( "skipped light %i intensity %g, type %d\n", light, maxIllum, world->worldlights[light].emittype );
	goto get_next_light;	
}
