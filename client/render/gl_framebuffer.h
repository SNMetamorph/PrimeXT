/*
gl_framebuffer.h - framebuffer implementation class
this code written for Paranoia 2: Savior modification
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

#ifndef GL_FRAMEBUFFER_H
#define GL_FRAMEBUFFER_H
#include "gl_export.h"

typedef enum
{
	FBO_COLOR = 0,	// only color texture is used
	FBO_DEPTH,		// only depth texture is used
	FBO_CUBE,		// only color texture as cubemap is side
} FramebufferType;

typedef enum
{
	FBO_MAKEPOW = (1 << 0),		// round buffer size to nearest pow
	FBO_NOTEXTURE = (1 << 1),   // don't create texture on initialization
	FBO_FLOAT = (1 << 2),		// use float texture
	FBO_RECTANGLE = (1 << 3),   // use rectangle texture
	FBO_LINEAR = (1 << 4),		// use linear filtering
	FBO_LUMINANCE = (1 << 5)	// force to luminance texture
} FramebufferFlags;

class CFrameBuffer
{
public:
	CFrameBuffer();
	~CFrameBuffer();

	bool Init(FramebufferType type, GLuint width, GLuint height, GLuint flags = 0);
	void Bind(GLuint texture = 0, GLuint side = 0, GLuint mipLevel = 0);
	bool ValidateFBO();
	void Free();

	unsigned int GetWidth() const	{ return m_iFrameWidth; }
	unsigned int GetHeight() const	{ return m_iFrameHeight; }
	int GetTexture() const			{ return m_iTexture; }
	bool Active() const				{ return m_bAllowFBO; }

private:
	GLuint		m_iFrameWidth;
	GLuint		m_iFrameHeight;
	GLint		m_iTexture;

	GLuint		m_iFrameBuffer;
	GLuint		m_iDepthBuffer;
	GLenum		m_iAttachment;	// attachment type
	bool		m_bAllowFBO;	// FBO is valid
	GLuint		m_iFlags;		// member FBO flags
	static int	m_iBufferNum;	// single object for all instances
};

#endif//GL_FRAMEBUFFER_H