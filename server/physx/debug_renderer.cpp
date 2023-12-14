/*
debug_renderer.cpp - part of PhysX physics engine implementation
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
#include "debug_renderer.h"
#include "extdll.h"
#include "triangleapi.h"
#include "util.h"

using namespace physx;

void DebugRenderer::SetupColor(PxU32 color) const
{
	float blue = PxU32(color & 0xff) / 255.0f;
	float green = PxU32((color >> 8) & 0xff) / 255.0f;
	float red = PxU32((color >> 16) & 0xff) / 255.0f;
	float alpha = PxU32((color >> 24) & 0xff) / 255.0f;
	Tri->Color4f(red, green, blue, alpha);
}

void DebugRenderer::RenderData(const PxRenderBuffer &data) const
{
	RenderPoints(data.getPoints(), data.getNbPoints());
	RenderLines(data.getLines(), data.getNbLines());
	RenderTriangles(data.getTriangles(), data.getNbTriangles());
}

void DebugRenderer::RenderPoints(const PxDebugPoint *points, PxU32 count) const
{
	Tri->Begin(TRI_POINTS);
	for (PxU32 i = 0; i < count; i++)
	{
		const PxDebugPoint &point = points[i];
		SetupColor(point.color);
		Tri->Vertex3fv((float *)&point.pos.x);
	}
	Tri->End();
}

void DebugRenderer::RenderLines(const PxDebugLine *lines, PxU32 count) const
{
	Tri->Begin(TRI_LINES);
	for (PxU32 i = 0; i < count; i++)
	{
		const PxDebugLine &line = lines[i];
		SetupColor(line.color0);
		Tri->Vertex3fv((float *)&line.pos0.x);
		SetupColor(line.color1);
		Tri->Vertex3fv((float *)&line.pos1.x);
	}
	Tri->End();
}

void DebugRenderer::RenderTriangles(const PxDebugTriangle *triangles, PxU32 count) const
{
	Tri->Begin(TRI_TRIANGLES);
	for (PxU32 i = 0; i < count; i++)
	{
		const PxDebugTriangle &tri = triangles[i];
		SetupColor(tri.color0);
		Tri->Vertex3fv((float *)&tri.pos0.x);
		SetupColor(tri.color1);
		Tri->Vertex3fv((float *)&tri.pos1.x);
		SetupColor(tri.color2);
		Tri->Vertex3fv((float *)&tri.pos2.x);
	}
	Tri->End();
}
