/*
conprint.cpp - extended printf function that allows
colored printing scheme from Quake3
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <stdio.h>
#include <basetypes.h>
#include <math.h>
#include <cmdlib.h>
#include "port.h"
#include "stringlib.h"
#include "conprint.h"

#define GAMMA		( 2.2f )		// Valve Software gamma
#define INVGAMMA		( 1.0f / 2.2f )	// back to 1.0
#define TEXGAMMA		( 0.7f )
#define TEXINTENSITY	( 1.2f )

static float	texturetolinear[256];	// texture (0..255) to linear (0..1)
static int	lineartotexture[1024];	// linear (0..1) to texture (0..255)
static byte	s_gammatable[256];
static byte	s_intensitytable[256];

void BuildGammaTable( void )
{
	int	i, inf;
	float	g;

	g = TEXGAMMA;

	for( i = 0; i < 256; i++ )
	{
		// convert from nonlinear texture space (0..255) to linear space (0..1)
		texturetolinear[i] =  pow( i / 255.0f, INVGAMMA );

		if( g == 1.0f ) inf = i;
		else inf = 255 * pow( i / 255.0f, 1.0f / g ) + 0.5f;
		s_gammatable[i] = bound( 0, inf, 255 );

		inf = i * TEXINTENSITY;
		s_intensitytable[i] = bound( 0, inf, 255 );
	}

	for( i = 0; i < 1024; i++ )
	{
		// convert from linear space (0..1) to nonlinear texture space (0..255)
		lineartotexture[i] =  pow( i / 1023.0, INVGAMMA ) * 255;
	}
}

// convert texture to linear 0..1 value
float TextureToLinear( int c ) { return texturetolinear[bound( 0, c, 255 )]; }

// convert texture to linear 0..1 value
int LinearToTexture( float f ) { return lineartotexture[bound( 0, (int)(f * 1023), 1023 )]; }

byte TextureLightScale( byte c ) { return s_gammatable[s_intensitytable[c]]; }
