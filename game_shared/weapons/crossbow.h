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
#include "weapon_context.h"
#include "weapon_layer.h"

#define WEAPON_CROSSBOW			6
#define CROSSBOW_WEIGHT			10
#define CROSSBOW_MAX_CLIP		5
#define CROSSBOW_DEFAULT_GIVE	5
#define CROSSBOW_CLASSNAME		weapon_crossbow

class CCrossbowWeaponLogic : public CBaseWeaponContext
{
public:
	CCrossbowWeaponLogic() = delete;
	~CCrossbowWeaponLogic() = default;
	CCrossbowWeaponLogic(std::unique_ptr<IWeaponLayer> &&layer);

	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);

	void FireBolt( void );
	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	bool Deploy( void );
	void Holster( void );
	void Reload( void );
	void WeaponIdle( void );

	bool m_fInZoom; // don't save this
	uint16_t m_usCrossbow;
	uint16_t m_usCrossbow2;
};
