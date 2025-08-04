/*
gl_backend.cpp - renderer backend routines
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <stdarg.h>
#include "hud.h"
#include "utils.h"
#include "const.h"
#include "com_model.h"
#include <stringlib.h>
#include "r_studioint.h"
#include <pm_movevars.h>
#include <pm_defs.h>
#include "ref_params.h"
#include "pmtrace.h"
#include "event_api.h"
#include "gl_local.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_shader.h"
#include "gl_cvars.h"
#include "gl_debug.h"
#include "imgui_manager.h"
#include "screenfade.h"
#include "shake.h"

/*
==============
R_Speeds_Printf

helper to print into r_speeds message
==============
*/
void R_Speeds_Printf( const char *msg, ... )
{
	va_list	argptr;
	char	text[2048];

	va_start( argptr, msg );
	Q_vsprintf( text, msg, argptr );
	va_end( argptr );

	Q_strncat( r_speeds_msg, text, sizeof( r_speeds_msg ));
}

/*
==============
GL_PrintStats

display renderer stats
==============
*/
static void GL_PrintStats( int params )
{
	GLint cur_avail_mem_kb = 0;
	GLint total_mem_kb = 0;

	if( r_speeds->value <= 0.0f || !FBitSet( params, RP_DRAW_WORLD ))
		return;

	msurface_t *surf = r_stats.debug_surface;

	switch( (int)r_speeds->value )
	{
	case 1:
		R_Speeds_Printf( "%3i solid faces\n%3i solid meshes\n", RI->frame.solid_faces.Count(), RI->frame.solid_meshes.Count() );
		R_Speeds_Printf( "%3i trans entries\n%3i grass faces\n", RI->frame.trans_list.Count(), RI->frame.grass_list.Count() );
		R_Speeds_Printf( "%3i mirror faces\n%3i tess verts\n", RI->frame.num_subview_faces, RI->frame.primverts.Count() );
		break;
	case 2:
		R_Speeds_Printf( "DIP count %3i\nShader bind %3i\n", r_stats.num_flushes_total, r_stats.num_shader_binds );
		R_Speeds_Printf( "Frame total tris %3i\n", r_stats.c_total_tris );
		R_Speeds_Printf( "Total GLSL shaders %3i\n", num_glsl_programs - 1 );
		R_Speeds_Printf( "\nSolid brush drawcall flushes:\n  shader %3i\n  material %3i\n  entity %3i\n  cubemap %3i\n  mirror %3i\n  lightmap %3i",
			r_stats.solid_brush_list_flushes.num_flushes_shader, 
			r_stats.solid_brush_list_flushes.num_flushes_material,
			r_stats.solid_brush_list_flushes.num_flushes_entity,
			r_stats.solid_brush_list_flushes.num_flushes_cubemap,
			r_stats.solid_brush_list_flushes.num_flushes_mirrortex,
			r_stats.solid_brush_list_flushes.num_flushes_lightmap
		);
		break;
	case 3:
		Q_snprintf(r_speeds_msg, sizeof(r_speeds_msg),
			"%3i mirrors\n%3i portals\n%3i screens\n%3i shadow passes\n%3i 3dsky passes\n%3i screencopy\n%3i occluded",
			r_stats.c_mirror_passes,
			r_stats.c_portal_passes,
			r_stats.c_screen_passes,
			r_stats.c_shadow_passes,
			r_stats.c_sky_passes,
			r_stats.c_screen_copy, 
			r_stats.c_occlusion_culled 
		);
		break;
	case 4:
		if( glConfig.hardware_type == GLHW_NVIDIA )
		{
			pglGetIntegerv( GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, &total_mem_kb );
			pglGetIntegerv( GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &cur_avail_mem_kb );
		}
		R_Speeds_Printf( "TEX used memory %s\n", Q_memprint( RENDER_GET_PARM( PARM_TEX_MEMORY, 0 )));
		R_Speeds_Printf( "VBO used memory %s\n", Q_memprint( tr.total_vbo_memory ));
		R_Speeds_Printf( "GPU used memory %s\n", Q_memprint(( total_mem_kb - cur_avail_mem_kb ) * 1024 ));
		break;	
	case 5: {
		// draw hierarchy map of recursion calls
		int totalPasses = r_stats.c_mirror_passes + r_stats.c_screen_passes + r_stats.c_portal_passes + r_stats.c_sky_passes;
		R_Speeds_Printf("total %3i subview passes\n", totalPasses);
		R_Speeds_Printf("%s", r_depth_msg);
		break;
	}
	case 6:
		if( r_stats.debug_surface != NULL )
		{
			glsl_program_t *cur = surf->info->forwardScene[0].GetShader();
			R_Speeds_Printf( "Surf: ^1%s^7\n", surf->texinfo->texture->name );
			R_Speeds_Printf( "Shader: ^3#%i %s^7\n", surf->info->forwardScene[0].GetHandle(), cur->name );
			R_Speeds_Printf( "List Options:\n" ); 
			R_Speeds_Printf( "%s\n", GL_PretifyListOptions( cur->options, true ));
		}
		break;
	case 7:
		R_Speeds_Printf( "%3i world lights (from %d lights)\n", r_stats.c_worldlights, world->numworldlights );
		break;
	}
}

/*
==============
GL_ComputeScreenRays
==============
*/
void GL_ComputeScreenRays( void )
{
	vec3_t	axis[3];
	float	tx, ty;
	int	i;

	// compute the world-space rays to the far plane corners
	tx = tan( DEG2RAD( RI->view.fov_x * 0.5f ));
	ty = tan( DEG2RAD( RI->view.fov_y * 0.5f ));

	for( i = 0; i < 3; i++ )
	{
		axis[0][i] = RI->view.matrix[0][i];
		axis[1][i] = RI->view.matrix[1][i] * tx;
		axis[2][i] = RI->view.matrix[2][i] * ty;

		// counter-clockwise order
		tr.screen_normals[0][i] = axis[0][i] + axis[1][i] + axis[2][i];	// top left
		tr.screen_normals[1][i] = axis[0][i] + axis[1][i] - axis[2][i];	// bottom left
		tr.screen_normals[2][i] = axis[0][i] - axis[1][i] - axis[2][i];	// bottom right
		tr.screen_normals[3][i] = axis[0][i] - axis[1][i] + axis[2][i];	// top right
	}
}

/*
==============
GL_ComputeSunParams
==============
*/
void GL_ComputeSunParams( const Vector &skyVector )
{
	// calculate ambient and diffuse light color
	float	sunPos = -skyVector.z;
	float	sunBrightness = 0.75f;
	float	refractionFactor = 1.0f - sqrt(Q_max(sunPos, 0.0f));
	vec3_t	sunColor = vec3_t(1.0f, 1.0f, 1.0f) - vec3_t(0.0f, 0.3f, 0.6f) * refractionFactor;
	float	ambientIntensity = 0.0025f + sunBrightness * Q_min(1.0f, Q_max(0.0f, (0.375f + sunPos) / 0.25f));
	float	diffuseIntensity = sunBrightness * Q_min(1.0f, Q_max(0.0f, (0.03125f + sunPos) / 0.0625f));

	float ambientFactor = RemapVal(tr.ambientFactor, AMBIENT_EPSILON, 1.0f, 0.0f, 1.0f);
	tr.sun_ambient = Q_max(ambientIntensity * ambientFactor, 0.0f);
	tr.sun_ambient = bound(0.0f, tr.sun_ambient, tr.ambientFactor);
	tr.sun_diffuse = sunColor * diffuseIntensity;
	tr.sun_refract = refractionFactor;
	tr.sky_normal = skyVector;
}

static void R_RenderScreenQuad()
{
	GL_DEBUG_SCOPE();
	// copy depth from HDR framebuffer to screen framebuffer
	pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO_MAIN);
	pglBindFramebuffer(GL_READ_FRAMEBUFFER, tr.screen_hdr_fbo->id);
	pglBlitFramebuffer(0, 0, glState.width, glState.height, 0, 0, glState.width, glState.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	GL_BindFBO(FBO_MAIN);
	GL_Setup2D();
	GL_Bind(GL_TEXTURE0, tr.screen_hdr_fbo->colortarget[0]);
	RenderFSQ(glState.width, glState.height);
	GL_Bind(GL_TEXTURE0, TextureHandle::Null());
}

/*
==============
GL_BackendStartFrame
==============
*/
bool GL_BackendStartFrame( ref_viewpass_t *rvp, RefParams params )
{
	ZoneScoped;

	bool allow_dynamic_sun = false;
	static float cached_lighting = 0.0f;
	static float shadowmap_size = 0.0f;
	static int waterlevel_old;
	float levelTime = r_sun_daytime->value;
	float blurAmount = 0.f; // gHUD.m_flBlurAmount;

	r_speeds_msg[0] = r_depth_msg[0] = '\0';
	tr.fCustomRendering = false;
	tr.fGamePaused = RENDER_GET_PARM( PARAM_GAMEPAUSED, 0 );
	memset( &r_stats, 0, sizeof( r_stats ));
	tr.sunlight = NULL;

	if( r_buildstats.total_buildtime > 0.0 && RENDER_GET_PARM( PARM_CLIENT_ACTIVE, 0 ))
	{
		// display time to building some stuff: lighting, VBO, shaders etc
		if (r_buildstats.compile_shaders > 0.0) {
			ALERT(at_aiconsole, "^3GL_BackendStartFrame: ^7shaders compiled in %.1f msec\n", r_buildstats.compile_shaders * 1000.0);
		}
		if (r_buildstats.create_buffer_object > 0.0) 
		{
			double time = (r_buildstats.create_buffer_object - r_buildstats.compile_shaders) * 1000.0;
			ALERT(at_aiconsole, "^3GL_BackendStartFrame: ^7VBO created in %1.f msec\n", time);
		}
		if (r_buildstats.create_light_cache > 0.0) {
			ALERT(at_aiconsole, "^3GL_BackendStartFrame: ^7VL cache created in %.1f msec\n", r_buildstats.create_light_cache * 1000.0);
		}
		if (r_buildstats.create_buffer_object > 0.0 || r_buildstats.create_light_cache > 0.0) {
			ALERT(at_aiconsole, "^3GL_BackendStartFrame: ^7total building time = %g seconds\n", r_buildstats.total_buildtime);
		}
		memset( &r_buildstats, 0, sizeof( r_buildstats ));
	}

	if( !g_fRenderInitialized )
		return 0;

	if( !FBitSet( params, RP_DRAW_WORLD ))
		GL_Setup3D();

	if( CVAR_TO_BOOL( r_finish ) && FBitSet( params, RP_DRAW_WORLD ))
		pglFinish();

	// setup light factors
	tr.ambientFactor = bound( AMBIENT_EPSILON, r_lighting_ambient->value, 1.0f );
	float cachedFactor = bound( tr.ambientFactor, 1.0f, 1.0f );

	if( cachedFactor != tr.diffuseFactor )
	{
		tr.diffuseFactor = cachedFactor;
		R_StudioClearLightCache();
	}

	if( cached_lighting != r_lighting_extended->value )
	{
		cached_lighting = r_lighting_extended->value;
		R_StudioClearLightCache();
	}

	if( shadowmap_size != r_shadowmap_size->value )
	{
		shadowmap_size = r_shadowmap_size->value;
		R_InitShadowTextures();
	}

	// FIXME: allow player overview for custom renderer
	if( !FBitSet( params, RP_DRAW_WORLD ))
		return 0;

	// use engine renderer
	if( cv_renderer->value == 0 )
	{
		glState.depthmin = -1.0f;
		glState.depthmax = -1.0f;
		glState.depthmask = -1;
		return 0;
	}

	// keep world in actual state
	GET_ENTITY( 0 )->curstate.messagenum = r_currentMessageNum;

	if (levelTime != -1.0f && r_sun_allowed->value == 1.0f)
	{
		Vector	skyVector;
		matrix4x4	a;

		float	time = levelTime;
		float	timeAng = ((( time + 12.0f ) / 24.0f ) * M_PI * 2.0f );
		float	longitude = RAD2DEG( 0.5f * M_PI ) - 90.0f;

		a.CreateRotate( RAD2DEG( timeAng ) - 180.0f, 1.0, 0.5f, 0.0f );
		a.ConcatRotate( longitude, -0.5f, 1.0f, 0.0f );
		skyVector = a.VectorRotate( Vector( 0.0f, 0.0f, 1.0f )).Normalize();
		GL_ComputeSunParams( skyVector );
		allow_dynamic_sun = true;

		gEngfuncs.Con_NPrintf(7, "Day Time: %i:%02.f\n", (int)time, (time - (int)time) * 59.0f);
		gEngfuncs.Con_NPrintf(8, "Sun Ambient: %f\n", tr.sun_ambient);
		gEngfuncs.Con_NPrintf(9, "Ambient Factor %f\n", tr.ambientFactor);
	}
	else
	{
		Vector	skyVector;

		skyVector.x = tr.movevars->skyvec_x;
		skyVector.y = tr.movevars->skyvec_y;
		skyVector.z = tr.movevars->skyvec_z;
		skyVector = skyVector.Normalize();

		GL_ComputeSunParams( skyVector );

		// hidden test mode
		if( r_sun_allowed->value == 2.0 )
			allow_dynamic_sun = true;
	}

	tr.sky_ambient.x = tr.movevars->skycolor_r;
	tr.sky_ambient.y = tr.movevars->skycolor_g;
	tr.sky_ambient.z = tr.movevars->skycolor_b;

	// NOTE: sunlight must be added first in list
	if( FBitSet( world->features, WORLD_HAS_SKYBOX ) && allow_dynamic_sun )
	{
		tr.sunlight = CL_AllocDlight( SUNLIGHT_KEY ); // alloc sun source
		R_SetupLightParams( tr.sunlight, g_vecZero, g_vecZero, 32768.0f, 90.0f, LIGHT_DIRECTIONAL );
		R_SetupLightTexture( tr.sunlight, tr.whiteTexture );
		tr.sunlight->die = GET_CLIENT_TIME();
		tr.sunlight->color = tr.sun_diffuse;
		tr.sun_light_enabled = true;
	}

	tr.gravity = tr.movevars->gravity;
	tr.farclip = tr.movevars->zmax;

	if( tr.farclip == 0.0f || worldmodel == NULL )
		tr.farclip = 4096.0f;

	tr.cached_vieworigin = rvp->vieworigin;
	tr.cached_viewangles = rvp->viewangles;
	tr.waterlevel = tr.viewparams.waterlevel;

	if(( tr.waterlevel >= 3 ) != ( waterlevel_old >= 3 ))
	{
		waterlevel_old = tr.waterlevel;
		tr.glsl_valid_sequence++; // now all uber-shaders are invalidate and possible need for recompile
		tr.params_changed = true;
	}

	if( tr.waterlevel >= 3 )
	{
		int waterent = WATER_ENTITY( RI->view.origin );

		// FIXME: how to allow fog on a world water?
		if( waterent > 0 && waterent < tr.viewparams.max_entities )
			tr.waterentity = GET_ENTITY( waterent );
		else tr.waterentity = NULL;
	}
	else tr.waterentity = NULL;

	R_GrassSetupFrame();
	// R_UpdateFogParameters();

	// apply the underwater warp
	if( tr.waterlevel >= 3 )
	{
		float f = sin( tr.time * 0.4f * ( M_PI * 1.7f ));
		rvp->fov_x += f;
		rvp->fov_y -= f;
	}

	// apply the blur effect
	if (blurAmount > 0.0f )
	{
		float f = sin( tr.time * 0.4 * ( M_PI * 1.7f )) * blurAmount * 20.0f;
		rvp->fov_x += f;
		rvp->fov_y += f;

		screenfade_t fade;

		f = ( sin( tr.time * 0.5f * ( M_PI * 1.7f )) * 127 ) + 128;
		fade.fadeFlags = FFADE_STAYOUT;
		fade.fader = fade.fadeg = fade.fadeb = 0;
		fade.fadeReset = tr.time + 0.1f;
		fade.fadeEnd = tr.time + 0.1f;
		fade.fadealpha = bound( 0, (byte)f, blurAmount * 255 );

		gEngfuncs.pfnSetScreenFade( &fade );
	}

	Mod_ResortFaces();
	GL_LoadAndRebuildCubemaps( params );
	tr.fCustomRendering = true;
	r_stats.debug_surface = NULL;

	if( r_speeds->value == 6.0f )
	{
		Vector vecSrc = RI->view.origin;
		Vector vecEnd = vecSrc + GetVForward() * RI->view.farClip;
		pmtrace_t trace;

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( (float *)&vecSrc, (float *)&vecEnd, PM_NORMAL, -1, &trace );
		r_stats.debug_surface = gEngfuncs.pEventAPI->EV_TraceSurface( trace.ent, (float *)&vecSrc, (float *)&vecEnd );
	}

	// setup light animation tables
	R_AnimateLight();

	return 1;
}

/*
==============
GL_BackendEndFrame
==============
*/
void GL_BackendEndFrame( ref_viewpass_t *rvp, RefParams params )
{
	GL_DEBUG_SCOPE();
	ZoneScoped;

	mstudiolight_t	light;
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);
	bool multisampling = CVAR_GET_FLOAT("gl_msaa") > 0.0f;

	tr.frametime = tr.saved_frametime;

	// go into 2D mode (in case we draw PlayerSetup between two 2d calls)
	if( !FBitSet( params, RP_DRAW_WORLD ))
		GL_Setup2D();

	if( tr.show_uniforms_peak )
	{
		ALERT( at_aiconsole, "peak used uniforms: %i\n", glConfig.peak_used_uniforms );
		tr.show_uniforms_peak = false;
	}

	DrawLightProbes();			// 3D

	DrawCubeMaps();			// 3D

	DrawViewLeaf();			// 3D

	DrawWireFrame();			// 3D

	DrawTangentSpaces();		// 3D

	DrawWirePoly( r_stats.debug_surface );	// 3D

	DBG_DrawLightFrustum();		// 3D

	R_PushRefState();
	RI->params = params;
	RI->view.fov_x = rvp->fov_x;
	RI->view.fov_y = rvp->fov_y; 

	R_DrawViewModel();		// 3D

	if (hdr_rendering)
	{
		if (multisampling)
		{
			// copy image from multisampling framebuffer to screen framebuffer
			pglBindFramebuffer(GL_DRAW_FRAMEBUFFER, tr.screen_hdr_fbo->id);
			pglBindFramebuffer(GL_READ_FRAMEBUFFER, tr.screen_multisample_fbo->id);
			pglBlitFramebuffer(0, 0, glState.width, glState.height, 0, 0, glState.width, glState.height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		}
		GL_BindFBO(tr.screen_hdr_fbo->id);
		RenderAverageLuminance();
	}

	RenderDOF();				// 2D
	RenderNerveGasBlur();		// 2D
	RenderUnderwaterBlur();		// 2D
	if (hdr_rendering)
	{
		RenderBloom();
		RenderTonemap();		// should be last step in HDR pipeline!!!
	}

	R_RenderDebugStudioList( true ); // 3D
	RenderSunShafts();			// 2D
	RenderPostprocessing();		// 2D
	R_ShowLightMaps();			// 2D
	
	if (hdr_rendering) {
		R_RenderScreenQuad();
	}

	GL_CleanupDrawState();
	g_ImGuiManager.NewFrame();

	// restore state for correct rendering other stuff
	GL_Setup3D();
	GL_Blend(GL_TRUE);
	GL_CleanupDrawState();
	R_PopRefState();

	// fill the r_speeds message
	GL_PrintStats( params );

	R_UnloadFarGrass();
	tr.params_changed = false;
	tr.realframecount++;

}

/*
===============
R_ScreenToWorld

transform point from 3D position to ortho
===============
*/
bool R_WorldToScreen( const Vector &point, Vector &screen )
{
	matrix4x4	worldToScreen;
	bool	behind;
	float	w;

	worldToScreen = RI->view.worldProjectionMatrix;

	screen[0] = worldToScreen[0][0] * point[0] + worldToScreen[1][0] * point[1] + worldToScreen[2][0] * point[2] + worldToScreen[3][0];
	screen[1] = worldToScreen[0][1] * point[0] + worldToScreen[1][1] * point[1] + worldToScreen[2][1] * point[2] + worldToScreen[3][1];
	w = worldToScreen[0][3] * point[0] + worldToScreen[1][3] * point[1] + worldToScreen[2][3] * point[2] + worldToScreen[3][3];
	screen[2] = 0.0f; // just so we have something valid here

	if( w < 0.001f )
	{
		behind = true;
		screen[0] *= 100000;
		screen[1] *= 100000;
	}
	else
	{
		float invw = 1.0f / w;
		behind = false;
		screen[0] *= invw;
		screen[1] *= invw;
	}

	return behind;
}

/*
===============
R_ScreenToWorld

transform point from ortho to 3D position
===============
*/
void R_ScreenToWorld( const Vector &screen, Vector &point )
{
	matrix4x4	screenToWorld;
	Vector	temp;
	float	w;

	screenToWorld = RI->view.worldProjectionMatrix.InvertFull();

	temp[0] = 2.0f * (screen[0] - RI->view.port[0]) / RI->view.port[2] - 1;
	temp[1] = -2.0f * (screen[1] - RI->view.port[1]) / RI->view.port[3] + 1;
	temp[2] = 0.0f; // just so we have something valid here

	point[0] = temp[0] * screenToWorld[0][0] + temp[1] * screenToWorld[0][1] + temp[2] * screenToWorld[0][2] + screenToWorld[0][3];
	point[1] = temp[0] * screenToWorld[1][0] + temp[1] * screenToWorld[1][1] + temp[2] * screenToWorld[1][2] + screenToWorld[1][3];
	point[2] = temp[0] * screenToWorld[2][0] + temp[1] * screenToWorld[2][1] + temp[2] * screenToWorld[2][2] + screenToWorld[2][3];
	w = temp[0] * screenToWorld[3][0] + temp[1] * screenToWorld[3][1] + temp[2] * screenToWorld[3][2] + screenToWorld[3][3];
	if( w ) point *= ( 1.0f / w );
}

/*
=============
R_GetSpriteTexture

=============
*/
TextureHandle R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if (!m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data) {
		return TextureHandle::Null();
	}
	return TextureHandle(R_GetSpriteFrame( m_pSpriteModel, frame ));
}

void R_RenderQuadPrimitive( CSolidEntry *entry )
{
	if( entry->m_bDrawType != DRAWTYPE_QUAD )
		return;

	GL_DEBUG_SCOPE();
	GL_CleanupDrawState();

	// select properly rendermode
	switch( entry->m_iRenderMode )
	{
	case kRenderTransAlpha:
		GL_AlphaTest( GL_TRUE );
		pglAlphaFunc( GL_GREATER, 0.0f );
	case kRenderTransColor:
	case kRenderTransTexture:
		GL_Blend( GL_TRUE );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		GL_DepthMask( GL_FALSE );
		break;
	case kRenderGlow:
		pglDisable( GL_DEPTH_TEST );
	case kRenderTransAdd:
		GL_Blend( GL_TRUE );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		GL_DepthMask( GL_FALSE );
		break;
	case kRenderNormal:
	default:
		GL_DepthMask( GL_TRUE );
		GL_Blend( GL_FALSE );
		break;
	}

	int	r, g, b, a;

	// draw the particle
	GL_Bind(GL_TEXTURE0, entry->m_hTexture);
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	UnpackRGBA( r, g, b, a, entry->m_iColor );
	pglColor4ub( r, g, b, a );

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		pglVertex3fv( RI->frame.primverts[entry->m_iStartVertex+0] );

		pglTexCoord2f( 0.0f, 0.0f );
		pglVertex3fv( RI->frame.primverts[entry->m_iStartVertex+1] );

		pglTexCoord2f( 1.0f, 0.0f );
		pglVertex3fv( RI->frame.primverts[entry->m_iStartVertex+2] );

		pglTexCoord2f( 1.0f, 1.0f );
		pglVertex3fv( RI->frame.primverts[entry->m_iStartVertex+3] );
	pglEnd();

	if( entry->m_iRenderMode == kRenderGlow )
		pglEnable( GL_DEPTH_TEST );

	if( entry->m_iRenderMode == kRenderTransAlpha )
		GL_AlphaTest( GL_FALSE );
}

int R_AllocFrameBuffer(const CViewport &viewport)
{
	int i = tr.num_framebuffers;

	if( i >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_AllocateFrameBuffer: FBO limit exceeded!\n" );
		return -1; // disable
	}

	gl_fbo_t *fbo = &tr.frame_buffers[i];
	tr.num_framebuffers++;

	if( fbo->init )
	{
		ALERT( at_warning, "R_AllocFrameBuffer: buffer %i already initialized\n", i );
		return i;
	}

	// create a depth-buffer
	pglGenRenderbuffers( 1, &fbo->renderbuffer );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglRenderbufferStorage( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, viewport.GetWidth(), viewport.GetHeight() );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, 0 );

	// create frame-buffer
	pglGenFramebuffers( 1, &fbo->framebuffer );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
	pglFramebufferRenderbuffer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	fbo->init = true;

	return i;
}

void R_FreeFrameBuffer( int buffer )
{
	if( buffer < 0 || buffer >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_FreeFrameBuffer: invalid buffer enum %i\n", buffer );
		return;
	}

	gl_fbo_t *fbo = &tr.frame_buffers[buffer];

	pglDeleteRenderbuffers( 1, &fbo->renderbuffer );
	pglDeleteFramebuffers( 1, &fbo->framebuffer );
	memset( fbo, 0, sizeof( *fbo ));
}

void GL_BindFrameBuffer( int buffer, TextureHandle texture )
{
	gl_fbo_t *fbo = NULL;

	if( buffer >= 0 && buffer < MAX_FRAMEBUFFERS )
		fbo = &tr.frame_buffers[buffer];

	if( !fbo )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = buffer;
		return;
	}

	// at this point FBO index is always positive
	if((unsigned int)glState.frameBuffer != fbo->framebuffer )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
		glState.frameBuffer = fbo->framebuffer;
	}		

	if( fbo->texture != texture )
	{
		// change texture attachment
		GLuint texnum = texture.GetGlHandle();
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texnum, 0 );
		fbo->texture = texture;
	}
}

/*
==============
GL_BindFBO
==============
*/
void GL_BindFBO( GLuint buffer )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return;

	if( glState.frameBuffer == buffer )
		return;

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, buffer );
	glState.frameBuffer = buffer;
}

/*
==================
GL_BindDrawbuffer
==================
*/
void GL_BindDrawbuffer( gl_drawbuffer_t *drawbuffer )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return;

	if( !drawbuffer )
	{
		if( !glState.frameBuffer )
			return;

		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = 0;
		return;
	}

	if( glState.frameBuffer == drawbuffer->id )
		return;

	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, drawbuffer->id );
	glState.frameBuffer = drawbuffer->id;
}

/*
==============
GL_DepthRange
==============
*/
void GL_DepthRange( GLfloat depthmin, GLfloat depthmax )
{
	if( glState.depthmin == depthmin && glState.depthmax == depthmax )
		return;

	pglDepthRange( depthmin, depthmax );
	glState.depthmin = depthmin;
	glState.depthmax = depthmax;
}

/*
==============
GL_DepthMask
==============
*/
void GL_DepthMask( GLint enable )
{
// it's won't work anyway
//	if( glState.depthmask == enable )
//		return;

	glState.depthmask = enable;
	pglDepthMask( enable );
}

/*
==============
GL_AlphaTest
==============
*/
void GL_AlphaTest( GLint enable )
{
	if( pglIsEnabled( GL_ALPHA_TEST ) == enable )
		return;

	if( enable ) pglEnable( GL_ALPHA_TEST );
	else pglDisable( GL_ALPHA_TEST );
}

/*
==============
GL_AlphaToCoverage
==============
*/
void GL_AlphaToCoverage(bool enable)
{
	// TODO store state locally to avoid GL-calls to get current state
	if (pglIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB) == enable) {
		return;
	}

	if (enable && CVAR_TO_BOOL(gl_alpha2coverage)) {
		pglEnable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);
	}
	else {
		pglDisable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);
	}
}

/*
==============
GL_UsingAlphaToCoverage
==============
*/
bool GL_UsingAlphaToCoverage()
{
	return CVAR_TO_BOOL(gl_alpha2coverage) && CVAR_GET_FLOAT("gl_msaa") > 0.0f;
}

/*
==============
GL_DepthTest
==============
*/
void GL_DepthTest( GLint enable )
{
	if( pglIsEnabled( GL_DEPTH_TEST ) == enable )
		return;

	if( enable ) pglEnable( GL_DEPTH_TEST );
	else pglDisable( GL_DEPTH_TEST );
}

/*
==============
GL_Blend
==============
*/
void GL_Blend( GLint enable )
{
	if( pglIsEnabled( GL_BLEND ) == enable )
		return;

	if( enable ) pglEnable( GL_BLEND );
	else pglDisable( GL_BLEND );
}

/*
==============
GL_CleanupAllTextureUnits
==============
*/
void GL_CleanupAllTextureUnits( void )
{
	// force to cleanup all the units
	GL_SelectTexture( glConfig.max_texture_units - 1 );
	GL_CleanUpTextureUnits( 0 );
}

/*
==============
GL_CleanupDrawState
==============
*/
void GL_CleanupDrawState( void )
{
	GL_CleanupAllTextureUnits();
	pglBindVertexArray( GL_FALSE );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	GL_BindShader( NULL );
}

/*
==============
GL_CheckVertexArrayBinding
==============
*/
void GL_CheckVertexArrayBinding( void )
{
	int	current_vao = 0;

	pglGetIntegerv( GL_VERTEX_ARRAY_BINDING, (int *)&current_vao );
	if( !current_vao ) return;

	ALERT( at_error, "detected active VAO %d while uploading vertex buffer!\n", current_vao );
	pglBindVertexArray( GL_FALSE );
}

/*
==============
GL_DisableAllTexGens
==============
*/
void GL_DisableAllTexGens( void )
{
	GL_TexGen( GL_S, 0 );
	GL_TexGen( GL_T, 0 );
	GL_TexGen( GL_R, 0 );
	GL_TexGen( GL_Q, 0 );
}

/*
=================
GL_ClipPlane
=================
*/
void GL_ClipPlane( bool enable )
{
	// if cliplane was not set - do nothing
	if( !FBitSet( RI->params, RP_CLIPPLANE ))
		return;

	if( glConfig.hardware_type == GLHW_RADEON )
		return; // ATI doesn't need clip planes

	if( enable )
	{
		GLdouble	clip[4];
		mplane_t	*p = &RI->clipPlane;

		clip[0] = p->normal[0];
		clip[1] = p->normal[1];
		clip[2] = p->normal[2];
		clip[3] = -p->dist;

		pglClipPlane( GL_CLIP_PLANE0, clip );
		pglEnable( GL_CLIP_PLANE0 );
	}
	else
	{
		pglDisable( GL_CLIP_PLANE0 );
	}
}


/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( GLenum front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

/*
=================
GL_LoadTexMatrix
=================
*/
void GL_LoadTexMatrix( const matrix4x4 &source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	GL_LoadTextureMatrix( dest );
}

/*
=================
GL_LoadMatrix
=================
*/
void GL_LoadMatrix( const matrix4x4 &source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	pglLoadMatrixf( dest );
}

/*
=================
GL_Setup2D
=================
*/
void GL_Setup2D( void )
{
	// set up full screen workspace
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -99999, 99999 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglDisable( GL_DEPTH_TEST );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_Blend( GL_FALSE );

	GL_Cull( GL_FRONT );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglViewport( 0, 0, glState.width, glState.height );
}

/*
=================
GL_Setup3D
=================
*/
void GL_Setup3D( void )
{
	RI->glstate.viewport.SetAsCurrent();

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI->glstate.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI->glstate.modelviewMatrix );

	pglEnable( GL_DEPTH_TEST );
	GL_DepthMask( GL_TRUE );
	GL_Cull( GL_FRONT );
}

void CompressNormalizedVector( char outVec[3], const Vector &inVec )
{
	const byte *bestFitTexture = GET_TEXTURE_DATA( tr.normalsFitting );
	Vector vNorm = inVec.Normalize(); // should be normalized

	if( bestFitTexture != NULL )
	{
		Vector uNorm = vNorm.Abs();		// get unsigned normal
		float maxNAbs = uNorm.MaxCoord();	// get the main axis for cubemap lookup

		// get texture dimensions (probably always equal 512)
		uint width = tr.normalsFitting.GetWidth();
		uint height = tr.normalsFitting.GetHeight();

		// get texture coordinates in a collapsed cubemap
		float u = (uNorm.z < maxNAbs ? (uNorm.y < maxNAbs ? uNorm.y : uNorm.x) : uNorm.x);
		float v = (uNorm.z < maxNAbs ? (uNorm.y < maxNAbs ? uNorm.z : uNorm.z) : uNorm.y);
		vNorm /= maxNAbs; // fit normal into the edge of unit cube

		if (u < v) {
			float temp = u;
			u = v;
			v = temp;
		}

		if( u != 0.0f ) 
			v /= u;

		uint x = uint( u * (float)width );
		uint y = uint( v * (float)height );

		// look-up fitting length and scale the normal to get the best fit
		float fFittingScale = (float)bestFitTexture[(y * width + x)] * (1.0f / 255.0f);
		// scale the normal to get the best fit
		vNorm *= fFittingScale;
	}

	outVec[0] = FloatToChar( vNorm.x );
	outVec[1] = FloatToChar( vNorm.y );
	outVec[2] = FloatToChar( vNorm.z );
}

int GL_TextureMipCount(int width, int height)
{
	return 1 + floor(log2(Q_max(width, height)));
}
