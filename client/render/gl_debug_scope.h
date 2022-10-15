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

#pragma once

class COpenGLDebugScope
{
public:
    COpenGLDebugScope(const char *funcName, int line);
    ~COpenGLDebugScope();
};

#ifdef _DEBUG
#define GL_DEBUG_SCOPE() COpenGLDebugScope debugScope((__FUNCTION__), (__LINE__))
#else
#define GL_DEBUG_SCOPE()
#endif
