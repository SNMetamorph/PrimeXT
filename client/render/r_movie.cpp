/*
r_movie.cpp - draw screen movie surfaces
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
#include "r_local.h"
#include "mathlib.h"
#include "event_api.h"

int R_PrecacheCinematic( const char *cinname )
{
	if( !cinname || !*cinname )
		return -1;

	// not AVI file
	if( Q_stricmp( UTIL_FileExtension( cinname ), "avi" ))
		return -1;

	// first check for co-existing
	for( int i = 0; i < MAX_MOVIES; i++ )
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

	ALERT( at_aiconsole, "Loading cinematic %s\n", cinname );
	tr.cinematics[i].state = OPEN_CINEMATIC( tr.cinematics[i].name, true );

	// grab info about movie
	if( tr.cinematics[i].state != NULL )
		CIN_GET_VIDEO_INFO( tr.cinematics[i].state, &tr.cinematics[i].xres, &tr.cinematics[i].yres, &tr.cinematics[i].length );

	return i;
}

void R_InitCinematics( void )
{
	const char *name, *ext;

	// make sure what we have texture to draw cinematics
	if( !tr.world_has_movies ) return;

	for( int i = 1; i < 1024; i++ )
	{
		name = gEngfuncs.pEventAPI->EV_EventForIndex( i );

		if( !name || !*name ) break; // end of events array

		ext = UTIL_FileExtension( name );
		if( Q_stricmp( ext, "avi" )) continue;	// not AVI

		if( R_PrecacheCinematic( name ) == -1 )
			break; // full
	}
}

void R_FreeCinematics( void )
{
	for( int i = 0; i < MAX_MOVIES; i++ )
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
		if( !tr.cinTextures[i] ) break;
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

	if( !tr.cinTextures[i] )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*cinematic%i", i );

		// create new cinematic texture
		// NOTE: dimension of texture is no matter because CIN_UPLOAD_FRAME will be rescale texture
		tr.cinTextures[i] = CREATE_TEXTURE( txName, 256, 256, NULL, txFlags ); 
	}

	return (i+1);
}

int R_DrawCinematic( msurface_t *surf, texture_t *t )
{
	if( !RI.currententity->curstate.body )
	{
		GL_Bind( GL_TEXTURE0, t->gl_texturenum );
		return 0;	// just disabled
	}

	// draw the cinematic
	mextrasurf_t *es = SURF_INFO( surf, RI.currentmodel );

	// found the corresponding cinstate
	const char *cinname = gEngfuncs.pEventAPI->EV_EventForIndex( RI.currententity->curstate.sequence );
	int cinhandle = R_PrecacheCinematic( cinname );

	if( cinhandle >= 0 && es->mirrortexturenum <= 0 )
		es->mirrortexturenum = R_AllocateCinematicTexture( TF_SCREEN );

	if( cinhandle == -1 || es->mirrortexturenum <= 0 || CIN_IS_ACTIVE( tr.cinematics[cinhandle].state ) == false )
	{
		// cinematic textures limit exceeded, so remove SURF_MOVIE flag
		GL_Bind( GL_TEXTURE0, t->gl_texturenum );
		surf->flags &= ~SURF_MOVIE;
		return 0;
	}

	gl_movie_t *cin = &tr.cinematics[cinhandle];
	float cin_time;

	if( RI.currententity->curstate.iuser1 & CF_LOOPED_MOVIE )
	{
		// advances cinematic time
		cin_time = fmod( RI.currententity->curstate.fuser2, cin->length );
	}
	else
	{
		cin_time = RI.currententity->curstate.fuser2;
	}

	// read the next frame
	int cin_frame = CIN_GET_FRAME_NUMBER( cin->state, cin_time );

	if( cin_frame != es->checkcount )
	{
		GL_SelectTexture( GL_TEXTURE0 );

		// upload the new frame
		byte *raw = CIN_GET_FRAMEDATA( cin->state, cin_frame );
		CIN_UPLOAD_FRAME( tr.cinTextures[es->mirrortexturenum-1], cin->xres, cin->yres, cin->xres, cin->yres, raw );
		es->checkcount = cin_frame;
	}
	else
	{
		// have valid cinematic texture
		GL_Bind( GL_TEXTURE0, tr.cinTextures[es->mirrortexturenum-1] );
	}

	return 1;
}
