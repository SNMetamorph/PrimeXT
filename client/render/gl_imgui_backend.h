/*
gl_imgui_backend.h - implementation of OpenGL backend for ImGui
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
#include "imgui.h"

class CImGuiBackend
{
public:
	CImGuiBackend();
	~CImGuiBackend();

	bool Init(const char *glsl_version = NULL);
	void Shutdown();
	void NewFrame();
	void RenderDrawData(ImDrawData *draw_data);

private:
	bool CreateDeviceObjects();
	void DestroyDeviceObjects();
	bool CreateFontsTexture();
	void DestroyFontsTexture();
};
