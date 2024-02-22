/*
gl_primitive.h - render primitives
this code written for Paranoia 2: Savior modification
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_PRIMITIVE_H
#define GL_PRIMITIVE_H
#include "texture_handle.h"
#include <stdint.h>

#define DRAWTYPE_UNKNOWN	0
#define DRAWTYPE_SURFACE	1
#define DRAWTYPE_MESH	2
#define DRAWTYPE_QUAD	3

//#pragma pack(1)

class CSolidEntry
{
public:
	void SetRenderPrimitive( const Vector verts[4], const Vector4D &color, TextureHandle texture, int rendermode );
	void SetRenderSurface( msurface_t *surface, word hProgram );
	void SetRenderMesh( struct vbomesh_t *mesh, word hProgram );
	virtual bool IsTranslucent( void ) { return false; }
	int GetType( void ) { return m_bDrawType; }

	byte			m_bDrawType;	// type of entry

	union
	{
		cl_entity_t	*m_pParentEntity;	// pointer to parent entity
		int		m_iRenderMode;	// rendermode for primitive
	};

	union
	{
		model_t		*m_pRenderModel;	// render model
		int		m_iStartVertex;	// offset in global heap
	};

	uint32_t m_hProgram;			// handle to glsl program (may be 0)
	TextureHandle	m_hTexture;	// texture for primitive (OpenGL texture handle)

	union
	{
		struct vbomesh_t	*m_pMesh;		// NULL or mesh
		msurface_t	*m_pSurf;		// NULL or surface
		int		m_iColor;		// primitive color
	};
};

class CTransEntry : public CSolidEntry
{
public:
	void ComputeViewDistance( const Vector &absmin, const Vector &absmax );
	void ComputeScissor( const Vector &absmin, const Vector &absmax );
	virtual bool IsTranslucent( void ) { return true; }
	void RequestScreencopy( bool copyColor = true, bool copyDepth = true );
	void RenderScissorDebug( void );

	float		m_flViewDist;
	Vector4D		m_vecRect;
	bool		m_bScissorReady : 1;
};
//#pragma pack()

#endif//GL_PRIMITIVE_H