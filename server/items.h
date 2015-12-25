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
#ifndef ITEMS_H
#define ITEMS_H

// g-cont. changed to CBaseDelay to allow firing delay
class CItem : public CBaseDelay
{
	DECLARE_CLASS( CItem, CBaseDelay );
public:
	void		Spawn( void );
	CBaseEntity*	Respawn( void );
	void		ItemTouch( CBaseEntity *pOther );
	void		Materialize( void );
	void		StartItem( void );
	virtual BOOL	MyTouch( CBasePlayer *pPlayer ) { return FALSE; };

	DECLARE_DATADESC();

	// g-cont. just for fun
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps()) | FCAP_HOLDABLE_ITEM; }
};

#endif // ITEMS_H

