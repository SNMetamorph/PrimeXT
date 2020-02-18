/*
r_surf.cpp - dynamic and static lights
Copyright (C) 2011 Uncle Mike

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
#include "utils.h"
#include "r_local.h"
#include "pm_movevars.h"
#include "pmtrace.h"
#include "mathlib.h"
#include "pm_defs.h"
#include "event_api.h"

/*
=============================================================================

PROJECTED LIGHTS

=============================================================================
*/
plight_t	cl_plights[MAX_PLIGHTS];

/*
================
CL_ClearPlights
================
*/
void CL_ClearPlights( void )
{
	memset( cl_plights, 0, sizeof( cl_plights ));
}

/*
===============
CL_AllocPlight

alloc first 64 slots only
===============
*/
plight_t *CL_AllocPlight( int key )
{
	plight_t *pl;
	int i;

	// first look for an exact key match
	if( key )
	{
		for( i = 0, pl = cl_plights; i < MAX_USER_PLIGHTS; i++, pl++ )
		{
			if( pl->key == key )
			{
				// reuse this light
				return pl;
			}
		}
	}

	// then look for anything else
	for( i = 0, pl = cl_plights; i < MAX_USER_PLIGHTS; i++, pl++ )
	{
		if( pl->die < tr.time && pl->key == 0 )
		{
			memset( pl, 0, sizeof( *pl ));
			pl->key = key;

			return pl;
		}
	}

	// otherwise grab first dlight
	pl = &cl_plights[0];
	memset( pl, 0, sizeof( *pl ));
	pl->key = key;

	return pl;
}

/*
================
R_SetupLightProjection

General setup light projections.
Calling only once per frame
================
*/
void R_GetLightVectors( cl_entity_t *pEnt, Vector &origin, Vector &angles )
{
	// fill default case
	origin = pEnt->origin;
	angles = pEnt->angles; 

	// try to grab position from attachment
	if( pEnt->curstate.aiment > 0 && pEnt->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t *pParent = GET_ENTITY( pEnt->curstate.aiment );
		studiohdr_t *pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pParent->model );

		if( pParent && pParent->model && pStudioHeader != NULL )
		{
			// make sure what model really has attachements
			if( pEnt->curstate.body > 0 && ( pStudioHeader && pStudioHeader->numattachments > 0 ))
			{
				int num = bound( 1, pEnt->curstate.body, MAXSTUDIOATTACHMENTS );
				R_StudioAttachmentTransform( pParent, num - 1, &origin, &angles );
//				angles[PITCH] = -angles[PITCH]; // stupid quake bug
			}
			else if( pParent->curstate.movetype == MOVETYPE_STEP )
			{
				origin = pParent->origin;
				angles = pParent->angles;

				// add the eye position for monster
				if( FBitSet( pParent->curstate.eflags, EFLAG_SLERP ))
				{
					Vector viewpos = pStudioHeader->eyeposition;
					if( viewpos == g_vecZero )
						viewpos = Vector( 0, 0, 0.1f );	// monster_cockroach
					origin += viewpos;
				}
			} 
			else
			{
				origin = pParent->origin;
				angles = pParent->angles;
			}
		}
	}
	// all other parent types will be attached on the server
}

/*
================
R_SetupLightProjection

General setup light projections.
Calling only once per frame
================
*/
void R_SetupLightProjection( plight_t *pl, const Vector &origin, const Vector &angles, float radius, float fov )
{
	if( pl->origin != origin || pl->angles != angles || pl->fov != fov || pl->radius != radius )
	{
		pl->origin = origin;
		pl->angles = angles;
		pl->radius = radius;
		pl->update = true;
		pl->fov = fov;
	}

	// update the frustum only if needs
	if( pl->update )
	{
		if( pl->pointlight )
		{
			// 'quake oriented' cubemaps probably starts from Tenebrae
			// may be it was an optimization?
			pl->modelviewMatrix.Identity();
//			pl->modelviewMatrix.CreateModelview();
			pl->projectionMatrix.CreateProjection( 90.0f, 90.0f, Z_NEAR_LIGHT, pl->radius );

			pl->frustum.InitBoxFrustum( pl->origin, pl->radius );
			pl->frustum.ComputeFrustumBounds( pl->absmin, pl->absmax );
		}
		else
		{
			float	fov_x, fov_y;

			// BUGBUG: we use 5:4 aspect not an 4:3 
			if( pl->flags & CF_ASPECT3X4 )
				fov_y = pl->fov * (5.0f / 4.0f); 
			else if( pl->flags & CF_ASPECT4X3 )
				fov_y = pl->fov * (4.0f / 5.0f);
			else fov_y = pl->fov;

			// e.g. for fake cinema projectors
			if( FBitSet( pl->flags, CF_FLIPTEXTURE ))
				fov_x = -pl->fov;
			else fov_x = pl->fov;

			pl->projectionMatrix.CreateProjection( fov_x, fov_y, Z_NEAR_LIGHT, pl->radius );
			pl->modelviewMatrix.CreateModelview(); // init quake world orientation
			pl->frustum.InitProjection( matrix4x4( pl->origin, pl->angles ), Z_NEAR_LIGHT, pl->radius, pl->fov, fov_y );
			pl->frustum.ComputeFrustumBounds( pl->absmin, pl->absmax );
			pl->frustum.DisablePlane( FRUSTUM_FAR ); // only use plane.normal
		}

		// transform projector by position and angles
		pl->modelviewMatrix.ConcatRotate( -pl->angles.z, 1, 0, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.x, 0, 1, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.y, 0, 0, 1 );
		pl->modelviewMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );

		matrix4x4 projectionView, m1, s1;

		projectionView = pl->projectionMatrix.Concat( pl->modelviewMatrix );
		pl->lightviewProjMatrix = projectionView;
		pl->update = false;
	}
}

/*
================
R_SetupLightProjectionTexture

select the custom projection texture or movie
================
*/
void R_SetupLightProjectionTexture( plight_t *pl, cl_entity_t *pEnt )
{
	// select the cinematic texture
	if( pl->flags & CF_TEXTURE )
	{
		if( !pl->projectionTexture )
		{
			const char *txname = gRenderfuncs.GetFileByIndex( pEnt->curstate.sequence );

			if( txname && *txname )
			{
				const char *ext = UTIL_FileExtension( txname );

				if( !Q_stricmp( ext, "dds" ) || !Q_stricmp( ext, "tga" ) || !Q_stricmp( ext, "mip" ))
				{
					pl->projectionTexture = LOAD_TEXTURE( txname, NULL, 0, TF_SPOTLIGHT );
				}
			}

			if( !pl->projectionTexture )
			{
				ALERT( at_error, "couldn't find texture %s\n", txname );
				pl->projectionTexture = tr.spotlightTexture;
			}
		}
	}
	else if( pl->flags & CF_SPRITE )
	{
		if( !pl->pSprite && !pl->projectionTexture )
		{
			// setup the sprite
			const char *sprname = gRenderfuncs.GetFileByIndex( pEnt->curstate.sequence );
			HSPRITE handle = SPR_LoadEx( sprname, TF_BORDER );
			pl->pSprite = (model_t *)gEngfuncs.GetSpritePointer( handle );

			if( !pl->pSprite )
			{
				ALERT( at_error, "couldn't find sprite %s\n", sprname );
				pl->projectionTexture = tr.spotlightTexture;
			}
		}

		if( pl->pSprite )
		{
			// animate frames!
			int texture = R_GetSpriteTexture( pl->pSprite, pEnt->curstate.frame );
			if( texture != 0 ) pl->projectionTexture = texture;
		}
	}
	else if( pl->flags & CF_MOVIE )
	{
		if( pl->projectionTexture == tr.spotlightTexture )
			return;	// bad video texture

		// found the corresponding cinstate
		const char *cinname = gRenderfuncs.GetFileByIndex( pEnt->curstate.sequence );
		int hCin = R_PrecacheCinematic( cinname );

		if( hCin >= 0 && !pl->cinTexturenum )
			pl->cinTexturenum = R_AllocateCinematicTexture( TF_SPOTLIGHT );

		if( hCin == -1 || pl->cinTexturenum <= 0 || !CIN_IS_ACTIVE( tr.cinematics[hCin].state ))
		{
			// cinematic textures limit exceeded or movie not found
			pl->projectionTexture = tr.spotlightTexture;
			return;
		}

		gl_movie_t *cin = &tr.cinematics[hCin];
		float cin_time;

		// advances cinematic time
		cin_time = fmod( pEnt->curstate.fuser2, cin->length );

		// read the next frame
		int cin_frame = CIN_GET_FRAME_NUMBER( cin->state, cin_time );

		if( cin_frame != pl->lastframe )
		{
			// upload the new frame
			byte *raw = CIN_GET_FRAMEDATA( cin->state, cin_frame );
			CIN_UPLOAD_FRAME( tr.cinTextures[pl->cinTexturenum-1],
				cin->xres, cin->yres, cin->xres, cin->yres, raw );
			pl->lastframe = cin_frame;
		}

		// have valid cinematic texture
		pl->projectionTexture = tr.cinTextures[pl->cinTexturenum-1];
	}
	else
	{
		// just use default texture
		pl->projectionTexture = tr.spotlightTexture;
	}

	// check for valid
	if( pl->projectionTexture <= 0 )
		pl->projectionTexture = tr.spotlightTexture;

	// set cubemap flag for easy check
	if( RENDER_GET_PARM( PARM_TEX_TARGET, pl->projectionTexture ) == GL_TEXTURE_CUBE_MAP_ARB )
	{
		pl->flags |= CF_CUBEMAP;
		pl->pointlight = true; // in case of
	}
}

/*
================
R_SetupLightAttenuationTexture

select the properly attenuation
================
*/
void R_SetupLightAttenuationTexture( plight_t *pl, int falloff )
{
	if( pl->flags & CF_NOATTEN )
	{
		// NOTE: we can't use shadows for projectors without stub texture:
		// because PCF shader expected it from us
		pl->lightFalloff = -1.0f; // we save branches in shader...
		return;
	}

	if( pl->pointlight )
	{
		pl->lightFalloff = 2.2f;
		return;
	}

	if( falloff <= 0 )
	{
		if( pl->radius <= 300 )
		{
			pl->lightFalloff = 3.5f;
		}
		else if( pl->radius > 300 && pl->radius <= 800 )
		{
			pl->lightFalloff = 1.5f;
		}
		else if( pl->radius > 800 )
		{
			pl->lightFalloff = 0.5f;
		}
	}
	else
	{
		switch( bound( 1, falloff, 3 ))
		{
		case 1:
			pl->lightFalloff = 0.5f;
			break;
		case 2:
			pl->lightFalloff = 1.5f;
			break;
		case 3:
			pl->lightFalloff = 3.5f;
			break;
		}
	}
}

/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights( void )
{
	plight_t	*pl;
	int	i;

	for( i = 0, pl = cl_plights; i < MAX_USER_PLIGHTS; i++, pl++ )
	{
		if( !pl->radius ) continue;

		pl->radius -= (tr.time - tr.oldtime) * pl->decay;
		if( pl->radius < 0 ) pl->radius = 0;

		if( pl->die < tr.time || !pl->radius ) 
			memset( pl, 0, sizeof( *pl ));
	}
}

/*
=============
R_CountPlights
=============
*/
int R_CountPlights( bool countShadowLights )
{
	int numPlights = 0;

	if( !g_fRenderInitialized )
		return 0;

	if( !worldmodel->lightdata || r_fullbright->value )
		return numPlights;

	for( int i = 0; i < MAX_PLIGHTS; i++ )
	{
		plight_t *pl = &cl_plights[i];

		if( pl->die < tr.time || !pl->radius || pl->culled )
			continue;

		// TODO: allow shadows for pointlights
		if(( pl->pointlight || FBitSet( pl->flags, CF_NOSHADOWS )) && countShadowLights )
			continue;

		numPlights++;
	}

	return numPlights;
}

/*
=============
R_PushDlights

copy dlights into projected lights
=============
*/
void R_PushDlights( void )
{
	Vector	bbox[8];
	plight_t	*pl;
	dlight_t	*dl;
	int lnum;

	for( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		dl = GET_DYNAMIC_LIGHT( lnum );
		pl = &cl_plights[MAX_USER_PLIGHTS+lnum];

		// NOTE: here we copies dlight settings 'as-is'
		// without reallocating by key because key may
		// be set indirectly without call CL_AllocDlight
		if( dl->die < tr.time || !dl->radius )
		{
			// light is expired. Clear it
			memset( pl, 0, sizeof( *pl ));
			continue;
		}

		pl->key = dl->key;
		pl->die = dl->die;
		pl->decay = dl->decay;
		pl->pointlight = true;
		pl->color.r = dl->color.r;
		pl->color.g = dl->color.g;
		pl->color.b = dl->color.b;	
		pl->projectionTexture = tr.dlightCubeTexture;
			
		R_SetupLightProjection( pl, dl->origin, g_vecZero, dl->radius, 90.0f );
		R_SetupLightAttenuationTexture( pl );
	}

	// setup light scissors for each viewpass
	for( lnum = 0; lnum < MAX_PLIGHTS; lnum++ )
	{
		pl = &cl_plights[lnum];

		if( pl->die < tr.time || !pl->radius )
			continue;

		pl->culled = false;
		pl->frustum.ComputeFrustumCorners( bbox );

		// compute scissor by real frustum corners to get more precision
		if( !R_ScissorForCorners( bbox, &pl->x, &pl->y, &pl->w, &pl->h ))
			pl->culled = true;	// light was culled by scissor
		else r_stats.c_plights++;
	}
}

/*
=============================================================================

LIGHTSTYLES

=============================================================================
*/
/*
==================
R_AnimateLight

==================
*/
void R_AnimateLight( void )
{
	int		k, flight, clight;
	float		l, lerpfrac, backlerp;
	float		scale;
	const lightstyle_t	*ls;

	if( !worldmodel )
		return;

	scale = r_lighting_modulate->value;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	for( int i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		ls = GET_LIGHTSTYLE( i );

		if( r_fullbright->value || !worldmodel->lightdata )
		{
			tr.lightstylevalue[i] = 256 * 256;
			tr.lightstyles[i] = 256.0f * 256.0f;
			continue;
		}

		flight = (int)Q_floor( ls->time * 10 );
		clight = (int)Q_ceil( ls->time * 10 );
		lerpfrac = ( ls->time * 10 ) - flight;
		backlerp = 1.0f - lerpfrac;

		if( !ls->length )
		{
			tr.lightstylevalue[i] = 256 * scale;
			tr.lightstyles[i] = 256.0f * scale;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			tr.lightstylevalue[i] = ls->map[0] * 22 * scale;
			tr.lightstyles[i] = ls->map[0] * 22.0f * scale;
			continue;
		}
		else if( !ls->interp || !r_lightstyle_lerping->value )
		{
			tr.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * scale;
			tr.lightstyles[i] = ls->map[flight%ls->length] * 22.0f * scale;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22.0f ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22.0f ) * lerpfrac;

		tr.lightstylevalue[i] = (int)l * scale;
		tr.lightstyles[i] = l * scale;
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

	if( radius <= 0.0f )
		radius = 1.0f;

	lightColor = g_vecZero;

	for( int lnum = 0; lnum < MAX_PLIGHTS; lnum++ )
	{
		plight_t *pl = &cl_plights[lnum];
		float atten = 1.0f;

		if( pl->die < tr.time || !pl->radius )
			continue;

		Vector dir = (pl->origin - point);
		float dist = dir.Length();

		if( !dist || dist > ( pl->radius + radius ))
			continue;

		if( pl->frustum.CullSphere( point, radius ))
			continue;

		if( pl->lightFalloff > 0.0f )
		{
			atten = 1.0 - saturate( pow( dist * ( 1.0f / pl->radius ), pl->lightFalloff ));
			if( atten <= 0.0 ) continue; // fast reject
		}

		if( !pl->pointlight )
		{
			Vector lightDir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal.Normalize();
			float fov_x, fov_y;

			// BUGBUG: we use 5:4 aspect not an 4:3 
			if( pl->flags & CF_ASPECT3X4 )
				fov_y = pl->fov * (5.0f / 4.0f); 
			else if( pl->flags & CF_ASPECT4X3 )
				fov_y = pl->fov * (4.0f / 5.0f);
			else fov_y = pl->fov;
			fov_x = pl->fov;

			// spot attenuation
			float spotDot = DotProduct( dir.Normalize(), lightDir );
			fov_x = DEG2RAD( fov_x * 0.25f );
			fov_y = DEG2RAD( fov_y * 0.25f );
			float spotCos = cos( fov_x + fov_y );
			if( spotDot < spotCos ) continue;
		}

		Vector light = Vector( pl->color.r / 255.0f, pl->color.g / 255.0f, pl->color.b / 255.0f );
		lightColor += (light * NORMAL_FLATSHADE * atten);
	}

	float f = Q_max( Q_max( lightColor.x, lightColor.y ), lightColor.z );
	if( f > 1.0f ) lightColor *= ( 1.0f / f );

	return lightColor;
}
