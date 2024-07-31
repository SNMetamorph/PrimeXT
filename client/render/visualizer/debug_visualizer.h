#pragma once
#include "vector.h"
#include "gl_frustum.h"

class CDebugVisualizer
{
public:
	void DrawAABB(const Vector &mins, const Vector &maxs, Vector color, float lifespan, bool depthTest);
	void DrawSphere(const Vector &center, float radius, Vector color, float lifespan, bool depthTest);
	void DrawVector(const Vector &position, const Vector& direction, Vector color, float lifespan, bool depthTest);
	void DrawFrustum(const CFrustum &frustum, Vector color, float lifespan, bool depthTest);
	// and so on...
};
