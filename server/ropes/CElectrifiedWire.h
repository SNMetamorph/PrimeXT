/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
#ifndef GAME_SERVER_ENTITIES_ROPE_CELECTRIFIEDWIRE_H
#define GAME_SERVER_ENTITIES_ROPE_CELECTRIFIEDWIRE_H

#include "CRope.h"

/**
*	An electrified wire.
*	Can be toggled on and off. Starts on.
*/
class CElectrifiedWire : public CRope
{
public:
	DECLARE_CLASS( CElectrifiedWire, CRope );
	DECLARE_DATADESC();

	CElectrifiedWire();
	
	void KeyValue( KeyValueData* pkvd );

	void Precache();

	void Spawn();

	void Think();

	void Use( CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float flValue );
	bool IsActive() const { return m_bIsActive; }
	bool ShouldDoEffect( const int iFrequency );
	void DoSpark( const int iSegment, const bool bExertForce );
	void DoLightning();
private:
	bool m_bIsActive;

	int m_iTipSparkFrequency;
	int m_iBodySparkFrequency;
	int m_iLightningFrequency;

	int m_iXJoltForce;
	int m_iYJoltForce;
	int m_iZJoltForce;

	int m_iNumUninsulatedSegments;
	int m_iUninsulatedSegments[MAX_SEGMENTS];
	int m_iLightningSprite;

	float m_flLastSparkTime;
};

#endif //GAME_SERVER_ENTITIES_ROPE_CELECTRIFIEDWIRE_H
