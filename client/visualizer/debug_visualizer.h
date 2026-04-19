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
#include <array>
#include <vector>
#include <optional>
#include <variant>

class CDebugVisualizer
{
public:
	enum class BlendMode { Alpha, Additive };

	struct AABBData { Vector mins, maxs; };
	struct SphereData { Vector center; float radius; };
	struct VectorData { Vector position, direction; };
	struct FrustumData { CFrustum frustum; };
	struct FilledBoxData {
		std::array<Vector, 8> corners;
		std::array<Vector, 6> faceColors;
		float alpha;
		BlendMode blendMode;
	};

	class Primitive
	{
	public:
		using DataVariant = std::variant<AABBData, SphereData, VectorData, FrustumData, FilledBoxData>;

		enum class Class
		{
			AABB,
			Sphere,
			Vector,
			Frustum,
			FilledBox,
		};

		Primitive(Class type, Vector color, std::optional<float> lifespan, bool depthTest, DataVariant data, float lineWidth = 1.0f);

		Class type;
		bool depthTest;
		bool rendered;
		float lineWidth;
		Vector color;
		std::optional<float> expirationTime;
		DataVariant data;
	};

	static CDebugVisualizer& GetInstance();
	const std::vector<Primitive>& GetPrimitives() const { return m_primitives; }
	void RunFrame();

	void DrawAABB(const Vector &mins, const Vector &maxs, Vector color, std::optional<float> lifespan, bool depthTest, float lineWidth = 1.0f);
	void DrawSphere(const Vector &center, float radius, Vector color, std::optional<float> lifespan, bool depthTest, float lineWidth = 1.0f);
	void DrawVector(const Vector &position, const Vector& direction, Vector color, std::optional<float> lifespan, bool depthTest, float lineWidth = 1.0f);
	void DrawFrustum(const CFrustum &frustum, Vector color, std::optional<float> lifespan, bool depthTest, float lineWidth = 1.0f);
	void DrawFilledBox(const std::array<Vector, 8> &corners, const std::array<Vector, 6> &faceColors, float alpha, std::optional<float> lifespan, bool depthTest, BlendMode blendMode = BlendMode::Alpha);
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
