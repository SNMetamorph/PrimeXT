/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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

#ifndef PARSEMSG_H
#define PARSEMSG_H
#include "basetypes.h"

// Macros to hook function calls into the HUD object
#define HOOK_MESSAGE(x)	gEngfuncs.pfnHookUserMsg(#x, __MsgFunc_##x );

#define DECLARE_MESSAGE(y, x) int __MsgFunc_##x(const char *pszName, int iSize, void *pbuf) \
{ \
	return gHUD.y.MsgFunc_##x(pszName, iSize, pbuf ); \
}

#define DECLARE_HUDMESSAGE(x) int __MsgFunc_##x(const char *pszName, int iSize, void *pbuf) \
{ \
	return gHUD.MsgFunc_##x(pszName, iSize, pbuf ); \
}

#define HOOK_COMMAND(x, y) ADD_COMMAND( x, __CmdFunc_##y );
#define DECLARE_COMMAND(y, x) void __CmdFunc_##x( void ) \
{ \
	gHUD.y.UserCmd_##x( ); \
}

extern void BEGIN_READ( const char *pszName, void *buf, int size );
extern void END_READ( void );
extern int READ_CHAR( void );
extern int READ_BYTE( void );
extern int READ_SHORT( void );
extern int READ_WORD( void );
extern int READ_LONG( void );
extern float READ_FLOAT( void );
extern char *READ_STRING( void );
extern float READ_COORD( void );
extern float READ_ANGLE( void );
extern float READ_HIRESANGLE( void );
extern void READ_BYTES( byte *out, int count );
extern bool REMAIN_BYTES(void);

#endif//PARSEMSG_H
