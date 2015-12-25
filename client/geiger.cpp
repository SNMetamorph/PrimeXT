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
// Geiger.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"

DECLARE_MESSAGE( m_Geiger, Geiger )

int CHudGeiger::Init( void )
{
	HOOK_MESSAGE( Geiger );

	m_iGeigerRange = 0;
	m_iFlags = 0;

	gHUD.AddHudElem( this );

	return 1;
}

int CHudGeiger::VidInit( void )
{
	return 1;
}

int CHudGeiger::MsgFunc_Geiger( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );

	// update geiger data
	m_iGeigerRange = READ_BYTE();
	m_iGeigerRange = m_iGeigerRange << 2;
	
	m_iFlags |= HUD_ACTIVE;

	END_READ();

	return 1;
}

int CHudGeiger::Draw( float flTime )
{
	if( m_iGeigerRange < 1000 && m_iGeigerRange > 0 )
	{
		float flvol;
		int i, pct;

		// peicewise linear is better than continuous formula for this
		if( m_iGeigerRange > 800 )
		{
			pct = 1;
			flvol = 0.2;
			i = 1;
		}
		else if( m_iGeigerRange > 600 )
		{
			pct = 2;
			flvol = 0.4;
			i = 1;
		}
		else if( m_iGeigerRange > 500 )
		{
			pct = 4;
			flvol = 0.5;
			i = 2;
		}
		else if( m_iGeigerRange > 400 )
		{
			pct = 8;
			flvol = 0.6;
			i = 2;
		}
		else if( m_iGeigerRange > 300 )
		{
			pct = 16;
			flvol = 0.7;
			i = 3;
		}
		else if( m_iGeigerRange > 200 )
		{
			pct = 28;
			flvol = 0.78;
			i = 3;
		}
		else if( m_iGeigerRange > 150 )
		{
			pct = 40;
			flvol = 0.80;
			i = 4;
		}
		else if( m_iGeigerRange > 100 )
		{
			pct = 60;
			flvol = 0.85;
			i = 4;
		}
		else if( m_iGeigerRange > 75 )
		{
			pct = 80;
			flvol = 0.9;
			i = 5;
		}
		else if( m_iGeigerRange > 50 )
		{
			pct = 90;
			flvol = 0.95;
			i = 5;
		}
		else
		{
			pct = 95;
			flvol = 1.0;
			i = 6;
		}

		flvol = flvol * RANDOM_FLOAT( 0.25f, 0.5f );

		if( RANDOM_LONG( 0, 100 ) < pct )
		{
			char sz[256];
			Q_snprintf( sz, sizeof( sz ), "player/geiger%d.wav", i );
			PlaySound( sz, flvol );
		}
	}

	return 1;
}
