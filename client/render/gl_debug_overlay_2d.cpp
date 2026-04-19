/*
gl_debug_overlay_2d.cpp - 2D screen-space debug overlay rendering
Copyright (C) 2025

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "gl_debug_overlay_2d.h"
#include "gl_local.h"

// Constants missing from gl_export.h but standard since GL 3.0.
#define GL_DEBUG_FRAMEBUFFER         0x8CA9
#define GL_DEBUG_FRAMEBUFFER_BINDING 0x8CA6

namespace
{
	// Save and restore the draw-framebuffer around a scope. Debug overlays
	// must land on the final screen buffer; intermediate HDR / post-process
	// FBOs are often bound when our helpers are invoked.
	struct ScopedScreenFBO
	{
		GLint saved = 0;
		ScopedScreenFBO() {
			pglGetIntegerv(GL_DEBUG_FRAMEBUFFER_BINDING, &saved);
			pglBindFramebuffer(GL_DEBUG_FRAMEBUFFER, 0);
		}
		~ScopedScreenFBO() {
			pglBindFramebuffer(GL_DEBUG_FRAMEBUFFER, saved);
		}
	};
}

CDebugOverlay2D& CDebugOverlay2D::GetInstance()
{
	static CDebugOverlay2D instance;
	return instance;
}

void CDebugOverlay2D::Initialize()
{
	if (m_bInitialized)
		return;

	word shaderHandle = GL_FindShader("common/debug_overlay2d", "common/debug_overlay2d", "common/debug_overlay2d");
	m_Shader.SetShader(shaderHandle);
	pglGenVertexArrays(1, &m_iVAO);
	pglGenBuffersARB(1, &m_iVBO);

	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);
	// 8 vec3 positions max — enough for a wire-rect (4 lines × 2 verts) or
	// a filled rect (2 tris × 3 verts).
	pglBufferDataARB(GL_ARRAY_BUFFER_ARB, 8 * sizeof(Vector), nullptr, GL_DYNAMIC_DRAW_ARB);

	pglBindVertexArray(m_iVAO);
	pglEnableVertexAttribArrayARB(ATTR_INDEX_POSITION);
	pglVertexAttribPointerARB(ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vector), (void *)0);
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	pglBindVertexArray(0);

	m_bInitialized = true;
}

void CDebugOverlay2D::Shutdown()
{
	if (!m_bInitialized)
		return;
	if (m_iVBO) {
		pglDeleteBuffersARB(1, &m_iVBO);
		m_iVBO = 0;
	}
	if (m_iVAO) {
		pglDeleteVertexArrays(1, &m_iVAO);
		m_iVAO = 0;
	}
	m_Shader.Invalidate();
	m_bInitialized = false;
}

void CDebugOverlay2D::UploadMatrixAndSize(struct glsl_prog_s *shader)
{
	const GLint sizeLoc = pglGetUniformLocation(shader->handle, "u_ScreenSize");
	if (sizeLoc >= 0) {
		const GLfloat screen[2] = { (float)glState.width, (float)glState.height };
		pglUniform2fvARB(sizeLoc, 1, screen);
	}
}

void CDebugOverlay2D::DrawWireRect(float x, float y, float w, float h, Vector color)
{
	if (!m_bInitialized)
		return;

	glsl_program_t *shader = m_Shader.GetShader();
	if (!shader)
		return;

	Vector verts[8] = {
		Vector(x,   y,   0), Vector(x+w, y,   0),
		Vector(x,   y,   0), Vector(x,   y+h, 0),
		Vector(x+w, y,   0), Vector(x+w, y+h, 0),
		Vector(x,   y+h, 0), Vector(x+w, y+h, 0),
	};

	GL_BindShader(shader);
	UploadMatrixAndSize(shader);
	const GLint colorLoc = pglGetUniformLocation(shader->handle, "u_Color");
	const GLint useTexLoc = pglGetUniformLocation(shader->handle, "u_UseTexture");
	if (colorLoc >= 0) {
		const GLfloat rgba[4] = { color.x, color.y, color.z, 1.0f };
		pglUniform4fvARB(colorLoc, 1, rgba);
	}
	if (useTexLoc >= 0) pglUniform1fARB(useTexLoc, 0.0f);

	// Draw into the currently-bound FBO (HDR intermediate during the debug-draw
	// phase). Forcing the default screen framebuffer here would land the output
	// on the pre-composite buffer that R_RenderScreenQuad later overwrites.
	pglBindVertexArray(m_iVAO);
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);
	pglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(verts), verts);
	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);
	pglLineWidth(4.0f);
	pglDrawArrays(GL_LINES, 0, 8);
	pglLineWidth(1.0f);
}

void CDebugOverlay2D::DrawTexturedRect(float x, float y, float w, float h, TextureHandle texture)
{
	if (!m_bInitialized)
		return;

	glsl_program_t *shader = m_Shader.GetShader();
	if (!shader)
		return;

	Vector verts[6] = {
		Vector(x,   y,   0),
		Vector(x+w, y,   0),
		Vector(x+w, y+h, 0),
		Vector(x,   y,   0),
		Vector(x+w, y+h, 0),
		Vector(x,   y+h, 0),
	};

	GL_BindShader(shader);
	UploadMatrixAndSize(shader);
	const GLint colorLoc = pglGetUniformLocation(shader->handle, "u_Color");
	if (colorLoc >= 0) {
		const GLfloat rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		pglUniform4fvARB(colorLoc, 1, rgba);
	}
	const GLint useTexLoc = pglGetUniformLocation(shader->handle, "u_UseTexture");
	if (useTexLoc >= 0) pglUniform1fARB(useTexLoc, 1.0f);
	const GLint samplerLoc = pglGetUniformLocation(shader->handle, "u_ColorMap");
	if (samplerLoc >= 0) pglUniform1iARB(samplerLoc, 0);

	GL_Bind(GL_TEXTURE0, texture);

	ScopedScreenFBO fbo;
	pglBindVertexArray(m_iVAO);
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);
	pglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(verts), verts);
	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);
	pglDrawArrays(GL_TRIANGLES, 0, 6);
}
