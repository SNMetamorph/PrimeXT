/*
fresnel.h - fresnel approximations
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef FRESNEL_H
#define FRESNEL_H

#include "mathlib.h"

#define FRESNEL_FACTOR 5.0
#define WATER_F0_VALUE 0.15
#define GENERIC_F0_VALUE 0.02

float GetFresnel( float cosTheta, float F0, float power )
{
	// Schlick approximation
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), power);
}

float GetFresnel( const vec3 v, const vec3 n, float F0, float power )
{
	// solution for disabling fresnel underwater
	float cosTheta = dot(v, n);
	return mix(0.0, GetFresnel(cosTheta, F0, power), step(0.0, cosTheta));
}

#endif//FRESNEL_H
