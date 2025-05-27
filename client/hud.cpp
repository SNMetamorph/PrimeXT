//=======================================================================
//			Copyright (C) XashXT Group 2007
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"

void CHud :: Init( void )
{
	InitHUDMessages();

	m_Ammo.Init();
	m_Health.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_Scoreboard.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
 	m_SayText.Init();
	m_Menu.Init();
	m_MOTD.Init();
		
	MsgFunc_ResetHUD( 0, 0, NULL );
}

CHud :: CHud() : 
	m_iSpriteCount(0), 
	m_pHudList(nullptr),
	m_pSpriteList(nullptr),
	m_pCvarDraw(nullptr),
	m_pCvarColorRed(nullptr),
	m_pCvarColorGreen(nullptr),
	m_pCvarColorBlue(nullptr),
	default_fov(nullptr),
	m_rghSprites(nullptr),
	m_rgrcRects(nullptr),
	m_rgszSpriteNames(nullptr)
{
}

CHud :: ~CHud()
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

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
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns -1 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for( int i = 0; i < m_iSpriteCount; i++ )
	{
		if( !Q_strncmp( SpriteName, m_rgszSpriteNames + ( i * MAX_SPRITE_NAME_LENGTH ), MAX_SPRITE_NAME_LENGTH ))
			return i;
	}
	return -1; // invalid sprite
}

void CHud :: VidInit( void )
{
	int j;
	m_scrinfo.iSize = sizeof( m_scrinfo );
	GetScreenInfo( &m_scrinfo );

	// ----------
	// Load Sprites
	// ---------
	m_hHudError = 0;
	
	if( ScreenWidth < 640 )
		m_iRes = 320;
	else m_iRes = 640;
	
	// Only load this once
	if( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList( "sprites/hud.txt", &m_iSpriteCountAllRes );

		if( m_pSpriteList )
		{
			// count the number of sprites of the appropriate res
			client_sprite_t *p = m_pSpriteList;
			m_iSpriteCount = 0;

			for( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new SpriteHandle[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;

			for( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if( p->iRes == m_iRes )
				{
					char sz[256];
					Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
					m_rghSprites[index] = SPR_Load( sz );
					m_rgrcRects[index] = p->rc;
					Q_strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}
				p++;
			}
		}
		else
		{
			ALERT( at_warning, "hud.txt couldn't load\n");
			m_pCvarDraw->value = 0;
			return;
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;

		for( int j = 0, index = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if( p->iRes == m_iRes )
			{
				char sz[256];
				Q_snprintf( sz, sizeof( sz ), "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load( sz );
				index++;
			}
			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );
	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;
	m_zoomMode = false;

	// loading error sprite
	m_HUD_error = GetSpriteIndex( "error" );
	m_hHudError = GetSprite( m_HUD_error );
	
	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_MOTD.VidInit();
	m_Message.VidInit();
	m_Scoreboard.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
}

void CHud::AddHudElem( CHudBase *phudelem )
{
	HUDLIST *pdl, *ptemp;

	if( !phudelem ) return;

	pdl = (HUDLIST *)malloc( sizeof( HUDLIST ));
	if( !pdl ) return;

	memset( pdl, 0, sizeof( HUDLIST ));
	pdl->p = phudelem;

	if( !m_pHudList )
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while( ptemp->pNext )
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}
