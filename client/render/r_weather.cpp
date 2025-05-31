/*
r_weather.cpp - rain and snow code based on original code by BUzer
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
#include "pm_defs.h"
#include "event_api.h"
#include "triangleapi.h"
#include "custom_alloc.h"
#include "parsemsg.h"
#include "enginefeatures.h"
#include "r_weather.h"
#include "gl_local.h"
#include "gl_debug.h"
#include "matrix.h"
#include <mathlib.h>

void WaterLandingEffect( cl_drip *drip );
void ParseRainFile( void );

struct
{
	int	dripsPerSecond;
	float	distFromPlayer;
	float	windX, windY;
	float	randX, randY;
	int	weatherMode;	// 0 - snow, 1 - rain
	float	globalHeight;
} Rain;

MemBlock<cl_drip>	g_dripsArray( MAXDRIPS );
MemBlock<cl_rainfx>	g_fxArray( MAXFX );

cvar_t		*cl_debug_rain = NULL;

double		rain_curtime;	// current time
double		rain_oldtime;	// last time we have updated drips
double		rain_timedelta;	// difference between old time and current time
double		rain_nextspawntime;	// when the next drip should be spawned
int		dripcounter = 0;
int		fxcounter = 0;

static Vector	rain_mins, rain_maxs; // for vis culling
static Vector	m_vertexarray[MAX_RAIN_VERTICES];
static byte	m_colorarray[MAX_RAIN_VERTICES][4];
static Vector2D	m_coordsarray[MAX_RAIN_VERTICES];
static word	m_indexarray[MAX_RAIN_INDICES];
static int	m_iNumVerts, m_iNumIndex;

/*
=================================
ProcessRain

Must think every frame.
=================================
*/
void ProcessRain( void )
{
	rain_oldtime = rain_curtime; // save old time
	rain_curtime = tr.time;
	rain_timedelta = rain_curtime - rain_oldtime;

	// first frame
	if( rain_oldtime == 0 )
	{
		// fix first frame bug with nextspawntime
		rain_nextspawntime = rain_curtime;
		ParseRainFile();
		return;
	}

	if(( !Rain.dripsPerSecond && g_dripsArray.IsClear( )) || rain_timedelta > 0.1f )
	{
		rain_timedelta = Q_min( rain_timedelta, 0.1f );

		// keep nextspawntime correct
		rain_nextspawntime = rain_curtime;
		return;
	}

	if( rain_timedelta == 0 ) return; // not in pause

	double timeBetweenDrips = 1.0 / (double)Rain.dripsPerSecond;

	if( !g_dripsArray.StartPass( ))
	{
		Rain.dripsPerSecond = 0; // disable rain
		ALERT( at_error, "rain: failed to allocate memory block!\n" );
		return;
	}

	cl_drip *curDrip = g_dripsArray.GetCurrent();

	// save debug info
	float debug_lifetime = 0;
	int debug_howmany = 0;
	int debug_attempted = 0;
	int debug_dropped = 0;

	ClearBounds( rain_mins, rain_maxs );

	while( curDrip != NULL ) // go through list
	{
		if( Rain.weatherMode == MODE_RAIN )
			curDrip->origin.z -= rain_timedelta * DRIPSPEED;
		else if( Rain.weatherMode == MODE_SNOW )
			curDrip->origin.z -= rain_timedelta * SNOWSPEED;
		else return;

		curDrip->origin.x += rain_timedelta * curDrip->xDelta;
		curDrip->origin.y += rain_timedelta * curDrip->yDelta;
#if 1
		// unrolled version of AddPointToBounds (perf)
		if( curDrip->origin[0] < rain_mins[0] ) rain_mins[0] = curDrip->origin[0];
		if( curDrip->origin[0] > rain_maxs[0] ) rain_maxs[0] = curDrip->origin[0];
		if( curDrip->origin[1] < rain_mins[1] ) rain_mins[1] = curDrip->origin[1];
		if( curDrip->origin[1] > rain_maxs[1] ) rain_maxs[1] = curDrip->origin[1];
		if( curDrip->origin[2] < rain_mins[2] ) rain_mins[2] = curDrip->origin[2];
		if( curDrip->origin[2] > rain_maxs[2] ) rain_maxs[2] = curDrip->origin[2];
#else
		AddPointToBounds( curDrip->origin, rain_mins, rain_maxs );
#endif		
		// remove drip if its origin lower than minHeight
		if( curDrip->origin.z < curDrip->minHeight ) 
		{
			if( curDrip->landInWater )
				WaterLandingEffect( curDrip ); // create water rings

			if( cl_debug_rain->value )
			{
				debug_lifetime += (rain_curtime - curDrip->birthTime);
				debug_howmany++;
			}
			
			g_dripsArray.DeleteCurrent();
			dripcounter--;
		}
		else
			g_dripsArray.MoveNext();

		curDrip = g_dripsArray.GetCurrent();
	}

	int maxDelta; // maximum height randomize distance
	float falltime;

	if( Rain.weatherMode == MODE_RAIN )
	{
		maxDelta = DRIPSPEED * rain_timedelta; // for rain
		falltime = (Rain.globalHeight + 4096) / DRIPSPEED;
	}
	else
	{
		maxDelta = SNOWSPEED * rain_timedelta; // for snow
		falltime = (Rain.globalHeight + 4096) / SNOWSPEED;
	}

	while( rain_nextspawntime < rain_curtime )
	{
		rain_nextspawntime += timeBetweenDrips;		

		if( cl_debug_rain->value )
			debug_attempted++;

		// check for overflow
		if( dripcounter < MAXDRIPS )
		{
			float deathHeight = 0.0f;
			Vector vecStart, vecEnd;

			vecStart[0] = RANDOM_FLOAT( RI->view.origin.x - Rain.distFromPlayer, RI->view.origin.x + Rain.distFromPlayer );
			vecStart[1] = RANDOM_FLOAT( RI->view.origin.y - Rain.distFromPlayer, RI->view.origin.y + Rain.distFromPlayer );
			vecStart[2] = Rain.globalHeight;

			float xDelta = Rain.windX + RANDOM_FLOAT( Rain.randX * -1, Rain.randX );
			float yDelta = Rain.windY + RANDOM_FLOAT( Rain.randY * -1, Rain.randY );

			// find a point at bottom of map
			vecEnd[0] = falltime * xDelta;
			vecEnd[1] = falltime * yDelta;
			vecEnd[2] = -4096;

			pmtrace_t pmtrace;
			int contents = CONTENTS_EMPTY;

			// FIXME: use VisTraceLine instead to handle func_water
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, vecStart + vecEnd, PM_STUDIO_IGNORE, -1, &pmtrace );

			// falling to water?
			contents = POINT_CONTENTS( pmtrace.endpos );

			if( pmtrace.startsolid || pmtrace.allsolid )
			{
				if( cl_debug_rain->value )
					debug_dropped++;
				continue; // drip cannot be placed
			}

			if( contents == CONTENTS_WATER )
			{
				int waterEntity = WATER_ENTITY( pmtrace.endpos );
				if( waterEntity > 0 )
				{
					cl_entity_t *pwater = GET_ENTITY( waterEntity );

					if( pwater && ( pwater->model != NULL ))
					{
						deathHeight = pwater->curstate.maxs.z - 1.0f;

						if( !Mod_BoxVisible( pwater->curstate.mins, pwater->curstate.maxs, Mod_GetCurrentVis( )))
							contents = CONTENTS_EMPTY; // not error, just water out of PVS
					}
					else
					{
						ALERT( at_error, "rain: can't get water entity\n");
						continue;
					}
				}
				else
				{
					ALERT( at_error, "rain: water is not func_water entity\n");
					continue;
				}
			}
			else
			{
				deathHeight = pmtrace.endpos.z;
			}

			// just in case..
			if( deathHeight > vecStart.z )
			{
				ALERT( at_error, "rain: can't create drip in water\n");
				continue;
			}


			cl_drip *newClDrip = g_dripsArray.Allocate();

			if( !newClDrip )
			{
				Rain.dripsPerSecond = 0; // disable rain
				ALERT( at_error, "rain: failed to allocate object!\n" );
				return;
			}
			
			vecStart.z -= RANDOM_FLOAT( 0, maxDelta ); // randomize a bit
			
			newClDrip->alpha = RANDOM_FLOAT( 0.12f, 0.2f );
			newClDrip->origin = vecStart;
			
			newClDrip->xDelta = xDelta;
			newClDrip->yDelta = yDelta;
	
			newClDrip->birthTime = rain_curtime; // store time when it was spawned
			newClDrip->minHeight = deathHeight;

			if( contents == CONTENTS_WATER )
				newClDrip->landInWater = 1;
			else
				newClDrip->landInWater = 0;

			// add to first place in chain
			dripcounter++;
		}
		else
		{
			ALERT( at_error, "rain: Drip limit overflow!\n" );
			return;
		}
	}

	if( cl_debug_rain->value )
	{
		// print debug info
		gEngfuncs.Con_NPrintf( 1, "rain info: Drips exist: %i\n", dripcounter );
		gEngfuncs.Con_NPrintf( 2, "rain info: FX's exist: %i\n", fxcounter );
		gEngfuncs.Con_NPrintf( 3, "rain info: Attempted/Dropped: %i, %i\n", debug_attempted, debug_dropped );

		if( debug_howmany )
		{
			float ave = debug_lifetime / (float)debug_howmany;
			gEngfuncs.Con_NPrintf( 4, "rain info: Average drip life time: %f\n", ave );
		}
		else
			gEngfuncs.Con_NPrintf( 4, "rain info: Average drip life time: --\n" );
	}
}

/*
=================================
WaterLandingEffect
=================================
*/
void WaterLandingEffect( cl_drip *drip )
{
	if( fxcounter >= MAXFX )
	{
		ALERT( at_error, "rain: FX limit overflow!\n" );
		return;
	}	
	
	cl_rainfx *newFX = g_fxArray.Allocate();
	if( !newFX )
	{
		ALERT( at_error, "rain: failed to allocate FX object!\n" );
		return;
	}
			
	newFX->alpha = RANDOM_FLOAT( 0.6f, 0.9f );
	newFX->origin = drip->origin; 
	newFX->origin.z = drip->minHeight - 1; // correct position
			
	newFX->birthTime = tr.time;
	newFX->life = RANDOM_FLOAT( 0.7f, 1.0f );

	// add to first place in chain
	fxcounter++;
}

/*
=================================
ProcessFXObjects

Remove all fx objects with out time to live
Call every frame before ProcessRain
=================================
*/
void ProcessFXObjects( void )
{
	if( !g_fxArray.StartPass( ))
	{
		Rain.dripsPerSecond = 0; // disable rain
		ALERT( at_error, "rain: failed to allocate FX object!\n" );
		return;
	}

	cl_rainfx* curFX = g_fxArray.GetCurrent();

	while( curFX != NULL ) // go through FX objects list
	{
		// delete current?
		if(( curFX->birthTime + curFX->life ) < rain_curtime )
		{
			g_fxArray.DeleteCurrent();					
			fxcounter--;
		}
		else
			g_fxArray.MoveNext();

		curFX = g_fxArray.GetCurrent();
	}
}

/*
=================================
ResetRain
clear memory, delete all objects
=================================
*/
void R_ResetWeather( void )
{
	// delete all drips
	g_dripsArray.Clear();
	g_fxArray.Clear();
	
	dripcounter = 0;
	fxcounter = 0;

	R_InitWeather();
}

/*
=================================
InitRain
initialze system
=================================
*/
void R_InitWeather( void )
{
	cl_debug_rain = CVAR_REGISTER( "cl_debug_rain", "0", 0 ); 
	memset( &Rain, 0, sizeof( Rain ));

	rain_nextspawntime = 0;
	rain_oldtime = 0;
	rain_curtime = 0;
}

/*
===========================
ParseRainFile

List of settings:
	drips 	- max raindrips\snowflakes
	distance 	- radius of rain\snow
	windx 	- wind shift X
	windy 	- wind shift Y
	randx 	- random shift X
	randy 	- random shift Y
	mode 	- rain = 0 \ snow =1
	height	- max height to create raindrips\snowflakes
===========================
*/
void ParseRainFile( void )
{
	if( Rain.distFromPlayer || Rain.dripsPerSecond || Rain.globalHeight )
		return;	// already specified?

	char mapname[64];
	char token[64];
	char *afile, *pfile;

	Q_strcpy( mapname, gEngfuncs.pfnGetLevelName( ));
	if( !Q_strlen( mapname ))
	{
		ALERT( at_error, "rain: unable to read map name\n");
		return;
	}

	COM_StripExtension( mapname );
	COM_DefaultExtension( mapname, ".pcs" );

	afile = (char *)gEngfuncs.COM_LoadFile( mapname, 5, NULL);
	pfile = afile;

	// no rain specified for this map
	if( !pfile ) return;

	while( 1 )
	{
		pfile = COM_ParseFile( pfile, token );
		if( !pfile ) break;

		if( !Q_stricmp( token, "drips" )) // dripsPerSecond
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.dripsPerSecond = Q_atoi( token );
		}
		else if( !Q_stricmp( token, "distance" )) // distFromPlayer
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.distFromPlayer = Q_atof( token );
		}
		else if( !Q_stricmp( token, "windx" )) // windX
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.windX = Q_atof( token );
		}
		else if( !Q_stricmp( token, "windy" )) // windY
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.windY = Q_atof( token );		
		}
		else if( !Q_stricmp( token, "randx" )) // randX
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.randX = Q_atof( token );
		}
		else if( !Q_stricmp( token, "randy" )) // randY
		{
			pfile = COM_ParseFile(pfile, token);
			Rain.randY = Q_atof(token);
		}
		else if( !Q_stricmp( token, "mode" )) // weatherMode
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.weatherMode = Q_atoi( token );
		}
		else if( !Q_stricmp( token, "height" )) // globalHeight
		{
			pfile = COM_ParseFile( pfile, token );
			Rain.globalHeight = Q_atof( token );
		}
		else
		{
			ALERT( at_error, "rain: unknown token %s in file %s\n", token, mapname );
		}
	}
	
	gEngfuncs.COM_FreeFile( afile );
}

/*
=================================
R_ParseRainMessage
=================================
*/
void R_ParseRainMessage( void )
{
	Rain.dripsPerSecond = READ_SHORT();
	Rain.distFromPlayer = READ_COORD();
	Rain.windX = READ_COORD();
	Rain.windY = READ_COORD();
	Rain.randX = READ_COORD();
	Rain.randY = READ_COORD();
	Rain.weatherMode = READ_SHORT();
	Rain.globalHeight= READ_COORD();
}

//-----------------------------------------------------
/*
=================================
DrawRain

draw raindrips and snowflakes
=================================
*/
void DrawRain( void )
{
	if( g_dripsArray.IsClear( ))
		return; // no drips to draw

	if( !Mod_BoxVisible( rain_mins, rain_maxs, Mod_GetCurrentVis( )))
		return; // rain volume is invisible

	SpriteHandle hsprTexture;

	if( Rain.weatherMode == MODE_RAIN )
		hsprTexture = LoadSprite( "sprites/raindrop.spr" ); // load rain sprite
	else if( Rain.weatherMode == MODE_SNOW )
		hsprTexture = LoadSprite( "sprites/snowflake.spr" ); // load snow sprite
	else hsprTexture = 0;

	if( !hsprTexture ) return;

	// usual triapi stuff
	const model_s *pTexture = gEngfuncs.GetSpritePointer( hsprTexture );
	if( !gEngfuncs.pTriAPI->SpriteTexture(( struct model_s *)pTexture, 0 ))
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	
	float visibleHeight = Rain.globalHeight - SNOWFADEDIST;

	m_iNumVerts = m_iNumIndex = 0;

	// go through drips list
	g_dripsArray.StartPass();
	cl_drip *Drip = g_dripsArray.GetCurrent();

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, m_vertexarray );

	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, 0, m_coordsarray );

	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_colorarray );

	if( Rain.weatherMode == MODE_RAIN )
	{
		while( Drip != NULL )
		{
			// cull invisible drips
			if( R_CullSphere( Drip->origin, SNOW_SPRITE_HALFSIZE + 1 ))
			{
				g_dripsArray.MoveNext();
				Drip = g_dripsArray.GetCurrent();
				continue;
			}

			if(( m_iNumVerts + 4 ) >= MAX_RAIN_VERTICES )
			{
				// it's time to flush
				if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
					pglDrawRangeElements( GL_TRIANGLES, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
				else pglDrawElements( GL_TRIANGLES, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
				r_stats.c_total_tris += (m_iNumVerts - 2);
				m_iNumVerts = m_iNumIndex = 0;
				r_stats.num_flushes_total++;
			}

			Vector2D toPlayer; 
			toPlayer.x = RI->view.origin[0] - Drip->origin[0];
			toPlayer.y = RI->view.origin[1] - Drip->origin[1];
			toPlayer = toPlayer.Normalize();
	
			toPlayer.x *= DRIP_SPRITE_HALFWIDTH;
			toPlayer.y *= DRIP_SPRITE_HALFWIDTH;

			float shiftX = (Drip->xDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;
			float shiftY = (Drip->yDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;

			byte alpha = Drip->alpha * 255;
			
			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Vector( Drip->origin.x - toPlayer.y - shiftX, Drip->origin.y + toPlayer.x - shiftY, Drip->origin.z + DRIP_SPRITE_HALFHEIGHT );
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			m_coordsarray[m_iNumVerts].x = 0.5f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Vector( Drip->origin.x + shiftX, Drip->origin.y + shiftY, Drip->origin.z - DRIP_SPRITE_HALFHEIGHT );
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set right top corner
			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Vector( Drip->origin.x + toPlayer.y - shiftX, Drip->origin.y - toPlayer.x - shiftY, Drip->origin.z + DRIP_SPRITE_HALFHEIGHT );
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}

		if( m_iNumVerts && m_iNumIndex )
		{
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElements( GL_TRIANGLES, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			else pglDrawElements( GL_TRIANGLES, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			r_stats.c_total_tris += (m_iNumVerts - 2);
			r_stats.num_flushes_total++;
		}
	}
	else if( Rain.weatherMode == MODE_SNOW )
	{ 
		while( Drip != NULL )
		{
			// cull invisible flakes
			if( R_CullSphere( Drip->origin, SNOW_SPRITE_HALFSIZE + 1 ))
			{
				g_dripsArray.MoveNext();
				Drip = g_dripsArray.GetCurrent();
				continue;
			}

			if(( m_iNumVerts + 4 ) >= MAX_RAIN_VERTICES )
			{
				// it's time to flush
				if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
					pglDrawRangeElements( GL_QUADS, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
				else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
				r_stats.c_total_tris += (m_iNumVerts / 2);
				m_iNumVerts = m_iNumIndex = 0;
				r_stats.num_flushes_total++;
			}

			// apply start fading effect
			byte alpha;

			if( Drip->origin.z <= visibleHeight )
				alpha = Drip->alpha * 255;
			else alpha = ((( Rain.globalHeight - Drip->origin.z ) / (float)SNOWFADEDIST ) * Drip->alpha) * 255;

			// set left bottom corner
			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + RI->view.matrix.GetRight() * -SNOW_SPRITE_HALFSIZE + RI->view.matrix.GetUp() * -SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set left top corner
			m_coordsarray[m_iNumVerts].x = 0.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + RI->view.matrix.GetRight() * -SNOW_SPRITE_HALFSIZE + RI->view.matrix.GetUp() * SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set right top corner
			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 0.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + RI->view.matrix.GetRight() * SNOW_SPRITE_HALFSIZE + RI->view.matrix.GetUp() * SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			// set right bottom corner
			m_coordsarray[m_iNumVerts].x = 1.0f;
			m_coordsarray[m_iNumVerts].y = 1.0f;
			m_colorarray[m_iNumVerts][0] = 255;
			m_colorarray[m_iNumVerts][1] = 255;
			m_colorarray[m_iNumVerts][2] = 255;
			m_colorarray[m_iNumVerts][3] = alpha;
			m_vertexarray[m_iNumVerts] = Drip->origin + RI->view.matrix.GetRight() * SNOW_SPRITE_HALFSIZE + RI->view.matrix.GetUp() * -SNOW_SPRITE_HALFSIZE;
			m_indexarray[m_iNumIndex++] = m_iNumVerts++;

			g_dripsArray.MoveNext();
			Drip = g_dripsArray.GetCurrent();
		}

		if( m_iNumVerts && m_iNumIndex )
		{
			// flush any remaining verts
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElements( GL_QUADS, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			r_stats.c_total_tris += (m_iNumVerts / 2);
			r_stats.num_flushes_total++;
		}
	}

	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
}

/*
=================================
DrawFXObjects
=================================
*/
void DrawFXObjects( void )
{
	if( g_fxArray.IsClear( ))
		return; // no objects to draw

	SpriteHandle hsprTexture;

	hsprTexture = LoadSprite( "sprites/waterring.spr" ); // load water ring sprite
	if( !hsprTexture ) return;

	const model_s *pTexture = gEngfuncs.GetSpritePointer( hsprTexture );
	if( !gEngfuncs.pTriAPI->SpriteTexture(( struct model_s *)pTexture, 0 ))
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE ); // because we also want to view water rings underwater
	
	// go through objects list
	g_fxArray.StartPass();
	cl_rainfx *curFX = g_fxArray.GetCurrent();

	m_iNumVerts = m_iNumIndex = 0;

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 0, m_vertexarray );

	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, 0, m_coordsarray );

	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_colorarray );

	while( curFX != NULL )
	{
		if(( m_iNumVerts + 4 ) >= MAX_RAIN_VERTICES )
		{
			// it's time to flush
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElements( GL_QUADS, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
			r_stats.c_total_tris += (m_iNumVerts / 2);
			m_iNumVerts = m_iNumIndex = 0;
			r_stats.num_flushes_total++;
		}

		// cull invisible rings
		if( R_CullSphere( curFX->origin, MAX_RING_HALFSIZE + 1 ))
		{
			g_fxArray.MoveNext();
			curFX = g_fxArray.GetCurrent();
			continue;
		}
					
		// fadeout
		byte alpha = (((curFX->birthTime + curFX->life - rain_curtime) / curFX->life) * curFX->alpha) * 255;
		float size = (rain_curtime - curFX->birthTime) * MAX_RING_HALFSIZE;

		m_coordsarray[m_iNumVerts].x = 0.0f;
		m_coordsarray[m_iNumVerts].y = 0.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x - size, curFX->origin.y - size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		m_coordsarray[m_iNumVerts].x = 0.0f;
		m_coordsarray[m_iNumVerts].y = 1.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x - size, curFX->origin.y + size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		m_coordsarray[m_iNumVerts].x = 1.0f;
		m_coordsarray[m_iNumVerts].y = 1.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x + size, curFX->origin.y + size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		m_coordsarray[m_iNumVerts].x = 1.0f;
		m_coordsarray[m_iNumVerts].y = 0.0f;
		m_colorarray[m_iNumVerts][0] = 255;
		m_colorarray[m_iNumVerts][1] = 255;
		m_colorarray[m_iNumVerts][2] = 255;
		m_colorarray[m_iNumVerts][3] = alpha;
		m_vertexarray[m_iNumVerts] = Vector( curFX->origin.x + size, curFX->origin.y - size, curFX->origin.z );
		m_indexarray[m_iNumIndex++] = m_iNumVerts++;

		g_fxArray.MoveNext();
		curFX = g_fxArray.GetCurrent();
	}

	if( m_iNumVerts && m_iNumIndex )
	{
		// flush any remaining verts
		if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
			pglDrawRangeElements( GL_QUADS, 0, m_iNumVerts, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
		else pglDrawElements( GL_QUADS, m_iNumIndex, GL_UNSIGNED_SHORT, m_indexarray );
		r_stats.c_total_tris += (m_iNumVerts / 2);
		r_stats.num_flushes_total++;
	}

	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );

	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
}

/*
=================================
R_DrawWeather
=================================
*/
void R_DrawWeather( void )
{
	ZoneScoped;

	// it was RP_SKYPORTALVIEW, is it correct?
	if( FBitSet( RI->params, RP_SKYVIEW))
		return;

	GL_DEBUG_SCOPE();
	ProcessRain();
	ProcessFXObjects();
	DrawRain();
	DrawFXObjects();
}
