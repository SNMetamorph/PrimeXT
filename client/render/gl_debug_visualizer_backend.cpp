/*
gl_debug_visualizer_backend.cpp - rendering backend for debug visualizer
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

#include "gl_debug_visualizer_backend.h"
#include "gl_local.h"
#include "mathlib.h"
#include <algorithm>
#include <cmath>
#include <variant>

#define MAX_DEBUG_VBO_SIZE (64 * 1024)

CDebugVisualizerBackend& CDebugVisualizerBackend::GetInstance()
{
	static CDebugVisualizerBackend instance;
	return instance;
}

void CDebugVisualizerBackend::Initialize()
{
	if (m_bInitialized)
		return;

	m_depthLines.reserve(1024);
	m_noDepthLines.reserve(1024);

	word shaderHandle = GL_FindShader("common/debug_draw", "common/debug_draw", "common/debug_draw");
	m_DebugShader.SetShader(shaderHandle);

	pglGenVertexArrays(1, &m_iVAO);
	pglGenBuffersARB(1, &m_iVBO);

	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);
	pglBufferDataARB(GL_ARRAY_BUFFER_ARB, MAX_DEBUG_VBO_SIZE * sizeof(SLineVertex), nullptr, GL_DYNAMIC_DRAW_ARB);

	pglBindVertexArray(m_iVAO);

	pglEnableVertexAttribArrayARB(ATTR_INDEX_POSITION);
	pglVertexAttribPointerARB(ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(SLineVertex), (void *)0);

	pglEnableVertexAttribArrayARB(ATTR_INDEX_LIGHT_COLOR);
	pglVertexAttribPointerARB(ATTR_INDEX_LIGHT_COLOR, 3, GL_FLOAT, GL_FALSE, sizeof(SLineVertex), (void *)(3 * sizeof(float)));

	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	pglBindVertexArray(0);

	m_bInitialized = true;
}

void CDebugVisualizerBackend::Shutdown()
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
	m_DebugShader.Invalidate();
	m_bInitialized = false;
}

void CDebugVisualizerBackend::DrawFrame()
{
	if (!m_bInitialized)
		return;

	m_depthLines.clear();
	m_noDepthLines.clear();

	const auto &primitives = CDebugVisualizer::GetInstance().GetPrimitives();
	for (const CDebugVisualizer::Primitive &primitive : primitives)
	{
		std::vector<SLineVertex> &bucket = primitive.depthTest ? m_depthLines : m_noDepthLines;
		std::visit([&](const auto &payload) {
			using T = std::decay_t<decltype(payload)>;
			if constexpr (std::is_same_v<T, CDebugVisualizer::AABBData>)
				TessellateAABB(payload, primitive.color, bucket);
			else if constexpr (std::is_same_v<T, CDebugVisualizer::SphereData>)
				TessellateSphere(payload, primitive.color, bucket);
			else if constexpr (std::is_same_v<T, CDebugVisualizer::VectorData>)
				TessellateVector(payload, primitive.color, bucket);
			else if constexpr (std::is_same_v<T, CDebugVisualizer::FrustumData>)
				TessellateFrustum(payload, primitive.color, bucket);
		}, primitive.data);
	}

	glsl_program_t *shader = m_DebugShader.GetShader();
	if (!shader)
		return;

	GL_BindShader(shader);

	GLint modelLoc = pglGetUniformLocation(shader->handle, "u_ModelMatrix");
	GLint projLoc = pglGetUniformLocation(shader->handle, "u_ProjectionMatrix");
	if (modelLoc >= 0) {
		static const GLfloat identity[16] = {
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f,
		};
		pglUniformMatrix4fvARB(modelLoc, 1, GL_FALSE, identity);
	}
	if (projLoc >= 0) {
		pglUniformMatrix4fvARB(projLoc, 1, GL_FALSE, RI->glstate.modelviewProjectionMatrix);
	}

	const GLboolean depthWasEnabled = pglIsEnabled(GL_DEPTH_TEST);
	RenderLines(m_depthLines, true);
	RenderLines(m_noDepthLines, false);
	if (depthWasEnabled)
		pglEnable(GL_DEPTH_TEST);
	else
		pglDisable(GL_DEPTH_TEST);
}

void CDebugVisualizerBackend::RenderLines(const std::vector<SLineVertex> &lines, bool depthEnabled)
{
	if (lines.empty())
		return;

	if (depthEnabled)
		pglEnable(GL_DEPTH_TEST);
	else
		pglDisable(GL_DEPTH_TEST);

	pglBindVertexArray(m_iVAO);
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);

	size_t offset = 0;
	while (offset < lines.size())
	{
		const size_t kLineVertexLimit = MAX_DEBUG_VBO_SIZE;
		size_t drawCount = std::min(kLineVertexLimit, lines.size() - offset);

		pglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, drawCount * sizeof(SLineVertex), lines.data() + offset);
		pglDrawArrays(GL_LINES, 0, drawCount);

		offset += drawCount;
	}
}

void CDebugVisualizerBackend::TessellateAABB(const CDebugVisualizer::AABBData &data, const Vector &color, std::vector<SLineVertex> &out)
{
	Vector from = data.mins;
	Vector to = data.maxs;

	Vector halfExtents = (to - from) * 0.5f;
	Vector center = (to + from) * 0.5f;

	Vector edgecoord(1.f, 1.f, 1.f), pa, pb;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			pa = Vector(edgecoord[0] * halfExtents[0], edgecoord[1] * halfExtents[1],
				edgecoord[2] * halfExtents[2]);
			pa += center;

			int othercoord = j % 3;
			edgecoord[othercoord] *= -1.f;
			pb = Vector(edgecoord[0] * halfExtents[0], edgecoord[1] * halfExtents[1],
				edgecoord[2] * halfExtents[2]);
			pb += center;

			out.push_back({ pa, color });
			out.push_back({ pb, color });
		}

		edgecoord = Vector(-1.f, -1.f, -1.f);
		if (i < 3)
			edgecoord[i] *= -1.f;
	}
}

void CDebugVisualizerBackend::TessellateSphere(const CDebugVisualizer::SphereData &data, const Vector &color, std::vector<SLineVertex> &out)
{
	const unsigned int latitudeDivisions = 8;
	const unsigned int longitudeDivisions = 8;

	auto pointAt = [&](unsigned int lat, unsigned int lon) {
		float theta = (float)lat * M_PI / (float)latitudeDivisions;
		float phi = (float)lon * M_PI2 / (float)longitudeDivisions;
		return Vector(
			data.center.x + data.radius * sinf(theta) * cosf(phi),
			data.center.y + data.radius * sinf(theta) * sinf(phi),
			data.center.z + data.radius * cosf(theta));
	};

	for (unsigned int lat = 0; lat < latitudeDivisions; ++lat) {
		for (unsigned int lon = 0; lon < longitudeDivisions; ++lon) {
			out.push_back({ pointAt(lat, lon),     color });
			out.push_back({ pointAt(lat, lon + 1), color });
			out.push_back({ pointAt(lat, lon),     color });
			out.push_back({ pointAt(lat + 1, lon), color });
		}
	}
}

void CDebugVisualizerBackend::TessellateVector(const CDebugVisualizer::VectorData &data, const Vector &color, std::vector<SLineVertex> &out)
{
	out.push_back({ data.position, color });
	out.push_back({ data.position + data.direction, color });
}

void CDebugVisualizerBackend::TessellateFrustum(const CDebugVisualizer::FrustumData &data, const Vector &color, std::vector<SLineVertex> &out)
{
	Vector corners[8];
	data.frustum.ComputeFrustumCorners(corners);

	// corner layout from CFrustum::ComputeFrustumCorners:
	//   0..3 = far  (TL, TR, BL, BR),  4..7 = near (TL, TR, BL, BR)
	static const int edges[12][2] = {
		{ 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 }, // far face
		{ 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 }, // near face
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, // connecting
	};

	for (const auto &edge : edges) {
		out.push_back({ corners[edge[0]], color });
		out.push_back({ corners[edge[1]], color });
	}
}
