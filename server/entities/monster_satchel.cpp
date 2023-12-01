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

#include "monster_satchel.h"

LINK_ENTITY_TO_CLASS(monster_satchel, CSatchelCharge);

BEGIN_DATADESC(CSatchelCharge)
	DEFINE_FUNCTION(SatchelThink),
	DEFINE_FUNCTION(SatchelSlide),
END_DATADESC()

//=========================================================
// Deactivate - do whatever it is we do to an orphaned 
// satchel when we don't want it in the world anymore.
//=========================================================
void CSatchelCharge::Deactivate(void)
{
	pev->solid = SOLID_NOT;
	UTIL_Remove(this);
}

void CSatchelCharge::Spawn(void)
{
	Precache();
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_satchel.mdl");
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));	// Uses point-sized, and can be stepped over

	SetTouch(&CSatchelCharge::SatchelSlide);
	SetUse(&CGrenade::DetonateUse);
	SetThink(&CSatchelCharge::SatchelThink);
	pev->nextthink = gpGlobals->time + 0.1;

	pev->gravity = 0.5;
	pev->friction = 0.8;

	pev->dmg = gSkillData.plrDmgSatchel;
	// ResetSequenceInfo( );
	pev->sequence = 1;
}

void CSatchelCharge::SatchelSlide(CBaseEntity *pOther)
{
	entvars_t	*pevOther = pOther->pev;

	// don't hit the guy that launched this grenade
	if (pOther->edict() == pev->owner)
		return;

	// SetLocalAvelocity( Vector( 300, 300, 300 ));
	pev->gravity = 1;// normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() - Vector(0, 0, 10), ignore_monsters, edict(), &tr);

	if (tr.flFraction < 1.0)
	{
		Vector vecVelocity = GetAbsVelocity();
		Vector vecAvelocity = GetLocalAvelocity();
		// add a bit of static friction
		SetAbsVelocity(vecVelocity * 0.95);
		SetAbsAvelocity(vecAvelocity * 0.9);
		// play sliding sound, volume based on velocity
	}
	if (!(pev->flags & FL_ONGROUND) && GetAbsVelocity().Length2D() > 10)
	{
		BounceSound();
	}
	StudioFrameAdvance();
}

void CSatchelCharge::SatchelThink(void)
{
	StudioFrameAdvance();
	pev->nextthink = gpGlobals->time + 0.1;

	if (!IsInWorld())
	{
		UTIL_Remove(this);
		return;
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		Vector vecVelocity = GetAbsVelocity() * 0.8f;
		Vector vecAvelocity = GetLocalAvelocity() * 0.9f;
		vecVelocity.z += 8.0f;
		SetAbsVelocity(vecVelocity);
		SetAbsAvelocity(vecAvelocity);
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		Vector vecVelocity = GetAbsVelocity();
		vecVelocity.z -= 8.0f;
		SetAbsVelocity(vecVelocity);
	}
}

void CSatchelCharge::Precache(void)
{
	PRECACHE_MODEL("models/grenade.mdl");
	PRECACHE_SOUND("weapons/g_bounce1.wav");
	PRECACHE_SOUND("weapons/g_bounce2.wav");
	PRECACHE_SOUND("weapons/g_bounce3.wav");
}

void CSatchelCharge::BounceSound(void)
{
	switch (RANDOM_LONG(0, 2))
	{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce1.wav", 1, ATTN_NORM);	break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce2.wav", 1, ATTN_NORM);	break;
		case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce3.wav", 1, ATTN_NORM);	break;
	}
}
