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

//
// Render parameters trigger
//
// This entity will copy its render parameters (renderfx, rendermode, rendercolor, renderamt)
// to its targets when triggered.
//


// Flags to indicate masking off various render parameters that are normally copied to the targets
#define SF_RENDER_MASKFX	(1<<0)
#define SF_RENDER_MASKAMT	(1<<1)
#define SF_RENDER_MASKMODE	(1<<2)
#define SF_RENDER_MASKCOLOR	(1<<3)
//LRC
#define SF_RENDER_KILLTARGET	(1<<5)
#define SF_RENDER_ONLYONCE		(1<<6)

//LRC-  RenderFxFader, a subsidiary entity for RenderFxManager
class CRenderFxFader : public CBaseEntity
{
	DECLARE_CLASS( CRenderFxFader, CBaseEntity );
public:
	void Spawn( void );
	void FadeThink( void );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	DECLARE_DATADESC();

	float m_flStartTime;
	float m_flDuration;
	float m_flCoarseness;
	int m_iStartAmt;
	int m_iOffsetAmt;
	Vector m_vecStartColor;
	Vector m_vecOffsetColor;
	float m_fStartScale;
	float m_fOffsetScale;
	EHANDLE m_hTarget;

	int m_iszAmtFactor;
};
