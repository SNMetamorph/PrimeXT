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
#ifndef PHYSCALLBACK_H
#define PHYSCALLBACK_H
#pragma once

#include "physint.h"

// Must be provided by user of this code
extern server_physics_api_t g_physfuncs;

// The actual physic callbacks
#define LINK_ENTITY		(*g_physfuncs.pfnLinkEdict)
#define PHYSICS_TIME	(*g_physfuncs.pfnGetServerTime)
#define HOST_FRAMETIME	(*g_physfuncs.pfnGetFrameTime)
#define MODEL_HANDLE	(*g_physfuncs.pfnGetModel)
#define GET_AREANODE	(*g_physfuncs.pfnGetHeadnode)
#define GET_SERVER_STATE	(*g_physfuncs.pfnServerState)
#define HOST_ERROR		(*g_physfuncs.pfnHost_Error)
#define Tri		(g_physfuncs.pTriAPI)
#define DrawConsoleString	(*g_physfuncs.pfnDrawConsoleString)
#define DrawSetTextColor	(*g_physfuncs.pfnDrawSetTextColor)
#define DrawConsoleStringLen	(*g_physfuncs.pfnDrawConsoleStringLen)
#define WORLD_HANDLE()	MODEL_HANDLE( 1 )
#define GET_LIGHT_STYLE	(*g_physfuncs.pfnGetLightStyle)
#define UPDATE_PACKED_FOG	(*g_physfuncs.pfnUpdateFogSettings)

#endif		//PHYSCALLBACK_H
