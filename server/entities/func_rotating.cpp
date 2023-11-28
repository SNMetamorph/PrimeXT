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

#include "func_rotating.h"

LINK_ENTITY_TO_CLASS(func_rotating, CFuncRotating);

BEGIN_DATADESC(CFuncRotating)
	DEFINE_FIELD(m_bStopAtStartPos, FIELD_FLOAT),
	DEFINE_FIELD(m_flFanFriction, FIELD_FLOAT),
	DEFINE_FIELD(m_flAttenuation, FIELD_FLOAT),
	DEFINE_FIELD(m_flTargetSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_flMaxSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_angStart, FIELD_VECTOR),
	DEFINE_FIELD(m_flVolume, FIELD_FLOAT),
	DEFINE_KEYFIELD(m_sounds, FIELD_STRING, "sounds"),
	DEFINE_FUNCTION(SpinUp),
	DEFINE_FUNCTION(SpinDown),
	DEFINE_FUNCTION(HurtTouch),
	DEFINE_FUNCTION(RotateFriction),
	DEFINE_FUNCTION(ReverseMove),
	DEFINE_FUNCTION(Rotate),
END_DATADESC()

void CFuncRotating::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "fanfriction"))
	{
		m_flFanFriction = Q_atof(pkvd->szValue) / 100.0f;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spawnorigin"))
	{
		Vector tmp = Q_atov(pkvd->szValue);
		if (tmp != g_vecZero) SetAbsOrigin(tmp);
	}
	else if (FStrEq(pkvd->szKeyName, "sounds"))
	{
		m_sounds = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		m_flVolume = bound(0.0f, Q_atof(pkvd->szValue) / 10.0f, 1.0f);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue(pkvd);
	}
}

void CFuncRotating::Spawn(void)
{
	// maintain compatibility with previous maps
	if (m_flVolume == 0.0) m_flVolume = 1.0;

	// if the designer didn't set a sound attenuation, default to one.
	m_flAttenuation = ATTN_NORM;

	if (FBitSet(pev->spawnflags, SF_ROTATING_SND_SMALL_RADIUS))
	{
		m_flAttenuation = ATTN_IDLE;
	}
	else if (FBitSet(pev->spawnflags, SF_ROTATING_SND_MEDIUM_RADIUS))
	{
		m_flAttenuation = ATTN_STATIC;
	}
	else if (FBitSet(pev->spawnflags, SF_ROTATING_SND_LARGE_RADIUS))
	{
		m_flAttenuation = ATTN_NORM;
	}

	// prevent divide by zero if level designer forgets friction!
	if (m_flFanFriction == 0) m_flFanFriction = 1.0f;

	if (FBitSet(pev->spawnflags, SF_ROTATING_Z_AXIS))
	{
		pev->movedir = Vector(0, 0, 1);
	}
	else if (FBitSet(pev->spawnflags, SF_ROTATING_X_AXIS))
	{
		pev->movedir = Vector(1, 0, 0);
	}
	else
	{
		pev->movedir = Vector(0, 1, 0); // y-axis
	}

	// check for reverse rotation
	if (FBitSet(pev->spawnflags, SF_ROTATING_BACKWARDS))
	{
		pev->movedir = pev->movedir * -1;
	}

	// some rotating objects like fake volumetric lights will not be solid.
	if (FBitSet(pev->spawnflags, SF_ROTATING_NOT_SOLID))
	{
		pev->solid = SOLID_NOT;
		pev->skin = CONTENTS_EMPTY;
		pev->movetype = MOVETYPE_PUSH;
	}
	else
	{
		pev->solid = SOLID_BSP;
		pev->movetype = MOVETYPE_PUSH;
	}

	SET_MODEL(edict(), GetModel());

	// did level designer forget to assign speed?
	m_flMaxSpeed = fabs(pev->speed);
	if (m_flMaxSpeed == 0) m_flMaxSpeed = 100;

	SetLocalAngles(m_vecTempAngles);	// all the child entities is set. Time to move hierarchy
	UTIL_SetOrigin(this, GetLocalOrigin());

	pev->speed = 0;
	m_angStart = GetLocalAngles();
	m_iState = STATE_OFF;

	m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);

	// instant-use brush?
	if (FBitSet(pev->spawnflags, SF_ROTATING_INSTANT))
	{
		SetThink(&CBaseEntity::SUB_CallUseToggle);
		SetNextThink(0.2); // leave a magic delay for client to start up
	}

	// can this brush inflict pain?
	if (FBitSet(pev->spawnflags, SF_ROTATING_HURT))
	{
		SetTouch(&CFuncRotating::HurtTouch);
	}

	Precache();
}

float CFuncRotating::GetNextMoveInterval(void) const
{
	if (m_bStopAtStartPos)
	{
		return gpGlobals->frametime;
	}
	return 0.1f;
}

void CFuncRotating::Precache(void)
{
	// set up fan sounds
	if (!FStringNull(pev->message))
	{
		// if a path is set for a wave, use it
		pev->noise3 = UTIL_PrecacheSound(STRING(pev->message));
	}
	else
	{
		int m_sound = UTIL_LoadSoundPreset(m_sounds);

		// otherwise use preset sound
		switch (m_sound)
		{
			case 1:
				pev->noise3 = UTIL_PrecacheSound("fans/fan1.wav");
				break;
			case 2:
				pev->noise3 = UTIL_PrecacheSound("fans/fan2.wav");
				break;
			case 3:
				pev->noise3 = UTIL_PrecacheSound("fans/fan3.wav");
				break;
			case 4:
				pev->noise3 = UTIL_PrecacheSound("fans/fan4.wav");
				break;
			case 5:
				pev->noise3 = UTIL_PrecacheSound("fans/fan5.wav");
				break;
			case 0:
				pev->noise3 = UTIL_PrecacheSound("common/null.wav");
				break;
			default:
				pev->noise3 = UTIL_PrecacheSound(m_sound);
				break;
		}
	}

	if (GetLocalAvelocity() != g_vecZero && !pev->friction)
	{
		// if fan was spinning, and we went through transition or save/restore,
		// make sure we restart the sound. 1.5 sec delay is magic number. KDB
		SetMoveDone(&CFuncRotating::SpinUp);
		SetMoveDoneTime(0.2);
	}
}

//
// Touch - will hurt others based on how fast the brush is spinning
//
void CFuncRotating::HurtTouch(CBaseEntity *pOther)
{
	entvars_t	*pevOther = pOther->pev;

	// we can't hurt this thing, so we're not concerned with it
	if (!pOther->pev->takedamage) return;

	// calculate damage based on rotation speed
	pev->dmg = GetLocalAvelocity().Length() / 10;

	pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH);

	Vector vecNewVelocity = (pOther->GetAbsOrigin() - Center()).Normalize();
	pOther->SetAbsVelocity(vecNewVelocity * pev->dmg);
}

void CFuncRotating::RampPitchVol(void)
{
	// calc volume and pitch as % of maximum vol and pitch.
	float fpct = fabs(GetLocalAvelocity().Length()) / m_flMaxSpeed;
	float fvol = bound(0.0f, m_flVolume * fpct, 1.0f); // slowdown volume ramps down to 0

	float fpitch = FANPITCHMIN + (FANPITCHMAX - FANPITCHMIN) * fpct;

	int pitch = bound(0, fpitch, 255);

	if (pitch == PITCH_NORM)
	{
		pitch = PITCH_NORM - 1;
	}

	// change the fan's vol and pitch
	EMIT_SOUND_DYN(edict(), CHAN_STATIC, STRING(pev->noise3), fvol, m_flAttenuation, SND_CHANGE_PITCH | SND_CHANGE_VOL, pitch);

}

void CFuncRotating::UpdateSpeed(float flNewSpeed)
{
	float flOldSpeed = pev->speed;
	pev->speed = bound(-m_flMaxSpeed, flNewSpeed, m_flMaxSpeed);

	if (m_bStopAtStartPos)
	{
		int checkAxis = 2;

		// see if we got close to the starting orientation
		if (pev->movedir[0] != 0)
		{
			checkAxis = 0;
		}
		else if (pev->movedir[1] != 0)
		{
			checkAxis = 1;
		}

		float angDelta = anglemod(GetLocalAngles()[checkAxis] - m_angStart[checkAxis]);

		if (angDelta > 180.0f)
			angDelta -= 360.0f;

		if (flNewSpeed < 100)
		{
			if (flNewSpeed <= 25 && fabs(angDelta) < 1.0f)
			{
				m_flTargetSpeed = 0;
				m_bStopAtStartPos = false;
				pev->speed = 0.0f;

				SetLocalAngles(m_angStart);
			}
			else if (fabs(angDelta) > 90.0f)
			{
				// keep rotating at same speed for now
				pev->speed = flOldSpeed;
			}
			else
			{
				float minSpeed = fabs(angDelta);
				if (minSpeed < 20) minSpeed = 20;

				pev->speed = flOldSpeed > 0.0f ? minSpeed : -minSpeed;
			}
		}
	}

	SetLocalAvelocity(pev->movedir * pev->speed);

	if ((flOldSpeed == 0) && (pev->speed != 0))
	{
		// starting to move - emit the sound.
		EMIT_SOUND_DYN(edict(), CHAN_STATIC, STRING(pev->noise3), 0.01f, m_flAttenuation, 0, FANPITCHMIN);
		RampPitchVol();
	}
	else if ((flOldSpeed != 0) && (pev->speed == 0))
	{
		// stopping - stop the sound.
		STOP_SOUND(edict(), CHAN_STATIC, STRING(pev->noise3));
		SetMoveDoneTime(-1);
		m_iState = STATE_OFF;
	}
	else
	{
		// changing speed - adjust the pitch and volume.
		RampPitchVol();
	}
}

//
// SpinUp - accelerates a non-moving func_rotating up to it's speed
//
void CFuncRotating::SpinUp(void)
{
	// calculate our new speed.
	float flNewSpeed = fabs(pev->speed) + 0.2f * m_flMaxSpeed * m_flFanFriction;
	bool bSpinUpDone = false;

	if (fabs(flNewSpeed) >= fabs(m_flTargetSpeed))
	{
		// Reached our target speed.
		flNewSpeed = m_flTargetSpeed;
		bSpinUpDone = !m_bStopAtStartPos;
	}
	else if (m_flTargetSpeed < 0)
	{
		// spinning up in reverse - negate the speed.
		flNewSpeed *= -1;
	}

	m_iState = STATE_TURN_ON;

	// Apply the new speed, adjust sound pitch and volume.
	UpdateSpeed(flNewSpeed);

	// If we've met or exceeded target speed, stop spinning up.
	if (bSpinUpDone)
	{
		SetMoveDone(&CFuncRotating::Rotate);
		Rotate();
	}

	SetMoveDoneTime(GetNextMoveInterval());
}

//-----------------------------------------------------------------------------
// Purpose: Decelerates the rotator from a higher speed to a lower one.
// Input  : flTargetSpeed - Speed to spin down to.
// Output : Returns true if we reached the target speed, false otherwise.
//-----------------------------------------------------------------------------
bool CFuncRotating::SpinDown(float flTargetSpeed)
{
	// Bleed off a little speed due to friction.
	bool bSpinDownDone = false;
	float flNewSpeed = fabs(pev->speed) - 0.1f * m_flMaxSpeed * m_flFanFriction;

	if (flNewSpeed < 0)
	{
		flNewSpeed = 0;
	}

	if (fabs(flNewSpeed) <= fabs(flTargetSpeed))
	{
		// Reached our target speed.
		flNewSpeed = flTargetSpeed;
		bSpinDownDone = !m_bStopAtStartPos;
	}
	else if (pev->speed < 0)
	{
		// spinning down in reverse - negate the speed.
		flNewSpeed *= -1;
	}

	m_iState = STATE_TURN_OFF;

	// Apply the new speed, adjust sound pitch and volume.
	UpdateSpeed(flNewSpeed);

	// If we've met or exceeded target speed, stop spinning down.
	return bSpinDownDone;
}

//
// SpinDown - decelerates a moving func_rotating to a standstill.
//
void CFuncRotating::SpinDown(void)
{
	// If we've met or exceeded target speed, stop spinning down.
	if (SpinDown(m_flTargetSpeed))
	{
		if (m_iState != STATE_OFF)
		{
			SetMoveDone(&CFuncRotating::Rotate);
			Rotate();
		}
	}
	else
	{
		SetMoveDoneTime(GetNextMoveInterval());
	}
}

void CFuncRotating::Rotate(void)
{
	// NOTE: only full speed moving set state to "On"
	if (fabs(pev->speed) == fabs(m_flMaxSpeed))
		m_iState = STATE_ON;

	SetMoveDoneTime(0.1f);

	if (m_bStopAtStartPos)
	{
		SetMoveDoneTime(GetNextMoveInterval());

		int checkAxis = 2;

		// see if we got close to the starting orientation
		if (pev->movedir[0] != 0)
		{
			checkAxis = 0;
		}
		else if (pev->movedir[1] != 0)
		{
			checkAxis = 1;
		}

		float angDelta = anglemod(GetLocalAngles()[checkAxis] - m_angStart[checkAxis]);
		if (angDelta > 180.0f)
			angDelta -= 360.0f;

		Vector avel = GetLocalAvelocity();

		// delta per tick
		Vector avelpertick = avel * gpGlobals->frametime;

		if (fabs(angDelta) < fabs(avelpertick[checkAxis]))
		{
			SetTargetSpeed(0);
			SetLocalAngles(m_angStart);
			m_bStopAtStartPos = false;
		}
	}
}

void CFuncRotating::RotateFriction(void)
{
	// angular impulse support
	if (GetLocalAvelocity() != g_vecZero)
	{
		m_iState = STATE_ON;
		SetMoveDoneTime(0.1);
		RampPitchVol();
	}
	else
	{
		STOP_SOUND(edict(), CHAN_STATIC, STRING(pev->noise3));
		SetMoveDoneTime(-1);
		m_iState = STATE_OFF;
	}
}

void CFuncRotating::ReverseMove(void)
{
	if (SpinDown(0))
	{
		// we've reached zero - spin back up to the target speed.
		SetTargetSpeed(m_flTargetSpeed);
	}
	else
	{
		SetMoveDoneTime(GetNextMoveInterval());
	}
}

void CFuncRotating::SetTargetSpeed(float flSpeed)
{
	if (flSpeed == 0.0f && FBitSet(pev->spawnflags, SF_ROTATING_STOP_AT_START_POS))
		m_bStopAtStartPos = true;

	// make sure the sign is correct - positive for forward rotation,
	// negative for reverse rotation.
	flSpeed = fabs(flSpeed);

	if (pev->impulse)
	{
		flSpeed *= -1;
	}

	m_flTargetSpeed = flSpeed;
	pev->friction = 0.0f; // clear impulse friction

	// If we don't accelerate, change to the new speed instantly.
	if (!FBitSet(pev->spawnflags, SF_ROTATING_ACCDCC))
	{
		UpdateSpeed(m_flTargetSpeed);
		SetMoveDone(&CFuncRotating::Rotate);
	}
	else
	{
		// Otherwise deal with acceleration/deceleration:
		if (((pev->speed > 0) && (m_flTargetSpeed < 0)) || ((pev->speed < 0) && (m_flTargetSpeed > 0)))
		{
			// check for reversing directions.
			SetMoveDone(&CFuncRotating::ReverseMove);
		}
		else if (fabs(pev->speed) < fabs(m_flTargetSpeed))
		{
			// If we are below the new target speed, spin up to the target speed.
			SetMoveDone(&CFuncRotating::SpinUp);
		}
		else if (fabs(pev->speed) > fabs(m_flTargetSpeed))
		{
			// If we are above the new target speed, spin down to the target speed.
			SetMoveDone(&CFuncRotating::SpinDown);
		}
		else
		{
			// we are already at the new target speed. Just keep rotating.
			SetMoveDone(&CFuncRotating::Rotate);
		}
	}

	SetMoveDoneTime(GetNextMoveInterval());
}

//=========================================================
// Rotating Use - when a rotating brush is triggered
//=========================================================
void CFuncRotating::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	m_bStopAtStartPos = false;

	if (useType == USE_SET)
	{
		if (value != 0)
		{
			// never toggle direction from momentary entities
			if (pCaller && !FClassnameIs(pCaller, "momentary_rot_button") && !FClassnameIs(pCaller, "momentary_rot_door"))
				pev->impulse = (value < 0) ? true : false;
			value = fabs(value);
			SetTargetSpeed(bound(0, value, 1) * m_flMaxSpeed);
		}
		else
		{
			SetTargetSpeed(0);
		}
		return;
	}

	// a liitle easter egg
	if (useType == USE_RESET)
	{
		if (value == 0.0f)
		{
			if (m_iState == STATE_OFF)
				pev->impulse = !pev->impulse;
			return;
		}

		if (m_iState == STATE_OFF)
		{
			// apply angular impulse (XashXT feature)
			SetLocalAvelocity(pev->movedir * (bound(-1, value, 1) * m_flMaxSpeed));
			SetMoveDone(&CFuncRotating::RotateFriction);
			pev->friction = 1.0f;
			RotateFriction();
		}
		return;
	}

	if (pev->speed != 0)
		SetTargetSpeed(0);
	else SetTargetSpeed(m_flMaxSpeed);
}

// rotatingBlocked - An entity has blocked the brush
void CFuncRotating::Blocked(CBaseEntity *pOther)
{
	pOther->TakeDamage(pev, pev, pev->dmg, DMG_CRUSH);

	if (gpGlobals->time < pev->dmgtime)
		return;

	pev->dmgtime = gpGlobals->time + 0.5f;
	UTIL_FireTargets(pev->target, this, this, USE_TOGGLE);
}