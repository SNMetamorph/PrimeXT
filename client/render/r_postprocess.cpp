/*
r_postprocess.cpp - post-processing effects
Copyright (C) 2018 Uncle Mike

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
#include "r_shader.h"

#define TARGET_SIZE		128

cvar_t *v_sunshafts;
static GLuint framebuffer[3];
static GLuint fb_bkp;
void InitPostEffects( void )
{
	v_sunshafts = CVAR_REGISTER( "gl_sunshafts", "1", FCVAR_ARCHIVE );
}

void InitPostTextures( void )
{
	if( tr.screen_depth )
	{
		FREE_TEXTURE( tr.screen_depth );
		tr.screen_depth = 0;
	}

	if( !tr.screen_depth ) // in case FBO not support
		tr.screen_depth = CREATE_TEXTURE( "*screendepth", glState.width, glState.height, NULL, TF_DEPTHBUFFER ); 

	if( tr.screen_color )
	{
		FREE_TEXTURE( tr.screen_color );
		tr.screen_color = 0;
	}

	if( !tr.screen_color )
		tr.screen_color = CREATE_TEXTURE( "*screencolor", glState.width, glState.height, NULL, TF_COLORBUFFER );

	if( tr.target_rgb[0] )
	{
		FREE_TEXTURE( tr.target_rgb[0] );
		tr.target_rgb[0] = 0;
	}

	if( !tr.target_rgb[0] )
		tr.target_rgb[0] = CREATE_TEXTURE( "*target0", TARGET_SIZE, TARGET_SIZE, NULL, TF_SCREEN );

	if( GL_Support( R_FRAMEBUFFER_OBJECT ) && GL_Support( R_ARB_TEXTURE_NPOT_EXT ) )
	{
		// for fbo, we need second target texture
		if( tr.target_rgb[1] )
			FREE_TEXTURE( tr.target_rgb[1] );

		tr.target_rgb[1] = CREATE_TEXTURE( "*target1", TARGET_SIZE, TARGET_SIZE, NULL, TF_SCREEN );

		if( framebuffer[0] )
			pglDeleteFramebuffers( 3, framebuffer );
		pglGenFramebuffers( 3, framebuffer );

		// same texture used for main view and subviews
		// if we need to support NPOT, need allocate depth textures for subviews and use it directly
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, framebuffer[0]);
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, tr.screen_color ), 0 );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, tr.screen_depth ), 0 );
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, framebuffer[1]);
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, tr.target_rgb[0] ), 0 );
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, framebuffer[2]);
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, RENDER_GET_PARM( PARM_TEX_TEXNUM, tr.target_rgb[1] ), 0 );
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, glState.frameBuffer == -1? 0 : glState.frameBuffer );
	}
}

void R_BindPostFramebuffers( void )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ) || !GL_Support( R_ARB_TEXTURE_NPOT_EXT) )
		return;

	if( !CVAR_TO_BOOL( v_sunshafts ) || !tr.screen_depth || !tr.screen_color || !tr.target_rgb[0] )
		return;

	if( !CheckShader( glsl.genSunShafts ) || !CheckShader( glsl.drawSunShafts ))
		return;

	if( !CheckShader( glsl.blurShader[0] ) || !CheckShader( glsl.blurShader[1] ))
		return;

	float blur = 0.2f;
	float zNear = Z_NEAR;
	float zFar = Q_max( 256.0f, RI->farClip );
	Vector skyColor = tr.sky_ambient * (1.0f / 128.0f) * r_lighting_modulate->value;
	Vector skyVec = tr.sky_normal.Normalize();
	Vector sunPos = RI->vieworg + skyVec * 1000.0;

	if( skyVec == g_vecZero ) return;

	ColorNormalize( skyColor, skyColor );

	Vector ndc, view;

	// project sunpos to screen
	R_TransformWorldToDevice( sunPos, ndc );
	R_TransformDeviceToScreen( ndc, view );
	view.z = DotProduct( -skyVec, RI->vforward );

	if( view.z < 0.01f ) return;	// fade out

	fb_bkp = glState.frameBuffer;
	GL_BindFBO( framebuffer[0] );
}

void RenderFSQ( int wide, int tall )
{
	float screenWidth = (float)wide;
	float screenHeight = (float)tall;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex2f( 0.0f, 0.0f );
		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex2f( screenWidth, 0.0f );
		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex2f( screenWidth, screenHeight );
		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex2f( 0.0f, screenHeight );
	pglEnd();
}

void RenderSunShafts( void )
{
	qboolean fbo = GL_Support( R_FRAMEBUFFER_OBJECT ) && GL_Support( R_ARB_TEXTURE_NPOT_EXT );

	if( !CVAR_TO_BOOL( v_sunshafts ) || !tr.screen_depth || !tr.screen_color || !tr.target_rgb[0] )
		return;

	if( !CheckShader( glsl.genSunShafts ) || !CheckShader( glsl.drawSunShafts ))
		return;

	if( !CheckShader( glsl.blurShader[0] ) || !CheckShader( glsl.blurShader[1] ))
		return;

	float blur = 0.2f;
	float zNear = Z_NEAR;
	float zFar = Q_max( 256.0f, RI->farClip );
	Vector skyColor = tr.sky_ambient * (1.0f / 128.0f) * r_lighting_modulate->value;
	Vector skyVec = tr.sky_normal.Normalize();
	Vector sunPos = RI->vieworg + skyVec * 1000.0;

	if( skyVec == g_vecZero ) return;

	ColorNormalize( skyColor, skyColor );

	Vector ndc, view;

	// project sunpos to screen 
	R_TransformWorldToDevice( sunPos, ndc );
	R_TransformDeviceToScreen( ndc, view );
	view.z = DotProduct( -skyVec, RI->vforward );

	if( view.z < 0.01f ) return;	// fade out

	GL_Setup2D();

	if( !fbo )
	{
		// capture screen
		GL_Bind( GL_TEXTURE0, tr.screen_color );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );

		// capture depth, if not captured previously in SSAO
		GL_Bind( GL_TEXTURE0, tr.screen_depth );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, glState.width, glState.height );
	}
	else
	{
		// render to target0
		GL_BindFBO( framebuffer[1] );
	}

	pglViewport( 0, 0, TARGET_SIZE, TARGET_SIZE );

	// generate shafts
	GL_BindShader( glsl.genSunShafts );
	ASSERT( RI->currentshader != NULL );

	pglUniform2fARB( RI->currentshader->u_ScreenSizeInv, 1.0f / glState.width, 1.0f / glState.height );
	pglUniform1fARB( RI->currentshader->u_zFar, zFar );
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	GL_Bind( GL_TEXTURE1, tr.screen_depth );
	RenderFSQ( glState.width, glState.height );

	GL_Bind( GL_TEXTURE0, tr.target_rgb[0] );

	if( fbo ) // render from target0 to target1
		GL_BindFBO( framebuffer[2] );
	else
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, TARGET_SIZE, TARGET_SIZE );

	pglViewport( 0, 0, TARGET_SIZE, TARGET_SIZE );

	// combine normal and blurred scenes
	GL_BindShader( glsl.blurShader[0] );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_BlurFactor, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	if( fbo )
	{
		// render from target1 to target0
		GL_BindFBO( framebuffer[1] );
		pglViewport( 0, 0, TARGET_SIZE, TARGET_SIZE );
		GL_Bind( GL_TEXTURE0, tr.target_rgb[1] );
	}
	else
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, TARGET_SIZE, TARGET_SIZE );

	GL_BindShader( glsl.blurShader[1] );
	ASSERT( RI->currentshader != NULL );

	pglUniform1fARB( RI->currentshader->u_BlurFactor, blur );	// set blur factor
	RenderFSQ( glState.width, glState.height );

	if( fbo )
	{
		// render from target0 to screen
		GL_Bind( GL_TEXTURE0, tr.target_rgb[0] );
		// reset framebuffer
		GL_BindFBO( fb_bkp );
	}
	else
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, TARGET_SIZE, TARGET_SIZE );

	pglViewport( 0, 0, glState.width, glState.height );

	GL_BindShader( glsl.drawSunShafts );
	ASSERT( RI->currentshader != NULL );

	pglUniform3fARB( RI->currentshader->u_LightOrigin, view.x / glState.width, view.y / glState.height, view.z );
	pglUniform3fARB( RI->currentshader->u_LightDiffuse, skyColor.x, skyColor.y, skyColor.z );
	pglUniform1fARB( RI->currentshader->u_zFar, zFar );
	GL_Bind( GL_TEXTURE0, tr.screen_color );
	GL_Bind( GL_TEXTURE1, tr.target_rgb[0] );
	RenderFSQ( glState.width, glState.height );

	GL_BindShader( NULL );
	GL_CleanUpTextureUnits( 0 );

	GL_Setup3D();
}
