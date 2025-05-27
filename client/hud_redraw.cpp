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

#include "hud.h"
#include "utils.h"

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};

void CHud::Think( void )
{
	HUDLIST *pList = m_pHudList;

	while( pList )
	{
		if( pList->p->m_iFlags & HUD_ACTIVE )
			pList->p->Think();
		pList = pList->pNext;
	}

	if (m_iFOV == 0 || !m_zoomMode)
	{
		// let players only adjust up in fov, and only if they are not overriden by zoom
		m_iFOV = Q_max(default_fov->value, 90);
	}

	m_color.r = static_cast<uint8_t>(m_pCvarColorRed->value);
	m_color.g = static_cast<uint8_t>(m_pCvarColorGreen->value);
	m_color.b = static_cast<uint8_t>(m_pCvarColorBlue->value);
}

int CHud :: Redraw( float flTime, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;

	// Clock was reset, reset delta
	if( m_flTimeDelta < 0 ) m_flTimeDelta = 0;

	m_iIntermission = intermission;

	if( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while( pList )
		{
			if( !intermission )
			{
				if(( pList->p->m_iFlags & HUD_ACTIVE ) && !( m_iHideHUDDisplay & HIDEHUD_ALL ))
					pList->p->Draw( flTime );
			}
			else
			{
				// it's an intermission, so only draw hud elements that are set
				// to draw during intermissions
				if( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}
			pList = pList->pNext;
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if( m_iLogo )
	{
		int x, y, i;

		if( m_hsprLogo == 0 )
			m_hsprLogo = LoadSprite( "sprites/%d_logo.spr" );

		SPR_Set( m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width( m_hsprLogo, 0 );
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0) / 2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive( i, x, y, NULL );
	}

 	return 1;
}

int CHud :: DrawHudString( int xpos, int ypos, int iMaxX, char *szString, int r, int g, int b )
{
	// draw the string until we hit the null character or a newline character
	for( byte *szIt = (byte *)szString; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int next = xpos + gHUD.m_scrinfo.charWidths[*szIt]; // variable-width fonts look cool
		if( next > iMaxX )
			return xpos;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
		xpos = next;		
	}
	return xpos;
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];

	Q_snprintf( szString, sizeof( szString ), "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	byte *szIt;
	// find the end of the string
	for( szIt = (byte *)szString; *szIt != 0; szIt++ )
	{
		// we should count the length?		
	}

	// iterate throug the string in reverse
	for( szIt--; szIt != (byte *)(szString - 1); szIt-- )	
	{
		int next = xpos - gHUD.m_scrinfo.charWidths[*szIt]; // variable-width fonts look cool
		if( next < iMinX )
			return xpos;
		xpos = next;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
	}
	return xpos;
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b )
{
	int iWidth = GetSpriteRect( m_HUD_number_0 ).right - GetSpriteRect( m_HUD_number_0 ).left;
	int k;
	
	if( iNumber > 0 )
	{
		// SPR_Draw 100's
		if( iNumber >= 100 )
		{
			k = iNumber/100;
			SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 + k ));
			x += iWidth;
		}
		else if( iFlags & ( DHN_3DIGITS ))
		{
			x += iWidth;
		}

		// SPR_Draw 10's
		if( iNumber >= 10 )
		{
			k = (iNumber % 100) / 10;
			SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect( m_HUD_number_0 + k ));
			x += iWidth;
		}
		else if( iFlags & ( DHN_3DIGITS | DHN_2DIGITS ))
		{
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set( GetSprite( m_HUD_number_0 + k ), r, g, b );
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 + k ));
		x += iWidth;
	} 
	else if( iFlags & DHN_DRAWZERO ) 
	{
		SPR_Set( GetSprite( m_HUD_number_0 ), r, g, b );

		// SPR_Draw 100's
		if( iFlags & ( DHN_3DIGITS ))
		{
			x += iWidth;
		}

		if( iFlags & ( DHN_3DIGITS|DHN_2DIGITS ))
			x += iWidth;

		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect( m_HUD_number_0 ));
		x += iWidth;
	}
	return x;
}

int CHud :: GetNumWidth( int iNumber, int iFlags )
{
	if( iFlags & ( DHN_3DIGITS ))
		return 3;

	if( iFlags & ( DHN_2DIGITS ))
		return 2;

	if( iNumber <= 0 )
	{
		if( iFlags & ( DHN_DRAWZERO ))
			return 1;
		return 0;
	}

	if( iNumber < 10 )
		return 1;

	if( iNumber < 100 )
		return 2;

	return 3;
}
