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

#include "base_tank.h"
#include "env_laser.h"

class CFuncTankLaser : public CBaseTank
{
	DECLARE_CLASS( CFuncTankLaser, CBaseTank );
public:
	void	Activate( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Fire( const Vector &barrelEnd, const Vector &forward, entvars_t *pevAttacker );
	void	Think( void );
	CLaser	*GetLaser( void );

	DECLARE_DATADESC();
private:
	CLaser	*m_pLaser;
	float	m_laserTime;
};