/*
utils.h - utility code
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

#ifndef UTILS_H
#define UTILS_H
#include "port.h"
#include "cvardef.h"
#include "exportdef.h"

typedef unsigned char byte;
typedef unsigned short word;

typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );
typedef int (*cmpfunc)( const void *a, const void *b );

extern int pause;
extern int developer_level;
extern int r_currentMessageNum;
extern float v_idlescale;

extern int g_iXashEngineBuildNumber;
extern BOOL g_fRenderInitialized;

enum
{
	DEV_NONE = 0,
	DEV_NORMAL,
	DEV_EXTENDED
};

#ifdef _WIN32
typedef HMODULE dllhandle_t;
#else
typedef void* dllhandle_t;
#endif
typedef struct dllfunc_s
{
	const char *name;
	void	**func;
} dllfunc_t;

client_sprite_t *GetSpriteList( client_sprite_t *pList, const char *psz, int iRes, int iCount );
extern void DrawQuad( float xmin, float ymin, float xmax, float ymax );
extern void ALERT( ALERT_TYPE level, char *szFmt, ... );

// dll managment
bool Sys_LoadLibrary( const char *dllname, dllhandle_t *handle, const dllfunc_t *fcts = NULL );
void *Sys_GetProcAddress( dllhandle_t handle, const char *name );
void Sys_FreeLibrary( dllhandle_t *handle );

// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight	(gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth		(gHUD.m_scrinfo.iWidth)

// Use this to set any co-ords in 640x480 space
#define XRES( x )		((int)(float( x ) * ((float)ScreenWidth / 640.0f) + 0.5f))
#define YRES( y )		((int)(float( y ) * ((float)ScreenHeight / 480.0f) + 0.5f))

// use this to project world coordinates to screen coordinates
#define XPROJECT( x )	(( 1.0f + (x)) * ScreenWidth * 0.5f )
#define YPROJECT( y )	(( 1.0f - (y)) * ScreenHeight * 0.5f )

extern float g_hullcolor[8][3];
extern int g_boxpnt[6][4];

inline int ConsoleStringLen( const char *string )
{
	int _width, _height;
	GetConsoleStringSize( string, &_width, &_height );
	return _width;
}

void ScaleColors( int &r, int &g, int &b, int a );

inline void UnpackRGB(int &r, int &g, int &b, unsigned long ulRGB)\
{\
	r = (ulRGB & 0xFF0000) >>16;\
	g = (ulRGB & 0xFF00) >> 8;\
	b = ulRGB & 0xFF;\
}

HSPRITE LoadSprite( const char *pszName );

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	bool		overflowed;
	short		*list;
	Vector		mins, maxs;
	struct mnode_s	*headnode;	// for overflows where each leaf can't be stored individually
} leaflist_t;

struct mleaf_s *Mod_PointInLeaf( Vector p, struct mnode_s *node );
bool Mod_BoxVisible( const Vector mins, const Vector maxs, const byte *visbits );
bool Mod_CheckEntityPVS( cl_entity_t *ent );
bool Mod_CheckTempEntityPVS( struct tempent_s *pTemp );
bool Mod_CheckEntityLeafPVS( const Vector &absmin, const Vector &absmax, struct mleaf_s *leaf );
bool Mod_CheckBoxVisible( const Vector &absmin, const Vector &absmax );
void Mod_GetFrames( int modelIndex, int &numFrames );
int Mod_GetType( int modelIndex );

bool R_ScissorForAABB( const Vector &absmin, const Vector &absmax, float *x, float *y, float *w, float *h );
bool R_ScissorForCorners( const Vector bbox[8], float *x, float *y, float *w, float *h );
bool R_AABBToScreen( const Vector &absmin, const Vector &absmax, Vector2D &scrmin, Vector2D &scrmax, wrect_t *rect = NULL );
void R_DrawScissorRectangle( float x, float y, float w, float h );
void R_TransformWorldToDevice( const Vector &world, Vector &ndc );
void R_TransformDeviceToScreen( const Vector &ndc, Vector &screen );
bool R_ClipPolygon( int numPoints, Vector *points, const struct mplane_s *plane, int *numClipped, Vector *clipped );
void R_SplitPolygon( int numPoints, Vector *points, const struct mplane_s *plane, int *numFront, Vector *front, int *numBack, Vector *back );
bool UTIL_IsPlayer( int idx );
bool UTIL_IsLocal( int idx );

extern void HUD_StudioEvent( const struct mstudioevent_s *event, const struct cl_entity_s *entity );
extern void HUD_TempEntUpdate( double frametime, double client_time, double cl_gravity, struct tempent_s **ppTempEntFree,
struct tempent_s **ppTempEntActive, int ( *Callback_AddVisibleEntity )( struct cl_entity_s *pEntity ),
void ( *Callback_TempEntPlaySound )( struct tempent_s *pTemp, float damp ));

extern int HUD_AddEntity( int type, struct cl_entity_s *ent, const char *modelname );
extern void HUD_TxferLocalOverrides( struct entity_state_s *state, const struct clientdata_s *client );
extern void HUD_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src );
extern void HUD_TxferPredictionData( entity_state_t *ps, const entity_state_t *pps, clientdata_t *pcd, const clientdata_t *ppcd, weapon_data_t *wd, const weapon_data_t *pwd );
extern void HUD_StudioEvent( const struct mstudioevent_s *event, const struct cl_entity_s *entity );
extern void HUD_CreateEntities( void );

extern int CL_ButtonBits( int );
extern void CL_ResetButtonBits( int bits );
extern void InitInput( void );
extern void ShutdownInput( void );
extern int KB_ConvertString( char *in, char **ppout );
extern void IN_Init( void );
extern void IN_Move( float frametime, struct usercmd_s *cmd );
extern void IN_Shutdown( void );

extern void IN_ActivateMouse( void );
extern void IN_DeactivateMouse( void );
extern void IN_MouseEvent( int mstate );
extern void IN_Accumulate( void );
extern void IN_ClearStates( void );
extern void *KB_Find( const char *name );
extern void CL_CreateMove( float frametime, struct usercmd_s *cmd, int active );
extern int CL_IsDead( void );

extern int HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback );
extern int HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );

extern void HUD_DrawNormalTriangles( void );
extern void HUD_DrawTransparentTriangles( void );

extern void PM_Init( struct playermove_s *ppmove );
extern void PM_Move( struct playermove_s *ppmove, int server );
extern char PM_FindTextureType( char *name );
extern void V_CalcRefdef( struct ref_params_s *pparams );

void UTIL_CreateAurora( cl_entity_t *ent, const char *file, int attachment, float lifetime = 0.0f );
void UTIL_RemoveAurora( cl_entity_t *ent );
extern int PM_GetPhysEntInfo( int ent );

extern void CAM_Think( void );
extern void CL_CameraOffset( float *ofs );
extern int CL_IsThirdPerson( void );

// xxx need client dll function to get and clear impuse
extern cvar_t *in_joystick;
extern int g_weaponselect;
   
#endif // UTILS_H
