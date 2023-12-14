/*
decomposed_shape.cpp - class for decomposing rigid body shapes to triangles
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
#include "decomposed_shape.h"

using namespace physx;

bool DecomposedShape::Triangulate(PxRigidActor *rigidBody)
{
	PxShape *pShape;
	rigidBody->getShapes(&pShape, 1);
	PxGeometryType::Enum shapeType = pShape->getGeometryType();

	if (shapeType == PxGeometryType::eBOX) {
		DecomposeBox(rigidBody);
	}
	else if (shapeType == PxGeometryType::eCONVEXMESH) {
		DecomposeConvexMesh(rigidBody, pShape);
	}
	else if (shapeType == PxGeometryType::eTRIANGLEMESH) {
		DecomposeTriangleMesh(rigidBody, pShape);
	}
	else {
		// unsupported mesh type, so skip them
		return false;
	}
	return true;
}

physx::PxGeometryType::Enum DecomposedShape::GetGeometryType(PxRigidActor *rigidBody) const
{
	PxShape *pShape;
	rigidBody->getShapes(&pShape, 1);
	return pShape->getGeometryType();
}

size_t DecomposedShape::GetTrianglesCount() const
{
	return m_trianglesCount;
}

const std::vector<uint32_t>& DecomposedShape::GetIndexBuffer() const
{
	return m_indexBuffer;
}

const std::vector<physx::PxVec3>& DecomposedShape::GetVertexBuffer() const
{
	return m_vertexBuffer;
}

void DecomposedShape::DecomposeBox(physx::PxRigidActor *rigidBody)
{
	PxBoxGeometry box;
	constexpr size_t vertsPerShape = 8;
	constexpr size_t trianglesCount = 12;
	constexpr uint32_t trianglesIndices[trianglesCount][3] = {
		{ 1, 2, 3 },
		{ 3, 0, 1 },
		{ 3, 0, 4 },
		{ 4, 7, 3 },
		{ 2, 3, 7 },
		{ 7, 6, 2 },
		{ 1, 0, 4 },
		{ 4, 5, 1 },
		{ 4, 5, 6 },
		{ 6, 7, 4 },
		{ 1, 5, 6 },
		{ 6, 2, 1 }
	};

	for (PxU32 i = 0; i < rigidBody->getNbShapes(); i++)
	{
		PxShape *pBoxShape;
		rigidBody->getShapes(&pBoxShape, 1, i);
		if (pBoxShape->getGeometryType() != PxGeometryType::eBOX) {
			// warn about unsupported shape
			continue;
		}

		pBoxShape->getBoxGeometry(box);
		m_indexBuffer.reserve(m_indexBuffer.size() + trianglesCount * 3);
		m_vertexBuffer.reserve(m_vertexBuffer.size() + vertsPerShape);

		// manually define box corner points
		m_vertexBuffer.emplace_back(box.halfExtents.x, -box.halfExtents.y, -box.halfExtents.z);
		m_vertexBuffer.emplace_back(-box.halfExtents.x, -box.halfExtents.y, -box.halfExtents.z);
		m_vertexBuffer.emplace_back(-box.halfExtents.x, box.halfExtents.y, -box.halfExtents.z);
		m_vertexBuffer.emplace_back(box.halfExtents.x, box.halfExtents.y, -box.halfExtents.z);
		m_vertexBuffer.emplace_back(box.halfExtents.x, -box.halfExtents.y, box.halfExtents.z);
		m_vertexBuffer.emplace_back(-box.halfExtents.x, -box.halfExtents.y, box.halfExtents.z);
		m_vertexBuffer.emplace_back(-box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);
		m_vertexBuffer.emplace_back(box.halfExtents.x, box.halfExtents.y, box.halfExtents.z);

		for (size_t j = 0; j < trianglesCount; j++)
		{
			m_indexBuffer.push_back(i * (vertsPerShape - 1) + trianglesIndices[j][0]);
			m_indexBuffer.push_back(i * (vertsPerShape - 1) + trianglesIndices[j][1]);
			m_indexBuffer.push_back(i * (vertsPerShape - 1) + trianglesIndices[j][2]);
			m_trianglesCount += 1;
		}
	}
}

void DecomposedShape::DecomposeConvexMesh(physx::PxRigidActor *rigidBody, physx::PxShape *shape)
{
	PxConvexMesh *convexMesh = shape->getGeometry().convexMesh().convexMesh;
	PxU32 nbVerts = convexMesh->getNbVertices();
	const PxVec3 *convexVerts = convexMesh->getVertices();
	const PxU8 *indexBuffer = convexMesh->getIndexBuffer();

	PxU32 totalTriangles = 0;
	PxU32 totalVerts = 0;
	for (PxU32 i = 0; i < convexMesh->getNbPolygons(); i++)
	{
		PxHullPolygon face;
		bool status = convexMesh->getPolygonData(i, face);
		PX_ASSERT(status);
		totalVerts += face.mNbVerts;
		totalTriangles += face.mNbVerts - 2;
	}

	// preallocate memory for things
	m_vertexBuffer.resize(totalVerts);
	m_indexBuffer.resize(3 * totalTriangles);
	m_trianglesCount = totalTriangles;

	PxU32 offset = 0;
	PxU32 indexOffset = 0;
	for (PxU32 i = 0; i < convexMesh->getNbPolygons(); i++)
	{
		PxHullPolygon face;
		bool status = convexMesh->getPolygonData(i, face);
		PX_ASSERT(status);
		const PxU8 *faceIndices = indexBuffer + face.mIndexBase;
		for (PxU32 j = 0; j < face.mNbVerts; j++) {
			m_vertexBuffer[offset + j] = convexVerts[faceIndices[j]];
		}
		for (PxU32 j = 2; j < face.mNbVerts; j++)
		{
			m_indexBuffer[indexOffset++] = offset;
			m_indexBuffer[indexOffset++] = offset + j;
			m_indexBuffer[indexOffset++] = offset + j - 1;
		}
		offset += face.mNbVerts;
	}
}

void DecomposedShape::DecomposeTriangleMesh(physx::PxRigidActor *rigidBody, physx::PxShape *shape)
{
	PxTriangleMesh *mesh = shape->getGeometry().triangleMesh().triangleMesh;
	const PxU32 nbVerts = mesh->getNbVertices();
	const PxVec3 *verts = mesh->getVertices();
	const PxU32 nbTris = mesh->getNbTriangles();
	const void *indices = mesh->getTriangles();
	const bool has16bitIndices = mesh->getTriangleMeshFlags() & PxTriangleMeshFlag::e16_BIT_INDICES;
	
	m_vertexBuffer.reserve(nbVerts);
	m_indexBuffer.reserve(nbTris * 3);
	m_trianglesCount = nbTris;

	for (size_t i = 0; i < nbVerts; i++) {
		m_vertexBuffer.push_back(verts[i]);
	}

	for (size_t i = 0; i < nbTris; i++)
	{
		for (size_t j = 0; j < 3; j++)
		{
			if (has16bitIndices) {
				const PxU16* triIndices = reinterpret_cast<const PxU16*>(indices);
				const PxU16 index = triIndices[3 * i + j];
				m_indexBuffer.push_back(static_cast<uint32_t>(index));
			}
			else {
				const PxU32* triIndices = reinterpret_cast<const PxU32*>(indices);
				const PxU32 index = triIndices[3 * i + j];
				m_indexBuffer.push_back(index);
			}
		}
	}
}
