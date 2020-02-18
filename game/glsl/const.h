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

#define NORMAL_FLATSHADE		0.7		// same as Valve 'flatshade'
#define SHADE_LAMBERT		1.495		// src_main\engine\client\gl_local.h
#define LIGHTMAP_SHIFT		(1.0 / 128.0)	// same as >> 7 in Quake
#define DLIGHT_SCALE		0.5		// dynamic light global multiplier
#define Z_NEAR			4.0		// Quake const
#define SHADOW_BIAS			0.001		// emperically determined
#define DETAIL_SCALE		2.0		// ( src + dst ) * 2.0
#define r_turbsin( x )		( sin( x ) * 4.0 )	// during the war sine values can reach 3.0!
#define FOV_MULT			0.5		// FIXME: this should be 0.25

#define BMODEL_ALPHA_THRESHOLD	0.25
#define STUDIO_ALPHA_THRESHOLD	0.5

#endif//CONST_H