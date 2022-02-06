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

#include "gl_viewport.h"

CViewport::CViewport(int x, int y, int w, int h)
{
    m_iOffsetX = x;
    m_iOffsetY = y;
    m_iWidth = w;
    m_iHeight = h;
}
