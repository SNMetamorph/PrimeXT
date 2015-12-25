/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// r_tempents.cpp
//
// Client side entity management functions
//

#include "hud.h"
#include "utils.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "r_local.h"

/*
=================
CL_UpdateTEnts

Simulation and cleanup of temporary entities
=================
*/
void HUD_TempEntUpdate(
	double frametime,   // Simulation time
	double client_time, // Absolute time on client
	double cl_gravity,  // True gravity on client
	TEMPENTITY **ppTempEntFree,   // List of freed temporary ents
	TEMPENTITY **ppTempEntActive, // List 
	int  (*Callback_AddVisibleEntity)( cl_entity_t *pEntity ),
	void (*Callback_TempEntPlaySound)( TEMPENTITY *pTemp, float damp ))
{
	static int gTempEntFrame = 0;
	TEMPENTITY *pTemp, *pnext, *pprev;
	float freq, gravity, gravitySlow, life, fastFreq;

	// nothing to simulate
	if( !*ppTempEntActive ) return;

	// in order to have tents collide with players, we have to run the player prediction code so
	// that the client has the player list. We run this code once when we detect any COLLIDEALL 
	// tent, then set this BOOL to true so the code doesn't get run again if there's more than
	// one COLLIDEALL ent for this update. (often are).
	gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers( -1 );	

	// !!!BUGBUG -- This needs to be time based
	gTempEntFrame = (gTempEntFrame+1) & 31;

	pTemp = *ppTempEntActive;

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if( frametime <= 0 )
	{
		while( pTemp )
		{
			if(!( pTemp->flags & FTENT_NOMODEL ))
			{
				Callback_AddVisibleEntity( &pTemp->entity );
				r_stats.c_active_tents_count++;
			}
			pTemp = pTemp->next;
		}
		goto finish;
	}

	pprev = NULL;
	freq = client_time * 0.01f;
	fastFreq = client_time * 5.5f;
	gravity = -frametime * cl_gravity;
	gravitySlow = gravity * 0.5f;

	while( pTemp )
	{
		int active = 1;

		life = pTemp->die - client_time;
		pnext = pTemp->next;

		if( life < 0 )
		{
			if( pTemp->flags & FTENT_FADEOUT )
			{
				if( pTemp->entity.curstate.rendermode == kRenderNormal )
					pTemp->entity.curstate.rendermode = kRenderTransTexture;
				pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt * ( 1.0f + life * pTemp->fadeSpeed );

				if( pTemp->entity.curstate.renderamt <= 0 )
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
			
			pTemp->entity.prevstate.origin = pTemp->entity.origin;

			if( pTemp->flags & FTENT_SPARKSHOWER )
			{
				// Adjust speed if it's time
				// Scale is next think time
				if( client_time > pTemp->entity.baseline.scale )
				{
					// show Sparks
					gEngfuncs.pEfxAPI->R_SparkEffect( pTemp->entity.origin, 8, -200, 200 );

					// reduce life
					pTemp->entity.baseline.framerate -= 0.1f;

					if( pTemp->entity.baseline.framerate <= 0.0f )
					{
						pTemp->die = client_time;
					}
					else
					{
						// so it will die no matter what
						pTemp->die = client_time + 0.5;

						// next think
						pTemp->entity.baseline.scale = client_time + 0.1f;
					}
				}
			}
			else if( pTemp->flags & FTENT_PLYRATTACHMENT )
			{
				cl_entity_t *pClient;

				pClient = gEngfuncs.GetEntityByIndex( pTemp->clientIndex );
				pTemp->entity.origin = pClient->origin + pTemp->tentOffset;
			}
			else if( pTemp->flags & FTENT_SINEWAVE )
			{
				pTemp->x += pTemp->entity.baseline.origin[0] * frametime;
				pTemp->y += pTemp->entity.baseline.origin[1] * frametime;

				pTemp->entity.origin[0] = pTemp->x + sin( pTemp->entity.baseline.origin[2] + client_time * pTemp->entity.prevstate.frame ) * (10 * pTemp->entity.curstate.framerate);
				pTemp->entity.origin[1] = pTemp->y + sin( pTemp->entity.baseline.origin[2] + fastFreq + 0.7f ) * (8 * pTemp->entity.curstate.framerate);
				pTemp->entity.origin[2] += pTemp->entity.baseline.origin[2] * frametime;
			}
			else if( pTemp->flags & FTENT_SPIRAL )
			{
				float s, c;
				SinCos( pTemp->entity.baseline.origin[2] + fastFreq, &s, &c );

				pTemp->entity.origin[0] += pTemp->entity.baseline.origin[0] * frametime + 8 * sin( client_time * 20 + (int)pTemp );
				pTemp->entity.origin[1] += pTemp->entity.baseline.origin[1] * frametime + 4 * sin( client_time * 30 + (int)pTemp );
				pTemp->entity.origin[2] += pTemp->entity.baseline.origin[2] * frametime;
			}
			
			else 
			{
				pTemp->entity.origin += pTemp->entity.baseline.origin * frametime;
			}
			
			if( pTemp->flags & FTENT_SPRANIMATE )
			{
				pTemp->entity.curstate.frame += frametime * pTemp->entity.curstate.framerate;

				if( pTemp->entity.curstate.frame >= pTemp->frameMax )
				{
					pTemp->entity.curstate.frame = pTemp->entity.curstate.frame - (int)(pTemp->entity.curstate.frame);

					if(!( pTemp->flags & FTENT_SPRANIMATELOOP ))
					{
						// this animating sprite isn't set to loop, so destroy it.
						pTemp->die = client_time;
						pTemp = pnext;
						continue;
					}
				}
			}
			else if( pTemp->flags & FTENT_SPRCYCLE )
			{
				pTemp->entity.curstate.frame += frametime * 10;

				if( pTemp->entity.curstate.frame >= pTemp->frameMax )
				{
					pTemp->entity.curstate.frame = pTemp->entity.curstate.frame - (int)(pTemp->entity.curstate.frame);
				}
			}

			if( pTemp->flags & FTENT_SCALE )
				pTemp->entity.curstate.framerate += 20.0 * (frametime / pTemp->entity.curstate.framerate);

			if( pTemp->flags & FTENT_ROTATE )
			{
				pTemp->entity.angles += pTemp->entity.baseline.angles * frametime;
				pTemp->entity.latched.prevangles = pTemp->entity.angles;
			}

			if( pTemp->flags & ( FTENT_COLLIDEALL|FTENT_COLLIDEWORLD ))
			{
				Vector	traceNormal;
				float	traceFraction = 1;

				if( pTemp->flags & FTENT_COLLIDEALL )
				{
					pmtrace_t pmtrace;
					physent_t *pe;
				
					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( pTemp->entity.prevstate.origin, pTemp->entity.origin, PM_STUDIO_BOX, -1, &pmtrace );

					if( pmtrace.fraction != 1.0f )
					{
						pe = gEngfuncs.pEventAPI->EV_GetPhysent( pmtrace.ent );

						if( !pmtrace.ent || ( pe->info != pTemp->clientIndex ))
						{
							traceFraction = pmtrace.fraction;
							traceNormal = pmtrace.plane.normal;

							if( pTemp->hitcallback )
							{
								(*pTemp->hitcallback)( pTemp, &pmtrace );
							}
						}
					}
				}
				else if( pTemp->flags & FTENT_COLLIDEWORLD )
				{
					pmtrace_t pmtrace;
					
					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( pTemp->entity.prevstate.origin, pTemp->entity.origin, PM_STUDIO_BOX | PM_WORLD_ONLY, -1, &pmtrace );

					if( pmtrace.fraction != 1.0f )
					{
						traceFraction = pmtrace.fraction;
						traceNormal = pmtrace.plane.normal;

						if( pTemp->flags & FTENT_SPARKSHOWER )
						{
							// Chop spark speeds a bit more
							pTemp->entity.baseline.origin *= 0.6f;

							if( pTemp->entity.baseline.origin.Length() < 10.0f )
							{
								pTemp->entity.baseline.framerate = 0.0f;
							}
						}

						if( pTemp->hitcallback )
						{
							(*pTemp->hitcallback)( pTemp, &pmtrace );
						}
					}
				}

				// decent collision now, and damping works
				if( traceFraction != 1 )
				{
					float  proj, damp;

					// place at contact point
					pTemp->entity.origin = pTemp->entity.prevstate.origin + pTemp->entity.baseline.origin * (traceFraction * frametime );

					// damp velocity
					damp = pTemp->bounceFactor;

					if( pTemp->flags & ( FTENT_GRAVITY|FTENT_SLOWGRAVITY ))
					{
						damp *= 0.5f;

						if( traceNormal.z > 0.9f ) // hit floor?
						{
							if( pTemp->entity.baseline.origin[2] <= 0 && pTemp->entity.baseline.origin[2] >= gravity * 3 )
							{
								damp = 0; // stop
								pTemp->flags &= ~(FTENT_ROTATE|FTENT_GRAVITY|FTENT_SLOWGRAVITY|FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL);
								pTemp->entity.angles.x = 0;
								pTemp->entity.angles.z = 0;
							}
						}
					}

					if( pTemp->hitSound )
					{
						Callback_TempEntPlaySound( pTemp, damp );
					}

					if( pTemp->flags & FTENT_COLLIDEKILL )
					{
						// die on impact
						pTemp->flags &= ~FTENT_FADEOUT;	
						pTemp->die = client_time;			
					}
					else
					{
						// reflect velocity
						if ( damp != 0 )
						{
							proj = DotProduct( pTemp->entity.baseline.origin, traceNormal );
							pTemp->entity.baseline.origin += traceNormal * (-proj * 2);
	         
							// reflect rotation (fake)
                   						pTemp->entity.angles.y = -pTemp->entity.angles.y;
						}
						
						if( damp != 1.0f )
						{
							pTemp->entity.baseline.origin *= damp;
							pTemp->entity.angles *= 0.9;
						}
					}
				}
			}


			if(( pTemp->flags & FTENT_FLICKER ) && gTempEntFrame == pTemp->entity.curstate.effects )
			{
				dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( 0 );
				dl->origin = pTemp->entity.origin;
				dl->radius = 60;
				dl->color.r = 255;
				dl->color.g = 120;
				dl->color.b = 0;
				dl->die = client_time + 0.01f;
			}

			if( pTemp->flags & FTENT_SMOKETRAIL )
			{
				gEngfuncs.pEfxAPI->R_RocketTrail( pTemp->entity.prevstate.origin, pTemp->entity.origin, 1 );
			}

			if( pTemp->flags & FTENT_GRAVITY )
				pTemp->entity.baseline.origin.z += gravity;
			else if( pTemp->flags & FTENT_SLOWGRAVITY )
				pTemp->entity.baseline.origin.z += gravitySlow;

			if( pTemp->flags & FTENT_CLIENTCUSTOM )
			{
				if( pTemp->callback )
				{
					(*pTemp->callback)( pTemp, frametime, client_time );
				}
			}

			// cull to PVS (not frustum cull, just PVS)
			if(!( pTemp->flags & FTENT_NOMODEL ))
			{
				if( g_fRenderInitialized )
				{
					Callback_AddVisibleEntity( &pTemp->entity );
					r_stats.c_active_tents_count++;
				}
				else
				{
					if( !Callback_AddVisibleEntity( &pTemp->entity ))
					{
						if(!( pTemp->flags & FTENT_PERSIST )) 
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
