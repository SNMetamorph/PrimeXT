#pragma once
#include "vector.h"
#include "gl_frustum.h"
#include "shader.h"

#define MAX_DEBUG_VBO_SIZE 1024 * 1024

struct SLineVertex
{
	Vector position;
	Vector color;
};

class CDebugVisualizer
{
public:
	static CDebugVisualizer& GetInstance();

public:
	void Initialize();
	void Shutdown();

	void Render();

	void DrawLine(const Vector &start, const Vector& end, Vector color, float lifespan, bool depthTest);
	void DrawAABB(const Vector &mins, const Vector &maxs, Vector color, float lifespan, bool depthTest);
	void DrawSphere(const Vector &center, float radius, Vector color, float lifespan, bool depthTest);
	void DrawVector(const Vector &position, const Vector& direction, Vector color, float lifespan, bool depthTest);
	void DrawFrustum(const CFrustum &frustum, Vector color, float lifespan, bool depthTest);
	// and so on...

private:
	void RenderLines(const std::vector<SLineVertex>& lines, bool isDepthEnabled);

private:
	std::vector<SLineVertex> m_noDepthLines;
	std::vector<SLineVertex> m_depthLines;

	shader_t m_DebugShader;

	GLuint m_iVAO = 0;
	GLuint m_iVBO = 0;

	bool m_bInitialized = false;
};

extern CDebugVisualizer g_DebugVisualizer;
