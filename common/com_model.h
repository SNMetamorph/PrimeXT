/*
com_model.h - cient model structures
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef COM_MODEL_H
#define COM_MODEL_H

#include "build.h"
#include "bspfile.h"	// we need some declarations from it
#include "shader.h"
#include "lightlimits.h"
#include "const.h"
#include "vector.h"
#include "basetypes.h"
#include <memory>

/*
==============================================================================

	ENGINE MODEL FORMAT
==============================================================================
*/
#define STUDIO_RENDER	BIT( 0 )
#define STUDIO_EVENTS	BIT( 1 )
#define STUDIO_FORCE	BIT( 2 )
#define STUDIO_LOCAL_SPACE	BIT( 3 )

#define ZISCALE		((float)0x8000)

#define MIPLEVELS		4
#define VERTEXSIZE		7
#define NUM_AMBIENTS	4		// automatic ambient sounds

// model types
typedef enum
{
	mod_bad = -1,
	mod_brush, 
	mod_sprite, 
	mod_alias, 
	mod_studio
} modtype_t;

//  model flags (stored in model_t->flags)
#define MODEL_CONVEYOR		BIT( 0 )
#define MODEL_HAS_ORIGIN		BIT( 1 )
#define MODEL_LIQUID		BIT( 2 )	// model has only point hull
#define MODEL_TRANSPARENT		BIT( 3 )	// have transparent surfaces
#define MODEL_COLORED_LIGHTING	BIT( 4 )	// lightmaps stored as RGB

#define MODEL_WORLD		BIT( 29 )	// it's a worldmodel
#define MODEL_CLIENT		BIT( 30 )	// client sprite

class CMeshDesc;

typedef struct mplane_s
{
	vec3_t		normal;
	float		dist;
	byte		type;		// for fast side tests
	byte		signbits;		// signx + (signy<<1) + (signz<<1)
	byte		pad[2];
} mplane_t;

typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

// brush material flags
#define BRUSH_TRANSPARENT	0x0001		// texture uses alpha-test for transparency
#define BRUSH_FULLBRIGHT	0x0002		// ignore lightmap for this surface (probably water)
#define BRUSH_HAS_DETAIL	0x0004		// has detail texture
#define BRUSH_HAS_BUMP		0x0008		// has normalmap
#define BRUSH_HAS_SPECULAR	0x0010		// has specular
#define BRUSH_TWOSIDE		0x0020		// some types of water
#define BRUSH_CONVEYOR		0x0040		// scrolled texture
#define BRUSH_GLOSSPOWER	0x0080		// gloss power stored in alphachannel of gloss map
#define BRUSH_REFLECT		0x0100		// reflect surface
#define BRUSH_UNDERWATER	0x0200		// ugly hack to tell GLSL about camera inside water volume
#define BRUSH_HAS_LUMA		0x0400		// self-illuminate parts
#define BRUSH_HAS_ALPHA		0x0800		// external albedo texture has alpha-channel
#define BRUSH_LIQUID		0x1000		// reflective & refractive water
#define BRUSH_MULTI_LAYERS	0x2000		// this face has multiply layers (random-tiling or landscape)
#define BRUSH_HAS_HEIGHTMAP	0x4000		// have heightmap

// each texture_t has a material
typedef struct material_s
{
	struct matdef_t	*effects;		// hit, impact, particle effects etc		
	struct texture_s *pSource;		// pointer to original texture
	std::shared_ptr<struct mbrushmat_s> impl; // client-side renderer internal material representation
	int	flags;						// brush material flags
} material_t;

typedef struct texture_s
{
	char		name[16];
	unsigned int	width, height;
	int		gl_texturenum;
	struct msurface_s	*texturechain;	// for gl_texsort drawing
	int		anim_total;	// total tenths in sequence ( 0 = no)
	int		anim_min, anim_max;	// time for this frame min <=time< max
	struct texture_s	*anim_next;	// in the animation sequence
	struct texture_s	*alternate_anims;	// bmodels in frame 1 use these
	unsigned short	fb_texturenum;	// auto-luma texturenum
	unsigned short	dt_texturenum;	// detail-texture binding
	material_t		*material;	// pointer to texture material
#if XASH_64BIT
	uint32_t		unused[1];
#else
	uint32_t		unused[2];
#endif
} texture_t;

typedef struct
{
	char		landname[16];	// name of decsription in mapname_land.txt
	unsigned short	texture_step;	// default is 16, pixels\luxels ratio
	unsigned short	max_extent;	// default is 16, subdivision step ((texture_step * max_extent) - texture_step)
	short		groupid;		// to determine equal landscapes from various groups, -1 - no group

	vec3_t		mins, maxs;

	// effects per-layers for playing sounds etc
	struct matdef_t	*effects[MAX_LANDSCAPE_LAYERS];	// hit, impact, particle effects etc

	byte		*heightmap;	// terrain heightmap raw data
	unsigned short	heightmap_width;
	unsigned short	heightmap_height;

#if CLIENT_DLL
	struct terrain_s	*terrain;		// valid only for client-side or local game
#else
	struct sv_terrain_s *terrain;
#endif
	intptr_t			reserved[13];	// just for future expansions or mod-makers
} mfaceinfo_t;

typedef struct
{
	mplane_t		*edges;
	int		numedges;
	vec3_t		origin;
	float		radius;		// for culling tests
	int		contents;		// sky or solid
} mfacebevel_t;

typedef struct
{
	float		vecs[2][4];	// [s/t] unit vectors in world space.
					// [i][3] is the s/t offset relative to the origin.
					// s or t = dot( 3Dpoint, vecs[i] ) + vecs[i][3]
	mfaceinfo_t	*faceinfo;	// pointer to landscape info and lightmap resolution (may be NULL)
	texture_t		*texture;
	int		flags;		// sky or slime, no lightmap or 256 subdivision
} mtexinfo_t;

typedef struct glpoly_s
{
	struct glpoly_s	*next;
	struct glpoly_s	*chain;
	int		numverts;
	int		flags;          		// for SURF_UNDERWATER
	float		verts[4][VERTEXSIZE];	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct mnode_s
{
// common with leaf
	int		contents;		// 0, to differentiate from leafs
	int		visframe;		// node needs to be traversed if current

	float		minmaxs[6];	// for bounding box culling
	struct mnode_s	*parent;

// node specific
	mplane_t		*plane;
	struct mnode_s	*children[2];	

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mnode_t;

typedef struct msurface_s	msurface_t;
typedef struct decal_s	decal_t;

// JAY: Compress this as much as possible
struct decal_s
{
	decal_t		*pnext;			// linked list for each surface
	msurface_t	*psurface;		// Surface id for persistence / unlinking
	float		dx;				// local texture coordinates
	float		dy;				// 
	float		scale;			// Pixel scale
	short		texture;		// Decal texture
	byte		flags;			// Decal flags  FDECAL_*
	short		entityIndex;	// Entity this is attached to
// Xash3D added
	vec3_t		position;		// location of the decal center in world space.
	glpoly_t	*polys;			// precomputed decal vertices
	intptr_t	reserved[4];	// just for future expansions or mod-makers
};

typedef struct mleaf_s
{
// common with node
	int		contents;
	int		visframe;		// node needs to be traversed if current

	float		minmaxs[6];	// for bounding box culling

	struct mnode_s	*parent;
// leaf specific
	byte		*compressed_vis;
	struct efrag_s	*efrags;

	msurface_t	**firstmarksurface;
	int		nummarksurfaces;
	int		cluster;		// helper to acess to uncompressed visdata
	byte		ambient_sound_level[NUM_AMBIENTS];

} mleaf_t;

// msurface_t flags
#define SURF_TWOSIDE		BIT( 0 )		// two-sided polygon (e.g. 'water4b')
#define SURF_PLANEBACK		BIT( 1 )		// plane should be negated
#define SURF_DRAWSKY		BIT( 2 )		// sky surface
#define SURF_DRAWTURB_QUADS	BIT( 3 )		// all subidivided polygons are quads 
#define SURF_DRAWTURB		BIT( 4 )		// warp surface
#define SURF_DRAWTILED		BIT( 5 )		// face without lighmap
#define SURF_CONVEYOR		BIT( 6 )		// scrolled texture (was SURF_DRAWBACKGROUND)
#define SURF_UNDERWATER		BIT( 7 )		// caustics
#define SURF_TRANSPARENT	BIT( 8 )		// it's a transparent texture (was SURF_DONTWARP)

// user-defined flags
#define SURF_HAS_DECALS		BIT( 9 )		// this surface has decals
#define SURF_LANDSCAPE		BIT( 10 )		// this is multi-layered texture surface
#define SURF_PORTAL			BIT( 11 )		// portal surface
#define SURF_SCREEN			BIT( 12 )		// screen surface
#define SURF_MOVIE			BIT( 13 )		// screen movie
#define SURF_GRASS_UPDATE	BIT( 14 )		// gamma has changed, need to update grass
#define SURF_NODRAW			BIT( 15 )		// failed to create shader for this surface
#define SURF_OCCLUDED		BIT( 16 )		// occlusion query result
#define SURF_QUEUED			BIT( 17 )		// add to queue for occlusion
#define SURF_NODLIGHT		BIT( 18 )		// failed to create dlight shader for this surface
#define SURF_NOSUNLIGHT		BIT( 19 )		// failed to create sun light shader for this surface

#define SURF_FULLBRIGHT		BIT( 25 )		// completely ignore lighting on this brush
#define SURF_OF_SUBMODEL	BIT( 26 )		// this face is owned by submodel (to differentiate from world faces)
#define SURF_LOCAL_SPACE	BIT( 27 )		// if bmodel has origin this surf is in local space (used for sorting)
#define SURF_LM_UPDATE		BIT( 28 )		// lightmap was changed, need to reload
#define SURF_DM_UPDATE		BIT( 29 )		// deluxmap was changed, need to reload
#define SURF_REFLECT_PUDDLE	BIT( 30 )		// special reflected surface (puddle reflection)
#define SURF_REFLECT		BIT( 31 )		// reflect surface (mirror)

// surface extradata
typedef struct mextrasurf_s
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// surface origin
	struct msurface_s	*surf;		// upcast to surface

	// extended light info
	int		dlight_s, dlight_t;	// gl lightmap coordinates for dynamic lightmaps

	short		lightmapmins[2];
	short		lightextents[2];
	float		lmvecs[2][4];

	color24		*normals;		// note: this is the actual deluxemap data for this surface
	byte		*shadows;		// note: occlusion map for this surface
// begin userdata
	struct msurface_s	*lightmapchain;	// lightmapped polys
	struct mextrasurf_s	*detailchain;	// for detail textures drawing
	mfacebevel_t	*bevel;		// for exact face traceline
	struct mextrasurf_s	*lumachain;	// draw fullbrights
	struct cl_entity_s	*parent;		// upcast to owner entity
// was mirrortexturenum, mirrormatrix (17 * sizeof( int ))
	unsigned short	light_s[MAXLIGHTMAPS];
	unsigned short	light_t[MAXLIGHTMAPS];
	byte		lights[MAXDYNLIGHTS];// static lights that affected this face (255 = no lights)
	float		texofs[2];	// conveyor offsets
	struct mcubemap_s *cubemap[2];	// two cubemaps nearest to surface
	float		lerpFactor;		// lerp factor between two nearest cubemaps
	short		subtexture[8];	// MAX_REF_STACK
	word		cintexturenum;	// cinematic handle
	word		lightmaptexturenum;	// custom lightmap number
	word		lastRenderMode;	// for catch change render modes
	word		checkcount;	// used for cin textures
// again matched with engine declaration
	struct grasshdr_s	*grass;		// grass that linked by this surface
	unsigned short	grasscount;	// number of bushes per polygon (used to determine total VBO size)
	unsigned short	numverts;		// world->vertexes[]
	int		firstvertex;	// fisrt look up in tr.tbn_vectors[], then acess to world->vertexes[]
// reserved fields (32 * sizeof( int ))
	unsigned int	query;		// test surface for occluding (active if query != 0)
// shader cache
	shader_t		forwardScene[2];	// two for mirrors
	shader_t		forwardLightSpot[2]; // first for w/ shadows, second for w/o shadows
	shader_t		forwardLightOmni[2]; 
	shader_t		forwardLightProj;
	shader_t		deferredScene;
	shader_t		deferredLight;
	shader_t		forwardDepth;

	struct brushdecal_s	*pdecals;		// linked decals
#if XASH_64BIT == 1
	intptr_t		reserved[24];	// just for future expansions or mod-makers
#else
	intptr_t		reserved[20];	// just for future expansions or mod-makers
#endif
} mextrasurf_t;

typedef struct msurface_s
{
	int		visframe;		// should be drawn when node is crossed

	mplane_t		*plane;		// pointer to shared plane
	int		flags;		// see SURF_ #defines

	int		firstedge;	// look up in model->surfedges[], negative numbers
	int		numedges;		// are backwards edges

	short		texturemins[2];
	short		extents[2];

	int		light_s, light_t;	// gl lightmap coordinates

	glpoly_t		*polys;		// multiple if warped
	struct msurface_s	*texturechain;

	mtexinfo_t	*texinfo;

	// lighting info
	int		dlightframe;	// last frame the surface was checked by an animated light
	int		dlightbits;	// dynamically generated. Indicates if the surface illumination
					// is modified by an animated light.

	int		lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];
	int		cached_light[MAXLIGHTMAPS];	// values currently used in lightmap
	mextrasurf_t	*info;		// pointer to surface extradata (was cached_dlight)

	color24		*samples;		// note: this is the actual lightmap data for this surface
	decal_t		*pdecals;
} msurface_t;

typedef struct hull_s
{
	dclipnode_t	*clipnodes;
	mplane_t		*planes;
	int		firstclipnode;
	int		lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

#ifndef CACHE_USER
#define CACHE_USER
typedef struct cache_user_s
{
	void		*data;		// extradata
} cache_user_t;
#endif

typedef struct model_s
{
	char		name[64];		// model name
	qboolean	needload;		// bmodels and sprites don't cache normally

	// shared modelinfo
	modtype_t	type;		// model type
	int			numframes;	// sprite's framecount
	poolhandle_t mempool;	// private mempool (was synctype)
	int			flags;		// hl compatibility

//
// volume occupied by the model
//
	vec3_t		mins, maxs;	// bounding box at angles '0 0 0'
	float		radius;
    
	// brush model
	union
	{
	int			firstmodelsurface;
	uint32_t	modelCRC;
	};
	int		nummodelsurfaces;

	int		numsubmodels;
	dmodel_t		*submodels;	// or studio animations

	int		numplanes;
	mplane_t		*planes;		// bsp or studio collision planes

	int		numleafs;		// number of visible leafs, not counting 0
	mleaf_t		*leafs;

	int		numvertexes;
	mvertex_t		*vertexes;

	int		numedges;
	medge_t		*edges;

	int		numnodes;
	mnode_t		*nodes;

	int		numtexinfo;
	mtexinfo_t	*texinfo;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numsurfedges;
	int		*surfedges;

	int		numclipnodes;
	dclipnode_t	*clipnodes;

	int		nummarksurfaces;
	msurface_t	**marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int		numtextures;
	texture_t		**textures;	// BSP textures or remap textures

	union
	{
	byte		*visdata;
	struct mposetobone_t *poseToBone;
	};

	union
	{
	color24		*lightdata;
	struct mstudiocache_t *studiocache;	// pointer to VBO-prepared model (only for mod_studio)
	};

	union
	{
	char		*entities;
	struct mstudiomat_s	*materials;	// studio materials (only for mod_studio, valid only on clientside)
	};
//
// additional model data
//
	cache_user_t	cache;		// only access through Mod_Extradata
} model_t;

typedef struct alight_s
{
	int		ambientlight;	// clip at 128
	int		shadelight;	// clip at 192 - ambientlight
	vec3_t		color;
	float		*plightvec;
} alight_t;

typedef struct auxvert_s
{
	float		fv[3];		// viewspace x, y
} auxvert_t;

#define MAX_SCOREBOARDNAME	32
#define MAX_INFO_STRING	256

#include "custom.h"

typedef struct player_info_s
{
	int		userid;			// User id on server
	char		userinfo[MAX_INFO_STRING];	// User info string
	char		name[MAX_SCOREBOARDNAME];	// Name (extracted from userinfo)
	int		spectator;		// Spectator or not, unused

	int		ping;
	int		packet_loss;

	// skin information
	char		model[64];
	int		topcolor;
	int		bottomcolor;

	// last frame rendered
	int		renderframe;	

	// Gait frame estimation
	int		gaitsequence;
	float		gaitframe;
	float		gaityaw;
	vec3_t		prevgaitorigin;

	customization_t	customdata;
} player_info_t;

//
// sprite representation in memory
//
typedef enum { SPR_SINGLE = 0, SPR_GROUP, SPR_ANGLED } spriteframetype_t;

typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float		up, down, left, right;
	int		gl_texturenum;
} mspriteframe_t;

typedef struct
{ 
	int		numframes;
	float		*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	spriteframetype_t	type;
	mspriteframe_t	*frameptr;
} mspriteframedesc_t;

typedef struct
{
	short		type;
	short		texFormat;
	int		maxwidth;
	int		maxheight;
	int		numframes;
	int		radius;
	int		facecull;
	int		synctype;
	mspriteframedesc_t	frames[1];
} msprite_t;

// check size of customizable structures for compliance with engine structs
#if XASH_X86 == 1
static_assert(sizeof(mextrasurf_t) == 324, "mextrasurf_t structure size should be same as on engine");
static_assert(sizeof(decal_t) == 60, "decal_t structure size should be same as on engine");
static_assert(sizeof(mfaceinfo_t) == 176, "mfaceinfo_t structure size should be same as on engine");
#elif XASH_AMD64 == 1
static_assert(sizeof(mextrasurf_t) == 496, "mextrasurf_t structure size should be same as on engine");
static_assert(sizeof(decal_t) == 88, "decal_t structure size should be same as on engine");
static_assert(sizeof(mfaceinfo_t) == 304, "mfaceinfo_t structure size should be same as on engine");
#endif

#endif // COM_MODEL_H