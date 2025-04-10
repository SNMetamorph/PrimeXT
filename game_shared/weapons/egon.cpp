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

#include "egon.h"
#include <utility>

#ifndef CLIENT_DLL
#include "weapon_egon.h"
#endif

std::array<int, 4> CEgonWeaponContext::g_fireAnims1 = { EGON_FIRE1, EGON_FIRE2, EGON_FIRE3, EGON_FIRE4 };
std::array<int, 1> CEgonWeaponContext::g_fireAnims2 = { EGON_ALTFIRECYCLE };

CEgonWeaponContext::CEgonWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer)),
	m_deployed(false),
	m_fireMode(FIRE_NARROW),
	m_fireState(FIRE_OFF),
	m_flAmmoUseTime(0.0f),
	m_flAttackCooldown(0.0f),
	m_shakeTime(0.0f)
{
	m_iId = WEAPON_EGON;
	m_iDefaultAmmo = EGON_DEFAULT_GIVE;
	m_usEgonFire = m_pLayer->PrecacheEvent("events/egon_fire.sc");
	m_usEgonStop = m_pLayer->PrecacheEvent("events/egon_stop.sc");
#ifndef CLIENT_DLL
	m_pBeam = nullptr;
	m_pNoise = nullptr;
	m_pSprite = nullptr;
#endif
}

bool CEgonWeaponContext::Deploy()
{
	m_deployed = false;
	m_fireState = FIRE_OFF;
	return DefaultDeploy( "models/v_egon.mdl", "models/p_egon.mdl", EGON_DRAW, "egon" );
}

void CEgonWeaponContext::Holster()
{
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	SendWeaponAnim( EGON_HOLSTER );
	EndAttack();
}

int CEgonWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(EGON_CLASSNAME);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = WEAPON_EGON;
	p->iFlags = 0;
	p->iWeight = EGON_WEIGHT;
	return 1;
}

float CEgonWeaponContext::GetPulseInterval()
{
	return m_pLayer->IsMultiplayer() ? 0.1f : EGON_PULSE_INTERVAL;
}

float CEgonWeaponContext::GetDischargeInterval()
{
	return m_pLayer->IsMultiplayer() ? 0.1f : EGON_DISCHARGE_INTERVAL;
}

void CEgonWeaponContext::Attack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
#ifdef CLIENT_DLL
		if (m_fireState != FIRE_OFF)
#else
		if (m_fireState != FIRE_OFF || m_pBeam)
#endif
		{
			EndAttack();
		}
		else
		{
			PlayEmptySound();
		}
		return;
	}

#ifndef CLIENT_DLL
	CEgon *pWeapon = static_cast<CEgon*>(m_pLayer->GetWeaponEntity());
	UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle + pWeapon->m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = pWeapon->m_pPlayer->GetGunPosition( );
#endif

	switch( m_fireState )
	{
		case FIRE_OFF:
		{
			if (!HasAmmo())
			{
				m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.25f);
				m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.25f;
				PlayEmptySound();
				return;
			}

			WeaponEventParams params;
			params.flags = WeaponEventFlags::NotHost;
			params.eventindex = m_usEgonFire;
			params.delay = 0.0f;
			params.origin = m_pLayer->GetGunPosition();
			params.angles = m_pLayer->GetViewAngles();
			params.fparam1 = 0.0f;
			params.fparam2 = 0.0f;
			params.iparam1 = m_fireState;
			params.iparam2 = m_fireMode;
			params.bparam1 = 1;
			params.bparam2 = 0;

			if (m_pLayer->ShouldRunFuncs()) {
				m_pLayer->PlaybackWeaponEvent(params);
			}

#ifndef CLIENT_DLL
			m_flAmmoUseTime = gpGlobals->time;// start using ammo ASAP.
			pWeapon->pev->dmgtime = gpGlobals->time + GetPulseInterval();
			pWeapon->m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
#endif
			m_shakeTime = 0.0f;
			m_flAttackCooldown = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 2.f;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.1f;
			m_fireState = FIRE_CHARGE;
		}
		break;

		case FIRE_CHARGE:
		{
#ifndef CLIENT_DLL
			Fire( vecSrc, vecAiming );
			pWeapon->m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
#endif
			if (m_flAttackCooldown <= m_pLayer->GetWeaponTimeBase(UsePredicting()))
			{
				// PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usEgonFire, 0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, m_fireState, m_fireMode, 0, 0 );
				WeaponEventParams params;
				params.flags = WeaponEventFlags::NotHost;
				params.eventindex = m_usEgonFire;
				params.delay = 0.0f;
				params.origin = m_pLayer->GetGunPosition();
				params.angles = m_pLayer->GetViewAngles();
				params.fparam1 = 0.0f;
				params.fparam2 = 0.0f;
				params.iparam1 = m_fireState;
				params.iparam2 = m_fireMode;
				params.bparam1 = 0;
				params.bparam2 = 0;

				if (m_pLayer->ShouldRunFuncs()) {
					m_pLayer->PlaybackWeaponEvent(params);
				}

				m_flAttackCooldown = 1000;
			}

			if (!HasAmmo())
			{
				EndAttack();
				m_fireState = FIRE_OFF;
				m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(1.0f);
				m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f;
			}
		}
		break;
	}
}

void CEgonWeaponContext::PrimaryAttack()
{
	m_fireMode = FIRE_WIDE;
	Attack();
}

void CEgonWeaponContext::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
#ifndef CLIENT_DLL
	TraceResult tr;
	CEgon *pWeapon = static_cast<CEgon*>(m_pLayer->GetWeaponEntity());
	Vector vecDest = vecOrigSrc + vecDir * 2048;
	Vector tmpSrc = vecOrigSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;
	edict_t *pentIgnore = pWeapon->m_pPlayer->edict();
	
	// ALERT( at_console, "." );
	
	UTIL_TraceLine( vecOrigSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );

	if (tr.fAllSolid)
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity == NULL)
		return;

	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( m_pSprite && pEntity->pev->takedamage )
		{
			m_pSprite->pev->effects &= ~EF_NODRAW;
		}
		else if ( m_pSprite )
		{
			m_pSprite->pev->effects |= EF_NODRAW;
		}
	}

	float timedist;

	switch ( m_fireMode )
	{
	case FIRE_NARROW:
		if ( pWeapon->pev->dmgtime < gpGlobals->time )
		{
			// Narrow mode only does damage to the entity it hits
			ClearMultiDamage();
			if (pEntity->pev->takedamage)
			{
				pEntity->TraceAttack( pWeapon->m_pPlayer->pev, gSkillData.plrDmgEgonNarrow, vecDir, &tr, DMG_ENERGYBEAM );
			}
			ApplyMultiDamage(pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// multiplayer uses 1 ammo every 1/10th second
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				}
			}
			else
			{
				// single player, use 3 ammo/second
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.166;
				}
			}

			pWeapon->pev->dmgtime = gpGlobals->time + GetPulseInterval();
		}
		timedist = ( pWeapon->pev->dmgtime - gpGlobals->time ) / GetPulseInterval();
		break;
	
	case FIRE_WIDE:
		if ( pWeapon->pev->dmgtime < gpGlobals->time )
		{
			// wide mode does damage to the ent, and radius damage
			ClearMultiDamage();
			if (pEntity->pev->takedamage)
			{
				pEntity->TraceAttack( pWeapon->m_pPlayer->pev, gSkillData.plrDmgEgonWide, vecDir, &tr, DMG_ENERGYBEAM | DMG_ALWAYSGIB);
			}
			ApplyMultiDamage(pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// radius damage a little more potent in multiplayer.
				::RadiusDamage( tr.vecEndPos, pWeapon->pev, pWeapon->m_pPlayer->pev, gSkillData.plrDmgEgonWide/4, 128, CLASS_NONE, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB );
			}

			if ( !pWeapon->m_pPlayer->IsAlive() )
				return;

			if ( g_pGameRules->IsMultiplayer() )
			{
				//multiplayer uses 5 ammo/second
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.2;
				}
			}
			else
			{
				// Wide mode uses 10 charges per second in single player
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				}
			}

			pWeapon->pev->dmgtime = gpGlobals->time + GetDischargeInterval();
			if ( m_shakeTime < gpGlobals->time )
			{
				UTIL_ScreenShake( tr.vecEndPos, 5.0, 150.0, 0.75, 250.0 );
				m_shakeTime = gpGlobals->time + 1.5;
			}
		}
		timedist = ( pWeapon->pev->dmgtime - gpGlobals->time ) / GetDischargeInterval();
		break;
	}

	if ( timedist < 0 )
		timedist = 0;
	else if ( timedist > 1 )
		timedist = 1;
	timedist = 1-timedist;

	UpdateEffect( tmpSrc, tr.vecEndPos, timedist );
#endif
}

bool CEgonWeaponContext::HasAmmo()
{
	return m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) > 0;
}

void CEgonWeaponContext::UseAmmo( int count )
{
	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) >= count)
		m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - count);
	else
		m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, 0);
}

void CEgonWeaponContext::UpdateEffect( const Vector &startPoint, const Vector &endPoint, float timeBlend )
{
#ifndef CLIENT_DLL
	if ( !m_pBeam )
	{
		CreateEffect();
	}

	m_pBeam->SetStartPos( endPoint );
	m_pBeam->SetBrightness( 255 - (timeBlend*180) );
	m_pBeam->SetWidth( 40 - (timeBlend*20) );

	if ( m_fireMode == FIRE_WIDE )
		m_pBeam->SetColor( 30 + (25*timeBlend), 30 + (30*timeBlend), 64 + 80*fabs(sin(gpGlobals->time*10)) );
	else
		m_pBeam->SetColor( 60 + (25*timeBlend), 120 + (30*timeBlend), 64 + 80*fabs(sin(gpGlobals->time*10)) );


	UTIL_SetOrigin( m_pSprite, endPoint );
	m_pSprite->pev->frame += 8 * gpGlobals->frametime;
	if ( m_pSprite->pev->frame > m_pSprite->Frames() )
		m_pSprite->pev->frame = 0;

	m_pNoise->SetStartPos( endPoint );
#endif
}

void CEgonWeaponContext::CreateEffect( void )
{
#ifndef CLIENT_DLL
	CEgon *pWeapon = static_cast<CEgon*>(m_pLayer->GetWeaponEntity());
	DestroyEffect();

	m_pBeam = CBeam::BeamCreate( EGON_BEAM_SPRITE, 40 );
	m_pBeam->PointEntInit( pWeapon->GetAbsOrigin(), pWeapon->m_pPlayer->entindex() );
	m_pBeam->SetFlags( BEAM_FSINE );
	m_pBeam->SetEndAttachment( 1 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;	// Flag these to be destroyed on save/restore or level transition
	m_pBeam->pev->flags |= FL_SKIPLOCALHOST;
	m_pBeam->pev->owner = pWeapon->m_pPlayer->edict();

	m_pNoise = CBeam::BeamCreate( EGON_BEAM_SPRITE, 55 );
	m_pNoise->PointEntInit( pWeapon->GetAbsOrigin(), pWeapon->m_pPlayer->entindex() );
	m_pNoise->SetScrollRate( 25 );
	m_pNoise->SetBrightness( 100 );
	m_pNoise->SetEndAttachment( 1 );
	m_pNoise->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_pNoise->pev->flags |= FL_SKIPLOCALHOST;
	m_pNoise->pev->owner = pWeapon->m_pPlayer->edict();

	m_pSprite = CSprite::SpriteCreate( EGON_FLARE_SPRITE, pWeapon->GetAbsOrigin(), FALSE );
	m_pSprite->pev->scale = 1.0;
	m_pSprite->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
	m_pSprite->pev->owner = pWeapon->m_pPlayer->edict();

	if ( m_fireMode == FIRE_WIDE )
	{
		m_pBeam->SetScrollRate( 50 );
		m_pBeam->SetNoise( 20 );
		m_pNoise->SetColor( 50, 50, 255 );
		m_pNoise->SetNoise( 8 );
	}
	else
	{
		m_pBeam->SetScrollRate( 110 );
		m_pBeam->SetNoise( 5 );
		m_pNoise->SetColor( 80, 120, 255 );
		m_pNoise->SetNoise( 2 );
	}
#endif
}

void CEgonWeaponContext::DestroyEffect()
{
#ifndef CLIENT_DLL
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
	if ( m_pNoise )
	{
		UTIL_Remove( m_pNoise );
		m_pNoise = NULL;
	}
	if ( m_pSprite )
	{
		if ( m_fireMode == FIRE_WIDE )
			m_pSprite->Expand( 10, 500 );
		else
			UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
#endif
}

void CEgonWeaponContext::EndAttack()
{
	bool bMakeNoise = m_fireState != FIRE_OFF; //Checking the button just in case!.

	// PLAYBACK_EVENT_FULL( FEV_GLOBAL | FEV_RELIABLE, m_pPlayer->edict(), m_usEgonStop, 0, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, bMakeNoise, 0, 0, 0 );

	WeaponEventParams params;
	params.flags = WeaponEventFlags::Global | WeaponEventFlags::Reliable;
	params.eventindex = m_usEgonStop;
	params.delay = 0.0f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetViewAngles();
	params.fparam1 = 0.0f;
	params.fparam2 = 0.0f;
	params.iparam1 = bMakeNoise;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 2.0f;
	m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.5f);
	m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
	m_fireState = FIRE_OFF;

	DestroyEffect();
}

void CEgonWeaponContext::WeaponIdle()
{
	ResetEmptySound();

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if (m_fireState != FIRE_OFF)
		EndAttack();

	int iAnim;
	float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);

	if (flRand <= 0.5)
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.0f, 15.0f);
	}
	else
	{
		iAnim = EGON_FIDGET1;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 3;
	}

	SendWeaponAnim(iAnim);
	m_deployed = true;
}
