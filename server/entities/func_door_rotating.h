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

#include "func_door.h"

class CRotDoor : public CBaseDoor
{
	DECLARE_CLASS( CRotDoor, CBaseDoor );
public:
	void Spawn( void );
	virtual void SetToggleState( int state );
	virtual bool IsRotatingDoor() { return true; }
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_SET_MOVEDIR; }
	void OnChangeParent( void );
	void OnClearParent( void );
};