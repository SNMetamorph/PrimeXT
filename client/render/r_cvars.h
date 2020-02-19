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
extern cvar_t	*r_extensions;
extern cvar_t	*r_shadows;	//original HL shadows
extern cvar_t	*r_finish;
extern cvar_t	*r_clear;
extern cvar_t	*r_speeds;
extern cvar_t	*cl_viewsize;
extern cvar_t	*r_dynamic;
extern cvar_t	*r_novis;
extern cvar_t	*r_nocull;
extern cvar_t	*r_nosort;
extern cvar_t	*r_lockpvs;
extern cvar_t	*r_lightmap;
extern cvar_t	*r_adjust_fov;
extern cvar_t	*r_wireframe;
extern cvar_t	*r_fullbright;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_allow_3dsky;
extern cvar_t	*r_allow_mirrors;
extern cvar_t	*r_allow_portals;
extern cvar_t	*r_allow_screens;
extern cvar_t	*r_recursion_depth;
extern cvar_t	*r_detailtextures;
extern cvar_t	*r_lighting_modulate;
extern cvar_t	*r_lightstyle_lerping;
extern cvar_t	*r_lighting_extended;
extern cvar_t	*r_recursive_world_node;
extern cvar_t	*r_polyoffset;
extern cvar_t	*r_grass;
extern cvar_t	*r_grass_vbo;
extern cvar_t	*r_grass_alpha;
extern cvar_t	*r_grass_lighting;
extern cvar_t	*r_grass_shadows;
extern cvar_t	*r_grass_fade_start;
extern cvar_t	*r_grass_fade_dist;
extern cvar_t	*gl_check_errors;
extern cvar_t	*vid_gamma;
extern cvar_t	*vid_brightness;
extern cvar_t	*r_show_renderpass;
extern cvar_t	*r_show_light_scissors;
extern cvar_t	*r_show_normals;
extern cvar_t	*r_show_lightprobes;

#endif//R_CVARS_H
