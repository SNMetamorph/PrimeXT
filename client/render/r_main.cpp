/*
r_main.cpp - renderer main loop
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
#include <stringlib.h>
#include "pm_movevars.h"
#include "mathlib.h"
#include "r_studio.h"
#include "r_sprite.h"
#include "r_particle.h"
#include "entity_types.h"
#include "event_api.h"
#include "r_weather.h"
#include "xash3d_features.h"
#include "r_shader.h"
#include "r_world.h"
#include "pm_defs.h"

#define IsLiquidContents( cnt )	( cnt == CONTENTS_WATER || cnt == CONTENTS_SLIME || cnt == CONTENTS_LAVA )

ref_globals_t	tr;
ref_stats_t	r_stats;
char		r_speeds_msg[2048];
char		r_depth_msg[2048];
ref_instance_t	*RI = &glState.stack[0];
float		gldepthmin, gldepthmax;
model_t		*worldmodel = NULL;	// must be set at begin each frame
BOOL		g_fRenderInitialized = FALSE;
int		r_currentMessageNum = 0;

const char *R_GetNameForView( void )
{
	static char	passName[256];

	passName[0] = '\0';

	if( FBitSet( RI->params, RP_MIRRORVIEW ))
		Q_strncat( passName, "mirror ", sizeof( passName ));
	if( FBitSet( RI->params, RP_ENVVIEW ))
		Q_strncat( passName, "cubemap ", sizeof( passName ));	
	if( FBitSet( RI->params, RP_SKYPORTALVIEW ))
		Q_strncat( passName, "3dsky ", sizeof( passName ));	
	if( FBitSet( RI->params, RP_PORTALVIEW ))
		Q_strncat( passName, "portal ", sizeof( passName ));	
	if( FBitSet( RI->params, RP_SCREENVIEW ))
		Q_strncat( passName, "screen ", sizeof( passName ));	
	if( FBitSet( RI->params, RP_SHADOWVIEW ))
		Q_strncat( passName, "shadow ", sizeof( passName ));
	if( RP_NORMALPASS( ))
		Q_strncat( passName, "general ", sizeof( passName ));

	return passName;	
}

void R_InitRefState( void )
{
	memset( glState.stack, 0, sizeof( glState.stack ));
	glState.stack_position = 0;

	RI = &glState.stack[glState.stack_position];
}

void R_ResetRefState( void )
{
	ref_instance_t	*prevRI;

	ASSERT( glState.stack_position > 0 );

	prevRI = &glState.stack[glState.stack_position - 1];
	RI = &glState.stack[glState.stack_position];

	// copy params from old refresh
	RI->params = prevRI->params;
	RI->viewentity = prevRI->viewentity;
	RI->fov_x = prevRI->fov_x;
	RI->fov_y = prevRI->fov_y;
	memcpy( RI->viewport, prevRI->viewport, sizeof( int[4] ));
	RI->frustum = prevRI->frustum;
	RI->pvsorigin = prevRI->pvsorigin;
	RI->viewangles = prevRI->viewangles;
	RI->vieworg = prevRI->vieworg;
	RI->vforward = prevRI->vforward;
	RI->vright = prevRI->vright;
	RI->vup = prevRI->vup;

	// store PVS data
	RI->viewleaf = prevRI->viewleaf;
	RI->oldviewleaf = prevRI->oldviewleaf;
	memcpy( RI->visbytes, prevRI->visbytes, MAX_MAP_LEAFS / 8 );

	// reset some global pointers
	RI->num_subview_faces = 0;
	RI->currententity = NULL;
	RI->currentmodel = NULL;
	RI->currentlight = NULL;
	RI->reject_face = NULL;
	GL_BindShader( NULL );
}

void R_PushRefState( void )
{
	if( ++glState.stack_position >= MAX_REF_STACK )
		HOST_ERROR( "Refresh stack overflow\n" );

	RI = &glState.stack[glState.stack_position];
	R_ResetRefState();
}

void R_PopRefState( void )
{
	if( --glState.stack_position < 0 )
		HOST_ERROR( "Refresh stack underflow\n" );
	RI = &glState.stack[glState.stack_position];
}

ref_instance_t *R_GetPrevInstance( void )
{
	ASSERT( glState.stack_position > 0 );

	return &glState.stack[glState.stack_position - 1];
}

static int R_RankForRenderMode( int rendermode )
{
	switch( rendermode )
	{
	case kRenderTransTexture:
		return 1;	// draw second
	case kRenderTransAdd:
		return 2;	// draw third
	case kRenderGlow:
		return 3;	// must be last!
	}
	return 0;
}

void R_AllowFog( int allowed )
{
	static int	isFogEnabled;

	if( allowed )
	{
		if( isFogEnabled )
			pglEnable( GL_FOG );
	}
	else
	{
		isFogEnabled = pglIsEnabled( GL_FOG );

		if( isFogEnabled )
			pglDisable( GL_FOG );
	}
}

/*
===============
R_OpaqueEntity

Opaque entity can be brush or studio model but sprite
===============
*/
static qboolean R_OpaqueEntity( cl_entity_t *ent )
{
	int rendermode = R_GetEntityRenderMode( ent );

	if( R_ModelOpaque( rendermode ))
		return true;
	return false;
}

/*
===============
R_TransEntityCompare

Sorting translucent entities by rendermode then by distance
===============
*/
static int R_TransEntityCompare( const cl_entity_t **a, const cl_entity_t **b )
{
	cl_entity_t *ent1 = (cl_entity_t *)*a;
	cl_entity_t *ent2 = (cl_entity_t *)*b;
	int rendermode1 = R_GetEntityRenderMode( ent1 );
	int rendermode2 = R_GetEntityRenderMode( ent2 );
	float dist1, dist2;

	// sort by distance
	if( ent1->model->type != mod_brush || rendermode1 != kRenderTransAlpha )
	{
		Vector org = ent1->origin + ((ent1->model->mins + ent1->model->maxs) * 0.5f);
		Vector vecLen = RI->vieworg - org;
		dist1 = DotProduct( vecLen, vecLen );
	}
	else dist1 = 1000000000;

	if( ent2->model->type != mod_brush || rendermode2 != kRenderTransAlpha )
	{
		Vector org = ent2->origin + ((ent2->model->mins + ent2->model->maxs) * 0.5f);
		Vector vecLen = RI->vieworg - org;
		dist2 = DotProduct( vecLen, vecLen );
	}
	else dist2 = 1000000000;

	if( dist1 > dist2 )
		return -1;
	if( dist1 < dist2 )
		return 1;

	// then sort by rendermode
	if( R_RankForRenderMode( rendermode1 ) > R_RankForRenderMode( rendermode2 ))
		return 1;
	if( R_RankForRenderMode( rendermode1 ) < R_RankForRenderMode( rendermode2 ))
		return -1;

	return 0;
}

qboolean R_WorldToScreen( const Vector &point, Vector &screen )
{
	matrix4x4	worldToScreen;
	qboolean	behind;
	float	w;

	worldToScreen = RI->worldviewProjectionMatrix;

	screen[0] = worldToScreen[0][0] * point[0] + worldToScreen[1][0] * point[1] + worldToScreen[2][0] * point[2] + worldToScreen[3][0];
	screen[1] = worldToScreen[0][1] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[2][1] * point[2] + worldToScreen[3][1];
	w = worldToScreen[0][3] * point[0] + worldToScreen[1][3] * point[1] + worldToScreen[2][3] * point[2] + worldToScreen[3][3];
	screen[2] = 0.0f; // just so we have something valid here

	if( w < 0.001f )
	{
		screen[0] *= 100000;
		screen[1] *= 100000;
		behind = true;
	}
	else
	{
		float invw = 1.0f / w;
		screen[0] *= invw;
		screen[1] *= invw;
		behind = false;
	}

	return behind;
}

void R_ScreenToWorld( const Vector &screen, Vector &point )
{
	matrix4x4	screenToWorld;
	Vector	temp;
	float	w;

	screenToWorld = RI->worldviewProjectionMatrix.InvertFull();

	temp[0] = 2.0f * (screen[0] - RI->viewport[0]) / RI->viewport[2] - 1;
	temp[1] = -2.0f * (screen[1] - RI->viewport[1]) / RI->viewport[3] + 1;
	temp[2] = 0.0f; // just so we have something valid here

	point[0] = temp[0] * screenToWorld[0][0] + temp[1] * screenToWorld[0][1] + temp[2] * screenToWorld[0][2] + screenToWorld[0][3];
	point[1] = temp[0] * screenToWorld[1][0] + temp[1] * screenToWorld[1][1] + temp[2] * screenToWorld[1][2] + screenToWorld[1][3];
	point[2] = temp[0] * screenToWorld[2][0] + temp[1] * screenToWorld[2][1] + temp[2] * screenToWorld[2][2] + screenToWorld[2][3];
	w = temp[0] * screenToWorld[3][0] + temp[1] * screenToWorld[3][1] + temp[2] * screenToWorld[3][2] + screenToWorld[3][3];
	if( w ) point *= ( 1.0f / w );
}

/*
===============
R_CheckChanges
===============
*/
void R_CheckChanges( void )
{
	static bool	fog_enabled_old;
	static float	waveheight_old;
	bool		settings_changed = false;

	if( FBitSet( r_recursion_depth->flags, FCVAR_CHANGED ))
	{
		float depth = bound( 0.0f, r_recursion_depth->value, MAX_REF_STACK - 2 );
		CVAR_SET_FLOAT( "gl_recursion_depth", depth );
		ClearBits( r_recursion_depth->flags, FCVAR_CHANGED );
	}

	if( FBitSet( r_drawentities->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_drawentities->flags, FCVAR_CHANGED );
		tr.params_changed = true;
	}

	if( FBitSet( r_lightmap->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_lightmap->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_allow_mirrors->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_allow_mirrors->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_allow_portals->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_allow_portals->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_allow_screens->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_allow_screens->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_detailtextures->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_detailtextures->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_fullbright->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_fullbright->flags, FCVAR_CHANGED );
		R_StudioClearLightCache();
		settings_changed = true;
	}

	if( FBitSet( r_lighting_modulate->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_lighting_modulate->flags, FCVAR_CHANGED );
		R_StudioClearLightCache();
		settings_changed = true;
	}

	if( FBitSet( r_test->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_test->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_recursive_world_node->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_recursive_world_node->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_grass->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_grass->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( r_shadows->flags, FCVAR_CHANGED ))
	{
		ClearBits( r_shadows->flags, FCVAR_CHANGED );
		settings_changed = true;
	}

	if( FBitSet( vid_gamma->flags, FCVAR_CHANGED ) || FBitSet( vid_brightness->flags, FCVAR_CHANGED ))
	{
		for( int i = 0; i < worldmodel->numsurfaces; i++ )
			SetBits( worldmodel->surfaces[i].flags, SURF_LM_UPDATE );
		R_StudioClearLightCache();
		settings_changed = true;
	}

	if( tr.fogEnabled != fog_enabled_old )
	{
		fog_enabled_old = tr.fogEnabled;
		settings_changed = true;
	}

	if( tr.movevars->waveHeight != waveheight_old )
	{
		waveheight_old = tr.movevars->waveHeight;
		settings_changed = true;
	}

	if( settings_changed )
	{
		tr.glsl_valid_sequence++; // now all uber-shaders are invalidate and possible need for recompile
		tr.params_changed = true;
	}
}

/*
===============
CL_FxBlend
===============
*/
int CL_FxBlend( cl_entity_t *e )
{
	int	blend = 0;
	float	offset, dist;
	Vector	tmp;

	if( RENDER_GET_PARM( PARAM_GAMEPAUSED, 0 ))
		return e->curstate.renderamt;

	offset = ((int)e->index ) * 363.0f; // Use ent index to de-sync these fx

	switch( e->curstate.renderfx ) 
	{
	case kRenderFxPulseSlowWide:
		blend = e->curstate.renderamt + 0x40 * sin( tr.time * 2 + offset );	
		break;
	case kRenderFxPulseFastWide:
		blend = e->curstate.renderamt + 0x40 * sin( tr.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = e->curstate.renderamt + 0x10 * sin( tr.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = e->curstate.renderamt + 0x10 * sin( tr.time * 8 + offset );
		break;
	case kRenderFxFadeSlow:			
		if( RP_NORMALPASS( ))
		{
			if( e->curstate.renderamt > 0 ) 
				e->curstate.renderamt -= 1;
			else e->curstate.renderamt = 0;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		if( RP_NORMALPASS( ))
		{
			if( e->curstate.renderamt > 3 ) 
				e->curstate.renderamt -= 4;
			else e->curstate.renderamt = 0;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		if( RP_NORMALPASS( ))
		{
			if( e->curstate.renderamt < 255 ) 
				e->curstate.renderamt += 1;
			else e->curstate.renderamt = 255;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		if( RP_NORMALPASS( ))
		{
			if( e->curstate.renderamt < 252 ) 
				e->curstate.renderamt += 4;
			else e->curstate.renderamt = 255;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( tr.time * 4 + offset );
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( tr.time * 16 + offset );
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( tr.time * 36 + offset );
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( tr.time * 2 ) + sin( tr.time * 17 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin( tr.time * 16 ) + sin( tr.time * 23 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = e->curstate.renderamt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		tmp = e->origin - RI->vieworg;
		dist = DotProduct( tmp, RI->vforward );
			
		// Turn off distance fade
		if( e->curstate.renderfx == kRenderFxDistort )
			dist = 1;

		if( dist <= 0 )
		{
			blend = 0;
		}
		else 
		{
			e->curstate.renderamt = 180;
			if( dist <= 100 ) blend = e->curstate.renderamt;
			else blend = (int) ((1.0f - ( dist - 100 ) * ( 1.0f / 400.0f )) * e->curstate.renderamt );
			blend += RANDOM_LONG( -32, 31 );
		}
		break;
	default:
		blend = e->curstate.renderamt;
		break;	
	}

	blend = bound( 0, blend, 255 );

	return blend;
}

/*
===============
GL_CacheState

build matrix pack or find already existed
===============
*/
word GL_CacheState( const Vector &origin, const Vector &angles )
{
	matrix4x4	modelMatrix;
	GLfloat	findMatrix[16];
	int	i;

	modelMatrix = matrix4x4( origin, angles, 1.0f );
	modelMatrix.CopyToArray( findMatrix );

	for( i = 0; i < tr.num_cached_states; i++ )
	{
		if( !memcmp( tr.cached_state[i].modelMatrix, findMatrix, sizeof( findMatrix )))
			return i;	// already cached?
	}

	if( tr.num_cached_states >= MAX_CACHED_STATES )
	{
		ALERT( at_error, "too many brush entities in frame\n" );
		return WORLD_MATRIX;
	}

	// store results
	modelMatrix.CopyToArray( tr.cached_state[i].modelMatrix );
	tr.cached_state[i].transform = modelMatrix;

	return tr.num_cached_states++;
}

/*
===============
R_ClearScene
===============
*/
void R_ClearScene( void )
{
	tr.time = GET_CLIENT_TIME();
	tr.oldtime = GET_CLIENT_OLDTIME();

	if( cl_viewsize != NULL && cl_viewsize->value != 120.0f )
		CVAR_SET_FLOAT( "viewsize", 120.0f );

	memset( &r_stats, 0, sizeof( r_stats ));
	tr.num_cached_states = 0;

	R_CheckChanges();

	CL_DecayLights();

	R_AnimateLight();

	// world matrix is bound to 0
	GET_ENTITY( 0 )->hCachedMatrix = GL_CacheState( g_vecZero, g_vecZero );
	GET_ENTITY( 0 )->curstate.messagenum = r_currentMessageNum;

	tr.num_solid_entities = tr.num_trans_entities = 0;
	tr.local_client_added = false;
	tr.num_shadows_used = 0;
	tr.sky_camera = NULL;
}

/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType )
{
	if( RENDER_GET_PARM( PARAM_GAMEPAUSED, 0 ))
		return true;

	if( FBitSet( clent->curstate.effects, EF_PROJECTED_LIGHT|EF_DYNAMIC_LIGHT ))
		return true; // no reason to drawing this entity

	if( !CVAR_TO_BOOL( r_drawentities ))
		return false; // not allow to drawing

	if( !clent || !clent->model )
		return false; // if set to invisible, skip

	if( FBitSet( clent->curstate.effects, EF_NODRAW ))
		return false; // done

	if( !R_ModelOpaque( clent->curstate.rendermode ) && CL_FxBlend( clent ) <= 0 )
		return true; // invisible

	if( entityType == ET_PLAYER && RP_LOCALCLIENT( clent ))
	{
		if( tr.local_client_added )
			return false; // already present in list
		tr.local_client_added = true;
	}

	if( entityType == ET_FRAGMENTED )
	{
		// non-solid statics wants a new lighting too :-)
		SetBits( clent->curstate.iuser1, CF_STATIC_ENTITY );
		r_stats.c_client_ents++;
	}

	if( R_OpaqueEntity( clent ))
	{
		// opaque
		if( tr.num_solid_entities >= MAX_VISIBLE_PACKET )
			return false;

		tr.solid_entities[tr.num_solid_entities] = clent;
		tr.num_solid_entities++;
	}
	else
	{
		// translucent
		if( tr.num_trans_entities >= MAX_VISIBLE_PACKET )
			return false;

		tr.trans_entities[tr.num_trans_entities] = clent;
		tr.num_trans_entities++;
	}

	if( clent->model->type == mod_brush )
		clent->hCachedMatrix = GL_CacheState( clent->origin, clent->angles );

	if( entityType == ET_FRAGMENTED )
	{
		clent->curstate.messagenum = r_currentMessageNum;
		clent->visframe = tr.realframecount;
	}

	return true;
}

/*
=============
R_Clear
=============
*/
static void R_Clear( int bitMask )
{
	int	bits;

	if( FBitSet( RI->params, RP_OVERVIEW ))
		pglClearColor( 0.0f, 1.0f, 0.0f, 1.0f ); // green background (Valve rules)
	else pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );

	bits = GL_DEPTH_BUFFER_BIT;

	// force to clearing screen to avoid ugly blur
	if( tr.fClearScreen && !CVAR_TO_BOOL( r_clear ))
		bits |= GL_COLOR_BUFFER_BIT;
#if 0
	if( glState.stencilEnabled )
		bits |= GL_STENCIL_BUFFER_BIT;
#endif
	bits &= bitMask;

	pglClear( bits );

	// change ordering for overview
	if( FBitSet( RI->params, RP_OVERVIEW ))
	{
		gldepthmin = 1.0f;
		gldepthmax = 0.0f;
	}
	else
	{
		if( GL_Support( R_PARANOIA_EXT ))
			gldepthmin = 0.0001f;
		else gldepthmin = 0.0f;
		gldepthmax = 1.0f;
	}

	pglDepthFunc( GL_LEQUAL );
	pglDepthRange( gldepthmin, gldepthmax );
}


void R_RestoreGLState( void )
{
	pglViewport( RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->worldviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );
	GL_Cull( GL_FRONT );
}

//=============================================================================
/*
===============
R_GetFarClip
===============
*/
static float R_GetFarClip( void )
{
	return tr.movevars->zmax * 1.73f;
}

/*
===============
R_SetupFrustum
===============
*/
void R_SetupFrustum( void )
{
	const ref_overview_t *ov = GET_OVERVIEW_PARMS();
	matrix4x4 cullMatrix;

	if( RP_NORMALPASS() && ( tr.viewparams.waterlevel >= 3 ))
	{
		RI->fov_x = atan( tan( DEG2RAD( RI->fov_x ) / 2 ) * ( 0.97 + sin( tr.time * 1.5 ) * 0.03 )) * 2 / (M_PI / 180.0);
		RI->fov_y = atan( tan( DEG2RAD( RI->fov_y ) / 2 ) * ( 1.03 - sin( tr.time * 1.5 ) * 0.03 )) * 2 / (M_PI / 180.0);
	}

	// build the transformation matrix for the given view angles
	AngleVectors( RI->viewangles, RI->vforward, RI->vright, RI->vup );
	RI->farClip = Q_max( 256.0f, R_GetFarClip());
	cullMatrix.SetForward( RI->vforward );
	cullMatrix.SetRight( RI->vright );
	cullMatrix.SetUp( RI->vup );
	cullMatrix.SetOrigin( RI->vieworg );

	if( FBitSet( RI->params, RP_OVERVIEW ))
		RI->frustum.InitOrthogonal( cullMatrix, ov->xLeft, ov->xRight, ov->yTop, ov->yBottom, ov->zNear, ov->zFar );
	else RI->frustum.InitProjection( cullMatrix, 0.0f, RI->farClip, RI->fov_x, RI->fov_y ); // NOTE: we ignore nearplane here (mirrors only)

	tr.lodScale = tan( DEG2RAD( RI->fov_x ) * 0.5f );
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m )
{
	if( FBitSet( RI->params, RP_OVERVIEW ))
	{
		const ref_overview_t *ov = GET_OVERVIEW_PARMS();

		m.CreateOrtho( ov->xLeft, ov->xRight, ov->yTop, ov->yBottom, ov->zNear, ov->zFar );
	}
	else
	{
		m.CreateProjection( fov_x, fov_y, Z_NEAR, RI->farClip );
	}
}

/*
=============
R_SetupModelviewMatrix
=============
*/
void R_SetupModelviewMatrix( matrix4x4 &m )
{
	m.CreateModelview(); // init quake world orientation
	m.ConcatRotate( -RI->viewangles[2], 1, 0, 0 );
	m.ConcatRotate( -RI->viewangles[0], 0, 1, 0 );
	m.ConcatRotate( -RI->viewangles[1], 0, 0, 1 );
	m.ConcatTranslate( -RI->vieworg[0], -RI->vieworg[1], -RI->vieworg[2] );
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	if( tr.modelviewIdentity ) return;

	RI->objectMatrix.Identity();
	RI->modelviewMatrix = RI->worldviewMatrix;

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->modelviewMatrix );
	tr.modelviewIdentity = true;
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity( cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == GET_ENTITY( 0 ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	if( e->model->type == mod_brush )
	{
		gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];
		RI->objectMatrix = glm->transform;
	}
	else
	{
		RI->objectMatrix = matrix4x4( e->origin, e->angles, scale );
	}
	RI->modelviewMatrix = RI->worldviewMatrix.ConcatTransforms( RI->objectMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
=============
R_TranslateForEntity
=============
*/
void R_TranslateForEntity( cl_entity_t *e )
{
	float	scale = 1.0f;

	if( e == GET_ENTITY( 0 ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	if( e->model->type == mod_brush )
	{
		gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];
		RI->objectMatrix = glm->transform;
	}
	else
	{
		RI->objectMatrix = matrix4x4( e->origin, g_vecZero, scale );
	}
	RI->modelviewMatrix = RI->worldviewMatrix.ConcatTransforms( RI->objectMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
=============
R_TransformForEntity
=============
*/
void R_TransformForEntity( const matrix4x4 &transform )
{
	RI->objectMatrix = transform;
	RI->modelviewMatrix = RI->worldviewMatrix.ConcatTransforms( RI->objectMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
===============
R_FindViewLeaf
===============
*/
void R_FindViewLeaf( void )
{
	RI->oldviewleaf = RI->viewleaf;
	RI->viewleaf = Mod_PointInLeaf( RI->pvsorigin, worldmodel->nodes );
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame( void )
{
	// setup viewplane dist
	RI->viewplanedist = DotProduct( RI->vieworg, RI->vforward );

	if( !CVAR_TO_BOOL( r_nosort ))
	{
		// sort translucents entities by rendermode and distance
		qsort( tr.trans_entities, tr.num_trans_entities, sizeof( cl_entity_t* ), (cmpfunc)R_TransEntityCompare );
	}

	// current viewleaf
	if( !FBitSet( RI->params, RP_OLDVIEWLEAF ))
		R_FindViewLeaf();
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL( void )
{
	R_SetupModelviewMatrix( RI->worldviewMatrix );
	R_SetupProjectionMatrix( RI->fov_x, RI->fov_y, RI->projectionMatrix );

	RI->worldviewProjectionMatrix = RI->projectionMatrix.Concat( RI->worldviewMatrix );

	if( RP_NORMALPASS( ))
	{
		// setup main viewport
		int x = floor( RI->viewport[0] * glState.width / glState.width );
		int x2 = ceil(( RI->viewport[0] + RI->viewport[2] ) * glState.width / glState.width );
		int y = floor( glState.height - RI->viewport[1] * glState.height / glState.height );
		int y2 = ceil( glState.height - ( RI->viewport[1] + RI->viewport[3] ) * glState.height / glState.height );
		pglViewport( x, y2, x2 - x, y - y2 );
	}
	else
	{
		// setup sub viewport
		pglViewport( RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3] );
	}

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->worldviewMatrix );

	if( FBitSet( RI->params, RP_CLIPPLANE ))
	{
		GLdouble	clip[4];
		mplane_t	*p = &RI->clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}

	GL_Cull( GL_FRONT );

	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
}

/*
=============
R_EndGL
=============
*/
static void R_EndGL( void )
{
	if( FBitSet( RI->params, RP_CLIPPLANE ))
		pglDisable( GL_CLIP_PLANE0 );
}

/*
=============
R_RecursiveFindWaterTexture

using to find source waterleaf with
watertexture to grab fog values from it
=============
*/
static int R_RecursiveFindWaterTexture( const mnode_t *node, const mnode_t *ignore, qboolean down )
{
	int tex = -1;

	// assure the initial node is not null
	// we could check it here, but we would rather check it 
	// outside the call to get rid of one additional recursion level
	assert( node != NULL );

	// ignore solid nodes
	if( node->contents == CONTENTS_SOLID )
		return -1;

	if( node->contents < 0 )
	{
		mleaf_t		*pleaf;
		msurface_t	**mark;
		int		i, c;

		// ignore non-liquid leaves
		if( !IsLiquidContents( node->contents ))
			 return -1;

		// find texture
		pleaf = (mleaf_t *)node;
		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;	

		for( i = 0; i < c; i++, mark++ )
		{
			if( (*mark)->flags & SURF_DRAWTURB && (*mark)->texinfo && (*mark)->texinfo->texture )
				return (*mark)->texinfo->texture->gl_texturenum;
		}

		// texture not found
		return -1;
	}

	// this is a regular node
	// traverse children
	if( node->children[0] && ( node->children[0] != ignore ))
	{
		tex = R_RecursiveFindWaterTexture( node->children[0], node, true );
		if( tex != -1 ) return tex;
	}

	if( node->children[1] && ( node->children[1] != ignore ))
	{
		tex = R_RecursiveFindWaterTexture( node->children[1], node, true );
		if( tex != -1 ) return tex;
	}

	// for down recursion, return immediately
	if( down ) return -1;

	// texture not found, step up if any
	if( node->parent )
		return R_RecursiveFindWaterTexture( node->parent, node, false );

	// top-level node, bail out
	return -1;
}

/*
=============
R_CheckFog

check for underwater fog
Using backward recursion to find waterline leaf
from underwater leaf (idea: XaeroX)
=============
*/
static void R_CheckFog( void )
{
	cl_entity_t	*ent = NULL;
	int		texture = -1;
	int		waterEntity;
	color24		fogColor;
	byte		fogDensity;

	tr.fogEnabled = false;

	// eyes above water
	if( tr.viewparams.waterlevel < 3 )
	{
		if( tr.movevars->fog_settings != 0 )
		{
			// enable global exponential color
			tr.fogColor[0] = ((tr.movevars->fog_settings & 0xFF000000) >> 24) / 255.0f;
			tr.fogColor[1] = ((tr.movevars->fog_settings & 0xFF0000) >> 16) / 255.0f;
			tr.fogColor[2] = ((tr.movevars->fog_settings & 0xFF00) >> 8) / 255.0f;
			if( FBitSet( RI->params, RP_SKYPORTALVIEW ))
				tr.fogDensity = (tr.movevars->fog_settings & 0xFF) * 0.00005f;
			else tr.fogDensity = (tr.movevars->fog_settings & 0xFF) * 0.000005f;
			tr.fogEnabled = true;
		}
		return;
	}

	waterEntity = WATER_ENTITY( RI->vieworg );
	if( waterEntity >= 0 && waterEntity < tr.viewparams.max_entities )
		ent = GET_ENTITY( waterEntity );

	// check for water texture
	if( ent && ent->model && ent->model->type == mod_brush )
	{
		for( int i = 0; i < ent->model->nummodelsurfaces; i++ )
		{
			msurface_t *surf = &ent->model->surfaces[ent->model->firstmodelsurface+i];
			if( FBitSet( surf->flags, SURF_DRAWTURB ))
			{
				texture = surf->texinfo->texture->gl_texturenum;
				break;
			}
		}
	}
	else if( RI->viewleaf )
	{
		texture = R_RecursiveFindWaterTexture( RI->viewleaf->parent, NULL, false );
	}
	if( texture == -1 ) return; // no valid fogs

	// extract fog settings from texture palette
	GET_EXTRA_PARAMS( texture, &fogColor.r, &fogColor.g, &fogColor.b, &fogDensity );

	// copy fog params
	tr.fogColor[0] = fogColor.r / 255.0f;
	tr.fogColor[1] = fogColor.g / 255.0f;
	tr.fogColor[2] = fogColor.b / 255.0f;
	tr.fogDensity = fogDensity * 0.000005f;
	tr.fogEnabled = true;
}

/*
=============
R_DrawFog

=============
*/
void R_DrawFog( void )
{
	if( !tr.fogEnabled ) return;

	pglEnable( GL_FOG );
	pglFogi( GL_FOG_MODE, GL_EXP );
	pglFogf( GL_FOG_DENSITY, tr.fogDensity );
	pglFogfv( GL_FOG_COLOR, tr.fogColor );
	pglHint( GL_FOG_HINT, GL_NICEST );
}

/*
=============
R_DrawParticles

NOTE: particles are drawing with engine methods
=============
*/
void R_DrawParticles( qboolean trans )
{
	ref_viewpass_t	rvp;

	// BUGBUG: drawing particles through sky in too many cases
	// because particles doesn't have individual check for pvs
	if( FBitSet( RI->params, RP_SKYPORTALVIEW ))
		return;

	rvp.viewport[0] = RI->viewport[0];
	rvp.viewport[1] = RI->viewport[1];
	rvp.viewport[2] = RI->viewport[2];
	rvp.viewport[3] = RI->viewport[3];

	rvp.viewangles = RI->viewangles;
	rvp.vieworigin = RI->vieworg;
	rvp.fov_x = RI->fov_x;
	rvp.fov_y = RI->fov_y;
	rvp.flags = 0;

	SetBits( rvp.flags, RF_DRAW_WORLD );

	if( RI->params & RP_ENVVIEW )
		SetBits( rvp.flags, RF_DRAW_CUBEMAP );
	if( FBitSet( RI->params, RP_OVERVIEW ))
		SetBits( rvp.flags, RF_DRAW_OVERVIEW );
	R_RestoreGLState();

	DRAW_PARTICLES( &rvp, trans, tr.frametime );
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList( void )
{
	int	i;

	glState.drawTrans = false;
	GL_CheckForErrors();
	tr.blend = 1.0f;

	// first draw solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI->currententity = tr.solid_entities[i];
		RI->currentmodel = RI->currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		switch( RI->currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI->currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI->currententity );
			break;
		default:
			break;
		}
	}

	GL_CheckForErrors();

	// draw sprites seperately, because of alpha blending
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI->currententity = tr.solid_entities[i];
		RI->currentmodel = RI->currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		switch( RI->currentmodel->type )
		{
		case mod_sprite:
			R_DrawSpriteModel( RI->currententity );
			break;
		default:
			break;
		}
	}

	GL_CheckForErrors();

	R_DrawParticles( false );

	GL_CheckForErrors();

	HUD_DrawNormalTriangles();

	GL_CheckForErrors();

	glState.drawTrans = true;

	// then draw translucent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		RI->currententity = tr.trans_entities[i];
		RI->currentmodel = RI->currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		// handle studiomodels with custom rendermodes on texture
		if( RI->currententity->curstate.rendermode != kRenderNormal )
			tr.blend = CL_FxBlend( RI->currententity ) / 255.0f;
		else tr.blend = 1.0f; // draw as solid but sorted by distance

		if( tr.blend <= 0.0f ) continue;

		switch( RI->currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI->currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI->currententity );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RI->currententity );
			break;
		default:
			break;
		}
	}

	GL_CheckForErrors();

	R_RestoreGLState();
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	HUD_DrawTransparentTriangles();

	GL_CheckForErrors();

	R_AllowFog( false );
	R_DrawParticles( true );
	R_AllowFog( true );

	GL_CheckForErrors();

	glState.drawTrans = false;
	pglDisable( GL_BLEND );	// Trinity Render issues

	R_DrawViewModel();

	GL_CheckForErrors();
}

/*
================
R_RenderScene

RI->refdef must be set before the first call
================
*/
void R_RenderScene( void )
{
	char	empty[MAX_REF_STACK];

	// set the worldmodel
	if(( worldmodel = GET_ENTITY( 0 )->model ) == NULL )
		HOST_ERROR( "R_RenderView: NULL worldmodel\n" );

	if( (int)r_speeds->value == 7 )
	{
		int	num_faces = 0;
		unsigned int i;

		if( glState.stack_position > 0 )
			num_faces = R_GetPrevInstance()->num_subview_faces;
		for( i = 0; i < glState.stack_position; i++ )
			empty[i] = ' ';
		empty[i] = '\0';

		// build pass hierarchy
		const char *string = va( "%s->%d %s (%d subview)\n", empty, glState.stack_position, R_GetNameForView( ), num_faces );
		Q_strncat( r_depth_msg, string, sizeof( r_depth_msg ));
	}

	// recursive draw mirrors, portals, etc
	R_RenderSubview ();

	R_RenderShadowmaps();

	R_CheckSkyPortal( tr.sky_camera );

	// frametime is valid only for normal pass
	if( RP_NORMALPASS( ))
		tr.frametime = tr.time - tr.oldtime;
	else tr.frametime = 0.0;

	r_stats.num_passes++;

	// begin a new frame
	R_SetupFrustum();
	R_SetupFrame();

	if( !FBitSet( RI->params, RP_MIRRORVIEW|RP_ENVVIEW|RP_SHADOWVIEW ))
		R_BindPostFramebuffers();

	R_SetupGL();
	R_Clear( ~0 );

	R_PushDlights();
	R_CheckFog();

	R_DrawWorld();

	R_DrawEntitiesOnList();

	R_EndGL();

	if( !FBitSet( RI->params, RP_MIRRORVIEW|RP_ENVVIEW|RP_SHADOWVIEW ))
		RenderSunShafts();
}

/*
==============
R_Speeds_Printf

helper to print into r_speeds message
==============
*/
void R_Speeds_Printf( const char *msg, ... )
{
	va_list	argptr;
	char	text[2048];

	va_start( argptr, msg );
	Q_vsprintf( text, msg, argptr );
	va_end( argptr );

	Q_strncat( r_speeds_msg, text, sizeof( r_speeds_msg ));
}

void HUD_PrintStats( void )
{
	if( !CVAR_TO_BOOL( r_speeds ))
		return;

	msurface_t *surf = r_stats.debug_surface;
	mleaf_t *curleaf = RI->viewleaf;

	R_Speeds_Printf( "Renderer: ^2XashXT^7\n\n" );

	switch( (int)r_speeds->value )
	{
	case 1:
		R_Speeds_Printf( "%3i wpoly %3i epoly\n", r_stats.c_world_polys, r_stats.c_studio_polys );
		R_Speeds_Printf( "%3i spoly %3i grass\n", r_stats.c_sprite_polys, r_stats.c_grass_polys );
		break;		
	case 2:
		if( !curleaf ) curleaf = worldmodel->leafs;
		R_Speeds_Printf( "visible leafs:\n%3i leafs\ncurrent leaf %3i\n", r_stats.c_world_leafs, curleaf - worldmodel->leafs );
		R_Speeds_Printf( "RecursiveWorldNode: %3lf secs\nDrawTextureChains %lf\n", r_stats.t_world_node, r_stats.t_world_draw );
		break;
	case 3:
		R_Speeds_Printf( "%3i static entities\n%3i normal entities\n", r_numStatics, r_numEntities - r_numStatics );
		break;
	case 4:
		R_Speeds_Printf( "%3i studio models drawn\n", r_stats.c_studio_models_drawn );
		R_Speeds_Printf( "%3i sprite models drawn\n", r_stats.c_sprite_models_drawn );
		R_Speeds_Printf( "%3i temp entities active\n", r_stats.c_active_tents_count );
		break;
	case 5:
		R_Speeds_Printf( "%3i mirrors\n", r_stats.c_mirror_passes );
		R_Speeds_Printf( "%3i portals\n", r_stats.c_portal_passes );
		R_Speeds_Printf( "%3i screens\n", r_stats.c_screen_passes );
		R_Speeds_Printf( "%3i shadows\n", r_stats.c_shadow_passes );
		R_Speeds_Printf( "%3i 3d sky\n", r_stats.c_sky_passes );
		R_Speeds_Printf( "%3i total\n", r_stats.num_passes );
		break;
	case 6:
		R_Speeds_Printf( "DIP count %3i\nShader bind %3i\n", r_stats.num_flushes, r_stats.num_shader_binds );
		R_Speeds_Printf( "Total GLSL shaders %3i\n", Q_max( num_glsl_programs - 1, 0 ));
		R_Speeds_Printf( "frame total tris %3i\n", r_stats.c_total_tris );
		break;
	case 7:
		// draw hierarchy map of recursion calls
		Q_strncpy( r_speeds_msg, r_depth_msg, sizeof( r_speeds_msg ));
		break;
	case 8:
		if( surf && surf->texinfo && surf->texinfo->texture )
		{
			glsl_program_t *cur = &glsl_programs[surf->info->shaderNum[0]];
			R_Speeds_Printf( "Surf: ^1%s^7\n", surf->texinfo->texture->name );
			R_Speeds_Printf( "Shader: ^3#%i %s^7\n", surf->info->shaderNum[0], cur->name );
			R_Speeds_Printf( "List Options:\n" ); 
			R_Speeds_Printf( "%s\n", GL_PretifyListOptions( cur->options, true ));
		}
		break;
	case 9:
		R_Speeds_Printf( "%s grass total size\n", Q_memprint( tr.grass_total_size ));
		break;
	}

	memset( &r_stats, 0, sizeof( r_stats ));
}

void R_RenderDebugSurface( void )
{
	r_stats.debug_surface = NULL;

	if( r_speeds->value != 8 )
		return;

	Vector vecSrc = RI->vieworg;
	Vector vecEnd = vecSrc + RI->vforward * RI->farClip;
	pmtrace_t trace;

	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( (float *)&vecSrc, (float *)&vecEnd, 0, -1, &trace );
	r_stats.debug_surface = gEngfuncs.pEventAPI->EV_TraceSurface( trace.ent, (float *)&vecSrc, (float *)&vecEnd );
	if( !r_stats.debug_surface ) return;

	physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( trace.ent );
	cl_entity_t *ent = (pe) ? GET_ENTITY( pe->info ) : NULL;

	if( !ent || !ent->model || ent->model->type != mod_brush )
		return;

	if( ent->curstate.angles != g_vecZero )
		R_RotateForEntity( ent );
	else R_TranslateForEntity( ent );

	pglEnable( GL_BLEND );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	pglColor4f( 0.5f, 1.0f, 0.36f, 0.99f ); 
	pglDisable( GL_TEXTURE_2D );
	pglLineWidth( 2.0f );

	pglDisable( GL_DEPTH_TEST );
	pglEnable( GL_LINE_SMOOTH );
	pglEnable( GL_POLYGON_SMOOTH );
	pglHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
	pglHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );

	mextrasurf_t *es = r_stats.debug_surface->info;

	pglBegin( GL_POLYGON );
	for( int j = 0; j < es->numverts; j++ )
	{
		bvert_t *v = &world->vertexes[es->firstvertex + j];
		pglVertex3fv( v->vertex + v->normal * 0.1f );
	}
	pglEnd();

	pglEnable( GL_TEXTURE_2D );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglDisable( GL_POLYGON_SMOOTH );
	pglDisable( GL_LINE_SMOOTH );
	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_BLEND );
	pglLineWidth( 1.0f );
	R_LoadIdentity ();
}

bool R_CheckMonsterView( const ref_viewpass_t *rvp )
{
	if( rvp->viewentity <= tr.viewparams.maxclients )
		return false;

	// get viewentity and monster eyeposition
	cl_entity_t *view = GET_ENTITY( rvp->viewentity );

 	if( view && view->model && view->model->type == mod_studio && FBitSet( view->curstate.eflags, EFLAG_SLERP ))
		return true;
	return false;
}

/*
===============
R_SetupRefParams

set initial params for renderer
===============
*/
void R_SetupRefParams( const ref_viewpass_t *rvp )
{
	RI->params = RP_NONE;
	RI->farClip = 0;

	tr.time = GET_CLIENT_TIME();
	tr.oldtime = GET_CLIENT_OLDTIME();
	tr.fGamePaused = RENDER_GET_PARM( PARAM_GAMEPAUSED, 0 );

	if( !FBitSet( rvp->flags, RF_DRAW_CUBEMAP ))
	{
		if( FBitSet( rvp->flags, RF_DRAW_OVERVIEW ))
		{
			SetBits( RI->params, RP_THIRDPERSON );
			SetBits( RI->params, RP_OVERVIEW );
		}

		if( gHUD.m_iCameraMode )
			SetBits( RI->params, RP_THIRDPERSON );
	}
	else
	{
		SetBits( RI->params, RP_ENVVIEW );
	}

	// setup viewport
	RI->viewport[0] = rvp->viewport[0];
	RI->viewport[1] = rvp->viewport[1];
	RI->viewport[2] = rvp->viewport[2];
	RI->viewport[3] = rvp->viewport[3];

	// calc FOV
	if( R_CheckMonsterView( rvp ))
	{
		RI->fov_x = 100; // adjust fov for monster view
		RI->fov_y = V_CalcFov( RI->fov_x, RI->viewport[2], RI->viewport[3] );
	}
	else
	{
		RI->fov_x = rvp->fov_x;
		RI->fov_y = rvp->fov_y;
	}

	RI->vieworg = rvp->vieworigin;
	RI->viewangles = rvp->viewangles;
	RI->pvsorigin = rvp->vieworigin;
	RI->viewentity = rvp->viewentity;
	RI->num_subview_faces = 0;

	tr.cached_vieworigin = rvp->vieworigin;
	tr.cached_viewangles = rvp->viewangles;

	// setup skyparams
	tr.sky_normal.x = tr.movevars->skyvec_x;
	tr.sky_normal.y = tr.movevars->skyvec_y;
	tr.sky_normal.z = tr.movevars->skyvec_z;
	
	tr.sky_ambient.x = tr.movevars->skycolor_r;
	tr.sky_ambient.y = tr.movevars->skycolor_g;
	tr.sky_ambient.z = tr.movevars->skycolor_b;
}

/*
===============
HUD_RenderFrame

A callback that replaces RenderFrame
engine function. Return 1 if you
override ALL the rendering and return 0
if we don't want draw from
the client (e.g. playersetup preview)
===============
*/
int HUD_RenderFrame( const ref_viewpass_t *rvp )
{
	r_speeds_msg[0] = r_depth_msg[0] = '\0';
	tr.fCustomRendering = false;

	// it's playersetup overview, ignore it	
	if( !FBitSet( rvp->flags, RF_DRAW_WORLD ))
		return 0;

	// draw client things through engine renderer
	if( FBitSet( rvp->flags, RF_ONLY_CLIENTDRAW ))
		return 0;

	R_SetupRefParams( rvp );

	if( !g_fRenderInitialized )
		return 0;

	// use engine renderer
	if( !CVAR_TO_BOOL( gl_renderer ))
	{
		tr.fResetVis = true;
		return 0;
	}

	memset( tr.visbytes, 0, tr.pvssize );
	tr.fCustomRendering = true;
	R_RunViewmodelEvents();
	tr.realframecount++;

	// draw main view
	R_RenderScene ();

	R_RestoreGLState();

	R_RenderDebugSurface ();

	if( CVAR_TO_BOOL( r_show_renderpass ))
		R_DrawRenderPasses( (int)r_show_renderpass->value - 1 );

	if( CVAR_TO_BOOL( r_show_light_scissors ))
		R_DrawLightScissors();

	if( CVAR_TO_BOOL( r_show_lightprobes ))
		DrawLightProbes();

	if( CVAR_TO_BOOL( r_show_normals ))
		DrawNormals();

	R_UnloadFarGrass();

	HUD_PrintStats ();

	return 1;
}

BOOL HUD_SpeedsMessage( char *out, size_t size )
{
	if( !g_fRenderInitialized || !CVAR_TO_BOOL( gl_renderer ))
		return false; // let the engine use built-in counters

	if( r_speeds->value <= 0 || !out || !size )
		return false;

	Q_strncpy( out, r_speeds_msg, size );

	return true;
}

void HUD_ProcessEntData( qboolean allocate )
{
	if( allocate ) Mod_PrepareModelInstances();
	else Mod_ThrowModelInstances();
}

void HUD_ProcessModelData( model_t *mod, qboolean create, const byte *buffer )
{
	if( !g_fRenderInitialized ) return;

	// g-cont. probably this is redundant :-)
	if( RENDER_GET_PARM( PARM_DEDICATED_SERVER, 0 ))
		return;

	if( FBitSet( mod->flags, MODEL_WORLD ))
		R_ProcessWorldData( mod, create, buffer );
	else R_ProcessStudioData( mod, create, buffer );
}

//
// Xash3D render interface
//
static render_interface_t gRenderInterface = 
{
	CL_RENDER_INTERFACE_VERSION,
	HUD_RenderFrame,
	HUD_BuildLightmaps,
	Mod_SetOrthoBounds,
	R_CreateStudioDecalList,
	R_ClearStudioDecals,
	HUD_SpeedsMessage,
	HUD_ProcessModelData,
	HUD_ProcessEntData,
	Mod_GetEngineVis,
	R_NewMap,
	R_ClearScene,
};

int HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback )
{
	if ( !callback || !renderfuncs || version != CL_RENDER_INTERFACE_VERSION )
	{
		return FALSE;
	}

	size_t iImportSize = sizeof( render_interface_t );
	size_t iExportSize = sizeof( render_api_t );

	// copy new physics interface
	memcpy( &gRenderfuncs, renderfuncs, iExportSize );

	// fill engine callbacks
	memcpy( callback, &gRenderInterface, iImportSize );

	// get pointer to movevars
	tr.movevars = gEngfuncs.pEventAPI->EV_GetMovevars();

	g_fRenderInitialized = TRUE;

	return TRUE;
}
