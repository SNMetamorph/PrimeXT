//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "gl_debug.h"
#include "hud.h"
#include "utils.h"
#include "const.h"
#include "com_model.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "triangleapi.h"
#include "gl_local.h"
#include "stringlib.h"
#include "gl_world.h"
#include "gl_shader.h"
#include "vertex_fmt.h"
#include "gl_cvars.h"
#include "mathlib.h"
#include "visualizer/debug_visualizer.h"
#include "gl_debug_overlay_2d.h"

void GL_GpuMemUsage_f( void )
{
	GLint cur_avail_mem_kb = 0;
	GLint total_mem_kb = 0;

	if( glConfig.hardware_type == GLHW_NVIDIA )
	{
		pglGetIntegerv( GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, &total_mem_kb );
		pglGetIntegerv( GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &cur_avail_mem_kb );
	}

	Msg( "TEX used memory %s\n", Q_memprint( RENDER_GET_PARM( PARM_TEX_MEMORY, 0 )));
	Msg( "VBO used memory %s\n", Q_memprint( tr.total_vbo_memory ));
	Msg( "GPU used memory %s\n", Q_memprint(( total_mem_kb - cur_avail_mem_kb ) * 1024 ));
}

static const char *GL_GetErrorString(int errorCode)
{
	switch (errorCode)
	{
		case GL_STACK_OVERFLOW:
			return "GL_STACK_OVERFLOW";
		case GL_STACK_UNDERFLOW:
			return "GL_STACK_UNDERFLOW";
		case GL_INVALID_ENUM:
			return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:
			return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:
			return "GL_INVALID_OPERATION";
		case GL_OUT_OF_MEMORY:
			return "GL_OUT_OF_MEMORY";
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			return "GL_INVALID_FRAMEBUFFER_OPERATION";
		default:
			return "UNKNOWN ERROR";
	}
}

void GL_CheckForErrorsInternal(const char *filename, int line)
{
	for (int code = pglGetError(); code != GL_NO_ERROR; code = pglGetError())
	{
		ALERT(at_error, "%s (at %s:%i)\n", GL_GetErrorString(code), filename, line);
	}
}

void DBG_PrintVertexVBOSizes( void )
{
	if( developer_level < at_aiconsole )
		return;

	ALERT( at_console, "sizeof( bvert_v0_gl21_t ) == %d bytes\n", sizeof( bvert_v0_gl21_t ));
	ALERT( at_console, "sizeof( bvert_v0_gl30_t ) == %d bytes\n", sizeof( bvert_v0_gl30_t ));
	ALERT( at_console, "sizeof( gvert_v0_gl21_t ) == %d bytes\n", sizeof( gvert_v0_gl21_t ));
	ALERT( at_console, "sizeof( gvert_v0_gl30_t ) == %d bytes\n", sizeof( gvert_v0_gl30_t ));
	ALERT( at_console, "sizeof( gvert_v1_gl21_t ) == %d bytes\n", sizeof( gvert_v1_gl21_t ));
	ALERT( at_console, "sizeof( gvert_v1_gl30_t ) == %d bytes\n", sizeof( gvert_v1_gl30_t ));
	ALERT( at_console, "sizeof( svert_v0_gl21_t ) == %d bytes\n", sizeof( svert_v0_gl21_t ));
	ALERT( at_console, "sizeof( svert_v0_gl30_t ) == %d bytes\n", sizeof( svert_v0_gl30_t ));
	ALERT( at_console, "sizeof( svert_v1_gl21_t ) == %d bytes\n", sizeof( svert_v1_gl21_t ));
	ALERT( at_console, "sizeof( svert_v1_gl30_t ) == %d bytes\n", sizeof( svert_v1_gl30_t ));
	ALERT( at_console, "sizeof( svert_v2_gl21_t ) == %d bytes\n", sizeof( svert_v2_gl21_t ));
	ALERT( at_console, "sizeof( svert_v2_gl30_t ) == %d bytes\n", sizeof( svert_v2_gl30_t ));
	ALERT( at_console, "sizeof( svert_v3_gl21_t ) == %d bytes\n", sizeof( svert_v3_gl21_t ));
	ALERT( at_console, "sizeof( svert_v3_gl30_t ) == %d bytes\n", sizeof( svert_v3_gl30_t ));
	ALERT( at_console, "sizeof( svert_v4_gl21_t ) == %d bytes\n", sizeof( svert_v4_gl21_t ));
	ALERT( at_console, "sizeof( svert_v4_gl30_t ) == %d bytes\n", sizeof( svert_v4_gl30_t ));
	ALERT( at_console, "sizeof( svert_v5_gl21_t ) == %d bytes\n", sizeof( svert_v5_gl21_t ));
	ALERT( at_console, "sizeof( svert_v5_gl30_t ) == %d bytes\n", sizeof( svert_v5_gl30_t ));
	ALERT( at_console, "sizeof( svert_v6_gl21_t ) == %d bytes\n", sizeof( svert_v6_gl21_t ));
	ALERT( at_console, "sizeof( svert_v6_gl30_t ) == %d bytes\n", sizeof( svert_v6_gl30_t ));
	ALERT( at_console, "sizeof( svert_v7_gl21_t ) == %d bytes\n", sizeof( svert_v7_gl21_t ));
	ALERT( at_console, "sizeof( svert_v7_gl30_t ) == %d bytes\n", sizeof( svert_v7_gl30_t ));
	ALERT( at_console, "sizeof( svert_v8_gl21_t ) == %d bytes\n", sizeof( svert_v8_gl21_t ));
	ALERT( at_console, "sizeof( svert_v8_gl30_t ) == %d bytes\n", sizeof( svert_v8_gl30_t ));
}

// Build the 8 corners of an axis-aligned cube centered at `pos` with the given
// half-size `rad`. Uses the engine's HL corner convention: bit 0 set = -X, bit 1 = -Y, bit 2 = -Z.
static void CubeCorners( const Vector &pos, float rad, std::array<Vector, 8> &out )
{
	for (int i = 0; i < 8; i++) {
		out[i].x = pos.x + ((i & 1) ? -rad : rad);
		out[i].y = pos.y + ((i & 2) ? -rad : rad);
		out[i].z = pos.z + ((i & 4) ? -rad : rad);
	}
}

void R_RenderLightProbe( mlightprobe_t *probe )
{
	const float rad = 4.0f;
	const float scale = tr.lightstyle[0] / 264.0f;
	const float gamma = 1.0f / tr.light_gamma;

	auto toLinear = [&]( const byte *c ) {
		return Vector(
			powf( c[0] / 255.0f, gamma ) * scale,
			powf( c[1] / 255.0f, gamma ) * scale,
			powf( c[2] / 255.0f, gamma ) * scale );
	};

	// probe->cube.color is indexed +X, -X, +Y, -Y, +Z, -Z;
	// g_boxpnt (backend face order) is +X, +Y, +Z, -X, -Y, -Z.
	std::array<Vector, 6> faceColors = {
		toLinear( probe->cube.color[0] ), // +X
		toLinear( probe->cube.color[2] ), // +Y
		toLinear( probe->cube.color[4] ), // +Z
		toLinear( probe->cube.color[1] ), // -X
		toLinear( probe->cube.color[3] ), // -Y
		toLinear( probe->cube.color[5] ), // -Z
	};

	std::array<Vector, 8> corners;
	CubeCorners( probe->origin, rad, corners );

	CDebugVisualizer::GetInstance().DrawFilledBox( corners, faceColors, 1.0f, std::nullopt, true );
}

void R_RenderCubemap( mcubemap_t *cube )
{
	const float rad = (float)cube->size * 0.1f;
	const float scale = tr.lightstyle[0] / 264.0f;
	const float gamma = 1.0f / tr.light_gamma;
	const float gray = powf( 127.0f / 255.0f, gamma ) * scale;
	const Vector color( gray, gray, gray );
	const std::array<Vector, 6> faceColors = { color, color, color, color, color, color };

	std::array<Vector, 8> corners;
	CubeCorners( cube->origin, rad, corners );

	CDebugVisualizer::GetInstance().DrawFilledBox( corners, faceColors, 1.0f, std::nullopt, true );
}

void R_RenderLightProbeInternal( const Vector &origin, const Vector lightCube[] )
{
	mlightprobe_t	probe;

	for( int i = 0; i < 6; i++ )
	{
		probe.cube.color[i][0] = lightCube[i].x * 255;
		probe.cube.color[i][1] = lightCube[i].y * 255;
		probe.cube.color[i][2] = lightCube[i].z * 255;
	}
	probe.origin = origin;

	R_RenderLightProbe( &probe );
}

/*
===============
R_DrawAABB

===============
*/
static void R_DrawAABB( const Vector &absmin, const Vector &absmax, int contents )
{
	Vector color;
	switch( contents )
	{
	case CONTENTS_EMPTY: color = Vector( 0.5f, 1.0f, 0.0f ); break; // green for empty
	case CONTENTS_SOLID: color = Vector( 1.0f, 0.0f, 0.0f ); break; // red for solid
	case CONTENTS_WATER: color = Vector( 0.0f, 0.5f, 1.0f ); break; // blue for water
	default:             color = Vector( 0.5f, 0.5f, 0.5f ); break; // gray otherwise
	}
	CDebugVisualizer::GetInstance().DrawAABB( absmin, absmax, color, std::nullopt, false );
}

/*
===============
R_RenderVisibleLeafs

===============
*/
static void R_RenderVisibleLeafs( void )
{
	mleaf_t	*leaf;
	int	i;

	// always skip the leaf 0, because is outside leaf
	for( i = 1, leaf = &worldmodel->leafs[1]; i < worldmodel->numleafs + 1; i++, leaf++ )
	{
		mextraleaf_t *eleaf = LEAF_INFO( leaf, worldmodel );

		if( CHECKVISBIT( RI->view.pvsarray, leaf->cluster ) && ( leaf->efrags || leaf->nummarksurfaces ))
		{
			R_DrawAABB( eleaf->mins, eleaf->maxs, CONTENTS_WATER );
			R_DrawAABB( Vector( leaf->minmaxs ), Vector( leaf->minmaxs + 3 ), leaf->contents );
		}
	}
}

/*
=============
DrawViewLeaf
=============
*/
void DrawViewLeaf( void )
{
	if( !CVAR_TO_BOOL( r_show_viewleaf ))
		return;

	GL_DEBUG_SCOPE();
	GL_Blend( GL_FALSE );
	R_RenderVisibleLeafs();
}

/*
=============
DrawLightProbes
=============
*/
void DrawLightProbes( void )
{
	int		i, j;
	mlightprobe_t	*probe;
	mleaf_t		*leaf;
	mextraleaf_t	*info;

	if( !CVAR_TO_BOOL( r_show_lightprobes ))
		return;

	GL_DEBUG_SCOPE();

	GL_Blend( GL_FALSE );
	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	// draw the all visible probes
	for( i = 1; i < world->numleafs; i++ )
	{
		leaf = (mleaf_t *)&worldmodel->leafs[i];
		info = LEAF_INFO( leaf, worldmodel );

		if( !CHECKVISBIT( RI->view.pvsarray, leaf->cluster ))
			continue;	// not visible from player

		for( j = 0; j < info->num_lightprobes; j++ )
		{
			probe = info->ambient_light + j;
			R_RenderLightProbe( probe );
		}
	}

	pglColor3f( 1.0f, 1.0f, 1.0f );
}

/*
=============
DrawCubeMaps
=============
*/
void DrawCubeMaps( void )
{
	mcubemap_t	*cm;

	if( !CVAR_TO_BOOL( r_show_cubemaps ))
		return;

	GL_DEBUG_SCOPE();
	GL_Blend( GL_FALSE );
	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	// draw the all cubemaps
	for( int i = 0; i < world->num_cubemaps; i++ )
	{
		cm = &world->cubemaps[i];
		R_RenderCubemap( cm );
	}

	pglColor3f( 1.0f, 1.0f, 1.0f );
}

void DBG_DrawLightFrustum( void )
{
	if( !CVAR_TO_BOOL( r_scissor_light_debug ) || !RP_NORMALPASS( ))
		return;

	auto &visualizer = CDebugVisualizer::GetInstance();
	const Vector frustumColor(1.0f, 1.0f, 0.0f);  // yellow
	const Vector bboxColor(1.0f, 0.0f, 1.0f);     // magenta

	for( int i = 0; i < MAX_DLIGHTS; i++ )
	{
		CDynLight *pl = &tr.dlights[i];
		if( !pl->Active( )) continue;

		if( r_scissor_light_debug->value == 1.0f )
		{
			R_DrawScissorRectangle( pl->x, pl->y, pl->w, pl->h );
		}
		else if( r_scissor_light_debug->value == 2.0f )
		{
			if( pl->type == LIGHT_DIRECTIONAL )
			{
				for (int j = 0; j < NUM_SHADOW_SPLITS + 1; j++)
					visualizer.DrawFrustum(pl->splitFrustum[j], frustumColor, std::nullopt, true);
			}
			else if( pl->type == LIGHT_OMNI )
			{
				visualizer.DrawSphere(pl->origin, pl->radius, frustumColor, std::nullopt, true);
			}
			else
			{
				visualizer.DrawFrustum(pl->frustum, frustumColor, std::nullopt, true);
			}
		}
		else
		{
			visualizer.DrawAABB(pl->absmin, pl->absmax, bboxColor, std::nullopt, true);
		}
	}
}

void DBG_DrawGlassScissors( void )
{
	if( CVAR_TO_BOOL( r_scissor_glass_debug ) && RP_NORMALPASS( ))
	{
		// debug scissor code
		GL_BindShader( NULL );

		// debug scissor code
		for( int k = 0; k < RI->frame.trans_list.Count(); k++ )
		{
			CTransEntry *entry = &RI->frame.trans_list[k];
			entry->RenderScissorDebug();
		}
	}
}

/*
================
DrawTangentSpaces

Draws vertex tangent spaces for debugging
================
*/
void DrawTangentSpaces( void )
{
	if( !CVAR_TO_BOOL( cv_show_tbn ))
		return;

	GL_DEBUG_SCOPE();
	const float vecLen = 4.0f;
	const Vector tangentColor( 1.0f, 0.0f, 0.0f );
	const Vector binormalColor( 0.0f, 1.0f, 0.0f );
	const Vector normalColor( 0.0f, 0.0f, 1.0f );
	auto &visualizer = CDebugVisualizer::GetInstance();

	for( int i = 0; i < worldmodel->nummodelsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		mextrasurf_t *esrf = surf->info;

		if( FBitSet( surf->flags, SURF_DRAWTURB|SURF_DRAWSKY ))
			continue;

		bvert_t *mv = &world->vertexes[esrf->firstvertex];

		for( int j = 0; j < esrf->numverts; j++, mv++ )
		{
			visualizer.DrawVector( mv->vertex, Vector( mv->tangent )  * vecLen, tangentColor,  std::nullopt, false );
			visualizer.DrawVector( mv->vertex, Vector( mv->binormal ) * vecLen, binormalColor, std::nullopt, false );
			visualizer.DrawVector( mv->vertex, Vector( mv->normal )   * vecLen, normalColor,   std::nullopt, false );
		}
	}
}

void DrawWireFrame( void )
{
	if( !CVAR_TO_BOOL( r_wireframe ))
		return;

	GL_DEBUG_SCOPE();
	auto &visualizer = CDebugVisualizer::GetInstance();
	const Vector solidColor( 1.0f, 1.0f, 1.0f ); // white: solid surfaces
	const Vector transColor( 0.0f, 1.0f, 0.0f ); // green: translucent surfaces

	auto submitSurfaceEdges = [&]( msurface_t *surf, const Vector &color ) {
		mextrasurf_t *es = surf->info;
		for( int j = 0; j < es->numverts; j++ )
		{
			bvert_t *va = &world->vertexes[es->firstvertex + j];
			bvert_t *vb = &world->vertexes[es->firstvertex + ((j + 1) % es->numverts)];
			const Vector a = Vector(va->vertex) + Vector(va->normal) * 0.1f;
			const Vector b = Vector(vb->vertex) + Vector(vb->normal) * 0.1f;
			visualizer.DrawVector( a, b - a, color, std::nullopt, false );
		}
	};

	for( int i = 0; i < RI->frame.solid_faces.Count(); i++ )
	{
		if( RI->frame.solid_faces[i].m_bDrawType != DRAWTYPE_SURFACE )
			continue;
		submitSurfaceEdges( RI->frame.solid_faces[i].m_pSurf, solidColor );
	}

	for( int i = 0; i < RI->frame.trans_list.Count(); i++ )
	{
		if( RI->frame.trans_list[i].m_bDrawType != DRAWTYPE_SURFACE )
			continue;
		submitSurfaceEdges( RI->frame.trans_list[i].m_pSurf, transColor );
	}
}

void DrawWirePoly( msurface_t *surf )
{
	if( !surf )
		return;

	GL_DEBUG_SCOPE();
	auto &visualizer = CDebugVisualizer::GetInstance();
	const Vector color( 0.5f, 1.0f, 0.36f );
	mextrasurf_t *es = surf->info;
	for( int j = 0; j < es->numverts; j++ )
	{
		bvert_t *va = &world->vertexes[es->firstvertex + j];
		bvert_t *vb = &world->vertexes[es->firstvertex + ((j + 1) % es->numverts)];
		const Vector a = Vector(va->vertex) + Vector(va->normal) * 0.1f;
		const Vector b = Vector(vb->vertex) + Vector(vb->normal) * 0.1f;
		visualizer.DrawVector( a, b - a, color, std::nullopt, false, 4.0f );
	}
}

void R_ShowLightMaps( void )
{
	if( !CVAR_TO_BOOL( r_showlightmaps ))
		return;

	int index = r_showlightmaps->value - 1.0f;
	if( tr.lightmaps[index].state == LM_FREE )
		return;

	CDebugOverlay2D::GetInstance().DrawTexturedRect(
		0.0f, 0.0f, (float)glState.width, (float)glState.height,
		tr.lightmaps[index].lightmap);
}
