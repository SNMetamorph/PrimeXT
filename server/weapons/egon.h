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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "env_beam.h"
#include "env_sprite.h"
#include "customentity.h"
#include "gamerules.h"
#include "user_messages.h"

class CEgon : public CBasePlayerWeapon
{
	DECLARE_CLASS( CEgon, CBasePlayerWeapon );
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	DECLARE_DATADESC();

	BOOL Deploy( void );
	void Holster( void );

	void UpdateEffect( const Vector &startPoint, const Vector &endPoint, float timeBlend );

	void CreateEffect ( void );
	void DestroyEffect ( void );

	void EndAttack( void );
	void Attack( void );
	void PrimaryAttack( void );
	void WeaponIdle( void );
	static int g_fireAnims1[];
	static int g_fireAnims2[];

	float m_flAmmoUseTime;// since we use < 1 point of ammo per update, we subtract ammo on a timer.

	float GetPulseInterval( void );
	float GetDischargeInterval( void );

	void Fire( const Vector &vecOrigSrc, const Vector &vecDir );

	BOOL HasAmmo(void);
	void UseAmmo(int count);
	
	enum EGON_FIRESTATE { FIRE_OFF, FIRE_CHARGE };
	enum EGON_FIREMODE { FIRE_NARROW, FIRE_WIDE };
private:
	float		m_shootTime;
	CBeam		*m_pBeam;
	CBeam		*m_pNoise;
	CSprite		*m_pSprite;
	EGON_FIRESTATE	m_fireState;
	EGON_FIREMODE	m_fireMode;
	float		m_shakeTime;
	BOOL		m_deployed;
};
