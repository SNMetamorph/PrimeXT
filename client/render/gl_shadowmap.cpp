/*
gl_shadowmap.cpp - render shadowmaps for directional lights
Copyright (C) 2018 Uncle Mike

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
#include "const.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>
#include "gl_studio.h"
#include "gl_sprite.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"
#include "pm_defs.h"

/*
=============================================================================

  SHADOWMAP ALLOCATION

=============================================================================
*/
static void SM_InitBlock( void )
{
	gl_shadowmap_t *sms = &tr.shadowmap;
	memset( sms->allocated, 0, sizeof( sms->allocated ));
	sms->shadowmap.Init( FBO_DEPTH, SHADOW_SIZE, SHADOW_SIZE );
}

static int SM_AllocBlock( unsigned short w, unsigned short h, unsigned short *x, unsigned short *y )
{
	gl_shadowmap_t	*sms = &tr.shadowmap;
	unsigned short	i, j, best, best2;

	best = SHADOW_SIZE;

	for( i = 0; i < SHADOW_SIZE - w; i++ )
	{
		best2 = 0;

		for( j = 0; j < w; j++ )
		{
			if( sms->allocated[i+j] >= best )
				break;
			if( sms->allocated[i+j] > best2 )
				best2 = sms->allocated[i+j];
		}

		if( j == w )
		{	
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	// atlas is full
	if( best + h > SHADOW_SIZE )
		return false;

	for( i = 0; i < w; i++ )
		sms->allocated[*x + i] = best + h;

	return true;
}

/*
================
R_RenderShadowScene

fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadow( mworldlight_t *wl )
{
	if( !wl->shadow_w || !wl->shadow_h )
	{
		wl->shadow_w = 256;
		wl->shadow_h = 256;
		SM_AllocBlock( wl->shadow_w, wl->shadow_h, &wl->shadow_x, &wl->shadow_y );
	}
}
