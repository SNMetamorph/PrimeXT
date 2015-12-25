/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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

#include "hud.h"
#include "utils.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "kbutton.h"
#include "r_view.h"
#include "keydefs.h"
#include <mathlib.h>

// in_win.c -- windows 95 mouse code

#define MOUSE_BUTTON_COUNT	5

extern kbutton_t	in_strafe;
extern kbutton_t	in_mlook;
extern kbutton_t	in_speed;
extern kbutton_t	in_jlook;

extern cvar_t	*m_pitch;
extern cvar_t	*m_yaw;
extern cvar_t	*m_forward;
extern cvar_t	*m_side;
extern cvar_t	*lookstrafe;
extern cvar_t	*lookspring;
extern cvar_t	*cl_pitchdown;
extern cvar_t	*cl_pitchup;
extern cvar_t	*cl_yawspeed;
extern cvar_t	*cl_sidespeed;
extern cvar_t	*cl_forwardspeed;
extern cvar_t	*cl_pitchspeed;
extern cvar_t	*cl_movespeedkey;

// mouse variables
cvar_t		*m_filter;
cvar_t		*sensitivity;

int		mouse_buttons;
int		mouse_oldbuttonstate;
POINT		current_pos;
int		mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

static int	restore_spi;
static int	originalmouseparms[3], newmouseparms[3] = { 0, 0, 1 };
static int	mouseactive;
static int	mouseinitialized;
static int	mouseparmsvalid;

/*
===========
Force_CenterView_f
===========
*/
void Force_CenterView_f( void )
{
	Vector viewangles;

	gEngfuncs.GetViewAngles( viewangles );
	viewangles[PITCH] = 0;

	float rgfl[3];
	viewangles.CopyToArray( rgfl );
	gEngfuncs.SetViewAngles( rgfl );
}

/*
===========
IN_ActivateMouse
===========
*/
void IN_ActivateMouse( void )
{
	if( mouseinitialized )
	{
		if( mouseparmsvalid )
			restore_spi = SystemParametersInfo( SPI_SETMOUSE, 0, newmouseparms, 0 );
		mouseactive = 1;
	}
}

/*
===========
IN_DeactivateMouse
===========
*/
void IN_DeactivateMouse( void )
{
	if( mouseinitialized )
	{
		if( restore_spi )
			SystemParametersInfo( SPI_SETMOUSE, 0, originalmouseparms, 0 );
		mouseactive = 0;
	}
}

/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse( void )
{
	if( gEngfuncs.CheckParm( "-nomouse", NULL )) 
		return; 

	mouseinitialized = 1;
	mouseparmsvalid = SystemParametersInfo( SPI_GETMOUSE, 0, originalmouseparms, 0 );

	if( mouseparmsvalid )
	{
		if( gEngfuncs.CheckParm( "-noforcemspd", NULL ))
		{ 
			newmouseparms[2] = originalmouseparms[2];
                    }

		if( gEngfuncs.CheckParm( "-noforcemaccel", NULL )) 
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
		}

		if( gEngfuncs.CheckParm( "-noforcemparms", NULL )) 
		{
			newmouseparms[0] = originalmouseparms[0];
			newmouseparms[1] = originalmouseparms[1];
			newmouseparms[2] = originalmouseparms[2];
		}
	}

	mouse_buttons = MOUSE_BUTTON_COUNT;
}

/*
===========
IN_Shutdown
===========
*/
void IN_Shutdown( void )
{
	IN_DeactivateMouse();
}

/*
===========
IN_GetMousePos

Ask for mouse position from engine
===========
*/
void IN_GetMousePos( int *mx, int *my )
{
	gEngfuncs.GetMousePosition( mx, my );
}

/*
===========
IN_ResetMouse

Reset mouse position from engine
===========
*/
void IN_ResetMouse( void )
{
	SetCursorPos( gEngfuncs.GetWindowCenterX(), gEngfuncs.GetWindowCenterY() );	
}

/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent( int mstate )
{
	// perform button actions
	for( int i = 0; i < mouse_buttons; i++ )
	{
		if(( mstate & BIT( i )) && !( mouse_oldbuttonstate & BIT( i )))
		{
			gEngfuncs.Key_Event( K_MOUSE1 + i, 1 );
		}

		if( !( mstate & BIT( i )) && ( mouse_oldbuttonstate & BIT( i )))
		{
			gEngfuncs.Key_Event( K_MOUSE1 + i, 0 );
		}
	}	
	
	mouse_oldbuttonstate = mstate;
}

/*
===========
IN_MouseMove
===========
*/
void IN_MouseMove( float frametime, usercmd_t *cmd )
{
	int mx, my;
	Vector viewangles;

	if( !mouseactive ) return;

	gEngfuncs.GetViewAngles( viewangles );

	if( in_mlook.state & 1 )
	{
		V_StopPitchDrift();
	}

	// jjb - this disbles normal mouse control if the user is trying to 
	// move the camera, or if the mouse cursor is visible or if we're in intermission
	if( !gHUD.m_iIntermission )
	{
		GetCursorPos( &current_pos );

		mx = current_pos.x - gEngfuncs.GetWindowCenterX() + mx_accum;
		my = current_pos.y - gEngfuncs.GetWindowCenterY() + my_accum;

		mx_accum = 0;
		my_accum = 0;

		if( m_filter->value )
		{
			mouse_x = (mx + old_mouse_x) * 0.5f;
			mouse_y = (my + old_mouse_y) * 0.5f;
		}
		else
		{
			mouse_x = mx;
			mouse_y = my;
		}

		old_mouse_x = mx;
		old_mouse_y = my;

		if( gHUD.GetSensitivity() != 0 )
		{
			mouse_x *= gHUD.GetSensitivity();
			mouse_y *= gHUD.GetSensitivity();
		}
		else
		{
			mouse_x *= sensitivity->value;
			mouse_y *= sensitivity->value;
		}

		// add mouse X/Y movement to cmd
		if(( in_strafe.state & 1 ) || ( lookstrafe->value && ( in_mlook.state & 1 )))
			cmd->sidemove += m_side->value * mouse_x;
		else
			viewangles[YAW] -= m_yaw->value * mouse_x;

		if(( in_mlook.state & 1 ) && !( in_strafe.state & 1 ))
		{
			viewangles[PITCH] += m_pitch->value * mouse_y;
			viewangles[PITCH] = bound( -cl_pitchup->value, viewangles[PITCH], cl_pitchdown->value );
		}
		else
		{
			if(( in_strafe.state & 1 ) && gEngfuncs.IsNoClipping( ))
			{
				cmd->upmove -= m_forward->value * mouse_y;
			}
			else
			{
				cmd->forwardmove -= m_forward->value * mouse_y;
			}
		}

		// if the mouse has moved, force it to the center, so there's room to move
		if( mx || my )
		{
			IN_ResetMouse();
		}
	}

	float rgfl[3];
	viewangles.CopyToArray( rgfl );
	gEngfuncs.SetViewAngles( rgfl );
}

/*
===========
IN_Accumulate
===========
*/
void IN_Accumulate( void )
{
	if( mouseactive )
	{
		GetCursorPos( &current_pos );

		mx_accum += current_pos.x - gEngfuncs.GetWindowCenterX();
		my_accum += current_pos.y - gEngfuncs.GetWindowCenterY();

		// force the mouse to the center, so there's room to move
		IN_ResetMouse();
	}
}

/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates( void )
{
	if( !mouseactive ) return;

	mx_accum = 0;
	my_accum = 0;
	mouse_oldbuttonstate = 0;
}

/*
===========
IN_Move
===========
*/
void IN_Move( float frametime, usercmd_t *cmd )
{
	IN_MouseMove( frametime, cmd );
}

/*
===========
IN_Init
===========
*/
void IN_Init( void )
{
	m_filter		= CVAR_REGISTER ( "m_filter","0", FCVAR_ARCHIVE );
	sensitivity	= CVAR_REGISTER ( "sensitivity","3", FCVAR_ARCHIVE ); // user mouse sensitivity setting.

	ADD_COMMAND ("force_centerview", Force_CenterView_f);
	IN_StartupMouse ();
}
