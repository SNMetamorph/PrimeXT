//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_sprite.h"
#include "mathlib.h"
#include "r_shader.h"

/*
=============
R_GetSpriteTexture

=============
*/
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if( !m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data )
		return 0;

	return R_GetSpriteFrame( m_pSpriteModel, frame )->gl_texturenum;
}

/*
==============
GL_DepthMask
==============
*/
void GL_DepthMask( GLint enable )
{
	if( glState.depthmask == enable )
		return;

	glState.depthmask = enable;
	pglDepthMask( enable );
}

/*
==============
GL_AlphaTest
==============
*/
void GL_AlphaTest( GLint enable )
{
	if( pglIsEnabled( GL_ALPHA_TEST ) == enable )
		return;

	if( enable ) pglEnable( GL_ALPHA_TEST );
	else pglDisable( GL_ALPHA_TEST );
#if 0
	if( enable ) pglEnable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
	else pglDisable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
#endif
}

/*
==============
GL_Blend
==============
*/
void GL_Blend( GLint enable )
{
	if( pglIsEnabled( GL_BLEND ) == enable )
		return;

	if( enable ) pglEnable( GL_BLEND );
	else pglDisable( GL_BLEND );
}

/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( GLenum front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
=================
GL_Setup2D
=================
*/
void GL_Setup2D( void )
{
	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglDisable( GL_DEPTH_TEST );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_Blend( GL_FALSE );

	GL_Cull( GL_FRONT );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );
}

/*
=================
GL_Setup3D
=================
*/
void GL_Setup3D( void )
{
	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->worldviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	GL_DepthMask( GL_TRUE );
	GL_Cull( GL_FRONT );
}

/*
=================
GL_LoadTexMatrix
=================
*/
void GL_LoadTexMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	GL_LoadTextureMatrix( dest );
}

/*
=================
GL_LoadMatrix
=================
*/
void GL_LoadMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	pglLoadMatrixf( dest );
}

/*
================
R_BeginDrawProjectionGLSL

Setup texture matrix for light texture
================
*/
bool R_BeginDrawProjectionGLSL( plight_t *pl, float lightscale )
{
	word	shaderNum = GL_UberShaderForDlightGeneric( pl );
	GLfloat	gl_lightViewProjMatrix[16];

	if( !shaderNum || tr.nodlights )
		return false;

	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	pglBlendFunc( GL_ONE, GL_ONE );
	pglEnable( GL_SCISSOR_TEST );

	if( glState.drawTrans )
		pglDepthFunc( GL_LEQUAL );
	else pglDepthFunc( GL_LEQUAL );
	RI->currentlight = pl;

	float y2 = (float)RI->viewport[3] - pl->h - pl->y;
	pglScissor( pl->x, y2, pl->w, pl->h );

	GL_BindShader( &glsl_programs[shaderNum] );			
	ASSERT( RI->currentshader != NULL );
	R_LoadIdentity();

	Vector lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
	pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );

	// write constants
	pglUniformMatrix4fvARB( RI->currentshader->u_LightViewProjectionMatrix, 1, GL_FALSE, &gl_lightViewProjMatrix[0] );
	float shadowWidth = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, pl->shadowTexture );
	float shadowHeight = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, pl->shadowTexture );

	// depth scale and bias and shadowmap resolution
	pglUniform4fARB( RI->currentshader->u_LightDir, lightdir.x, lightdir.y, lightdir.z, pl->fov );
	pglUniform4fARB( RI->currentshader->u_LightDiffuse, pl->color.r / 255.0f, pl->color.g / 255.0f, pl->color.b / 255.0f, pl->lightFalloff );
	pglUniform4fARB( RI->currentshader->u_ShadowParams, shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
	pglUniform4fARB( RI->currentshader->u_LightOrigin, pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius ));
	pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
	pglUniform1fARB( RI->currentshader->u_LightScale, lightscale );

	GL_Bind( GL_TEXTURE1, pl->projectionTexture );
	GL_Bind( GL_TEXTURE2, pl->shadowTexture );

	return true;
}

/*
================
R_EndDrawProjection

Restore identity texmatrix
================
*/
void R_EndDrawProjectionGLSL( void )
{
	GL_BindShader( NULL );

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	pglDisable( GL_SCISSOR_TEST );
	pglDepthFunc( GL_LEQUAL );
	RI->currentlight = NULL;
	GL_Blend( GL_FALSE );
}

int R_AllocFrameBuffer( int viewport[4] )
{
	int i = tr.num_framebuffers;

	if( i >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_AllocateFrameBuffer: FBO limit exceeded!\n" );
		return -1; // disable
	}

	gl_fbo_t *fbo = &tr.frame_buffers[i];
	tr.num_framebuffers++;

	if( fbo->init )
	{
		ALERT( at_warning, "R_AllocFrameBuffer: buffer %i already initialized\n", i );
		return i;
	}

	// create a depth-buffer
	pglGenRenderbuffers( 1, &fbo->renderbuffer );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglRenderbufferStorage( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, viewport[2], viewport[3] );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, 0 );

	// create frame-buffer
	pglGenFramebuffers( 1, &fbo->framebuffer );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
	pglFramebufferRenderbuffer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	fbo->init = true;

	return i;
}

void R_FreeFrameBuffer( int buffer )
{
	if( buffer < 0 || buffer >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_FreeFrameBuffer: invalid buffer enum %i\n", buffer );
		return;
	}

	gl_fbo_t *fbo = &tr.frame_buffers[buffer];

	pglDeleteRenderbuffers( 1, &fbo->renderbuffer );
	pglDeleteFramebuffers( 1, &fbo->framebuffer );
	memset( fbo, 0, sizeof( *fbo ));
}

void GL_BindFrameBuffer( int buffer, int texture )
{
	gl_fbo_t *fbo = NULL;

	if( buffer >= 0 && buffer < MAX_FRAMEBUFFERS )
		fbo = &tr.frame_buffers[buffer];

	if( !fbo )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = buffer;
		return;
	}

	// at this point FBO index is always positive
	if((unsigned int)glState.frameBuffer != fbo->framebuffer )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
		glState.frameBuffer = fbo->framebuffer;
	}		

	if( fbo->texture != texture )
	{
		// change texture attachment
		GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, texture );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texnum, 0 );
		fbo->texture = texture;
	}
}

/*
==============
GL_BindFBO
==============
*/
void GL_BindFBO( GLint buffer )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return;

	if( glState.frameBuffer == buffer )
		return;

	if( buffer < 0 )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = buffer;
		return;
	}

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, buffer );
	glState.frameBuffer = buffer;
}
