/*
gl_rsurf.cpp - surface-related code
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "gl_local.h"

static int	rtable[MOD_FRAMES][MOD_FRAMES];

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation( msurface_t *s )
{
	texture_t	*base = s->texinfo->texture;
	int	count, reletive = 0;

	if(( RI->currententity != NULL ) && ( RI->currententity->curstate.frame != 0.0f ))
	{
		if( base->alternate_anims )
			base = base->alternate_anims;
	}
	
	if( !base->anim_total )
		return base;

	if( base->name[0] == '-' )
	{
		int tx = (int)((s->texturemins[0] + (base->width << 16)) / base->width) % MOD_FRAMES;
		int ty = (int)((s->texturemins[1] + (base->height << 16)) / base->height) % MOD_FRAMES;
		reletive = rtable[tx][ty] % base->anim_total;
	}
	else
	{
		reletive = (int)(tr.time * 20) % base->anim_total;
	}

	count = 0;

	while( base->anim_min > reletive || base->anim_max <= reletive )
	{
		base = base->anim_next;

		if( !base )
		{
			ALERT( at_error, "R_TextureAnimation: broken loop\n" );
			return s->texinfo->texture;
		}

		if( ++count > MOD_FRAMES )
		{
			ALERT( at_error, "R_TextureAnimation: infinite loop\n" );
			return s->texinfo->texture;
		}
	}

	return base;
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/
void GL_InitRandomTable( void )
{
	int	tu, tv;

	// make random predictable
	RANDOM_SEED( 255 );

	for( tu = 0; tu < MOD_FRAMES; tu++ )
	{
		for( tv = 0; tv < MOD_FRAMES; tv++ )
		{
			rtable[tu][tv] = RANDOM_LONG( 0, 0x7FFF );
		}
	}

	RANDOM_SEED( 0 );
}