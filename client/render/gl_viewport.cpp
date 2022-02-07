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
#include "gl_export.h"

CViewport::CViewport()
{
    m_iOffsetX = 0;
    m_iOffsetY = 0;
    m_iWidth = 0;
    m_iHeight = 0;
}

CViewport::CViewport(const int *viewportArray)
{
    m_iOffsetX = viewportArray[0];
    m_iOffsetY = viewportArray[1];
    m_iWidth = viewportArray[2];
    m_iHeight = viewportArray[3];
}

CViewport::CViewport(int x, int y, int w, int h)
{
    m_iOffsetX = x;
    m_iOffsetY = y;
    m_iWidth = w;
    m_iHeight = h;
}

void CViewport::SetAsCurrent() const
{
    pglViewport(m_iOffsetX, m_iOffsetY, m_iWidth, m_iHeight);
}

void CViewport::WriteToArray(int *viewportArray) const
{
    viewportArray[0] = m_iOffsetX;
    viewportArray[1] = m_iOffsetY;
    viewportArray[2] = m_iWidth;
    viewportArray[3] = m_iHeight;
}

bool CViewport::CompareWith(const CViewport &viewport) const
{
    if (viewport.GetX() != m_iOffsetX) {
        return true; // different
    }
    if (viewport.GetY() != m_iOffsetY) {
        return true;
    }
    if (viewport.GetWidth() != m_iWidth) {
        return true;
    }
    if (viewport.GetHeight() != m_iHeight) {
        return true;
    }
    return false; // equal
}

bool CViewport::CompareWith(const int *viewportArray) const
{
    CViewport operand = CViewport(viewportArray);
    return operand.CompareWith(*this);
}
