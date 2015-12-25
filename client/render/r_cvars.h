/*
r_cvars.h - renderer console variables list
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef R_CVARS_H
#define R_CVARS_H

extern cvar_t	*gl_renderer;
extern cvar_t	*r_test;	// just cvar for testify new effects
extern cvar_t	*r_debug;	//show renderer info
extern cvar_t	*mod_allow_materials;
extern cvar_t	*r_shadows;	//original HL shadows
extern cvar_t	*r_mirrors;	//mirrors
extern cvar_t	*r_screens;	//screens
extern cvar_t	*r_overview;
extern cvar_t	*r_finish;
extern cvar_t	*r_fastsky;
extern cvar_t	*r_speeds;
extern cvar_t	*cl_viewsize;
extern cvar_t	*r_stencilbits;
extern cvar_t	*r_draw_beams;
extern cvar_t	*r_dynamic;
extern cvar_t	*r_width;
extern cvar_t	*r_height;
extern cvar_t	*r_novis;
extern cvar_t	*r_nocull;
extern cvar_t	*r_lockpvs;
extern cvar_t	*r_lightmap;
extern cvar_t	*r_adjust_fov;
extern cvar_t	*r_wireframe;
extern cvar_t	*r_fullbright;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_allow_static;
extern cvar_t	*r_allow_mirrors;
extern cvar_t	*r_allow_portals;
extern cvar_t	*r_allow_screens;
extern cvar_t	*r_faceplanecull;
extern cvar_t	*r_detailtextures;
extern cvar_t	*r_lighting_ambient;
extern cvar_t	*r_lighting_modulate;
extern cvar_t	*r_lightstyle_lerping;
extern cvar_t	*r_lighting_extended;
extern cvar_t	*r_bloom_alpha;
extern cvar_t	*r_bloom_diamond_size;
extern cvar_t	*r_bloom_intensity;
extern cvar_t	*r_bloom_darken;
extern cvar_t	*r_bloom_sample_size;
extern cvar_t	*r_bloom_fast_sample;
extern cvar_t	*r_bloom;
extern cvar_t	*r_grass;
extern cvar_t	*r_grass_alpha;
extern cvar_t	*r_grass_lighting;
extern cvar_t	*r_grass_shadows;
extern cvar_t	*r_grass_fade_start;
extern cvar_t	*r_grass_fade_dist;
extern cvar_t	*r_overbright;
extern cvar_t	*r_skyfog;

#endif//R_CVARS_H
