/*
const.h - GLSL constants
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

#ifndef CONST_H
#define CONST_H

//
// shared constants
//
#define INV_M_PI		( 1.0 / M_PI)
#define Z_NEAR		4.0		// don't edit this!
#define DETAIL_SCALE	2.0
#define LIGHT_SCALE		1.0
#define DLIGHT_SCALE	1.5
#define LIGHT_FALLOFF	2.2		// close matched with attenuation texture
#define LIGHTMAP_SHIFT	(1.0 / 128.0)	// same as >> 7 in Quake
#define NORMAL_FLATSHADE	0.7
#define GRASS_FLATSHADE	0.5
#define PUDDLE_NORMAL	vec3( 0.0, 0.0, 1.0 )
#define SHADE_LAMBERT	1.495
#define FOV_MULT		0.5		// FIXME: this should be 0.25
#define SCREEN_GAMMA    2.2     // TODO: make it as uniform and controlled by 'gamma' cvar

// signed distance field stuff
#define SOFT_EDGE_MIN		0.15
#define SOFT_EDGE_MAX		0.45

// alphafunc thresholds
#define STUDIO_ALPHA_THRESHOLD	0.5
#define BMODEL_ALPHA_THRESHOLD	0.25

//
// deferred rendering only
//
#define DISCARD_EPSILON		0.001
#define EQUAL_EPSILON    		4e-3

// !!! if this is changed, it must be changed in bspfile.h too !!!
#define EMIT_SURFACE		0.0
#define EMIT_POINT			1.0
#define EMIT_SPOTLIGHT		2.0
#define EMIT_SKYLIGHT		3.0

#define falloff_inverse		1.0	// inverse (1/x), scaled by 1/128
#define falloff_valve		6.0	// special case for valve attenuation style (unreachable by "delay" field)

#endif//CONST_H