/*
cl_env_dynlight.cpp - client wrapper class for env_dynlight entity
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "cl_env_dynlight.h"
#include "gl_studio.h"

CEntityEnvDynlight::CEntityEnvDynlight(cl_entity_t *ent)
{
    m_pEntity = ent;
}

bool CEntityEnvDynlight::Cinematic() const
{
    return m_pEntity->curstate.renderfx == kRenderFxCinemaLight;
}

bool CEntityEnvDynlight::Spotlight() const
{
    return m_pEntity->curstate.scale > 0.0f;
}

bool CEntityEnvDynlight::DisableShadows() const
{
    return m_pEntity->curstate.effects & EF_NOSHADOW;
}

bool CEntityEnvDynlight::DisableBump() const
{
    return m_pEntity->curstate.effects & EF_NOBUMP;
}

bool CEntityEnvDynlight::EnableLensFlare() const
{
    return m_pEntity->curstate.effects & EF_LENSFLARE;
}

int CEntityEnvDynlight::GetSpotlightTextureIndex() const
{
    return bound(0, m_pEntity->curstate.rendermode, 7);
}

int CEntityEnvDynlight::GetVideoFileIndex() const
{
    return m_pEntity->curstate.sequence;
}

int CEntityEnvDynlight::GetLightstyleIndex() const
{
    return m_pEntity->curstate.skin & 0x3F;
}

float CEntityEnvDynlight::GetFOVAngle() const
{
    return m_pEntity->curstate.scale;
}

float CEntityEnvDynlight::GetRadius() const
{
    return m_pEntity->curstate.renderamt * 8.0f;
}

float CEntityEnvDynlight::GetBrightness() const
{
    return m_pEntity->curstate.framerate;
}

float CEntityEnvDynlight::GetCinTime() const
{
    return m_pEntity->curstate.fuser2;
}

Vector CEntityEnvDynlight::GetColor() const
{
    return Vector(
        m_pEntity->curstate.rendercolor.r / 255.f, 
        m_pEntity->curstate.rendercolor.g / 255.f,
        m_pEntity->curstate.rendercolor.b / 255.f
    );
}

void CEntityEnvDynlight::GetLightVectors(Vector &origin, Vector &angles) const
{
	// fill default case
	origin = m_pEntity->origin;
	angles = m_pEntity->angles;

	// try to grab position from attachment
	if (m_pEntity->curstate.aiment > 0 && m_pEntity->curstate.movetype == MOVETYPE_FOLLOW)
	{
		cl_entity_t *pParent = GET_ENTITY(m_pEntity->curstate.aiment);
		studiohdr_t *pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata(pParent->model);

		if (pParent && pParent->model && pStudioHeader != NULL)
		{
			// make sure what model really has attachements
			if (m_pEntity->curstate.body > 0 && (pStudioHeader && pStudioHeader->numattachments > 0))
			{
				int num = bound(1, m_pEntity->curstate.body, MAXSTUDIOATTACHMENTS);
				R_StudioAttachmentPosAng(pParent, num - 1, &origin, &angles);
			}
			else if (pParent->curstate.movetype == MOVETYPE_STEP)
			{
				origin = pParent->origin;
				angles = pParent->angles;

				// add the eye position for monster
				if (pParent->curstate.eflags & EFLAG_SLERP)
					origin += pStudioHeader->eyeposition;
			}
			else
			{
				origin = pParent->origin;
				angles = pParent->angles;
			}
		}
	}
	// all other parent types will be attached on the server
}
