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
	// Schlick approximation, use abs here for two-sided surfaces, like water
	return F0 + (1.0 - F0) * pow(1.0 - abs(cosTheta), power);
}

float GetFresnel( const vec3 v, const vec3 n, float F0, float power )
{
	return GetFresnel(dot(v, n), F0, power);
}

#endif//FRESNEL_H
