/*
gl_local.h - renderer local definitions
this code written for Paranoia 2: Savior modification
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

#ifndef GL_LOCAL_H
#define GL_LOCAL_H

#include "gl_export.h"
#include "ref_params.h"
#include "com_model.h"
#include "pm_movevars.h"
#include "r_studioint.h"
#include "gl_framebuffer.h"
#include "gl_cubemap.h"
#include "gl_frustum.h"
#include "gl_viewport.h"
#include "gl_primitive.h"
#include "gl_shader.h"
#include "cl_dlight.h"
#include "cl_entity.h"
#include "xash3d_features.h"
#include <utlarray.h>
#include "vector.h"
#include <matrix.h>
#include "material.h"

#define ACTUAL_GL_VERSION	30.0f

// limits
#define MAX_REF_STACK		8			// pass depth
#define MAX_VISIBLE_ENTS	4096			// total pack of frame ents
#define MAX_SORTED_FACES	32768			// bmodels only
#define MAX_SUBVIEW_FACES	1024			// mirrors, portals, monitors, water, puddles. NOTE: multipass faces can merge view passes
#define MAX_OCCLUDED_FACES	1024			// mirrors + water
#define MAX_SORTED_MESHES	2048			// studio only
#define MAX_MOVIES			16			// max various movies per level
#define MAX_MOVIE_TEXTURES	64			// max # of unique video textures per level
#define MAX_LIGHTSTYLES		64			// a byte limit, don't modify
#define MAX_LIGHTMAPS		256			// Xash3D supports up to 256 lightmaps
#define MAX_DLIGHTS 		64			// per one frame. unsigned int limit
#define MAX_ENGINE_DLIGHTS 	32
#define MAX_LIGHTCACHE			8196	// unique models with instanced vertex lighting, TODO make growable
#define MAX_SHADOWS				MAX_DLIGHTS
#define MAX_SUBVIEW_TEXTURES	64			// total depth
#define MAX_FRAMEBUFFERS		MAX_SUBVIEW_TEXTURES
#define MAX_FBO_ATTACHMENTS		8			// color attachments per FBO
#define DEFAULT_CUBEMAP_SIZE	128			// same as in Source
#define AMBIENT_EPSILON			0.001f			// to avoid division by zero
#define STAIR_INTERP_TIME		100.0f
#define LIGHT_PROBES			10			// eight OBB corners, center and one reserved slot (NOTE: not all the probes will be used)
#define LIGHT_SAMPLES			8			// GPU limitation for local arrays (very slowly if more than eight elements)
#define MOD_FRAMES				20

#define WATER_TEXTURES	29
#define WATER_ANIMTIME	20.0f

#define INVALID_HANDLE	0xFFFF	// studio cache

#define FLASHLIGHT_KEY	-666
#define SUNLIGHT_KEY	-777

#define LM_SAMPLE_SIZE	16
#define LM_SAMPLE_EXTRASIZE	8

#define BLOCK_SIZE		glConfig.block_size		// lightmap blocksize
#define BLOCK_SIZE_DEFAULT	128			// for keep backward compatibility
#define BLOCK_SIZE_MAX	2048			// must match with engine const!!!
#define SHADOW_SIZE		4096			// atlas size

#define WORLD_MATRIX	0			// must be 0 always
#define REFPVS_RADIUS	2.0f			// PVS radius for rendering
#define Z_NEAR		4.0f
#define Z_NEAR_LIGHT	0.1f
#define BACKFACE_EPSILON	0.01f

#define CVAR_TO_BOOL( x )	((x) && ((x)->value != 0.0f) ? true : false )

// VBO offsets
#define OFFSET( type, var )	((const void *)&(((type *)NULL)->var))
#define R_OpaqueEntity( e )	(( (e)->curstate.rendermode == kRenderNormal ) || ( (e)->curstate.rendermode == kRenderTransAlpha ))
#define R_ModelOpaque( rm )	(( rm == kRenderNormal ) || ( rm == kRenderTransAlpha ))
#define R_StaticEntity( e )	( (e)->origin == g_vecZero && (e)->angles == g_vecZero )
#define RP_OUTSIDE( leaf )	(((( leaf ) - worldmodel->leafs ) - 1 ) == -1 )
#define R_WaterEntity( m )	( FBitSet( m->flags, BIT( 2 )))

#define ScreenCopyRequired( x )	((x) && FBitSet( (x)->status, SHADER_USE_SCREENCOPY ))
#define ShaderUseCubemaps( x )	((x) && FBitSet( (x)->status, SHADER_USE_CUBEMAPS ))

// refparams
enum RefParams
{
	RP_NONE				= 0,
	RP_MIRRORVIEW		= BIT(0),	// lock pvs at vieworg
	RP_ENVVIEW			= BIT(1),	// used for cubemapshot
	RP_CLIPPLANE		= BIT(2),	// mirrors used
	RP_DRAW_WORLD		= BIT(3),	// otherwise it's player customization window
	RP_DRAW_OVERVIEW	= BIT(4),	// dev_overview is active, draw orthogonal projection
	RP_MERGE_PVS		= BIT(5),	// merge PVS with previous pass
	RP_SKYVIEW			= BIT(6),	// render skyonly
	RP_SKYPORTALVIEW	= BIT(7),	// view through env_sky camera
	RP_PORTALVIEW		= BIT(8),	// view through portal
	RP_SCREENVIEW		= BIT(9),	// view through screen
	RP_SHADOWVIEW		= BIT(10),	// view through light
	RP_NOSHADOWS		= BIT(11),	// disable shadows for this pass
	RP_WATERPASS		= BIT(12),	// it's mirorring plane for water surface
	RP_NOGRASS			= BIT(13),	// don't draw grass
	RP_DEFERREDSCENE	= BIT(14),	// render scene into geometry buffer
	RP_DEFERREDLIGHT	= BIT(15),	// render scene into low-res lightmap
	RP_THIRDPERSON		= BIT(16),	// camera is thirdperson
	RP_FORCE_NOPLAYER	= BIT(17)	// ignore player drawing in some special cases
};

inline RefParams operator|(RefParams a, RefParams b)
{
	return static_cast<RefParams>(static_cast<int>(a) | static_cast<int>(b));
}

// RI->view.changed
#define RC_ORIGIN_CHANGED	BIT( 0 )	// origin is changed from the previous frame
#define RC_ANGLES_CHANGED	BIT( 1 )	// angles is changed from the previous frame
#define RC_VIEWLEAF_CHANGED	BIT( 2 )	// viewleaf is changed
#define RC_FOV_CHANGED		BIT( 3 )	// FOV is changed
#define RC_PVS_CHANGED		BIT( 4 )	// now our PVS was potentially changed :-)
#define RC_FRUSTUM_CHANGED	BIT( 5 )	// now our frustum was potentially changed :-)
#define RC_FORCE_UPDATE		BIT( 6 )	// some cvar manipulations invoke updates of visible list

// RI->view.flags
#define RF_SKYVISIBLE	BIT( 0 )	// sky is visible for this frame
#define RF_HASDYNLIGHTS	BIT( 1 )	// pass have dynlights

#define RP_NONVIEWERREF	(RP_MIRRORVIEW|RP_PORTALVIEW|RP_SCREENVIEW|RP_ENVVIEW|RP_SHADOWVIEW|RP_SKYVIEW)
#define RP_LOCALCLIENT( e )	(gEngfuncs.GetLocalPlayer() && ((e)->index == gEngfuncs.GetLocalPlayer()->index && e->curstate.entityType == ET_PLAYER ))
#define RP_NORMALPASS()	( FBitSet( RI->params, RP_NONVIEWERREF ) == 0 )
#define RP_CUBEPASS()	( FBitSet( RI->params, RP_SKYVIEW|RP_ENVVIEW ))

#define TF_LIGHTMAP		(TF_NOMIPMAP|TF_ATLAS_PAGE|TF_HAS_ALPHA)
#define TF_DELUXMAP		(TF_NOMIPMAP|TF_NORMALMAP|TF_ATLAS_PAGE)
#define TF_IMAGE		(TF_NOMIPMAP|TF_CLAMP)
#define TF_SCREEN		(TF_NOMIPMAP|TF_CLAMP)
#define TF_SPOTLIGHT	(TF_NOMIPMAP|TF_BORDER)
#define TF_SHADOW		(TF_NOMIPMAP|TF_CLAMP|TF_DEPTHMAP|TF_LUMINANCE)
#define TF_SHADOW_CUBEMAP	(TF_NOMIPMAP|TF_CLAMP|TF_CUBEMAP|TF_DEPTHMAP|TF_LUMINANCE)
#define TF_RECTANGLE_SCREEN	(TF_RECTANGLE|TF_NOMIPMAP|TF_CLAMP)
#define TF_DEPTH		(TF_NOMIPMAP|TF_CLAMP|TF_NEAREST|TF_DEPTHMAP|TF_LUMINANCE|TF_NOCOMPARE)
#define TF_GRASS		(TF_CLAMP)
#define TF_DEPTHBUFF	(TF_DEPTHMAP|TF_LUMINANCE|TF_NOCOMPARE)
#define TF_STORAGE		(TF_NEAREST|TF_RECTANGLE|TF_ARB_FLOAT|TF_HAS_ALPHA|TF_CLAMP|TF_NOMIPMAP)

#define TF_DEPTHBUFFER	(TF_SCREEN|TF_DEPTHMAP|TF_NEAREST|TF_NOCOMPARE)
#define TF_COLORBUFFER	(TF_SCREEN|TF_NEAREST)

#define CULL_VISIBLE	0	// not culled
#define CULL_BACKSIDE	1	// backside of transparent wall
#define CULL_FRUSTUM	2	// culled by frustum
#define CULL_OTHER		3	// culled by other reason

#define TF_RT_COLOR		(TF_NEAREST|TF_CLAMP|TF_NOMIPMAP)
#define TF_RT_NORMAL	(TF_NEAREST|TF_CLAMP|TF_NOMIPMAP|TF_NORMALMAP)
#define TF_RT_DEPTH		(TF_NEAREST|TF_CLAMP|TF_NOMIPMAP|TF_DEPTHMAP|TF_NOCOMPARE)

#define MAT_ALL_EFFECTS	(BRUSH_HAS_BUMP|BRUSH_HAS_SPECULAR|BRUSH_REFLECT|BRUSH_FULLBRIGHT)

#define FBO_MAIN		0

// light types
#define LIGHT_SPOT		0	// standard projection light
#define LIGHT_OMNI		1	// omnidirectional light
#define LIGHT_DIRECTIONAL	2	// parallel light (sun light)

// helpers
#define GetVForward()	Vector( RI->view.matrix[0] )
#define GetVRight()		Vector( RI->view.matrix[1] )
#define GetVLeft()		-Vector(RI->view.matrix[1] )
#define GetVUp()		Vector( RI->view.matrix[2] )
#define GetVieworg()	RI->view.origin

typedef enum
{
	DRAWLIST_ALL = 0,	// special case for decals
	DRAWLIST_SOLID,
	DRAWLIST_TRANS,
	DRAWLIST_LIGHT,
	DRAWLIST_SUBVIEW,
	DRAWLIST_SHADOW,
} drawlist_t;

enum
{
	BUMP_BASELIGHT_STYLE	= 61,
	BUMP_ADDLIGHT_STYLE		= 62,
	BUMP_LIGHTVECS_STYLE	= 63,
};

class DecalGroupEntry;
typedef int (*cmpfunc)( const void *a, const void *b );
typedef void (*pfnShaderCallback)( struct glsl_prog_s *shader );

// multiple output draw buffers
typedef struct
{
	char		name[32];
	int		width;
	int		height;
	int		depth;
	int		colortarget[MAX_FBO_ATTACHMENTS];
	int		depthtarget;
	uint		id;
} gl_drawbuffer_t;

typedef struct gl_fbo_s
{
	GLboolean		init;
	GLuint		framebuffer;
	GLuint		renderbuffer;
	int		texture;
} gl_fbo_t;

typedef struct gl_movie_s
{
	char		name[32];
	void		*state;
	float		length;		// total cinematic length
	int			xres, yres;	// size of cinematic
} gl_movie_t;

typedef struct gl_texbuffer_s
{
	int		framebuffer;
	int		texturenum;
	int		texframe;		// this frame texture was used
	matrix4x4		matrix;		// texture matrix
} gl_texbuffer_t;

class gl_state_t
{
public:
	GLfloat		modelMatrix[16];	// matrix4x4( origin, angles, scale )
	matrix4x4		transform;	// entity transformation matrix 
	const Vector GetModelOrigin( void );
};

typedef enum
{
	LM_FREE = 0,	// lightmap is clear
	LM_USED,		// partially used, has free space
	LM_DONE,		// completely full
} lmstate_t;

typedef struct
{
	lmstate_t		state;
	unsigned short	allocated[BLOCK_SIZE_MAX];
	int		lightmap;
	int		deluxmap;	
} gl_lightmap_t;

typedef struct
{
	unsigned short	allocated[SHADOW_SIZE];
	CFrameBuffer	shadowmap;
} gl_shadowmap_t;

typedef struct
{
	Vector		diffuse;		// direct light color
	Vector		normal;		// direct light normal
	int		ambientlight;	// clip at 128
	int		shadelight;	// clip at 192 - ambientlight
	Vector		ambient[6];	// cubemap 1x1 (single pixel per side)
	bool		nointerp;		// flickering light force nointerp
} mstudiolight_t;

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
	matdesc_t		*material[MAX_LANDSCAPE_LAYERS];	// layer settings
	float		smoothness[MAX_LANDSCAPE_LAYERS];	// shader params
	unsigned short	gl_diffuse_id;			// diffuse texture array
	unsigned short	gl_detail_id;			// detail texture
	unsigned short	gl_normalmap_id;			// normalmap array
	unsigned short	gl_specular_id;			// specular array
} layerMap_t;

typedef struct terrain_s
{
	char		name[16];
	indexMap_t	indexmap;
	layerMap_t	layermap;
	int		numLayers;	// count of array textures
	int		tessSize;
	float		texScale;		// global texture scale
	bool		valid;		// if heightmap was actual
} terrain_t;

typedef struct
{
	int		changed;				// whats changed from last frame
	int		flags;				// some info about current frame (sets by renderer)

	int		entity;				// playernum or camera edict
	Vector		origin;
	Vector		angles;
	Vector		pvspoint;
	ref_overview_t	over;				// cached overview

	int		client_frame;			// cached client frame
	bool		novis_cached;			// last value of r_novis variable
	bool		lockpvs_cached;			// last value of r_lockpvs variable

	float		fov_x;				// actual fov_x
	float		fov_y;				// actual fov_y
	float		farClip;
	float		planedist;			// for sort translucent surfaces
	float		lodScale;

	CViewport	port;				// cached view.port
	CFrustum	frustum;			// view frustum
	CFrustum	splitFrustum[MAX_SHADOWMAPS];
	float		parallelSplitDistances[MAX_SHADOWMAPS];	// distances in camera space

	matrix4x4		matrix;				// untransformed viewmatrix
	matrix4x4		worldMatrix;			// modelview for world
	matrix4x4		projectionMatrix;			// gl frustum
	matrix4x4		worldProjectionMatrix;		// worldviewMatrix * projectionMatrix

	mleaf_t		*leaf;				// leaf where are vieworg located

	vec3_t		visMins;				// visMins used for compute precision farclip
	vec3_t		visMaxs;

	float		skyMins[2][6];			// sky texcoords
	float		skyMaxs[2][6];			// sky texcoords

	byte		pvsarray[(MAX_MAP_LEAFS+7)/8];	// actual PVS for current frame
	byte		visfaces[(MAX_MAP_FACES+7)/8];	// actual visible faces for current frame (world only)
	byte		vislight[(MAX_MAP_WORLDLIGHTS+7)/8];	// visible lights per current frame
} ref_viewcache_t;

typedef struct
{
	// forward rendering lists
	CUtlArray<CSolidEntry>	solid_faces;
	CUtlArray<CSolidEntry>	solid_meshes;
	CUtlArray<CTransEntry>	trans_list;
	CUtlArray<struct grass_s*>	grass_list;
	CUtlArray<Vector>		primverts;		// primitive vertexes

	// forward lighting lists
	CUtlArray<CSolidEntry>	light_faces;
	CUtlArray<CSolidEntry>	light_meshes;
	CUtlArray<struct grass_s*>	light_grass;

	msurface_t		*subview_faces[MAX_SUBVIEW_FACES];	// 6 kb
	int			num_subview_faces;
} ref_drawlist_t;

// contain gl-friendly data that may keep from previous frame
// to avoid to recompute it again
typedef struct
{
	CViewport	viewport;		// in OpenGL space
	GLfloat		modelviewMatrix[16];	// worldviewMatrix
	GLfloat		projectionMatrix[16];	// projection matrix
	GLfloat		modelviewProjectionMatrix[16];// send to engine
} ref_glstate_t;

typedef struct
{
	RefParams params; // rendering parameters

	// NEW STATE
	ref_viewcache_t	view;		// cached view
	ref_glstate_t	glstate;		// cached glstate
	ref_drawlist_t	frame;		// frame lists

	// GLOBAL STATE
	mplane_t		clipPlane;
	cl_entity_t	*currententity;
	model_t		*currentmodel;
	CDynLight		*currentlight;
	struct glsl_prog_s	*currentshader;
	msurface_t	*reject_face;	// avoid recursion to himself
} ref_instance_t;

/*
=======================================================================

 GL STATE MACHINE

=======================================================================
*/
enum
{
	R_OPENGL_110 = 0,		// base
	R_OPENGL_200,
	R_WGL_PROCADDRESS,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_ARB_VERTEX_ARRAY_OBJECT_EXT,
	R_TEXTURE_ARRAY_EXT,	// shaders only
	R_EXT_GPU_SHADER4,		// shaders only
	R_DRAW_BUFFERS_EXT,
	R_ARB_MULTITEXTURE,
	R_TEXTURECUBEMAP_EXT,
	R_SHADER_GLSL100_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_TEXTURE_3D_EXT,
	R_SHADER_OBJECTS_EXT,
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_ARB_TEXTURE_NPOT_EXT,
	R_TEXTURE_2D_RECT_EXT,
	R_DEPTH_TEXTURE,
	R_SHADOW_EXT,
	R_FRAMEBUFFER_OBJECT,
	R_SEPARATE_BLENDFUNC_EXT,
	R_OCCLUSION_QUERIES_EXT,
	R_SEAMLESS_CUBEMAP,
	R_BINARY_SHADER_EXT,
	R_PARANOIA_EXT,		// custom OpenGL32.dll with hacked function glDepthRange
	R_DEBUG_OUTPUT,
	R_KHR_DEBUG,
	R_ARB_PIXEL_BUFFER_OBJECT,
	R_EXTCOUNT,		// must be last
};

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
	GL_TEXTURE8,
	GL_TEXTURE9,
	GL_TEXTURE10,
	GL_TEXTURE11,
	MAX_TEXTURE_UNITS
};

typedef struct
{
	bool		fCustomRendering;
	bool		fClearScreen;			// force clear if world shaders failed to build
	int		fGamePaused;

	double		time;				// cl.time
	double		oldtime;				// cl.oldtime
	double		frametime;			// special frametime for multipass rendering (will set to 0 on a nextview)
	double		saved_frametime;			// push\pop
	ref_params_t viewparams;

	cl_entity_t	*draw_entities[MAX_VISIBLE_ENTS];	// list of all pending entities for current frame
	int		num_draw_entities;			// count for actual rendering frame

	int		defaultTexture;   	// use for bad textures
	int		skyboxTextures[6];	// skybox sides
	int		normalmapTexture;	// default normalmap
	int		deluxemapTexture;	// default deluxemap
	int		vsdctCubeTexture;	// Virtual Shadow Depth Cubemap Texture
	int		whiteCubeTexture;	// stub
	int		blackCubeTexture;	// stub
	int		depthTexture;	// stub
	int		depthCubemap;	// stub
	int		normalsFitting;	// best fit normals
	int		brdfApproxTexture; // BRDF look-up table texture for PBR lighting
	int		defaultProjTexture;	// fallback for missed textures
	int		flashlightTexture;	// flashlight projection texture
	int		spotlightTexture[8];// reserve for eight textures
	int		cinTextures[MAX_MOVIE_TEXTURES];
	gl_texbuffer_t	subviewTextures[MAX_SUBVIEW_TEXTURES];
	int		shadowTextures[MAX_SHADOWS];
	int		shadowCubemaps[MAX_SHADOWS];
	int		waterTextures[WATER_TEXTURES];
	int		num_2D_shadows_used;	// used shadow textures per full frame
	int		num_CM_shadows_used;	// used shadow textures per full frame
	int		num_subview_used;		// used mirror textures per full frame
	int		num_cin_used;		// used movie textures per full frame

	int		screen_color;
	int		screen_depth;
	gl_drawbuffer_t *screencopy_fbo;

	int		grayTexture;
	int		whiteTexture;
	int		blackTexture;
	int		screenTexture;

	CUtlArray<gl_state_t>	cached_state;

	gl_lightmap_t	lightmaps[MAX_LIGHTMAPS];
	byte		current_lightmap_texture;
	int		packed_lights_texture;
	int		packed_planes_texture;
	int		packed_nodes_texture;
	int		packed_models_texture;

	gl_shadowmap_t	shadowmap;	// single atlas

	// framebuffers
	CFrameBuffer	fbo_shadow2D;	// used for projection shadowmapping
	CFrameBuffer	fbo_shadowCM;	// used for omnidirectional shadowmapping
	CFrameBuffer	sunShadowFBO[MAX_SHADOWMAPS];	// extra-large shadowmap for sun rendering
	CFrameBuffer	fbo_light;	// store lightmap
	CFrameBuffer	fbo_filter;	// store filtered lightmap
	CFrameBuffer	fbo_shadow;	// store shadowflags

	// deffered rendering framebuffers
	gl_drawbuffer_t	*defscene_fbo;
	gl_drawbuffer_t	*deflight_fbo;
	word		defSceneShader[2];	// geometry pass
	word		defLightShader;	// light pass
	word		defDynLightShader[2];// dynamic light pass
	word		bilateralShader;	// upscale filter

	// HDR rendering stuff
	gl_drawbuffer_t *screen_hdr_fbo;
	gl_drawbuffer_t *screen_multisample_fbo;
	uint	screen_hdr_fbo_mip[6];
	uint	screen_hdr_fbo_texture_color;
	uint	screen_hdr_fbo_texture_depth;
	uint	screen_multisample_fbo_texture_color;
	uint	screen_multisample_fbo_texture_depth;

	// skybox shaders
	word		skyboxEnv[2];	// skybox & sun
	word		defSceneSky;	// skybox & sun
	word		defLightSky;	// skybox & sun

	// framebuffers
	gl_fbo_t		frame_buffers[MAX_FRAMEBUFFERS];
	int		num_framebuffers;

	int		realframecount;	// not including passes
	int		grassunloadframe;	// unload too far grass to save video memory

	Vector		ambientLight;	// at vieworg
	int		waterlevel;	// player waterlevel
	cl_entity_t	*waterentity;	// player inside

	// fog params
	bool		fogEnabled;
	Vector		fogColor;
	float		fogDensity;
	float		fogSkyDensity;

	// sky params
	bool		ignore_2d_skybox;	// we already draw 3d skybox, so don't overwrite it in current pass
	cl_entity_t *sky_camera;
	Vector		sky_normal;		// sky vector
	Vector		sky_ambient;	// sky ambient color

	// global ambient\direct factors
	float		ambientFactor;
	float		diffuseFactor;

	float		sun_refract;
	float		sun_ambient;
	Vector		sun_diffuse;

	CDynLight		*sunlight;		// sun is active if not a NULL

	Vector		screen_normals[4];		// helper to transform world->screen space

	float		farclip;			// max viewable distance
	float		gravity;			// particles used

	float		lightstyle[MAX_LIGHTSTYLES]; // value in range [0.0; 550.0]
	gl_movie_t	cinematics[MAX_MOVIES];	// precached cinematics

	movevars_t	*movevars;

	// generic light that used to cache shaders
	CDynLight		defaultlightSpot;
	CDynLight		defaultlightOmni;
	CDynLight		defaultlightProj;

	matdesc_t		*materials;
	int				matcount;

	bool		params_changed;		// some cvars are toggled, shaders needs to recompile and resort
	bool		local_client_added;		// indicate what a local client already been added into renderlist
	bool		sun_light_enabled;		// map have a light_environment with valid direction
	bool		lighting_changed;		// r_lighting_modulate was changed
	bool		shadows_notsupport;		// no shadow textures
	bool		show_uniforms_peak;		// print the maxcount of used uniforms
	int		glsl_valid_sequence;	// reloas shaders while some render cvars was changed
	int		total_vbo_memory;		// statistics
	Vector4D		gamma_table[64];

	CDynLight		dlights[MAX_DLIGHTS];

	vec3_t		ambient_color;
	float		direct_scale;
	float		light_gamma;
	float		light_threshold;
	float		smoothing_threshold;

	// original player vieworg and angles
	Vector		cached_vieworigin;
	Vector		cached_viewangles;

	struct mvbocache_s	*vertex_light_cache[MAX_LIGHTCACHE];	// FIXME: make growable
	struct mvbocache_s	*surface_light_cache[MAX_LIGHTCACHE];

	// cull info
	Vector		modelorg;		// relative to viewpoint
} ref_globals_t;

typedef struct
{
	unsigned int	c_world_leafs;
	unsigned int	c_world_nodes;	// walking by BSP tree

	unsigned int	c_culled_entities;
	unsigned int	c_total_tris;	// triangle count

	unsigned int	c_mirror_passes;
	unsigned int	c_portal_passes;
	unsigned int	c_screen_passes;
	unsigned int	c_shadow_passes;
	unsigned int	c_sky_passes;

	unsigned int	c_worldlights;
	unsigned int	c_occlusion_culled;	// culled by occlusion query

	unsigned int	c_screen_copy;	// how many times screen was copied

	unsigned int	num_shader_binds;
	unsigned int	num_flushes;

	msurface_t	*debug_surface;
} ref_stats_t;

typedef struct
{
	double		compile_shaders;
	double		create_light_cache;
	double		create_buffer_object;
	double		total_buildtime;
} ref_buildstats_t;

typedef enum
{
	GLHW_GENERIC,		// where everthing works the way it should
	GLHW_RADEON,		// where you don't have proper GLSL support
	GLHW_NVIDIA		// Geforce 8/9 class DX10 hardware
} glHWType_t;

typedef struct
{
	int		width, height;
	int		defWidth, defHeight;
	qboolean		fullScreen;
	qboolean		wideScreen;

	int		faceCull;
	int		frontFace;
	int		frameBuffer;

	GLfloat		depthmin;
	GLfloat		depthmax;
	GLint		depthmask;
	GLfloat		identityMatrix[16];

	ref_instance_t	stack[MAX_REF_STACK];
	GLuint		stack_position;
} glState_t;

typedef struct
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*version_string;
	const char	*vendor_string;

	glHWType_t	hardware_type;
	float		version;

	// list of supported extensions
	const char	*extensions_string;
	bool		extension[R_EXTCOUNT];

	int		block_size;		// lightmap blocksize
	
	int		max_texture_units;
	GLint		max_2d_texture_size;
	GLint		max_3d_texture_size;
	GLint		max_2d_texture_layers;
	GLint		max_cubemap_size;
	GLint		binary_formats;
	GLint		num_formats;

	int		max_vertex_uniforms;
	int		max_vertex_attribs;
	int		max_varying_floats;
	int		max_skinning_bones;		// total bones that can be transformed with GLSL
	int		peak_used_uniforms;
} glConfig_t;

extern glState_t		glState;
extern glConfig_t		glConfig;
extern engine_studio_api_t	IEngineStudio;
extern float		gldepthmin, gldepthmax;
extern int		sunSize[MAX_SHADOWMAPS];
extern char		r_speeds_msg[2048];
extern char		r_depth_msg[2048];
extern model_t		*worldmodel;
//extern int		g_iGunMode;
extern ref_stats_t		r_stats;
extern ref_buildstats_t	r_buildstats;
extern ref_instance_t	*RI;
extern ref_globals_t	tr;

//
// gl_backend.cpp
//
void R_InitRefState(void);
void R_PushRefState(void);
void R_PopRefState(void);
void R_ResetRefState(void);
ref_instance_t *R_GetPrevInstance(void);
void CompressNormalizedVector(char outVec[3], const Vector &inVec);
bool GL_BackendStartFrame(ref_viewpass_t *rvp, RefParams params);
void GL_BackendEndFrame(ref_viewpass_t *rvp, RefParams params);
int R_GetSpriteTexture(const model_t *m_pSpriteModel, int frame);
void GL_BindDrawbuffer(gl_drawbuffer_t *framebuffer);
void GL_DepthRange(GLfloat depthmin, GLfloat depthmax);
void R_RenderQuadPrimitive(CSolidEntry *entry);
int GL_TextureMipCount(int width, int height);
void GL_LoadMatrix(const matrix4x4 &source);
void GL_LoadTexMatrix(const matrix4x4 &source);
void GL_BindFrameBuffer(int buffer, int texture);
void R_Speeds_Printf(const char *msg, ...);
int R_AllocFrameBuffer(const CViewport &viewport);
void GL_CheckVertexArrayBinding(void);
void R_FreeFrameBuffer(int buffer);
void GL_CleanupAllTextureUnits(void);
void GL_ComputeScreenRays(void);
void GL_DisableAllTexGens(void);
void GL_DepthMask(GLint enable);
void GL_FrontFace(GLenum front);
void GL_ClipPlane(bool enable);
void GL_BindFBO(GLuint buffer);
void GL_AlphaTest(GLint enable);
void GL_DepthTest(GLint enable);
void GL_CleanupDrawState(void);
void GL_SetDefaultState(void);
void GL_Blend(GLint enable);
void GL_Cull(GLenum cull);
void GL_Setup2D(void);
void GL_Setup3D(void);

//
// gl_cubemaps.cpp
//
void CL_FindNearestCubeMap( const Vector &pos, mcubemap_t **result );
void CL_FindTwoNearestCubeMap( const Vector &pos, mcubemap_t **result1, mcubemap_t **result2 );
void CL_FindNearestCubeMapForSurface( const Vector &pos, const msurface_t *surf, mcubemap_t **result );
void CL_FindTwoNearestCubeMapForSurface( const Vector &pos, const msurface_t *surf, mcubemap_t **result1, mcubemap_t **result2 );
Vector CL_GetCubemapSideViewangles(int side);
void CL_BuildCubemaps_f( void );
void R_InitCubemaps();
void R_InitCubemapShaders();
void Mod_LoadCubemaps(const byte *base, const dlump_t *l);
void Mod_FreeCubemaps();

//
// gl_cull.cpp
//
bool R_CullModel( cl_entity_t *e, const Vector &mins, const Vector &maxs );
int R_CullSurface( msurface_t *surf, const Vector &vieworg, CFrustum *frustum, int clipFlags = 0 );
bool R_CullBrushModel( cl_entity_t *e );
bool R_CullNodeTopView( mnode_t *node );

#define R_CullBox( mins, maxs )		( RI->view.frustum.CullBox( mins, maxs ))
#define R_CullSphere( centre, radius )		( RI->view.frustum.CullSphere( centre, radius ))
#define R_CullFrustum( otherFrustum )		( RI->view.frustum.CullFrustum( otherFrustum ))

//
// gl_deferred.cpp
//
void GL_SetupGBuffer( void );
void GL_ResetGBuffer( void );
void GL_DrawDeferredPass( void );

//
// gl_framebuffer.c
//
gl_drawbuffer_t *GL_AllocDrawbuffer(const char *name, int width, int height, int depth = 1);
void GL_ResizeDrawbuffer(gl_drawbuffer_t *fbo, int width, int height, int depth = 1);
void GL_AttachColorTextureToFBO(gl_drawbuffer_t *fbo, int texture, int colorIndex, int cubemapSide = 0, int mipLevel = 0);
void GL_AttachDepthTextureToFBO(gl_drawbuffer_t *fbo, int texture, int side = 0);
void GL_CheckFBOStatus(gl_drawbuffer_t *fbo);
void GL_VidInitDrawBuffers();
void GL_FreeDrawbuffers();
void GL_FreeDrawbuffer(gl_drawbuffer_t *fbo);

//
// gl_lightmap.cpp
//
void R_UpdateSurfaceParams( msurface_t *surf );
void R_UpdateSurfaceParams( struct mstudiosurface_s *surf );
void GL_BeginBuildingLightmaps( void );
void GL_AllocLightmapForFace( msurface_t *surf );
bool GL_AllocLightmapForFace( struct mstudiosurface_s *surf );
void GL_EndBuildingLightmaps( bool lightmap, bool deluxmap );
void R_TextureCoords( msurface_t *surf, const Vector &vec, float *out );
void R_GlobalCoords( msurface_t *surf, const Vector &point, float *out );
void R_GlobalCoords( msurface_t *surf, const Vector &point, const Vector &absmin, const Vector &absmax, float scale, float *out );
void R_LightmapCoords( msurface_t *surf, const Vector &vec, float *coords, int style );
void R_LightmapCoords( struct mstudiosurface_s *surf, const Vector &vec, const Vector lmvecs[2], float *coords, int style );

//
// gl_dlight.cpp
//
CDynLight *CL_AllocDlight( int key );
void R_SetupLightParams( CDynLight *pl, const Vector &origin, const Vector &angles, float radius, float fov, int type, int flags = 0 );
void R_FindWorldLights( const Vector &origin, const Vector &mins, const Vector &maxs, byte lights[MAXDYNLIGHTS], bool skipZ = false );
void R_LightForStudio( const Vector &point, mstudiolight_t *light, bool ambient );
void R_PointAmbientFromLeaf( const Vector &point, mstudiolight_t *light );
void R_LightForSky( const Vector &point, mstudiolight_t *light );
void R_LightVec( const Vector &point, mstudiolight_t *light, bool ambient );
Vector R_LightsForPoint( const Vector &point, float radius );
void R_SetupLightTexture( CDynLight *pl, int texture );
void R_SetupDynamicLights( void );
void CL_ClearDlights( void );
void R_AnimateLight( void );
int HasDynamicLights( void );
int HasStaticLights( void );
void CL_DecayLights( void );
bool R_UseSkyLightstyle(int lightstyleIndex);

//
// gl_rmain.cpp
//
void R_ClearScene( void );
int R_ComputeFxBlend( cl_entity_t *e );
void R_RenderScene( const ref_viewpass_t *rvp, RefParams params );
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType );
bool R_WorldToScreen( const Vector &point, Vector &screen );
void R_ScreenToWorld( const Vector &screen, Vector &point );
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m );
unsigned short GL_CacheState( const Vector &origin, const Vector &angles, bool skyentity = false );
void R_MarkWorldVisibleFaces( model_t *model );
gl_state_t *GL_GetCache( word hCachedMatrix );
void R_DrawParticles( qboolean trans );
void R_SetupGLstate( void );
void R_RenderTransList( void );
void R_SetupFrustum( void );
void R_Clear( int bitMask );

//
// gl_rmisc.cpp
//
void R_NewMap( void );
void R_VidInit( void );
void CL_InitMaterials( void );
matdesc_t *CL_FindMaterial( const char *name );
void R_LoadLandscapes( const char *filename );
terrain_t *R_FindTerrain( const char *texname );
void R_InitDynLightShaders( void );
void R_InitShadowTextures( void );
void R_FreeLandscapes( void );
byte R_LightToTexGamma( byte input );

//
// gl_rsurf.cpp
//
texture_t *R_TextureAnimation( msurface_t *s );
void GL_InitRandomTable( void );

//
// gl_shader.cpp
//
const char *GL_PretifyListOptions( const char *options, bool newlines = false );
word GL_FindUberShader( const char *glname, const char *options = "" );
word GL_FindShader( const char *glname, const char *vpname, const char *fpname, const char *options = "" );
void GL_SetShaderDirective( char *options, const char *directive );
void GL_AddShaderDirective( char *options, const char *directive );
void GL_AddShaderFeature( word shaderNum, int feature );
void GL_CheckTextureAlpha( char *options, int texturenum );
void GL_EncodeNormal( char *options, int texturenum );
void GL_BindShader( struct glsl_prog_s *shader );
void GL_FreeUberShaders( void );
void GL_InitGPUShaders( void );
void GL_FreeGPUShaders( void );

//
// gl_shadows.cpp
//
void R_RenderShadowmaps( void );
void R_RenderDeferredShadows( void );

//
// gl_movie.cpp
//
void R_InitCinematics( void );
void R_FreeCinematics( void );
int R_PrecacheCinematic( const char *cinname );
int R_AllocateCinematicTexture( unsigned int txFlags );
void R_UpdateCinematic( const msurface_t *surf );
void R_UpdateCinSound( cl_entity_t *e );

//
// gl_mirror.cpp
//
void R_RenderSubview( void );

//
// gl_scene.cpp
//
void R_CheckChanges( void );
void R_InitDefaultLights( void );
bool R_FullBright();

//
// gl_sky.cpp
//
void R_AddSkyBoxSurface( msurface_t *fa );
void R_DrawSkyBox( void );
void R_CheckSkyPortal(cl_entity_t *skyPortal);

//
// gl_postprocess.cpp
//
void InitPostTextures();
void InitPostEffects();
void InitPostprocessShaders();
void RenderDOF();
void RenderUnderwaterBlur();
void RenderNerveGasBlur();
void RenderPostprocessing();
void RenderSunShafts();
void RenderBloom();
void RenderTonemap();
void RenderFSQ(int wide, int tall);
void RenderAverageLuminance();

//
// gl_world_new.cpp
//
void Mod_ThrowModelInstances( void );
void Mod_PrepareModelInstances( void );
void GL_LoadAndRebuildCubemaps( RefParams refParams );
void Mod_SetOrthoBounds( const float *mins, const float *maxs );
bool Mod_CheckLayerNameForSurf( msurface_t *surf, const char *checkName );
bool Mod_CheckLayerNameForPixel( mfaceinfo_t *land, const Vector &point, const char *checkName );
int Mod_FatPVS( model_t *model, const vec3_t org, float radius, byte *visbuffer, int visbytes, bool merge, bool fullvis );
void Mod_FindStaticLights( byte *vislight, byte lights[MAXDYNLIGHTS], const Vector &origin );
void R_ProcessWorldData( model_t *mod, qboolean create, const byte *buffer );
bool R_AddSurfaceToDrawList( msurface_t *surf, drawlist_t type );
void R_MarkVisibleLights( byte lights[MAXDYNLIGHTS] );
gl_texbuffer_t *Surf_GetSubview( mextrasurf_t *es );
void R_RenderTransSurface( CTransEntry *entry );
int Mod_SampleSizeForFace( msurface_t *surf );
bool Surf_CheckSubview( mextrasurf_t *es, bool puddle = false );
void R_RenderDynLightList( bool solid );
void R_MarkSubmodelVisibleFaces( void );
void Mod_InitBSPModelsTexture( void );
void R_UpdateSubmodelParams( void );
void Mod_ResortFaces( void );

//
// gl_world.cpp
//
void R_RenderDeferredBrushList( void );
void R_RenderSolidBrushList( void );
void R_RenderShadowBrushList( void );
void R_RenderSurfOcclusionList( void );

//
// gl_export.cpp
//
bool GL_Init(void);
void GL_MapChanged(void);
void GL_Shutdown(void);
bool GL_Support(int r_ext);
bool GL_SupportExtension(const char *name);
void R_VidInit(void);

#endif//GL_LOCAL_H
