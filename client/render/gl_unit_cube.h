/*
gl_unit_cube.h - class for suitable rendering unit cube in OpenGL
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
#include "gl_export.h"
#include "matrix.h"

class COpenGLUnitCube
{
public:
	static COpenGLUnitCube &GetInstance();
	void Initialize();
	void Draw();
	void CreateModelViewMatrix(matrix4x4 &modelViewMatrix, const vec3_t &angles);

private:
	COpenGLUnitCube() {};
	~COpenGLUnitCube() {};
	COpenGLUnitCube(const COpenGLUnitCube &) = delete;
	COpenGLUnitCube &operator=(const COpenGLUnitCube &) = delete;
	
	GLuint m_iCubeVAO = 0;
	GLuint m_iCubeVBO = 0;
};
