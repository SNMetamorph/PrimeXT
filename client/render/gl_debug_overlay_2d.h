/*
gl_debug_overlay_2d.h - 2D screen-space debug overlay rendering
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

#pragma once
#include "gl_export.h"
#include "shader.h"
#include "texture_handle.h"
#include "vector.h"

// Small modern-GL helper for 2D debug overlays (screen-space wireframe rectangles
// and textured quads). Used by debug features that previously emitted
// immediate-mode pglBegin calls (scissor-rectangle debug, lightmap inspector).
class CDebugOverlay2D
{
public:
	static CDebugOverlay2D& GetInstance();

	void Initialize();
	void Shutdown();

	// Wireframe rectangle outlined with 4 lines. Coordinates in pixels (top-left origin).
	void DrawWireRect(float x, float y, float w, float h, Vector color);

	// Textured quad. Coordinates in pixels, UVs default to fill [0..1].
	void DrawTexturedRect(float x, float y, float w, float h, TextureHandle texture);

private:
	CDebugOverlay2D() = default;
	~CDebugOverlay2D() = default;
	CDebugOverlay2D(const CDebugOverlay2D&) = delete;
	CDebugOverlay2D& operator=(const CDebugOverlay2D&) = delete;

	void UploadMatrixAndSize(struct glsl_prog_s *shader);

	shader_t m_Shader;
	GLuint m_iVAO = 0;
	GLuint m_iVBO = 0;
	bool m_bInitialized = false;
};
