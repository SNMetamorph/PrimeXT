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

#include "func_transporter.h"

LINK_ENTITY_TO_CLASS(func_transporter, CFuncTransporter);

BEGIN_DATADESC(CFuncTransporter)
	DEFINE_FIELD(m_flMaxSpeed, FIELD_FLOAT),
	DEFINE_FIELD(m_flWidth, FIELD_FLOAT),
END_DATADESC()

void CFuncTransporter::Spawn(void)
{
	pev->flags |= FL_WORLDBRUSH;
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;

	SET_MODEL(edict(), GetModel());
	SetBits(pev->effects, EF_CONVEYOR);

	// is mapper forgot set angles?
	if (pev->movedir == g_vecZero)
		pev->movedir = Vector(1, 0, 0);

	model_t *bmodel;

	// get a world struct
	if ((bmodel = (model_t *)MODEL_HANDLE(pev->modelindex)) == NULL)
	{
		ALERT(at_warning, "%s: unable to fetch model pointer %s\n", GetDebugName(), GetModel());
		m_flWidth = 64.0f;	// default
	}
	else
	{
		// lookup for the scroll texture
		for (int i = 0; i < bmodel->nummodelsurfaces; i++)
		{
			msurface_t *face = &bmodel->surfaces[bmodel->firstmodelsurface + i];
			if (FBitSet(face->flags, SURF_CONVEYOR))
			{
				m_flWidth = face->texinfo->texture->width;
				break;
			}
		}

		if (m_flWidth <= 0.0)
		{
			ALERT(at_warning, "%s: unable to find scrolling texture\n", GetDebugName());
			m_flWidth = 64.0f;	// default
		}
	}

	if (FBitSet(pev->spawnflags, SF_TRANSPORTER_SOLID))
	{
		if (m_hParent)
			m_pUserData = WorldPhysic->CreateKinematicBodyFromEntity(this);
		else m_pUserData = WorldPhysic->CreateStaticBodyFromEntity(this);
		SetBits(pev->flags, FL_CONVEYOR);
	}
	else
	{
		pev->solid = SOLID_NOT;
		pev->skin = 0; // don't want the engine thinking we've got special contents on this brush
	}

	if (pev->speed == 0)
		pev->speed = 100;

	m_flMaxSpeed = pev->speed;	// save initial speed

	if (FBitSet(pev->spawnflags, SF_TRANSPORTER_STARTON))
		UpdateSpeed(m_flMaxSpeed);
	else UpdateSpeed(0);
	SetNextThink(0);
}

void CFuncTransporter::UpdateSpeed(float speed)
{
	// set conveyor state
	m_iState = (speed != 0) ? STATE_ON : STATE_OFF;
	pev->speed = speed;
}

void CFuncTransporter::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (IsLockedByMaster(pActivator))
		return;

	if (ShouldToggle(useType))
	{
		if (useType == USE_SET)
		{
			UpdateSpeed(value);
		}
		else
		{
			if (GetState() == STATE_ON)
				UpdateSpeed(0);
			else UpdateSpeed(m_flMaxSpeed);
		}
	}
}

void CFuncTransporter::Think(void)
{
	if (pev->speed != 0)
	{
		pev->fuser1 += pev->speed * gpGlobals->frametime * (1.0f / m_flWidth);

		// don't exceed some limits to prevent precision loosing
		while (pev->fuser1 > m_flWidth)
			pev->fuser1 -= m_flWidth;
		while (pev->fuser1 < -m_flWidth)
			pev->fuser1 += m_flWidth;
	}

	// think every frame
	SetNextThink(gpGlobals->frametime);
}