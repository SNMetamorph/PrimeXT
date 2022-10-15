/*
gl_debug_scope.h - OpenGL debug scope implementation for suitable graphical debugging
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "gl_debug_scope.h"
#include "gl_export.h"
#include "gl_local.h"
#include "utils.h"

COpenGLDebugScope::COpenGLDebugScope(const char *funcName, int line)
{
	if (developer_level > 0 && GL_Support(R_KHR_DEBUG)) {
		pglPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION_ARB, 0, -1, funcName);
	}
}

COpenGLDebugScope::~COpenGLDebugScope()
{
	if (developer_level > 0 && GL_Support(R_KHR_DEBUG)) {
		pglPopDebugGroup();
	}
}
