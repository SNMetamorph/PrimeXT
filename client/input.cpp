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

// cl.input.c  -- builds an intended movement command to send to the server

#include "hud.h"
#include "utils.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "kbutton.h"
#include "r_view.h"
#include "keydefs.h"
#include <mathlib.h>
#include "gl_local.h"

#define BUTTON_DOWN		1
#define IMPULSE_DOWN	2
#define IMPULSE_UP		4

int in_impulse = 0;
int in_cancel = 0;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*lookstrafe;
cvar_t	*lookspring;
cvar_t	*cl_pitchup;
cvar_t	*cl_pitchdown;
cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_backspeed;
cvar_t	*cl_sidespeed;
cvar_t	*cl_movespeedkey;
cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_anglespeedkey;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/

kbutton_t	in_mlook;
kbutton_t	in_klook;
kbutton_t	in_jlook;
kbutton_t	in_left;
kbutton_t	in_right;
kbutton_t	in_forward;
kbutton_t	in_back;
kbutton_t	in_lookup;
kbutton_t	in_lookdown;
kbutton_t	in_moveleft;
kbutton_t	in_moveright;
kbutton_t	in_strafe;
kbutton_t	in_speed;
kbutton_t	in_use;
kbutton_t	in_jump;
kbutton_t	in_attack;
kbutton_t	in_attack2;
kbutton_t	in_up;
kbutton_t	in_down;
kbutton_t	in_duck;
kbutton_t	in_reload;
kbutton_t	in_alt1;
kbutton_t	in_score;
kbutton_t	in_break;
kbutton_t	in_graph;		// Display the netgraph

typedef struct kblist_s
{
	struct kblist_s	*next;
	kbutton_t		*pkey;
	char		name[32];
} kblist_t;

kblist_t *g_kbkeys = NULL;

/*
============
KB_ConvertString

Removes references to +use and replaces them with the keyname in the output string.  If
 a binding is unfound, then the original text is retained.
NOTE:  Only works for text with +word in it.
============
*/
int KB_ConvertString( char *in, char **ppout )
{
	if( !ppout )
		return 0;

	char sz[4096];
	char binding[64];
	char *p, *pOut, *pEnd;
	const char *pBinding;

	*ppout = NULL;
	p = in;
	pOut = sz;

	while( *p )
	{
		if( *p == '+' )
		{
			pEnd = binding;
			while( *p && ( isalnum( *p ) || ( pEnd == binding )) && (( pEnd - binding ) < 63 ))
			{
				*pEnd++ = *p++;
			}

			*pEnd =  '\0';

			pBinding = NULL;
			if( Q_strlen( binding + 1 ) > 0 )
			{
				// See if there is a binding for binding?
				pBinding = gEngfuncs.Key_LookupBinding( binding + 1 );
			}

			if( pBinding )
			{
				*pOut++ = '[';
				pEnd = (char *)pBinding;
			}
			else
			{
				pEnd = binding;
			}

			while( *pEnd )
			{
				*pOut++ = *pEnd++;
			}

			if( pBinding )
			{
				*pOut++ = ']';
			}
		}
		else
		{
			*pOut++ = *p++;
		}
	}

	*pOut = '\0';

	pOut = ( char * )malloc( Q_strlen( sz ) + 1 );
	Q_strcpy( pOut, sz );
	*ppout = pOut;

	return 1;
}

/*
============
KB_Find

Allows the engine to get a kbutton_t directly ( so it can check +mlook state, etc ) for saving out to .cfg files
============
*/
void *KB_Find( const char *name )
{
	kblist_t *p;
	p = g_kbkeys;

	while( p )
	{
		if( !Q_stricmp( name, p->name ))
			return p->pkey;

		p = p->next;
	}
	return NULL;
}

/*
============
KB_Add

Add a kbutton_t * to the list of pointers the engine can retrieve via KB_Find
============
*/
void KB_Add( const char *name, kbutton_t *pkb )
{
	kblist_t *p;	
	kbutton_t *kb;

	kb = (kbutton_t *)KB_Find( name );
	
	if( kb ) return;

	p = ( kblist_t *)malloc( sizeof( kblist_t ));
	memset( p, 0, sizeof( *p ));

	Q_strcpy( p->name, name );
	p->pkey = pkb;

	p->next = g_kbkeys;
	g_kbkeys = p;
}

/*
============
KB_Init

Add kbutton_t definitions that the engine can query if needed
============
*/
void KB_Init( void )
{
	g_kbkeys = NULL;

	KB_Add( "in_graph", &in_graph );
	KB_Add( "in_mlook", &in_mlook );
	KB_Add( "in_jlook", &in_jlook );
}

/*
============
KB_Shutdown

Clear kblist
============
*/
void KB_Shutdown( void )
{
	kblist_t *p, *n;
	p = g_kbkeys;

	while( p )
	{
		n = p->next;
		free( p );
		p = n;
	}
	g_kbkeys = NULL;
}

/*
============
KeyDown
============
*/
void KeyDown( kbutton_t *b )
{
	char	*c;
	int	k;

	c = CMD_ARGV( 1 );

	if( c[0] )
		k = Q_atoi( c );
	else
		k = -1; // typed manually at the console for continuous down

	if( k == b->down[0] || k == b->down[1] )
		return; // repeating key
	
	if( !b->down[0] )
	{
		b->down[0] = k;
	}
	else if( !b->down[1] )
	{
		b->down[1] = k;
	}
	else
	{
		ALERT( at_aiconsole, "Three keys down for a button '%c' '%c' '%c'!\n", b->down[0], b->down[1], c );
		return;
	}
	
	if( b->state & BUTTON_DOWN ) return;	// still down
	b->state |= (BUTTON_DOWN|IMPULSE_DOWN);	// down + impulse down
}

/*
============
KeyUp
============
*/
void KeyUp( kbutton_t *b )
{
	char	*c;
	int	k;	

	c = CMD_ARGV( 1 );

	if( c[0] )
	{
		k = Q_atoi( c );
	}
	else
	{
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = IMPULSE_UP; // impulse up
		return;
	}

	if( b->down[0] == k )
	{
		b->down[0] = 0;
	}
	else if( b->down[1] == k )
	{
		b->down[1] = 0;
	}
	else
	{
		// key up without coresponding down (menu pass through)
		return;
	}

	if( b->down[0] || b->down[1] )
	{
		// some other key is still holding it down
		return;
	}

	if( !( b->state & BUTTON_DOWN )) return;// still up (this should not happen)

	b->state &= ~BUTTON_DOWN;	// now up
	b->state |= IMPULSE_UP;	// impulse up
}

void IN_BreakDown( void )	{ KeyDown( &in_break ); };
void IN_BreakUp( void )	{ KeyUp( &in_break ); };
void IN_KLookDown( void )	{ KeyDown( &in_klook ); }
void IN_KLookUp( void )	{ KeyUp( &in_klook ); }
void IN_JLookDown( void )	{ KeyDown( &in_jlook ); }
void IN_JLookUp( void )	{ KeyUp( &in_jlook ); }
void IN_MLookDown( void )	{ KeyDown( &in_mlook ); }
void IN_UpDown( void )	{ KeyDown( &in_up ); }
void IN_UpUp( void )	{ KeyUp( &in_up ); }
void IN_DownDown( void )	{ KeyDown( &in_down ); }
void IN_DownUp( void )	{ KeyUp( &in_down ); }
void IN_LeftDown( void )	{ KeyDown( &in_left ); }
void IN_LeftUp( void )	{ KeyUp( &in_left ); }
void IN_RightDown( void )	{ KeyDown( &in_right ); }
void IN_RightUp( void )	{ KeyUp( &in_right ); }
void IN_ForwardDown( void )	{ KeyDown( &in_forward ); }
void IN_ForwardUp( void )	{ KeyUp( &in_forward ); }
void IN_BackDown( void )	{ KeyDown( &in_back ); }
void IN_BackUp( void )	{ KeyUp( &in_back ); }
void IN_LookupDown( void )	{ KeyDown( &in_lookup ); }
void IN_LookupUp( void )	{ KeyUp( &in_lookup ); }
void IN_LookdownDown( void )	{ KeyDown( &in_lookdown ); }
void IN_LookdownUp( void )	{ KeyUp( &in_lookdown ); }
void IN_MoveleftDown( void )	{ KeyDown( &in_moveleft ); }
void IN_MoveleftUp( void )	{ KeyUp( &in_moveleft ); }
void IN_MoverightDown( void )	{ KeyDown( &in_moveright ); }
void IN_MoverightUp( void )	{ KeyUp( &in_moveright ); }
void IN_SpeedDown( void )	{ KeyDown( &in_speed ); }
void IN_SpeedUp( void )	{ KeyUp( &in_speed ); }
void IN_StrafeDown( void )	{ KeyDown( &in_strafe ); }
void IN_StrafeUp( void )	{ KeyUp( &in_strafe ); }
void IN_Attack2Down( void )	{ KeyDown( &in_attack2 ); }
void IN_Attack2Up( void )	{ KeyUp( &in_attack2 ); }
void IN_UseDown( void )	{ KeyDown( &in_use ); }
void IN_UseUp( void )	{ KeyUp( &in_use ); }
void IN_JumpDown( void )	{ KeyDown( &in_jump ); }
void IN_JumpUp( void )	{ KeyUp( &in_jump ); }
void IN_DuckDown( void )	{ KeyDown( &in_duck ); }
void IN_DuckUp( void )	{ KeyUp( &in_duck ); }
void IN_ReloadDown( void )	{ KeyDown( &in_reload ); }
void IN_ReloadUp( void )	{ KeyUp( &in_reload ); }
void IN_Alt1Down( void )	{ KeyDown( &in_alt1 ); }
void IN_Alt1Up( void )	{ KeyUp( &in_alt1 ); }
void IN_GraphDown( void )	{ KeyDown( &in_graph ); }
void IN_GraphUp( void )	{ KeyUp( &in_graph ); }
void IN_AttackDown( void )	{ KeyDown( &in_attack ); }
void IN_AttackUp( void )	{ KeyUp( &in_attack ); in_cancel = 0; }
void IN_ScoreDown(void)	{ KeyDown(&in_score); }
void IN_ScoreUp(void)	{ KeyUp(&in_score); }
void IN_Cancel( void )	{ in_cancel = 1; }	// Special handling
void IN_Impulse( void )	{ in_impulse = Q_atoi( CMD_ARGV( 1 )); }

void IN_MLookUp( void )
{
	KeyUp( &in_mlook );

	if(!( in_mlook.state & 1 ) && lookspring->value )
	{
		V_StartPitchDrift();
	}
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState( kbutton_t *key )
{
	float	val = 0.0f;
	int	impulsedown, impulseup, down;
	
	impulsedown = key->state & IMPULSE_DOWN;
	impulseup	= key->state & IMPULSE_UP;
	down = key->state & BUTTON_DOWN;
	
	if( impulsedown && !impulseup )
	{
		// pressed and held this frame?
		val = down ? 0.5f : 0.0f;
	}

	if( impulseup && !impulsedown )
	{
		// released this frame?
		val = down ? 0.0f : 0.0f;
	}

	if( !impulsedown && !impulseup )
	{
		// held the entire frame?
		val = down ? 1.0f : 0.0f;
	}

	if( impulsedown && impulseup )
	{
		if( down )
		{
			// released and re-pressed this frame
			val = 0.75f;	
		}
		else
		{
			// pressed and released this frame
			val = 0.25f;	
		}
	}

	// clear impulses
	key->state &= BUTTON_DOWN;		

	return val;
}

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles( float frametime, Vector &viewangles )
{
	float	speed;
	float	up, down;
	
	if( in_speed.state & BUTTON_DOWN )
	{
		speed = frametime * cl_anglespeedkey->value;
	}
	else
	{
		speed = frametime;
	}

	if( !( in_strafe.state & BUTTON_DOWN ))
	{
		viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState( &in_right );
		viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState( &in_left );
		viewangles[YAW] = anglemod( viewangles[YAW] );
	}

	if( in_klook.state & BUTTON_DOWN )
	{
		V_StopPitchDrift ();
		viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_forward );
		viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_back );
	}
	
	up = CL_KeyState( &in_lookup );
	down = CL_KeyState( &in_lookdown );
	
	viewangles[PITCH] -= speed * cl_pitchspeed->value * up;
	viewangles[PITCH] += speed * cl_pitchspeed->value * down;

	if( up || down )
		V_StopPitchDrift ();

	viewangles[PITCH] = bound( -cl_pitchup->value, viewangles[PITCH], cl_pitchdown->value );
	viewangles[ROLL] = bound( -50, viewangles[ROLL], 50 );
}

/*
============
CL_IsDead

Returns 1 if health is <= 0
============
*/
int CL_IsDead( void )
{
	return ( gHUD.m_Health.m_iHealth <= 0 ) ? 1 : 0;
}

/*
================
CL_CreateMove

Send the intended movement message to the server
if active == 1 then we are 1) not playing back demos ( where our commands are ignored ) and
2 ) we have finished signing on to server
================
*/
void CL_CreateMove( float frametime, usercmd_t *cmd, int active )
{	
	float spd;
	Vector viewangles, forward;
	static Vector oldangles;

	if( active )
	{
		gEngfuncs.GetViewAngles( viewangles );

		CL_AdjustAngles( frametime, viewangles );

		memset( cmd, 0, sizeof( *cmd ));

		gEngfuncs.SetViewAngles( viewangles );

		if( in_strafe.state & BUTTON_DOWN )
		{
			cmd->sidemove += cl_sidespeed->value * CL_KeyState( &in_right );
			cmd->sidemove -= cl_sidespeed->value * CL_KeyState( &in_left );
		}

		cmd->sidemove += cl_sidespeed->value * CL_KeyState( &in_moveright );
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState( &in_moveleft );

		cmd->upmove += cl_upspeed->value * CL_KeyState( &in_up );
		cmd->upmove -= cl_upspeed->value * CL_KeyState( &in_down );

		if(!( in_klook.state & BUTTON_DOWN ))
		{	
			cmd->forwardmove += cl_forwardspeed->value * CL_KeyState( &in_forward );
			cmd->forwardmove -= cl_backspeed->value * CL_KeyState( &in_back );
		}	

		// adjust for speed key
		if( in_speed.state & BUTTON_DOWN )
		{
			cmd->forwardmove *= cl_movespeedkey->value;
			cmd->sidemove *= cl_movespeedkey->value;
			cmd->upmove *= cl_movespeedkey->value;
		}

		// clip to maxspeed
		spd = gEngfuncs.GetClientMaxspeed();
		if( spd != 0.0f )
		{
			// scale the 3 speeds so that the total velocity is not > cl.maxspeed
			float fmov = sqrt(( cmd->forwardmove * cmd->forwardmove) + (cmd->sidemove * cmd->sidemove) + (cmd->upmove * cmd->upmove));

			if( fmov > spd )
			{
				float fratio = spd / fmov;
				cmd->forwardmove *= fratio;
				cmd->sidemove *= fratio;
				cmd->upmove *= fratio;
			}
		}

		// allow mice and other controllers to add their inputs
		if( !CL_IsDead( )) IN_Move( frametime, cmd );
	}

	cmd->impulse = in_impulse;
	in_impulse = 0;

	cmd->weaponselect = g_weaponselect;
	g_weaponselect = 0;

	// set button and flag bits
	cmd->buttons = CL_ButtonBits( 1 );

	gEngfuncs.GetViewAngles( viewangles );

	// Set current view angles.
	if( CL_IsDead( ))
	{
		cmd->viewangles = oldangles;
	}
	else
	{
		cmd->viewangles = viewangles;
		oldangles = viewangles;
	}
}

/*
============
CL_ButtonBits

Returns appropriate button info for keyboard and mouse state
Set bResetState to 1 to clear old state info
============
*/
int CL_ButtonBits( int bResetState )
{
	int bits = 0;

	if( in_attack.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_ATTACK;
	}
	
	if( in_duck.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_DUCK;
	}
 
	if( in_jump.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_JUMP;
	}

	if( in_forward.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_FORWARD;
	}
	
	if( in_back.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_BACK;
	}

	if( in_use.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_USE;
	}

	if( in_cancel )
	{
		bits |= IN_CANCEL;
	}

	if( in_left.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_LEFT;
	}
	
	if( in_right.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_RIGHT;
	}
	
	if( in_moveleft.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_MOVELEFT;
	}
	
	if( in_moveright.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_MOVERIGHT;
	}

	if( in_attack2.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_ATTACK2;
	}

	if( in_reload.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_RELOAD;
	}

	if( in_alt1.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_ALT1;
	}

	if( in_score.state & (BUTTON_DOWN|IMPULSE_DOWN))
	{
		bits |= IN_SCORE;
	}

	if( gHUD.m_Scoreboard.IsVisible() )
	{
		bits |= IN_SCORE;
	}

	if( bResetState )
	{
		in_attack.state &= ~IMPULSE_DOWN;
		in_duck.state &= ~IMPULSE_DOWN;
		in_jump.state &= ~IMPULSE_DOWN;
		in_forward.state &= ~IMPULSE_DOWN;
		in_back.state &= ~IMPULSE_DOWN;
		in_use.state &= ~IMPULSE_DOWN;
		in_left.state &= ~IMPULSE_DOWN;
		in_right.state &= ~IMPULSE_DOWN;
		in_moveleft.state &= ~IMPULSE_DOWN;
		in_moveright.state &= ~IMPULSE_DOWN;
		in_attack2.state &= ~IMPULSE_DOWN;
		in_reload.state &= ~IMPULSE_DOWN;
		in_alt1.state &= ~IMPULSE_DOWN;
		in_score.state &= ~IMPULSE_DOWN;
	}

	return bits;
}

/*
============
CL_ResetButtonBits

============
*/
void CL_ResetButtonBits( int bits )
{
	int bitsNew = CL_ButtonBits( 0 ) ^ bits;

	// has the attack button been changed
	if( bitsNew & IN_ATTACK )
	{
		// Was it pressed? or let go?
		if( bits & IN_ATTACK )
		{
			KeyDown( &in_attack );
		}
		else
		{
			// totally clear state
			in_attack.state &= ~(BUTTON_DOWN|IMPULSE_DOWN|IMPULSE_UP);
		}
	}
}

/*
============
InitInput
============
*/
void InitInput( void )
{
	ADD_COMMAND ("+moveup", IN_UpDown);
	ADD_COMMAND ("-moveup", IN_UpUp);
	ADD_COMMAND ("+movedown", IN_DownDown);
	ADD_COMMAND ("-movedown", IN_DownUp);
	ADD_COMMAND ("+left", IN_LeftDown);
	ADD_COMMAND ("-left", IN_LeftUp);
	ADD_COMMAND ("+right", IN_RightDown);
	ADD_COMMAND ("-right", IN_RightUp);
	ADD_COMMAND ("+forward", IN_ForwardDown);
	ADD_COMMAND ("-forward", IN_ForwardUp);
	ADD_COMMAND ("+back", IN_BackDown);
	ADD_COMMAND ("-back", IN_BackUp);
	ADD_COMMAND ("+lookup", IN_LookupDown);
	ADD_COMMAND ("-lookup", IN_LookupUp);
	ADD_COMMAND ("+lookdown", IN_LookdownDown);
	ADD_COMMAND ("-lookdown", IN_LookdownUp);
	ADD_COMMAND ("+strafe", IN_StrafeDown);
	ADD_COMMAND ("-strafe", IN_StrafeUp);
	ADD_COMMAND ("+moveleft", IN_MoveleftDown);
	ADD_COMMAND ("-moveleft", IN_MoveleftUp);
	ADD_COMMAND ("+moveright", IN_MoverightDown);
	ADD_COMMAND ("-moveright", IN_MoverightUp);
	ADD_COMMAND ("+speed", IN_SpeedDown);
	ADD_COMMAND ("-speed", IN_SpeedUp);
	ADD_COMMAND ("+attack", IN_AttackDown);
	ADD_COMMAND ("-attack", IN_AttackUp);
	ADD_COMMAND ("+attack2", IN_Attack2Down);
	ADD_COMMAND ("-attack2", IN_Attack2Up);
	ADD_COMMAND ("+use", IN_UseDown);
	ADD_COMMAND ("-use", IN_UseUp);
	ADD_COMMAND ("+jump", IN_JumpDown);
	ADD_COMMAND ("-jump", IN_JumpUp);
	ADD_COMMAND ("impulse", IN_Impulse);
	ADD_COMMAND ("+klook", IN_KLookDown);
	ADD_COMMAND ("-klook", IN_KLookUp);
	ADD_COMMAND ("+mlook", IN_MLookDown);
	ADD_COMMAND ("-mlook", IN_MLookUp);
	ADD_COMMAND ("+jlook", IN_JLookDown);
	ADD_COMMAND ("-jlook", IN_JLookUp);
	ADD_COMMAND ("+duck", IN_DuckDown);
	ADD_COMMAND ("-duck", IN_DuckUp);
	ADD_COMMAND ("+reload", IN_ReloadDown);
	ADD_COMMAND ("-reload", IN_ReloadUp);
	ADD_COMMAND ("+alt1", IN_Alt1Down);
	ADD_COMMAND ("-alt1", IN_Alt1Up);
	ADD_COMMAND ("+graph", IN_GraphDown);
	ADD_COMMAND ("-graph", IN_GraphUp);
	ADD_COMMAND ("+score", IN_ScoreDown);
	ADD_COMMAND ("-score", IN_ScoreUp);
	ADD_COMMAND ("+break", IN_BreakDown);
	ADD_COMMAND ("-break", IN_BreakUp);

	lookstrafe	= CVAR_REGISTER ( "lookstrafe", "0", FCVAR_ARCHIVE );
	lookspring	= CVAR_REGISTER ( "lookspring", "0", FCVAR_ARCHIVE );
	cl_anglespeedkey	= CVAR_REGISTER ( "cl_anglespeedkey", "0.67", 0 );
	cl_yawspeed	= CVAR_REGISTER ( "cl_yawspeed", "210", 0 );
	cl_pitchspeed	= CVAR_REGISTER ( "cl_pitchspeed", "225", 0 );
	cl_upspeed	= CVAR_REGISTER ( "cl_upspeed", "320", 0 );
	cl_forwardspeed	= CVAR_REGISTER ( "cl_forwardspeed", "400", FCVAR_ARCHIVE );
	cl_backspeed	= CVAR_REGISTER ( "cl_backspeed", "400", FCVAR_ARCHIVE );
	cl_sidespeed	= CVAR_REGISTER ( "cl_sidespeed", "400", 0 );
	cl_movespeedkey	= CVAR_REGISTER ( "cl_movespeedkey", "0.3", 0 );
	cl_pitchup	= CVAR_REGISTER ( "cl_pitchup", "89", 0 );
	cl_pitchdown	= CVAR_REGISTER ( "cl_pitchdown", "89", 0 );

	m_pitch		= CVAR_REGISTER ( "m_pitch","0.022", FCVAR_ARCHIVE );
	m_yaw		= CVAR_REGISTER ( "m_yaw","0.022", FCVAR_ARCHIVE );
	m_forward		= CVAR_REGISTER ( "m_forward","1", FCVAR_ARCHIVE );
	m_side		= CVAR_REGISTER ( "m_side","0.8", FCVAR_ARCHIVE );

	// initialize inputs
	IN_Init();

	// initialize keyboard
	KB_Init();

	// initialize view system
	V_Init();
}

/*
============
ShutdownInput
============
*/
void ShutdownInput( void )
{
	IN_Shutdown();
	KB_Shutdown();
}
