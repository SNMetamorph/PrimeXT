/*
weapon_predicting_context.cpp - part of client-side weapons predicting implementation
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

#include "weapon_predicting_context.h"
#include "client_weapon_layer_impl.h"
#include "hud.h"
#include "utils.h"
#include "weapons/glock.h"
#include "weapons/crossbow.h"
#include "weapons/python.h"
#include "weapons/mp5.h"
#include "weapons/shotgun.h"
#include <cstring>

CWeaponPredictingContext::CWeaponPredictingContext()
{
}

void CWeaponPredictingContext::PostThink(local_state_t *from, local_state_t *to, usercmd_t *cmd, bool runfuncs, double time, uint32_t randomSeed)
{
	m_playerState.time = time;
	m_playerState.randomSeed = randomSeed;
	m_playerState.runfuncs = runfuncs;
	ReadPlayerState(from, cmd);
	ReadWeaponsState(from);

	const bool playerAlive = m_playerState.deadflag != (DEAD_DISCARDBODY + 1);
	CBaseWeaponContext *currWeapon = GetWeaponContext(from->client.m_iId);
	if (currWeapon)
	{
		if (runfuncs) {
			HandlePlayerSpawnDeath(to, currWeapon);
		}

		if (playerAlive && m_playerState.viewmodel)
		{
			if (m_playerState.nextAttack <= 0.0f) {
				currWeapon->ItemPostFrame();
			}
		}

		if (playerAlive && cmd->weaponselect)
		{
			// handle weapon switching on weapons code side
			HandleWeaponSwitch(from, to, cmd, currWeapon);
		}
		else
		{
			// does not changing weapons now, keep weapon ID same
			to->client.m_iId = from->client.m_iId;
		}

		// check for desync between local & server-side weapon animation
		if (runfuncs && (m_playerState.activeWeaponanim != m_playerState.weaponanim))
		{
			gEngfuncs.pfnWeaponAnim( m_playerState.weaponanim, 0 );
			m_playerState.activeWeaponanim = m_playerState.weaponanim;
		}
	}

	UpdatePlayerTimers(cmd);
	WritePlayerState(to);
	WriteWeaponsState(to, cmd);
}

void CWeaponPredictingContext::ReadPlayerState(const local_state_t *from, usercmd_t *cmd)
{
	m_playerState.cached.buttons = from->playerstate.oldbuttons;
	int32_t buttonsChanged = (m_playerState.cached.buttons ^ cmd->buttons);
	m_playerState.buttonsPressed = buttonsChanged & cmd->buttons;	
	m_playerState.buttonsReleased = buttonsChanged & (~cmd->buttons);

	m_playerState.viewAngles = cmd->viewangles;
	m_playerState.viewOffset = from->client.view_ofs;
	m_playerState.origin = from->playerstate.origin;

	m_playerState.buttons = cmd->buttons;
	m_playerState.velocity = from->client.velocity;
	m_playerState.flags = from->client.flags;

	m_playerState.deadflag = from->client.deadflag;
	m_playerState.waterlevel = from->client.waterlevel;
	m_playerState.maxSpeed = from->client.maxspeed;

	m_playerState.fov = from->client.fov;
	m_playerState.weaponanim = from->client.weaponanim;
	m_playerState.viewmodel = from->client.viewmodel;
	m_playerState.nextAttack = from->client.m_flNextAttack;
	//m_playerState.m_flNextAmmoBurn = from->client.fuser2;
	//m_playerState.m_flAmmoStartCharge = from->client.fuser3;
}

void CWeaponPredictingContext::WritePlayerState(local_state_t *to)
{
	// here we need to write back values that potentially could be modified from weapons code
	to->client.viewmodel				= m_playerState.viewmodel;
	to->client.fov						= m_playerState.fov;
	to->client.weaponanim				= m_playerState.weaponanim;
	to->client.m_flNextAttack			= m_playerState.nextAttack;
	//to->client.fuser2					= m_playerState.m_flNextAmmoBurn;
	//to->client.fuser3					= m_playerState.m_flAmmoStartCharge;
	to->client.maxspeed					= m_playerState.maxSpeed;
}

void CWeaponPredictingContext::UpdatePlayerTimers(const usercmd_t *cmd)
{
	m_playerState.nextAttack -= cmd->msec / 1000.0;
	if (m_playerState.nextAttack < -0.001)
		m_playerState.nextAttack = -0.001;

	//m_playerState.fuser2 -= cmd->msec / 1000.0;
	//if (m_playerState.fuser2 < -0.001)
	//{
	//	m_playerState.fuser2 = -0.001;
	//}

	//m_playerState.fuser3 -= cmd->msec / 1000.0;
	//if (m_playerState.fuser3 < -0.001)
	//{
	//	m_playerState.fuser3 = -0.001;
	//}
}

void CWeaponPredictingContext::UpdateWeaponTimers(CBaseWeaponContext *weapon, const usercmd_t *cmd)
{
	weapon->m_flNextPrimaryAttack		-= cmd->msec / 1000.0;
	weapon->m_flNextSecondaryAttack		-= cmd->msec / 1000.0;
	weapon->m_flTimeWeaponIdle			-= cmd->msec / 1000.0;
	//weapon->m_flNextReload			-= cmd->msec / 1000.0;
	//weapon->m_fNextAimBonus			-= cmd->msec / 1000.0;
	//weapon->fuser1					-= cmd->msec / 1000.0;

	if (weapon->m_flNextPrimaryAttack < -1.0)
		weapon->m_flNextPrimaryAttack = -1.0;

	if (weapon->m_flNextSecondaryAttack < -0.001)
		weapon->m_flNextSecondaryAttack = -0.001;

	if (weapon->m_flTimeWeaponIdle < -0.001)
		weapon->m_flTimeWeaponIdle = -0.001;

	//if (weapon->m_fNextAimBonus < -1.0)
	//	weapon->m_fNextAimBonus = -1.0;

	//if (weapon->m_flNextReload < -0.001)
	//	weapon->m_flNextReload = -0.001;

	//if (weapon->fuser1 < -0.001)
	//	weapon->fuser1 = -0.001;
}

void CWeaponPredictingContext::ReadWeaponsState(const local_state_t *from)
{
	for (size_t i = 0; i < MAX_LOCAL_WEAPONS; i++)
	{
		CBaseWeaponContext *weapon = GetWeaponContext(i);
		const weapon_data_t &data = from->weapondata[i];
		if (weapon)
		{
			weapon->m_fInReload				= data.m_fInReload;
			weapon->m_fInSpecialReload		= data.m_fInSpecialReload;
			//weapon->m_flPumpTime			= data.m_flPumpTime;
			weapon->m_iClip					= data.m_iClip;
			weapon->m_flNextPrimaryAttack	= data.m_flNextPrimaryAttack;
			weapon->m_flNextSecondaryAttack	= data.m_flNextSecondaryAttack;
			weapon->m_flTimeWeaponIdle		= data.m_flTimeWeaponIdle;
			//weapon->pev->fuser1			= data.fuser1;
			//weapon->m_flStartThrow		= data.fuser2;
			//weapon->m_flReleaseThrow		= data.fuser3;
			//weapon->m_chargeReady			= data.iuser1;
			//weapon->m_fInAttack			= data.iuser2;
			//weapon->m_fireState			= data.iuser3;

			weapon->m_iSecondaryAmmoType = static_cast<int>(from->client.vuser3.z);
			weapon->m_iPrimaryAmmoType = static_cast<int>(from->client.vuser4.x);
			m_playerState.ammo[weapon->m_iPrimaryAmmoType] = static_cast<int>(from->client.vuser4.y);
			m_playerState.ammo[weapon->m_iSecondaryAmmoType] = static_cast<int>(from->client.vuser4.z);
		}
	}
}

void CWeaponPredictingContext::WriteWeaponsState(local_state_t *to, const usercmd_t *cmd)
{
	for (size_t i = 0; i < MAX_LOCAL_WEAPONS; i++)
	{
		weapon_data_t &data = to->weapondata[i];
		CBaseWeaponContext *weapon = GetWeaponContext(i);
		if (!weapon)
		{
			std::memset(&data, 0x0, sizeof(weapon_data_t));
			continue;
		}
		else
		{
			UpdateWeaponTimers(weapon, cmd);
			data.m_fInReload				= weapon->m_fInReload;
			data.m_fInSpecialReload			= weapon->m_fInSpecialReload;
			//data.m_flPumpTime				= pCurrent->m_flPumpTime;
			data.m_iClip					= weapon->m_iClip; 
			data.m_flNextPrimaryAttack		= weapon->m_flNextPrimaryAttack;
			data.m_flNextSecondaryAttack	= weapon->m_flNextSecondaryAttack;
			data.m_flTimeWeaponIdle			= weapon->m_flTimeWeaponIdle;
			//data.fuser1					= pCurrent->pev->fuser1;
			//data.fuser2					= weapon->m_flStartThrow;
			//data.fuser3					= weapon->m_flReleaseThrow;
			//data.iuser1					= weapon->m_chargeReady;
			//data.iuser2					= weapon->m_fInAttack;
			//data.iuser3					= weapon->m_fireState;
		}
	}
}

void CWeaponPredictingContext::HandlePlayerSpawnDeath(local_state_t *to, CBaseWeaponContext *weapon)
{
	if (to->client.health <= 0 && m_playerState.cached.health > 0) {
		weapon->Holster();
	}
	else if (to->client.health > 0 && m_playerState.cached.health <= 0) {
		weapon->Deploy();
	}
	m_playerState.cached.health = to->client.health;
}

void CWeaponPredictingContext::HandleWeaponSwitch(const local_state_t *from, local_state_t *to, const usercmd_t *cmd, CBaseWeaponContext *weapon)
{
	if (from->weapondata[cmd->weaponselect].m_iId == cmd->weaponselect)
	{
		CBaseWeaponContext *selectedWeapon = GetWeaponContext(cmd->weaponselect);
		if (selectedWeapon && selectedWeapon->m_iId != weapon->m_iId)
		{
			weapon->Holster();
			selectedWeapon->Deploy();
			to->client.m_iId = cmd->weaponselect;
		}
	}
}

CBaseWeaponContext* CWeaponPredictingContext::GetWeaponContext(uint32_t weaponID)
{
	if (m_weaponsState.count(weaponID)) {
		return m_weaponsState[weaponID].get();
	}
	else
	{
		switch (weaponID)
		{
			case WEAPON_GLOCK:  
				m_weaponsState[weaponID] = std::make_unique<CGlockWeaponLogic>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_CROSSBOW:
				m_weaponsState[weaponID] = std::make_unique<CCrossbowWeaponLogic>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_PYTHON:
				m_weaponsState[weaponID] = std::make_unique<CPythonWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_MP5:
				m_weaponsState[weaponID] = std::make_unique<CMP5WeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			case WEAPON_SHOTGUN:
				m_weaponsState[weaponID] = std::make_unique<CShotgunWeaponContext>(std::make_unique<CClientWeaponLayerImpl>(m_playerState));
				break;
			default: 
				return nullptr;
		}

		ItemInfo itemInfo;
		m_weaponsState[weaponID]->GetItemInfo(&itemInfo);
		CBaseWeaponContext::ItemInfoArray[weaponID] = itemInfo;
		return m_weaponsState[weaponID].get();
	}
}
