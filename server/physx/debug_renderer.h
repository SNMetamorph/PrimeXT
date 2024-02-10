/*
debug_renderer.h - part of PhysX physics engine implementation
Copyright (C) 2023 SNMetamorph

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
#include <PxConfig.h>
#include <PxPhysicsAPI.h>

class DebugRenderer
{
public:
	void RenderData(const physx::PxRenderBuffer& data) const;

private:
	void SetupColor(physx::PxU32 color) const;
	void RenderPoints(const physx::PxDebugPoint *points, physx::PxU32 count) const;
	void RenderLines(const physx::PxDebugLine *lines, physx::PxU32 count) const;
	void RenderTriangles(const physx::PxDebugTriangle *triangles, physx::PxU32 count) const;
};
