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
#include "r_weather.h"
#include "features.h"

#define IsLiquidContents( cnt )	( cnt == CONTENTS_WATER || cnt == CONTENTS_SLIME || cnt == CONTENTS_LAVA )

ref_programs_t	cg;
ref_globals_t	tr;
ref_instance_t	RI;
ref_stats_t	r_stats;
ref_params_t	r_lastRefdef;
char		r_speeds_msg[2048];
float		gldepthmin, gldepthmax;
model_t		*worldmodel = NULL;	// must be set at begin each frame
BOOL		g_fRenderInitialized = FALSE;
mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
int		r_currentMessageNum = 0;

static int R_RankForRenderMode( cl_entity_t *ent )
{
	switch( ent->curstate.rendermode )
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

/*
===============
R_StaticEntity

Static entity is the brush which has no custom origin and not rotated
typically is a func_wall, func_breakable, func_ladder etc
===============
*/
static qboolean R_StaticEntity( cl_entity_t *ent )
{
	if( !r_allow_static->value )
		return false;

	if( ent->curstate.rendermode != kRenderNormal )
		return false;

	if( ent->model->type != mod_brush )
		return false;

	if( ent->curstate.effects & ( EF_SCREEN|EF_SCREENMOVIE ))
		return false;

	if( ent->curstate.effects & ( EF_NOREFLECT|EF_REFLECTONLY ))
		return false;

	if( ent->curstate.frame || ent->model->flags & MODEL_CONVEYOR )
		return false;

	if( ent->curstate.scale ) // waveheight specified
		return false;

	if( ent->origin != g_vecZero || ent->angles != g_vecZero )
		return false;

	return true;
}

/*
===============
R_FollowEntity

Follow entity is attached to another studiomodel and used last cached bones
from parent
===============
*/
static qboolean R_FollowEntity( cl_entity_t *ent )
{
	if( ent->model->type != mod_studio )
		return false;

	if( ent->curstate.movetype != MOVETYPE_FOLLOW )
		return false;

	if( ent->curstate.aiment <= 0 )
		return false;

	return true;
}

/*
===============
R_OpaqueEntity

Opaque entity can be brush or studio model but sprite
===============
*/
static qboolean R_OpaqueEntity( cl_entity_t *ent )
{
	if( ent->curstate.rendermode == kRenderNormal )
		return true;

	if( ent->model->type == mod_sprite )
		return false;

	if( ent->curstate.rendermode == kRenderTransAlpha )
		return true;

	return false;
}

/*
===============
R_SolidEntityCompare

Sorting opaque entities by model type
===============
*/
static int R_SolidEntityCompare( const cl_entity_t **a, const cl_entity_t **b )
{
	cl_entity_t	*ent1, *ent2;

	ent1 = (cl_entity_t *)*a;
	ent2 = (cl_entity_t *)*b;

	if( ent1->model->type > ent2->model->type )
		return 1;
	if( ent1->model->type < ent2->model->type )
		return -1;

	return 0;
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

	// now sort by rendermode
	if( R_RankForRenderMode( ent1 ) > R_RankForRenderMode( ent2 ))
		return 1;
	if( R_RankForRenderMode( ent1 ) < R_RankForRenderMode( ent2 ))
		return -1;

	Vector	org;
	float	len1, len2;

	// then by distance
	if( ent1->model->type == mod_brush )
	{
		org = ent1->origin + ((ent1->model->mins + ent1->model->maxs) * 0.5f);
		len1 = (RI.vieworg - org).Length();
	}
	else
	{
		len1 = (RI.vieworg - ent1->origin).Length();
	}

	if( ent2->model->type == mod_brush )
	{
		org = ent2->origin + ((ent2->model->mins + ent2->model->maxs) * 0.5f);
		len2 = (RI.vieworg - org).Length();
	}
	else
	{
		len2 = (RI.vieworg - ent2->origin).Length();
	}

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}

qboolean R_WorldToScreen( const Vector &point, Vector &screen )
{
	matrix4x4	worldToScreen;
	qboolean	behind;
	float	w;

	worldToScreen = RI.worldviewProjectionMatrix;

	screen[0] = worldToScreen[0][0] * point[0] + worldToScreen[1][0] * point[1] + worldToScreen[2][0] * point[2] + worldToScreen[3][0];
	screen[1] = worldToScreen[0][1] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[2][1] * point[2] + worldToScreen[3][1];
//	z = worldToScreen[0][2] * point[0] + worldToScreen[1][2] * point[1] + worldToScreen[2][2] * point[2] + worldToScreen[3][2];
	w = worldToScreen[0][3] * point[0] + worldToScreen[1][3] * point[1] + worldToScreen[2][3] * point[2] + worldToScreen[3][3];
	screen[2] = 0.0f; // just so we have something valid here

	if( w < 0.001f )
	{
		behind = true;
		screen[0] *= 100000;
		screen[1] *= 100000;
	}
	else
	{
		float invw = 1.0f / w;
		behind = false;
		screen[0] *= invw;
		screen[1] *= invw;
	}
	return behind;
}

void R_ScreenToWorld( const Vector &screen, Vector &point )
{
	matrix4x4	screenToWorld;
	Vector	temp;
	float	w;

	screenToWorld = RI.worldviewProjectionMatrix.InvertFull();

	temp[0] = 2.0f * (screen[0] - RI.viewport[0]) / RI.viewport[2] - 1;
	temp[1] = -2.0f * (screen[1] - RI.viewport[1]) / RI.viewport[3] + 1;
	temp[2] = 0.0f; // just so we have something valid here

	point[0] = temp[0] * screenToWorld[0][0] + temp[1] * screenToWorld[0][1] + temp[2] * screenToWorld[0][2] + screenToWorld[0][3];
	point[1] = temp[0] * screenToWorld[1][0] + temp[1] * screenToWorld[1][1] + temp[2] * screenToWorld[1][2] + screenToWorld[1][3];
	point[2] = temp[0] * screenToWorld[2][0] + temp[1] * screenToWorld[2][1] + temp[2] * screenToWorld[2][2] + screenToWorld[2][3];
	w = temp[0] * screenToWorld[3][0] + temp[1] * screenToWorld[3][1] + temp[2] * screenToWorld[3][2] + screenToWorld[3][3];
	if( w ) point *= ( 1.0f / w );
}

/*
===============
R_ComputeFxBlend
===============
*/
int R_ComputeFxBlend( cl_entity_t *e )
{
	int	blend = 0, renderAmt;
	float	offset, dist;
	Vector	tmp;

	offset = ((int)e->index ) * 363.0f; // Use ent index to de-sync these fx
	renderAmt = e->curstate.renderamt;

	switch( e->curstate.renderfx ) 
	{
	case kRenderFxPulseSlowWide:
		blend = renderAmt + 0x40 * sin( RI.refdef.time * 2 + offset );	
		break;
	case kRenderFxPulseFastWide:
		blend = renderAmt + 0x40 * sin( RI.refdef.time * 8 + offset );
		break;
	case kRenderFxPulseSlow:
		blend = renderAmt + 0x10 * sin( RI.refdef.time * 2 + offset );
		break;
	case kRenderFxPulseFast:
		blend = renderAmt + 0x10 * sin( RI.refdef.time * 8 + offset );
		break;
	// JAY: HACK for now -- not time based
	case kRenderFxFadeSlow:			
		if( renderAmt > 0 ) 
			renderAmt -= 1;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxFadeFast:
		if( renderAmt > 3 ) 
			renderAmt -= 4;
		else renderAmt = 0;
		blend = renderAmt;
		break;
	case kRenderFxSolidSlow:
		if( renderAmt < 255 ) 
			renderAmt += 1;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxSolidFast:
		if( renderAmt < 252 ) 
			renderAmt += 4;
		else renderAmt = 255;
		blend = renderAmt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin( RI.refdef.time * 4 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin( RI.refdef.time * 16 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin( RI.refdef.time * 36 + offset );
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin( RI.refdef.time * 2 ) + sin( RI.refdef.time * 17 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin( RI.refdef.time * 16 ) + sin( RI.refdef.time * 23 + offset ));
		if( blend < 0 ) blend = 0;
		else blend = renderAmt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		tmp = e->origin - RI.refdef.vieworg;
		dist = DotProduct( tmp, RI.refdef.forward );
			
		// Turn off distance fade
		if( e->curstate.renderfx == kRenderFxDistort )
			dist = 1;

		if( dist <= 0 )
		{
			blend = 0;
		}
		else 
		{
			renderAmt = 180;
			if( dist <= 100 ) blend = renderAmt;
			else blend = (int) ((1.0f - ( dist - 100 ) * ( 1.0f / 400.0f )) * renderAmt );
			blend += RANDOM_LONG( -32, 31 );
		}
		break;
	case kRenderFxGlowShell:	// safe current renderamt because it's shell scale!
	case kRenderFxDeadPlayer:	// safe current renderamt because it's player index!
		blend = renderAmt;
		break;
	case kRenderFxNone:
	case kRenderFxClampMinScale:
	default:
		if( e->curstate.rendermode == kRenderNormal )
			blend = 255;
		else blend = renderAmt;
		break;	
	}

	if( e->model->type != mod_brush )
	{
		// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
		if( !e->curstate.rendercolor.r && !e->curstate.rendercolor.g && !e->curstate.rendercolor.b )
			e->curstate.rendercolor.r = e->curstate.rendercolor.g = e->curstate.rendercolor.b = 255;
	}

	// apply scale to studiomodels and sprites only
	if( e->model && e->model->type != mod_brush && !e->curstate.scale )
		e->curstate.scale = 1.0f;

	blend = bound( 0, blend, 255 );

	return blend;
}

/*
===============
R_ClearScene
===============
*/
void R_ClearScene( void )
{
	if( !g_fRenderInitialized )
		return;

	if( cl_viewsize != NULL && cl_viewsize->value != 120.0f )
		CVAR_SET_FLOAT( "viewsize", 120.0f );

	memset( &r_stats, 0, sizeof( r_stats ));

	CL_DecayLights();

	tr.num_solid_entities = tr.num_trans_entities = 0;
	tr.num_static_entities = tr.num_mirror_entities = 0;
	tr.num_child_entities = tr.num_beams_entities =0;
	tr.num_mirrors_used = tr.num_portals_used = 0;
	tr.num_portal_entities = tr.num_screens_used = 0;
	tr.num_shadows_used = tr.local_client_added = 0;
	tr.sky_camera = NULL;
}

/*
===============
R_AddEntity
===============
*/
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType )
{
	if( !r_drawentities->value )
		return false; // not allow to drawing

	if( !clent || !clent->model )
		return false; // if set to invisible, skip

	if( clent->curstate.effects & EF_NODRAW )
		return false; // done

	if( clent->curstate.effects & ( EF_PROJECTED_LIGHT|EF_DYNAMIC_LIGHT ))
		return true; // no reason to drawing this entity

	if( entityType == ET_PLAYER && RP_LOCALCLIENT( clent ))
	{
		if( tr.local_client_added )
			return false; // already present in list
		tr.local_client_added = true;
	}

	clent->curstate.renderamt = R_ComputeFxBlend( clent );

	if( clent->curstate.rendermode != kRenderNormal )
	{
		if( clent->curstate.renderamt <= 0.0f )
			return true; // invisible
	}

	clent->curstate.entityType = entityType;

	if( entityType == ET_FRAGMENTED )
		r_stats.c_client_ents++;

	if( R_FollowEntity( clent ))
	{
		// follow entity
		if( tr.num_child_entities >= MAX_VISIBLE_PACKET )
			return false;

		tr.child_entities[tr.num_child_entities] = clent;
		tr.num_child_entities++;

		return true;
	}

	if( R_OpaqueEntity( clent ))
	{
		if( R_StaticEntity( clent ))
		{
			// opaque static
			if( tr.num_static_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.static_entities[tr.num_static_entities] = clent;
			tr.num_static_entities++;
		}
		else
		{
			// opaque moving
			if( tr.num_solid_entities >= MAX_VISIBLE_PACKET )
				return false;

			tr.solid_entities[tr.num_solid_entities] = clent;
			tr.num_solid_entities++;
		}
	}
	else
	{
		// translucent
		if( tr.num_trans_entities >= MAX_VISIBLE_PACKET )
			return false;

		tr.trans_entities[tr.num_trans_entities] = clent;
		tr.num_trans_entities++;
	}

	if( entityType == ET_FRAGMENTED )
		clent->visframe = tr.realframecount;

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

	if( r_overview && r_overview->value )
		pglClearColor( 0.0f, 1.0f, 0.0f, 1.0f ); // green background (Valve rules)
	else pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );

	bits = GL_DEPTH_BUFFER_BIT;

	if( r_fastsky->value )
		bits |= GL_COLOR_BUFFER_BIT;

	if( glState.stencilEnabled )
		bits |= GL_STENCIL_BUFFER_BIT;

	bits &= bitMask;

	pglClear( bits );

	// change ordering for overview
	if( RI.drawOrtho )
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

/*
===============
R_SetupFrustumOrtho
===============
*/
static void R_SetupFrustumOrtho( void )
{
	const ref_overview_t *ov = GET_OVERVIEW_PARMS();

	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip
	// 5 - nearclip

	// setup the near and far planes.
	float orgOffset = DotProduct( RI.vieworg, RI.vforward );

	RI.frustum[4].normal = -RI.vforward;
	RI.frustum[4].dist = -ov->zFar - orgOffset;

	RI.frustum[5].normal = RI.vforward;
	RI.frustum[5].dist = ov->zNear + orgOffset;

	// left and right planes...
	orgOffset = DotProduct( RI.vieworg, RI.vright );
	RI.frustum[0].normal = RI.vright;
	RI.frustum[0].dist = ov->xLeft + orgOffset;

	RI.frustum[1].normal = -RI.vright;
	RI.frustum[1].dist = -ov->xRight - orgOffset;

	// top and buttom planes...
	orgOffset = DotProduct( RI.vieworg, RI.vup );
	RI.frustum[3].normal = RI.vup;
	RI.frustum[3].dist = ov->xTop + orgOffset;

	RI.frustum[2].normal = -RI.vup;
	RI.frustum[2].dist = -ov->xBottom - orgOffset;

	for( int i = 0; i < 6; i++ )
	{
		RI.frustum[i].type = PLANE_NONAXIAL;
		RI.frustum[i].signbits = SignbitsForPlane( RI.frustum[i].normal );
	}
}

/*
===============
R_SetupFrustum
===============
*/
void R_SetupFrustum( void )
{
	// 0 - left
	// 1 - right
	// 2 - down
	// 3 - up
	// 4 - farclip
	// 5 - nearclip

	if( RI.drawOrtho )
	{
		R_SetupFrustumOrtho();
		return;
	}

	Vector farPoint = RI.vieworg + RI.vforward * (RI.refdef.movevars->zmax * 1.5f);

	// rotate RI.vforward right by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[0].normal, RI.vup, RI.vforward, -(90 - RI.refdef.fov_x / 2));
	// rotate RI.vforward left by FOV_X/2 degrees
	RotatePointAroundVector( RI.frustum[1].normal, RI.vup, RI.vforward, 90 - RI.refdef.fov_x / 2 );
	// rotate RI.vforward up by FOV_Y/2 degrees
	RotatePointAroundVector( RI.frustum[2].normal, RI.vright, RI.vforward, 90 - RI.refdef.fov_y / 2 );
	// rotate RI.vforward down by FOV_Y/2 degrees
	RotatePointAroundVector( RI.frustum[3].normal, RI.vright, RI.vforward, -(90 - RI.refdef.fov_y / 2));

	for( int i = 0; i < 4; i++ )
	{
		RI.frustum[i].type = PLANE_NONAXIAL;
		RI.frustum[i].dist = DotProduct( RI.vieworg, RI.frustum[i].normal );
		RI.frustum[i].signbits = SignbitsForPlane( RI.frustum[i].normal );
	}

	RI.frustum[4].normal = -RI.vforward;
	RI.frustum[4].type = PLANE_NONAXIAL;
	RI.frustum[4].dist = DotProduct( farPoint, RI.frustum[4].normal );
	RI.frustum[4].signbits = SignbitsForPlane( RI.frustum[4].normal );

	// no need to setup backplane for general view. It's only used for portals and mirrors
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m )
{
	GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;

	if( RI.drawOrtho )
	{
		const ref_overview_t *ov = GET_OVERVIEW_PARMS();
		m.CreateOrtho( ov->xLeft, ov->xRight, ov->xTop, ov->xBottom, ov->zNear, ov->zFar );
		return;
	}

	RI.farClip = RI.refdef.movevars->zmax * 1.5f;

	zNear = 4.0f;
	zFar = max( 256.0f, RI.farClip );

	yMax = zNear * tan( fov_y * M_PI / 360.0 );
	yMin = -yMax;

	xMax = zNear * tan( fov_x * M_PI / 360.0 );
	xMin = -xMax;

	m.CreateProjection( xMax, xMin, yMax, yMin, zNear, zFar );
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( const ref_params_t *pparams, matrix4x4 &m )
{
#if 0
	m.Identity();
	m.ConcatRotate( -90, 1, 0, 0 );
	m.ConcatRotate( 90, 0, 0, 1 );
#else
	m.CreateModelview(); // init quake world orientation
#endif
	m.ConcatRotate( -pparams->viewangles[2], 1, 0, 0 );
	m.ConcatRotate( -pparams->viewangles[0], 0, 1, 0 );
	m.ConcatRotate( -pparams->viewangles[1], 0, 0, 1 );
	m.ConcatTranslate( -pparams->vieworg[0], -pparams->vieworg[1], -pparams->vieworg[2] );
}

/*
=============
R_LoadIdentity
=============
*/
void R_LoadIdentity( void )
{
	if( tr.modelviewIdentity ) return;

	RI.objectMatrix.Identity();
	RI.modelviewMatrix = RI.worldviewMatrix;

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.modelviewMatrix );
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

	if( e == GET_ENTITY( 0 ) || R_StaticEntity( e ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	RI.objectMatrix = matrix4x4( e->origin, e->angles, scale );
	RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( RI.objectMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.modelviewMatrix );
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

	if( e == GET_ENTITY( 0 ) || R_StaticEntity( e ))
	{
		R_LoadIdentity();
		return;
	}

	if( e->model->type != mod_brush && e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	RI.objectMatrix = matrix4x4( e->origin, g_vecZero, scale );
	RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( RI.objectMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.modelviewMatrix );
	tr.modelviewIdentity = false;
}

/*
===============
R_FindViewLeaf
===============
*/
void R_FindViewLeaf( void )
{
	float	height;
	mleaf_t	*leaf;
	Vector	tmp;

	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;
	leaf = Mod_PointInLeaf( RI.pvsorigin, worldmodel->nodes );
	r_viewleaf2 = r_viewleaf = leaf;
	height = RI.waveHeight ? RI.waveHeight : 16;

	// check above and below so crossing solid water doesn't draw wrong
	if( leaf->contents == CONTENTS_EMPTY )
	{
		// look down a bit
		tmp = RI.pvsorigin;
		tmp[2] -= height;
		leaf = Mod_PointInLeaf( tmp, worldmodel->nodes );

		if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
			r_viewleaf2 = leaf;
	}
	else
	{
		// look up a bit
		tmp = RI.pvsorigin;
		tmp[2] += height;
		leaf = Mod_PointInLeaf( tmp, worldmodel->nodes );

		if(( leaf->contents != CONTENTS_SOLID ) && ( leaf != r_viewleaf2 ))
			r_viewleaf2 = leaf;
	}
}

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame( void )
{
	// build the transformation matrix for the given view angles
	RI.vieworg = RI.refdef.vieworg;
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );

	// setup viewplane dist
	RI.viewplanedist = DotProduct( RI.refdef.vieworg, RI.vforward );

	R_AnimateLight();
	R_RunViewmodelEvents();

	// sort opaque entities by model type to avoid drawing model shadows under alpha-surfaces
	qsort( tr.solid_entities, tr.num_solid_entities, sizeof( cl_entity_t* ), (cmpfunc)R_SolidEntityCompare );

	// sort translucents entities by rendermode and distance
	qsort( tr.trans_entities, tr.num_trans_entities, sizeof( cl_entity_t* ), (cmpfunc)R_TransEntityCompare );

	// current viewleaf
	RI.waveHeight = RI.refdef.movevars->waveHeight * 2.0f;	// set global waveheight
	RI.isSkyVisible = false; // unknown at this moment

	if(!( RI.params & RP_OLDVIEWLEAF ))
		R_FindViewLeaf();
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL( void )
{
	if( RI.refdef.waterlevel >= 3 )
	{
		float f = sin( GET_CLIENT_TIME() * 0.4f * ( M_PI * 2.7f ));
		RI.refdef.fov_x += f;
		RI.refdef.fov_y -= f;
	}

	R_SetupModelviewMatrix( &RI.refdef, RI.worldviewMatrix );
	R_SetupProjectionMatrix( RI.refdef.fov_x, RI.refdef.fov_y, RI.projectionMatrix );

//	if( RI.params & RP_MIRRORVIEW ) RI.projectionMatrix[0][0] = -RI.projectionMatrix[0][0];

	RI.worldviewProjectionMatrix = RI.projectionMatrix.Concat( RI.worldviewMatrix );

	GLfloat dest[16];

	// tell engine about worldviewprojection matrix so TriWorldToScreen and TriScreenToWorld
	// will be working properly
	RI.worldviewProjectionMatrix.CopyToArray( dest );
	SET_ENGINE_WORLDVIEW_MATRIX( dest );

	pglScissor( RI.scissor[0], RI.scissor[1], RI.scissor[2], RI.scissor[3] );
	pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	if( RI.params & RP_CLIPPLANE )
	{
		GLdouble	clip[4];
		mplane_t	*p = &RI.clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}

	if( RI.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

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
	if( RI.params & RP_FLIPFRONTFACE )
		GL_FrontFace( !glState.frontFace );

	if( RI.params & RP_CLIPPLANE )
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
		if( node->contents != CONTENTS_WATER && node->contents != CONTENTS_LAVA && node->contents != CONTENTS_SLIME )
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
	int		i, cnt, count;

	RI.fogEnabled = false;

	int waterEntity = WATER_ENTITY( RI.vieworg );

	if( waterEntity >= 0 && waterEntity < RI.refdef.max_entities )
	{
		ent = GET_ENTITY( waterEntity );
	}

	if( ent && ent->model && ent->model->type == mod_brush && ent->curstate.skin < 0 )
		cnt = ent->curstate.skin;
	else cnt = (r_viewleaf) ? r_viewleaf->contents : CONTENTS_EMPTY;

	if( IsLiquidContents( RI.cached_contents ) && !IsLiquidContents( cnt ))
	{
		if( RI.refdef.movevars->fog_settings != 0 )
		{
			// enable global exponential color
			RI.fogColor[0] = ((RI.refdef.movevars->fog_settings & 0xFF000000) >> 24) / 255.0f;
			RI.fogColor[1] = ((RI.refdef.movevars->fog_settings & 0xFF0000) >> 16) / 255.0f;
			RI.fogColor[2] = ((RI.refdef.movevars->fog_settings & 0xFF00) >> 8) / 255.0f;
			RI.fogDensity = (RI.refdef.movevars->fog_settings & 0xFF) * 0.000025f;
			RI.fogStart = RI.fogEnd = 0.0f;
			RI.fogCustom = false;
			RI.fogEnabled = true;
		}
		RI.cached_contents = CONTENTS_EMPTY;
		return;
	}

	if( RI.refdef.waterlevel < 3 )
	{
		if( RI.refdef.movevars->fog_settings != 0 )
		{
			// enable global exponential color
			RI.fogColor[0] = ((RI.refdef.movevars->fog_settings & 0xFF000000) >> 24) / 255.0f;
			RI.fogColor[1] = ((RI.refdef.movevars->fog_settings & 0xFF0000) >> 16) / 255.0f;
			RI.fogColor[2] = ((RI.refdef.movevars->fog_settings & 0xFF00) >> 8) / 255.0f;
			RI.fogDensity = (RI.refdef.movevars->fog_settings & 0xFF) * 0.000025f;
			RI.fogStart = RI.fogEnd = 0.0f;
			RI.fogCustom = false;
			RI.fogEnabled = true;
		}
		return;
	}

	if( !IsLiquidContents( RI.cached_contents ) && IsLiquidContents( cnt ))
	{
		int texture = -1;

		// check for water texture
		if( ent && ent->model && ent->model->type == mod_brush )
		{
			msurface_t	*surf;
	
			count = ent->model->nummodelsurfaces;

			for( i = 0, surf = &ent->model->surfaces[ent->model->firstmodelsurface]; i < count; i++, surf++ )
			{
				if( surf->flags & SURF_DRAWTURB && surf->texinfo && surf->texinfo->texture )
				{
					texture = surf->texinfo->texture->gl_texturenum;
					RI.cached_contents = ent->curstate.skin;
					break;
				}
			}
		}
		else
		{

			texture = R_RecursiveFindWaterTexture( r_viewleaf->parent, NULL, false );
			if( texture != -1 ) RI.cached_contents = r_viewleaf->contents;
		}

		if( texture == -1 ) return; // no valid fogs

		color24	fogColor;
		byte	fogDensity;

		GET_EXTRA_PARAMS( texture, &fogColor.r, &fogColor.g, &fogColor.b, &fogDensity );

		// copy fog params
		RI.fogColor[0] = fogColor.r / 255.0f;
		RI.fogColor[1] = fogColor.g / 255.0f;
		RI.fogColor[2] = fogColor.b / 255.0f;
		RI.fogDensity = fogDensity * 0.000025f;
		RI.fogStart = RI.fogEnd = 0.0f;
		RI.fogCustom = false;
		RI.fogEnabled = true;
	}
	else
	{
		RI.fogCustom = false;
		RI.fogEnabled = true;
	}
}

/*
=============
R_DrawParticles

NOTE: particles are drawing with engine methods
=============
*/
void R_DrawParticles( void )
{
	// BUGBUG: drawing particles through sky in too many cases
	// because particles doesn't have individual check for pvs
	if( RI.params & RP_SKYPORTALVIEW ) return;

	DRAW_PARTICLES( RI.vieworg, RI.vforward, RI.vright, RI.vup, RI.clipFlags );
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

	R_DrawWaterSurfaces( false );

	// first draw solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI.currententity );
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI.currententity );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RI.currententity );
			break;
		default:
			break;
		}
	}

	CL_DrawBeams( false );

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );

	pglDepthMask( GL_FALSE );
	glState.drawTrans = true;

	R_DrawWaterSurfaces( true );

	// then draw translucent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		if( RI.refdef.onlyClientDraw )
			break;

		RI.currententity = tr.trans_entities[i];
		RI.currentmodel = RI.currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI.currententity );
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModel( RI.currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI.currententity );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RI.currententity );
			break;
		default:
			break;
		}
	}

	g_pParticleSystems->UpdateSystems();

	CL_DrawBeams( true );

	R_DrawParticles();

	R_DrawWeather();

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );

	glState.drawTrans = false;
	pglDepthMask( GL_TRUE );
	pglDisable( GL_BLEND );	// Trinity Render issues

	R_DrawViewModel();
}

/*
================
R_RenderScene

RI.refdef must be set before the first call
================
*/
void R_RenderScene( const ref_params_t *pparams )
{
	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderView: NULL worldmodel\n" );
		return;
	}

	R_RenderShadowmaps();

	RI.refdef = *pparams;
	tr.fIgnoreSkybox = false;

	if( tr.sky_camera && !( RI.params & RP_SKYPORTALVIEW ) && !RI.drawOrtho )
	{
		R_CheckSkyPortal( tr.sky_camera );
	}

	r_stats.num_passes++;
	r_stats.num_drawed_ents = 0;
	tr.framecount++;

	R_PushDlights();

	R_SetupFrame();
	R_SetupFrustum();
	R_SetupGL();
	R_Clear( ~0 );

	R_MarkLeaves();
	R_CheckFog();
	R_DrawWorld();

	R_DrawEntitiesOnList();

	R_EndGL();
}

void HUD_PrintStats( void )
{
	if( r_speeds->value <= 0 )
		return;

	msurface_t *surf = r_stats.debug_surface;

	switch( (int)r_speeds->value )
	{
	case 1:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i wpoly, %3i bpoly\n%3i epoly, %3i spoly\n%3i grass poly",
		r_stats.c_world_polys, r_stats.c_brush_polys, r_stats.c_studio_polys, r_stats.c_sprite_polys, r_stats.c_grass_polys );
		break;		
	case 2:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "visible leafs:\n%3i leafs\ncurrent leaf %3i",
		r_stats.c_world_leafs, r_viewleaf - worldmodel->leafs );
		break;
	case 3:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i studio models drawn\n%3i sprites drawn",
		r_stats.c_studio_models_drawn, r_stats.c_sprite_models_drawn );
		break;
	case 4:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i static entities\n%3i normal entities\n%3i cache count",
		r_numStatics, r_numEntities - r_numStatics, g_StudioRenderer.CacheCount() );
		break;
	case 5:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i tempents\n%3i viewbeams\n%3i particles",
		r_stats.c_active_tents_count, r_stats.c_view_beams_count, r_stats.c_particle_count );
		break;
	case 6:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i mirrors\n%3i portals\n%3i screens\n%3i sky passes\n%3i shadow passes",
		r_stats.c_mirror_passes, r_stats.c_portal_passes, r_stats.c_screen_passes, r_stats.c_sky_passes, r_stats.c_shadow_passes );
		break;
	case 7:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i projected lights\n",
		r_stats.c_plights );
		break;
	case 8:
		if( RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_BUILD_SURFMESHES )
		{
			if( surf && surf->texinfo && surf->texinfo->texture )
				Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%s\n", surf->texinfo->texture->name );
		}
		else
		{
			Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "debug trace is unavailable\n" );
		}
		break;
	case 9:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%3i drawed aurora particles\n%3i particle systems\n%3i flushes",
		r_stats.num_drawed_particles, r_stats.num_particle_systems, r_stats.num_flushes );
		break;
	case 10:
		Q_snprintf( r_speeds_msg, sizeof( r_speeds_msg ), "%s grass total size\n", Q_memprint( tr.grass_total_size ));
		break;
	}

	memset( &r_stats, 0, sizeof( r_stats ));
}

void R_RenderDebugSurface( void )
{
	r_stats.debug_surface = NULL;

	if( r_speeds->value != 8 )
		return;

	pmtrace_t trace;
	r_stats.debug_surface = R_TraceLine( &trace, RI.vieworg, RI.vieworg + RI.vforward * 4096, 0 );
	if( !r_stats.debug_surface ) return;

	cl_entity_t *ent = GET_ENTITY( trace.ent );

	if( !ent || !ent->model || ent->model->type != mod_brush )
		return;

	mextrasurf_t *info = SURF_INFO( r_stats.debug_surface, ent->model );
	if( !info || !info->mesh ) return; 

	msurfmesh_t *mesh = info->mesh;

	if( ent->curstate.angles != g_vecZero )
		R_RotateForEntity( ent );
	else R_TranslateForEntity( ent );

	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );	// red bboxes for studiomodels

	// turbulent surfaces not a valid polygons. just triangles
	if( r_stats.debug_surface->flags & SURF_DRAWTURB )
	{
		pglBegin( GL_TRIANGLES );

		for( int i = 0; i < mesh->numElems; i += 3 )
		{
			pglVertex3fv( mesh->verts[mesh->elems[i+0]].vertex );
			pglVertex3fv( mesh->verts[mesh->elems[i+1]].vertex );
			pglVertex3fv( mesh->verts[mesh->elems[i+2]].vertex );
                    }

		pglEnd();
	}
	else
	{
		pglBegin( GL_POLYGON );

		for( int i = 0; i < mesh->numVerts; i++ )
			pglVertex3fv( mesh->verts[i].vertex );

		pglEnd();
	}

	pglEnable( GL_TEXTURE_2D );
	pglEnable( GL_DEPTH_TEST );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	R_LoadIdentity ();
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
int HUD_RenderFrame( const ref_params_t *pparams, qboolean drawWorld )
{
	tr.fCustomRendering = false;

	if( !g_fRenderInitialized )
		return 0;

	r_speeds_msg[0] = '\0';

	// always the copy params in case we need have valid movevars
	RI.refdef = *pparams;
	tr.cached_refdef = pparams;

	// it's playersetup overview, ignore it	
	if( !drawWorld ) return 0;

	// we are in dev_overview mode, ignore it
	if( r_overview && r_overview->value )
	{
		RI.drawOrtho = true;
		tr.fResetVis = true;
	}
	else
	{
		RI.drawOrtho = false;
	}

	// use engine renderer
	if( gl_renderer->value == 0 )
	{
		tr.fResetVis = true;
		return 0;
	}

	tr.fCustomRendering = true;
	r_lastRefdef = *pparams;
	RI.params = RP_NONE;
	RI.farClip = 0;
	RI.clipFlags = 15;
	RI.drawWorld = true;
	RI.thirdPerson = gHUD.m_iCameraMode;
	RI.pvsorigin = pparams->vieworg;
	memset( RI.frustum, 0, sizeof( RI.frustum ));

	// setup scissor
	RI.scissor[0] = pparams->viewport[0];
	RI.scissor[1] = pparams->viewport[1];
	RI.scissor[2] = pparams->viewport[2];
	RI.scissor[3] = pparams->viewport[3];

	// setup viewport
	RI.viewport[0] = pparams->viewport[0];
	RI.viewport[1] = pparams->viewport[1];
	RI.viewport[2] = pparams->viewport[2];
	RI.viewport[3] = pparams->viewport[3];

	if( r_finish->value ) pglFinish();

	if( r_allow_screens->value )
	{
		// render screens
		if( R_FindScreens( pparams ))
			R_DrawScreens ();
	}

	if( r_allow_mirrors->value )
	{
		// render mirrors
		if( R_FindMirrors( pparams ))
			R_DrawMirrors ();
	}

	if( r_allow_portals->value )
	{
		// render portals
		if( R_FindPortals( pparams ))
			R_DrawPortals ();
	}

	// draw main view
	R_RenderScene( pparams );
	tr.realframecount++;

	R_RenderDebugSurface ();

	R_BloomBlend( pparams );

	HUD_PrintStats ();

	return 1;
}

/*
===============
HUD_DrawCubemapView
===============
*/
BOOL HUD_DrawCubemapView( const float *origin, const float *angles, int size )
{
	ref_params_t *fd;

	if( !tr.fCustomRendering )
		return 0;

	fd = &RI.refdef;
	*fd = r_lastRefdef;
	fd->time = 0;
	fd->viewport[0] = 0;
	fd->viewport[1] = 0;
	fd->viewport[2] = size;
	fd->viewport[3] = size;
	fd->fov_x = 90;
	fd->fov_y = 90;
	fd->vieworg = origin;
	fd->viewangles = angles;
	RI.pvsorigin = fd->vieworg;
	RI.params |= RP_ENVVIEW;
		
	// setup scissor
	RI.scissor[0] = fd->viewport[0];
	RI.scissor[1] = fd->viewport[1];
	RI.scissor[2] = fd->viewport[2];
	RI.scissor[3] = fd->viewport[3];

	// setup viewport
	RI.viewport[0] = fd->viewport[0];
	RI.viewport[1] = fd->viewport[1];
	RI.viewport[2] = fd->viewport[2];
	RI.viewport[3] = fd->viewport[3];

	R_RenderScene( fd );

	r_viewleaf = r_viewleaf2 = NULL;		// force markleafs next frame
	RI.params &= ~RP_ENVVIEW;

	return 1;
}

BOOL HUD_SpeedsMessage( char *out, size_t size )
{
	if( !gl_renderer || !gl_renderer->value )
		return false; // let the engine use built-in counters

	if( r_speeds->value <= 0 ) return false;
	if( !out || !size ) return false;

	Q_strncpy( out, r_speeds_msg, size );

	return true;
}

void HUD_ProcessEntData( qboolean allocate )
{
	if( allocate ) Mod_PrepareModelInstances();
	else Mod_ThrowModelInstances();
}

//
// Xash3D render interface
//
static render_interface_t gRenderInterface = 
{
	CL_RENDER_INTERFACE_VERSION,
	HUD_RenderFrame,
	HUD_BuildLightmaps,
	HUD_SetOrthoBounds,
	R_StudioDecalShoot,
	R_CreateStudioDecalList,
	R_ClearStudioDecals,
	HUD_SpeedsMessage,
	HUD_DrawCubemapView,
	NULL,
	HUD_ProcessEntData,
};

int HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback )
{
	if ( !callback || !renderfuncs || version != CL_RENDER_INTERFACE_VERSION )
	{
		return FALSE;
	}

	size_t iImportSize = sizeof( render_interface_t );
	size_t iExportSize = sizeof( render_api_t );

	if( g_iXashEngineBuildNumber <= 3500 )
		iImportSize -= 4; // skip the HUD_ProcessEntData

	// copy new physics interface
	memcpy( &gRenderfuncs, renderfuncs, iExportSize );

	// fill engine callbacks
	memcpy( callback, &gRenderInterface, iImportSize );

	g_fRenderInitialized = TRUE;

	return TRUE;
}
