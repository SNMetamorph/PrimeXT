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

#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include "pm_movevars.h"
#include "enginefeatures.h"

static byte	*gpBuf;
static const char	*gpszName;
static int	giSize;
static int	giRead;
static bool	giBadRead;

void BEGIN_READ( const char *pszName, void *buf, int size )
{
	giRead = 0;
	giSize = size;
	gpBuf = (byte*)buf;
	gpszName = pszName;
	giBadRead = false;
}

void END_READ( void )
{
	if( giBadRead )
		ALERT( at_warning, "%s was received with errors (read more than %i bytes)\n", gpszName, giSize );
}

int READ_CHAR( void )
{
	int     c;
	
	if( giRead + 1 > giSize )
	{
		giBadRead = true;
		return -1;
	}
		
	c = (signed char)gpBuf[giRead];
	giRead++;
	
	return c;
}

int READ_BYTE( void )
{
	int     c;
	
	if( giRead + 1 > giSize )
	{
		giBadRead = true;
		return -1;
	}
		
	c = (unsigned char)gpBuf[giRead];
	giRead++;
	
	return c;
}

int READ_SHORT( void )
{
	int     c;
	
	if( giRead + 2 > giSize )
	{
		giBadRead = true;
		return -1;
	}
		
	c = (short)( gpBuf[giRead] + ( gpBuf[giRead+1] << 8 ));
	giRead += 2;
	
	return c;
}

int READ_WORD( void )
{
	return (word)READ_SHORT();
}

int READ_LONG( void )
{
	int     c;
	
	if( giRead + 4 > giSize )
	{
		giBadRead = true;
		return -1;
	}
 	c = gpBuf[giRead] + (gpBuf[giRead + 1] << 8) + (gpBuf[giRead + 2] << 16) + (gpBuf[giRead + 3] << 24);
	giRead += 4;
	
	return c;
}

float READ_FLOAT( void )
{
	int c = READ_LONG();

	return *((float *)&c);
}

char* READ_STRING( void )
{
	static char     string[2048];
	int             l, c;

	string[0] = 0;

	l = 0;
	do
	{
		if( giRead + 1 > giSize )
			break; // no more characters

		c = READ_CHAR();
		if( c == 0 ) break;

		string[l] = c;
		l++;
	} while( l < sizeof( string ) - 1 );
	
	string[l] = 0;

	return string;
}

float READ_COORD( void )
{
	// g-cont. we loose precision here but keep old size of coord variable!
	if( g_fRenderInitialized && RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_WRITE_LARGE_COORD )
		return (float)(READ_SHORT());
	return (float)(READ_SHORT() * (1.0f / 8.0f));
}

float READ_ANGLE( void )
{
	return (float)(READ_CHAR() * (360.0f / 256.0f));
}

float READ_HIRESANGLE( void )
{
	return (float)(READ_SHORT() * (360.0f / 65536.0f));
}

void READ_BYTES( byte *out, int count )
{
	for( int i = 0; i < count && !giBadRead; i++ )
	{
		out[i] = READ_BYTE();
	}
}

bool REMAIN_BYTES(void)
{
	return giRead < giSize;
}
