/*
r_shadows.cpp - render shadowmaps for directional lights
Copyright (C) 2012 Uncle Mike

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
#include "event_api.h"
#include "r_studio.h"
#include "r_sprite.h"

static GLuint framebuffer[MAX_SHADOWS];

/*
=============================================================

	SHADOW RENDERING

=============================================================
*/
int R_AllocateShadowFramebuffer( void )
{
	int i = tr.num_shadows_used;

	if( !framebuffer[0] )
		pglGenFramebuffers( MAX_SHADOWS, framebuffer );

	if( i >= MAX_SHADOWS )
	{
		ALERT( at_error, "R_AllocateShadowTexture: shadow textures limit exceeded!\n" );
		return 0; // disable
	}

	int texture = tr.shadowTextures[i];
	tr.num_shadows_used++;

	if( !tr.shadowTextures[i] )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );

		tr.shadowTextures[i] = CREATE_TEXTURE( txName, RI->viewport[2], RI->viewport[3], NULL, TF_SHADOW );
		texture = tr.shadowTextures[i];
	}

	GL_BindFBO( framebuffer[i] );
	pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, texture ), 0 );

	return texture;
}

int R_AllocateShadowTexture( void )
{
	int i = tr.num_shadows_used;

	if( i >= MAX_SHADOWS )
	{
		ALERT( at_error, "R_AllocateShadowTexture: shadow textures limit exceeded!\n" );
		return 0; // disable
	}

	int texture = tr.shadowTextures[i];
	tr.num_shadows_used++;

	if( !tr.shadowTextures[i] )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );

		tr.shadowTextures[i] = CREATE_TEXTURE( txName, RI->viewport[2], RI->viewport[3], NULL, TF_SHADOW ); 
		texture = tr.shadowTextures[i];
	}

	GL_Bind( GL_TEXTURE0, texture );
	pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3], 0 );

	return texture;
}

/*
===============
R_ShadowPassSetupFrame
===============
*/
static void R_ShadowPassSetupFrame( plight_t *pl )
{
	// build the transformation matrix for the given view angles
	RI->viewangles[0] = anglemod( pl->angles[0] );
	RI->viewangles[1] = anglemod( pl->angles[1] );
	RI->viewangles[2] = anglemod( pl->angles[2] );	
	AngleVectors( RI->viewangles, RI->vforward, RI->vright, RI->vup );
	RI->vieworg = pl->origin;

	// setup the screen FOV
	RI->fov_x = pl->fov;

	if( pl->flags & CF_ASPECT3X4 )
		RI->fov_y = pl->fov * (5.0f / 4.0f); 
	else if( pl->flags & CF_ASPECT4X3 )
		RI->fov_y = pl->fov * (4.0f / 5.0f);
	else RI->fov_y = pl->fov;

	// setup frustum
	RI->frustum = pl->frustum;

	if(!( RI->params & RP_OLDVIEWLEAF ))
		R_FindViewLeaf();

	RI->currentlight = pl;
}

/*
=============
R_ShadowPassSetupGL
=============
*/
static void R_ShadowPassSetupGL( const plight_t *pl )
{
	// matrices already computed
	RI->worldviewMatrix = pl->modelviewMatrix;
	RI->projectionMatrix = pl->projectionMatrix;
	RI->worldviewProjectionMatrix = RI->projectionMatrix.Concat( RI->worldviewMatrix );

	pglViewport( RI->viewport[0], RI->viewport[1], RI->viewport[2], RI->viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->worldviewMatrix );

	GL_Cull( GL_FRONT );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	pglEnable( GL_POLYGON_OFFSET_FILL );
	pglDisable( GL_TEXTURE_2D );
	pglDepthMask( GL_TRUE );
	pglPolygonOffset( 1.0f, 2.0f );
	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_BLEND );
}

/*
=============
R_ShadowPassEndGL
=============
*/
static void R_ShadowPassEndGL( void )
{
	pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglEnable( GL_TEXTURE_2D );
	pglPolygonOffset( -1, -2 );
	GL_BindShader( GL_FALSE );
}


/*
=============
R_ShadowPassDrawWorld
=============
*/
static void R_ShadowPassDrawWorld( plight_t *pl )
{
	if( FBitSet( pl->flags, CF_NOWORLD_PROJECTION ))
		return;	// no worldlight, no worldshadows

	R_DrawWorldShadowPass();
}

void R_ShadowPassDrawSolidEntities( plight_t *pl )
{
	int	i;

	glState.drawTrans = false;

	// draw solid entities only.
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI->currententity = tr.solid_entities[i];
		RI->currentmodel = RI->currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		switch( RI->currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModelShadow( RI->currententity );
			break;
		case mod_studio:
			R_DrawStudioModel( RI->currententity );
			break;
		case mod_sprite:
//			R_DrawSpriteModel( RI->currententity );
			break;
		default:
			break;
		}
	}

	// draw solid entities only.
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		RI->currententity = tr.trans_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currententity->curstate.rendermode != kRenderTransAlpha )
			continue;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI->currententity );

		switch( RI->currentmodel->type )
		{
		case mod_brush:
			R_DrawBrushModelShadow( RI->currententity );
			break;
		default:
			break;
		}
	}

	// may be solid cables
	R_DrawParticles( false );
}

/*
================
R_RenderScene

RI->refdef must be set before the first call
fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadowScene( plight_t *pl )
{
	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderShadowView: NULL worldmodel\n" );
		return;
	}

	r_stats.num_passes++;
	R_ShadowPassSetupFrame( pl );
	R_ShadowPassSetupGL( pl );
	pglClear( GL_DEPTH_BUFFER_BIT );

	R_MarkLeaves();
	R_ShadowPassDrawWorld( pl );
	R_ShadowPassDrawSolidEntities( pl );

	R_ShadowPassEndGL();
}

void R_RenderShadowmaps( void )
{
	if( CVAR_TO_BOOL( r_fullbright ) || !CVAR_TO_BOOL( r_shadows ))
		return;

	if( tr.shadows_notsupport || tr.fGamePaused )
		return;

	// check for dynamic lights
	if( !R_CountPlights( true )) return;

	R_PushRefState();

	for( int i = 0; i < MAX_PLIGHTS; i++ )
	{
		plight_t *pl = &cl_plights[i];
		GLuint oldfb = glState.frameBuffer;

		if( pl->die < tr.time || !pl->radius || pl->culled )
			continue;

		// TODO: allow shadows for pointlights
		if( pl->pointlight || FBitSet( pl->flags, CF_NOSHADOWS ))
		{
			pl->shadowTexture = tr.depthTexture;
			continue;
		}

		if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
			continue;

		if( R_CullBox( pl->absmin, pl->absmax ))
			continue;

		RI->params = RP_SHADOWVIEW|RP_MERGEVISIBILITY;

		// allow screen size
		RI->viewport[2] = RI->viewport[3] = 512;

		if( GL_Support( R_FRAMEBUFFER_OBJECT ) )
			pl->shadowTexture = R_AllocateShadowFramebuffer();

		R_RenderShadowScene( pl );
		r_stats.c_shadow_passes++;

		if( GL_Support( R_FRAMEBUFFER_OBJECT ) )
			GL_BindFBO( oldfb );
		else
			pl->shadowTexture = R_AllocateShadowTexture();
		R_ResetRefState();
	}

	R_PopRefState();
}
