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
//  hud.h
//
// class CHud declaration
//
// CHud handles the message, calculation, and drawing the HUD
//
#ifndef HUD_H
#define HUD_H

#define RGB_YELLOWISH	0x00FFA000 // 255, 160, 0
#define RGB_REDISH		0x00FF1010 // 255, 160, 0
#define RGB_GREENISH	0x0000A000 // 0, 160, 0

#include "wrect.h"
#include "port.h"
#include "mathlib.h"
#include "cdll_int.h"
#include "cdll_dll.h"
#include "render_api.h"
#include "enginecallback.h"
#include "randomrange.h"
#include "screenfade.h"
#include "r_view.h"
#include "ammo.h"
#include "color24.h"

typedef struct cvar_s	cvar_t;

#define DHN_DRAWZERO 		1
#define DHN_2DIGITS  		2
#define DHN_3DIGITS  		4
#define MIN_ALPHA	 		100
#define FADE_TIME			100

#define MAX_PLAYERS			64
#define MAX_TEAMS			64
#define MAX_TEAM_NAME		16

#define HUD_ACTIVE			1
#define HUD_INTERMISSION		2

#define MAX_PLAYER_NAME_LENGTH	32
#define MAX_MOTD_LENGTH		1536

#define ROLL_CURVE_ZERO		20	// roll less than this is clamped to zero
#define ROLL_CURVE_LINEAR		90	// roll greater than this is copied out

#define PITCH_CURVE_ZERO		10	// pitch less than this is clamped to zero
#define PITCH_CURVE_LINEAR		45	// pitch greater than this is copied out
					// spline in between
									
//
//-----------------------------------------------------
//
class CHudBase
{
public:
	int m_iFlags; // active, moving,
	virtual ~CHudBase() {}
	virtual int Init( void ) { return 0; }
	virtual int VidInit( void ) { return 0; }
	virtual int Draw(float flTime) { return 0; }
	virtual void Think(void) {}
	virtual void Reset(void) {}
	virtual void InitHUDData( void ) {} // called every time a server is connected to

};

struct HUDLIST
{
	CHudBase	*p;
	HUDLIST	*pNext;
};

//
//-----------------------------------------------------
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "cl_entity.h"

//
//-----------------------------------------------------
//
class CHudAmmo: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Think( void );
	void Reset( void );
	int DrawWList( float flTime );
	int MsgFunc_CurWeapon( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeaponList( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_AmmoX( const char *pszName, int iSize, void *pbuf );

	void SlotInput( int iSlot );
	void _cdecl UserCmd_Slot1( void );
	void _cdecl UserCmd_Slot2( void );
	void _cdecl UserCmd_Slot3( void );
	void _cdecl UserCmd_Slot4( void );
	void _cdecl UserCmd_Slot5( void );
	void _cdecl UserCmd_Slot6( void );
	void _cdecl UserCmd_Slot7( void );
	void _cdecl UserCmd_Slot8( void );
	void _cdecl UserCmd_Slot9( void );
	void _cdecl UserCmd_Slot10( void );
	void _cdecl UserCmd_Close( void );
	void _cdecl UserCmd_NextWeapon( void );
	void _cdecl UserCmd_PrevWeapon( void );
private:
	float	m_fFade;
	WEAPON	*m_pWeapon;
	int	m_HUD_bucket0;
	int	m_HUD_selection;

};

//
//-----------------------------------------------------
//

class CHudAmmoSecondary: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );

	int MsgFunc_SecAmmoVal( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_SecAmmoIcon( const char *pszName, int iSize, void *pbuf );

private:
	enum {
		MAX_SEC_AMMO_VALUES = 4
	};

	int m_HUD_ammoicon; // sprite indices
	int m_iAmmoAmounts[MAX_SEC_AMMO_VALUES];
	float m_fFade;
};


#include "health.h"

//
//-----------------------------------------------------
//
class CHudGeiger: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Geiger( const char *pszName, int iSize, void *pbuf );
private:
	int m_iGeigerRange;

};

//
//-----------------------------------------------------
//
class CHudTrain: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_Train( const char *pszName, int iSize, void *pbuf );
private:
	SpriteHandle m_hSprite;
	int m_iPos;

};

class CHudMOTD : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

	int MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf );

protected:
	char m_szMOTD[MAX_MOTD_LENGTH];
	int MOTD_DISPLAY_TIME;
	float m_flActiveTill;
	int m_iLines;
};

//
//-----------------------------------------------------
//
class CHudScoreboard: public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int DrawPlayers( int xoffset, float listslot, int nameoffset = 0, char *team = NULL ); // returns the ypos where it finishes drawing
	void UserCmd_ShowScores( void );
	void UserCmd_HideScores( void );
	int MsgFunc_ScoreInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamInfo( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_TeamScore( const char *pszName, int iSize, void *pbuf );
	void DeathMsg( int killer, int victim );
	bool IsVisible() const;

	int m_iNumTeams;

	int m_iLastKilledBy;
	int m_fLastKillTime;
	int m_iPlayerNum;
	int m_iShowscoresHeld;

	void GetAllPlayersInfo( void );
};

//
//-----------------------------------------------------
//
class CHudStatusBar : public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	void ParseStatusString( int line_num );

	int MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf );

protected:
	enum {
		MAX_STATUSTEXT_LENGTH = 128,
		MAX_STATUSBAR_VALUES = 8,
		MAX_STATUSBAR_LINES = 2,
	};

	char m_szStatusText[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];  // a text string describing how the status bar is to be drawn
	char m_szStatusBar[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];	// the constructed bar that is drawn
	int m_iStatusValues[MAX_STATUSBAR_VALUES];  // an array of values for use in the status bar

	int m_bReparseString; // set to TRUE whenever the m_szStatusBar needs to be recalculated

	// an array of colors...one color for each line
	float *m_pflNameColors[MAX_STATUSBAR_LINES];
};

struct extra_player_info_t
{
	short frags;
	short deaths;
	short playerclass;
	short teamnumber;
	char teamname[MAX_TEAM_NAME];
};

struct team_info_t
{
	char name[MAX_TEAM_NAME];
	short frags;
	short deaths;
	short ping;
	short packetloss;
	short ownteam;
	short players;
	int already_drawn;
	int scores_overriden;
	int teamnumber;
};

//
//-----------------------------------------------------
//
class CHudDeathNotice : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf );

private:
	int m_HUD_d_skull;  // sprite index of skull icon
};

//
//-----------------------------------------------------
//
class CHudMenu : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	void Reset( void );
	int Draw( float flTime );
	int MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf );

	void SelectMenuItem( int menu_item );

	int m_fMenuDisplayed;
	int m_bitsValidSlots;
	float m_flShutoffTime;
	int m_fWaitingForMore;
};

//
//-----------------------------------------------------
//
class CHudSayText : public CHudBase
{
public:
	int Init( void );
	void InitHUDData( void );
	int VidInit( void );
	int Draw( float flTime );
	int MsgFunc_SayText( const char *pszName, int iSize, void *pbuf );
	void SayTextPrint( const char *pszBuf, int iBufSize, int clientIndex = -1 );
	void EnsureTextFitsInOneLineAndWrapIfHaveTo( int line );
private:

	struct cvar_s *m_HUD_saytext;
	struct cvar_s *m_HUD_saytext_time;
};

//
//-----------------------------------------------------
//
class CHudBattery: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_Battery(const char *pszName,  int iSize, void *pbuf );

private:
	SpriteHandle m_hSprite1;
	SpriteHandle m_hSprite2;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	int m_iBat;
	float m_fFade;
	int m_iHeight;		// width of the battery innards
};

//
//-----------------------------------------------------
//
class CHudFlashlight: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );
	int MsgFunc_Flashlight( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_FlashBat( const char *pszName, int iSize, void *pbuf );

private:
	SpriteHandle m_hSprite1;
	SpriteHandle m_hSprite2;
	SpriteHandle m_hBeam;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	wrect_t *m_prcBeam;
	float m_flBat;
	int m_iBat;
	int m_fOn;
	float m_fFade;
	int m_iWidth; // width of the battery innards
};

//
//-----------------------------------------------------
//
const int maxHUDMessages = 16;

struct message_parms_t
{
	client_textmessage_t *pMessage;
	float time;
	int x, y;
	int totalWidth, totalHeight;
	int width;
	int lines;
	int lineLength;
	int length;
	int r, g, b;
	int text;
	int fadeBlend;
	float charTime;
	float fadeTime;
};

//
//-----------------------------------------------------
//
class CHudTextMessage: public CHudBase
{
public:
	int Init( void );
	static char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size );
	static char *BufferedLocaliseTextString( const char *msg );
	char *LookupString( const char *msg_name, int *msg_dest = NULL );
	int MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
};

//
//-----------------------------------------------------
//
class CHudMessage: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw(float flTime);
	int MsgFunc_HudText(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_GameTitle(const char *pszName, int iSize, void *pbuf);

	float FadeBlend( float fadein, float fadeout, float hold, float localTime );
	int XPosition( float x, int width, int lineWidth );
	int YPosition( float y, int height );

	void MessageAdd( const char *pName, float time );
	void MessageAdd(client_textmessage_t * newMessage );
	void MessageDrawScan( client_textmessage_t *pMessage, float time );
	void MessageScanStart( void );
	void MessageScanNextChar( void );
	void Reset( void );

private:
	client_textmessage_t *m_pMessages[maxHUDMessages];
	float m_startTime[maxHUDMessages];
	message_parms_t m_parms;
	float m_gameTitleTime;
	client_textmessage_t *m_pGameTitle;
	int m_HUD_title_life;
	int m_HUD_title_half;
};

//
//-----------------------------------------------------
//
#define MAX_SPRITE_NAME_LENGTH	32

class CHudStatusIcons: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	void Reset( void );
	int Draw(float flTime);
	int MsgFunc_StatusIcon( const char *pszName, int iSize, void *pbuf );

	enum {
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 4,
	};


	// had to make these public so CHud could access them (to enable concussion icon)
	// could use a friend declaration instead...
	void EnableIcon( char *pszIconName, unsigned char red, unsigned char green, unsigned char blue );
	void DisableIcon( char *pszIconName );

private:
	typedef struct
	{
		char szSpriteName[MAX_ICONSPRITENAME_LENGTH];
		SpriteHandle spr;
		wrect_t rc;
		unsigned char r, g, b;
	} icon_sprite_t;

	icon_sprite_t m_IconList[MAX_ICONSPRITES];
};

//
//-----------------------------------------------------
//
class CHud
{
private:
	HUDLIST *m_pHudList;
	client_sprite_t *m_pSpriteList;
	int m_iSpriteCount;
	int m_iSpriteCountAllRes;
	float m_flMouseSensitivity;
	int m_iConcussionEffect;
	SpriteHandle m_hsprLogo;
	int m_iLogo;
public:
	float m_flTime;	 // the current client time
	float m_fOldTime;	 // the time at which the HUD was last redrawn
	double m_flTimeDelta;// the difference between flTime and fOldTime
	Vector m_vecOrigin;
	Vector m_vecAngles;
	int m_iKeyBits;
	int m_iHideHUDDisplay;
	bool m_zoomMode;
	int m_iFOV;
	int m_Teamplay;
	int m_iRes;
	cvar_t *m_pCvarDraw;
	cvar_t *m_pCvarColorRed;
	cvar_t *m_pCvarColorGreen;
	cvar_t *m_pCvarColorBlue;
	cvar_t *default_fov;
	int	m_iViewModelIndex;
	int m_iCameraMode;
	int m_iFontHeight;
	color24 m_color;

	int DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b );
	int DrawHudString( int x, int y, int iMaxX, char *szString, int r, int g, int b );
	int DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b );
	int DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b );
	int GetNumWidth( int iNumber, int iFlags );

private:
	// the memory for these arrays are allocated in the first call to CHud::VidInit()
	// when the hud.txt and associated sprites are loaded. freed in ~CHud()
	SpriteHandle *m_rghSprites; // the sprites loaded from hud.txt
	wrect_t *m_rgrcRects;
	char *m_rgszSpriteNames;

public:
	SpriteHandle GetSprite( int index ) { return (index < 0) ? 0 : m_rghSprites[index]; }
	wrect_t& GetSpriteRect( int index ) { return m_rgrcRects[index]; }
    int InitHUDMessages( void ); // init hud messages
	int GetSpriteIndex( const char *SpriteName ); // gets a sprite index, for use in the m_rghSprites[] array

	CHudAmmo		m_Ammo;
	CHudHealth	m_Health;
	CHudGeiger	m_Geiger;
	CHudBattery	m_Battery;
	CHudTrain		m_Train;
	CHudFlashlight	m_Flash;
	CHudMessage	m_Message;
	CHudScoreboard	m_Scoreboard;
	CHudStatusBar	m_StatusBar;
	CHudDeathNotice	m_DeathNotice;
	CHudSayText	m_SayText;
	CHudMenu		m_Menu;
	CHudAmmoSecondary	m_AmmoSecondary;
	CHudTextMessage	m_TextMessage;
	CHudStatusIcons	m_StatusIcons;
	CHudMOTD		m_MOTD;

	ViewSmoothingData_t	m_ViewSmoothingData;
	
	void Init( void );
	void VidInit( void );
	void Think(void);
	int Redraw( float flTime, int intermission );
	int UpdateClientData( client_data_t *cdata, float time );

	CHud();
	~CHud();	// destructor, frees allocated memory

	// user messages
	int _cdecl MsgFunc_Damage( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_GameMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Logo( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ResetHUD( const char *pszName,  int iSize, void *pbuf );
	int _cdecl MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_RainData( const char *pszName, int iSize, void *pbuf ); 
	int _cdecl MsgFunc_SetBody( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetSkin( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Particle( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_KillPart( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_KillDecals( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_WeaponAnim( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_MusicFade( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Weapons( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_CustomDecal(const char *pszName, int iSize, void *pbuf);
	int  _cdecl MsgFunc_StudioDecal( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_SetupBones( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_PostFxSettings( const char *pszName, int iSize, void *pbuf );

	// Screen information
	SCREENINFO m_scrinfo;

	byte m_iWeaponBits[MAX_WEAPON_BYTES];
	int m_fPlayerDead;
	int m_iIntermission;

	// sprite indexes
	int m_HUD_number_0;
	
	// error sprite
	int m_HUD_error;
	SpriteHandle m_hHudError;
	
	void AddHudElem( CHudBase *p );
	float GetSensitivity() { return m_flMouseSensitivity; }
	BOOL HasWeapon( int weaponnum ) { return FBitSet( m_iWeaponBits[weaponnum >> 3], BIT( weaponnum & 7 )); }
	void AddWeapon( int weaponnum ) { SetBits( m_iWeaponBits[weaponnum >> 3], BIT( weaponnum & 7 )); }
};

extern CHud		gHUD;
extern hud_player_info_t	g_PlayerInfoList[MAX_PLAYERS+1];	// player info from the engine
extern extra_player_info_t	g_PlayerExtraInfo[MAX_PLAYERS+1];	// additional player info sent directly to the client dll
extern team_info_t		g_TeamInfo[MAX_TEAMS+1];

#endif