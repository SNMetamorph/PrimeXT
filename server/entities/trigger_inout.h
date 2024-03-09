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
#include "triggers.h"
#include "inout_register.h"

class CTriggerInOut : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerInOut, CBaseTrigger );
public:
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void Think( void );
	int Restore( CRestore &restore );
	virtual void FireOnEntry( CBaseEntity *pOther );
	virtual void FireOnLeaving( CBaseEntity *pOther );
	virtual void OnRemove( void );
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	STATE GetState() { return m_pRegister->IsEmpty() ? STATE_OFF : STATE_ON; }

	string_t m_iszAltTarget;
	string_t m_iszBothTarget;
	CInOutRegister *m_pRegister;
};
