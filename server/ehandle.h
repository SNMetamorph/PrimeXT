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

#ifndef EHANDLE_H
#define EHANDLE_H
#include "const.h"

//
// EHANDLE. Safe way to point to CBaseEntities who may die between frames
//
class CBaseEntity;
class EHANDLE
{
private:
	edict_t	*m_pent;
	int	m_serialnumber;
public:
	edict_t *Get( void );
	edict_t *Set( edict_t *pent );
	CBaseEntity *GetPointer();

	operator int ( void );

	operator CBaseEntity *( void );

	CBaseEntity *operator = (CBaseEntity *pEntity);
	CBaseEntity *operator ->( void );
};

#endif//EHANDLE_H
