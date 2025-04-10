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

#include "gauss.h"
#include <utility>

#ifndef CLIENT_DLL
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "shake.h"
#include "weapon_gauss.h"
#else
#include "hud.h"
#include "const.h"
#include "utils.h"
#endif

#define SND_SPAWNING		(1<<8)		// duplicated in protocol.h we're spawing, used in some cases for ambients 
#define SND_STOP			(1<<5)		// duplicated in protocol.h stop sound
#define SND_CHANGE_VOL		(1<<6)		// duplicated in protocol.h change sound vol
#define SND_CHANGE_PITCH	(1<<7)		// duplicated in protocol.h change sound pitch

CGaussWeaponContext::CGaussWeaponContext(std::unique_ptr<IWeaponLayer>&& layer) :
	CBaseWeaponContext(std::move(layer)),
	m_fInAttack(0),
	m_fPrimaryFire(false),
	m_flAmmoStartCharge(0.0f),
	m_flNextAmmoBurn(0.0f),
	m_flPlayAftershock(0.0f),
	m_flStartCharge(0.0f),
	m_iSoundState(0)
{
	m_iId = WEAPON_GAUSS;
	m_iDefaultAmmo = GAUSS_DEFAULT_GIVE;
	m_usGaussFire = m_pLayer->PrecacheEvent("events/gauss.sc");
	m_usGaussSpin = m_pLayer->PrecacheEvent("events/gaussspin.sc");
}

int CGaussWeaponContext::GetItemInfo(ItemInfo *p) const
{
	p->pszName = CLASSNAME_STR(GAUSS_CLASSNAME);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iId = m_iId;
	p->iFlags = 0;
	p->iWeight = GAUSS_WEIGHT;
	return 1;
}

bool CGaussWeaponContext::Deploy()
{
	m_flPlayAftershock = 0.0f;
	return DefaultDeploy( "models/v_gauss.mdl", "models/p_gauss.mdl", GAUSS_DRAW, "gauss" );
}

void CGaussWeaponContext::Holster()
{
	m_fInAttack = 0;
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
	SendWeaponAnim( GAUSS_HOLSTER );
	// EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CGaussWeaponContext::PrimaryAttack()
{
	// don't fire underwater
	if (m_pLayer->GetPlayerWaterlevel() == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.15f);
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.15f;
		return;
	}

	if (m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 2)
	{
		PlayEmptySound();
		m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
		return;
	}

	m_fPrimaryFire = TRUE;
	m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 2);

#ifndef CLIENT_DLL
	CGauss *pWeapon = static_cast<CGauss*>(m_pLayer->GetWeaponEntity());
	pWeapon->m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
#endif

	StartFire();
	m_fInAttack = 0;
	m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f;
	m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.2f);
}

void CGaussWeaponContext::SecondaryAttack()
{
#ifndef CLIENT_DLL
	// JoshA: Sanitize this so it's not total garbage on level transition
	// and we end up ear blasting the player!
	CGauss *pWeapon = static_cast<CGauss*>(m_pLayer->GetWeaponEntity());
#endif
	m_flStartCharge = std::min(m_flStartCharge, m_pLayer->GetTime());

	// don't fire underwater
	if ( m_pLayer->GetPlayerWaterlevel() == 3)
	{
		if ( m_fInAttack != 0 )
		{
#ifndef CLIENT_DLL
			EMIT_SOUND_DYN(ENT(pWeapon->m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f));
#endif
			SendWeaponAnim( GAUSS_IDLE );
			m_fInAttack = 0;
		}
		else
		{
			PlayEmptySound( );
		}

		m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(0.5f);
		m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f;
		return;
	}

	if ( m_fInAttack == 0 )
	{
		if ( m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) < 1 )
		{
			PlayEmptySound( );
			m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5f);
			return;
		}

		m_fPrimaryFire = FALSE;
		m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1); // take one ammo just to start the spin
		m_flNextAmmoBurn = m_pLayer->GetWeaponTimeBase(UsePredicting());
		
		SendWeaponAnim( GAUSS_SPINUP );
		m_fInAttack = 1;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.5;
		m_flStartCharge = m_pLayer->GetTime();
		m_flAmmoStartCharge = m_pLayer->GetWeaponTimeBase(UsePredicting()) + GetFullChargeTime();

		WeaponEventParams params;
		params.flags = WeaponEventFlags::NotHost;
		params.eventindex = m_usGaussSpin;
		params.delay = 0.0f;
		params.origin = m_pLayer->GetGunPosition();
		params.angles = m_pLayer->GetCameraOrientation().GetAngles();
		params.fparam1 = 0.0f;
		params.fparam2 = 0.0f;
		params.iparam1 = 110;
		params.iparam2 = 0;
		params.bparam1 = 0;
		params.bparam2 = 0;

		if (m_pLayer->ShouldRunFuncs()) {
			m_pLayer->PlaybackWeaponEvent(params);
		}

		// spin up
		m_iSoundState = SND_CHANGE_PITCH;
#ifndef CLIENT_DLL
		pWeapon->m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;
#endif
	}
	else if (m_fInAttack == 1)
	{
		if (m_flTimeWeaponIdle < m_pLayer->GetWeaponTimeBase(UsePredicting()))
		{
			SendWeaponAnim( GAUSS_SPIN );
			m_fInAttack = 2;
		}
	}
	else
	{
		// during the charging process, eat one bit of ammo every once in a while
		if ( m_pLayer->GetWeaponTimeBase(UsePredicting()) >= m_flNextAmmoBurn && m_flNextAmmoBurn != 1000 )
		{
			if ( m_pLayer->IsMultiplayer() )
			{
				m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);
				m_flNextAmmoBurn = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.1f;
			}
			else
			{
				m_pLayer->SetPlayerAmmo(m_iPrimaryAmmoType, m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) - 1);
				m_flNextAmmoBurn = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 0.3f;
			}
		}

		if ( m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0 )
		{
			// out of ammo! force the gun to fire
			StartFire();
			m_fInAttack = 0;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f;
			m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f);
			return;
		}
		
		if ( m_pLayer->GetWeaponTimeBase(UsePredicting()) >= m_flAmmoStartCharge )
		{
			// don't eat any more ammo after gun is fully charged.
			m_flNextAmmoBurn = 1000;
		}

		int pitch = (m_pLayer->GetTime() - m_flStartCharge) * (150 / GetFullChargeTime()) + 100;
		if (pitch > 250)
			pitch = 250;
		
		// ALERT( at_console, "%d %d %d\n", m_fInAttack, m_iSoundState, pitch );
		//if ( m_iSoundState == 0 )
		//	ALERT( at_console, "sound state %d\n", m_iSoundState );

		WeaponEventParams params;
		params.flags = WeaponEventFlags::NotHost;
		params.eventindex = m_usGaussSpin;
		params.delay = 0.0f;
		params.origin = m_pLayer->GetGunPosition();
		params.angles = m_pLayer->GetViewAngles();
		params.fparam1 = 0.0f;
		params.fparam2 = 0.0f;
		params.iparam1 = pitch;
		params.iparam2 = 0;
		params.bparam1 = ( m_iSoundState == SND_CHANGE_PITCH ) ? 1 : 0;
		params.bparam2 = 0;

		if (m_pLayer->ShouldRunFuncs()) {
			m_pLayer->PlaybackWeaponEvent(params);
		}

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions
#ifndef CLIENT_DLL
		pWeapon->m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;
#endif
		// m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		if (m_flStartCharge < m_pLayer->GetTime() - 10.0f)
		{
#ifndef CLIENT_DLL
			// Player charged up too long. Zap him.
			EMIT_SOUND_DYN(ENT(pWeapon->m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0, 0x3f));
			EMIT_SOUND_DYN(ENT(pWeapon->m_pPlayer->pev), CHAN_ITEM, "weapons/electro6.wav", 1.0, ATTN_NORM, 0, 75 + RANDOM_LONG(0, 0x3f));
#endif		
			m_fInAttack = 0;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f;
			m_pLayer->SetPlayerNextAttackTime(m_pLayer->GetWeaponTimeBase(UsePredicting()) + 1.0f);
#ifndef CLIENT_DLL
			pWeapon->m_pPlayer->TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 50, DMG_SHOCK);
			UTIL_ScreenFade(pWeapon-> m_pPlayer, Vector(255,128,0), 2, 0.5, 128, FFADE_IN);
#endif
			SendWeaponAnim( GAUSS_IDLE );
			
			// Player may have been killed and this weapon dropped, don't execute any more code after this!
			return;
		}
	}
}

//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CGaussWeaponContext::StartFire()
{
	float flDamage;

	// JoshA: Sanitize this so it's not total garbage on level transition
	// and we end up ear blasting the player!
	m_flStartCharge = std::min( m_flStartCharge, m_pLayer->GetTime() );
#ifndef CLIENT_DLL
	CGauss *pWeapon = static_cast<CGauss*>(m_pLayer->GetWeaponEntity());
	UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle + pWeapon->m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
#else
	Vector vecAiming = m_pLayer->GetCameraOrientation().GetForward();
#endif
	Vector vecSrc = m_pLayer->GetGunPosition(); // + gpGlobals->v_up * -8 + gpGlobals->v_right * 8;
	
	if ( m_pLayer->GetTime() - m_flStartCharge > GetFullChargeTime())
	{
		flDamage = 200;
	}
	else
	{
		flDamage = 200 * (( m_pLayer->GetTime() - m_flStartCharge) / GetFullChargeTime() );
	}

	if ( m_fPrimaryFire )
	{
		// fixed damage on primary attack
#ifdef CLIENT_DLL
		flDamage = 20;
#else 
		flDamage = gSkillData.plrDmgGauss;
#endif
	}

	if (m_fInAttack != 3)
	{
		//ALERT ( at_console, "Time:%f Damage:%f\n", gpGlobals->time - m_flStartCharge, flDamage );
		if ( !m_fPrimaryFire )
		{
			const Vector origVelocity = m_pLayer->GetPlayerVelocity();
			Vector velocity = origVelocity - vecAiming * flDamage * 5;

			// in deathmatch, gauss can pop you up into the air. Not in single play.
			if (!m_pLayer->IsMultiplayer()) {
				velocity.z = origVelocity.z;
			}

			m_pLayer->SetPlayerVelocity(velocity);
		}

		SendWeaponAnim( GAUSS_FIRE2 );

#ifndef CLIENT_DLL
		// player "shoot" animation
		pWeapon->m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
#endif
	}

	// time until aftershock 'static discharge' sound
	m_flPlayAftershock = m_pLayer->GetTime() + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.3f, 0.8f);

	Fire( vecSrc, vecAiming, flDamage );
}

void CGaussWeaponContext::Fire( Vector vecOrigSrc, Vector vecDir, float flDamage )
{
	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 8192;
	float flMaxFrac = 1.0;
	int	nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int	nMaxHits = 10;

#ifndef CLIENT_DLL
	TraceResult tr, beam_tr;
	CGauss *pWeapon = static_cast<CGauss*>(m_pLayer->GetWeaponEntity());
	edict_t *pentIgnore = pWeapon->m_pPlayer->edict();
	pWeapon->m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
#endif
	
	// The main firing event is sent unreliably so it won't be delayed.
	// PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usGaussFire, 0.0, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, flDamage, 0.0, 0, 0, m_fPrimaryFire ? 1 : 0, 0 );
	WeaponEventParams params;
	params.flags = WeaponEventFlags::NotHost;
	params.eventindex = m_usGaussFire;
	params.delay = 0.0f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetViewAngles();
	params.fparam1 = flDamage;
	params.fparam2 = 0.0f;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = m_fPrimaryFire ? 1 : 0;
	params.bparam2 = 0;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}

	// This reliable event is used to stop the spinning sound
	// It's delayed by a fraction of second to make sure it is delayed by 1 frame on the client
	// It's sent reliably anyway, which could lead to other delays
	params.flags = WeaponEventFlags::NotHost | WeaponEventFlags::Reliable;
	params.eventindex = m_usGaussFire;
	params.delay = 0.01f;
	params.origin = m_pLayer->GetGunPosition();
	params.angles = m_pLayer->GetViewAngles();
	params.fparam1 = 0.0f;
	params.fparam2 = 0.0f;
	params.iparam1 = 0;
	params.iparam2 = 0;
	params.bparam1 = 0;
	params.bparam2 = 1;

	if (m_pLayer->ShouldRunFuncs()) {
		m_pLayer->PlaybackWeaponEvent(params);
	}
	
	/*ALERT( at_console, "%f %f %f\n%f %f %f\n", 
		vecSrc.x, vecSrc.y, vecSrc.z, 
		vecDest.x, vecDest.y, vecDest.z );*/
	
//	ALERT( at_console, "%f %f\n", tr.flFraction, flMaxFrac );

#ifndef CLIENT_DLL
	while (flDamage > 10 && nMaxHits > 0)
	{
		nMaxHits--;

		// ALERT( at_console, "." );
		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

		if (tr.fAllSolid)
			break;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity == NULL)
			break;

		if ( fFirstBeam )
		{
			pWeapon->m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
			fFirstBeam = 0;
	
			nTotal += 26;
		}
		
		if (pEntity->pev->takedamage)
		{
			ClearMultiDamage();

			// if you hurt yourself clear the headshot bit
			if (pWeapon->m_pPlayer->pev == pEntity->pev)
			{
				tr.iHitgroup = 0;
			}

			pEntity->TraceAttack( pWeapon->m_pPlayer->pev, flDamage, vecDir, &tr, DMG_BULLET );
			ApplyMultiDamage(pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev);
		}

		if ( pEntity->ReflectGauss() )
		{
			float n;

			pentIgnore = NULL;

			n = -DotProduct(tr.vecPlaneNormal, vecDir);

			if (n < 0.5) // 60 degrees
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				Vector r;
			
				r = 2.0 * tr.vecPlaneNormal * n + vecDir;
				flMaxFrac = flMaxFrac - tr.flFraction;
				vecDir = r;
				vecSrc = tr.vecEndPos + vecDir * 8;
				vecDest = vecSrc + vecDir * 8192;

				// explode a bit
				pWeapon->m_pPlayer->RadiusDamage( tr.vecEndPos, pWeapon->pev, pWeapon->m_pPlayer->pev, flDamage * n, CLASS_NONE, DMG_BLAST );

				nTotal += 34;
				
				// lose energy
				if (n == 0) n = 0.1;
				flDamage = flDamage * (1 - n);
			}
			else
			{
				nTotal += 13;

				DecalGunshot( &tr, BULLET_MONSTER_12MM, vecSrc, vecDest);

				// limit it to one hole punch
				if (fHasPunched)
					break;
				fHasPunched = 1;

				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if ( !m_fPrimaryFire )
				{
					UTIL_TraceLine( tr.vecEndPos + vecDir * 8, vecDest, dont_ignore_monsters, pentIgnore, &beam_tr);
					if (!beam_tr.fAllSolid)
					{
						// trace backwards to find exit point
						UTIL_TraceLine( beam_tr.vecEndPos, tr.vecEndPos, dont_ignore_monsters, pentIgnore, &beam_tr);

						float n = (beam_tr.vecEndPos - tr.vecEndPos).Length( );

						if (n < flDamage)
						{
							if (n == 0) n = 1;
							flDamage -= n;

							// ALERT( at_console, "punch %f\n", n );
							nTotal += 21;

							// exit blast damage
							//m_pPlayer->RadiusDamage( beam_tr.vecEndPos + vecDir * 8, pev, m_pPlayer->pev, flDamage, CLASS_NONE, DMG_BLAST );
							float damage_radius = flDamage * (m_pLayer->IsMultiplayer() ? 1.75f : 2.5f);
							::RadiusDamage( beam_tr.vecEndPos + vecDir * 8, pWeapon->pev, pWeapon->m_pPlayer->pev, flDamage, damage_radius, CLASS_NONE, DMG_BLAST );
							CSoundEnt::InsertSound( bits_SOUND_COMBAT, pWeapon->pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );
							DecalGunshot( &beam_tr, BULLET_MONSTER_12MM, vecSrc, vecDest);

							nTotal += 53;
							vecSrc = beam_tr.vecEndPos + vecDir;
						}
					}
					else
					{
						//ALERT( at_console, "blocked %f\n", n );
						flDamage = 0;
					}
				}
				else
				{
					//ALERT( at_console, "blocked solid\n" );
					flDamage = 0;
				}
			}
		}
		else
		{
			vecSrc = tr.vecEndPos + vecDir;
			pentIgnore = ENT( pEntity->pev );
		}
	}
#endif
	// ALERT( at_console, "%d bytes\n", nTotal );
}

void CGaussWeaponContext::WeaponIdle()
{
	ResetEmptySound();

	// play aftershock static discharge
	if (m_flPlayAftershock && m_flPlayAftershock < m_pLayer->GetTime())
	{
#ifndef CLIENT_DLL
		CGauss *pWeapon = static_cast<CGauss*>(m_pLayer->GetWeaponEntity());
		switch (RANDOM_LONG(0, 3))
		{
		case 0:	EMIT_SOUND(ENT(pWeapon->m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 1:	EMIT_SOUND(ENT(pWeapon->m_pPlayer->pev), CHAN_WEAPON, "weapons/electro5.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 2:	EMIT_SOUND(ENT(pWeapon->m_pPlayer->pev), CHAN_WEAPON, "weapons/electro6.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 3:	break; // no sound
		}
#endif
		m_flPlayAftershock = 0.0f;
	}

	if (m_flTimeWeaponIdle > m_pLayer->GetWeaponTimeBase(UsePredicting()))
		return;

	if (m_fInAttack != 0)
	{
		StartFire();
		m_fInAttack = 0;
		m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 2.0f;
	}
	else
	{
		int iAnim;
		float flRand = m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 0.0f, 1.0f);
		if (flRand <= 0.5f)
		{
			iAnim = GAUSS_IDLE;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
		}
		else if (flRand <= 0.75f)
		{
			iAnim = GAUSS_IDLE2;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + m_pLayer->GetRandomFloat(m_pLayer->GetRandomSeed(), 10.f, 15.f);
		}
		else
		{
			iAnim = GAUSS_FIDGET;
			m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + 3.0f;
		}
		SendWeaponAnim( iAnim );
	}
}

float CGaussWeaponContext::GetFullChargeTime()
{
	return m_pLayer->IsMultiplayer() ? 1.5f : 4.0f;
}
