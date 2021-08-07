/*
r_cvars.cpp - renderer console variables list
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

#include "gl_cvars.h"
#include "enginecallback.h"

// P2 renderer cvars
cvar_t *r_test;	// just cvar for testify new effects
cvar_t *r_stencilbits;
cvar_t *r_drawentities;
cvar_t *gl_extensions;
cvar_t *cv_dynamiclight;
cvar_t *r_detailtextures;
cvar_t *r_lighting_ambient;
cvar_t *r_lighting_modulate;
cvar_t *r_lightstyle_lerping;
cvar_t *r_lighting_extended;
cvar_t *r_occlusion_culling;
cvar_t *r_show_lightprobes;
cvar_t *r_show_cubemaps;
cvar_t *r_show_viewleaf;
cvar_t *cv_crosshair;
cvar_t *r_shadows;
cvar_t *r_fullbright;
cvar_t *r_draw_beams;
cvar_t *r_overview;
cvar_t *r_novis;
cvar_t *r_nocull;
cvar_t *r_lockpvs;
cvar_t *r_dof;
cvar_t *r_dof_hold_time;
cvar_t *r_dof_change_time;
cvar_t *r_dof_focal_length;
cvar_t *r_dof_fstop;
cvar_t *r_dof_debug;
cvar_t *r_allow_mirrors;
cvar_t *cv_renderer;
cvar_t *cv_brdf;
cvar_t *cv_bump;
cvar_t *cv_bumpvecs;
cvar_t *cv_specular;
cvar_t *cv_parallax;
cvar_t *cv_decals;
cvar_t *cv_realtime_puddles;
cvar_t *cv_shadow_offset;
cvar_t *cv_cubemaps;
cvar_t *cv_deferred;
cvar_t *cv_deferred_full;
cvar_t *cv_deferred_maxlights;
cvar_t *cv_deferred_tracebmodels;
cvar_t *cv_cube_lod_bias;
cvar_t *cv_gamma;
cvar_t *cv_brightness;
cvar_t *cv_water;
cvar_t *cv_decalsdebug;
cvar_t *cv_show_tbn;
cvar_t *cv_nosort;
cvar_t *r_lightmap;
cvar_t *r_speeds;
cvar_t *r_decals;
cvar_t *r_studio_decals;
cvar_t *r_hand;
cvar_t *r_sunshadows;
cvar_t *r_sun_allowed;
cvar_t *r_shadow_split_weight;
cvar_t *r_wireframe;
cvar_t *r_lightstyles;
cvar_t *r_polyoffset;
cvar_t *r_dynamic;
cvar_t *r_finish;
cvar_t *r_clear;
cvar_t *r_grass;
cvar_t *r_grass_alpha;
cvar_t *r_grass_lighting;
cvar_t *r_grass_shadows;
cvar_t *r_grass_fade_start;
cvar_t *r_grass_fade_dist;
cvar_t *r_scissor_glass_debug;
cvar_t *r_scissor_light_debug;
cvar_t *r_showlightmaps;
cvar_t *r_recursion_depth;
cvar_t *r_shadowmap_size;
cvar_t *r_pssm_show_split;
cvar_t *v_sunshafts;
cvar_t *v_glows;
cvar_t *v_posteffects;
cvar_t *v_grayscale;

// XashXT original renderer cvars
/*
cvar_t	*gl_renderer;
cvar_t	*r_test;	// just cvar for testify new effects
cvar_t	*r_debug;	//show renderer info
cvar_t	*r_extensions;
cvar_t	*r_shadows;	//original HL shadows
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_speeds;
cvar_t	*cl_viewsize;
cvar_t	*r_dynamic;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nosort;
cvar_t	*r_lockpvs;
cvar_t	*r_lightmap;
cvar_t	*r_adjust_fov;
cvar_t	*r_wireframe;
cvar_t	*r_fullbright;
cvar_t	*r_drawentities;
cvar_t   *r_drawworld;
cvar_t   *r_staticentities;
cvar_t   *r_worldpvscull;
cvar_t	*r_allow_3dsky;
cvar_t	*r_allow_mirrors;
cvar_t	*r_allow_portals;
cvar_t	*r_allow_screens;
cvar_t	*r_recursion_depth;
cvar_t	*r_detailtextures;
cvar_t	*r_lighting_modulate;
cvar_t	*r_lightstyle_lerping;
cvar_t	*r_lighting_extended;
cvar_t	*r_recursive_world_node;
cvar_t	*r_polyoffset;
cvar_t	*r_grass;
cvar_t	*r_grass_vbo;
cvar_t	*r_grass_alpha;
cvar_t	*r_grass_lighting;
cvar_t	*r_grass_shadows;
cvar_t	*r_grass_fade_start;
cvar_t	*r_grass_fade_dist;
cvar_t	*gl_check_errors;
cvar_t	*vid_gamma;
cvar_t	*vid_brightness;
cvar_t	*r_show_renderpass;
cvar_t	*r_show_light_scissors;
cvar_t	*r_show_normals;
cvar_t	*r_show_lightprobes;
*/

void InitializeConVars()
{
	// setup some engine cvars for custom rendering
	/* XashXT original renderer cvars
	r_extensions = CVAR_GET_POINTER("gl_allow_extensions");
	r_finish = CVAR_GET_POINTER("gl_finish");
	r_clear = CVAR_GET_POINTER("gl_clear");
	r_speeds = CVAR_GET_POINTER("r_speeds");
	r_test = CVAR_GET_POINTER("gl_test");


	r_novis = CVAR_GET_POINTER("r_novis");
	r_nocull = CVAR_GET_POINTER("r_nocull");
	r_nosort = CVAR_GET_POINTER("gl_nosort");
	r_lockpvs = CVAR_GET_POINTER("r_lockpvs");
	r_dynamic = CVAR_GET_POINTER("r_dynamic");
	r_lightmap = CVAR_GET_POINTER("r_lightmap");
	r_wireframe = CVAR_GET_POINTER("gl_wireframe");
	r_adjust_fov = CVAR_GET_POINTER("r_adjust_fov");
	gl_check_errors = CVAR_GET_POINTER("gl_check_errors");
	vid_gamma = CVAR_GET_POINTER("gamma");
	vid_brightness = CVAR_GET_POINTER("brightness");
	r_polyoffset = CVAR_GET_POINTER("gl_polyoffset");

	r_fullbright = CVAR_GET_POINTER("r_fullbright");
	r_drawentities = CVAR_GET_POINTER("r_drawentities");
	r_detailtextures = CVAR_GET_POINTER("r_detailtextures");
	r_lighting_modulate = CVAR_GET_POINTER("r_lighting_modulate");
	r_lightstyle_lerping = CVAR_GET_POINTER("cl_lightstyle_lerping");
	r_lighting_extended = CVAR_GET_POINTER("r_lighting_extended");


	r_allow_portals = CVAR_REGISTER("gl_allow_portals", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);
	r_allow_screens = CVAR_REGISTER("gl_allow_screens", "1", FCVAR_ARCHIVE | FCVAR_CLIENTDLL);
	gl_renderer = CVAR_REGISTER("gl_renderer", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	r_shadows = CVAR_REGISTER("r_shadows", "2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	r_allow_3dsky = CVAR_REGISTER("gl_allow_3dsky", "1", FCVAR_ARCHIVE);
	r_allow_mirrors = CVAR_REGISTER("gl_allow_mirrors", "1", FCVAR_ARCHIVE);
	r_recursion_depth = CVAR_REGISTER("gl_recursion_depth", "1", FCVAR_ARCHIVE);
	r_recursive_world_node = CVAR_REGISTER("gl_recursive_world_node", "0", FCVAR_ARCHIVE);

	r_grass = CVAR_REGISTER("r_grass", "1", FCVAR_ARCHIVE);
	r_grass_alpha = CVAR_REGISTER("r_grass_alpha", "0.5", FCVAR_ARCHIVE);
	r_grass_lighting = CVAR_REGISTER("r_grass_lighting", "1", FCVAR_ARCHIVE);
	r_grass_shadows = CVAR_REGISTER("r_grass_shadows", "1", FCVAR_ARCHIVE);
	r_grass_fade_start = CVAR_REGISTER("r_grass_fade_start", "2048", FCVAR_ARCHIVE);
	r_grass_fade_dist = CVAR_REGISTER("r_grass_fade_dist", "2048", FCVAR_ARCHIVE);
	r_grass_vbo = CVAR_REGISTER("r_grass_vbo", "-1", FCVAR_ARCHIVE);

	r_drawworld = CVAR_REGISTER("r_drawworld", "1", FCVAR_CHEAT);
	r_staticentities = CVAR_REGISTER("r_staticentities", "1", FCVAR_ARCHIVE);
	r_worldpvscull = CVAR_REGISTER("r_worldpvscull", "1", FCVAR_ARCHIVE);
	r_show_renderpass = CVAR_REGISTER("r_show_renderpass", "0", 0);
	r_show_light_scissors = CVAR_REGISTER("r_show_light_scissors", "0", 0);
	r_show_normals = CVAR_REGISTER("r_show_normals", "0", 0);
	r_show_lightprobes = CVAR_REGISTER("r_show_lightprobes", "0", 0);
	*/

	r_shadows = CVAR_REGISTER("r_shadows", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	r_shadow_split_weight = CVAR_REGISTER("r_pssm_split_weight", "0.9", FCVAR_ARCHIVE);
	r_pssm_show_split = CVAR_REGISTER("r_pssm_show_split", "0", 0); // debug feature
	r_scissor_glass_debug = CVAR_REGISTER("r_scissor_glass_debug", "0", FCVAR_ARCHIVE);
	r_scissor_light_debug = CVAR_REGISTER("r_scissor_light_debug", "0", FCVAR_ARCHIVE);
	r_recursion_depth = CVAR_REGISTER("gl_recursion_depth", "1", FCVAR_ARCHIVE);
	r_showlightmaps = CVAR_REGISTER("r_showlightmaps", "0", 0);

	// setup some engine cvars for custom rendering
	r_test = CVAR_GET_POINTER("gl_test");
	cv_nosort = CVAR_GET_POINTER("gl_nosort");
	r_overview = CVAR_GET_POINTER("dev_overview");
	r_fullbright = CVAR_GET_POINTER("r_fullbright");
	r_drawentities = CVAR_GET_POINTER("r_drawentities");
	gl_extensions = CVAR_GET_POINTER("gl_allow_extensions");
	r_finish = CVAR_GET_POINTER("gl_finish");
	cv_crosshair = CVAR_GET_POINTER("crosshair");
	r_lighting_ambient = CVAR_GET_POINTER("r_lighting_ambient");
	r_lighting_modulate = CVAR_GET_POINTER("r_lighting_modulate");
	r_lightstyle_lerping = CVAR_GET_POINTER("cl_lightstyle_lerping");
	r_lighting_extended = CVAR_GET_POINTER("r_lighting_extended");
	r_draw_beams = CVAR_GET_POINTER("cl_draw_beams");
	r_detailtextures = CVAR_GET_POINTER("r_detailtextures");
	r_speeds = CVAR_GET_POINTER("r_speeds");
	r_novis = CVAR_GET_POINTER("r_novis");
	r_nocull = CVAR_GET_POINTER("r_nocull");
	r_lockpvs = CVAR_GET_POINTER("r_lockpvs");
	r_wireframe = CVAR_GET_POINTER("gl_wireframe");
	r_lightmap = CVAR_GET_POINTER("r_lightmap");
	r_decals = CVAR_GET_POINTER("r_decals");
	r_clear = CVAR_GET_POINTER("gl_clear");
	r_dynamic = CVAR_GET_POINTER("r_dynamic");
	cv_gamma = CVAR_GET_POINTER("gamma");
	cv_brightness = CVAR_GET_POINTER("brightness");
	r_polyoffset = CVAR_GET_POINTER("gl_polyoffset");

	v_glows = CVAR_REGISTER("gl_glows", "1", FCVAR_ARCHIVE);

	r_dof = CVAR_REGISTER("r_dof", "1", FCVAR_ARCHIVE);
	r_dof_hold_time = CVAR_REGISTER("r_dof_hold_time", "0.2", FCVAR_ARCHIVE);
	r_dof_change_time = CVAR_REGISTER("r_dof_change_time", "0.8", FCVAR_ARCHIVE);
	r_dof_focal_length = CVAR_REGISTER("r_dof_focal_length", "600.0", FCVAR_ARCHIVE);
	r_dof_fstop = CVAR_REGISTER("r_dof_fstop", "8", FCVAR_ARCHIVE);
	r_dof_debug = CVAR_REGISTER("r_dof_debug", "0", FCVAR_ARCHIVE);

	cv_renderer = CVAR_REGISTER("gl_renderer", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	cv_bump = CVAR_REGISTER("gl_bump", "1", FCVAR_ARCHIVE);
	cv_bumpvecs = CVAR_REGISTER("bump_vecs", "0", 0);
	cv_specular = CVAR_REGISTER("gl_specular", "1", FCVAR_ARCHIVE);
	cv_dynamiclight = CVAR_REGISTER("gl_dynlight", "1", FCVAR_ARCHIVE);
	cv_parallax = CVAR_REGISTER("gl_parallax", "1", FCVAR_ARCHIVE);
	cv_shadow_offset = CVAR_REGISTER("gl_shadow_offset", "0.02", FCVAR_ARCHIVE);
	cv_deferred = CVAR_REGISTER("gl_deferred", "0", FCVAR_ARCHIVE);
	cv_deferred_full = CVAR_REGISTER("gl_deferred_fullres_shadows", "0", FCVAR_ARCHIVE);
	cv_deferred_maxlights = CVAR_REGISTER("gl_deferred_maxlights", "8", FCVAR_ARCHIVE | FCVAR_LATCH);
	cv_deferred_tracebmodels = CVAR_REGISTER("gl_deferred_tracebmodels", "0", FCVAR_ARCHIVE);
	cv_cubemaps = CVAR_REGISTER("gl_cubemap_reflection", "1", FCVAR_ARCHIVE);
	cv_cube_lod_bias = CVAR_REGISTER("gl_cube_lod_bias", "3", FCVAR_ARCHIVE);
	cv_water = CVAR_REGISTER("gl_waterblur", "1", FCVAR_ARCHIVE);
	cv_decalsdebug = CVAR_REGISTER("gl_decals_debug", "0", FCVAR_ARCHIVE);
	cv_realtime_puddles = CVAR_REGISTER("gl_realtime_puddles", "0", FCVAR_ARCHIVE);
	r_sunshadows = CVAR_REGISTER("gl_sun_shadows", "0", FCVAR_ARCHIVE);
	r_sun_allowed = CVAR_REGISTER("r_sun_allowed", "1", FCVAR_ARCHIVE);
	r_shadowmap_size = CVAR_REGISTER("gl_shadowmap_size", "512", FCVAR_ARCHIVE);
	r_occlusion_culling = CVAR_REGISTER("r_occlusion_culling", "0", FCVAR_ARCHIVE);
	r_show_lightprobes = CVAR_REGISTER("r_show_lightprobes", "0", FCVAR_ARCHIVE);
	r_show_cubemaps = CVAR_REGISTER("r_show_cubemaps", "0", FCVAR_ARCHIVE);
	r_show_viewleaf = CVAR_REGISTER("r_show_viewleaf", "0", FCVAR_ARCHIVE);
	cv_decals = CVAR_REGISTER("gl_decals", "1", FCVAR_ARCHIVE);
	r_lightstyles = CVAR_REGISTER("gl_lightstyles", "1", FCVAR_ARCHIVE);
	r_allow_mirrors = CVAR_REGISTER("gl_allow_mirrors", "1", FCVAR_ARCHIVE);
	r_studio_decals = CVAR_REGISTER("r_studio_decals", "32", FCVAR_ARCHIVE);
	cv_show_tbn = CVAR_REGISTER("gl_show_basis", "0", FCVAR_ARCHIVE);
	cv_brdf = CVAR_REGISTER("r_lighting_brdf", "1", FCVAR_ARCHIVE);

	r_grass = CVAR_REGISTER("r_grass", "1", FCVAR_ARCHIVE);
	r_grass_alpha = CVAR_REGISTER("r_grass_alpha", "0.5", FCVAR_ARCHIVE);
	r_grass_lighting = CVAR_REGISTER("r_grass_lighting", "1", FCVAR_ARCHIVE);
	r_grass_shadows = CVAR_REGISTER("r_grass_shadows", "1", FCVAR_ARCHIVE);
	r_grass_fade_start = CVAR_REGISTER("r_grass_fade_start", "1024", FCVAR_ARCHIVE);
	r_grass_fade_dist = CVAR_REGISTER("r_grass_fade_dist", "2048", FCVAR_ARCHIVE);
}

