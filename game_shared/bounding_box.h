/*
bounding_box.h - class representing axis-aligned bounding box
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

#pragma once
#include "vector.h"

class CBoundingBox
{
public:
    CBoundingBox();
    CBoundingBox(const vec3_t &size);
    CBoundingBox(const vec3_t &mins, const vec3_t &maxs);

    vec3_t GetCenterPoint() const;
    void SetCenterToPoint(const vec3_t &point);
    void ExpandToPoint(const vec3_t &point);
    bool ContainsPoint(const vec3_t &point) const;

    bool Contains(const CBoundingBox &rhs) const;
    bool Intersects(const CBoundingBox &rhs) const;
    CBoundingBox GetUnionBounds(const CBoundingBox &rhs) const;
    CBoundingBox GetIntersectionBounds(const CBoundingBox &rhs) const;
    
    const vec3_t& GetSize() const { return m_size; };
    const vec3_t& GetMins() const { return m_mins; };
    const vec3_t& GetMaxs() const { return m_maxs; };

private:
    vec3_t m_mins, m_maxs, m_size;
};
