#include "gl_local.h"
#include "debug_visualizer.h"

CDebugVisualizer g_DebugVisualizer;

CDebugVisualizer& CDebugVisualizer::GetInstance()
{
	return g_DebugVisualizer;
}

void CDebugVisualizer::Initialize()
{
	if (m_bInitialized)
		return;

	// reserve memory for line buffers
	m_noDepthLines.reserve(1024);
	m_depthLines.reserve(1024);

	// load debug shader
	word shaderHandle = GL_FindShader("common/debug_draw", "common/debug_draw", "common/debug_draw");
	m_DebugShader.SetShader(shaderHandle);

	// create VAO and VBO
	pglGenVertexArrays(1, &m_iVAO);
	pglGenBuffersARB(1, &m_iVBO);

	// fill buffer
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);
	pglBufferDataARB(GL_ARRAY_BUFFER_ARB, MAX_DEBUG_VBO_SIZE * sizeof(SLineVertex), nullptr, GL_DYNAMIC_DRAW_ARB);

	// link vertex attributes
	pglBindVertexArray(m_iVAO);

	pglEnableVertexAttribArrayARB(ATTR_INDEX_POSITION);
	pglVertexAttribPointerARB(ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);

	pglEnableVertexAttribArrayARB(/*ATTR_INDEX_TANGENT*/ATTR_INDEX_LIGHT_COLOR);
	pglVertexAttribPointerARB(/*ATTR_INDEX_TANGENT*/ATTR_INDEX_LIGHT_COLOR, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));

	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	pglBindVertexArray(0);

	m_bInitialized = true;
}

void CDebugVisualizer::Shutdown()
{
	// Release ? TODO...
}

void CDebugVisualizer::Render()
{
	GL_BindShader(m_DebugShader.GetShader());

	// Render lines with depth buffer
	RenderLines(m_depthLines, true);

	// ... and without
	RenderLines(m_noDepthLines, false);
}

void CDebugVisualizer::RenderLines(const std::vector<SLineVertex>& lines, bool isDepthEnabled)
{
	if (lines.empty())
		return;

	if (isDepthEnabled)
		pglEnable(GL_DEPTH_TEST);
	else
		pglDisable(GL_DEPTH_TEST);

	// bind vertex buffer
	pglBindVertexArray(m_iVAO);
	pglBindBufferARB(GL_ARRAY_BUFFER_ARB, m_iVBO);

	size_t offset = 0;
	while (offset < lines.size())
	{
		const size_t kLineVertexLimit = MAX_DEBUG_VBO_SIZE;

		size_t drawCount = std::min(kLineVertexLimit, lines.size() - offset);

		// Update vertex buffer
		//pglBufferDataARB(GL_ARRAY_BUFFER_ARB, drawCount * sizeof(SLineVertex), lines.data() + offset, GL_DYNAMIC_DRAW_ARB);
		pglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, drawCount * sizeof(SLineVertex), lines.data() + offset);
		pglDrawArrays(GL_LINES, 0, lines.size());

		offset += drawCount;
	}

	// TODO: return depth back
}

void CDebugVisualizer::DrawLine(const Vector& start, const Vector& end, Vector color, float lifespan, bool depthTest)
{
	SLineVertex vertexA = { start, color };
	SLineVertex vertexB = { end, color };

	std::vector<SLineVertex>& lines = depthTest ? m_depthLines : m_noDepthLines;
	lines.push_back(vertexA);
	lines.push_back(vertexB);
}

void CDebugVisualizer::DrawAABB(const Vector &mins, const Vector &maxs, Vector color, float lifespan, bool depthTest)
{
	Vector from = Vector(mins.x, mins.y, mins.z);
	Vector to = Vector(maxs.x, maxs.y, maxs.z);

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

			DrawLine(pa, pb, color, lifespan, depthTest);
		}

		edgecoord = Vector(-1.f, -1.f, -1.f);
		if (i < 3)
			edgecoord[i] *= -1.f;
	}
}

void CDebugVisualizer::DrawSphere(const Vector& center, float radius, Vector color, float lifespan, bool depthTest)
{
	std::vector<SLineVertex>& lines = depthTest ? m_depthLines : m_noDepthLines;

	const uint latitudeDivisions = 8;
	const uint longitudeDivisions = 8;
	uint voffset = lines.size();
	uint count = (latitudeDivisions + 1) * (longitudeDivisions + 1);
	for (uint lat = 0; lat <= latitudeDivisions; ++lat) {
		float theta = (float)lat * M_PI / (float)latitudeDivisions;
		for (uint lon = 0; lon <= longitudeDivisions; ++lon) {
			float phi = (float)lon * M_PI2 / (float)longitudeDivisions;
			lines.push_back({ {
					center.x + radius * sin(theta) * cos(phi),
					center.y + radius * sin(theta) * sin(phi),
					center.z + radius * cos(theta) },
					color });
		}
	}
}

void CDebugVisualizer::DrawVector(const Vector& position, const Vector& direction, Vector color, float lifespan, bool depthTest)
{
	DrawLine(position, (position + direction) * position.Length(), color, lifespan, depthTest);
}

void CDebugVisualizer::DrawFrustum(const CFrustum& frustum, Vector color, float lifespan, bool depthTest)
{
	// TODO
}
