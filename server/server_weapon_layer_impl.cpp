/*
server_weapon_layer_impl.cpp - part of server-side weapons predicting implementation
Copyright (C) 2025 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "server_weapon_layer_impl.h"
#include "gamerules.h"
#include "game.h"

CServerWeaponLayerImpl::CServerWeaponLayerImpl(CBasePlayerWeapon *weaponEntity) :
	m_pWeapon(weaponEntity)
{
}

int CServerWeaponLayerImpl::GetWeaponBodygroup()
{
	return m_pWeapon->pev->body;
}

void CServerWeaponLayerImpl::SetWeaponBodygroup(int value)
{
	m_pWeapon->pev->body = value;
}

Vector CServerWeaponLayerImpl::GetGunPosition()
{
	return m_pWeapon->m_pPlayer->GetAbsOrigin() + m_pWeapon->m_pPlayer->pev->view_ofs;
}

matrix3x3 CServerWeaponLayerImpl::GetCameraOrientation()
{
	CBasePlayer *player = m_pWeapon->m_pPlayer;
	return matrix3x3(player->pev->v_angle + player->pev->punchangle);
}

Vector CServerWeaponLayerImpl::GetViewAngles()
{
	return m_pWeapon->m_pPlayer->pev->v_angle;
}

Vector CServerWeaponLayerImpl::GetAutoaimVector(float delta)
{
	CBasePlayer *player = m_pWeapon->m_pPlayer;

	if (g_iSkillLevel == SKILL_HARD)
	{
		UTIL_MakeVectors( player->pev->v_angle + player->pev->punchangle );
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition( );
	float flDist = 8192;

	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (1 || g_iSkillLevel == SKILL_MEDIUM)
	{
		player->m_vecAutoAim = Vector( 0, 0, 0 );
		// flDelta *= 0.5;
	}

	BOOL m_fOldTargeting = player->m_fOnTarget;
	Vector angles = player->AutoaimDeflection(vecSrc, flDist, delta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		player->m_fOnTarget = 0;
	else if (m_fOldTargeting != player->m_fOnTarget)
	{
		player->m_pActiveItem->UpdateItemInfo( );
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;


	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (0 || g_iSkillLevel == SKILL_EASY)
	{
		player->m_vecAutoAim = player->m_vecAutoAim * 0.67 + angles * 0.33;
	}
	else
	{
		player->m_vecAutoAim = angles * 0.9;
	}

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	if ( g_psv_aim->value != 0 )
	{
		if ( player->m_vecAutoAim.x != player->m_lastx ||
			 player->m_vecAutoAim.y != player->m_lasty )
		{
			SET_CROSSHAIRANGLE( player->edict(), -player->m_vecAutoAim.x, player->m_vecAutoAim.y );
			
			player->m_lastx = player->m_vecAutoAim.x;
			player->m_lasty = player->m_vecAutoAim.y;
		}
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors( player->pev->v_angle + player->pev->punchangle + player->m_vecAutoAim );
	return gpGlobals->v_forward;
}

Vector CServerWeaponLayerImpl::FireBullets(int bullets, Vector origin, matrix3x3 orientation, float distance, float spread, int bulletType, uint32_t seed, int damage)
{
	float x, y, z;
	TraceResult tr;
	CBasePlayer *player = m_pWeapon->m_pPlayer;

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	for (uint32_t i = 1; i <= bullets; i++)
	{
		// use player's random seed, get circular gaussian spread
		x = m_randomGenerator.GetFloat(seed + i, -0.5f, 0.5f) + m_randomGenerator.GetFloat(seed + 1 + i, -0.5f, 0.5f);
		y = m_randomGenerator.GetFloat(seed + 2 + i, -0.5f, 0.5f) + m_randomGenerator.GetFloat(seed + 3 + i, -0.5f, 0.5f);
		z = x * x + y * y;

		Vector vecDir = orientation.GetForward() +
						x * spread * orientation.GetRight() +
						y * spread * orientation.GetUp();
		Vector vecEnd = origin + vecDir * distance;

		SetBits(gpGlobals->trace_flags, FTRACE_MATERIAL_TRACE);
		UTIL_TraceLine(origin, vecEnd, dont_ignore_monsters, ENT(player->pev), &tr);
		ClearBits(gpGlobals->trace_flags, FTRACE_MATERIAL_TRACE);

		// do damage, paint decals
		if (tr.flFraction != 1.0)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if ( damage )
			{
				pEntity->TraceAttack(player->pev, damage, vecDir, &tr, DMG_BULLET | ((damage > 16) ? DMG_ALWAYSGIB : DMG_NEVERGIB) );
				
				TEXTURETYPE_PlaySound(&tr, origin, vecEnd, bulletType);
				DecalGunshot( &tr, bulletType, origin, vecEnd );
			} 
			else
			{
				switch (bulletType)
				{
					default:
					case BULLET_PLAYER_9MM:
						pEntity->TraceAttack(player->pev, gSkillData.plrDmg9MM, vecDir, &tr, DMG_BULLET);
						break;

					case BULLET_PLAYER_MP5:
						pEntity->TraceAttack(player->pev, gSkillData.plrDmgMP5, vecDir, &tr, DMG_BULLET);
						break;

					case BULLET_PLAYER_BUCKSHOT:
						// make distance based!
						pEntity->TraceAttack(player->pev, gSkillData.plrDmgBuckshot, vecDir, &tr, DMG_BULLET);
						break;

					case BULLET_PLAYER_357:
						pEntity->TraceAttack(player->pev, gSkillData.plrDmg357, vecDir, &tr, DMG_BULLET);
						break;

					case BULLET_NONE: // FIX 
						pEntity->TraceAttack(player->pev, 50, vecDir, &tr, DMG_CLUB);
						break;
				}

				TEXTURETYPE_PlaySound(&tr, origin, vecEnd, bulletType);
				DecalGunshot(&tr, bulletType, origin, vecEnd);
			}
		}
		// make bullet trails
		UTIL_BubbleTrail( origin, tr.vecEndPos, (distance * tr.flFraction) / 64.0 );
	}

	ApplyMultiDamage(player->pev, player->pev);
	return Vector( x * spread, y * spread, 0.0 );
}

int CServerWeaponLayerImpl::GetPlayerAmmo(int ammoType)
{
	return m_pWeapon->m_pPlayer->m_rgAmmo[ammoType];
}

void CServerWeaponLayerImpl::SetPlayerAmmo(int ammoType, int count)
{
	m_pWeapon->m_pPlayer->m_rgAmmo[ammoType] = count;
}

void CServerWeaponLayerImpl::SetPlayerWeaponAnim(int anim)
{
	m_pWeapon->m_pPlayer->pev->weaponanim = anim;
}

void CServerWeaponLayerImpl::SetPlayerViewmodel(std::string_view model)
{
	m_pWeapon->m_pPlayer->pev->viewmodel = MAKE_STRING(model.data());
}

void CServerWeaponLayerImpl::DisablePlayerViewmodel()
{
	m_pWeapon->m_pPlayer->pev->viewmodel = iStringNull;
}

int CServerWeaponLayerImpl::GetPlayerViewmodel()
{
	return m_pWeapon->m_pPlayer->pev->viewmodel;
}

int CServerWeaponLayerImpl::GetPlayerWaterlevel()
{
	return m_pWeapon->m_pPlayer->pev->waterlevel;
}

bool CServerWeaponLayerImpl::CheckPlayerButtonFlag(int buttonMask)
{
	return FBitSet(m_pWeapon->m_pPlayer->pev->button, buttonMask);
}

void CServerWeaponLayerImpl::ClearPlayerButtonFlag(int buttonMask)
{
	ClearBits(m_pWeapon->m_pPlayer->pev->button, buttonMask);
}

float CServerWeaponLayerImpl::GetPlayerNextAttackTime()
{
	return m_pWeapon->m_pPlayer->m_flNextAttack; 
}

void CServerWeaponLayerImpl::SetPlayerNextAttackTime(float value)
{
	m_pWeapon->m_pPlayer->m_flNextAttack = value;
}

void CServerWeaponLayerImpl::SetPlayerFOV(float value)
{
	m_pWeapon->m_pPlayer->pev->fov = value;
	m_pWeapon->m_pPlayer->m_iFOV = value;
}

float CServerWeaponLayerImpl::GetPlayerFOV()
{
	return m_pWeapon->m_pPlayer->pev->fov;
}

Vector CServerWeaponLayerImpl::GetPlayerVelocity()
{
	return m_pWeapon->m_pPlayer->GetAbsVelocity();
}

void CServerWeaponLayerImpl::SetPlayerVelocity(Vector value)
{
	m_pWeapon->m_pPlayer->SetAbsVelocity(value);
}

float CServerWeaponLayerImpl::GetWeaponTimeBase(bool usePredicting)
{
	return usePredicting ? 0.0f : gpGlobals->time;
}

float CServerWeaponLayerImpl::GetTime()
{
	return gpGlobals->time;
}

uint32_t CServerWeaponLayerImpl::GetRandomSeed()
{
	return m_pWeapon->m_pPlayer->random_seed;
}

uint32_t CServerWeaponLayerImpl::GetRandomInt(uint32_t seed, int32_t min, int32_t max)
{
	return m_randomGenerator.GetInteger(seed, min, max);
}

float CServerWeaponLayerImpl::GetRandomFloat(uint32_t seed, float min, float max)
{
	return m_randomGenerator.GetFloat(seed, min, max);
}

uint16_t CServerWeaponLayerImpl::PrecacheEvent(const char *eventName)
{
	return g_engfuncs.pfnPrecacheEvent(1, eventName);
}

void CServerWeaponLayerImpl::PlaybackWeaponEvent(const WeaponEventParams &params)
{
	// this weird division-multiplying by 3 is somehow relatable to stupid quake bug
	// TODO maybe create new features flag in engine to get rid of this hack entirely?
	Vector anglesFixed = params.angles;
	anglesFixed[PITCH] /= 3.f;
	g_engfuncs.pfnPlaybackEvent(static_cast<int>(params.flags), m_pWeapon->m_pPlayer->edict(),
		params.eventindex, params.delay, 
		params.origin, anglesFixed, 
		params.fparam1, params.fparam2, 
		params.iparam1, params.iparam2, 
		params.bparam1, params.bparam2);
}

bool CServerWeaponLayerImpl::ShouldRunFuncs()
{
	return true; // always true, because server do not any kind of subticking for weapons
}

bool CServerWeaponLayerImpl::IsMultiplayer()
{
	// in case gamerules not available at the moment, likely this is singleplayer
	// and we're loading from save-file. therefore return false as default value.
	return g_pGameRules ? g_pGameRules->IsMultiplayer() : false;
}
