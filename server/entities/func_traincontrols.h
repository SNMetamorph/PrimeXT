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

// This class defines the volume of space that the player must stand in to control the train
class CFuncTrainControls : public CBaseEntity
{
	DECLARE_CLASS( CFuncTrainControls, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IGNORE_PARENT; }
	void Find( void );
	void Spawn( void );

	DECLARE_DATADESC();
};
