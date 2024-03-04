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

class CFuncPlat : public CBasePlatTrain
{
	DECLARE_CLASS( CFuncPlat, CBasePlatTrain );
public:
	void Spawn( void );
	void Precache( void );
	void Setup( void );

	virtual void Blocked( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return (CBasePlatTrain :: ObjectCaps()) | FCAP_SET_MOVEDIR; }
	void PlatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void GoToFloor( float floor );
	void FloorCalc( void );	// floor calc

	void CallGoUp( void ) { GoUp(); }
	void CallGoDown( void ) { GoDown(); }
	void CallHitTop( void  ) { HitTop(); }
	void CallHitBottom( void ) { HitBottom(); }
	void CallHitFloor( void ) { HitFloor(); }

	virtual void GoUp( void );
	virtual void GoDown( void );
	virtual void HitTop( void );
	virtual void HitBottom( void );
	virtual void HitFloor( void );

	DECLARE_DATADESC();

	// func_platform routines
	float CalcFloor( void ) { return Q_rint( (( GetLocalOrigin().z - m_vecPosition1.z ) / step( )) + 1 ); }
	float step( void ) { return ( pev->size.z + m_flHeight - 2 ); }
	float ftime( void ) { return ( step() / pev->speed ); } // time to moving between floors
};
