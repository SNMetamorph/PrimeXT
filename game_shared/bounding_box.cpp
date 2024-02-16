/*
bounding_box.cpp - class representing axis-aligned bounding box
Copyright (C) 2023 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
*/

#include "bounding_box.h"
#include <algorithm>

CBoundingBox::CBoundingBox() :
    m_mins(0.f, 0.f, 0.f),
    m_maxs(0.f, 0.f, 0.f),
    m_size(0.f, 0.f, 0.f)
{
}

CBoundingBox::CBoundingBox(const vec3_t &size)
{
    m_mins = -size / 2.0f;
    m_maxs = size / 2.0f;
    m_size = size;
}

CBoundingBox::CBoundingBox(const vec3_t &mins, const vec3_t &maxs)
{
    m_mins = mins;
    m_maxs = maxs;
    m_size = maxs - mins;
}

vec3_t CBoundingBox::GetCenterPoint() const
{
    return m_mins + m_size / 2.0f;
}

void CBoundingBox::SetCenterToPoint(const vec3_t &point)
{
    m_mins = point - m_size / 2.0f;
    m_maxs = point + m_size / 2.0f;
    m_size = m_maxs - m_mins;
}

CBoundingBox CBoundingBox::GetUnionBounds(const CBoundingBox &rhs) const
{
    CBoundingBox result;
    const vec3_t &currMins = m_mins;
    const vec3_t &currMaxs = m_maxs;
    const vec3_t &operandMins = rhs.GetMins();
    const vec3_t &operandMaxs = rhs.GetMaxs();

    result.m_mins.x = std::min(currMins.x, operandMins.x);
    result.m_mins.y = std::min(currMins.y, operandMins.y);
    result.m_mins.z = std::min(currMins.z, operandMins.z);

    result.m_maxs.x = std::max(currMaxs.x, operandMaxs.x);
    result.m_maxs.y = std::max(currMaxs.y, operandMaxs.y);
    result.m_maxs.z = std::max(currMaxs.z, operandMaxs.z);
    result.m_size = result.m_maxs - result.m_mins;
    return result;
}

CBoundingBox CBoundingBox::GetIntersectionBounds(const CBoundingBox &rhs) const
{
    if (!Intersects(rhs)) {
        return CBoundingBox(); // no intersection, return empty bbox
    }

    CBoundingBox result;
    const vec3_t &currMins = m_mins;
    const vec3_t &currMaxs = m_maxs;
    const vec3_t &operandMins = rhs.GetMins();
    const vec3_t &operandMaxs = rhs.GetMaxs();

    result.m_mins.x = std::max(currMins.x, operandMins.x);
    result.m_mins.y = std::max(currMins.y, operandMins.y);
    result.m_mins.z = std::max(currMins.z, operandMins.z);

    result.m_maxs.x = std::min(currMaxs.x, operandMaxs.x);
    result.m_maxs.y = std::min(currMaxs.y, operandMaxs.y);
    result.m_maxs.z = std::min(currMaxs.z, operandMaxs.z);
    result.m_size = result.m_maxs - result.m_mins;
    return result;
}

void CBoundingBox::ExpandToPoint(const vec3_t &point)
{
    if (point.x < m_mins.x)
        m_mins.x = point.x;
    if (point.y < m_mins.y)
        m_mins.y = point.y;
    if (point.z < m_mins.z)
        m_mins.z = point.z;

    if (point.x > m_maxs.x)
        m_maxs.x = point.x;
    if (point.y > m_maxs.y)
        m_maxs.y = point.y;
    if (point.z > m_maxs.z)
        m_maxs.z = point.z;

    m_size = m_maxs - m_mins;
}

bool CBoundingBox::Contains(const CBoundingBox &rhs) const
{
    const vec3_t &operandMins = rhs.GetMins();
    const vec3_t &operandMaxs = rhs.GetMaxs();

    if (operandMins.x < m_mins.x || operandMins.x > m_maxs.x)
        return false;
    if (operandMins.y < m_mins.y || operandMins.y > m_maxs.y)
        return false;
    if (operandMins.z < m_mins.z || operandMins.z > m_maxs.z)
        return false;

    if (operandMaxs.x < m_mins.x || operandMaxs.x > m_maxs.x)
        return false;
    if (operandMaxs.y < m_mins.y || operandMaxs.y > m_maxs.y)
        return false;
    if (operandMaxs.z < m_mins.z || operandMaxs.z > m_maxs.z)
        return false;

    return true;
}

bool CBoundingBox::Intersects(const CBoundingBox &rhs) const
{
    const vec3_t &operandMins = rhs.GetMins();
    const vec3_t &operandMaxs = rhs.GetMaxs();
    if (m_maxs.x < operandMins.x || m_mins.x > operandMaxs.x) {
        return false; // no intersection along the x-axis
    }
    if (m_maxs.y < operandMins.y || m_mins.y > operandMaxs.y) {
        return false; // no intersection along the y-axis
    }
    if (m_maxs.z < operandMins.z || m_mins.z > operandMaxs.z) {
        return false; // no intersection along the z-axis
    }
    return true;
}

bool CBoundingBox::ContainsPoint(const vec3_t &point) const
{
    if (point.x < m_mins.x || point.x > m_maxs.x)
        return false;
    if (point.y < m_mins.y || point.y > m_maxs.y)
        return false;
    if (point.z < m_mins.z || point.z > m_maxs.z)
        return false;
    else
        return true;
}
