/*
decomposed_shape.h - class for decomposing rigid body shapes to triangles
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
#include "novodex.h"
#include "PxPhysicsAPI.h"
#include <stdint.h>
#include <vector>

class CPhysicNovodex::DecomposedShape
{
public:
	DecomposedShape() = default;

	bool Triangulate(physx::PxRigidBody *rigidBody);
	physx::PxGeometryType::Enum GetGeometryType(physx::PxRigidBody *rigidBody) const;
	size_t GetTrianglesCount() const;
	const std::vector<uint32_t>& GetIndexBuffer() const;
	const std::vector<physx::PxVec3>& GetVertexBuffer() const;

private:
	void DecomposeBox(physx::PxRigidBody *rigidBody);
	void DecomposeConvexMesh(physx::PxRigidBody *rigidBody, physx::PxShape *shape);
	void DecomposeTriangleMesh(physx::PxRigidBody *rigidBody, physx::PxShape *shape);
	void DebugDummyBox();

	size_t m_trianglesCount = 0;
	std::vector<uint32_t> m_indexBuffer;
	std::vector<physx::PxVec3> m_vertexBuffer;
};
