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
#include <map>
#include <vector>

class CDebugVisualizerBackend
{
public:
	static CDebugVisualizerBackend& GetInstance();

	void Initialize();
	void Shutdown();
	void DrawFrame();

private:
	struct SVertex
	{
		Vector position;
		Vector color;
		float alpha;
	};

	CDebugVisualizerBackend() = default;
	~CDebugVisualizerBackend() = default;
	CDebugVisualizerBackend(const CDebugVisualizerBackend&) = delete;
	CDebugVisualizerBackend(CDebugVisualizerBackend&&) = delete;
	CDebugVisualizerBackend& operator=(const CDebugVisualizerBackend&) = delete;
	CDebugVisualizerBackend& operator=(CDebugVisualizerBackend&&) = delete;

	struct LineBucketKey { bool depthTest; float lineWidth; };
	struct LineBucketKeyLess {
		bool operator()(const LineBucketKey &a, const LineBucketKey &b) const {
			if (a.depthTest != b.depthTest) return a.depthTest < b.depthTest;
			return a.lineWidth < b.lineWidth;
		}
	};
	struct TriBucketKey { bool depthTest; CDebugVisualizer::BlendMode blendMode; };
	struct TriBucketKeyLess {
		bool operator()(const TriBucketKey &a, const TriBucketKey &b) const {
			if (a.depthTest != b.depthTest) return a.depthTest < b.depthTest;
			return static_cast<int>(a.blendMode) < static_cast<int>(b.blendMode);
		}
	};

	void TessellateAABB(const CDebugVisualizer::AABBData &data, const Vector &color, std::vector<SVertex> &out);
	void TessellateSphere(const CDebugVisualizer::SphereData &data, const Vector &color, std::vector<SVertex> &out);
	void TessellateVector(const CDebugVisualizer::VectorData &data, const Vector &color, std::vector<SVertex> &out);
	void TessellateFrustum(const CDebugVisualizer::FrustumData &data, const Vector &color, std::vector<SVertex> &out);
	void TessellateFilledBox(const CDebugVisualizer::FilledBoxData &data, std::vector<SVertex> &out);
	void RenderPrimitives(const std::vector<SVertex> &verts, GLenum topology);

	std::map<LineBucketKey, std::vector<SVertex>, LineBucketKeyLess> m_lineBuckets;
	std::map<TriBucketKey, std::vector<SVertex>, TriBucketKeyLess> m_triBuckets;
	shader_t m_DebugShader;
	GLuint m_iVAO = 0;
	GLuint m_iVBO = 0;
	bool m_bInitialized = false;
};
