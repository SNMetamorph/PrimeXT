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

class CGlow : public CPointEntity
{
	DECLARE_CLASS( CGlow, CPointEntity );
public:
	void Spawn( void );
	void Think( void );
	void Animate( float frames );

	DECLARE_DATADESC();

	float m_lastTime;
	float m_maxFrame;
};
