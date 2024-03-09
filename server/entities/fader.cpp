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

#include "fader.h"

LINK_ENTITY_TO_CLASS( fader, CRenderFxFader );

BEGIN_DATADESC( CRenderFxFader )
	DEFINE_FIELD( m_flStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flDuration, FIELD_FLOAT ),
	DEFINE_FIELD( m_flCoarseness, FIELD_FLOAT ),
	DEFINE_FIELD( m_iStartAmt, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOffsetAmt, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecStartColor, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecOffsetColor, FIELD_VECTOR ),
	DEFINE_FIELD( m_fStartScale, FIELD_FLOAT),
	DEFINE_FIELD( m_fOffsetScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),
	DEFINE_FUNCTION( FadeThink ),
END_DATADESC()

void CRenderFxFader :: Spawn( void )
{
	SetThink( &CRenderFxFader::FadeThink );
}

void CRenderFxFader :: FadeThink( void )
{
	if ( m_hTarget == NULL )
	{
		SUB_Remove();
		return;
	}

	float flDegree = (gpGlobals->time - m_flStartTime) / m_flDuration;

	if (flDegree >= 1)
	{
		m_hTarget->pev->renderamt = m_iStartAmt + m_iOffsetAmt;
		m_hTarget->pev->rendercolor = m_vecStartColor + m_vecOffsetColor;
		m_hTarget->pev->scale = m_fStartScale + m_fOffsetScale;

		SUB_UseTargets( m_hTarget, USE_TOGGLE, 0 );

		if (pev->spawnflags & SF_RENDER_KILLTARGET)
		{
			m_hTarget->SetThink( &CBaseEntity::SUB_Remove );
			m_hTarget->SetNextThink( 0.1 );
		}

		m_hTarget = NULL;

		SetNextThink( 0.1 );
		SetThink( &CBaseEntity::SUB_Remove );
	}
	else
	{
		m_hTarget->pev->renderamt = m_iStartAmt + m_iOffsetAmt * flDegree;

		m_hTarget->pev->rendercolor.x = m_vecStartColor.x + m_vecOffsetColor.x * flDegree;
		m_hTarget->pev->rendercolor.y = m_vecStartColor.y + m_vecOffsetColor.y * flDegree;
		m_hTarget->pev->rendercolor.z = m_vecStartColor.z + m_vecOffsetColor.z * flDegree;

		m_hTarget->pev->scale = m_fStartScale + m_fOffsetScale * flDegree;

		SetNextThink( m_flCoarseness );
	}
}
