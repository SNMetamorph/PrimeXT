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

#include "bspfile.h"	// we need some declarations from it

/*
==============================================================================

	ENGINE MODEL FORMAT
==============================================================================
*/
#define STUDIO_RENDER	1
#define STUDIO_EVENTS	2

#define ZISCALE		((float)0x8000)

#define MIPLEVELS		4
#define VERTEXSIZE		7
#define MAXLIGHTMAPS	4
#define NUM_AMBIENTS	4		// automatic ambient sounds

// model flags (stored in model_t->flags)
#define MODEL_CONVEYOR		BIT( 0 )
#define MODEL_HAS_ORIGIN		BIT( 1 )
#define MODEL_LIQUID		BIT( 2 )	// model has only point hull
#define MODEL_TRANSPARENT		BIT( 3 )	// have transparent surfaces
#define MODEL_COLORED_LIGHTING	BIT( 4 )	// lightmaps stored as RGB

#define MODEL_WORLD			BIT( 29 )	// it's a worldmodel
#define MODEL_CLIENT		BIT( 30 )	// client sprite

// model types
typedef enum
{
	mod_bad = -1,
	mod_brush, 
	mod_sprite, 
	mod_alias, 
	mod_studio
} modtype_t;

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
	struct mextrasurf_s	*lightchain;	// used for drawing spotlights
	unsigned int	unused[2];	// reserved 
} texture_t;

typedef struct
{
	char		landname[16];	// name of decsription in mapname_land.txt
	unsigned short	texture_step;	// default is 16, pixels\luxels ratio
	unsigned short	max_extent;	// default is 16, subdivision step ((texture_step * max_extent) - texture_step)
	short		groupid;		// to determine equal landscapes from various groups, -1 - no group

	vec3_t		mins, maxs;	// terrain bounds (fill by user)
	struct terrain_s	*terrain;		// valid only for client-side or local game

	int		reserved[31];	// just for future expansions or mod-makers
} mfaceinfo_t;

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
	decal_t		*pnext;		// linked list for each surface
	msurface_t	*psurface;	// Surface id for persistence / unlinking
	float		dx;		// local texture coordinates
	float		dy;		// 
	float		scale;		// Pixel scale
	short		texture;		// Decal texture
	short		flags;		// Decal flags  FDECAL_*
	short		entityIndex;	// Entity this is attached to
// Xash3D specific
	vec3_t		position;		// location of the decal center in world space.
	glpoly_t		*polys;		// precomputed decal vertices
	int		glsl_sequence;	// shader cache
	word		shaderNum;
	word		unused;
	int		reserved[2];	// just for future expansions or mod-makers
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

// surface extradata
typedef struct mextrasurf_s
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// surface origin
	struct msurface_s	*surf;		// upcast to surface

	// extended light info
	int		dlight_s, dlight_t;	// gl lightmap coordinates for dynamic lightmaps

	short		lightmapmins[2];	// lightmatrix
	short		lightextents[2];
	float		lmvecs[2][4];

	color24		*deluxemap;	// note: this is the actual deluxemap data for this surface
	byte		*shadowmap;	// note: occlusion map for this surface
// begin userdata
	int		cintexturenum;	// cinematic handle
	struct mextrasurf_s	*detailchain;	// for detail textures drawing
	struct mextrasurf_s	*mirrorchain;	// for gl_texsort drawing
	struct mextrasurf_s	*lumachain;	// draw fullbrights
	struct cl_entity_s	*parent;		// upcast to owner entity
// after this point engine touches nothing
	int		unused;

	struct grasshdr_s	*grass;		// grass that linked by this surface
	unsigned short	grasscount;	// number of bushes per polygon (used to determine total VBO size)
	unsigned short	numverts;		// world->vertexes[]
	int		firstvertex;	// fisrt look up in tr.tbn_vectors[], then acess to world->vertexes[]

	struct mextrasurf_s	*lightchain;	// for spotlight drawing
	struct mextrasurf_s	*chromechain;	// for chrome-effect drawing
	int		checkcount;	// used for cin textures
	int		tracecount;	// used for trace checking

	unsigned short	light_s[MAXLIGHTMAPS];
	unsigned short	light_t[MAXLIGHTMAPS];
	word		lightmaptexturenum;	// custom lightmap number

	word		shaderNum[2];		// constantly assigned shader to this surface
	word		omniLightShaderNum;		// cached omni light shader for this face
	word		projLightShaderNum[2];	// cached proj light shader for this face
	word		glsl_sequence[2];		// if( tr.glsl_valid_sequence != es->glsl_sequence ) it's time to recompile shader
	word		glsl_sequence_omni;		// same as above but for omnilights
	word		glsl_sequence_proj[2];	// same as above but for projlights
	float		texofs[2];		// conveyor offsets
	int		cached_frame;

	unsigned short	*indexes;		// for subdivided polys
	unsigned short	numindexes;

	short		subtexture[8];	// MAX_REF_STACK
	word		gl_texturenum;	// current diffuse texture
	word		dt_texturenum;	// current detail texture
	int		reserved[25];	// just for future expansions or mod-makers
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
	qboolean		needload;		// bmodels and sprites don't cache normally

	// shared modelinfo
	modtype_t		type;		// model type
	int		numframes;	// sprite's framecount
	byte		*mempool;		// private mempool (was synctype)
	int		flags;		// hl compatibility

//
// volume occupied by the model
//
	vec3_t		mins, maxs;	// bounding box at angles '0 0 0'
	float		radius;
    
	// brush model
	union
	{
	int		firstmodelsurface;
	int		modelCRC;
	};
	int		nummodelsurfaces;

	int		numsubmodels;
	dmodel_t		*submodels;	// or studio animations

	int		numplanes;
	mplane_t		*planes;

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
	texture_t		**textures;

	union
	{
	byte		*visdata;
	struct mposebone_s	*poseToBone;
	};

	union
	{
	color24		*lightdata;
	struct mbodypart_s	*bodyparts;	// pointer to VBO-prepared model (only for mod_studio)
	};

	union
	{
	char		*entities;
	struct mstudiomat_s	*materials;	// studio materials (only for mod_studio)
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

// extended lightinfo
typedef struct
{
	vec3_t		ambient;
	vec3_t		diffuse;
	vec3_t		direction;
	vec3_t		origin;			// used for debug
} lightinfo_t;

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
	unsigned int	renderframe;	

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

#endif//COM_MODEL_H
