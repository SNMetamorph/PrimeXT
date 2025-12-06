/*
gl_debug_visualizer_backend.h - rendering backend for debug visualizer
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

class CDebugVisualizerBackend
{
public:
	CDebugVisualizerBackend();
	~CDebugVisualizerBackend();

	void DrawFrame();

private:
	CDebugVisualizerBackend(const CDebugVisualizerBackend&) = delete;
	CDebugVisualizerBackend(CDebugVisualizerBackend&&) = delete;
	CDebugVisualizerBackend& operator=(const CDebugVisualizerBackend&) = delete;
	CDebugVisualizerBackend& operator=(CDebugVisualizerBackend&&) = delete;
};
