/*
r_misc.cpp - renderer misceallaneous
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
#include "r_particle.h"
#include "entity_types.h"
#include "event_api.h"
#include "r_weather.h"
#include "r_efx.h"
#include "pm_defs.h"
#include <mathlib.h>
#include "r_view.h"

#define FLASHLIGHT_DISTANCE		2048	// in units

// experimental stuff
#define FLASHLIGHT_SPOT		// use new-style flashlight like in Paranoia

/*
================
HUD_UpdateFlashlight

update client flashlight
================
*/
void HUD_UpdateFlashlight( cl_entity_t *pEnt )
{
	Vector	v_angles, forward, right, up;
	Vector	v_origin;

	if( UTIL_IsLocal( pEnt->index ))
	{
		ref_params_t tmpRefDef = RI.refdef;

		// player seen through camera. Restore firstperson view here
		if( RI.refdef.viewentity > RI.refdef.maxclients )
			V_CalcFirstPersonRefdef( &tmpRefDef );

		v_angles = tmpRefDef.viewangles;
		v_origin = tmpRefDef.vieworg;
	}
	else
	{
		// restore viewangles from angles
		v_angles[PITCH] = -pEnt->angles[PITCH] * 3;
		v_angles[YAW] = pEnt->angles[YAW];
		v_angles[ROLL] = 0;	// no roll
		v_origin = pEnt->origin;
	}

	AngleVectors( v_angles, forward, NULL, NULL );

#ifdef FLASHLIGHT_SPOT
	plight_t	*pl = CL_AllocPlight( pEnt->curstate.number );

	pl->die = GET_CLIENT_TIME() + 0.05f; // die at next frame
	pl->color.r = pl->color.g = pl->color.b = 255;
	pl->flags = CF_NOSHADOWS;

	R_SetupLightProjectionTexture( pl, pEnt );
	R_SetupLightProjection( pl, v_origin, v_angles, 768, 50 );
	R_SetupLightAttenuationTexture( pl );
#else
	Vector vecEnd = v_origin + forward * FLASHLIGHT_DISTANCE;

	pmtrace_t	trace;
	int traceFlags = PM_STUDIO_BOX;

	if( r_lighting_extended->value < 2 )
		traceFlags |= PM_GLASS_IGNORE;

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( v_origin, vecEnd, traceFlags, -1, &trace );
	float falloff = trace.fraction * FLASHLIGHT_DISTANCE;

	if( falloff < 250.0f ) falloff = 1.0f;
	else falloff = 250.0f / falloff;

	falloff *= falloff;

	// update flashlight endpos
	dlight_t	*dl = gEngfuncs.pEfxAPI->CL_AllocDlight( pEnt->curstate.number );
	dl->origin = trace.endpos;
	dl->die = GET_CLIENT_TIME() + 0.01f; // die on next frame
	dl->color.r = bound( 0, 255 * falloff, 255 );
	dl->color.g = bound( 0, 255 * falloff, 255 );
	dl->color.b = bound( 0, 255 * falloff, 255 );
	dl->radius = 72;
#endif
}

/*
========================
HUD_AddEntity

Return 0 to filter entity from visible list for rendering
========================
*/
int HUD_AddEntity( int type, struct cl_entity_s *ent, const char *modelname )
{
	if( g_fRenderInitialized )
	{
		// use engine renderer
		if( gl_renderer->value == 0 )
			return 1;

		if( ent->curstate.effects & EF_SKYCAMERA )
		{
			// found env_sky
			tr.sky_camera = ent;
			return 0;
		}

		if( type == ET_BEAM )
		{
			R_AddServerBeam( ent );
			return 0;
		}

		if( !R_AddEntity( ent, type ))
			return 0;

		// apply effects
		if( ent->curstate.effects & EF_BRIGHTFIELD )
			gEngfuncs.pEfxAPI->R_EntityParticles( ent );

		// add in muzzleflash effect
		if( ent->curstate.effects & EF_MUZZLEFLASH )
		{
			if( ent == gEngfuncs.GetViewModel( ))
				ent->curstate.effects &= ~EF_MUZZLEFLASH;

			// make sure what attachment is valid
			if( ent->origin != ent->attachment[0] )
                    	{
				dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocElight( 0 );

				dl->origin = ent->attachment[0];
				dl->die = gEngfuncs.GetClientTime() + 0.05f;
				dl->color.r = 255;
				dl->color.g = 180;
				dl->color.b = 64;
				dl->radius = 100;
			}
		}

		// add light effect
		if( ent->curstate.effects & EF_LIGHT )
		{
			dlight_t	*dl = gEngfuncs.pEfxAPI->CL_AllocDlight( ent->curstate.number );
			dl->origin = ent->origin;
			dl->die = gEngfuncs.GetClientTime();	// die at next frame
			dl->color.r = 100;
			dl->color.g = 100;
			dl->color.b = 100;
			dl->radius = 200;
			gEngfuncs.pEfxAPI->R_RocketFlare( ent->origin );
		}

		// add dimlight
		if( ent->curstate.effects & EF_DIMLIGHT )
		{
			if( type == ET_PLAYER )
			{
				HUD_UpdateFlashlight( ent );
			}
			else
			{
				dlight_t	*dl = gEngfuncs.pEfxAPI->CL_AllocDlight( ent->curstate.number );
				dl->origin = ent->origin;
				dl->die = gEngfuncs.GetClientTime();	// die at next frame
				dl->color.r = 255;
				dl->color.g = 255;
				dl->color.b = 255;
				dl->radius = gEngfuncs.pfnRandomLong( 200, 230 );
			}
		}	

		if( ent->curstate.effects & EF_BRIGHTLIGHT )
		{			
			dlight_t	*dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
			dl->origin = ent->origin;
			dl->origin.z += 16;
			dl->die = GET_CLIENT_TIME() + 0.001f; // die at next frame
			dl->color.r = 255;
			dl->color.g = 255;
			dl->color.b = 255;

			if( type == ET_PLAYER )
				dl->radius = 430;
			else dl->radius = gEngfuncs.pfnRandomLong( 400, 430 );
		}

		// projected light can be attached like as normal dlight
		if( ent->curstate.effects & EF_PROJECTED_LIGHT )
		{
			plight_t	*pl = CL_AllocPlight( ent->curstate.number );
			float factor = 1.0f;

			if( ent->curstate.renderfx )
			{
				factor = RI.lightstylevalue[ent->curstate.renderfx] * (1.0f/255.0f);
			}

			if( ent->curstate.rendercolor.r == 0 && ent->curstate.rendercolor.g == 0 && ent->curstate.rendercolor.b == 0 )
			{
				pl->color.r = pl->color.g = pl->color.b = 255;
			}
			else
			{
				pl->color.r = ent->curstate.rendercolor.r;
				pl->color.g = ent->curstate.rendercolor.g;
				pl->color.b = ent->curstate.rendercolor.b;
			}

			pl->color.r *= factor;
			pl->color.g *= factor;
			pl->color.b *= factor;

			float radius = ent->curstate.scale ? (ent->curstate.scale * 8.0f) : 500; // default light radius
			float fov = ent->curstate.iuser2 ? ent->curstate.iuser2 : 50;
			pl->die = GET_CLIENT_TIME() + 0.05f; // die at next frame
			pl->flags = ent->curstate.iuser1;
			Vector origin, angles;

			R_GetLightVectors( ent, origin, angles );
			R_SetupLightProjectionTexture( pl, ent );
			R_SetupLightProjection( pl, origin, angles, radius, fov );
			R_SetupLightAttenuationTexture( pl, ent->curstate.renderamt );
		}

		// dynamic light can be attached like as normal dlight
		if( ent->curstate.effects & EF_DYNAMIC_LIGHT )
		{
			if( tr.attenuationTexture3D && tr.dlightCubeTexture )
			{
				plight_t	*pl = CL_AllocPlight( ent->curstate.number );
				float factor = 1.0f;

				if( ent->curstate.renderfx )
				{
					factor = RI.lightstylevalue[ent->curstate.renderfx] * (1.0f/255.0f);
					factor = bound( 0.0f, factor, 1.0f );
				}

				if( ent->curstate.rendercolor.r == 0 && ent->curstate.rendercolor.g == 0 && ent->curstate.rendercolor.b == 0 )
				{
					pl->color.r = pl->color.g = pl->color.b = 255;
				}
				else
				{
					pl->color.r = ent->curstate.rendercolor.r;
					pl->color.g = ent->curstate.rendercolor.g;
					pl->color.b = ent->curstate.rendercolor.b;
				}

				pl->color.r *= factor;
				pl->color.g *= factor;
				pl->color.b *= factor;

				float radius = ent->curstate.scale ? (ent->curstate.scale * 8.0f) : 300; // default light radius
				pl->die = GET_CLIENT_TIME() + 0.05f; // die at next frame
				pl->flags = ent->curstate.iuser1;
				pl->projectionTexture = tr.dlightCubeTexture;
				pl->pointlight = true;
				Vector origin, angles;

				R_GetLightVectors( ent, origin, angles );

				if( pl->flags & CF_NOLIGHT_IN_SOLID )
				{
					pmtrace_t	tr;

					// test the lights who stuck in the solid geometry
					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( origin, origin, PM_STUDIO_IGNORE, -1, &tr );

					// an experimental feature for point lights
					if( tr.allsolid ) radius = 0.0f;
				}

				if( radius != 0.0f )
				{
					R_SetupLightProjection( pl, origin, angles, radius, 90.0f );
					R_SetupLightAttenuationTexture( pl );
				}
				else
				{
					// light in solid
					pl->radius = 0.0f;
				}
			}
			else
			{
				// cubemaps or 3d textures isn't supported: use old-style dlights
				dlight_t	*dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );

				if( ent->curstate.rendercolor.r == 0 && ent->curstate.rendercolor.g == 0 && ent->curstate.rendercolor.b == 0 )
				{
					dl->color.r = dl->color.g = dl->color.b = 255;
				}
				else
				{
					dl->color.r = ent->curstate.rendercolor.r;
					dl->color.g = ent->curstate.rendercolor.g;
					dl->color.b = ent->curstate.rendercolor.b;
				}

				dl->radius = ent->curstate.scale ? (ent->curstate.scale * 8.0f) : 300; // default light radius
				dl->die = GET_CLIENT_TIME() + 0.001f; // die at next frame
				dl->origin = ent->origin;
			}
		}

		if( ent->model->type == mod_studio )
		{
			if (ent->model->flags & STUDIO_ROTATE)
				ent->angles[1] = anglemod(100 * GET_CLIENT_TIME());

			if( ent->model->flags & STUDIO_GIB )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 2 );
			else if( ent->model->flags & STUDIO_ZOMGIB )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 4 );
			else if( ent->model->flags & STUDIO_TRACER )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 3 );
			else if( ent->model->flags & STUDIO_TRACER2 )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 5 );
			else if( ent->model->flags & STUDIO_ROCKET )
			{
				dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( ent->curstate.number );
				dl->origin = ent->origin;
				dl->color.r = 255;
				dl->color.g = 255;
				dl->color.b = 255;

				// HACKHACK: get radius from head entity
				if( ent->curstate.rendermode != kRenderNormal )
					dl->radius = max( 0, ent->curstate.renderamt - 55 );
				else dl->radius = 200;
				dl->die = GET_CLIENT_TIME() + 0.01;

				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 0 );
			}
			else if( ent->model->flags & STUDIO_GRENADE )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 1 );
			else if( ent->model->flags & STUDIO_TRACER3 )
				gEngfuncs.pEfxAPI->R_RocketTrail( ent->prevstate.origin, ent->curstate.origin, 6 );
		}
		return 0;
	}

	return 1;
}

/*
=========================
HUD_TxferLocalOverrides

The server sends us our origin with extra precision as part of the clientdata structure, not during the normal
playerstate update in entity_state_t.  In order for these overrides to eventually get to the appropriate playerstate
structure, we need to copy them into the state structure at this point.
=========================
*/
void HUD_TxferLocalOverrides( struct entity_state_s *state, const struct clientdata_s *client )
{
	state->origin = client->origin;
	state->velocity = client->velocity;

	// Duck prevention
	state->iuser3 = client->iuser3;

	// Fire prevention
	state->iuser4 = client->iuser4;

	// always have valid PVS message
	r_currentMessageNum = state->messagenum;
}

/*
=========================
HUD_ProcessPlayerState

We have received entity_state_t for this player over the network.  We need to copy appropriate fields to the
playerstate structure
=========================
*/
void HUD_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src )
{
	// Copy in network data
	dst->origin	= src->origin;
	dst->angles	= src->angles;

	dst->velocity	= src->velocity;
          dst->basevelocity	= src->basevelocity;
          	
	dst->frame	= src->frame;
	dst->modelindex	= src->modelindex;
	dst->skin		= src->skin;
	dst->effects	= src->effects;
	dst->weaponmodel	= src->weaponmodel;
	dst->movetype	= src->movetype;
	dst->sequence	= src->sequence;
	dst->animtime	= src->animtime;
	
	dst->solid	= src->solid;
	
	dst->rendermode	= src->rendermode;
	dst->renderamt	= src->renderamt;	
	dst->rendercolor.r	= src->rendercolor.r;
	dst->rendercolor.g	= src->rendercolor.g;
	dst->rendercolor.b	= src->rendercolor.b;
	dst->renderfx	= src->renderfx;

	dst->framerate	= src->framerate;
	dst->body		= src->body;

	dst->friction	= src->friction;
	dst->gravity	= src->gravity;
	dst->gaitsequence	= src->gaitsequence;
	dst->usehull	= src->usehull;
	dst->playerclass	= src->playerclass;
	dst->team		= src->team;
	dst->colormap	= src->colormap;

	memcpy( &dst->controller[0], &src->controller[0], 4 * sizeof( byte ));
	memcpy( &dst->blending[0], &src->blending[0], 2 * sizeof( byte ));

	// Save off some data so other areas of the Client DLL can get to it
	cl_entity_t *player = GET_LOCAL_PLAYER(); // Get the local player's index

	if( dst->number == player->index )
	{
		// always have valid PVS message
		r_currentMessageNum = src->messagenum;
	}
}

/*
=========================
HUD_TxferPredictionData

Because we can predict an arbitrary number of frames before the server responds with an update, we need to be able to copy client side prediction data in
 from the state that the server ack'd receiving, which can be anywhere along the predicted frame path ( i.e., we could predict 20 frames into the future and the server ack's
 up through 10 of those frames, so we need to copy persistent client-side only state from the 10th predicted frame to the slot the server
 update is occupying.
=========================
*/
void HUD_TxferPredictionData( entity_state_t *ps, const entity_state_t *pps, clientdata_t *pcd, const clientdata_t *ppcd, weapon_data_t *wd, const weapon_data_t *pwd )
{
	ps->oldbuttons	= pps->oldbuttons;
	ps->flFallVelocity	= pps->flFallVelocity;
	ps->iStepLeft	= pps->iStepLeft;
	ps->playerclass	= pps->playerclass;

	pcd->viewmodel	= ppcd->viewmodel;
	pcd->m_iId	= ppcd->m_iId;
	pcd->ammo_shells	= ppcd->ammo_shells;
	pcd->ammo_nails	= ppcd->ammo_nails;
	pcd->ammo_cells	= ppcd->ammo_cells;
	pcd->ammo_rockets	= ppcd->ammo_rockets;
	pcd->m_flNextAttack	= ppcd->m_flNextAttack;
	pcd->fov		= ppcd->fov;
	pcd->weaponanim	= ppcd->weaponanim;
	pcd->tfstate	= ppcd->tfstate;
	pcd->maxspeed	= ppcd->maxspeed;

	pcd->deadflag	= ppcd->deadflag;

	// Duck prevention
	pcd->iuser3 	= ppcd->iuser3;

	// Fire prevention
	pcd->iuser4 	= ppcd->iuser4;

	pcd->fuser2	= ppcd->fuser2;
	pcd->fuser3	= ppcd->fuser3;

	pcd->vuser1 = ppcd->vuser1;
	pcd->vuser2 = ppcd->vuser2;
	pcd->vuser3 = ppcd->vuser3;
	pcd->vuser4 = ppcd->vuser4;

	memcpy( wd, pwd, 32 * sizeof( weapon_data_t ));
}

/*
=========================
HUD_CreateEntities
	
Gives us a chance to add additional entities to the render this frame
=========================
*/
void HUD_CreateEntities( void )
{
	// e.g., create a persistent cl_entity_t somewhere.
	// Load an appropriate model into it ( gEngfuncs.CL_LoadModel )
	// Call gEngfuncs.CL_CreateVisibleEntity to add it to the visedicts list

	if( tr.world_has_portals || tr.world_has_screens )
		HUD_AddEntity( ET_PLAYER, GET_LOCAL_PLAYER(), GET_LOCAL_PLAYER()->model->name );
}

//======================
//	DRAW BEAM EVENT
//
// special effect for displacer
//======================
void HUD_DrawBeam( void )
{
	cl_entity_t *view = GET_VIEWMODEL();
	int m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/plasma.spr" );

	gEngfuncs.pEfxAPI->R_BeamEnts( view->index|0x1000, view->index|0x2000, m_iBeam, 1.05f, 0.8f, 0.5f, 0.5f, 0.6f, 0, 10, 2, 10, 0 );
	gEngfuncs.pEfxAPI->R_BeamEnts( view->index|0x1000, view->index|0x3000, m_iBeam, 1.05f, 0.8f, 0.5f, 0.5f, 0.6f, 0, 10, 2, 10, 0 );
	gEngfuncs.pEfxAPI->R_BeamEnts( view->index|0x1000, view->index|0x4000, m_iBeam, 1.05f, 0.8f, 0.5f, 0.5f, 0.6f, 0, 10, 2, 10, 0 );
}

//======================
//	Eject Shell
//
// eject specified shell from viewmodel
// NOTE: shell model must be precached on a server
//======================
void HUD_EjectShell( const struct mstudioevent_s *event, const struct cl_entity_s *entity )
{
	if( entity != GET_VIEWMODEL( ))
		return; // can eject shells only from viewmodel

	Vector view_ofs, ShellOrigin, ShellVelocity, forward, right, up;
	Vector angles = Vector( 0, entity->angles[YAW], 0 );
	Vector origin = entity->origin;
	
	float fR, fU;

          int shell = gEngfuncs.pEventAPI->EV_FindModelIndex( event->options );

	if( !shell )
	{
		ALERT( at_error, "model %s not precached\n", event->options );
		return;
	}

	for( int i = 0; i < 3; i++ )
	{
		if( angles[i] < -180 ) angles[i] += 360; 
		else if( angles[i] > 180 ) angles[i] -= 360;
          }

	angles.x = -angles.x;
	AngleVectors( angles, forward, right, up );
          
	fR = RANDOM_FLOAT( 50, 70 );
	fU = RANDOM_FLOAT( 100, 150 );

	for( i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = RI.refdef.simvel[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i]   = RI.refdef.vieworg[i] + up[i] * -12 + forward[i] * 20 + right[i] * 4;
	}

	gEngfuncs.pEfxAPI->R_TempModel( ShellOrigin, ShellVelocity, angles, RANDOM_LONG( 5, 10 ), shell, TE_BOUNCE_SHELL );
}

/*
=========================
HUD_StudioEvent

The entity's studio model description indicated an event was
fired during this frame, handle the event by it's tag ( e.g., muzzleflash, sound )
=========================
*/
void HUD_StudioEvent( const struct mstudioevent_s *event, const struct cl_entity_s *entity )
{
	switch( event->event )
	{
	case 5001:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[0], Q_atoi( event->options ));
		break;
	case 5011:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[1], Q_atoi( event->options ));
		break;
	case 5021:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[2], Q_atoi( event->options ));
		break;
	case 5031:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[3], Q_atoi( event->options ));
		break;
	case 5002:
		gEngfuncs.pEfxAPI->R_SparkEffect( (float *)&entity->attachment[0], Q_atoi( event->options ), -100, 100 );
		break;
	case 5004: // client side sound		
		gEngfuncs.pfnPlaySoundByNameAtLocation( (char *)event->options, 1.0, (float *)&entity->attachment[0] );
		break;
	case 5005: // client side sound with random pitch		
		gEngfuncs.pEventAPI->EV_PlaySound( entity->index, (float *)&entity->attachment[0], CHAN_WEAPON, (char *)event->options,
		RANDOM_FLOAT( 0.7f, 0.9f ), ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ));
		break;
	case 5050: // Special event for displacer
		HUD_DrawBeam();
		break;
	case 5060:
	          HUD_EjectShell( event, entity );
		break;
	default:
		ALERT( at_error, "Unknown event %i with options %i\n", event->event, event->options );
		break;
	}
}

/*
==================
R_NewMap

Called always when map is changed or restarted
==================
*/
void R_NewMap( void )
{
	int	i;

	// get the actual screen size
	glState.width = RENDER_GET_PARM( PARM_SCREEN_WIDTH, 0 );
	glState.height = RENDER_GET_PARM( PARM_SCREEN_HEIGHT, 0 );

	// release old mirror textures
	for( i = 0; i < MAX_MIRRORS; i++ )
	{
		if( !tr.mirrorTextures[i] ) break;
		FREE_TEXTURE( tr.mirrorTextures[i] );
	}

	for( i = 0; i < MAX_MIRRORS; i++ )
	{
		if( !tr.portalTextures[i] ) break;
		FREE_TEXTURE( tr.portalTextures[i] );
	}

	for( i = 0; i < MAX_MIRRORS; i++ )
	{
		if( !tr.screenTextures[i] ) break;
		FREE_TEXTURE( tr.screenTextures[i] );
	}

	for( i = 0; i < MAX_SHADOWS; i++ )
	{
		if( !tr.shadowTextures[i] ) break;
		FREE_TEXTURE( tr.shadowTextures[i] );
	}

	for( i = 0; i < tr.num_framebuffers; i++ )
	{
		if( !tr.frame_buffers[i].init ) break;
		R_FreeFrameBuffer( i );
	}

	CL_ClearPlights();

	R_FreeCinematics(); // free old cinematics

	memset( tr.mirrorTextures, 0, sizeof( tr.mirrorTextures ));
	memset( tr.portalTextures, 0, sizeof( tr.portalTextures ));
	memset( tr.screenTextures, 0, sizeof( tr.screenTextures ));
	memset( tr.shadowTextures, 0, sizeof( tr.shadowTextures ));
	memset( tr.frame_buffers, 0, sizeof( tr.frame_buffers ));
	tr.num_framebuffers = 0;

	r_viewleaf = r_viewleaf2 = NULL;
	tr.framecount = tr.visframecount = 1;	// no dlight cache

	if( GL_Support( R_FRAMEBUFFER_OBJECT ))
	{
		// allocate FBO's
		tr.fbo[FBO_MIRRORS] = R_AllocFrameBuffer();
		tr.fbo[FBO_SCREENS] = R_AllocFrameBuffer();
		tr.fbo[FBO_PORTALS] = R_AllocFrameBuffer();
	}

	// setup the skybox sides
	for( i = 0; i < 6; i++ )
		tr.skyboxTextures[i] = RENDER_GET_PARM( PARM_TEX_SKYBOX, i );

	tr.skytexturenum = RENDER_GET_PARM( PARM_TEX_SKYTEXNUM, 0 ); // not a gl_texturenum!

	v_intermission_spot = NULL;

	R_InitCinematics();

	R_InitBloomTextures();

	if( Q_stricmp( worldmodel->name, tr.worldname ))
	{
		Q_strncpy( tr.worldname, worldmodel->name, sizeof( tr.worldname ));
		R_ParseGrassFile();
	}
}
