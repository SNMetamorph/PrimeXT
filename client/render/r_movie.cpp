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
#include "r_world.h"

int R_PrecacheCinematic( const char *cinname )
{
	int load_sound = 0;
	int i;

	if( !cinname || !*cinname )
		return -1;

	if( *cinname == '*' )
	{
		if( g_iXashEngineBuildNumber >= 4256 )
			load_sound = 1;
		cinname++;
	}

	// not AVI file
	if( Q_stricmp( UTIL_FileExtension( cinname ), "avi" ))
		return -1;

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

	ALERT( at_console, "Loading cinematic %s [%s]\n", cinname, load_sound ? "sound" : "muted" );
	tr.cinematics[i].state = OPEN_CINEMATIC( tr.cinematics[i].name, load_sound );

	// grab info about movie
	if( tr.cinematics[i].state != NULL )
		CIN_GET_VIDEO_INFO( tr.cinematics[i].state, &tr.cinematics[i].xres, &tr.cinematics[i].yres, &tr.cinematics[i].length );

	return i;
}

void R_InitCinematics( void )
{
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

	if( cinhandle == -1 || es->cintexturenum <= 0 || CIN_IS_ACTIVE( tr.cinematics[cinhandle].state ) == false )
	{
		// cinematic textures limit exceeded, so remove SURF_MOVIE flag
		((msurface_t *)surf)->flags &= ~SURF_MOVIE;
		return;
	}

	gl_movie_t *cin = &tr.cinematics[cinhandle];
	float cin_time;

	if( FBitSet( RI->currententity->curstate.iuser1, CF_LOOPED_MOVIE ))
	{
		// advances cinematic time
		cin_time = fmod( RI->currententity->curstate.fuser2, cin->length );
	}
	else
	{
		cin_time = RI->currententity->curstate.fuser2;
	}

	// read the next frame
	int cin_frame = CIN_GET_FRAME_NUMBER( cin->state, cin_time );

	// upload the new frame
	if( cin_frame != es->checkcount )
	{
		GL_SelectTexture( GL_TEXTURE0 ); // doesn't matter. select 0-th unit just as default
		byte *raw = CIN_GET_FRAMEDATA( cin->state, cin_frame );
		CIN_UPLOAD_FRAME( tr.cinTextures[es->cintexturenum-1], cin->xres, cin->yres, cin->xres, cin->yres, raw );
		es->checkcount = cin_frame;
	}
}

void R_UpdateCinSound( cl_entity_t *e )
{
	if( g_iXashEngineBuildNumber < 4256 )
		return; // too old for this feature

	if( !e->curstate.body || !FBitSet( e->curstate.iuser1, CF_MOVIE_SOUND ))
		return; // just disabled

	// found the corresponding cinstate
	const char *cinname = gRenderfuncs.GetFileByIndex( e->curstate.sequence );
	int cinhandle = R_PrecacheCinematic( cinname );

	if( cinhandle == -1 || CIN_IS_ACTIVE( tr.cinematics[cinhandle].state ) == false )
		return;

	gl_movie_t *cin = &tr.cinematics[cinhandle];
	float cin_time;

	if( FBitSet( e->curstate.iuser1, CF_LOOPED_MOVIE ))
	{
		// advances cinematic time
		cin_time = fmod( e->curstate.fuser2, cin->length );
	}
	else
	{
		cin_time = e->curstate.fuser2;
	}

	// stream avi sound
	CIN_UPDATE_SOUND( cin->state, e->index, VOL_NORM, ATTN_IDLE, cin_time );
}

