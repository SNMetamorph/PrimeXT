/*
debug_visualizer.cpp - utilities for visualizing data for debug purposes
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

#include "debug_visualizer.h"
#include "hud.h"

CDebugVisualizer::Primitive::Primitive(Class type, Vector color, std::optional<float> lifespan, bool depthTest, DataVariant data) :
	type(type),
	color(color),
	depthTest(depthTest),
	rendered(false), 
	data(std::move(data)) 
{
	const float currentTime = gEngfuncs.GetClientTime();
	if (lifespan.has_value()) {
		expirationTime = currentTime + lifespan.value();
	}
	else {
		expirationTime = std::nullopt;
	}
}

CDebugVisualizer& CDebugVisualizer::GetInstance()
{
	static CDebugVisualizer instance;
	return instance;
}

void CDebugVisualizer::RunFrame()
{
	const float currentTime = gEngfuncs.GetClientTime();
	auto predicate = [currentTime](const Primitive& prim) {
		if (prim.expirationTime.has_value()) {
			return currentTime >= prim.expirationTime.value();
		}
		return prim.rendered;
	};
	m_primitives.erase(std::remove_if(m_primitives.begin(), m_primitives.end(), predicate), m_primitives.end());
}

void CDebugVisualizer::DrawAABB(const Vector &mins, const Vector &maxs, Vector color, std::optional<float> lifespan, bool depthTest)
{
	m_primitives.emplace_back(Primitive::Class::AABB, color, lifespan, depthTest, AABBData{mins, maxs});
}

void CDebugVisualizer::DrawSphere(const Vector &center, float radius, Vector color, std::optional<float> lifespan, bool depthTest)
{
	m_primitives.emplace_back(Primitive::Class::Sphere, color, lifespan, depthTest, SphereData{ center, radius });
}

void CDebugVisualizer::DrawVector(const Vector &position, const Vector &direction, Vector color, std::optional<float> lifespan, bool depthTest)
{
	m_primitives.emplace_back(Primitive::Class::Vector, color, lifespan, depthTest, VectorData{ position, direction });
}

void CDebugVisualizer::DrawFrustum(const CFrustum &frustum, Vector color, std::optional<float> lifespan, bool depthTest)
{
	m_primitives.emplace_back(Primitive::Class::Frustum, color, lifespan, depthTest, FrustumData{ frustum });
}
