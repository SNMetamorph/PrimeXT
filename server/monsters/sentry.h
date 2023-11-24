/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
/*

===== sentry.h ========================================================

*/

#pragma once

#include "baseturret.h"

class CSentry : public CBaseTurret
{
	DECLARE_CLASS( CSentry, CBaseTurret );
public:
	void Spawn( );
	void Precache(void);

	// other functions
	void Shoot(Vector &vecSrc, Vector &vecDirToEnemy);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void SentryTouch( CBaseEntity *pOther );
	void SentryDeath( void );

	DECLARE_DATADESC();
};