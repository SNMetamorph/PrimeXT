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
//
//  cdll_dll.h

// this file is included by both the game-dll and the client-dll,

#ifndef CDLL_DLL_H
#define CDLL_DLL_H

#define WEAPON_SUIT			31
#define MAX_WEAPONS			32

#define MAX_WEAPON_SLOTS		5	// hud item selection slots
#define MAX_ITEM_TYPES		6	// hud item selection slots
#define MAX_ITEMS			5	// hard coded item types

#define HIDEHUD_WEAPONS		( 1<<0 )
#define HIDEHUD_FLASHLIGHT		( 1<<1 )
#define HIDEHUD_ALL			( 1<<2 )
#define HIDEHUD_HEALTH		( 1<<3 )

#define MAX_AMMO_TYPES		32
#define MAX_AMMO_SLOTS  		32

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE		2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4

// decal flags
#define FDECAL_PERMANENT		0x01	// This decal should not be removed in favor of any new decals
#define FDECAL_CUSTOM		0x02	// This is a custom clan logo and should not be saved/restored
#define FDECAL_DONTSAVE		0x04	// Decal was loaded from adjacent level, don't save it for this level
#define FDECAL_CLIPTEST		0x08	// Decal needs to be clip-tested
#define FDECAL_NOCLIP		0x10	// Decal is not clipped by containing polygon
#define FDECAL_USESAXIS		0x20	// Uses the s axis field to determine orientation (footprints)
#define FDECAL_STUDIO		0x40	// Indicates a studio decal
#define FDECAL_LOCAL_SPACE		0x80	// Decal is in local space (any decal after serialization)

#endif
