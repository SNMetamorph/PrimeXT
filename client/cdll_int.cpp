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
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include <mathlib.h>
#include "usercmd.h"
#include "entity_state.h"
#include "cdll_exp.h"
#include "tri.h"
#include "mobility_int.h"
#include "imgui_manager.h"
#include "meshdesc_factory.h"
#include "events/game_event_manager.h"
#include "weapon_predicting_context.h"
#include "pm_shared.h"
#include "filesystem_utils.h"
#include "build_info.h"

int developer_level;
int g_iXashEngineBuildNumber;
cl_enginefunc_t gEngfuncs;
mobile_engfuncs_t gMobileAPI;
render_api_t gRenderfuncs;
static CGameEventManager *g_pEventManager = nullptr;
static CWeaponPredictingContext *g_pWeaponPredicting = nullptr;
CHud gHUD;

/*
==========================
    Initialize

Called when the DLL is first loaded.
==========================
*/
/*
================================
HUD_GetHullBounds

Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch( hullnumber )
	{
	case 0:	// Normal player
		Vector( -16, -16, -36 ).CopyToArray( mins );
		Vector(  16,  16,  36 ).CopyToArray( maxs );
		iret = 1;
		break;
	case 1:	// Crouched player
		Vector( -16, -16, -18 ).CopyToArray( mins );
		Vector(  16,  16,  18 ).CopyToArray( maxs );
		iret = 1;
		break;
	case 2:	// Point based hull
		g_vecZero.CopyToArray( mins );
		g_vecZero.CopyToArray( maxs );
		iret = 1;
		break;
	}
	return iret;
}

/*
================================
HUD_ConnectionlessPacket

Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.
Incoming, it holds the max size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int HUD_ConnectionlessPacket( const struct netadr_s *, const char *, char *, int *response_buffer_size )
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void HUD_PlayerMoveInit( struct playermove_s *ppmove )
{
	PM_Init( ppmove );
}

char HUD_PlayerMoveTexture( char *name )
{
	return '\0';
}

extern "C" void DLLEXPORT HUD_PlayerMove(struct playermove_s *ppmove, int server)
{
	PM_Move( ppmove, server );
}

int Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
{
	gEngfuncs = *pEnginefuncs;

	if( iVersion != CLDLL_INTERFACE_VERSION )
		return 0;

	memcpy( &gEngfuncs, pEnginefuncs, sizeof( cl_enginefunc_t ));

	// get developer level
	developer_level = (int)CVAR_GET_FLOAT( "developer" );

	if (!CVAR_GET_POINTER("host_clientloaded")) {
		// TODO maybe should warn that only Xash3D supported?
		return 0;	// Not a Xash3D engine
	}

	g_iXashEngineBuildNumber = (int)CVAR_GET_FLOAT( "build" ); // 0 for old builds or GoldSrc
	if( g_iXashEngineBuildNumber <= 0 )
		g_iXashEngineBuildNumber = (int)CVAR_GET_FLOAT( "buildnum" );

	if (!fs::Initialize())
	{
		ALERT(at_error, "Failed to initialize filesystem inteface in client library\n");
		return 0;
	}

	ALERT(at_console, "Running PrimeXT: built ^1%s^7, commit ^2%s^7, architecture ^3%s^7, platform ^5%s\n", 
		BuildInfo::GetDate(),
		BuildInfo::GetCommitHash(),
		BuildInfo::GetArchitecture(),
		BuildInfo::GetPlatform()
	);

	g_pEventManager = new CGameEventManager();
	return 1;
}

/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/
extern "C" int DLLEXPORT HUD_VidInit( void )
{
	gHUD.VidInit();
	R_VidInit();
	g_ImGuiManager.VidInitialize();
	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all
the hud variables.
==========================
*/
void DLLEXPORT HUD_Init( void )
{
	InitInput();
	gHUD.Init();

	if (g_fRenderInitialized)
		GL_Init();
}

void DLLEXPORT HUD_Shutdown( void )
{
	ShutdownInput();

	if (g_fRenderInitialized)
		GL_Shutdown();

	if (g_pEventManager) 
	{
		delete g_pEventManager;
		g_pEventManager = nullptr;
	}

	if (g_pWeaponPredicting) 
	{
		delete g_pWeaponPredicting;
		g_pWeaponPredicting = nullptr;
	}
}

/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/
extern "C" int DLLEXPORT HUD_Redraw(float time, int intermission)
{
	return gHUD.Redraw( time, intermission );
}

/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/
extern "C" int DLLEXPORT HUD_UpdateClientData( client_data_t *pcldata, float flTime )
{
	return gHUD.UpdateClientData( pcldata, flTime );
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/
extern "C" void DLLEXPORT HUD_Reset( void )
{
	gHUD.VidInit();
}

/*
========================
HUD_MobilityInterface
========================
*/
extern "C" int DLLEXPORT HUD_MobilityInterface(mobile_engfuncs_t *mobileapi)
{
	if (mobileapi->version != MOBILITY_API_VERSION)
	{
		gEngfuncs.Con_Printf("Client Error: Mobile API version mismatch. Got: %i, want: %i\n",
			mobileapi->version, MOBILITY_API_VERSION);
		return 1;
	}
	gMobileAPI = *mobileapi;
	return 0;
}

#ifdef _WIN32

static int *VGUI_GetRect( void )
{
	static int extent[4];
	RECT wrect;

	if( GetWindowRect( GetActiveWindow(), &wrect ))
          {
		if( !wrect.left )
		{
			// fullscreen
			extent[0] = wrect.left;
			extent[1] = wrect.top;
			extent[2] = wrect.right;
			extent[3] = wrect.bottom;
		}
		else
		{
			// window
			extent[0] = wrect.left + 4;
			extent[1] = wrect.top + 30;
			extent[2] = wrect.right - 4;
			extent[3] = wrect.bottom - 4;
		}
	}
	return extent;	
}
#else

/*
=================
VGUI_GetRect

VGui stub
=================
*/
int *VGUI_GetRect( void )
{
	static int extent[4];

	extent[0] = gEngfuncs.GetWindowCenterX() - ScreenWidth / 2;
	extent[1] = gEngfuncs.GetWindowCenterY() - ScreenHeight / 2;
	extent[2] = gEngfuncs.GetWindowCenterX() + ScreenWidth / 2;
	extent[3] = gEngfuncs.GetWindowCenterY() + ScreenHeight / 2;

	return extent;
}

#endif

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/
extern "C" void DLLEXPORT HUD_Frame( double time )
{
	// run anti (_-=ZhekA=-_) system for Xash3D engine
	gEngfuncs.VGui_ViewportPaintBackground( VGUI_GetRect( ));
}

extern "C" int DLLEXPORT HUD_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
	return g_ImGuiManager.KeyInput(down != 0, keynum, pszCurrentBinding) ? 1 : 0;
}

extern "C" void DLLEXPORT HUD_PostRunCmd(local_state_t *from, local_state_t *to, usercmd_t *cmd, int runfuncs, double time, unsigned int random_seed)
{
	if (CVAR_GET_FLOAT("cl_lw") > 0.0f) {
		g_pWeaponPredicting->PostThink(from, to, cmd, runfuncs != 0, time, random_seed);
	}
}

/*
==========================
HUD_ClipMoveToEntity

This called only for non-local clients (multiplayer)
==========================
*/
extern "C" void DLLEXPORT HUD_ClipMoveToEntity(physent_t * pe, const float *start, float *mins, float *maxs, const float *end, pmtrace_t * tr)
{
	// convert physent_t to cl_entity_t
	cl_entity_t *pTouch = gEngfuncs.GetEntityByIndex(pe->info);
	trace_t trace;

	if (!pTouch)
	{
		// removed entity?
		tr->allsolid = false;
		return;
	}

	// make trace default
	memset(&trace, 0, sizeof(trace));
	trace.allsolid = true;
	trace.fraction = 1.0f;
	trace.endpos = end;

	Physic_SweepTest(pTouch, start, mins, maxs, end, &trace);

	// convert trace_t into pmtrace_t
	tr->allsolid = trace.allsolid;
	tr->startsolid = trace.startsolid;
	tr->inopen = trace.inopen;
	tr->inwater = trace.inwater;
	tr->fraction = trace.fraction;
	tr->endpos = trace.endpos;
	tr->plane.dist = trace.plane.dist;
	tr->plane.normal = trace.plane.normal;
	tr->materialHash = trace.materialHash;
	
	if (trace.ent != NULL && PM_GetPlayerMove())
		tr->ent = pe - PM_GetPlayerMove()->physents;
	else 
		tr->ent = -1;
}

void HUD_VoiceStatus( int entindex, qboolean bTalking )
{
}

void HUD_DirectorMessage( int iSize, void *pbuf )
{
}

void Demo_ReadBuffer( int size, unsigned char *buffer )
{
}

/*
==================
CL_NewMap

Called always when map is changed or restarted
==================
*/
void CL_NewMap()
{
	// flush collision meshes cache on clientside
	auto &meshDescFactory = CMeshDescFactory::Instance();
	meshDescFactory.ClearCache();

	if (g_pWeaponPredicting) {
		delete g_pWeaponPredicting;
	}
	g_pWeaponPredicting = new CWeaponPredictingContext();
}

cldll_func_t cldll_func = 
{
	Initialize,
	HUD_Init,
	HUD_VidInit,
	HUD_Redraw,
	HUD_UpdateClientData,
	HUD_Reset,
	HUD_PlayerMove,
	HUD_PlayerMoveInit,
	HUD_PlayerMoveTexture,
	IN_ActivateMouse,
	IN_DeactivateMouse,
	IN_MouseEvent,
	IN_ClearStates,
	IN_Accumulate,
	CL_CreateMove,
	CL_IsThirdPerson,
	CL_CameraOffset,
	KB_Find,
	CAM_Think,
	V_CalcRefdef,
	HUD_AddEntity,
	HUD_CreateEntities,
	HUD_DrawNormalTriangles,
	HUD_DrawTransparentTriangles,
	HUD_StudioEvent,
	HUD_PostRunCmd,
	HUD_Shutdown,
	HUD_TxferLocalOverrides,
	HUD_ProcessPlayerState,
	HUD_TxferPredictionData,
	Demo_ReadBuffer,
	HUD_ConnectionlessPacket,
	HUD_GetHullBounds,
	HUD_Frame,
	HUD_Key_Event,
	HUD_TempEntUpdate,
	HUD_GetUserEntity,
	HUD_VoiceStatus,
	HUD_DirectorMessage,
	HUD_GetStudioModelInterface,
	NULL,	// HUD_ChatInputPosition,
	HUD_GetRenderInterface,
	NULL,	/// HUD_ClipMoveToEntity
};

extern "C" void DLLEXPORT GetClientAPI( void *pv )
{
	cldll_func_t *pcldll_func = (cldll_func_t *)pv;
	*pcldll_func = cldll_func;
}
