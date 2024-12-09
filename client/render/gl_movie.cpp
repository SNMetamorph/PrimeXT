/*
gl_movie.cpp - draw screen movie surfaces
Copyright (C) 2011 Uncle Mike

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
#include "gl_world.h"
#include "mathlib.h"
#include "event_api.h"
#include <stringlib.h>

int R_PrecacheCinematic( const char *cinname )
{
	if( !cinname || !*cinname )
		return -1;

	// not AVI file
	const char *ext = UTIL_FileExtension( cinname );
	if( Q_stricmp( ext, "avi" ) && Q_stricmp( ext, "webm" )) // with ffmpeg we don't really have a limit here
		return -1;

	int i;
	// first check for co-existing
	for( i = 0; i < MAX_MOVIES; i++ )
	{
		if( !Q_stricmp( tr.cinematics[i].name, cinname ))
		{
			// already existed
			return i;
		}
	}

	// found an empty slot
	for( i = 0; i < MAX_MOVIES; i++ )
	{
		if( !tr.cinematics[i].name[0] )
			break;
	}

	if( i == MAX_MOVIES )
	{
		ALERT( at_error, "R_PrecacheCinematic: cinematic list limit exceeded\n" );
		return -1;
	}

	// register new cinematic
	Q_strncpy( tr.cinematics[i].name, cinname, sizeof( tr.cinematics[0].name ));
	if( tr.cinematics[i].state )
	{
		ALERT( at_warning, "Reused cin state %i with %s\n", i, tr.cinematics[i].name );
		FREE_CINEMATIC( tr.cinematics[i].state );
	}

	ALERT( at_console, "Loading cinematic %s [%s]\n", cinname, "sound" );

	// FIXME: engine is hardcoded to load file in media/ folder, must be fixed on engine side
	const char *p = tr.cinematics[i].name;
	if( !Q_strnicmp( p, "media/", 6 ))
		p += 6;

	tr.cinematics[i].state = (movie_state_s *)OPEN_CINEMATIC( p, true );

	// grab info about movie
	if( tr.cinematics[i].state != NULL )
		CIN_GET_VIDEO_INFO( tr.cinematics[i].state, &tr.cinematics[i].xres, &tr.cinematics[i].yres, &tr.cinematics[i].length );

	return i;
}

void R_InitCinematics( void )
{
	// a1ba: this function is useless lmao
	// it's called before WORLD_HAS_MOVIES bit set
	const char *name, *ext;

	// make sure what we have texture to draw cinematics
	if( !FBitSet( world->features, WORLD_HAS_MOVIES ))
		return;

	for( int i = 1; i < 1024; i++ )
	{
		name = gRenderfuncs.GetFileByIndex( i );

		if( !name || !*name ) break; // end of files array

		ext = UTIL_FileExtension( name );
		if( Q_stricmp( ext, "avi" )) continue;	// not AVI

		if( R_PrecacheCinematic( name ) == -1 )
			break; // full
	}
}

void R_FreeCinematics( void )
{
	int i;
	for( i = 0; i < MAX_MOVIES; i++ )
	{
		if( tr.cinematics[i].state )
		{
			ALERT( at_notice, "release cinematic %s\n", tr.cinematics[i].name );
			FREE_CINEMATIC( tr.cinematics[i].state );
		}
	}

	memset( tr.cinematics, 0, sizeof( tr.cinematics ));

	for( i = 0; i < MAX_MOVIE_TEXTURES; i++ )
	{
		if( !tr.cinTextures[i].Initialized() ) break;
		FREE_TEXTURE( tr.cinTextures[i] );
	}

	memset( tr.cinTextures, 0, sizeof( tr.cinTextures ));
}

int R_AllocateCinematicTexture( unsigned int txFlags )
{
	int i = tr.num_cin_used;

	if( i >= MAX_MOVIE_TEXTURES )
	{
		ALERT( at_error, "R_AllocateCinematicTexture: cine textures limit exceeded!\n" );
		return 0; // disable
	}
	tr.num_cin_used++;

	if( !tr.cinTextures[i].Initialized() )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*cinematic%i", i );

		// create new cinematic texture
		// NOTE: dimension of texture is no matter because CIN_UPLOAD_FRAME will be rescale texture
		tr.cinTextures[i] = CREATE_TEXTURE( txName, 256, 256, NULL, txFlags ); 
	}

	return (i+1);
}

void R_UpdateCinematic( const msurface_t *surf )
{
	if( !RI->currententity->curstate.body )
		return; // just disabled

	// draw the cinematic
	mextrasurf_t *es = surf->info;

	// found the corresponding cinstate
	const char *cinname = gRenderfuncs.GetFileByIndex( RI->currententity->curstate.sequence );
	int cinhandle = R_PrecacheCinematic( cinname );

	if( cinhandle >= 0 && es->cintexturenum <= 0 )
		es->cintexturenum = R_AllocateCinematicTexture( TF_NOMIPMAP );

	// a1ba: isn't this kinda stupid? If movie isn't active anymore, we will never draw movie on it again
	if( cinhandle == -1 || es->cintexturenum <= 0 || CIN_IS_ACTIVE( tr.cinematics[cinhandle].state ) == false )
	{
		// cinematic textures limit exceeded, so remove SURF_MOVIE flag
		((msurface_t *)surf)->flags &= ~SURF_MOVIE;
		return;
	}

	gl_movie_t *cin = &tr.cinematics[cinhandle];

	if( cin->finished )
		return;

	if( !cin->texture_set )
	{
		CIN_SET_PARM( cin->state,
			AVI_RENDER_TEXNUM, tr.cinTextures[es->cintexturenum-1],
			AVI_RENDER_W, cin->xres,
			AVI_RENDER_H, cin->yres,
			AVI_PARM_LAST );
		cin->texture_set = true;
	}
}

void R_UpdateCinSound( cl_entity_t *e )
{
	if( !e->curstate.body || !FBitSet( e->curstate.iuser1, CF_MOVIE_SOUND ))
		return; // just disabled

	// found the corresponding cinstate
	const char *cinname = gRenderfuncs.GetFileByIndex( e->curstate.sequence );
	int cinhandle = R_PrecacheCinematic( cinname );

	if( cinhandle == -1 || CIN_IS_ACTIVE( tr.cinematics[cinhandle].state ) == false )
		return;

	gl_movie_t *cin = &tr.cinematics[cinhandle];

	if( cin->finished )
		return;

	if( !cin->sound_set )
	{
		CIN_SET_PARM( cin->state,
			AVI_ENTNUM, e->index,
			AVI_VOLUME, static_cast<int>( VOL_NORM * 255 ),
			AVI_ATTN, ATTN_NORM,
			AVI_PARM_LAST );
		cin->sound_set = true;
	}

	if( !CIN_THINK( cin->state )) // TODO: make a video manager that will call this each frame
	{
		if( FBitSet( RI->currententity->curstate.iuser1, CF_LOOPED_MOVIE ))
			CIN_SET_PARM( cin->state, AVI_REWIND, AVI_PARM_LAST );
		else cin->finished = true;
	}
}
