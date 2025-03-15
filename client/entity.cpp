//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Client side entity management functions

#include <memory.h>

#include "hud.h"
#include "utils.h"
#include "const.h"
#include "entity_types.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"	
#include "gl_local.h"
#include "gl_studio.h"
#include "gl_cvars.h"
#include "gl_rpart.h"
#include "exportdef.h"
#include "events/egon_fire_event.h"

void Game_AddObjects( void );

extern vec3_t v_origin;
//extern vec3_t g_vSpread;
//extern int g_iGunMode;

int g_iAlive = 1;
int r_currentMessageNum = 0;
int GlowFilterEntities ( int type, struct cl_entity_s *ent, const char *modelname ); // buz

/*
========================
HUD_AddEntity
	Return 0 to filter entity from visible list for rendering
========================
*/
int DLLEXPORT HUD_AddEntity(int type, struct cl_entity_s *ent, const char *modelname)
{
	if( ent->curstate.rendermode == kRenderTransAlpha && ent->model && ent->model->type == mod_brush )
	{
		// fix invisible grates on fallback renderer
		ent->curstate.renderamt = 255;
	}

	if( g_fRenderInitialized )
	{
		// use engine renderer
		if(cv_renderer->value == 0 )
			return 1;

		if( type == ET_BEAM )
			return 1;	// let the engine draw beams

		R_AddEntity( ent, type );

		return 0;
	}
	
	// each frame every entity passes this function, so the overview hooks it to filter the overview entities
	// in spectator mode:
	// each frame every entity passes this function, so the overview hooks 
	// it to filter the overview entities

	//if ( g_iUser1 )
	//{
	//	gHUD.m_Spectator.AddOverviewEntity( type, ent, modelname );

	//	if ( (	g_iUser1 == OBS_IN_EYE || gHUD.m_Spectator.m_pip->value == INSET_IN_EYE ) &&
	//			ent->index == g_iUser2 )
	//		return 0;	// don't draw the player we are following in eye

	//}

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
void DLLEXPORT HUD_TxferLocalOverrides( struct entity_state_s *state, const struct clientdata_s *client )
{
	state->origin = client->origin;
	state->velocity = client->velocity;

	gHUD.m_iViewModelIndex = client->viewmodel;

	// Spectator
	state->iuser1 = client->iuser1;
	state->iuser2 = client->iuser2;

	// Duck prevention
	state->iuser3 = client->iuser3;

	// Fire prevention
	state->iuser4 = client->iuser4;

	// always have valid PVS message
	r_currentMessageNum = state->messagenum;

	// IMPORTANT: this data doesn't present in entity_state_t
	// but only in clientdata_t for local player (gun params, spectator mode etc)
	//g_iUser1 = client->iuser1;
	//g_iUser2 = client->iuser2;
	//g_iUser3 = client->iuser3;

	// buz
	//g_vSpread = client->vuser1;
	//g_iGunMode = client->iuser4;
}

/*
=========================
HUD_ProcessPlayerState

We have received entity_state_t for this player over the network.  We need to copy appropriate fields to the
playerstate structure
=========================
*/
void DLLEXPORT HUD_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src )
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

	dst->fuser1	= src->fuser1;
	dst->fuser2	= src->fuser2;
	dst->fuser3	= src->fuser3;
	dst->fuser4	= src->fuser4;

	dst->vuser1	= src->vuser1;
	dst->vuser2	= src->vuser2;
	dst->vuser3	= src->vuser3;
	dst->vuser4	= src->vuser4;

	memcpy( &dst->controller[0], &src->controller[0], 4 * sizeof( byte ));
	memcpy( &dst->blending[0], &src->blending[0], 2 * sizeof( byte ));

	// Save off some data so other areas of the Client DLL can get to it
	//cl_entity_t *player = gEngfuncs.GetLocalPlayer();	// Get the local player's index
	//if ( dst->number == player->index )
	//{
	//	g_iPlayerClass = dst->playerclass;
	//	g_iTeamNumber = dst->team;
	//}
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
void DLLEXPORT HUD_TxferPredictionData ( struct entity_state_s *ps, const struct entity_state_s *pps, struct clientdata_s *pcd, const struct clientdata_s *ppcd, struct weapon_data_s *wd, const struct weapon_data_s *pwd )
{
	ps->oldbuttons				= pps->oldbuttons;
	ps->flFallVelocity			= pps->flFallVelocity;
	ps->iStepLeft				= pps->iStepLeft;
	ps->playerclass				= pps->playerclass;

	pcd->viewmodel				= ppcd->viewmodel;
	pcd->m_iId					= ppcd->m_iId;
	pcd->ammo_shells			= ppcd->ammo_shells;
	pcd->ammo_nails				= ppcd->ammo_nails;
	pcd->ammo_cells				= ppcd->ammo_cells;
	pcd->ammo_rockets			= ppcd->ammo_rockets;
	pcd->m_flNextAttack			= ppcd->m_flNextAttack;
	pcd->fov					= ppcd->fov;
	pcd->weaponanim				= ppcd->weaponanim;
	pcd->tfstate				= ppcd->tfstate;
	pcd->maxspeed				= ppcd->maxspeed;

	pcd->deadflag				= ppcd->deadflag;

	// Spectating or not dead == get control over view angles.
	g_iAlive = ( ppcd->iuser1 || ( pcd->deadflag == DEAD_NO ) ) ? 1 : 0;

	// Spectator
	pcd->iuser1					= ppcd->iuser1;
	pcd->iuser2					= ppcd->iuser2;

	// Duck prevention
	pcd->iuser3 = ppcd->iuser3;

	//if ( gEngfuncs.IsSpectateOnly() )
	//{
	//	// in specator mode we tell the engine who we want to spectate and how
	//	// iuser3 is not used for duck prevention (since the spectator can't duck at all)
	//	pcd->iuser1 = g_iUser1; // observer mode
	//	pcd->iuser2 = g_iUser2; // first target
	//	pcd->iuser3 = g_iUser3; // second target

	//}

	// Fire prevention
	pcd->iuser4 = ppcd->iuser4;

	pcd->fuser1 = ppcd->fuser1;
	pcd->fuser2 = ppcd->fuser2;
	pcd->fuser3 = ppcd->fuser3;
	pcd->fuser4 = ppcd->fuser4;

	pcd->vuser1 = ppcd->vuser1;
	pcd->vuser2 = ppcd->vuser2;
	pcd->vuser3 = ppcd->vuser3;
	pcd->vuser4 = ppcd->vuser4;

	memcpy( wd, pwd, MAX_LOCAL_WEAPONS * sizeof( weapon_data_t ) );
}

/*
=========================
HUD_CreateEntities
	
Gives us a chance to add additional entities to the render this frame
=========================
*/
void DLLEXPORT HUD_CreateEntities( void )
{
	// e.g., create a persistent cl_entity_t somewhere.
	// Load an appropriate model into it ( gEngfuncs.CL_LoadModel )
	// Call gEngfuncs.CL_CreateVisibleEntity to add it to the visedicts list

	//GetClientVoiceMgr()->CreateEntities();
	CEgonFireEvent::UpdateBeams();

	// used to draw legs
	HUD_AddEntity( ET_PLAYER, GET_LOCAL_PLAYER(), GET_LOCAL_PLAYER()->model->name );
}

void DlightFlash( const Vector &origin, int index )
{
	CDynLight *pl = CL_AllocDlight( index );
	R_SetupLightParams( pl, origin, g_vecZero, 128.0f, 0.0f, LIGHT_OMNI, DLF_NOSHADOWS );
	pl->color = Vector( 1.75f, 1.5f, 1.25f );
	pl->die = GET_CLIENT_TIME() + 0.06f;
}

/*
==============
CL_MuzzleFlash

Do muzzleflash
==============
*/
void HUD_MuzzleFlash( const cl_entity_t *e, const Vector &pos, const Vector &fwd, int type, float mul )
{
	TEMPENTITY	*pTemp;
	int		body, modelIndex, frameCount;
	Vector		flash_angles;
	int		flags = 0;
	float		scale;

	if( RP_NORMALPASS( ))
	{
		if( e == gEngfuncs.GetViewModel( ))
			flags |= EF_NOREFLECT|EF_NODEPTHTEST;
		else if( e->player && RP_LOCALCLIENT( e ))
			flags |= EF_REFLECTONLY;
	}
	else
	{
		if( e->player && RP_LOCALCLIENT( e ))
			flags |= EF_REFLECTONLY;
	}

	body = bound( 0, type % 5, 4 );
	scale = (type / 5) * 0.2f;
	if( scale == 0.0f ) scale = 0.5f;

	modelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/m_flash1.mdl");
	if( !modelIndex ) return;

	Mod_GetFrames( modelIndex, frameCount );

	if( body > ( frameCount - 1 ))
		body = frameCount - 1;

	// must set position for right culling on render
	if( !( pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh((float *)&pos, MODEL_HANDLE(modelIndex))))
		return;

	VectorAngles( -fwd, flash_angles );
	scale *= mul;

	pTemp->entity.curstate.rendermode = kRenderGlow;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.renderfx = 0;
	pTemp->entity.angles = flash_angles;
	pTemp->die = tr.time + 0.015f; // die at next frame
	pTemp->entity.curstate.body = body;
//	pTemp->flags |= FTENT_MDLANIMATE|FTENT_MDLANIMATELOOP;
	pTemp->entity.angles[2] = RANDOM_LONG( 0, 359 );
	pTemp->entity.curstate.scale = scale;
	pTemp->frameMax = frameCount - 1;

	gEngfuncs.CL_CreateVisibleEntity( ET_TEMPENTITY, &pTemp->entity );
	pTemp->entity.curstate.effects |= EF_FULLBRIGHT|flags; // CL_CreateVisibleEntity clears 'effect' field, so we need add it here
}

/*
=========================
HUD_StudioEvent

The entity's studio model description indicated an event was
fired during this frame, handle the event by it's tag ( e.g., muzzleflash, sound )
=========================
*/
void DLLEXPORT HUD_StudioEvent( const struct mstudioevent_s *event, const struct cl_entity_s *entity )
{
	float	rnd2 = gEngfuncs.pfnRandomFloat( -0.03, 0.03 );
	Vector	pos, dir;
	float	mul = 2.0f;
	int	shell;

//	ALERT( at_console, "Play event: %i, options %s, framecount %i\n", event->event, event->options, tr.realframecount );

	if( entity == GET_VIEWMODEL( ))
		mul = 8.0f;

	switch( event->event )
	{
	case 5001:
		R_StudioAttachmentPosDir( entity, 0, &pos, &dir );
		HUD_MuzzleFlash( entity, pos, dir, atoi( event->options), mul );
		DlightFlash((float *)&entity->attachment[0], entity->index ); 		
		g_pParticles.GunSmoke(entity->attachment[0], 2);
		break;
	case 5007:		 		
		g_pParticles.GunSmoke(entity->attachment[0], 2);
		break;
	case 5008:		 		
		g_pParticles.GunSmoke(entity->attachment[1], 2);
		break;
	case 5009: // custom shell ejection
		shell = gEngfuncs.pEventAPI->EV_FindModelIndex( event->options );
		R_StudioAttachmentPosDir( entity, 2, &pos, &dir );
		//EV_EjectBrass( pos, dir, 0, shell, TE_BOUNCE_SHELL );
		break;
	case 5010: // custom shell ejection, no velocity
		shell = gEngfuncs.pEventAPI->EV_FindModelIndex( event->options );
		R_StudioAttachmentPosDir( entity, 2, &pos, &dir );
		//EV_EjectBrass( pos, (float *)&g_vecZero, 0, shell, TE_BOUNCE_SHELL );
		break;
	case 5011:
		R_StudioAttachmentPosDir( entity, 1, &pos, &dir );
		HUD_MuzzleFlash( entity, pos, dir, atoi( event->options), mul );
		DlightFlash((float *)&entity->attachment[1], entity->index );
		g_pParticles.GunSmoke(entity->attachment[1], 2);
		break;
	case 5021:
		R_StudioAttachmentPosDir( entity, 2, &pos, &dir );
		HUD_MuzzleFlash( entity, pos, dir, atoi( event->options), mul );
		DlightFlash((float *)&entity->attachment[2], entity->index );
		g_pParticles.GunSmoke(entity->attachment[2], 2);
		break;
	case 5031:
		R_StudioAttachmentPosDir( entity, 3, &pos, &dir );
		HUD_MuzzleFlash( entity, pos, dir, atoi( event->options), mul );
		DlightFlash((float *)&entity->attachment[3], entity->index );
		g_pParticles.GunSmoke(entity->attachment[3], 2);
		break;
	case 5002:
		gEngfuncs.pEfxAPI->R_SparkEffect( (float *)&entity->attachment[0], atoi( event->options), -100, 100 );
		break;
	// Client side sound
	case 5004:		
		gEngfuncs.pfnPlaySoundByNameAtLocation( (char *)event->options, 1.0, (float *)&entity->attachment[0] );
		break;
	case 5005: // buz: left foot step (attach 3)
		{
			int contents = gEngfuncs.PM_PointContents( (float *)&entity->attachment[3], NULL );
			if (contents == CONTENTS_WATER) // leg is in the water
			{
				int waterEntity = gEngfuncs.PM_WaterEntity( (float *)&entity->attachment[3] );
				if ( waterEntity > 0 )	// water should be func_water entity
				{
					cl_entity_t *pwater = gEngfuncs.GetEntityByIndex( waterEntity );
					if ( pwater && ( pwater->model != NULL ) )
					{
						if ((pwater->curstate.maxs[2] - entity->attachment[3][2]) < 16)
						{
							vec3_t vecNull(0, 0, 0);
							vec3_t vecSrc((float *)&entity->attachment[3]);
							vecSrc.z += 25;
							int iPuff = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/wsplash_x.spr");
							TEMPENTITY *wp = gEngfuncs.pEfxAPI->R_TempSprite(vecSrc, vecNull, 0.5, iPuff, kRenderTransAdd, kRenderFxNone, 1, 5, FTENT_SPRANIMATE);
							wp->entity.curstate.framerate = 20;
							//wp->entity.curstate.rendercolor.r = entity->cvFloorColor.r;
							//wp->entity.curstate.rendercolor.g = entity->cvFloorColor.g;
							//wp->entity.curstate.rendercolor.b = entity->cvFloorColor.b;							
						}
					}
				}
			}
			break;
		}
	case 5015: // buz: right foot step (attach 2)
		{
			int contents = gEngfuncs.PM_PointContents( (float *)&entity->attachment[2], NULL );
			if (contents == CONTENTS_WATER) // leg is in the water
			{
				int waterEntity = gEngfuncs.PM_WaterEntity( (float *)&entity->attachment[2] );
				if ( waterEntity > 0 )	// water should be func_water entity
				{
					cl_entity_t *pwater = gEngfuncs.GetEntityByIndex( waterEntity );
					if ( pwater && ( pwater->model != NULL ) )
					{
						if ((pwater->curstate.maxs[2] - entity->attachment[2][2]) < 16)
						{
							vec3_t vecNull(0, 0, 0);
							vec3_t vecSrc((float *)&entity->attachment[2]);
							vecSrc.z += 25;
							int iPuff = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/wsplash_x.spr");
							TEMPENTITY *wp = gEngfuncs.pEfxAPI->R_TempSprite(vecSrc, vecNull, 0.5, iPuff, kRenderTransAdd, kRenderFxNone, 1, 5, FTENT_SPRANIMATE);
							wp->entity.curstate.framerate = 20;
							//wp->entity.curstate.rendercolor.r = entity->cvFloorColor.r;
							//wp->entity.curstate.rendercolor.g = entity->cvFloorColor.g;
							//wp->entity.curstate.rendercolor.b = entity->cvFloorColor.b;
						}
					}
				}
			}
			break;
		}
	case 5006: // buz: shell at 2nd attachment flying to 3rd
		{
			int shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/shell.mdl");
			vec3_t	VecDir = entity->attachment[2] - entity->attachment[1];
			VecDir = VecDir * 10;
			//EV_EjectBrass ( (float *)&entity->attachment[1], VecDir, 0, shell, TE_BOUNCE_SHELL );
			break;
		}
	case 5040:
		// make aurora for origin
		UTIL_CreateAurora((cl_entity_t *)entity, event->options, 0, 0.0f );
		break;
	case 5041:
		// make aurora for attachment #1
		UTIL_CreateAurora((cl_entity_t *)entity, event->options, 1, 0.0f );
		break;
	case 5042:
		// make aurora for attachment #2
		UTIL_CreateAurora((cl_entity_t *)entity, event->options, 2, 0.0f );
		break;
	case 5043:
		// make aurora for attachment #3
		UTIL_CreateAurora((cl_entity_t *)entity, event->options, 3, 0.0f );
		break;
	case 5044:
		// make aurora for attachment #4
		UTIL_CreateAurora((cl_entity_t *)entity, event->options, 4, 0.0f );
		break;
	default:
		break;
	}
}

/*
=================
CL_UpdateTEnts

Simulation and cleanup of temporary entities
=================
*/
void DLLEXPORT HUD_TempEntUpdate (
	double frametime,   // Simulation time
	double client_time, // Absolute time on client
	double cl_gravity,  // True gravity on client
	TEMPENTITY **ppTempEntFree,   // List of freed temporary ents
	TEMPENTITY **ppTempEntActive, // List 
	int (*Callback_AddVisibleEntity)( cl_entity_t *pEntity ),
	void (*Callback_TempEntPlaySound)( TEMPENTITY *pTemp, float damp ) )
{
	static int gTempEntFrame = 0;
	TEMPENTITY *pTemp, *pnext, *pprev;
	float freq, gravity, gravitySlow, life, fastFreq;

	// Nothing to simulate
	if( !*ppTempEntActive ) return;

	// in order to have tents collide with players, we have to run the player prediction code so
	// that the client has the player list. We run this code once when we detect any COLLIDEALL 
	// tent, then set this BOOL to true so the code doesn't get run again if there's more than
	// one COLLIDEALL ent for this update. (often are).
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( -1 );	

	// !!!BUGBUG -- This needs to be time based
	gTempEntFrame = (gTempEntFrame+1) & 31;

	pTemp = *ppTempEntActive;

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if( frametime <= 0 )
	{
		while( pTemp )
		{
			if( !(pTemp->flags & FTENT_NOMODEL ))
			{
				Callback_AddVisibleEntity( &pTemp->entity );
			}
			pTemp = pTemp->next;
		}
		goto finish;
	}

	pprev = NULL;
	freq = client_time * 0.01;
	fastFreq = client_time * 5.5;
	gravity = -frametime * cl_gravity;
	gravitySlow = gravity * 0.5;

	while ( pTemp )
	{
		int active = 1;

		life = pTemp->die - client_time;
		pnext = pTemp->next;

		if( life < 0 )
		{
			if ( pTemp->flags & FTENT_FADEOUT )
			{
				if (pTemp->entity.curstate.rendermode == kRenderNormal)
					pTemp->entity.curstate.rendermode = kRenderTransTexture;
				pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt * ( 1 + life * pTemp->fadeSpeed );

				if ( pTemp->entity.curstate.renderamt <= 0 )
					active = 0;

			}
			else active = 0;
		}

		if( !active ) // Kill it
		{
			pTemp->next = *ppTempEntFree;
			*ppTempEntFree = pTemp;

			if( !pprev ) // deleting at head of list
				*ppTempEntActive = pnext;
			else
				pprev->next = pnext;
		}
		else
		{
			pprev = pTemp;
			
			VectorCopy( pTemp->entity.origin, pTemp->entity.prevstate.origin );

			if ( pTemp->flags & FTENT_SPARKSHOWER )
			{
				// Adjust speed if it's time
				// Scale is next think time
				if ( client_time > pTemp->entity.baseline.scale )
				{
					// Show Sparks
					gEngfuncs.pEfxAPI->R_SparkEffect( pTemp->entity.origin, 8, -200, 200 );

					// Reduce life
					pTemp->entity.baseline.framerate -= 0.1;

					if ( pTemp->entity.baseline.framerate <= 0.0 )
					{
						pTemp->die = client_time;
					}
					else
					{
						// So it will die no matter what
						pTemp->die = client_time + 0.5;

						// Next think
						pTemp->entity.baseline.scale = client_time + 0.1;
					}
				}
			}
			else if ( pTemp->flags & FTENT_PLYRATTACHMENT )
			{
				cl_entity_t *pClient;

				pClient = gEngfuncs.GetEntityByIndex( pTemp->clientIndex );
				//VectorAdd( pClient->origin, pTemp->tentOffset, pTemp->entity.origin );
				pTemp->entity.origin = pClient->origin + pTemp->tentOffset;
			}
			else if ( pTemp->flags & FTENT_SINEWAVE )
			{
				pTemp->x += pTemp->entity.baseline.origin[0] * frametime;
				pTemp->y += pTemp->entity.baseline.origin[1] * frametime;

				pTemp->entity.origin[0] = pTemp->x + sin( pTemp->entity.baseline.origin[2] + client_time * pTemp->entity.prevstate.frame ) * (10*pTemp->entity.curstate.framerate);
				pTemp->entity.origin[1] = pTemp->y + sin( pTemp->entity.baseline.origin[2] + fastFreq + 0.7 ) * (8*pTemp->entity.curstate.framerate);
				pTemp->entity.origin[2] += pTemp->entity.baseline.origin[2] * frametime;
			}
			else if ( pTemp->flags & FTENT_SPIRAL )
			{
				float s = sin( pTemp->entity.baseline.origin[2] + fastFreq );
				float c = cos( pTemp->entity.baseline.origin[2] + fastFreq );
				float seed = static_cast<float>(reinterpret_cast<size_t>(pTemp));

				pTemp->entity.origin[0] += pTemp->entity.baseline.origin[0] * frametime + 8 * sin(client_time * 20 + seed);
				pTemp->entity.origin[1] += pTemp->entity.baseline.origin[1] * frametime + 4 * sin(client_time * 30 + seed);
				pTemp->entity.origin[2] += pTemp->entity.baseline.origin[2] * frametime;
			}
			
			else 
			{
				for ( int i = 0; i < 3; i++ ) 
					pTemp->entity.origin[i] += pTemp->entity.baseline.origin[i] * frametime;
			}
			
			if ( pTemp->flags & FTENT_SPRANIMATE )
			{
				pTemp->entity.curstate.frame += frametime * pTemp->entity.curstate.framerate;
				if ( pTemp->entity.curstate.frame >= pTemp->frameMax )
				{
					pTemp->entity.curstate.frame = pTemp->entity.curstate.frame - (int)(pTemp->entity.curstate.frame);

					if ( !(pTemp->flags & FTENT_SPRANIMATELOOP) )
					{
						// this animating sprite isn't set to loop, so destroy it.
						pTemp->die = client_time;
						pTemp = pnext;
						continue;
					}
				}
			}
			else if ( pTemp->flags & FTENT_MDLANIMATE )
			{
				pTemp->entity.curstate.body += frametime * pTemp->entity.curstate.framerate;
				if ( pTemp->entity.curstate.body >= pTemp->frameMax )
				{
					pTemp->entity.curstate.body = pTemp->frameMax;

					if( !( pTemp->flags & FTENT_MDLANIMATELOOP ))
					{
						// this animating sprite isn't set to loop, so destroy it.
						pTemp->die = client_time;
						pTemp = pnext;
						continue;
					}
				}
			}
			else if ( pTemp->flags & FTENT_SPRCYCLE )
			{
				pTemp->entity.curstate.frame += frametime * 10;
				if ( pTemp->entity.curstate.frame >= pTemp->frameMax )
				{
					pTemp->entity.curstate.frame = pTemp->entity.curstate.frame - (int)(pTemp->entity.curstate.frame);
				}
			}
// Experiment
#if 0
			if ( pTemp->flags & FTENT_SCALE )
				pTemp->entity.curstate.framerate += 20.0 * (frametime / pTemp->entity.curstate.framerate);
#endif

			if ( pTemp->flags & FTENT_ROTATE )
			{
				pTemp->entity.angles[0] += pTemp->entity.baseline.angles[0] * frametime;
				pTemp->entity.angles[1] += pTemp->entity.baseline.angles[1] * frametime;
				pTemp->entity.angles[2] += pTemp->entity.baseline.angles[2] * frametime;

				VectorCopy( pTemp->entity.angles, pTemp->entity.latched.prevangles );
			}

			if ( pTemp->flags & (FTENT_COLLIDEALL | FTENT_COLLIDEWORLD) )
			{
				vec3_t	traceNormal;
				float	traceFraction = 1;

				if ( pTemp->flags & FTENT_COLLIDEALL )
				{
					pmtrace_t pmtrace;
					physent_t *pe;
				
					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );

					gEngfuncs.pEventAPI->EV_PlayerTrace( pTemp->entity.prevstate.origin, pTemp->entity.origin, PM_STUDIO_BOX, -1, &pmtrace );


					if ( pmtrace.fraction != 1 )
					{
						pe = gEngfuncs.pEventAPI->EV_GetPhysent( pmtrace.ent );

						if ( !pmtrace.ent || ( pe->info != pTemp->clientIndex ) )
						{
							traceFraction = pmtrace.fraction;
							VectorCopy( pmtrace.plane.normal, traceNormal );

							if ( pTemp->hitcallback )
							{
								(*pTemp->hitcallback)( pTemp, &pmtrace );
							}
						}
					}
				}
				else if ( pTemp->flags & FTENT_COLLIDEWORLD )
				{
					pmtrace_t pmtrace;
					
					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );

					gEngfuncs.pEventAPI->EV_PlayerTrace( pTemp->entity.prevstate.origin, pTemp->entity.origin, PM_STUDIO_BOX | PM_WORLD_ONLY, -1, &pmtrace );					

					if ( pmtrace.fraction != 1 )
					{
						traceFraction = pmtrace.fraction;
						VectorCopy( pmtrace.plane.normal, traceNormal );

						if ( pTemp->flags & FTENT_SPARKSHOWER )
						{
							// Chop spark speeds a bit more
							//
							//VectorScale( pTemp->entity.baseline.origin, 0.6, pTemp->entity.baseline.origin );
							pTemp->entity.baseline.origin = pTemp->entity.baseline.origin * 0.6f;
							if (pTemp->entity.baseline.origin.Length() < 10 )
							{
								pTemp->entity.baseline.framerate = 0.0;								
							}
						}

						if ( pTemp->hitcallback )
						{
							(*pTemp->hitcallback)( pTemp, &pmtrace );
						}
					}
				}
				
				if ( traceFraction != 1 )	// Decent collision now, and damping works
				{
					float  proj, damp;

					// Place at contact point
					VectorMA( pTemp->entity.prevstate.origin, traceFraction*frametime, pTemp->entity.baseline.origin, pTemp->entity.origin );
					// Damp velocity
					damp = pTemp->bounceFactor;
					if ( pTemp->flags & (FTENT_GRAVITY|FTENT_SLOWGRAVITY) )
					{
						damp *= 0.5;
						if ( traceNormal[2] > 0.9 )		// Hit floor?
						{
							if ( pTemp->entity.baseline.origin[2] <= 0 && pTemp->entity.baseline.origin[2] >= gravity*3 )
							{
								damp = 0;		// Stop
								pTemp->flags &= ~(FTENT_ROTATE|FTENT_GRAVITY|FTENT_SLOWGRAVITY|FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL);
								pTemp->entity.angles[0] = 0;
								pTemp->entity.angles[2] = 0;
							}
						}
					}

					if (pTemp->hitSound)
					{
						Callback_TempEntPlaySound(pTemp, damp);
					}

					if (pTemp->flags & FTENT_COLLIDEKILL)
					{
						// die on impact
						pTemp->flags &= ~FTENT_FADEOUT;	
						pTemp->die = client_time;			
					}
					else
					{
						// Reflect velocity
						if ( damp != 0 )
						{
							proj = DotProduct( pTemp->entity.baseline.origin, traceNormal );
							VectorMA( pTemp->entity.baseline.origin, -proj*2, traceNormal, pTemp->entity.baseline.origin );
							// Reflect rotation (fake)
							pTemp->entity.angles[1] = -pTemp->entity.angles[1];
						}
						
						if ( damp != 1 )
						{
							//VectorScale( pTemp->entity.baseline.origin, damp, pTemp->entity.baseline.origin );
							//VectorScale( pTemp->entity.angles, 0.9, pTemp->entity.angles );
							pTemp->entity.baseline.origin = pTemp->entity.baseline.origin * damp;
							pTemp->entity.angles = pTemp->entity.angles * 0.9f;
						}
					}
				}
			}


			if ( (pTemp->flags & FTENT_FLICKER) && gTempEntFrame == pTemp->entity.curstate.effects )
			{
				dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight (0);
				//VectorCopy (pTemp->entity.origin, dl->origin);
				dl->origin = pTemp->entity.origin;
				dl->radius = 60;
				dl->color.r = 255;
				dl->color.g = 120;
				dl->color.b = 0;
				dl->die = client_time + 0.01;
			}

			if ( pTemp->flags & FTENT_SMOKETRAIL )
			{
				gEngfuncs.pEfxAPI->R_RocketTrail (pTemp->entity.prevstate.origin, pTemp->entity.origin, 1);
			}

			if ( pTemp->flags & FTENT_GRAVITY )
				pTemp->entity.baseline.origin[2] += gravity;
			else if ( pTemp->flags & FTENT_SLOWGRAVITY )
				pTemp->entity.baseline.origin[2] += gravitySlow;

			if ( pTemp->flags & FTENT_CLIENTCUSTOM )
			{
				if ( pTemp->callback )
				{
					( *pTemp->callback )( pTemp, frametime, client_time );
				}
			}

			// Cull to PVS (not frustum cull, just PVS)
			if ( !(pTemp->flags & FTENT_NOMODEL ) )
			{
				if( g_fRenderInitialized )
				{
					Callback_AddVisibleEntity( &pTemp->entity );
				}
				else
				{
					if ( !Callback_AddVisibleEntity( &pTemp->entity ) )
					{
						if ( !(pTemp->flags & FTENT_PERSIST) ) 
						{
							pTemp->die = client_time;		// If we can't draw it this frame, just dump it.
							pTemp->flags &= ~FTENT_FADEOUT;	// Don't fade out, just die
						}
					}
				}
			}
		}
		pTemp = pnext;
	}

finish:
	// Restore state info
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

/*
=================
HUD_GetUserEntity

If you specify negative numbers for beam start and end point entities, then
  the engine will call back into this function requesting a pointer to a cl_entity_t 
  object that describes the entity to attach the beam onto.

Indices must start at 1, not zero.
=================
*/
cl_entity_t DLLEXPORT *HUD_GetUserEntity( int index )
{
	return nullptr;
}
