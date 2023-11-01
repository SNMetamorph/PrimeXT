/*
gl_deferred.cpp - deferred rendering
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
#include "gl_local.h"
#include "gl_shader.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"
#include "gl_debug.h"

enum
{
	SCENE_PASS = 0,
	LIGHT_PASS,
};

enum
{
	BILATERAL_PASS0 = 0,
	BILATERAL_PASS1,
};

void GL_InitDefSceneFBO( void )
{
	GLenum	MRTBuffers[5] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_COLOR_ATTACHMENT3_EXT, GL_COLOR_ATTACHMENT4_EXT };
	TextureHandle	albedo_buffer;
	TextureHandle	normal_buffer;
	TextureHandle	smooth_buffer;
	TextureHandle	light0_buffer;
	TextureHandle	light1_buffer;
	TextureHandle	depth_buffer;

	if( !GL_Support( R_FRAMEBUFFER_OBJECT ) || !GL_Support( R_DRAW_BUFFERS_EXT ))
		return;

	// Create and set up the framebuffer
	tr.defscene_fbo = GL_AllocDrawbuffer( "*defscene", glState.width, glState.height, 1 );

	// create textures
	albedo_buffer = CREATE_TEXTURE( "*albedo_rt", glState.width, glState.height, NULL, TF_RT_COLOR|TF_HAS_ALPHA );
	normal_buffer = CREATE_TEXTURE( "*normal_rt", glState.width, glState.height, NULL, TF_RT_NORMAL|TF_HAS_ALPHA );
	smooth_buffer = CREATE_TEXTURE( "*smooth_rt", glState.width, glState.height, NULL, TF_RT_COLOR|TF_HAS_ALPHA );
	light0_buffer = CREATE_TEXTURE( "*light0_rt", glState.width, glState.height, NULL, TF_RT_COLOR|TF_HAS_ALPHA );
	light1_buffer = CREATE_TEXTURE( "*light1_rt", glState.width, glState.height, NULL, TF_RT_COLOR|TF_HAS_ALPHA );
	depth_buffer = CREATE_TEXTURE( "*depth_rt", glState.width, glState.height, NULL, TF_RT_DEPTH );

	GL_AttachColorTextureToFBO( tr.defscene_fbo, albedo_buffer, 0);
	GL_AttachColorTextureToFBO( tr.defscene_fbo, normal_buffer, 1 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, smooth_buffer, 2 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, light0_buffer, 3 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, light1_buffer, 4 );
	GL_AttachDepthTextureToFBO( tr.defscene_fbo, depth_buffer );

	pglDrawBuffersARB( ARRAYSIZE( MRTBuffers ), MRTBuffers );
	pglReadBuffer( GL_NONE );

	// check the framebuffer status
	GL_CheckFBOStatus( tr.defscene_fbo );
}

void GL_VidInitDefSceneFBO( void )
{
	// resize attached textures
	GL_ResizeDrawbuffer( tr.defscene_fbo, glState.width, glState.height, 1 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, tr.defscene_fbo->colortarget[0], 0 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, tr.defscene_fbo->colortarget[1], 1 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, tr.defscene_fbo->colortarget[2], 2 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, tr.defscene_fbo->colortarget[3], 3 );
	GL_AttachColorTextureToFBO( tr.defscene_fbo, tr.defscene_fbo->colortarget[4], 4 );
	GL_AttachDepthTextureToFBO( tr.defscene_fbo, tr.defscene_fbo->depthtarget );
}

void GL_InitDefLightFBO( void )
{
	GLenum	MRTBuffers[3] = { GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_COLOR_ATTACHMENT2_EXT };
	TextureHandle	normal_buffer;
	TextureHandle	light0_buffer;
	TextureHandle	light1_buffer;
	TextureHandle	depth_buffer;

	if( !GL_Support( R_FRAMEBUFFER_OBJECT ) || !GL_Support( R_DRAW_BUFFERS_EXT ))
		return;

	// Create and set up the framebuffer
	tr.deflight_fbo = GL_AllocDrawbuffer( "*deflight", glState.defWidth, glState.defHeight, 1 );

	// create textures
	normal_buffer = CREATE_TEXTURE( "*normal_rs", glState.defWidth, glState.defHeight, NULL, TF_RT_NORMAL|TF_HAS_ALPHA );
	light0_buffer = CREATE_TEXTURE( "*light0_rs", glState.defWidth, glState.defHeight, NULL, TF_RT_COLOR|TF_HAS_ALPHA );
	light1_buffer = CREATE_TEXTURE( "*light1_rs", glState.defWidth, glState.defHeight, NULL, TF_RT_COLOR|TF_HAS_ALPHA );
	depth_buffer = CREATE_TEXTURE( "*depth_rs", glState.defWidth, glState.defHeight, NULL, TF_RT_DEPTH );

	GL_AttachColorTextureToFBO( tr.deflight_fbo, normal_buffer, 0);
	GL_AttachColorTextureToFBO( tr.deflight_fbo, light0_buffer, 1);
	GL_AttachColorTextureToFBO( tr.deflight_fbo, light1_buffer, 2);
	GL_AttachDepthTextureToFBO( tr.deflight_fbo, depth_buffer );

	pglDrawBuffersARB( ARRAYSIZE( MRTBuffers ), MRTBuffers );
	pglReadBuffer( GL_NONE );

	// check the framebuffer status
	GL_CheckFBOStatus( tr.deflight_fbo );
}

void GL_VidInitDefLightFBO( void )
{
	// resize attached textures
	GL_ResizeDrawbuffer( tr.deflight_fbo, glState.defWidth, glState.defHeight, 1 );
	GL_AttachColorTextureToFBO( tr.deflight_fbo, tr.deflight_fbo->colortarget[0], 0 );
	GL_AttachColorTextureToFBO( tr.deflight_fbo, tr.deflight_fbo->colortarget[1], 1 );
	GL_AttachColorTextureToFBO( tr.deflight_fbo, tr.deflight_fbo->colortarget[2], 2 );
	GL_AttachDepthTextureToFBO( tr.deflight_fbo, tr.deflight_fbo->depthtarget );
}

void GL_VidInitDrawBuffers( void )
{
	if( tr.defscene_fbo == NULL )
		GL_InitDefSceneFBO();
	else GL_VidInitDefSceneFBO();

	if( tr.deflight_fbo == NULL )
		GL_InitDefLightFBO();
	else GL_VidInitDefLightFBO();

	// unbind the framebuffer
	GL_BindDrawbuffer( NULL );
}

void GL_SetupGBuffer( void )
{
	// bind geometry fullscreen buffer
	if( FBitSet( RI->params, RP_DEFERREDSCENE ))
		GL_BindDrawbuffer( tr.defscene_fbo );

	// bind light-pass downscaled buffer
	if( FBitSet( RI->params, RP_DEFERREDLIGHT ))
		GL_BindDrawbuffer( tr.deflight_fbo );

	R_Clear( ~0 );
}

void GL_ResetGBuffer( void )
{
	GL_BindDrawbuffer( NULL );
}

void GL_DrawScreenSpaceQuad( const vec3_t normals[4] )
{
	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglNormal3fv( normals[0] );
		pglVertex2f( 0.0f, 0.0f );
		pglTexCoord2f( 1.0f, 1.0f );
		pglNormal3fv( normals[3] );
		pglVertex2f( RI->view.port[2], 0.0f );
		pglTexCoord2f( 1.0f, 0.0f );
		pglNormal3fv( normals[2] );
		pglVertex2f( RI->view.port[2], RI->view.port[3] );
		pglTexCoord2f( 0.0f, 0.0f );
		pglNormal3fv( normals[1] );
		pglVertex2f( 0.0f, RI->view.port[3] );
	pglEnd();
}

/*
=================
GL_Setup2D
=================
*/
void GL_SetupDeferred( void )
{
	GL_DEBUG_SCOPE();
	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, RI->view.port[2], RI->view.port[3], 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	pglDisable( GL_DEPTH_TEST );	// older dirvers has issues with this

	if( RI->currentlight != NULL )
	{
		GL_Blend( GL_TRUE );
		pglBlendFunc( GL_ONE, GL_ONE );
	}
	else
	{
		GL_Blend( GL_FALSE );
	}

	GL_Cull( GL_FRONT );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, RI->view.port.GetWidth(), RI->view.port.GetHeight() );
}

static void GL_DrawDeferred( word hProgram, int pass )
{
	if( hProgram <= 0 )
	{
		GL_BindShader( NULL );
		return; // bad shader?
	}

	// prepare deferred pass
	GL_DEBUG_SCOPE();
	GL_SetupDeferred();

	if( RI->currentshader != &glsl_programs[hProgram] )
	{
		// force to bind new shader
		GL_BindShader( &glsl_programs[hProgram] );
	}

	glsl_program_t *shader = RI->currentshader;
	CDynLight *pl = RI->currentlight; // may be NULL
	Vector4D lightdir;
	float *v;

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			u->SetValue( tr.defscene_fbo->colortarget[0].ToInt() );
			break;
		case UT_NORMALMAP:
			if( pass == LIGHT_PASS )
				u->SetValue( tr.deflight_fbo->colortarget[0].ToInt() );
			else 
				u->SetValue( tr.defscene_fbo->colortarget[1].ToInt() );
			break;
		case UT_GLOSSMAP:
			u->SetValue( tr.defscene_fbo->colortarget[2].ToInt() );
			break;
		case UT_VISLIGHTMAP0:
			if( pass == LIGHT_PASS )
				u->SetValue( tr.deflight_fbo->colortarget[1].ToInt() );
			else 
				u->SetValue( tr.defscene_fbo->colortarget[3].ToInt() );
			break;
		case UT_VISLIGHTMAP1:
			if( pass == LIGHT_PASS )
				u->SetValue( tr.deflight_fbo->colortarget[2].ToInt() );
			else 
				u->SetValue( tr.defscene_fbo->colortarget[4].ToInt() );
			break;
		case UT_DEPTHMAP:
			if( pass == LIGHT_PASS )
				u->SetValue( tr.deflight_fbo->depthtarget.ToInt() );
			else 
				u->SetValue( tr.defscene_fbo->depthtarget.ToInt() );
			break;
		case UT_BSPPLANESMAP:
			u->SetValue( tr.packed_planes_texture.ToInt() );
			break;
		case UT_BSPNODESMAP:
			u->SetValue( tr.packed_nodes_texture.ToInt() );
			break;
		case UT_BSPLIGHTSMAP:
			u->SetValue( tr.packed_lights_texture.ToInt() );
			break;
		case UT_BSPMODELSMAP:
			u->SetValue( tr.packed_models_texture.ToInt() );
			break;
		case UT_SHADOWMAP:
			if( pl ) 
				u->SetValue( pl->shadowTexture[0].ToInt() );
			else 
				u->SetValue( tr.fbo_light.GetTexture().ToInt() );
			break;
		case UT_SHADOWMAP0:
			if( pl ) 
				u->SetValue( pl->shadowTexture[0].ToInt() );
			else 
				u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP1:
			if( pl ) 
				u->SetValue(pl->shadowTexture[1].ToInt() );
			else 
				u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP2:
			if( pl ) 
				u->SetValue( pl->shadowTexture[2].ToInt() );
			else 
				u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP3:
			if( pl ) 
				u->SetValue( pl->shadowTexture[3].ToInt() );
			else 
				u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_PROJECTMAP:
			if( pl && pl->type == LIGHT_SPOT )
				u->SetValue( pl->spotlightTexture.ToInt() );
			else 
				u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( RI->view.matrix[3][0], RI->view.matrix[3][1], RI->view.matrix[3][2] );
			break;
		case UT_LIGHTSTYLEVALUES:
			u->SetValue( &tr.lightstyle[0], MAX_LIGHTSTYLES );
			break;
		case UT_GAMMATABLE: // TODO get rid of this
			u->SetValue( &tr.gamma_table[0][0], 64 );
			break;
		case UT_LIGHTGAMMA:
			u->SetValue( tr.light_gamma );
			break;
		case UT_LIGHTTHRESHOLD:
			u->SetValue( tr.light_threshold );
			break;
		case UT_LIGHTSCALE:
			u->SetValue( tr.direct_scale );
			break;
		case UT_ZFAR:
			u->SetValue( RI->view.farClip );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_AMBIENTFACTOR:
			if( pl && pl->type == LIGHT_DIRECTIONAL )
				u->SetValue( tr.sun_ambient );
			else u->SetValue( tr.ambientFactor );
			break;
		case UT_SUNREFRACT:
			u->SetValue( tr.sun_refract );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		case UT_SHADOWPARMS:
			if( pl != NULL )
			{
				float shadowWidth = 1.0f / (float)pl->shadowTexture[0].GetWidth();
				float shadowHeight = 1.0f / (float)pl->shadowTexture[0].GetHeight();
				// depth scale and bias and shadowmap resolution
				u->SetValue( shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
			}
			else u->SetValue( 0.0f, 0.0f, 0.0f, 0.0f );
			break;
		case UT_SHADOWMATRIX:
			if( pl ) u->SetValue( &pl->gl_shadowMatrix[0][0], MAX_SHADOWMAPS );
			break;
		case UT_SHADOWSPLITDIST:
			v = RI->view.parallelSplitDistances;
			u->SetValue( v[0], v[1], v[2], v[3] );
			break;
		case UT_TEXELSIZE:
			u->SetValue( 1.0f / (float)sunSize[0], 1.0f / (float)sunSize[1], 1.0f / (float)sunSize[2], 1.0f / (float)sunSize[3] );
			break;
		case UT_LIGHTDIR:
			if( pl )
			{
				if( pl->type == LIGHT_DIRECTIONAL ) lightdir = -tr.sky_normal;
				else lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
				u->SetValue( lightdir.x, lightdir.y, lightdir.z, pl->fov );
			}
			break;
		case UT_LIGHTDIFFUSE:
			if( pl ) u->SetValue( pl->color.x, pl->color.y, pl->color.z );
			break;
		case UT_LIGHTORIGIN:
			if( pl ) u->SetValue( pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius )); 
			break;
		case UT_LIGHTVIEWPROJMATRIX:
			if( pl )
			{
				GLfloat gl_lightViewProjMatrix[16];
				pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );
				u->SetValue( &gl_lightViewProjMatrix[0] );
			}
			break;
		case UT_NUMVISIBLEMODELS:
			u->SetValue( world->num_visible_models );
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}

	GL_DrawScreenSpaceQuad( tr.screen_normals );
}

static void GL_DrawBilateralFilter( word hProgram, int pass )
{
	if( hProgram <= 0 )
	{
		GL_BindShader( NULL );
		return; // bad shader?
	}

	if( RI->currentshader != &glsl_programs[hProgram] )
	{
		// force to bind new shader
		GL_BindShader( &glsl_programs[hProgram] );
	}

	glsl_program_t *shader = RI->currentshader;

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			if( pass == BILATERAL_PASS0 )
				u->SetValue( tr.fbo_light.GetTexture().ToInt() );
			else if( pass == BILATERAL_PASS1 )
				u->SetValue( tr.fbo_filter.GetTexture().ToInt() );
			else 
				u->SetValue( tr.defscene_fbo->colortarget[0].ToInt() );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.defscene_fbo->depthtarget.ToInt() );
			break;
		case UT_ZFAR:
			u->SetValue( RI->view.farClip );
			break;
		case UT_SCREENSIZEINV:
			if( pass == BILATERAL_PASS0 )
				u->SetValue( 1.0f / (float)glState.width, 0.0f );
			else if( pass == BILATERAL_PASS1 )
				u->SetValue( 0.0f, 1.0f / (float)glState.height );
			else u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_SHADOWPARMS:
			u->SetValue( RI->view.projectionMatrix[3][2], RI->view.projectionMatrix[2][2] );
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}

	RenderFSQ( glState.width, glState.height );
}

void GL_SetupWorldLightPass( void )
{
	tr.fbo_shadow.Bind(TextureHandle::Null()); // <- render to shadow texture

	// FIXME! modelview isn't be changed anyway until current pass!!!
//	pglMatrixMode( GL_MODELVIEW );
//	pglLoadMatrixf( RI->glstate.modelviewMatrix );

	GL_DrawDeferred( tr.defLightShader, LIGHT_PASS );
	GL_CleanupAllTextureUnits();

	GL_Setup2D();
	tr.fbo_light.Bind(TextureHandle::Null());	// <- copy to fbo_light result of works complex light shader
	GL_BindShader( NULL );
	GL_Bind( GL_TEXTURE0, tr.fbo_shadow.GetTexture() );
	RenderFSQ( glState.width, glState.height );

	if( tr.bilateralShader )
	{
		// apply bilateral filter
		tr.fbo_filter.Bind(TextureHandle::Null());	// <- bilateral pass0, filtering by width, store to fbo_filter
		GL_DrawBilateralFilter( tr.bilateralShader, BILATERAL_PASS0 );

		tr.fbo_light.Bind(TextureHandle::Null());	// <- bilateral pass1, filtering by height, store back to fbo_light
		GL_DrawBilateralFilter( tr.bilateralShader, BILATERAL_PASS1 );
	}
}

void GL_SetupDynamicPass( void )
{
	if( !FBitSet( RI->view.flags, RF_HASDYNLIGHTS ))
		return;

	pglEnable( GL_SCISSOR_TEST );
	CDynLight *pl = tr.dlights;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		// deferred path are ignored sunlights
		if( pl->Expired( ) || pl->type == LIGHT_DIRECTIONAL )
			continue;

		if( !pl->Active( )) continue;

		if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
			continue;

		if( R_CullFrustum( &pl->frustum ))
			continue;

		float y2 = (float)RI->view.port[3] - pl->h - pl->y;
		pglScissor( pl->x, y2, pl->w, pl->h );
		RI->currentlight = pl;

		GL_DrawDeferred( tr.defDynLightShader[pl->type], SCENE_PASS );
	}

	pglDisable( GL_SCISSOR_TEST );
	RI->currentlight = NULL;
}

void GL_SetupWorldScenePass( void )
{
	mworldlight_t	*wl;
	int		i;

	// debug
	for( i = 0, wl = world->worldlights; i < world->numworldlights; i++, wl++ )
	{
		if( wl->emittype == emit_ignored )
			continue;

		if( !CHECKVISBIT( RI->view.vislight, i ))
			continue;
		r_stats.c_worldlights++;
	}

	int type = CVAR_TO_BOOL( cv_deferred_full ) ? 1 : 0;
	GL_DrawDeferred( tr.defSceneShader[type], SCENE_PASS );

	// also render a standard dynamic lights
	GL_SetupDynamicPass();
}

void GL_DrawDeferredPass( void )
{
	if( !tr.defSceneShader[0] || !tr.defSceneShader[1] || !tr.defLightShader )
		return; // oops!

	GL_DEBUG_SCOPE();
	GL_ComputeScreenRays();

	if( FBitSet( RI->params, RP_DEFERREDSCENE ))
		GL_SetupWorldScenePass();

	if( FBitSet( RI->params, RP_DEFERREDLIGHT ))
		GL_SetupWorldLightPass();

	GL_CleanupDrawState();
	GL_BindFBO( FBO_MAIN );
	GL_Setup3D();
}
