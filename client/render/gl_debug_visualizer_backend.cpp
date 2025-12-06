/*
gl_debug_visualizer_backend.cpp - rendering backend for debug visualizer
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

#include "gl_debug_visualizer_backend.h"
#include "visualizer/debug_visualizer.h"

CDebugVisualizerBackend::CDebugVisualizerBackend()
{
	// buffers/shaders initialization
}

CDebugVisualizerBackend::~CDebugVisualizerBackend()
{
	// buffers/shaders cleanup
}

void CDebugVisualizerBackend::DrawFrame()
{
	const auto &visualizer = CDebugVisualizer::GetInstance();
	for (const CDebugVisualizer::Primitive &primitive: visualizer.GetPrimitives())
	{
		// iterate throught primitives, generate geometry data for rendering, fill buffers with it, and finally render it
	}
}
