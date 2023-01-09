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
#include "cvardef.h"
#include "gl_local.h"

void R_InitializeConVars();

extern cvar_t *r_test;	// just cvar for testify new effects
extern cvar_t *r_stencilbits;
extern cvar_t *r_drawentities;
extern cvar_t *gl_extensions;
extern cvar_t *gl_debug_verbose;
extern cvar_t *cv_dynamiclight;
extern cvar_t *r_detailtextures;
extern cvar_t *r_lighting_ambient;
extern cvar_t *r_lighting_modulate;
extern cvar_t *r_lightstyle_lerping;
extern cvar_t *r_lighting_extended;
extern cvar_t *r_occlusion_culling;
extern cvar_t *r_show_lightprobes;
extern cvar_t *r_show_cubemaps;
extern cvar_t *r_show_viewleaf;
extern cvar_t *r_show_luminance;
extern cvar_t *cv_crosshair;
extern cvar_t *r_shadows;
extern cvar_t *r_fullbright;
extern cvar_t *r_draw_beams;
extern cvar_t *r_overview;
extern cvar_t *r_novis;
extern cvar_t *r_nocull;
extern cvar_t *r_lockpvs;
extern cvar_t *gl_hdr;
extern cvar_t *r_tonemap;
extern cvar_t *r_bloom;
extern cvar_t *r_bloom_scale;
extern cvar_t *r_dof;
extern cvar_t *r_dof_hold_time;
extern cvar_t *r_dof_change_time;
extern cvar_t *r_dof_focal_length;
extern cvar_t *r_dof_fstop;
extern cvar_t *r_dof_debug;
extern cvar_t *r_allow_mirrors;
extern cvar_t *r_allow_portals;
extern cvar_t *r_allow_screens;
extern cvar_t *r_allow_3dsky;
extern cvar_t *cv_renderer;
extern cvar_t *cv_brdf;
extern cvar_t *cv_bump;
extern cvar_t *cv_bumpvecs;
extern cvar_t *cv_specular;
extern cvar_t *cv_parallax;
extern cvar_t *cv_decals;
extern cvar_t *cv_realtime_puddles;
extern cvar_t *cv_shadow_offset;
extern cvar_t *cv_deferred;
extern cvar_t *cv_deferred_full;
extern cvar_t *cv_deferred_maxlights;
extern cvar_t *cv_deferred_tracebmodels;
extern cvar_t *cv_cube_lod_bias;
extern cvar_t *cv_gamma;
extern cvar_t *cv_brightness;
extern cvar_t *cv_water;
extern cvar_t *cv_decalsdebug;
extern cvar_t *cv_show_tbn;
extern cvar_t *cv_nosort;
extern cvar_t *r_cubemap;
extern cvar_t *r_cubemap_realtime;
extern cvar_t *r_lightmap;
extern cvar_t *r_speeds;
extern cvar_t *r_decals;
extern cvar_t *r_studio_decals;
extern cvar_t *r_hand;
extern cvar_t *r_sunshadows;
extern cvar_t *r_sun_allowed;
extern cvar_t *r_sun_daytime;
extern cvar_t *r_shadow_split_weight;
extern cvar_t *r_wireframe;
extern cvar_t *r_lightstyles;
extern cvar_t *r_polyoffset;
extern cvar_t *r_dynamic;
extern cvar_t *r_finish;
extern cvar_t *r_clear;
extern cvar_t *r_grass;
extern cvar_t *r_grass_alpha;
extern cvar_t *r_grass_lighting;
extern cvar_t *r_grass_shadows;
extern cvar_t *r_grass_fade_start;
extern cvar_t *r_grass_fade_dist;
extern cvar_t *r_scissor_glass_debug;
extern cvar_t *r_scissor_light_debug;
extern cvar_t *r_showlightmaps;
extern cvar_t *r_recursion_depth;
extern cvar_t *r_shadowmap_size;
extern cvar_t *r_pssm_show_split;
extern cvar_t *r_renderplayershadow;
extern cvar_t *v_sunshafts;
extern cvar_t *v_glows;
extern cvar_t *v_posteffects;
extern cvar_t *r_postfx_enable;

#endif//R_CVARS_H
