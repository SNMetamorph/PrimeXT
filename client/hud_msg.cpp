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
//  hud_msg.cpp
//
#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"
#include "r_weather.h"
#include "r_efx.h"

// CHud message handlers
DECLARE_HUDMESSAGE( Logo );
DECLARE_HUDMESSAGE( HUDColor );
DECLARE_HUDMESSAGE( SetFog );
DECLARE_HUDMESSAGE( RainData );
DECLARE_HUDMESSAGE( SetBody );
DECLARE_HUDMESSAGE( SetSkin );
DECLARE_HUDMESSAGE( ResetHUD );
DECLARE_HUDMESSAGE( InitHUD );
DECLARE_HUDMESSAGE( ViewMode );
DECLARE_HUDMESSAGE( Particle );
DECLARE_HUDMESSAGE( KillPart );
DECLARE_HUDMESSAGE( SetFOV );
DECLARE_HUDMESSAGE( Concuss );
DECLARE_HUDMESSAGE( GameMode );
DECLARE_HUDMESSAGE( MusicFade );
DECLARE_HUDMESSAGE( WeaponAnim );
DECLARE_HUDMESSAGE( KillDecals );

int CHud :: InitHUDMessages( void )
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( HUDColor );
	HOOK_MESSAGE( Particle );
	HOOK_MESSAGE( KillPart );
	HOOK_MESSAGE( SetFog );
	HOOK_MESSAGE( RainData ); 
	HOOK_MESSAGE( SetBody );
	HOOK_MESSAGE( SetSkin );
	HOOK_MESSAGE( MusicFade );
	HOOK_MESSAGE( WeaponAnim );
	HOOK_MESSAGE( KillDecals );

	m_iFOV = 0;
	m_iHUDColor = 0x00FFA000; // 255,160,0
	
	CVAR_REGISTER( "zoom_sensitivity_ratio", "1.2", 0 );
	default_fov = CVAR_REGISTER( "default_fov", "90", 0 );
	m_pCvarDraw = CVAR_REGISTER( "hud_draw", "1", FCVAR_ARCHIVE );
	m_pSpriteList = NULL;

	// clear any old HUD list
	if( m_pHudList )
	{
		HUDLIST *pList;

		while( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	m_flTime = 1.0;

	return 1;
}

int CHud :: MsgFunc_ResetHUD( const char *pszName, int iSize, void *pbuf )
{
	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p )
			pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	// reset fog
	m_fStartDist = 0;
	m_fEndDist = 0;

	return 1;
}

int CHud :: MsgFunc_Logo( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	// update logo data
	m_iLogo = READ_BYTE();

	END_READ();

	return 1;
}

int CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	return 1;
}

int CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	m_fStartDist = 0;
	m_fEndDist = 0;

	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}

	return 1;
}

int CHud::MsgFunc_SetFOV( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int newfov = READ_BYTE();
	float def_fov = CVAR_GET_FLOAT( "default_fov" );

	if( newfov == 0 )
		m_iFOV = def_fov;
	else m_iFOV = newfov;

	if( m_iFOV == def_fov )
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		m_flMouseSensitivity = CVAR_GET_FLOAT( "sensitivity" );

		// scale by zoom ratio
		m_flMouseSensitivity *= ((float)newfov / def_fov) * CVAR_GET_FLOAT( "zoom_sensitivity_ratio" );
	}

	END_READ();

	return 1;
}

int CHud::MsgFunc_HUDColor( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	m_iHUDColor = READ_LONG();
	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_vecFogColor.x = (float)READ_BYTE() * (1.0f / 255.0f);
	m_vecFogColor.y = (float)READ_BYTE() * (1.0f / 255.0f);
	m_vecFogColor.z = (float)READ_BYTE() * (1.0f / 255.0f);

	m_fStartDist = (float)READ_SHORT();
          m_fEndDist = (float)READ_SHORT();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_Teamplay = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_Damage( const char *pszName, int iSize, void *pbuf )
{
	int	armor, blood;
	Vector	from;
	float	count;

	BEGIN_READ( pszName, pbuf, iSize );

	armor = READ_BYTE();
	blood = READ_BYTE();

	for( int i = 0; i < 3; i++ )
		from[i] = READ_COORD();

	count = (blood * 0.5f) + (armor * 0.5f);
	if( count < 10 ) count = 10;

	END_READ();
	
	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	m_iConcussionEffect = READ_BYTE();

	if( m_iConcussionEffect )
		m_StatusIcons.EnableIcon("dmg_concuss", 255, 160, 0 );
	else m_StatusIcons.DisableIcon( "dmg_concuss" );

	END_READ();

	return 1;
}

int CHud :: MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	gEngfuncs.GetViewModel()->curstate.body = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	gEngfuncs.GetViewModel()->curstate.skin = READ_BYTE();

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_WeaponAnim( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int sequence = READ_BYTE();
	int body = READ_BYTE();
	float framerate = READ_BYTE() * 0.125f;

	END_READ();

	cl_entity_t *view = gEngfuncs.GetViewModel();

	// call weaponanim with same body
	gEngfuncs.pfnWeaponAnim( sequence, body );

	if( framerate > 0.0f )
		view->curstate.framerate = framerate;
	else view->curstate.framerate = 1.0f;
#if 0
	// just for test Glow Shell effect
	view->curstate.renderfx = kRenderFxGlowShell;
	view->curstate.rendercolor.r = 255;
	view->curstate.rendercolor.g = 255;
	view->curstate.rendercolor.b = 255;
	view->curstate.renderamt = 100;
#endif	
	return 1;
}

int CHud :: MsgFunc_Particle( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int entindex = READ_SHORT();
	char *sz = READ_STRING();
	int attachment = READ_BYTE();

	COM_DefaultExtension( sz, ".aur" );

	UTIL_CreateAurora( GET_ENTITY( entindex ), sz, attachment );

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_KillPart( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	int entindex = READ_SHORT();

	UTIL_RemoveAurora( GET_ENTITY( entindex ));

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_KillDecals( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	int entityIndex = READ_SHORT();

	g_StudioRenderer.RemoveAllDecals( entityIndex );

	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( entityIndex );

	if( ent->model && ent->model->type == mod_brush )
	{
		REMOVE_BSP_DECALS( ent->model );
	}

	END_READ();
	
	return 1;
}

int CHud :: MsgFunc_RainData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	R_ParseRainMessage();

	END_READ();	

	return 1;
}

int CHud :: MsgFunc_MusicFade( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	MUSIC_FADE_VOLUME( (float)READ_SHORT() / 100.0f );

	END_READ();

	return 1;
}
