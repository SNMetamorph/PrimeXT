/*
gl_primitive.cpp - rendering primitives
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
#include <stringlib.h>
#include "utils.h"
#include "pm_defs.h"
#include "event_api.h"
#include "gl_local.h"
#include "gl_studio.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"

void CSolidEntry :: SetRenderPrimitive( const Vector verts[4], const Vector4D &color, int texture, int rendermode )
{
	m_bDrawType = DRAWTYPE_QUAD;
	m_iStartVertex = RI->frame.primverts.Count();

	for( int i = 0; i < 4; i++ )
		RI->frame.primverts.AddToTail( verts[i] );
	m_iColor = PackRGBA( color.x * 255, color.y * 255, color.z * 255, color.w * 255 );
	m_iRenderMode = rendermode;
	m_hTexture = texture;
}

void CSolidEntry :: SetRenderSurface( msurface_t *surface, word hProgram )
{
	m_bDrawType = DRAWTYPE_SURFACE;
	m_pSurf = surface;
	m_pParentEntity = RI->currententity;
	m_pRenderModel = RI->currentmodel;
	m_hProgram = hProgram;
}

void CSolidEntry :: SetRenderMesh( vbomesh_t *mesh, word hProgram )
{
	m_bDrawType = DRAWTYPE_MESH;
	m_pMesh = mesh;
	m_pParentEntity = RI->currententity;
	m_pRenderModel = RI->currentmodel;
	m_hProgram = hProgram;
}

void CTransEntry :: ComputeViewDistance( const Vector &absmin, const Vector &absmax )
{
#if 1
	m_flViewDist = CalcSqrDistanceToAABB( absmin, absmax, GetVieworg( ));
#else
	Vector origin = (absmin + absmax) * 0.5f; 
	m_flViewDist = DotProduct( origin, GetVForward() ) - RI->view.planedist;
//	m_flViewDist = VectorDistance2( origin, GetVieworg( ));
#endif
}

void CTransEntry :: ComputeScissor( const Vector &absmin, const Vector &absmax )
{
	ComputeViewDistance( absmin, absmax );

	if( R_ScissorForAABB( absmin, absmax, &m_vecRect.x, &m_vecRect.y, &m_vecRect.z, &m_vecRect.w ))
		m_bScissorReady = true;
	else m_bScissorReady = false;
}

void CTransEntry::RequestScreencopy(bool copyColor, bool copyDepth)
{
	if (!m_bScissorReady) {
		return;
	}

	ASSERT(copyColor || copyDepth);
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);
	float y2 = (float)RI->view.port[3] - m_vecRect.w - m_vecRect.y;

	if (hdr_rendering)
	{
		GLenum filtering = 0;
		GLbitfield bufferMask = 0;
		if (copyColor && copyDepth) {
			bufferMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
			filtering = GL_NEAREST;
		}
		else if (copyColor) {
			bufferMask = GL_COLOR_BUFFER_BIT;
			filtering = GL_LINEAR; // so, why?
		}
		else if (copyDepth) {
			bufferMask = GL_DEPTH_BUFFER_BIT;
			filtering = GL_NEAREST;
		}

		pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, tr.screencopy_fbo->id);
		pglBlitFramebuffer(
			m_vecRect.x, 
			y2, 
			m_vecRect.x + m_vecRect.z, 
			y2 + m_vecRect.w, m_vecRect.x, 
			y2, 
			m_vecRect.x + m_vecRect.z, 
			y2 + m_vecRect.w, 
			bufferMask, 
			filtering
		);
		pglBindFramebuffer(GL_FRAMEBUFFER_EXT, glState.frameBuffer);
	}
	else
	{
		if (copyColor) 
		{
			GL_Bind(GL_TEXTURE0, tr.screen_color);
			pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, m_vecRect.x, y2, m_vecRect.x, y2, m_vecRect.z, m_vecRect.w);
		}
		if (copyDepth)
		{
			GL_Bind(GL_TEXTURE0, tr.screen_depth);
			pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, m_vecRect.x, y2, m_vecRect.x, y2, m_vecRect.z, m_vecRect.w);
		}
	}
}

void CTransEntry :: RenderScissorDebug( void )
{
	if( !m_bScissorReady ) return;

	R_DrawScissorRectangle( m_vecRect.x, m_vecRect.y, m_vecRect.z, m_vecRect.w );
}