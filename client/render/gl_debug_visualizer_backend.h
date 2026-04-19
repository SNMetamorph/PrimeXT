/*
gl_debug_visualizer_backend.h - rendering backend for debug visualizer
Copyright (C) 2025 SNMetamorph

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
#include "visualizer/debug_visualizer.h"
#include <vector>

class CDebugVisualizerBackend
{
public:
	static CDebugVisualizerBackend& GetInstance();

	void Initialize();
	void Shutdown();
	void DrawFrame();

private:
	struct SLineVertex
	{
		Vector position;
		Vector color;
	};

	CDebugVisualizerBackend() = default;
	~CDebugVisualizerBackend() = default;
	CDebugVisualizerBackend(const CDebugVisualizerBackend&) = delete;
	CDebugVisualizerBackend(CDebugVisualizerBackend&&) = delete;
	CDebugVisualizerBackend& operator=(const CDebugVisualizerBackend&) = delete;
	CDebugVisualizerBackend& operator=(CDebugVisualizerBackend&&) = delete;

	void TessellateAABB(const CDebugVisualizer::AABBData &data, const Vector &color, std::vector<SLineVertex> &out);
	void TessellateSphere(const CDebugVisualizer::SphereData &data, const Vector &color, std::vector<SLineVertex> &out);
	void TessellateVector(const CDebugVisualizer::VectorData &data, const Vector &color, std::vector<SLineVertex> &out);
	void TessellateFrustum(const CDebugVisualizer::FrustumData &data, const Vector &color, std::vector<SLineVertex> &out);
	void RenderLines(const std::vector<SLineVertex> &lines, bool depthEnabled);

	std::vector<SLineVertex> m_depthLines;
	std::vector<SLineVertex> m_noDepthLines;
	shader_t m_DebugShader;
	GLuint m_iVAO = 0;
	GLuint m_iVBO = 0;
	bool m_bInitialized = false;
};
