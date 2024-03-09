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

//===========================================================
//LRC - trigger_inout, a trigger which fires _only_ when
// the player enters or leaves it.
//   If there's more than one entity it can trigger off, then
// it will trigger for each one that enters and leaves.
//===========================================================
class CTriggerInOut;

class CInOutRegister : public CPointEntity
{
	DECLARE_CLASS( CInOutRegister, CPointEntity );
public:
	// returns true if found in the list
	BOOL IsRegistered ( CBaseEntity *pValue );
	// remove all invalid entries from the list, trigger their targets as appropriate
	// returns the new list
	CInOutRegister *Prune( void );
	// adds a new entry to the list
	CInOutRegister *Add( CBaseEntity *pValue );
	BOOL IsEmpty( void ) { return m_pNext ? FALSE : TRUE; };

	DECLARE_DATADESC();

	CTriggerInOut *m_pField;
	CInOutRegister *m_pNext;
	EHANDLE m_hValue;
};
