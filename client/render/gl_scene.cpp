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
#include "gl_postprocess.h"
#include "screenfade.h"
#include "shake.h"
#include "cl_env_dynlight.h"
#include <unordered_map>

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

	if (FBitSet(r_cubemap->flags, FCVAR_CHANGED))
	{
		ClearBits(r_cubemap->flags, FCVAR_CHANGED);
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

	if (FBitSet(r_show_luminance->flags, FCVAR_CHANGED))
	{
		ClearBits(r_show_luminance->flags, FCVAR_CHANGED);
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

	tr.sky_camera = nullptr;
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

	if (tr.params_changed)
	{
		R_InitDynLightShaders();
		InitPostprocessShaders();
	}
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

static void R_SetupPlayerFlashlight(cl_entity_t *ent)
{
	pmtrace_t ptr;
	Vector origin, vecEnd, angles;
	Vector forward, right, up;
	static std::unordered_map<int, float> cached_smooth_values;

	if (UTIL_IsLocal(ent->index))
	{
		vec3_t viewOffset;
		cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
        gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(viewOffset);
		origin = localPlayer->origin + viewOffset;

		gEngfuncs.GetViewAngles(angles);
		gEngfuncs.pfnAngleVectors(angles, forward, right, up);
		origin += (up * 6.0f) + (right * 5.0f) + (forward * 2.0f);
		vecEnd = origin + forward * 700.0f;
	}
	else
	{
		angles = ent->angles;
		angles.x = -angles.x * 3;
		gEngfuncs.pfnAngleVectors(angles, forward, right, up);
		origin = ent->curstate.origin;
		vecEnd = origin + forward * 700.0f;
	}

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, vecEnd, 0, -1, &ptr);

	float addideal = 0.0f;
	if (ptr.fraction < 1.0f) {
		addideal = (1.0f - ptr.fraction) * 30.0f;
	}

	float &add = cached_smooth_values[ent->index];
	float speed = (add - addideal) * 10.0f;
	if (speed < 0) speed *= -1.0f;

	if (add < addideal)
	{
		add += GET_FRAMETIME() * speed;
		if (add > addideal) add = addideal;
	}
	else if (add > addideal)
	{
		add -= GET_FRAMETIME() * speed;
		if (add < addideal) add = addideal;
	}

	CDynLight *flashlight = CL_AllocDlight(FLASHLIGHT_KEY - ent->index);
	R_SetupLightParams(flashlight, origin, angles, 700.0f, 35.0f + add, LIGHT_SPOT);
	R_SetupLightTexture(flashlight, tr.flashlightTexture);

	flashlight->flags = DLF_PARENTENTITY_NOSHADOW;
	flashlight->parentEntity = ent;
	flashlight->color = Vector(1.4f, 1.4f, 1.4f); // make model dymanic lighting happy
	flashlight->die = tr.time + 0.05f;
}

static bool R_HandleLightEntity(cl_entity_t *ent)
{
	// not light entity
	if (ent->curstate.renderfx != kRenderFxDynamicLight &&
		ent->curstate.renderfx != kRenderFxCinemaLight)
	{
		return false;
	}

	CDynLight *dlight = CL_AllocDlight(ent->curstate.number);
	CEntityEnvDynlight entity(ent);
	TextureHandle tex = TextureHandle::Null();
	int flags = 0, type = 0;
	
	if (!entity.Cinematic()) // dynamic light
	{
		if (entity.Spotlight()) // spotlight
		{
			tex = tr.spotlightTexture[entity.GetSpotlightTextureIndex()];
			type = LIGHT_SPOT;
		}
		else 
			type = LIGHT_OMNI;

		if (entity.DisableShadows())
			flags |= DLF_NOSHADOWS;

		if (entity.DisableBump())
			flags |= DLF_NOBUMP;

		if (entity.EnableLensFlare())
			flags |= DLF_LENSFLARE;
	}
	else // dynamic light with avi file
	{
		if (!entity.GetAviFileIndex())
			return true; // bad avi file

		if (dlight->spotlightTexture == tr.spotlightTexture[1])
			return true; // bad avi file

		flags = DLF_ASPECT3X4;	// fit to film01.avi aspect
		type = LIGHT_SPOT;

		// found the corresponding cinstate
		const char *cinname = gRenderfuncs.GetFileByIndex(entity.GetAviFileIndex());
		int hCin = R_PrecacheCinematic(cinname);

		if (hCin >= 0 && !dlight->cinTexturenum)
			dlight->cinTexturenum = R_AllocateCinematicTexture(TF_SPOTLIGHT);

		if (hCin == -1 || dlight->cinTexturenum <= 0 || !CIN_IS_ACTIVE(tr.cinematics[hCin].state))
		{
			// cinematic textures limit exceeded or movie not found
			dlight->spotlightTexture = tr.spotlightTexture[1];
			return true;
		}

		gl_movie_t *cin = &tr.cinematics[hCin];

		// advances cinematic time
		float cin_time = fmod(entity.GetCinTime(), cin->length);

		// read the next frame
		int cin_frame = CIN_GET_FRAME_NUMBER(cin->state, cin_time);
		if (cin_frame != dlight->lastframe)
		{
			// upload the new frame
			byte *raw = CIN_GET_FRAMEDATA(cin->state, cin_frame);
			CIN_UPLOAD_FRAME(tr.cinTextures[dlight->cinTexturenum - 1], cin->xres, cin->yres, cin->xres, cin->yres, raw);
			dlight->lastframe = cin_frame;
		}

		if (entity.DisableShadows())
			flags |= DLF_NOSHADOWS;

		if (entity.DisableBump())
			flags |= DLF_NOBUMP;

		tex = tr.cinTextures[dlight->cinTexturenum - 1];
	}

	Vector origin, angles;
	entity.GetLightVectors(origin, angles);
	R_SetupLightParams(dlight, origin, angles, entity.GetRadius(), entity.GetFOVAngle(), type, flags);
	R_SetupLightTexture(dlight, tex);

	dlight->lightstyleIndex = entity.GetLightstyleIndex();
	float lightstyleMult = tr.lightstyle[dlight->lightstyleIndex] / 550.f;
	dlight->color = entity.GetColor() * entity.GetBrightness() * lightstyleMult;
	dlight->die = tr.time + 0.05f;
	
	return true; // no reason to drawing this entity
}
/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType )
{
	if (!CVAR_TO_BOOL( r_drawentities ))
		return false; // not allow to drawing

	if (!clent || !clent->model)
		return false; // if set to invisible, skip

	if (clent->curstate.effects & EF_NODRAW)
		return false; // done

	if (clent->curstate.effects & EF_SKYCAMERA)
	{
		tr.sky_camera = clent;
		return true; // env_sky entity
	}

	if (entityType == ET_PLAYER && RP_LOCALCLIENT( clent ))
	{
		if( tr.local_client_added )
			return false; // already present in list
		tr.local_client_added = true;
	}

	if (R_HandleLightEntity(clent))
		return true;

	if (clent->curstate.effects & EF_DIMLIGHT) {
		R_SetupPlayerFlashlight(clent);
	}

	if (clent->curstate.effects & EF_SCREENMOVIE)
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
		clent->hCachedMatrix = GL_CacheState( clent->origin, clent->angles );
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

bool R_FullBright()
{
	if (CVAR_TO_BOOL(r_fullbright))
		return true;
	if (worldmodel && !worldmodel->lightdata)
		return true;
	else
		return false;
}
