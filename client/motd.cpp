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
//
// MOTD.cpp
//
// for displaying a server-sent message of the day
//

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"

#define LINE_HEIGHT  	13

DECLARE_MESSAGE( m_MOTD, MOTD );

int CHudMOTD :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( MOTD );

	CVAR_REGISTER( "motd_display_time", "6", 0 );

	m_iFlags &= ~HUD_ACTIVE;  // start out inactive
	m_szMOTD[0] = 0;

	return 1;
}

int CHudMOTD :: VidInit( void )
{
	return 1;
}

void CHudMOTD :: Reset( void )
{
	m_iFlags &= ~HUD_ACTIVE;  // start out inactive
	m_szMOTD[0] = 0;
	m_iLines = 0;
	m_flActiveTill = 0;
}

int CHudMOTD :: Draw( float fTime )
{
	// Draw MOTD line-by-line

	if( m_flActiveTill < gHUD.m_flTime )
	{
		// finished with MOTD, disable it
		m_szMOTD[0] = 0;
		m_iLines = 0;
		m_iFlags &= ~HUD_ACTIVE;
		return 1;
	}

	// cap activetill time to the display time
	m_flActiveTill = min( gHUD.m_flTime + MOTD_DISPLAY_TIME, m_flActiveTill );

	// find the top of where the MOTD should be drawn,  so the whole thing is centered in the screen
	int ypos = max((( ScreenHeight - ( m_iLines * LINE_HEIGHT )) / 2) - 40, 30 ); // shift it up slightly
	char *ch = m_szMOTD;

	while( *ch )
	{
		byte *next_line;
		int line_length = 0;  // count the length of the current line

		for( next_line = (byte *)ch; *next_line != '\n' && *next_line != 0; next_line++ )
			line_length += gHUD.m_scrinfo.charWidths[*next_line];

		char *top = (char *)next_line;
		if( *top == '\n' )
			*top = 0;
		else top = NULL;

		// find where to start drawing the line
		int xpos = (ScreenWidth - line_length) / 2;

		gHUD.DrawHudString( xpos, ypos, ScreenWidth, ch, 255, 180, 0 );

		ypos += LINE_HEIGHT;

		if( top )  // restore 
			*top = '\n';
		ch = (char *)next_line;

		if( *ch == '\n' )
			ch++;

		if( ypos > ( ScreenHeight - 20 ))
			break;  // don't let it draw too low
	}
	return 1;
}

int CHudMOTD :: MsgFunc_MOTD( const char *pszName, int iSize, void *pbuf )
{
	if( m_iFlags & HUD_ACTIVE )
	{
		Reset(); // clear the current MOTD in prep for this one
	}

	BEGIN_READ( pszName, pbuf, iSize );

	int is_finished = READ_BYTE();
	Q_strcat( m_szMOTD, READ_STRING( ));

	if( is_finished )
	{
		m_iFlags |= HUD_ACTIVE;

		MOTD_DISPLAY_TIME = CVAR_GET_FLOAT( "motd_display_time" );

		m_flActiveTill = gHUD.m_flTime + MOTD_DISPLAY_TIME;

		// count the number of lines in the MOTD
		for( char *sz = m_szMOTD; *sz != 0; sz++ )
			if( *sz == '\n' ) m_iLines++;
	}

	END_READ();

	return 1;
}
