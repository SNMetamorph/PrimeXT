//=======================================================================
//			Copyright (C) Shambler Team 2005
//			r_view.cpp - multipass renderer
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "r_view.h"
#include "ref_params.h"
#include "pm_movevars.h"
#include "pm_defs.h"
#include "r_local.h"
#include "r_studioint.h"
#include "r_studio.h"
#include <mathlib.h>

// thirdperson camera
void CAM_Think( void ) {}
void CL_CameraOffset( float *ofs ) { g_vecZero.CopyToArray( ofs ); }
int CL_IsThirdPerson( void ){ return (gHUD.m_iCameraMode ? 1 : 0); }

cl_entity_t *v_intermission_spot;
float v_idlescale;
static int pause = 0;

cvar_t	*r_test;
cvar_t	*r_extensions;
cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;
cvar_t	*cl_weaponlag;
cvar_t	*cl_vsmoothing;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;
cvar_t	*cl_viewsize;
cvar_t	*gl_renderer;
cvar_t	*gl_check_errors;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_speeds;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nosort;
cvar_t	*r_lockpvs;
cvar_t	*r_dynamic;
cvar_t	*r_shadows;
cvar_t	*r_lightmap;
cvar_t	*vid_gamma;
cvar_t	*vid_brightness;
cvar_t	*r_adjust_fov;
cvar_t	*r_wireframe;
cvar_t	*r_fullbright;
cvar_t	*r_allow_3dsky;
cvar_t	*r_allow_mirrors;
cvar_t	*r_allow_portals;
cvar_t	*r_allow_screens;
cvar_t	*r_drawentities;
cvar_t	*r_detailtextures;
cvar_t	*r_recursion_depth;
cvar_t	*r_lighting_modulate;
cvar_t	*r_lighting_extended;
cvar_t	*r_lightstyle_lerping;
cvar_t	*r_recursive_world_node;
cvar_t	*r_polyoffset;
cvar_t	*r_grass;
cvar_t	*r_grass_vbo;
cvar_t	*r_grass_alpha;
cvar_t	*r_grass_lighting;
cvar_t	*r_grass_shadows;
cvar_t	*r_grass_fade_start;
cvar_t	*r_grass_fade_dist;
cvar_t	*r_show_renderpass;
cvar_t	*r_show_light_scissors;
cvar_t	*r_show_normals;
cvar_t	*r_show_lightprobes;

cvar_t	v_iyaw_cycle	= { "v_iyaw_cycle", "2", 0, 2 };
cvar_t	v_iroll_cycle	= { "v_iroll_cycle", "0.5", 0, 0.5 };
cvar_t	v_ipitch_cycle	= { "v_ipitch_cycle", "1", 0, 1 };
cvar_t	v_iyaw_level  	= { "v_iyaw_level", "0.3", 0, 0.3 };
cvar_t	v_iroll_level 	= { "v_iroll_level", "0.1", 0, 0.1 };
cvar_t	v_ipitch_level	= { "v_ipitch_level", "0.3", 0, 0.3 };

//============================================================================== 
//				VIEW RENDERING 
//============================================================================== 

//==========================
// V_ThirdPerson
//
// switch to third person
//==========================
void V_ThirdPerson( void )
{
	if( gEngfuncs.GetMaxClients() == 1 )
		gHUD.m_iCameraMode = 1;
}

//==========================
// V_FirstPerson
//
// switch to first person
//==========================
void V_FirstPerson( void )
{
	gHUD.m_iCameraMode = 0;
}

//==========================
// V_Init
//==========================
void V_Init( void )
{
	v_centermove	= CVAR_REGISTER( "v_centermove", "0.15", 0 );
	v_centerspeed	= CVAR_REGISTER( "v_centerspeed","500", 0 );

	cl_bobcycle	= CVAR_REGISTER( "cl_bobcycle","0.8", 0 );
	cl_bob		= CVAR_REGISTER( "cl_bob","0.01", 0 );
	cl_bobup		= CVAR_REGISTER( "cl_bobup","0.5", 0 );
	cl_waterdist	= CVAR_REGISTER( "cl_waterdist","4", 0 );
	cl_chasedist	= CVAR_REGISTER( "cl_chasedist","112", 0 );
	cl_weaponlag	= CVAR_REGISTER( "cl_weaponlag", "0.3", FCVAR_ARCHIVE );

	// setup some engine cvars for custom rendering
	r_extensions	= CVAR_GET_POINTER( "gl_allow_extensions" );
	r_finish		= CVAR_GET_POINTER( "gl_finish" );
	r_clear		= CVAR_GET_POINTER( "gl_clear" );
	r_speeds		= CVAR_GET_POINTER( "r_speeds" );
	r_test		= CVAR_GET_POINTER( "gl_test" );
	cl_viewsize	= CVAR_GET_POINTER( "viewsize" );

	r_novis		= CVAR_GET_POINTER( "r_novis" );
	r_nocull		= CVAR_GET_POINTER( "r_nocull" );
	r_nosort		= CVAR_GET_POINTER( "gl_nosort" );
	r_lockpvs		= CVAR_GET_POINTER( "r_lockpvs" );
	r_dynamic		= CVAR_GET_POINTER( "r_dynamic" );
	r_lightmap	= CVAR_GET_POINTER( "r_lightmap" );
	r_wireframe	= CVAR_GET_POINTER( "gl_wireframe" );
	r_adjust_fov	= CVAR_GET_POINTER( "r_adjust_fov" );
	gl_check_errors	= CVAR_GET_POINTER( "gl_check_errors" );
	vid_gamma		= CVAR_GET_POINTER( "gamma" );
	vid_brightness	= CVAR_GET_POINTER( "brightness" );
	r_polyoffset	= CVAR_GET_POINTER( "gl_polyoffset" );

	r_fullbright	= CVAR_GET_POINTER( "r_fullbright" );
	r_drawentities	= CVAR_GET_POINTER( "r_drawentities" );
	r_detailtextures	= CVAR_GET_POINTER( "r_detailtextures" );
	r_lighting_modulate	= CVAR_GET_POINTER( "r_lighting_modulate" );
	r_lightstyle_lerping= CVAR_GET_POINTER( "cl_lightstyle_lerping" );
	r_lighting_extended	= CVAR_GET_POINTER( "r_lighting_extended" );

	cl_vsmoothing	= CVAR_REGISTER( "cl_vsmoothing", "0.05", FCVAR_ARCHIVE );
	r_allow_portals	= CVAR_REGISTER( "gl_allow_portals", "1", FCVAR_ARCHIVE|FCVAR_CLIENTDLL );
	r_allow_screens	= CVAR_REGISTER( "gl_allow_screens", "1", FCVAR_ARCHIVE|FCVAR_CLIENTDLL );
	gl_renderer	= CVAR_REGISTER( "gl_renderer", "1", FCVAR_CLIENTDLL|FCVAR_ARCHIVE ); 
	r_shadows		= CVAR_REGISTER( "r_shadows", "2", FCVAR_CLIENTDLL|FCVAR_ARCHIVE ); 
	r_allow_3dsky	= CVAR_REGISTER( "gl_allow_3dsky", "1", FCVAR_ARCHIVE );
	r_allow_mirrors	= CVAR_REGISTER( "gl_allow_mirrors", "1", FCVAR_ARCHIVE );
	r_recursion_depth	= CVAR_REGISTER( "gl_recursion_depth", "1", FCVAR_ARCHIVE );
	r_recursive_world_node = CVAR_REGISTER( "gl_recursive_world_node", "0", FCVAR_ARCHIVE );

	r_grass = CVAR_REGISTER( "r_grass", "1", FCVAR_ARCHIVE );
	r_grass_alpha = CVAR_REGISTER( "r_grass_alpha", "0.5", FCVAR_ARCHIVE );
	r_grass_lighting = CVAR_REGISTER( "r_grass_lighting", "1", FCVAR_ARCHIVE );
	r_grass_shadows = CVAR_REGISTER( "r_grass_shadows", "1", FCVAR_ARCHIVE );
	r_grass_fade_start = CVAR_REGISTER( "r_grass_fade_start", "2048", FCVAR_ARCHIVE );
	r_grass_fade_dist = CVAR_REGISTER( "r_grass_fade_dist", "2048", FCVAR_ARCHIVE );
	r_grass_vbo = CVAR_REGISTER( "r_grass_vbo", "-1", FCVAR_ARCHIVE );

	r_show_renderpass = CVAR_REGISTER( "r_show_renderpass", "0", 0 );
	r_show_light_scissors = CVAR_REGISTER( "r_show_light_scissors", "0", 0 );
	r_show_normals = CVAR_REGISTER( "r_show_normals", "0", 0 );
	r_show_lightprobes = CVAR_REGISTER( "r_show_lightprobes", "0", 0 );

	ADD_COMMAND( "thirdperson", V_ThirdPerson );
	ADD_COMMAND( "firstperson", V_FirstPerson );
	ADD_COMMAND( "centerview", V_StartPitchDrift );
}

//==========================
// V_CalcBob
//
// Quakeworld bob code, this fixes jitters in the mutliplayer
// since the clock (pparams->time) isn't quite linear
//==========================
float V_CalcBob( struct ref_params_s *pparams )
{
	static double bobtime;
	static float bob, lasttime;
	float cycle;
	Vector vel;

	if( pparams->onground == -1 || pparams->time == lasttime )
		return bob;	

	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if( cycle < cl_bobup->value )
	{
		cycle = M_PI * cycle / cl_bobup->value;
	}
	else
	{
		cycle = M_PI + M_PI * ( cycle - cl_bobup->value ) / ( 1.0f - cl_bobup->value );
	}

	vel = pparams->simvel;
	bob = sqrt( vel.x * vel.x + vel.y * vel.y ) * cl_bob->value;
	bob = bob * 0.3f + bob * 0.7f * sin( cycle );

	return bound( -7, bob, 4 );
}

extern cvar_t *cl_forwardspeed;

static struct
{
	float	pitchvel;
	int  	nodrift;
	float	driftmove;
	double	laststop;
} pd;

//==========================
// V_StartPitchDrift
//==========================
void V_StartPitchDrift( void )
{
	if( pd.laststop == GET_CLIENT_TIME( ))
	{
		// something else is keeping it from drifting
		return;
	}

	if( pd.nodrift || !pd.pitchvel )
	{
		pd.pitchvel = v_centerspeed->value;
		pd.driftmove = 0;
		pd.nodrift = 0;
	}
}

//==========================
// V_StopPitchDrift
//==========================
void V_StopPitchDrift( void )
{
	pd.laststop = GET_CLIENT_TIME();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
===============
*/
void V_DriftPitch( struct ref_params_s *pparams )
{
	if( gEngfuncs.IsNoClipping() || !pparams->onground || pparams->demoplayback )
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	if( pd.nodrift )
	{
		if( fabs( pparams->cmd->forwardmove ) < cl_forwardspeed->value )
		{
			pd.driftmove = 0;
		}
		else
		{
			pd.driftmove += pparams->frametime;
		}

		if( pd.driftmove > v_centermove->value )
			V_StartPitchDrift ();
		return;
	}
	
	float delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if( !delta )
	{
		pd.pitchvel = 0;
		return;
	}

	float move = pparams->frametime * pd.pitchvel;
	pd.pitchvel += pparams->frametime * v_centerspeed->value;
	
	if( delta > 0 )
	{
		if( move > delta )
		{
			pd.pitchvel = 0;
			move = delta;
		}

		pparams->cl_viewangles[PITCH] += move;
	}
	else if( delta < 0 )
	{
		if( move > -delta )
		{
			pd.pitchvel = 0;
			move = -delta;
		}

		pparams->cl_viewangles[PITCH] -= move;
	}
}

//==========================
// V_CalcGunAngle
//==========================
void V_CalcGunAngle( struct ref_params_s *pparams )
{	
	cl_entity_t *viewent;
	
	viewent = GET_VIEWMODEL();
	if( !viewent ) return;

	viewent->angles[YAW] = pparams->viewangles[YAW] + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25f;
	viewent->angles[ROLL] -= v_idlescale * sin( pparams->time * v_iroll_cycle.value ) * v_iroll_level.value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin( pparams->time*v_ipitch_cycle.value ) * (v_ipitch_level.value * 0.5f);
	viewent->angles[YAW] -= v_idlescale * sin( pparams->time*v_iyaw_cycle.value ) * v_iyaw_level.value;

	viewent->latched.prevangles = viewent->angles;
	viewent->curstate.angles = viewent->angles;
}

//==========================
// V_CalcViewModelLag
//==========================
void V_CalcViewModelLag( ref_params_t *pparams, Vector &origin, Vector &angles, const Vector &original_angles )
{
	static Vector m_vecLastFacing;
	Vector vOriginalOrigin = origin;
	Vector vOriginalAngles = angles;

	// Calculate our drift
	Vector forward, right, up;

	AngleVectors( angles, forward, right, up );

	if( pparams->frametime != 0.0f )	// not in paused
	{
		Vector vDifference;

		vDifference = forward - m_vecLastFacing;

		float flSpeed = 5.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.
		// Solves the problem with fast cl_yawspeed, m_yaw or joysticks rotating quickly.
		// The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();

		if(( flDiff > cl_weaponlag->value ) && ( cl_weaponlag->value > 0.0f ))
		{
			float flScale = flDiff / cl_weaponlag->value;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		m_vecLastFacing = m_vecLastFacing + vDifference * ( flSpeed * pparams->frametime );
		// Make sure it doesn't grow out of control!!!
		m_vecLastFacing = m_vecLastFacing.Normalize();
		origin = origin + (vDifference * -1.0f) * flSpeed;
	}

	AngleVectors( original_angles, forward, right, up );

	float pitch = original_angles[PITCH];

	if( pitch > 180.0f )
	{
		pitch -= 360.0f;
	}
	else if( pitch < -180.0f )
	{
		pitch += 360.0f;
	}

	if( cl_weaponlag->value <= 0.0f )
	{
		origin = vOriginalOrigin;
		angles = vOriginalAngles;
	}
	else
	{
		// FIXME: These are the old settings that caused too many exposed polys on some models
		origin = origin + forward * ( -pitch * 0.035f );
		origin = origin + right * ( -pitch * 0.03f );
		origin = origin + up * ( -pitch * 0.02f );
	}
}

//==========================
// V_AddIdle
//==========================
void V_AddIdle( struct ref_params_s *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * v_ipitch_level.value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

//==========================
// V_CalcViewRoll
//==========================
void V_CalcViewRoll( struct ref_params_s *pparams )
{
	float   sign, side, value;
	Vector  right;
	
	cl_entity_t *viewentity = GET_ENTITY( pparams->viewentity );
	if( !viewentity ) return;

	AngleVectors( viewentity->angles, NULL, right, NULL );
	side = DotProduct( pparams->simvel, right );
	sign = side < 0 ? -1 : 1;
	side = fabs( side );
	value = pparams->movevars->rollangle;

	if( side < pparams->movevars->rollspeed )
		side = side * value / pparams->movevars->rollspeed;
	else side = value;

	side = side * sign;		
	pparams->viewangles[ROLL] += side;

	if( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ))
	{
		// only roll the view if the player is dead and the viewheight[2] is nonzero 
		// this is so deadcam in multiplayer will work.
		pparams->viewangles[ROLL] = 80; // dead view angle
		return;
	}
}

//==========================
// V_CalcSendOrigin
//==========================
void V_CalcSendOrigin( struct ref_params_s *pparams )
{
	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// FIXME, we send origin at 1/128 now, change this?
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
	pparams->vieworg[0] += 1.0f / 32;
	pparams->vieworg[1] += 1.0f / 32;
	pparams->vieworg[2] += 1.0f / 32;
}

//==========================
// V_GetChaseOrigin
//==========================
void V_GetChaseOrigin( const Vector &angles, const Vector &origin, float distance, Vector &returnvec )
{
	Vector vecStart, vecEnd;
	pmtrace_t *trace;
	int maxLoops = 8;

	Vector forward, right, up;	

	// trace back from the target using the player's view angles
	AngleVectors( angles, forward, right, up );
	forward = -forward;

	vecStart = origin;
	vecEnd = vecStart + forward * distance;

	int ignoreent = -1;	// first, ignore no entity
	cl_entity_t *ent = NULL;

	while( maxLoops > 0 )
	{
		trace = gEngfuncs.PM_TraceLine( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent );
		if( trace->ent <= 0 ) break; // we hit the world or nothing, stop trace

		ent = GET_ENTITY( PM_GetPhysEntInfo( trace->ent ));
		if( ent == NULL ) break;

		// hit non-player solid BSP, stop here
		if( ent->curstate.solid == SOLID_BSP && !ent->player )
			break;

		// if close enought to end pos, stop, otherwise continue trace
		if(( vecEnd - trace->endpos ).Length() < 1.0f )
		{
			break;
		}
		else
		{
			ignoreent = trace->ent;	// ignore last hit entity
			vecStart = trace->endpos;
		}
		maxLoops--;
	}  

	returnvec = trace->endpos + trace->plane.normal * 8;
}

// remaps an angular variable to a 3 band function:
// 0 <= t < start :		f(t) = 0
// start <= t <= end :	f(t) = end * spline(( t-start) / (end-start) )  // s curve between clamped and linear
// end < t :			f(t) = t
float RemapAngleRange( float startInterval, float endInterval, float value, RemapAngleRange_CurvePart_t *peCurvePart )
{
	// Fixup the roll
	value = AngleNormalize( value );
	float absAngle = fabs(value);

	// beneath cutoff?
	if( absAngle < startInterval )
	{
		if( peCurvePart )
		{
			*peCurvePart = RemapAngleRange_CurvePart_Zero;
		}
		value = 0;
	}
	// in spline range?
	else if( absAngle <= endInterval )
	{
		float newAngle = SimpleSpline(( absAngle - startInterval ) / ( endInterval-startInterval )) * endInterval;

		// grab the sign from the initial value
		if( value < 0 )
		{
			newAngle *= -1;
		}

		if( peCurvePart )
		{
			*peCurvePart = RemapAngleRange_CurvePart_Spline;
		}
		value = newAngle;
	}
	// else leave it alone, in linear range
	else if( peCurvePart )
	{
		*peCurvePart = RemapAngleRange_CurvePart_Linear;
	}

	return value;
}

float ApplyViewLocking( ref_params_t *params, float flAngleRaw, float flAngleClamped, ViewLockData_t &lockData, RemapAngleRange_CurvePart_t eCurvePart )
{
	// If we're set up to never lock this degree of freedom, return the clamped value.
	if( lockData.flLockInterval == 0 )
		return flAngleClamped;

	float flAngleOut = flAngleClamped;

	// Lock the view if we're in the linear part of the curve, and keep it locked
	// until some duration after we return to the flat (zero) part of the curve.
	if(( eCurvePart == RemapAngleRange_CurvePart_Linear ) || ( lockData.bLocked && ( eCurvePart == RemapAngleRange_CurvePart_Spline )))
	{
		lockData.bLocked = true;
		lockData.flUnlockTime = params->time + lockData.flLockInterval;
		flAngleOut = flAngleRaw;
	}
	else
	{
		if(( lockData.bLocked ) && ( params->time > lockData.flUnlockTime ))
		{
			lockData.bLocked = false;
			if ( lockData.flUnlockBlendInterval > 0 )
			{
				lockData.flUnlockTime = params->time;
			}
			else
			{
				lockData.flUnlockTime = 0;
			}
		}

		if ( !lockData.bLocked )
		{
			if( lockData.flUnlockTime != 0 )
			{
				// Blend out from the locked raw view (no remapping) to a remapped view.
				float flBlend = RemapValClamped( params->time-lockData.flUnlockTime, 0, lockData.flUnlockBlendInterval, 0, 1 );
				Msg( "BLEND %f\n", flBlend );

				flAngleOut = Lerp( flBlend, flAngleRaw, flAngleClamped );

				if( flBlend >= 1.0f )
				{
					lockData.flUnlockTime = 0;
				}
			}
			else
			{
				// Not blending out from a locked view to a remapped view.
				Msg( "CLAMPED\n" );
				flAngleOut = flAngleClamped;
			}
		}
		else
		{
			Msg( "STILL LOCKED\n" );
			flAngleOut = flAngleRaw;
		}
	}

	return flAngleOut;
}

void RemapViewAngles( struct ref_params_s *pparams, ViewSmoothingData_t *pData, Vector &vehicleEyeAngles )
{
	Vector vecEyeAnglesRemapped;

	// Clamp pitch.
	RemapAngleRange_CurvePart_t ePitchCurvePart;
	vecEyeAnglesRemapped.x = RemapAngleRange( pData->flPitchCurveZero, pData->flPitchCurveLinear, vehicleEyeAngles.x, &ePitchCurvePart );

	vehicleEyeAngles.z = vecEyeAnglesRemapped.z = AngleNormalize( vehicleEyeAngles.z );

	// Blend out the roll dampening as our pitch approaches 90 degrees, to avoid gimbal lock problems.
	float flBlendRoll = 1.0;

	if( fabs( vehicleEyeAngles.x ) > 60 )
	{
		flBlendRoll = RemapValClamped( fabs( vecEyeAnglesRemapped.x ), 60, 80, 1, 0);
	}

	RemapAngleRange_CurvePart_t eRollCurvePart;
	float flRollDamped = RemapAngleRange( pData->flRollCurveZero, pData->flRollCurveLinear, vecEyeAnglesRemapped.z, &eRollCurvePart );
	vecEyeAnglesRemapped.z = Lerp( flBlendRoll, vecEyeAnglesRemapped.z, flRollDamped );

	//Msg("PITCH ");
	vehicleEyeAngles.x = ApplyViewLocking( pparams, vehicleEyeAngles.x, vecEyeAnglesRemapped.x, pData->pitchLockData, ePitchCurvePart );

	//Msg("ROLL ");
	vehicleEyeAngles.z = ApplyViewLocking( pparams, vehicleEyeAngles.z, vecEyeAnglesRemapped.z, pData->rollLockData, eRollCurvePart );
}

void CalcVehicleRefdef( struct ref_params_s *pparams, int vstate )
{
	ViewSmoothingData_t	*pData = &gHUD.m_ViewSmoothingData;
	cl_entity_t *vehicle = GET_ENTITY( pparams->viewentity );
	int eyeAttachmentIndex = R_StudioLookupAttachment( vehicle, "vehicle_driver_eyes" );
	Vector vecEyeExitEndpoint = vehicle->curstate.endpos;

	Vector vehicleEyeOrigin;
	Vector vehicleEyeAngles;
	R_StudioAttachmentTransform( vehicle, eyeAttachmentIndex, &vehicleEyeOrigin, &vehicleEyeAngles, AF_FORCE_RECALC );
	matrix3x4 vehicleEyePosToWorld = matrix3x4( g_vecZero, vehicleEyeAngles );
	matrix3x4 vehicleTransform = matrix3x4( vehicle->origin, vehicle->angles );
	matrix3x4 v_angleTransform = matrix3x4( g_vecZero, pparams->cl_viewangles );
	v_angleTransform = vehicleTransform.ConcatTransforms( v_angleTransform );

	// Dampen the eye positional change as we drive around.
	pparams->viewangles = v_angleTransform.GetAngles();

	// Started running an entry or exit anim?
	bool bRunningAnim = ( vstate == VEHICLE_ENTERING || vstate == VEHICLE_LEAVING );

	if( bRunningAnim && !pData->bWasRunningAnim )
	{
		pData->bRunningEnterExit = true;
		pData->flEnterExitStartTime = pparams->time;
		pData->flEnterExitDuration = R_StudioSequenceDuration( vehicle, vehicle->curstate.sequence );

		pData->vecOriginSaved = tr.cached_vieworigin;
		pData->vecAnglesSaved = tr.cached_viewangles;

		// Save our initial angular error, which we will blend out over the length of the animation.
		pData->vecAngleDiffSaved.x = AngleDiff( vehicleEyeAngles.x, pData->vecAnglesSaved.x );
		pData->vecAngleDiffSaved.y = AngleDiff( vehicleEyeAngles.y, pData->vecAnglesSaved.y );
		pData->vecAngleDiffSaved.z = AngleDiff( vehicleEyeAngles.z, pData->vecAnglesSaved.z );

		pData->vecAngleDiffMin = pData->vecAngleDiffSaved;
	}

	pData->bWasRunningAnim = bRunningAnim;

	float frac = 0;

	// If we're in an enter/exit animation, blend the player's eye angles to the attachment's
	if( bRunningAnim || pData->bRunningEnterExit )
	{
		pparams->viewangles = vehicleEyeAngles;

		// forward integrate to determine the elapsed time in this entry/exit anim.
		frac = ( pparams->time - pData->flEnterExitStartTime ) / pData->flEnterExitDuration;
		frac = bound( 0.0f, frac, 1.0f );

		if( frac >= 1.0 )
		{
			pData->bRunningEnterExit = false;

			// Enter animation has finished, align view with the eye attachment point
			// so they can start mouselooking around.
			if( vstate != VEHICLE_LEAVING )
			{
				pparams->cl_viewangles = R_StudioAttachmentAngles( vehicle, eyeAttachmentIndex, AF_FORCE_RECALC|AF_LOCAL_SPACE );
			}
		}
	}

	// Compute the relative rotation between the unperturbed eye attachment + the eye angles
	matrix3x4 cameraToWorld = matrix3x4( g_vecZero, pparams->viewangles );
	matrix3x4 worldToEyePos = vehicleEyePosToWorld.Transpose();
	matrix3x4 vehicleCameraToEyePos = worldToEyePos.ConcatTransforms( cameraToWorld );

	// Damp out some of the vehicle motion (neck/head would do this)
	if( pData->bClampEyeAngles )
	{
		RemapViewAngles( pparams, pData, vehicleEyeAngles );
	}

	vehicleEyePosToWorld = matrix3x4( vehicleEyeOrigin, vehicleEyeAngles );

	// Now treat the relative eye angles as being relative to this new, perturbed view position...
	matrix3x4 newCameraToWorld = vehicleEyePosToWorld.ConcatTransforms( vehicleCameraToEyePos );

	// output new view abs angles
	newCameraToWorld.GetOrigin( pparams->vieworg ); 
	newCameraToWorld.GetAngles( pparams->viewangles ); 

	// If we're playing an entry or exit animation...
	if( bRunningAnim || pData->bRunningEnterExit )
	{
		float flSplineFrac = bound( 0, SimpleSpline( frac ), 1 );

		// Blend out the error between the player's initial eye angles and the animation's initial
		// eye angles over the duration of the animation. 
		Vector vecAngleDiffBlend = (( 1 - flSplineFrac ) * pData->vecAngleDiffSaved );

		// If our current error is less than the error amount that we're blending 
		// out, use that. This lets the angles converge as quickly as possible.
		Vector vecAngleDiffCur;
		vecAngleDiffCur.x = AngleDiff( vehicleEyeAngles.x, pData->vecAnglesSaved.x );
		vecAngleDiffCur.y = AngleDiff( vehicleEyeAngles.y, pData->vecAnglesSaved.y );
		vecAngleDiffCur.z = AngleDiff( vehicleEyeAngles.z, pData->vecAnglesSaved.z );

		// In either case, never increase the error, so track the minimum error and clamp to that.
		for( int i = 0; i < 3; i++ )
		{
			if( fabs( vecAngleDiffCur[i] ) < fabs( pData->vecAngleDiffMin[i] ))
			{
				pData->vecAngleDiffMin[i] = vecAngleDiffCur[i];
			}

			if( fabs(vecAngleDiffBlend[i] ) < fabs( pData->vecAngleDiffMin[i] ))
			{
				pData->vecAngleDiffMin[i] = vecAngleDiffBlend[i];
			}
		}

		// Add the error to the animation's eye angles.
		pparams->viewangles -= pData->vecAngleDiffMin;

		// Use this as the basis for the next error calculation.
		pData->vecAnglesSaved = v_angleTransform.GetAngles();

		Vector vecAbsOrigin = pparams->vieworg;

		// If we're exiting, our desired position is the server-sent exit position
		if( vstate == VEHICLE_LEAVING )
		{
			// Blend to the exit position
			pparams->vieworg = Lerp( flSplineFrac, vecAbsOrigin, vecEyeExitEndpoint );
		}
		else
		{
			// Blend from our starting position to the desired origin
			pparams->vieworg = Lerp( flSplineFrac, pData->vecOriginSaved, vecAbsOrigin );
		}
	}
}

void V_ResetCarSmoothData( void )
{
	ViewSmoothingData_t	*pData = &gHUD.m_ViewSmoothingData;

	// reset car smoothing data
	if( pData->bRunningEnterExit || pData->bWasRunningAnim )
	{
		pData->bRunningEnterExit = false;
		pData->bWasRunningAnim = false;
	}
}

//==========================
// V_CalcCameraRefdef
//==========================
void V_CalcCameraRefdef( struct ref_params_s *pparams )
{
	static float lasttime, oldz = 0;

	// get viewentity and monster eyeposition
	cl_entity_t *view = GET_ENTITY( pparams->viewentity );

 	if( view )
	{		 
		int vstate = atoi( gEngfuncs.PhysInfo_ValueForKey( "incar" ));

		if( vstate != VEHICLE_INACTIVE )
		{
			CalcVehicleRefdef( pparams, vstate );
		}
		else
		{
			pparams->vieworg = view->origin;
			pparams->viewangles = view->angles;

			studiohdr_t *viewmonster = (studiohdr_t *)IEngineStudio.Mod_Extradata( view->model );

			if( viewmonster && view->curstate.eflags & EFLAG_SLERP )
			{
				Vector forward;
				AngleVectors( pparams->viewangles, forward, NULL, NULL );

				Vector viewpos = viewmonster->eyeposition;

				if( viewpos == g_vecZero )
					viewpos = Vector( 0, 0, 8 );	// monster_cockroach

				pparams->vieworg += viewpos + forward * 8;	// best value for humans
				// NOTE: fov computation moved into r_main.cpp
			}

			// this is smooth stair climbing in thirdperson mode but not affected for client model :(
			if( !pparams->smoothing && pparams->onground && view->origin[2] - oldz > 0.0f && viewmonster != NULL )
			{
				float steptime;
		
				steptime = pparams->time - lasttime;
				if( steptime < 0 ) steptime = 0;

				oldz += steptime * 150.0f;

				if( oldz > view->origin[2] )
					oldz = view->origin[2];
				if( view->origin[2] - oldz > pparams->movevars->stepsize )
					oldz = view->origin[2] - pparams->movevars->stepsize;

				pparams->vieworg[2] += oldz - view->origin[2];
			}
			else
			{
				oldz = view->origin[2];
			}

			lasttime = pparams->time;
			V_ResetCarSmoothData();
		}                    	

		if( view->curstate.effects & EF_NUKE_ROCKET )
			pparams->viewangles.x = -pparams->viewangles.x; // stupid quake bug!

		// g-cont. apply shake to camera
		gEngfuncs.V_CalcShake();
		gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0f );
	}
}

#define MAX_SPOTS	16

//==========================
// V_FindIntermisionSpot
//==========================
cl_entity_t *V_FindIntermisionSpot( struct ref_params_s *pparams )
{
	int i, j, spotindex[MAX_SPOTS];
	cl_entity_t *ent;

	// found intermission point
	for( i = 0, j = 0; i < pparams->max_entities; i++ )
	{
		ent = GET_ENTITY( i );

		if( ent && ent->curstate.eflags & EFLAG_INTERMISSION )
		{
			spotindex[j] = ent->index;
			if( ++j >= MAX_SPOTS ) break; // full
		}
	}	
	
	// ok, we have list of intermission spots
	if( j != 0 )
	{
		if( j > 1 ) j = RANDOM_LONG( 0, j );
		ent = GET_ENTITY( spotindex[j-1] );
	}
	else
	{
		// defaulted to player view
		ent = gEngfuncs.GetLocalPlayer();
	}	

	return ent;
}

//==========================
// V_CalcIntermisionRefdef
//==========================
void V_CalcIntermisionRefdef( struct ref_params_s *pparams )
{
          if( !v_intermission_spot )
          	v_intermission_spot = V_FindIntermisionSpot( pparams );

	pparams->vieworg = v_intermission_spot->origin;
	pparams->viewangles = v_intermission_spot->angles;

	cl_entity_t *view = GET_VIEWMODEL();	
	view->model = NULL;

	// allways idle in intermission
	float old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle( pparams );
	v_idlescale = old;
}

//==========================
// V_CalcThirdPersonRefdef
//==========================
void V_CalcThirdPersonRefdef( struct ref_params_s * pparams )
{
	static float lasttime, oldz = 0;

	pparams->vieworg = pparams->simorg;
	pparams->vieworg += pparams->viewheight;
	pparams->viewangles = pparams->cl_viewangles;

	V_CalcSendOrigin( pparams );

	// this is smooth stair climbing in thirdperson mode but not affected for client model :(
	if( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0.0f )
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if( steptime < 0 ) steptime = 0;

		oldz += steptime * 150.0f;

		if( oldz > pparams->simorg[2] )
			oldz = pparams->simorg[2];
		if( pparams->simorg[2] - oldz > pparams->movevars->stepsize )
			oldz = pparams->simorg[2] - pparams->movevars->stepsize;

		pparams->vieworg[2] += oldz - pparams->simorg[2];
	}
	else
	{
		oldz = pparams->simorg[2];
	}

	lasttime = pparams->time;

	V_GetChaseOrigin( pparams->viewangles, pparams->vieworg, cl_chasedist->value, pparams->vieworg );

	float pitch = pparams->viewangles[PITCH];

	// normalize angles
	if( pitch > 180.0f ) 
		pitch -= 360.0f;
	else if( pitch < -180.0f )
		pitch += 360.0f;

	// player pitch is inverted
	pitch /= -3.0;

	cl_entity_t *ent = GET_LOCAL_PLAYER();

	// slam local player's pitch value
	ent->angles[PITCH] = pitch;
	ent->curstate.angles[PITCH] = pitch;
	ent->prevstate.angles[PITCH] = pitch;
	ent->latched.prevangles[PITCH] = pitch;
}

//==========================
// V_CalcWaterLevel
//==========================
float V_CalcWaterLevel( struct ref_params_s *pparams )
{
	float waterOffset = 0.0f;
	
	if( pparams->waterlevel >= 2 )
	{
		int waterEntity = WATER_ENTITY( pparams->simorg );
		float waterDist = cl_waterdist->value;

		if( waterEntity >= 0 && waterEntity < pparams->max_entities )
		{
			cl_entity_t *pwater = GET_ENTITY( waterEntity );
			if( pwater && ( pwater->model != NULL ))
				waterDist += ( pwater->curstate.scale * 16.0f );
		}

		Vector point = pparams->vieworg;

		// eyes are above water, make sure we're above the waves
		if( pparams->waterlevel == 2 )	
		{
			point.z -= waterDist;

			for( int i = 0; i < waterDist; i++ )
			{
				int contents = POINT_CONTENTS( point );
				if( contents > CONTENTS_WATER )
					break;
				point.z += 1;
			}
			waterOffset = (point.z + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water. Make sure we're far enough under
			point[2] += waterDist;

			for( int i = 0; i < waterDist; i++ )
			{
				int contents = POINT_CONTENTS( point );
				if( contents <= CONTENTS_WATER )
					break;

				point.z -= 1;
			}
			waterOffset = (point.z - waterDist) - pparams->vieworg[2];
		}
	}

	return waterOffset;
}

#define ORIGIN_BACKUP	64
#define ORIGIN_MASK		( ORIGIN_BACKUP - 1 )

typedef struct 
{
	Vector	Origins[ORIGIN_BACKUP];
	float	OriginTime[ORIGIN_BACKUP];

	Vector	Angles[ORIGIN_BACKUP];
	float	AngleTime[ORIGIN_BACKUP];

	int	CurrentOrigin;
	int	CurrentAngle;
} viewinterp_t;

//==========================
// V_InterpolatePos
//==========================
void V_InterpolatePos( struct ref_params_s *pparams )
{
	static Vector lastorg;
	static viewinterp_t	ViewInterp;

	// view is the weapon model (only visible from inside body )
	cl_entity_t *view = GET_VIEWMODEL();

	Vector delta = pparams->simorg - lastorg;

	if( delta.Length() != 0.0f )
	{
		ViewInterp.Origins[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->simorg;
		ViewInterp.OriginTime[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
		ViewInterp.CurrentOrigin++;

		lastorg = pparams->simorg;
	}

	if( cl_vsmoothing->value && pparams->smoothing && ( pparams->maxclients > 1 ))
	{
		int i, foundidx;
		float t;

		if( cl_vsmoothing->value < 0.0f )
			CVAR_SET_FLOAT( "cl_vsmoothing", 0.0 );

		t = pparams->time - cl_vsmoothing->value;

		for( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;

			if( ViewInterp.OriginTime[foundidx & ORIGIN_MASK] <= t )
				break;
		}

		if( i < ORIGIN_MASK &&  ViewInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0f )
		{
			// Interpolate
			Vector neworg;
			double dt, frac;

			dt = ViewInterp.OriginTime[(foundidx + 1) & ORIGIN_MASK] - ViewInterp.OriginTime[foundidx & ORIGIN_MASK];
			if( dt > 0.0f )
			{
				frac = ( t - ViewInterp.OriginTime[foundidx & ORIGIN_MASK] ) / dt;
				delta = ViewInterp.Origins[( foundidx + 1 ) & ORIGIN_MASK] - ViewInterp.Origins[foundidx & ORIGIN_MASK];
				frac = Q_min( 1.0, frac );

				neworg = ViewInterp.Origins[foundidx & ORIGIN_MASK] + delta * frac;

				// dont interpolate large changes (less than 64 units)
				if( delta.Length() < 64 )
				{
					delta = neworg - pparams->simorg;
					pparams->simorg += delta;
					pparams->vieworg += delta;
					view->origin += delta;
				}
			}
		}
	}
}

//==========================
// V_CalcFirstPersonRefdef
//==========================
void V_CalcFirstPersonRefdef( struct ref_params_s *pparams )
{
	V_DriftPitch( pparams );

	float bob = V_CalcBob( pparams );

	pparams->vieworg = pparams->simorg;
	pparams->vieworg += pparams->viewheight;
	pparams->vieworg.z += bob;

	pparams->viewangles = pparams->cl_viewangles;

	gEngfuncs.V_CalcShake();
	gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0f );

	V_CalcSendOrigin( pparams );

	float waterOffset = V_CalcWaterLevel( pparams );
	pparams->vieworg[2] += waterOffset;

	V_CalcViewRoll( pparams );
	V_AddIdle( pparams );

	// offsets
	AngleVectors( pparams->cl_viewangles, pparams->forward, pparams->right, pparams->up );

	cl_entity_t *view = GET_VIEWMODEL();
	Vector lastAngles = view->angles = pparams->cl_viewangles;

	V_CalcGunAngle( pparams );

	// use predicted origin as view origin.
	view->origin = pparams->simorg;      
	view->origin += pparams->viewheight;
	view->origin.z += waterOffset;

	// Let the viewmodel shake at about 10% of the amplitude
	gEngfuncs.V_ApplyShake( view->origin, view->angles, 0.9f );

	view->origin += pparams->forward * bob * 0.4f;
	view->origin.z += bob;

	view->angles[PITCH] -= bob * 0.3f;
	view->angles[YAW] -= bob * 0.5f;
	view->angles[ROLL] -= bob * 1.0f;
	view->origin.z -= 1;

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if( pparams->viewsize == 110 )
	{
		view->origin[2] += 1;
	}
	else if( pparams->viewsize == 100 )
	{
		view->origin[2] += 2;
	}
	else if( pparams->viewsize == 90 )
	{
		view->origin[2] += 1;
	}
	else if( pparams->viewsize == 80 )
	{
		view->origin[2] += 0.5;
	}

	V_CalcViewModelLag( pparams, view->origin, view->angles, lastAngles );
		
	pparams->viewangles += pparams->punchangle;

	static float lasttime, oldz = 0;

	if( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0.0f )
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if( steptime < 0 ) steptime = 0;

		oldz += steptime * 150.0f;

		if( oldz > pparams->simorg[2] )
			oldz = pparams->simorg[2];
		if( pparams->simorg[2] - oldz > pparams->movevars->stepsize )
			oldz = pparams->simorg[2] - pparams->movevars->stepsize;

		pparams->vieworg[2] += oldz - pparams->simorg[2];
		view->origin.z += oldz - pparams->simorg[2];
	}
	else
	{
		oldz = pparams->simorg[2];
	}

	lasttime = pparams->time;

	// smooth player view in multiplayer
	V_InterpolatePos( pparams );
}

//==========================
// V_CalcRefdef
//==========================
void V_CalcRefdef( struct ref_params_s *pparams )
{
	// store a local copy in case we need to calc firstperson later
	memcpy( &tr.viewparams, pparams, sizeof( ref_params_t ));

	pause = pparams->paused;
	if( pause ) return;

	if( pparams->intermission )
	{
		V_CalcIntermisionRefdef( pparams );
		V_ResetCarSmoothData();
	}
	else if( pparams->viewentity > pparams->maxclients )
	{
		V_CalcCameraRefdef( pparams );
	}
	else if( gHUD.m_iCameraMode )
	{
		V_CalcThirdPersonRefdef( pparams );
		V_ResetCarSmoothData();
	}
	else
	{
		V_CalcFirstPersonRefdef( pparams );
		V_ResetCarSmoothData();
	}
}
