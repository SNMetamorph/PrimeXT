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

#include "func_button.h"

#define SF_ROTBUTTON_PASSABLE		BIT( 0 )
#define SF_ROTBUTTON_ROTATE_BACKWARDS	BIT( 1 )

class CRotButton : public CBaseButton
{
	DECLARE_CLASS( CRotButton, CBaseButton );
public:
	void Spawn( void );
	void Precache( void );
};