/*
gl_framebuffer.cpp - framebuffer implementation class
Copyright (C) 2014 Uncle Mike

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
#include <mathlib.h>
#include <stringlib.h>

static gl_drawbuffer_t	gl_drawbuffers[MAX_FRAMEBUFFERS];
static int		gl_num_drawbuffers;

/*
==================
GL_AllocDrawbuffer
==================
*/
gl_drawbuffer_t *GL_AllocDrawbuffer( const char *name, int width, int height, int depth )
{
	gl_drawbuffer_t	*fbo;
	int		i;

	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return NULL;

	// find a free FBO slot
	for( i = 0, fbo = gl_drawbuffers; i < gl_num_drawbuffers; i++, fbo++ )
		if( !fbo->name[0] ) break;

	if( i == gl_num_drawbuffers )
	{
		if( gl_num_drawbuffers == MAX_FRAMEBUFFERS )
			HOST_ERROR( "GL_AllocDrawBuffer: MAX_FRAMEBUFFERS limit exceeds\n" );
		gl_num_drawbuffers++;
	}

	if( !GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		width = NearestPOW( width, true );
		height = NearestPOW( height, true );
	}

	// fill it in
	Q_strncpy( fbo->name, name, sizeof( fbo->name ));

	// make sure it's fit in limits
	fbo->width = Q_min( glConfig.max_2d_texture_size, width );
	fbo->height = Q_min( glConfig.max_2d_texture_size, height );
	fbo->depth = depth;

	// generate the drawbuffer
	pglGenFramebuffers( 1, &fbo->id );

	return fbo;
}

/*
==================
GL_ResizeDrawbuffer
==================
*/
void GL_ResizeDrawbuffer( gl_drawbuffer_t *fbo, int width, int height, int depth )
{
	ASSERT( fbo != NULL );

	// fill it in
	fbo->width = width;
	fbo->height = height;
	fbo->depth = depth;
}

/*
==================
GL_AttachColorTextureToFBO
==================
*/
void GL_AttachColorTextureToFBO( gl_drawbuffer_t *fbo, int texture, int colorIndex, int index )
{
	GLuint	target = RENDER_GET_PARM( PARM_TEX_TARGET, texture );
	GLuint	format = RENDER_GET_PARM( PARM_TEX_GLFORMAT, texture );
	GLint	width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
	GLint	height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );
	GLint	depth = RENDER_GET_PARM( PARM_TEX_DEPTH, texture );

	if( target == GL_TEXTURE_2D )
	{
		// set the texture
		fbo->colortarget[colorIndex] = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_2d_texture_size || fbo->height > glConfig.max_2d_texture_size )
				HOST_ERROR( "GL_AttachColorTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i)\n",
				fbo->width, glConfig.max_2d_texture_size, fbo->height, glConfig.max_2d_texture_size );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );

			pglTexImage2D( target, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		}

		// Bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// Set up the color attachment
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + colorIndex, GL_TEXTURE_2D, texture, 0 );
	}
	else if( target == GL_TEXTURE_CUBE_MAP_ARB )
	{
		// set the texture
		fbo->colortarget[colorIndex] = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_cubemap_size || fbo->height > glConfig.max_cubemap_size )
				HOST_ERROR( "GL_AttachColorTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i)\n",
				fbo->width, glConfig.max_cubemap_size, fbo->height, glConfig.max_cubemap_size );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );

			pglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 0, format, fbo->width, fbo->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		}

		// bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// set up the color attachment
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + colorIndex, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + index, texture, 0 );
	}
	else if( target == GL_TEXTURE_2D_ARRAY_EXT )
	{
		// Set the texture
		fbo->colortarget[colorIndex] = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height || depth != fbo->depth )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_2d_texture_size || fbo->height > glConfig.max_2d_texture_size || fbo->depth > glConfig.max_2d_texture_layers )
				HOST_ERROR( "GL_AttachColorTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i or %i > %i)\n",
				fbo->width, glConfig.max_2d_texture_size, fbo->height, glConfig.max_2d_texture_size, fbo->depth, glConfig.max_2d_texture_layers );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );
			fbo->depth = RENDER_GET_PARM( PARM_TEX_DEPTH, texture );

			pglTexImage3D( target, 0, format, fbo->width, fbo->height, fbo->depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		}

		// bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// Set up the color attachment
		pglFramebufferTextureLayer( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + colorIndex, texture, 0, index );
	}
	else if( target == GL_TEXTURE_3D )
	{
		// Set the texture
		fbo->colortarget[colorIndex] = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height || depth != fbo->depth )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_3d_texture_size || fbo->height > glConfig.max_3d_texture_size || fbo->depth > glConfig.max_3d_texture_size )
				HOST_ERROR( "GL_AttachColorTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i or %i > %i)\n",
				fbo->width, glConfig.max_3d_texture_size, fbo->height, glConfig.max_3d_texture_size, fbo->depth, glConfig.max_3d_texture_size );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );
			fbo->depth = RENDER_GET_PARM( PARM_TEX_DEPTH, texture );

			pglTexImage3D( target, 0, format, fbo->width, fbo->height, fbo->depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		}

		// bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// Set up the color attachment
		pglFramebufferTexture3D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + colorIndex, GL_TEXTURE_3D, texture, 0, index );
	}
	else
	{
		HOST_ERROR( "GL_AttachColorTextureToFBO: bad texture target (%i)\n", target );
	}
}

/*
==================
GL_AttachDepthTextureToFBO
==================
*/
void GL_AttachDepthTextureToFBO( gl_drawbuffer_t *fbo, int texture, int index )
{
	GLuint	target = RENDER_GET_PARM( PARM_TEX_TARGET, texture );
	GLuint	format = RENDER_GET_PARM( PARM_TEX_GLFORMAT, texture );
	GLint	width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
	GLint	height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );
	GLint	depth = RENDER_GET_PARM( PARM_TEX_DEPTH, texture );

	if( target == GL_TEXTURE_2D )
	{
		// set the texture
		fbo->depthtarget = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_2d_texture_size || fbo->height > glConfig.max_2d_texture_size )
				HOST_ERROR( "GL_AttachDepthTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i)\n",
				fbo->width, glConfig.max_2d_texture_size, fbo->height, glConfig.max_2d_texture_size );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );

			pglTexImage2D( target, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
		}

		// Bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// Set up the color attachment
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texture, 0 );
	}
	else if( target == GL_TEXTURE_CUBE_MAP_ARB )
	{
		// set the texture
		fbo->depthtarget = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_cubemap_size || fbo->height > glConfig.max_cubemap_size )
				HOST_ERROR( "GL_AttachDepthTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i)\n",
				fbo->width, glConfig.max_cubemap_size, fbo->height, glConfig.max_cubemap_size );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );

			pglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
			pglTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 0, format, fbo->width, fbo->height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
		}

		// bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// set up the color attachment
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + index, texture, 0 );
	}
	else if( target == GL_TEXTURE_2D_ARRAY_EXT )
	{
		// Set the texture
		fbo->depthtarget = texture;

		// if the drawbuffer has been resized
		if( width != fbo->width || height != fbo->height || depth != fbo->depth )
		{
			// check the dimensions
			if( fbo->width > glConfig.max_2d_texture_size || fbo->height > glConfig.max_2d_texture_size || fbo->depth > glConfig.max_2d_texture_layers )
				HOST_ERROR( "GL_AttachDepthTextureToFBO: size exceeds hardware limits (%i > %i or %i > %i or %i > %i)\n",
				fbo->width, glConfig.max_2d_texture_size, fbo->height, glConfig.max_2d_texture_size, fbo->depth, glConfig.max_2d_texture_layers );

			// reallocate the texture
			GL_UpdateTexSize( texture, fbo->width, fbo->height, fbo->depth );
			GL_Bind( GL_KEEP_UNIT, texture );

			// need to refresh real FBO size with possible hardware limitations
			fbo->width = RENDER_GET_PARM( PARM_TEX_WIDTH, texture );
			fbo->height = RENDER_GET_PARM( PARM_TEX_HEIGHT, texture );
			fbo->depth = RENDER_GET_PARM( PARM_TEX_DEPTH, texture );

			pglTexImage3D( target, 0, format, fbo->width, fbo->height, fbo->depth, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL );
		}

		// bind the drawbuffer
		GL_BindDrawbuffer( fbo );

		// Set up the color attachment
		pglFramebufferTextureLayer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, texture, 0, index );
	}
	else
	{
		HOST_ERROR( "GL_AttachDepthTextureToFBO: bad texture target (%i)\n", target );
	}
}

/*
==================
GL_CheckFBOStatus
==================
*/
void GL_CheckFBOStatus( gl_drawbuffer_t *fbo )
{
	const char	*string;
	int		status;

	// bind the drawbuffer
	GL_BindDrawbuffer( fbo );

	// check the framebuffer status
	status = pglCheckFramebufferStatus( GL_FRAMEBUFFER_EXT );

	if( status == GL_FRAMEBUFFER_COMPLETE_EXT )
		return;

	switch( status )
	{
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
		string = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
		string = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
		string = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
		string = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
		string = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		string = "GL_FRAMEBUFFER_UNSUPPORTED";
		break;
	case GL_FRAMEBUFFER_UNDEFINED_EXT:
		string = "GL_FRAMEBUFFER_UNDEFINED";
		break;
	default:
		string = "UNKNOWN STATUS";
		break;
	}

	HOST_ERROR( "GL_CheckFBOStatus: %s for '%s'\n", string, fbo->name );
}

/*
==================
GL_FreeDrawbuffers
==================
*/
void GL_FreeDrawbuffers( void )
{
	gl_drawbuffer_t	*fbo;

	// unbind the drawbuffer
	GL_BindDrawbuffer( NULL );

	// delete all the drawbuffers
	for( int i = 0; i < gl_num_drawbuffers; i++ )
	{
		fbo = &gl_drawbuffers[i];
		pglDeleteFramebuffers( 1, &fbo->id );
	}

	// NOTE: let the engine release attached textures
	memset( gl_drawbuffers, 0, sizeof( gl_drawbuffers ));
	gl_num_drawbuffers = 0;
}

CFrameBuffer :: CFrameBuffer( void )
{
	m_iFrameWidth = m_iFrameHeight = 0;
	m_iFrameBuffer = m_iDepthBuffer = 0;
	m_iTexture = m_iAttachment = 0;

	m_bAllowFBO = false;
}

CFrameBuffer :: ~CFrameBuffer( void )
{
	// NOTE: static case will be failed
	Free();
}

int CFrameBuffer :: m_iBufferNum = 0;
	
bool CFrameBuffer :: Init( FBO_TYPE type, GLuint width, GLuint height, GLuint flags )
{
	Free(); // release old buffer

	m_iFlags = flags;

	if( !GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
		SetBits( m_iFlags, FBO_MAKEPOW );

	if( FBitSet( m_iFlags, FBO_MAKEPOW ))
	{
		width = NearestPOW( width, true );
		height = NearestPOW( height, true );
	}

	// clamp size to hardware limits
	if( type == FBO_CUBE )
	{
		m_iFrameWidth = bound( 0, width, glConfig.max_cubemap_size );
		m_iFrameHeight = bound( 0, height, glConfig.max_cubemap_size );
	}
	else
	{
		m_iFrameWidth = bound( 0, width, glConfig.max_2d_texture_size );
		m_iFrameHeight = bound( 0, height, glConfig.max_2d_texture_size );
	}

	if( !m_iFrameWidth || !m_iFrameHeight )
	{
		ALERT( at_error, "CFrameBuffer( %i x %i ) invalid size\n", m_iFrameWidth, m_iFrameHeight );
		return false;
	}

	// create FBO texture
	if( !FBitSet( m_iFlags, FBO_NOTEXTURE ))
	{
		int texFlags = (TF_NOMIPMAP|TF_CLAMP);

		if( type == FBO_CUBE )
			SetBits( texFlags, TF_CUBEMAP );
		else if( type == FBO_DEPTH ) 
			SetBits( texFlags, TF_DEPTHMAP );

		if( !FBitSet( m_iFlags, FBO_LINEAR ))
			SetBits( texFlags, TF_NEAREST );

		if( FBitSet( m_iFlags, FBO_FLOAT ))
			SetBits( texFlags, TF_ARB_FLOAT );

		if( FBitSet( m_iFlags, FBO_RECTANGLE ))
			SetBits( texFlags, TF_RECTANGLE );

		if( FBitSet( m_iFlags, FBO_LUMINANCE ))
			SetBits( texFlags, TF_LUMINANCE );
		else if( type == FBO_COLOR )
			SetBits( texFlags, TF_HAS_ALPHA );

		m_iTexture = CREATE_TEXTURE( va( "*framebuffer#%i", m_iBufferNum++ ), m_iFrameWidth, m_iFrameHeight, NULL, texFlags ); 
	}

	m_bAllowFBO = (GL_Support( R_FRAMEBUFFER_OBJECT )) ? true : false;

	if( m_bAllowFBO )
	{
		// frame buffer
		pglGenFramebuffers( 1, &m_iFrameBuffer );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, m_iFrameBuffer );

		// depth buffer
		pglGenRenderbuffers( 1, &m_iDepthBuffer );
		pglBindRenderbuffer( GL_RENDERBUFFER_EXT, m_iDepthBuffer );
		pglRenderbufferStorage( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, m_iFrameWidth, m_iFrameHeight );

		// attach depthbuffer to framebuffer
		pglFramebufferRenderbuffer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_iDepthBuffer );

		//  attach the framebuffer to our texture, which may be a depth texture
		if( type == FBO_DEPTH )
		{
			m_iAttachment = GL_DEPTH_ATTACHMENT_EXT;
			pglDrawBuffer( GL_NONE );
			pglReadBuffer( GL_NONE );
		}
		else
		{
			m_iAttachment = GL_COLOR_ATTACHMENT0_EXT;
			pglDrawBuffer( m_iAttachment );
			pglReadBuffer( m_iAttachment );
		}

		if( m_iTexture != 0 )
		{
			GLuint target = RENDER_GET_PARM( PARM_TEX_TARGET, m_iTexture );
			GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, m_iTexture );

			if( target == GL_TEXTURE_CUBE_MAP_ARB )
			{
				target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB;
				for( int i = 0; i < 6; i++ )
					pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, m_iAttachment, target + i, texnum, 0 );
			}
			else pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, m_iAttachment, target, texnum, 0 );
		}

		m_bAllowFBO = ValidateFBO();
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	}

	return true;
}

void CFrameBuffer :: Free( void )
{
	if( !m_bAllowFBO ) return;

	if( !FBitSet( m_iFlags, FBO_NOTEXTURE ) && m_iTexture != 0 )
		FREE_TEXTURE( m_iTexture );

	pglDeleteRenderbuffers( 1, &m_iDepthBuffer );
	pglDeleteFramebuffers( 1, &m_iFrameBuffer );

	m_iFrameWidth = m_iFrameHeight = 0;
	m_iFrameBuffer = m_iDepthBuffer = 0;
	m_iTexture = m_iAttachment = 0;

	m_bAllowFBO = false;
}

bool CFrameBuffer :: ValidateFBO( void )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return false;

	// check FBO status
	GLenum status = pglCheckFramebufferStatus( GL_FRAMEBUFFER_EXT );

	switch( status )
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
		return true;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
		ALERT( at_error, "CFrameBuffer: attachment is NOT complete\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
		ALERT( at_error, "CFrameBuffer: no image is attached to FBO\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
		ALERT( at_error, "CFrameBuffer: attached images have different dimensions\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
		ALERT( at_error, "CFrameBuffer: color attached images have different internal formats\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
		ALERT( at_error, "CFrameBuffer: draw buffer incomplete\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
		ALERT( at_error, "CFrameBuffere: read buffer incomplete\n" );
		return false;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		ALERT( at_error, "CFrameBuffer: unsupported by current FBO implementation\n" );
		return false;
	default:
		ALERT( at_error, "CFrameBuffer: unknown error\n" );
		return false;
	}
}

void CFrameBuffer :: Bind( GLuint texture, GLuint side )
{
	if( !m_bAllowFBO ) return;

	if( glState.frameBuffer != m_iFrameBuffer )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, m_iFrameBuffer );
		glState.frameBuffer = m_iFrameBuffer;
	}

	// change texture if needs
	if( FBitSet( m_iFlags, FBO_NOTEXTURE ) && texture != 0 )
	{
		m_iTexture = texture;

		GLuint target = RENDER_GET_PARM( PARM_TEX_TARGET, m_iTexture );
		GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, m_iTexture );

		if( target == GL_TEXTURE_CUBE_MAP_ARB )
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side;

		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, m_iAttachment, target, texnum, 0 );
	}	

	ValidateFBO();
}