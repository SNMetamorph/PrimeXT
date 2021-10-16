/*
gl_scene.cpp - scene management
Copyright (C) 2016 Uncle Mike

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
#include "entity_types.h"
#include "gl_local.h"
#include <mathlib.h>
#include "gl_aurora.h"
#include "gl_rpart.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "event_api.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"
#include "screenfade.h"
#include "shake.h"

/*
===============
R_CheckChanges
===============
*/
void R_CheckChanges(void)
{
	static bool	fog_enabled_old;
	bool		settings_changed = false;

	if (FBitSet(r_showlightmaps->flags, FCVAR_CHANGED))
	{
		float lightmap = bound(0.0f, r_showlightmaps->value, MAX_LIGHTMAPS);
		CVAR_SET_FLOAT("r_showlightmaps", lightmap);
		ClearBits(r_showlightmaps->flags, FCVAR_CHANGED);
	}

	if (FBitSet(r_studio_decals->flags, FCVAR_CHANGED))
	{
		float maxStudioDecals = bound(10.0f, r_studio_decals->value, 256.0f);
		CVAR_SET_FLOAT("r_studio_decals", maxStudioDecals);
		ClearBits(r_studio_decals->flags, FCVAR_CHANGED);
	}

	if (FBitSet(cv_deferred_maxlights->flags, FCVAR_CHANGED))
	{
		float maxDeferredLights = bound(1, cv_deferred_maxlights->value, MAXDYNLIGHTS);
		if (maxDeferredLights != cv_deferred_maxlights->value)
			CVAR_SET_FLOAT("gl_deferred_maxlights", maxDeferredLights);
		ClearBits(cv_deferred_maxlights->flags, FCVAR_CHANGED);
	}

	if (FBitSet(cv_deferred_tracebmodels->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_deferred_tracebmodels->flags, FCVAR_CHANGED);
		tr.params_changed = true;
	}

	if (FBitSet(r_recursion_depth->flags, FCVAR_CHANGED))
	{
		float depth = bound(0.0f, r_recursion_depth->value, MAX_REF_STACK - 2);
		CVAR_SET_FLOAT("gl_recursion_depth", depth);
		ClearBits(r_recursion_depth->flags, FCVAR_CHANGED);
	}

	if (FBitSet(r_drawentities->flags, FCVAR_CHANGED))
	{
		ClearBits(r_drawentities->flags, FCVAR_CHANGED);
		tr.params_changed = true;
	}

	if (FBitSet(r_lightstyles->flags, FCVAR_CHANGED))
	{
		ClearBits(r_lightstyles->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_cubemaps->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_cubemaps->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_deferred->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_deferred->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_lightmap->flags, FCVAR_CHANGED))
	{
		ClearBits(r_lightmap->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_allow_mirrors->flags, FCVAR_CHANGED))
	{
		ClearBits(r_allow_mirrors->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_realtime_puddles->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_realtime_puddles->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_detailtextures->flags, FCVAR_CHANGED))
	{
		ClearBits(r_detailtextures->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_fullbright->flags, FCVAR_CHANGED))
	{
		ClearBits(r_fullbright->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_sunshadows->flags, FCVAR_CHANGED))
	{
		ClearBits(r_sunshadows->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_sun_allowed->flags, FCVAR_CHANGED))
	{
		ClearBits(r_sun_allowed->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_parallax->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_parallax->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_specular->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_specular->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_grass_shadows->flags, FCVAR_CHANGED))
	{
		ClearBits(r_grass_shadows->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_shadows->flags, FCVAR_CHANGED))
	{
		ClearBits(r_shadows->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_bump->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_bump->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_brdf->flags, FCVAR_CHANGED))
	{
		ClearBits(cv_brdf->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(r_test->flags, FCVAR_CHANGED))
	{
		ClearBits(r_test->flags, FCVAR_CHANGED);
		R_StudioClearLightCache();
		settings_changed = true;
	}

	if (FBitSet(r_grass->flags, FCVAR_CHANGED))
	{
		if (worldmodel != NULL)
		{
			for (int i = 0; i < worldmodel->numsurfaces; i++)
				SetBits(worldmodel->surfaces[i].flags, SURF_GRASS_UPDATE);
		}
		ClearBits(r_grass->flags, FCVAR_CHANGED);
		settings_changed = true;
	}

	if (FBitSet(cv_gamma->flags, FCVAR_CHANGED) || FBitSet(cv_brightness->flags, FCVAR_CHANGED))
	{
		if (worldmodel != NULL)
		{
			for (int i = 0; i < worldmodel->numsurfaces; i++)
				SetBits(worldmodel->surfaces[i].flags, SURF_LM_UPDATE | SURF_GRASS_UPDATE);
		}
		R_StudioClearLightCache();
	}

	if (tr.fogEnabled != fog_enabled_old)
	{
		fog_enabled_old = tr.fogEnabled;
		settings_changed = true;
	}

	if (settings_changed || tr.params_changed)
	{
		tr.glsl_valid_sequence++; // now all uber-shaders are invalidate and possible need for recompile
		tr.params_changed = true;
	}
}

/*
===============
R_ClearScene
===============
*/
void R_ClearScene( void )
{
	if( !g_fRenderInitialized ) return;

	CL_DecayLights();

	tr.time = GET_CLIENT_TIME();
	tr.oldtime = GET_CLIENT_OLDTIME();
	tr.frametime = tr.time - tr.oldtime;
	tr.saved_frametime = tr.frametime; // backup

	memset( &r_stats, 0, sizeof( r_stats ));

	tr.local_client_added = false;
	tr.num_draw_entities = 0;
	tr.cached_state.Purge(); // invalidate cache
	GET_ENTITY( 0 )->hCachedMatrix = GL_CacheState( g_vecZero, g_vecZero );

	tr.num_2D_shadows_used = tr.num_CM_shadows_used = 0;
	tr.sun_light_enabled = false;

	if( r_sunshadows->value > 2.0f )
		CVAR_SET_FLOAT( "gl_sun_shadows", 2.0f );
	else if( r_sunshadows->value < 0.0f )
		CVAR_SET_FLOAT( "gl_sun_shadows", 0.0f );		

	if( tr.shadows_notsupport )
		CVAR_SET_FLOAT( "r_shadows", 0.0f );

	R_CheckChanges();

	if( tr.params_changed )
		R_InitDynLightShaders();
}

/*
===============
R_ComputeFxBlend
===============
*/
int R_ComputeFxBlend( cl_entity_t *e )
{
	int	blend = 0, renderAmt;
	float	offset, dist;
	Vector	tmp;

	offset = ((int)e->index ) * 363.0f; // Use ent index to de-sync these fx
	renderAmt = e->curstate.renderamt;

	switch( e->curstate.renderfx ) 
	{
	case kRenderFxPulseSlowWide:
		blend = renderAmt + 0x40 * sin( tr.time * 2 + offset );	
		break;
	case kRenderFxPulseFastWide:
		blend = renderAmt + 0x40 * sin( tr.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = renderAmt + 0x10 * sin( tr.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = renderAmt + 0x10 * sin( tr.time * 8 + offset );
		break;
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:			
		if( renderAmt > 0 ) 
			renderAmt -= 1;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxFadeFast:
		if( renderAmt > 3 ) 
			renderAmt -= 4;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxSolidSlow:
		if( renderAmt < 255 ) 
			renderAmt += 1;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxSolidFast:
		if( renderAmt < 252 ) 
			renderAmt += 4;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( tr.time * 4 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( tr.time * 16 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( tr.time * 36 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( tr.time * 2 ) + sin( tr.time * 17 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin( tr.time * 16 ) + sin( tr.time * 23 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		tmp = e->origin - GetVieworg();
		dist = DotProduct( tmp, GetVForward( ));
			
		// Turn off distance fade
		if( e->curstate.renderfx == kRenderFxDistort )
			dist = 1;

		if( dist <= 0 )
		{
			blend = 0;
		}
		else 
		{
			renderAmt = 180;
			if( dist <= 100 ) blend = renderAmt;
			else blend = (int) ((1.0f - ( dist - 100 ) * ( 1.0f / 400.0f )) * renderAmt );
			blend += RANDOM_LONG( -32, 31 );
		}
		break;
	case kRenderFxGlowShell:	// safe current renderamt because it's shell scale!
	case kRenderFxDeadPlayer:	// safe current renderamt because it's player index!
		blend = renderAmt;
		break;
	case kRenderFxNone:
	case kRenderFxClampMinScale:
	default:
		if( e->curstate.rendermode == kRenderNormal )
			blend = 255;
		else blend = renderAmt;
		break;	
	}

	if( e->model->type != mod_brush || R_WaterEntity( e->model ))
	{
		// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
		if( !e->curstate.rendercolor.r && !e->curstate.rendercolor.g && !e->curstate.rendercolor.b )
			e->curstate.rendercolor.r = e->curstate.rendercolor.g = e->curstate.rendercolor.b = 255;
	}

	// apply scale to studiomodels and sprites only
	if( e->model && e->model->type != mod_brush && !e->curstate.scale )
		e->curstate.scale = 1.0f;

	blend = bound( 0, blend, 255 );

	return blend;
}

/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType )
{
	if( !CVAR_TO_BOOL( r_drawentities ))
		return false; // not allow to drawing

	if( !clent || !clent->model )
		return false; // if set to invisible, skip

	if( clent->curstate.effects & EF_NODRAW )
		return false; // done

	if( entityType == ET_PLAYER && RP_LOCALCLIENT( clent ))
	{
		if( tr.local_client_added )
			return false; // already present in list
		tr.local_client_added = true;
	}

	if (clent->curstate.renderfx == kRenderFxDynamicLight) // dynamic light
	{
		CDynLight *dl = CL_AllocDlight( clent->curstate.number );

		float radius = clent->curstate.renderamt * 8.0f;
		float fov = clent->curstate.scale;
		int tex = 0, flags = 0, type;
		Vector origin, angles;

		if( clent->curstate.scale ) // spotlight
		{
			int i = bound( 0, clent->curstate.rendermode, 7 );
			tex = tr.spotlightTexture[i];
			type = LIGHT_SPOT;
		}		
		else type = LIGHT_OMNI;

		if( clent->curstate.effects & EF_NOSHADOW )
			flags |= DLF_NOSHADOWS;

		if( clent->curstate.effects & EF_NOBUMP )
			flags |= DLF_NOBUMP;

		if( clent->curstate.effects & EF_LENSFLARE )
			flags |= DLF_LENSFLARE;

		R_GetLightVectors( clent, origin, angles );
		R_SetupLightParams( dl, origin, angles, radius, clent->curstate.scale, type, flags );
		R_SetupLightTexture( dl, tex );

		dl->color[0] = (float)clent->curstate.rendercolor.r / 128;
		dl->color[1] = (float)clent->curstate.rendercolor.g / 128;
		dl->color[2] = (float)clent->curstate.rendercolor.b / 128;
		dl->die = tr.time + 0.05f;

		return true; // no reason to drawing this entity
          }
	else if (clent->curstate.renderfx == kRenderFxCinemaLight) // dynamic light with avi file
	{
		if( !clent->curstate.sequence )
			return true; // bad avi file

		CDynLight *dl = CL_AllocDlight( clent->curstate.number );

		if( dl->spotlightTexture == tr.spotlightTexture[1] )
			return true; // bad avi file

		float radius = clent->curstate.renderamt * 8.0f;
		float fov = clent->curstate.scale;
		Vector origin, angles;
		int flags = DLF_ASPECT3X4;	// fit to film01.avi aspect

		// found the corresponding cinstate
		const char *cinname = gRenderfuncs.GetFileByIndex( clent->curstate.sequence );
		int hCin = R_PrecacheCinematic( cinname );

		if( hCin >= 0 && !dl->cinTexturenum )
			dl->cinTexturenum = R_AllocateCinematicTexture( TF_SPOTLIGHT );

		if( hCin == -1 || dl->cinTexturenum <= 0 || !CIN_IS_ACTIVE( tr.cinematics[hCin].state ))
		{
			// cinematic textures limit exceeded or movie not found
			dl->spotlightTexture = tr.spotlightTexture[1];
			return true;
		}

		gl_movie_t *cin = &tr.cinematics[hCin];
		float cin_time;

		// advances cinematic time
		cin_time = fmod( clent->curstate.fuser2, cin->length );

		// read the next frame
		int cin_frame = CIN_GET_FRAME_NUMBER( cin->state, cin_time );

		if( cin_frame != dl->lastframe )
		{
			// upload the new frame
			byte *raw = CIN_GET_FRAMEDATA( cin->state, cin_frame );
			CIN_UPLOAD_FRAME( tr.cinTextures[dl->cinTexturenum-1], cin->xres, cin->yres, cin->xres, cin->yres, raw );
			dl->lastframe = cin_frame;
		}

		if( clent->curstate.effects & EF_NOSHADOW )
			flags |= DLF_NOSHADOWS;

		if( clent->curstate.effects & EF_NOBUMP )
			flags |= DLF_NOBUMP;

		R_GetLightVectors( clent, origin, angles );
		R_SetupLightParams( dl, origin, angles, radius, clent->curstate.scale, LIGHT_SPOT, flags );
		R_SetupLightTexture( dl, tr.cinTextures[dl->cinTexturenum-1] );

		dl->color[0] = (float)clent->curstate.rendercolor.r / 128;
		dl->color[1] = (float)clent->curstate.rendercolor.g / 128;
		dl->color[2] = (float)clent->curstate.rendercolor.b / 128;
		dl->die = GET_CLIENT_TIME() + 0.05f;

		return true; // no reason to drawing this entity
          }

	if( clent->curstate.effects & EF_SCREENMOVIE )
	{
		// update cin sound properly
		R_UpdateCinSound( clent );
	}

	clent->curstate.renderamt = R_ComputeFxBlend( clent );

	if( !R_OpaqueEntity( clent ))
	{
		if( clent->curstate.renderamt <= 0.0f )
			return true; // invisible
	}
#if 0
	if( clent->model->type == mod_brush && CVAR_TO_BOOL( r_test ))
		return true;
#endif
	if( clent->model->type == mod_brush )
		clent->hCachedMatrix = GL_CacheState( clent->origin, clent->angles, ( clent->curstate.renderfx == SKYBOX_ENTITY ));
	clent->curstate.entityType = entityType;

	// mark static entity as visible
	if( entityType == ET_FRAGMENTED )
	{
		// non-solid statics wants a new lighting too :-)
		SetBits( clent->curstate.iuser1, CF_STATIC_ENTITY );
		clent->visframe = tr.realframecount;
	}

	if( tr.num_draw_entities < MAX_VISIBLE_ENTS )
	{
		tr.draw_entities[tr.num_draw_entities] = clent;
		tr.num_draw_entities++;
	}

	return true;
}