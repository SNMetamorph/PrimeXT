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
#include "plats.h"
#include "func_platform.h"

class CFuncPlatRot : public CFuncPlat
{
	DECLARE_CLASS( CFuncPlatRot, CFuncPlat );
public:
	void Spawn( void );
	void SetupRotation( void );

	virtual void GoUp( void );
	virtual void GoDown( void );
	virtual void HitTop( void );
	virtual void HitBottom( void );
	
	void RotMove( Vector &destAngle, float time );

	DECLARE_DATADESC();

	Vector m_end, m_start;
};
