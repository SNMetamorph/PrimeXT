/*
vector.cpp - shared vector operations
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifdef USE_PHYSICS_ENGINE
#include "vector.h"
#include "PxVec3.h"

Vector::Vector(const physx::PxVec3& v)
{
	x = v.x; y = v.y; z = v.z;
}

const Vector& Vector::operator=(const physx::PxVec3& v)
{
	x = v.x; y = v.y; z = v.z;
	return *this;
}

Vector::operator physx::PxVec3() const
{
	return physx::PxVec3(x, y, z);
}

#endif
