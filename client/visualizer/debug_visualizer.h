/*
debug_visualizer.h - utilities for visualizing data for debug purposes
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
#include "vector.h"
#include "frustum.h"
#include <vector>
#include <optional>
#include <variant>

class CDebugVisualizer
{
public:
	struct AABBData { Vector mins, maxs; };
	struct SphereData { Vector center; float radius; };
	struct VectorData { Vector position, direction; };
	struct FrustumData { CFrustum frustum; };

	class Primitive
	{
	public:
		using DataVariant = std::variant<AABBData, SphereData, VectorData, FrustumData>;

		enum class Class
		{
			AABB,
			Sphere,
			Vector,
			Frustum,
		};

		Primitive(Class type, Vector color, std::optional<float> lifespan, bool depthTest, DataVariant data);

		Class type;
		bool depthTest;
		bool rendered;
		Vector color;
		std::optional<float> expirationTime;
		DataVariant data;
	};

	static CDebugVisualizer& GetInstance();
	const std::vector<Primitive>& GetPrimitives() const { return m_primitives; }
	void RunFrame();

	void DrawAABB(const Vector &mins, const Vector &maxs, Vector color, std::optional<float> lifespan, bool depthTest);
	void DrawSphere(const Vector &center, float radius, Vector color, std::optional<float> lifespan, bool depthTest);
	void DrawVector(const Vector &position, const Vector& direction, Vector color, std::optional<float> lifespan, bool depthTest);
	void DrawFrustum(const CFrustum &frustum, Vector color, std::optional<float> lifespan, bool depthTest);
	// ...and other geometric primitives as needed

private:
	CDebugVisualizer() = default;
	~CDebugVisualizer() = default;
	CDebugVisualizer(const CDebugVisualizer&) = delete;
	CDebugVisualizer(CDebugVisualizer&&) = delete;
	CDebugVisualizer& operator=(const CDebugVisualizer&) = delete;
	CDebugVisualizer& operator=(CDebugVisualizer&&) = delete;

	std::vector<Primitive> m_primitives;
};
