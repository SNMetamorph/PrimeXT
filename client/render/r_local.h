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
#include "dlight.h"
#include "r_cvars.h"
#include "studio.h"
#include "r_studioint.h"
#include "r_studio.h"
#include "r_grass.h"

#define INVALID_HANDLE	0xFFFF

// brush model flags (stored in model_t->flags)
#define MODEL_CONVEYOR	BIT( 0 )
#define MODEL_HAS_ORIGIN	BIT( 1 )

// refparams
#define RP_NONE		0
#define RP_MIRRORVIEW	BIT( 0 )	// lock pvs at vieworg
#define RP_ENVVIEW		BIT( 1 )	// used for cubemapshot
#define RP_OLDVIEWLEAF	BIT( 2 )
#define RP_CLIPPLANE	BIT( 3 )	// mirrors used
#define RP_FLIPFRONTFACE	BIT( 4 )	// e.g. for mirrors drawing
#define RP_SKYPORTALVIEW	BIT( 5 )	// view through env_sky
#define RP_PORTALVIEW	BIT( 6 )	// view through portal
#define RP_SCREENVIEW	BIT( 7 )	// view through screen
#define RP_SHADOWVIEW	BIT( 8 )	// view through light
#define RP_WORLDSURFVISIBLE	BIT( 9 )	// indicates what we view at least one surface from the current vieworg
#define RP_MERGEVISIBILITY	BIT( 10 )	// merge visibility for additional passes
#define RP_FORCE_NOPLAYER	BIT( 11 )	// ignore player drawing in some special cases

#define RP_NONVIEWERREF	(RP_MIRRORVIEW|RP_PORTALVIEW|RP_ENVVIEW|RP_SCREENVIEW|RP_SKYPORTALVIEW|RP_SHADOWVIEW)
#define RP_LOCALCLIENT( e )	(gEngfuncs.GetLocalPlayer() && ((e)->index == gEngfuncs.GetLocalPlayer()->index && e->curstate.entityType == ET_PLAYER ))
#define RP_NORMALPASS()	((RI.params & RP_NONVIEWERREF) == 0 )

#define MAX_MIRROR_DEPTH	2

#define LM_SAMPLE_SIZE	tr.lm_sample_size	// lightmap resoultion

#define AREA_NODES		32
#define AREA_DEPTH		4

#define TF_LIGHTMAP		(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP)
#define TF_IMAGE		(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP)
#define TF_SCREEN		(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP)
#define TF_SPOTLIGHT	(TF_UNCOMPRESSED|TF_NOMIPMAP|TF_BORDER)
#define TF_SHADOW		(TF_UNCOMPRESSED|TF_NOMIPMAP|TF_CLAMP|TF_DEPTHMAP)

// overbright helper macroses
#define R_OVERBRIGHT()		(r_overbright->value && !r_fullbright->value)
#define R_OVERBRIGHT_SILENT() 	(r_overbright->value >= 2.0f && !r_fullbright->value)
#define R_OVERBRIGHT_SFACTOR()	(R_OVERBRIGHT() ? GL_DST_COLOR : GL_ZERO)

enum
{
	GL_TEXTURE0 = 0,
	GL_TEXTURE1,
	GL_TEXTURE2,
	GL_TEXTURE3,
	MAX_TEXTURE_UNITS
};

// mirror entity
typedef struct gl_entity_s
{
	cl_entity_t	*ent;
	mextrasurf_t	*chain;
} gl_entity_t;

typedef struct gl_movie_s
{
	char		name[32];
	void		*state;
	float		length;		// total cinematic length
	long		xres, yres;	// size of cinematic
} gl_movie_t;

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
	mplane_t		frustum[6];	// light frustum
	unsigned int	clipflags;	// probably not needs
	unsigned int	flags;		// flashlight flags (typically come from iuser1)

	matrix4x4		projectionMatrix;	// light projection matrix
	matrix4x4		modelviewMatrix;	// light modelview
	matrix4x4		textureMatrix;	// result texture matrix	
	matrix4x4		textureMatrix2;	// for bmodels
	matrix4x4		shadowMatrix;	// result texture matrix	
	matrix4x4		shadowMatrix2;	// for bmodels
	Vector		origin, angles;	// cached origin and angles

	int		projectionTexture;	// 2D projection texture (e.g. flashlight.tga)
	int		attenuationTexture;	// 1D attenuation texture
	struct model_s	*pSprite;		// animated sprite
	int		cinTexturenum;	// not gltexturenum!
	int		lastframe;	// cinematic lastframe
	bool		pointlight;	// it's a point light (may be with cubemap)
	int		shadowTexture;	// shadowmap for this light

	// light params
	color24		color;
	float		radius;	// FOV
	float		die;	// stop lighting after this time
	float		decay;	// drop this each second
	int		key;
	float		fov;
} plight_t;

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		lights;		// world lights
} areanode_t;

typedef struct
{
	int		params;		// rendering parameters

	qboolean		drawWorld;	// ignore world for drawing PlayerModel
	qboolean		thirdPerson;	// thirdperson camera is enabled
	qboolean		isSkyVisible;	// sky is visible
	qboolean		drawOrtho;	// draw world as orthogonal projection	

	ref_params_t	refdef;		// actual refdef

	cl_entity_t	*currententity;
	model_t		*currentmodel;
	cl_entity_t	*currentbeam;	// same as above but for beams
	const plight_t	*currentlight;	// only valid for shadow passes

	int		viewport[4];
	int		scissor[4];
	mplane_t		frustum[6];

	Vector		pvsorigin;
	Vector		vieworg;		// locked vieworigin
	Vector		vforward;
	Vector		vright;
	Vector		vup;

	float		farClip;
	unsigned int	clipFlags;

	qboolean		fogCustom;
	qboolean		fogEnabled;
	Vector		fogColor;
	float		fogDensity;
	float		fogStart;
	float		fogEnd;
	int		cached_contents;	// in water

	float		waveHeight;	// global waveHeight
	float		currentWaveHeight;	// current entity waveHeight

	float		skyMins[2][6];
	float		skyMaxs[2][6];

	matrix4x4		objectMatrix;		// currententity matrix
	matrix4x4		worldviewMatrix;		// modelview for world
	matrix4x4		modelviewMatrix;		// worldviewMatrix * objectMatrix

	matrix4x4		projectionMatrix;
	matrix4x4		worldviewProjectionMatrix;	// worldviewMatrix * projectionMatrix
	int		lightstylevalue[MAX_LIGHTSTYLES];	// value 0 - 65536
	float		lightcache[MAX_LIGHTSTYLES];
	byte		visbytes[MAX_MAP_LEAFS/8];	// individual visbytes for each pass
	
	float		viewplanedist;
	mplane_t		clipPlane;
} ref_instance_t;

typedef struct
{
	char		worldname[64];	// for catch map changes

	int		cinTexture;      	// cinematic texture
	int		skyTexture;	// default sky texture
	int		whiteTexture;
	int		grayTexture;
	int		blackTexture;
	int		defaultTexture;   	// use for bad textures
	int		particleTexture;	// particle texture
	int		particleTexture2;	// unsmoothed particle texture
	int		solidskyTexture;	// quake1 solid-sky layer
	int		alphaskyTexture;	// quake1 alpha-sky layer
	int		lightmapTextures[MAX_LIGHTMAPS];
	int		dlightTexture;	// custom dlight texture (128x128)
	int		dlightTexture2;	// custom dlight texture (256x256)
	int		attenuationTexture;	// normal attenuation
	int		attenuationTexture2;// dark attenuation
	int		attenuationTexture3;// bright attenuation
	int		attenuationTexture3D;// 3D attenuation
	int		attenuationStubTexture;
	int		chromeTexture;	// for shiny faces
	int		normalizeTexture;
	int		dlightCubeTexture;	// for point lights
	int		skyboxTextures[6];	// skybox sides
	int		mirrorTextures[MAX_MIRRORS];
	int		portalTextures[MAX_MIRRORS];
	int		screenTextures[MAX_MIRRORS];
	int		cinTextures[MAX_MOVIE_TEXTURES];
	int		shadowTextures[MAX_SHADOWS];
	int		num_mirrors_used;	// used mirror textures per full frame
	int		num_portals_used;	// used portal textures per full frame
	int		num_screens_used;	// used screen textures per full frame
	int		num_cin_used;	// used movie textures per full frame
	int		num_shadows_used;	// used shadow textures per full frame

	int		skytexturenum;	// this not a gl_texturenum!
	int		spotlightTexture;
	int		noiseTexture;
	int		fogTexture2D;
	int		fogTexture1D;

	int		lm_sample_size;

	// framebuffers
	gl_fbo_t		frame_buffers[MAX_FRAMEBUFFERS];
	int		num_framebuffers;

	int		fbo[FBO_NUM_TYPES];

	// entity lists
	cl_entity_t	*static_entities[MAX_VISIBLE_PACKET];	// opaque non-moved brushes
	gl_entity_t	mirror_entities[MAX_VISIBLE_PACKET];	// an entities that has mirror
	gl_entity_t	portal_entities[MAX_VISIBLE_PACKET];	// an entities that has portal
	gl_entity_t	screen_entities[MAX_VISIBLE_PACKET];	// an entities that has screen
	cl_entity_t	*solid_entities[MAX_VISIBLE_PACKET];	// opaque moving or alpha brushes
	cl_entity_t	*trans_entities[MAX_VISIBLE_PACKET];	// translucent brushes
	cl_entity_t	*child_entities[MAX_VISIBLE_PACKET];	// entities with MOVETYPE_FOLLOW
	cl_entity_t	*beams_entities[MAX_VISIBLE_PACKET];	// server beams
	gl_movie_t	cinematics[MAX_MOVIES];		// precached cinematics
	int		num_static_entities;
	int		num_mirror_entities;
	int		num_portal_entities;
	int		num_screen_entities;
	int		num_solid_entities;
	int		num_trans_entities;
	int		num_child_entities;
	int		num_beams_entities;

	cl_entity_t	*sky_camera;
	bool		fIgnoreSkybox;
	bool		fResetVis;
	bool		fCustomRendering;
	int		insideView;	// this is view through portal or mirror
         
	// OpenGL matrix states
	bool		modelviewIdentity;
	
	int		visframecount;	// PVS frame
	int		realframecount;	// not including passes
	int		traceframecount;	// to avoid checked each surface multiple times
	int		grassframecount;
	int		framecount;

	bool		world_has_portals;	// indicate a surfaces with SURF_PORTAL bit set
	bool		world_has_screens;	// indicate a surfaces with SURF_SCREEN bit set
	bool		world_has_movies;	// indicate a surfaces with SURF_MOVIE bit set
	bool		local_client_added;	// indicate what a local client already been added into renderlist

	const ref_params_t	*cached_refdef;	// pointer to viewer refdef

	// cull info
	Vector		modelorg;		// relative to viewpoint

	size_t		grass_total_size;	// debug
} ref_globals_t;

typedef struct
{
	unsigned int	screen_shader;
	unsigned int	shadow_shader;
	unsigned int	liquid_shader;

	unsigned int	decal0_shader;
	unsigned int	decal1_shader;
	unsigned int	decal2_shader;
	unsigned int	decal3_shader;	// for pointlights
} ref_programs_t;

typedef struct
{
	unsigned int	c_world_polys;
	unsigned int	c_brush_polys;
	unsigned int	c_studio_polys;
	unsigned int	c_sprite_polys;
	unsigned int	c_world_leafs;
	unsigned int	c_grass_polys;

	unsigned int	c_view_beams_count;
	unsigned int	c_active_tents_count;
	unsigned int	c_studio_models_drawn;
	unsigned int	c_sprite_models_drawn;
	unsigned int	c_particle_count;

	unsigned int	c_portal_passes;
	unsigned int	c_mirror_passes;
	unsigned int	c_screen_passes;
	unsigned int	c_shadow_passes;
	unsigned int	c_sky_passes;	// drawing through portal or monitor will be increase counter

	unsigned int	c_plights;	// count of actual projected lights
	unsigned int	c_client_ents;	// entities that moved to client

	unsigned int	num_drawed_ents;
	unsigned int	num_passes;

	unsigned int	num_drawed_particles;
	unsigned int	num_particle_systems;
	unsigned int	num_flushes;

	msurface_t	*debug_surface;
} ref_stats_t;

extern engine_studio_api_t	IEngineStudio;
extern mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
extern float		gldepthmin, gldepthmax;
extern model_t		*worldmodel;
extern ref_params_t		r_lastRefdef;
extern ref_instance_t	RI;
extern ref_globals_t	tr;
extern ref_programs_t	cg;
extern ref_stats_t		r_stats;
extern cl_entity_t		*v_intermission_spot;
extern plight_t		cl_plights[MAX_PLIGHTS];
#define r_numEntities	(tr.num_solid_entities + tr.num_trans_entities + tr.num_child_entities + tr.num_static_entities - r_stats.c_client_ents)
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
	R_ENV_COMBINE_EXT,
	R_ARB_MULTITEXTURE,
	R_TEXTURECUBEMAP_EXT,
	R_DOT3_ARB_EXT,
	R_ANISOTROPY_EXT,
	R_TEXTURE_LODBIAS,
	R_OCCLUSION_QUERIES_EXT,
	R_TEXTURE_COMPRESSION_EXT,
	R_SHADER_GLSL100_EXT,
	R_SGIS_MIPMAPS_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_LOCKARRAYS_EXT,
	R_TEXTURE_3D_EXT,
	R_CLAMPTOEDGE_EXT,
	R_BLEND_MINMAX_EXT,
	R_STENCILTWOSIDE_EXT,
	R_BLEND_SUBTRACT_EXT,
	R_SHADER_OBJECTS_EXT,
	R_VERTEX_PROGRAM_EXT,	// cg vertex program
	R_FRAGMENT_PROGRAM_EXT,	// cg fragment program
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_EXT_POINTPARAMETERS,
	R_SEPARATESTENCIL_EXT,
	R_ARB_TEXTURE_NPOT_EXT,
	R_CUSTOM_VERTEX_ARRAY_EXT,
	R_TEXTURE_ENV_ADD_EXT,
	R_CLAMP_TEXBORDER_EXT,
	R_DEPTH_TEXTURE,
	R_SHADOW_EXT,
	R_FRAMEBUFFER_OBJECT,
	R_PARANOIA_EXT,		// custom OpenGL32.dll with hacked function glDepthRange
	R_EXTCOUNT,		// must be last
};

#define MAX_TEXTURE_UNITS	4

typedef struct
{
	int		width, height;
	qboolean		fullScreen;
	qboolean		wideScreen;

	int		faceCull;
	int		frontFace;
	int		frameBuffer;

	qboolean		drawTrans;
	qboolean		drawProjection;
	qboolean		stencilEnabled;
	qboolean		in2DMode;
} glState_t;

typedef struct
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*version_string;
	const char	*vendor_string;

	// list of supported extensions
	const char	*extensions_string;
	bool		extension[R_EXTCOUNT];

	int		block_size;		// lightmap blocksize
	
	int		max_texture_units;
	GLint		max_2d_texture_size;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_size;
	GLint		texRectangle;

	GLfloat		max_texture_anisotropy;
	GLfloat		max_texture_lodbias;
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

//
// r_backend.cpp
//
void GL_LoadMatrix( const matrix4x4 source );
void GL_LoadTexMatrix( const matrix4x4 source );
void GL_DisableAllTexGens( void );
void GL_FrontFace( GLenum front );
void GL_SetRenderMode( int mode );
void GL_Cull( GLenum cull );
qboolean R_SetupFogProjection( void );
void R_BeginDrawProjection( const plight_t *pl, bool decalPass = false );
void R_EndDrawProjection( void );
int TriSpriteTexture( model_t *pSpriteModel, int frame );
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame );
int R_AllocFrameBuffer( void );
void GL_BindFrameBuffer( int buffer, int texture );
void R_FreeFrameBuffer( int buffer );
void R_SetupOverbright( qboolean active );

//
// r_beams.cpp
//
void R_InitViewBeams( void );
void R_AddServerBeam( cl_entity_t *pEnvBeam );
void CL_DrawBeams( int fTrans );

//
// r_bloom.cpp
//
void R_InitBloomTextures( void );
void R_BloomBlend( const ref_params_t *fd );

//
// r_cull.cpp
//
bool R_CullBoxExt( const mplane_t frustum[6], const Vector &mins, const Vector &maxs, unsigned int clipflags );
bool R_CullSphereExt( const mplane_t frustum[6], const Vector &centre, float radius, unsigned int clipflags );
bool R_CullModel( cl_entity_t *e, const Vector &origin, const Vector &mins, const Vector &maxs, float radius );
bool R_CullSurfaceExt( msurface_t *surf, const mplane_t frustum[6], unsigned int clipflags );
bool R_VisCullBox( const Vector &mins, const Vector &maxs );
bool R_VisCullSphere( const Vector &origin, float radius );

#define R_CullBox( mins, maxs, clipFlags )	R_CullBoxExt( RI.frustum, mins, maxs, clipFlags )
#define R_CullSphere( centre, radius, clipFlags )	R_CullSphereExt( RI.frustum, centre, radius, clipFlags )
#define R_CullSurface( surf, clipFlags )	R_CullSurfaceExt( surf, RI.frustum, clipFlags )

//
// r_debug.c
//
msurface_t *R_TraceLine( pmtrace_t *result, const Vector &start, const Vector &end, int traceFlags );

//
// r_light.c
//
void R_AnimateLight( void );
void R_PushDlights( void );
int R_CountPlights( bool countShadowLights = false );
void R_LightForPoint( const Vector &point, color24 *ambientLight, bool invLight, bool useAmbient, float radius );
void R_RecursiveLightNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags );
void R_GetLightVectors( cl_entity_t *pEnt, Vector &origin, Vector &angles );
void R_SetupLightProjection( plight_t *pl, const Vector &origin, const Vector &angles, float radius, float fov );
void R_MergeLightProjection( plight_t *pl );
void R_SetupLightProjectionTexture( plight_t *pl, cl_entity_t *pEnt );
void R_SetupLightAttenuationTexture( plight_t *pl, int falloff = -1 );
void R_LightStaticBrushes( const plight_t *pl );
plight_t *CL_AllocPlight( int key );
void R_DrawShadowChains( void );
void R_DrawLightChains( void );
void CL_DecayLights( void );
void CL_ClearPlights( void );

//
// r_main.cpp
//
void R_ClearScene( void );
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType );
qboolean R_WorldToScreen( const Vector &point, Vector &screen );
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m );
void R_ScreenToWorld( const Vector &screen, Vector &point );
void R_RenderScene( const ref_params_t *pparams );
int R_ComputeFxBlend( cl_entity_t *e );
void R_LoadIdentity( void );
void R_RotateForEntity( cl_entity_t *e );
void R_TranslateForEntity( cl_entity_t *e );
void R_FindViewLeaf( void );
void R_SetupFrustum( void );

//
// r_mirror.cpp
//
void R_BeginDrawMirror( msurface_t *fa );
void R_EndDrawMirror( void );
bool R_FindMirrors( const ref_params_t *fd );
void R_DrawMirrors( cl_entity_t *ignoreent = NULL );

//
// r_misc.cpp
//
void R_NewMap( void );

//
// r_movie.cpp
//
int R_PrecacheCinematic( const char *cinname );
void R_InitCinematics( void );
void R_FreeCinematics( void );
int R_DrawCinematic( msurface_t *surf, texture_t *t );
int R_AllocateCinematicTexture( unsigned int txFlags );

//
// r_portal.cpp
//
bool R_FindPortals( const ref_params_t *fd );
void R_DrawPortals( cl_entity_t *ignoreent = NULL );

//
// r_screen.cpp
//
void R_BeginDrawScreen( msurface_t *fa );
void R_EndDrawScreen( void );
bool R_FindScreens( const ref_params_t *fd );
void R_DrawScreens( cl_entity_t *ignoreent = NULL );

//
// r_shadows.cpp
//
void R_RenderShadowmaps( void );

//
// r_surf.cpp
//
void R_MarkLeaves( void );
void R_DrawWorld( void );
void R_DrawWaterSurfaces( bool fTrans );
void R_DrawBrushModel( cl_entity_t *e );
void HUD_BuildLightmaps( void );
void HUD_SetOrthoBounds( const float *mins, const float *maxs );

//
// r_textures.cpp
//
qboolean HUD_LoadTextures( const void *in, model_t *out, int *sky1, int *sky2 );

//
// r_util.cpp
//
int WorldToScreen( const Vector &world, Vector &screen );
byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model );
byte *Mod_GetCurrentVis( void );
float V_CalcFov( float &fov_x, float width, float height );
void V_AdjustFov( float &fov_x, float &fov_y, float width, float height, bool lock_x );

//
// r_warp.cpp
//
void R_DrawSkyBox( void );
void R_ClearSkyBox( void );
void EmitWaterPolys( glpoly_t *polys, qboolean noCull, qboolean reflection = false );
void R_RecursiveSkyNode( mnode_t *node, unsigned int clipflags );
void EmitWaterPolysReflection( msurface_t *fa );
void R_CheckSkyPortal( cl_entity_t *skyPortal );
void R_DrawSkyPortal( cl_entity_t *skyPortal );
void R_AddSkyBoxSurface( msurface_t *fa );
void EmitSkyPolys( msurface_t *fa );
void R_DrawSkyChain( msurface_t *s );
void EmitSkyLayers( msurface_t *fa );

#endif//R_LOCAL_H
