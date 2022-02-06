/*
gl_viewport.h - generic OpenGL viewport representation
Copyright (C) 2021 SNMetamorph

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

class CViewport
{
public:
	CViewport(int x, int y, int w, int h);
	inline int GetX() const { return m_iOffsetX; };
	inline int GetY() const { return m_iOffsetY; };
	inline int GetWidth() const { return m_iWidth; };
	inline int GetHeight() const { return m_iHeight; };

	inline void SetX(int value) { m_iOffsetX = value; }
	inline void SetY(int value) { m_iOffsetY = value; }
	inline void SetWidth(int value) { m_iWidth = value; }
	inline void SetHeight(int value) { m_iHeight = value; }

	// for using class like array, with operator []
	operator int *() { return &m_iOffsetX; } 
	operator const int *() const { return &m_iOffsetX; }

private:
	int m_iOffsetX = 0;
	int m_iOffsetY = 0;
	int m_iWidth = 0;
	int m_iHeight = 0;
};
