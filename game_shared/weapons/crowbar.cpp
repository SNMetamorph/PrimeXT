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

#include "crowbar.h"
#include <utility>

#ifndef CLIENT_DLL
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "weapon_crowbar.h"
#endif

#define CROWBAR_BODYHIT_VOLUME	128
#define CROWBAR_WALLHIT_VOLUME	512

CCrowbarWeaponContext::CCrowbarWeaponContext(std::unique_ptr<IWeaponLayer> &&layer) :
	CBaseWeaponContext(std::move(layer))
{
	m_iId = WEAPON_CROWBAR;
	m_iClip = -1;
	m_iSwing = 0;
	m_usCrowbar = m_pLayer->PrecacheEvent("events/crowbar.sc");
}

int CCrowbarWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(CROWBAR_CLASSNAME);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = m_iId;
	p->iWeight = CROWBAR_WEIGHT;
	return 1;
}

bool CCrowbarWeaponContext::Deploy( )
{
	return DefaultDeploy( "models/v_crowbar.mdl", "models/p_crowbar.mdl", CROWBAR_DRAW, "crowbar" );
}

void CCrowbarWeaponContext::Holster()
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	SendWeaponAnim( CROWBAR_HOLSTER );
}

void CCrowbarWeaponContext::PrimaryAttack()
{
	if (!Swing(true))
	{
#ifndef CLIENT_DLL
		CCrowbar *weapon = static_cast<CCrowbar*>(m_pLayer->GetWeaponEntity());
		weapon->SetThink(&CCrowbar::SwingAgain);
		weapon->pev->nextthink = gpGlobals->time + 0.1;
#endif
	}
}

#ifndef CLIENT_DLL
static void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}
#endif

bool CCrowbarWeaponContext::Swing(bool fFirst)
{
#ifdef CLIENT_DLL
	if (fFirst)
	{
		WeaponEventParams params;
		params.flags = WeaponEventFlags::NotHost;
		params.eventindex = m_usCrowbar;
		params.delay = 0.0f;
		params.origin = m_pLayer->GetGunPosition();
		params.angles = m_pLayer->GetCameraOrientation();
		params.fparam1 = 0.0f;
		params.fparam2 = 0.0f;
		params.iparam1 = 0;
		params.iparam2 = 0;
		params.bparam1 = 0;
		params.bparam2 = 0;

		if (m_pLayer->ShouldRunFuncs()) {
			m_pLayer->PlaybackWeaponEvent(params);
		}

		// miss
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
	}
	return false;
#else
	bool fDidHit = false;

	TraceResult tr;
	CCrowbar *weapon = static_cast<CCrowbar*>(m_pLayer->GetWeaponEntity());
	CBasePlayer *player = weapon->m_pPlayer;

	UTIL_MakeVectors(player->pev->v_angle);
	Vector vecSrc	= player->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( player->pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( player->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, player->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if (fFirst)
	{
		WeaponEventParams params;
		params.flags = WeaponEventFlags::NotHost;
		params.eventindex = m_usCrowbar;
		params.delay = 0.0f;
		params.origin = m_pLayer->GetGunPosition();
		params.angles = m_pLayer->GetCameraOrientation();
		params.fparam1 = 0.0f;
		params.fparam2 = 0.0f;
		params.iparam1 = 0;
		params.iparam2 = 0;
		params.bparam1 = 0;
		params.bparam2 = 0;

		if (m_pLayer->ShouldRunFuncs()) {
			m_pLayer->PlaybackWeaponEvent(params);
		}
	}

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
			
			// player "shoot" animation
			player->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( CROWBAR_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( CROWBAR_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( CROWBAR_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		player->SetAnimation( PLAYER_ATTACK1 );
		
		// hit
		fDidHit = true;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		ClearMultiDamage( );

		// JoshA: Changed from < -> <= to fix the full swing logic since client weapon prediction.
		// -1.0f + 1.0f = 0.0f. UTIL_WeaponTimeBase is always 0 with client weapon prediction (0 time base vs curtime base)
		if ( ( m_flNextPrimaryAttack + 1.0f <= m_pLayer->GetWeaponTimeBase(UsePredicting()) ) || m_pLayer->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(player->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(player->pev, gSkillData.plrDmgCrowbar / 2, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}	
		ApplyMultiDamage( player->pev, player->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		bool fHitWorld = true;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND(ENT(player->pev), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(player->pev), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM); break;
				case 2:
					EMIT_SOUND(ENT(player->pev), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM); break;
				}
				player->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;

				if ( !pEntity->IsAlive() )
					  return TRUE;
				else
					  flVol = 0.1;

				fHitWorld = false;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play crowbar strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(player->pev), CHAN_ITEM, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(player->pev), CHAN_ITEM, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		player->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;
		m_flNextPrimaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.25f;
		weapon->SetThink( &CCrowbar::Smack );
		weapon->pev->nextthink = gpGlobals->time + 0.2f;
	}
	return fDidHit;
#endif
}
