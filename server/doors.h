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
#ifndef DOORS_H
#define DOORS_H

// doors
#define SF_DOOR_START_OPEN			BIT( 0 )
#define SF_DOOR_ROTATE_BACKWARDS		BIT( 1 )
#define SF_DOOR_ONOFF_MODE			BIT( 2 )	// was Door don't link from quake. This feature come from Spirit. Thx Laurie
#define SF_DOOR_PASSABLE			BIT( 3 )
#define SF_DOOR_ONEWAY			BIT( 4 )
#define SF_DOOR_NO_AUTO_RETURN		BIT( 5 )
#define SF_DOOR_ROTATE_Z			BIT( 6 )
#define SF_DOOR_ROTATE_X			BIT( 7 )
#define SF_DOOR_USE_ONLY			BIT( 8 )	// door must be opened by player's use button.
#define SF_DOOR_NOMONSTERS			BIT( 9 )	// monster can't open

#define SF_DOOR_SILENT			BIT( 31 )

#endif//DOORS_H
