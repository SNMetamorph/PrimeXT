//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "utils.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "r_particle.h"
#include "r_weather.h"
#include "r_local.h"

void HUD_DrawNormalTriangles( void )
{
}

void HUD_DrawTransparentTriangles( void )
{
	if( !g_fRenderInitialized )
	{
		gEngfuncs.GetViewAngles( (float *)RI->viewangles );
		tr.time = GET_CLIENT_TIME();
		tr.oldtime = GET_CLIENT_OLDTIME();
	}

	if( !tr.fCustomRendering )
	{
		AngleVectors( RI->viewangles, RI->vforward, RI->vright, RI->vup );
		tr.frametime = tr.time - tr.oldtime;
	}

	if( g_pParticleSystems )
		g_pParticleSystems->UpdateSystems();

	if( g_fRenderInitialized )
		R_DrawWeather();
}
