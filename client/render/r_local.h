/*
r_local.h - renderer local definitions
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

#ifndef R_LOCAL_H
#define R_LOCAL_H

#include "r_opengl.h"
#include "matrix.h"
#include "ref_params.h"
#include "com_model.h"
#include "r_frustum.h"
#include "dlight.h"
#include "r_cvars.h"
#include "studio.h"
#include "r_studioint.h"
#include "r_studio.h"
#include "r_grass.h"

// brush model flags (stored in model_t->flags)
#define MODEL_CONVEYOR	BIT( 0 )
#define MODEL_HAS_ORIGIN	BIT( 1 )

#define R_SurfCmp( a, b )	(( a.shaderNum > b.shaderNum ) ? true : ( a.shaderNum < b.shaderNum ))
#define R_ModelOpaque( rm )	( rm == kRenderNormal )

// refparams
#define RP_NONE		0
#define RP_MIRRORVIEW	BIT( 0 )	// lock pvs at vieworg
#define RP_ENVVIEW		BIT( 1 )	// used for cubemapshot
#define RP_OLDVIEWLEAF	BIT( 2 )	// g-cont. it's even needed ?
#define RP_CLIPPLANE	BIT( 3 )	// mirrors and portals used
#define RP_FORCE_NOPLAYER	BIT( 4 )	// ignore player drawing in some special cases
#define RP_SKYPORTALVIEW	BIT( 5 )	// view through env_sky
#define RP_PORTALVIEW	BIT( 6 )	// view through portal
#define RP_SCREENVIEW	BIT( 7 )	// view through screen
#define RP_SHADOWVIEW	BIT( 8 )	// view through light
#define RP_OVERVIEW		BIT( 9 )	// draw orthogonal projection (overview)
#define RP_MERGEVISIBILITY	BIT( 10 )	// merge visibility for additional passes
#define RP_SKYVISIBLE	BIT( 11 )	// sky is visible
#define RP_NOGRASS		BIT( 12 )	// don't draw grass
#define RP_THIRDPERSON	BIT( 13 )	// camera is thirdperson

#define RP_NONVIEWERREF	(RP_MIRRORVIEW|RP_PORTALVIEW|RP_ENVVIEW|RP_SCREENVIEW|RP_SKYPORTALVIEW|RP_SHADOWVIEW)
#define RP_LOCALCLIENT( e )	(gEngfuncs.GetLocalPlayer() && ((e)->index == gEngfuncs.GetLocalPlayer()->index && e->player ))
#define RP_NORMALPASS()	( FBitSet( RI->params, RP_NONVIEWERREF ) == 0 )
#define R_StaticEntity( ent )	( ent->origin == g_vecZero && ent->angles == g_vecZero )
#define R_FullBright()	( CVAR_TO_BOOL( r_fullbright ) || !worldmodel->lightdata )
#define RP_OUTSIDE( leaf )	(((( leaf ) - worldmodel->leafs ) - 1 ) == -1 )

#define LM_SAMPLE_SIZE	16
#define LM_SAMPLE_EXTRASIZE	8

#define TF_LIGHTMAP		(TF_NOMIPMAP|TF_CLAMP|TF_ATLAS_PAGE|TF_HAS_ALPHA)
#define TF_DELUXMAP		(TF_CLAMP|TF_NOMIPMAP|TF_NORMALMAP|TF_ATLAS_PAGE)
#define TF_SCREEN		(TF_NOMIPMAP|TF_CLAMP)
#define TF_SPOTLIGHT	(TF_BORDER)
#define TF_SHADOW		(TF_CLAMP|TF_DEPTHMAP)

#define TF_DEPTHBUFFER	(TF_SCREEN|TF_DEPTHMAP|TF_NEAREST|TF_LUMINANCE|TF_NOCOMPARE)
#define TF_COLORBUFFER	(TF_SCREEN|TF_NEAREST)

#define CULL_VISIBLE	0		// not culled
#define CULL_BACKSIDE	1		// backside of transparent wall
#define CULL_FRUSTUM	2		// culled by frustum
#define CULL_OTHER		3		// culled by other reason

enum
{
	GL_KEEP_UNIT = -1,		// alternative way - change the unit by GL_SelectTexture
	GL_TEXTURE0 = 0,
	GL_TEXTURE1,
	GL_TEXTURE2,
	GL_TEXTURE3,
	GL_TEXTURE4,
	GL_TEXTURE5,
	GL_TEXTURE6,
	GL_TEXTURE7,
	MAX_TEXTURE_UNITS
};

typedef struct
{
	GLfloat		modelMatrix[16];			// matrix4x4( origin, angles, scale )
	matrix4x4		transform;			// entity transformation matrix 
} gl_state_t;

typedef enum
{
	LM_FREE = 0,	// lightmap is clear
	LM_USED,		// partially used, has free space
	LM_DONE,		// completely full
} lmstate_t;

typedef struct
{
	lmstate_t		state;
	unsigned short	allocated[BLOCK_SIZE];
	int		lightmap;
	int		deluxmap;
} gl_lightmap_t;

typedef void (*pfnShaderCallback)( struct glsl_prog_s *shader );

typedef struct gl_movie_s
{
	char		name[32];
	void		*state;
	float		length;		// total cinematic length
	long		xres, yres;	// size of cinematic
} gl_movie_t;

typedef struct gl_texbuffer_s
{
	int		framebuffer;
	int		texturenum;
	int		texframe;		// this frame texture was used
	matrix4x4		matrix;		// texture matrix
} gl_texbuffer_t;

typedef struct gl_fbo_s
{
	GLboolean		init;
	GLuint		framebuffer;
	GLuint		renderbuffer;
	int		texture;
} gl_fbo_t;

// plight - projected light
typedef struct plight_s
{
	Vector		origin;		// cached origin and angles
	Vector		angles;
	float		radius;
	color24		color;		// ignored for spotlights, they have a texture
	float		die;		// stop lighting after this time
	float		decay;		// drop this each second
	int		key;
	bool		pointlight;	// it's a point light (may be with cubemap)
	bool		update;		// light needs update
	bool		culled;		// culled by scissor for this pass

	matrix4x4		projectionMatrix;	// light projection matrix
	matrix4x4		modelviewMatrix;	// light modelview
	matrix4x4		lightviewProjMatrix;// lightview projection

	Vector		absmin, absmax;	// world bounds
	CFrustum		frustum;		// light frustum

	int		projectionTexture;	// 2D projection texture (e.g. flashlight.tga)
	struct model_s	*pSprite;		// animated sprite
	int		cinTexturenum;	// not gltexturenum!
	int		lastframe;	// cinematic lastframe
	int		shadowTexture;	// shadowmap for this light

	// scissor data
	float		x, y, w, h;

	// light params
	float		lightFalloff;	// falloff factor
	int		flags;
	float		fov;
} plight_t;

// 68 bytes here
typedef struct bvert_s
{
	Vector		vertex;			// position
	Vector		normal;			// normal (for dynamic lighting)
	float		stcoord0[4];		// ST texture coords
	float		lmcoord0[MAXLIGHTMAPS];	// LM texture coords for styles 0-1
	float		lmcoord1[MAXLIGHTMAPS];	// LM texture coords for styles 2-3
	byte		styles[MAXLIGHTMAPS];	// light styles
} bvert_t;

// 6 bytes here
typedef struct
{
	msurface_t	*surface;
	unsigned short	hProgram;		// handle to glsl program
} gl_bmodelface_t;

typedef struct
{
	char		name[64];
	char		diffuse[64];
	unsigned short	gl_diffuse_id;	// diffuse texture
	unsigned short	gl_heightmap_id;
	unsigned short	width, height;
	byte		maxHeight;
	byte		numLayers;	// layers that specified on heightmap
	byte		*pixels;		// pixels are immediately goes here
} indexMap_t;

typedef struct
{
	char		pathes[MAX_LANDSCAPE_LAYERS][64];	// path to texture (may include extension etc)
	char		names[MAX_LANDSCAPE_LAYERS][64];	// basenames
	unsigned short	gl_diffuse_id;			// diffuse texture array
	unsigned short	gl_detail_id;			// detail texture
} layerMap_t;

// simple version of P2:Savior landscape: global texture only
typedef struct terrain_s
{
	char		name[16];
	indexMap_t	indexmap;
	layerMap_t	layermap;
	int		numLayers;	// count of array textures
	float		texScale;		// global texture scale
	bool		valid;		// if heightmap was actual
} terrain_t;

typedef struct
{
	int		params;			// rendering parameters
	int		viewentity;

	float		fov_x, fov_y;		// current view fov

	cl_entity_t	*currententity;
	model_t		*currentmodel;
	plight_t		*currentlight;		// only valid for shadow passes
	struct glsl_prog_s	*currentshader;

	int		viewport[4];
	CFrustum		frustum;

	mleaf_t		*viewleaf;
	mleaf_t		*oldviewleaf;
	Vector		pvsorigin;
	Vector		viewangles;
	Vector		vieworg;			// locked vieworigin
	Vector		vforward;
	Vector		vright;
	Vector		vup;

	float		farClip;

	float		skyMins[2][6];
	float		skyMaxs[2][6];

	matrix4x4		objectMatrix;		// currententity matrix
	matrix4x4		worldviewMatrix;		// modelview for world
	matrix4x4		modelviewMatrix;		// worldviewMatrix * objectMatrix

	matrix4x4		projectionMatrix;
	matrix4x4		worldviewProjectionMatrix;	// worldviewMatrix * projectionMatrix
	byte		visbytes[MAX_MAP_LEAFS/8];	// individual visbytes for each pass
	byte		visfaces[MAX_MAP_FACES/8];	// actual visible faces for current frame (world only)
	int		visframecount;

	msurface_t	*subview_faces[MAX_SUBVIEWS];
	int		num_subview_faces;
	msurface_t	*reject_face;		// avoid recursion to himself

	float		viewplanedist;
	mplane_t		clipPlane;
} ref_instance_t;

typedef struct
{
	bool		fResetVis;
	bool		fCustomRendering;
	bool		fClearScreen;			// force clear if world shaders failed to build
	int		fGamePaused;

	double		time;		// cl.time
	double		oldtime;		// cl.oldtime
	double		frametime;	// special frametime for multipass rendering (will set to 0 on a nextview)

	int		whiteTexture;
	int		grayTexture;
	int		depthTexture;	// stub
	int		defaultTexture;   	// use for bad textures
	int		alphaContrastTexture;
	int		dlightCubeTexture;	// for point lights
	int		skyboxTextures[6];	// skybox sides
	gl_texbuffer_t	subviewTextures[MAX_SUBVIEW_TEXTURES];
	int		cinTextures[MAX_MOVIE_TEXTURES];
	int		shadowTextures[MAX_SHADOWS];
	int		num_subview_used;	// used mirror textures per full frame
	int		num_shadows_used;	// used shadow textures per full frame
	int		num_cin_used;	// used movie textures per full frame

	int		screen_depth;
	int		screen_color;
	int		target_rgb[2];

	int		spotlightTexture;

	// framebuffers
	gl_fbo_t		frame_buffers[MAX_FRAMEBUFFERS];
	int		num_framebuffers;

	gl_state_t	cached_state[MAX_CACHED_STATES];
	int		num_cached_states;

	// lighting stuff
	int		lightstylevalue[MAX_LIGHTSTYLES];	// value 0 - 65536
	float		lightstyles[MAX_LIGHTSTYLES];		// GLSL cache-friendly array
	gl_lightmap_t	lightmaps[MAX_LIGHTMAPS];
	byte		current_lightmap_texture;

	// entity lists
	cl_entity_t	*solid_entities[MAX_VISIBLE_PACKET];	// opaque moving or alpha brushes
	cl_entity_t	*trans_entities[MAX_VISIBLE_PACKET];	// translucent brushes
	gl_movie_t	cinematics[MAX_MOVIES];		// precached cinematics
	int		num_solid_entities;
	int		num_trans_entities;

	gl_bmodelface_t	draw_surfaces[MAX_MAP_FACES];		// 390 kB here
	int		num_draw_surfaces;

	gl_bmodelface_t	light_surfaces[MAX_MAP_FACES];	// 390 kB here
	int		num_light_surfaces;

	msurface_t	*draw_decals[MAX_DECAL_SURFS];
	int		num_draw_decals;

	cl_entity_t	*sky_camera;
	bool		fIgnoreSkybox;		// we already draw 3d skybox, so don't overwrite it in current pass
	ref_params_t	viewparams;		// local copy of ref_params_t
	struct movevars_s	*movevars;

	bool		fogEnabled;
	Vector		fogColor;
	float		fogDensity;

	float		blend;
	float		lodScale;
         
	// OpenGL matrix states
	bool		modelviewIdentity;
	
	int		traceframecount;		// to avoid checked each surface multiple times
	int		realframecount;		// complete frame with passes

	bool		params_changed;		// some cvars are toggled, shaders needs to recompile and resort
	bool		local_client_added;		// indicate what a local client already been added into renderlist
	bool		shadows_notsupport;		// no shadow textures
	int		glsl_valid_sequence;	// reloas shaders while some render cvars was changed
	bool		show_uniforms_peak;		// print the maxcount of used uniforms
	int		num_draw_grass;		// number of bushes per normal or shadow pass
	int		num_light_grass;		// number of bushes per dynamic light
	int		grassunloadframe;		// unload too far grass to save video memory

	byte		visbytes[MAX_MAP_LEAFS/8];	// shared visbytes for engine checking
	int		pvssize;

	// cached shadernums for dynamic lighting
	bool		nodlights;
	unsigned short	omniLightShaderNum;		// cached omni light shader for this face
	unsigned short	projLightShaderNum[2];	// cached proj light shader for this face
	unsigned short	glsl_sequence_omni;		// same as above but for omnilights
	unsigned short	glsl_sequence_proj[2];	// same as above but for projlights

	Vector4D		gamma_table[64];

	// original player vieworg and angles
	Vector		cached_vieworigin;
	Vector		cached_viewangles;

	double		buildtime;

	Vector		sky_normal;		// sky vector
	Vector		sky_ambient;		// sky ambient color

	struct mvbocache_s	*vertex_light_cache[MAX_LIGHTCACHE];	// FIXME: make growable

	// cull info
	Vector		modelorg;			// relative to viewpoint

	size_t		grass_total_size;		// debug
} ref_globals_t;

typedef struct
{
	unsigned int	c_world_polys;
	unsigned int	c_studio_polys;
	unsigned int	c_sprite_polys;
	unsigned int	c_world_leafs;
	unsigned int	c_grass_polys;

	unsigned int	c_active_tents_count;
	unsigned int	c_studio_models_drawn;
	unsigned int	c_sprite_models_drawn;

	unsigned int	c_portal_passes;
	unsigned int	c_mirror_passes;
	unsigned int	c_screen_passes;
	unsigned int	c_shadow_passes;
	unsigned int	c_sky_passes;	// drawing through portal or monitor will be increase counter

	unsigned int	c_total_tris;	// triangle count

	unsigned int	c_plights;	// count of actual projected lights
	unsigned int	c_client_ents;	// entities that moved to client

	unsigned int	c_screen_copy;	// how many times screen was copied
	unsigned int	num_passes;

	unsigned int	num_shader_binds;
	unsigned int	num_flushes;

	msurface_t	*debug_surface;
	double		t_world_node;
	double		t_world_draw;
} ref_stats_t;

extern engine_studio_api_t	IEngineStudio;
extern float		gldepthmin, gldepthmax;
extern model_t		*worldmodel;
extern ref_instance_t	*RI;
extern ref_globals_t	tr;
extern ref_stats_t		r_stats;
extern cl_entity_t		*v_intermission_spot;
extern plight_t		cl_plights[MAX_PLIGHTS];
#define r_numEntities	(tr.num_solid_entities + tr.num_trans_entities - r_stats.c_client_ents)
#define r_numStatics	(r_stats.c_client_ents)

/*
=======================================================================

 GL STATE MACHINE

=======================================================================
*/
enum
{
	R_OPENGL_110 = 0,		// base
	R_WGL_PROCADDRESS,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_ARB_VERTEX_ARRAY_OBJECT_EXT,
	R_EXT_GPU_SHADER4,		// shaders only
	R_TEXTURE_ARRAY_EXT,	// shaders only
	R_ARB_MULTITEXTURE,
	R_TEXTURECUBEMAP_EXT,
	R_SHADER_GLSL100_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_TEXTURE_3D_EXT,
	R_SHADER_OBJECTS_EXT,
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_ARB_TEXTURE_NPOT_EXT,
	R_DEPTH_TEXTURE,
	R_SHADOW_EXT,
	R_FRAMEBUFFER_OBJECT,
	R_PARANOIA_EXT,		// custom OpenGL32.dll with hacked function glDepthRange
	R_DEBUG_OUTPUT,
	R_EXTCOUNT,		// must be last
};

typedef enum
{
	GLHW_GENERIC,		// where everthing works the way it should
	GLHW_RADEON,		// where you don't have proper GLSL support
	GLHW_NVIDIA		// Geforce 8/9 class DX10 hardware
} glHWType_t;

typedef struct
{
	int		width, height;
	qboolean		fullScreen;
	qboolean		wideScreen;

	int		faceCull;
	int		frontFace;
	int		frameBuffer;
	GLint		depthmask;

	qboolean		drawTrans;		// FIXME: get rid of this
	qboolean		stencilEnabled;

	ref_instance_t	stack[MAX_REF_STACK];
	GLuint		stack_position;
} glState_t;

typedef struct
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*version_string;
	const char	*vendor_string;

	glHWType_t	hardware_type;

	// list of supported extensions
	const char	*extensions_string;
	bool		extension[R_EXTCOUNT];

	int		max_texture_units;
	GLint		max_2d_texture_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_size;
	GLint		texRectangle;

	int		max_vertex_uniforms;
	int		max_vertex_attribs;
	int		max_varying_floats;
	int		max_skinning_bones;		// total bones that can be transformed with GLSL
	int		peak_used_uniforms;
	bool		uniforms_economy;
} glConfig_t;

extern glState_t glState;
extern glConfig_t glConfig;

//
// r_opengl.cpp
//
bool R_Init( void );
void R_VidInit( void );
void R_Shutdown( void );
bool GL_Support( int r_ext );
#define GL_CheckForErrors() GL_CheckForErrors_( __FILE__, __LINE__ )
void GL_CheckForErrors_( const char *filename, const int fileline );

//
// r_backend.cpp
//
void GL_LoadMatrix( const matrix4x4 source );
void GL_LoadTexMatrix( const matrix4x4 source );
void GL_FrontFace( GLenum front );
void GL_Cull( GLenum cull );
bool R_BeginDrawProjectionGLSL( plight_t *pl, float lightscale = 1.0f );
void R_EndDrawProjectionGLSL( void );
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame );
int R_AllocFrameBuffer( int viewport[4] );
void GL_BindFrameBuffer( int buffer, int texture );
void R_FreeFrameBuffer( int buffer );
void GL_DepthMask( GLint enable );
void GL_AlphaTest( GLint enable );
void GL_Blend( GLint enable );
void GL_BindFBO( GLint buffer );
void GL_Setup2D( void );
void GL_Setup3D( void );

//
// r_cull.cpp
//
bool R_CullModel( cl_entity_t *e, const Vector &absmin, const Vector &absmax );
int R_CullSurfaceExt( msurface_t *surf, CFrustum *frustum );
bool R_CullNodeTopView( const struct mworldnode_s *node );

#define R_CullBox( mins, maxs )	( RI->frustum.CullBox( mins, maxs ))
#define R_CullSphere( centre, radius )	( RI->frustum.CullSphere( centre, radius ))
#define R_CullSurface( surf )		R_CullSurfaceExt( surf, &RI->frustum )

//
// r_debug.c
//
void R_DrawScissorRectangle( float x, float y, float w, float h );
void DBG_DrawBBox( const Vector &mins, const Vector &maxs );
void R_DrawRenderPasses( int passnum );
void R_DrawLightScissors( void );
void DrawLightProbes( void );
void DrawWireFrame( void );
void DrawNormals( void );

//
// r_decals.c
//
void DrawDecalsBatch( void );
void DrawSurfaceDecals( msurface_t *fa, qboolean single, qboolean reverse );
void DrawSingleDecal( decal_t *pDecal, msurface_t *fa );

//
// r_light.c
//
void R_AnimateLight( void );
void R_PushDlights( void );
int R_CountPlights( bool countShadowLights = false );
Vector R_LightsForPoint( const Vector &point, float radius );
void R_GetLightVectors( cl_entity_t *pEnt, Vector &origin, Vector &angles );
void R_SetupLightProjection( plight_t *pl, const Vector &origin, const Vector &angles, float radius, float fov );
void R_SetupLightProjectionTexture( plight_t *pl, cl_entity_t *pEnt );
void R_SetupLightAttenuationTexture( plight_t *pl, int falloff = -1 );
plight_t *CL_AllocPlight( int key );
void CL_DecayLights( void );
void CL_ClearPlights( void );

//
// r_lightmap.cpp
//
void R_UpdateSurfaceParams( msurface_t *surf );
void GL_BeginBuildingLightmaps( void );
void GL_AllocLightmapForFace( msurface_t *surf );
void GL_EndBuildingLightmaps( bool lightmap, bool deluxmap );
void R_TextureCoords( msurface_t *surf, const Vector &vec, float *out );
void R_GlobalCoords( msurface_t *surf, const Vector &point, float *out );
void R_LightmapCoords( msurface_t *surf, const Vector &vec, float *coords, int style );

//
// r_main.cpp
//
void R_ClearScene( void );
word GL_CacheState( const Vector &origin, const Vector &angles );
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType );
qboolean R_WorldToScreen( const Vector &point, Vector &screen );
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m );
void R_ScreenToWorld( const Vector &screen, Vector &point );
void R_SetupModelviewMatrix( matrix4x4 &m );
void R_DrawParticles( qboolean trans );
void R_CheckChanges( void );
void R_RenderScene( void );
int CL_FxBlend( cl_entity_t *e );
void R_LoadIdentity( void );
void R_RotateForEntity( cl_entity_t *e );
void R_TranslateForEntity( cl_entity_t *e );
void R_TransformForEntity( const matrix4x4 &transform );
const char *R_GetNameForView( void );
void R_AllowFog( int allowed );
void R_FindViewLeaf( void );
void R_SetupFrustum( void );
void R_InitRefState( void );
void R_ResetRefState( void );
void R_PushRefState( void );
void R_PopRefState( void );
ref_instance_t *R_GetPrevInstance( void );

//
// r_misc.cpp
//
void R_NewMap( void );
void Mod_ThrowModelInstances( void );
void Mod_PrepareModelInstances( void );
int Mod_SampleSizeForFace( msurface_t *surf );
void R_LoadLandscapes( const char *filename );
terrain_t *R_FindTerrain( const char *texname );
void R_FreeLandscapes( void );

//
// r_movie.cpp
//
int R_PrecacheCinematic( const char *cinname );
void R_InitCinematics( void );
void R_FreeCinematics( void );
int R_AllocateCinematicTexture( unsigned int txFlags );
void R_UpdateCinematic( const msurface_t *surf );
void R_UpdateCinSound( cl_entity_t *e );

//
// r_postprocess.cpp
//
void InitPostTextures( void );
void InitPostEffects( void );
void RenderSunShafts( void );
void R_BindPostFramebuffers( void );

//
// r_shadows.cpp
//
void R_RenderShadowmaps( void );

//
// r_surf.cpp
//
void R_MarkLeaves( void );
void HUD_BuildLightmaps( void );
texture_t *R_TextureAnimation( msurface_t *s );
void GL_InitRandomTable( void );

//
// r_subview.cpp
//
void R_RenderSubview( void );

//
// gl_shader.cpp
//
const char *GL_PretifyListOptions( const char *options, bool newlines = false );
void GL_BindShader( struct glsl_prog_s *shader );
void GL_FreeUberShaders( void );
void GL_InitGPUShaders( void );
void GL_FreeGPUShaders( void );
word GL_UberShaderForSolidBmodel( msurface_t *s, bool translucent = false );
word GL_UberShaderForBmodelDlight( const plight_t *pl, msurface_t *s, bool translucent = false );
word GL_UberShaderForBmodelDecal( decal_t *decal );
word GL_UberShaderForGrassSolid( msurface_t *s, struct grass_s *g );
word GL_UberShaderForGrassDlight( plight_t *pl, struct grass_s *g );
word GL_UberShaderForDlightGeneric( const plight_t *pl );
word GL_UberShaderForSolidStudio( struct mstudiomat_s *mat, bool vertex_lighting, bool bone_weighting, bool fullbright, int numbones = 0 );
word GL_UberShaderForDlightStudio( const plight_t *dl, struct mstudiomat_s *mat, bool bone_weighting, int numbones = 0 );
word GL_UberShaderForStudioDecal( struct mstudiomat_s *mat );

//
// r_util.cpp
//
int WorldToScreen( const Vector &world, Vector &screen );
byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model );
byte *Mod_GetCurrentVis( void );
byte *Mod_GetEngineVis( void );
float V_CalcFov( float &fov_x, float width, float height );
void V_AdjustFov( float &fov_x, float &fov_y, float width, float height, bool lock_x );

//
// r_world.cpp
//
void R_DrawWorld( void );
void R_DrawWorldShadowPass( void );
void R_DrawBrushModel( cl_entity_t *e );
void R_DrawBrushModelShadow( cl_entity_t *e );
void R_ProcessWorldData( model_t *mod, qboolean create, const byte *buffer );
bool Mod_CheckLayerNameForSurf( msurface_t *surf, const char *checkName );
bool Mod_CheckLayerNameForPixel( mfaceinfo_t *land, const Vector &point, const char *checkName );
void Mod_SetOrthoBounds( const float *mins, const float *maxs );
void R_SetRenderColor( cl_entity_t *e );
void R_WorldSetupVisibility( void );
void Mod_ResortFaces( void );

//
// r_warp.cpp
//
void R_DrawSkyBox( void );
void R_ClearSkyBox( void );
void R_CheckSkyPortal( cl_entity_t *skyPortal );
void R_DrawSkyPortal( cl_entity_t *skyPortal );
void R_AddSkyBoxSurface( msurface_t *fa );

#endif//R_LOCAL_H
