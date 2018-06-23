/*
bspfile.h - BSP format included q1, hl1 support
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

#ifndef BSPFILE_H
#define BSPFILE_H

/*
==============================================================================

BRUSH MODELS

.bsp contain level static geometry with including PVS and lightning info
==============================================================================
*/

// header
#define Q1BSP_VERSION	29	// quake1 regular version (beta is 28)
#define HLBSP_VERSION	30	// half-life regular version

#define IDEXTRAHEADER	(('H'<<24)+('S'<<16)+('A'<<8)+'X') // little-endian "XASH"
#define EXTRA_VERSION	4	// ver. 1 was occupied by old versions of XashXT, ver. 2 was occupied by old vesrions of P2:savior
				// ver. 3 was occupied by experimental versions of P2:savior change fmt
// worldcraft predefined angles
#define ANGLE_UP			-1
#define ANGLE_DOWN			-2

// bmodel limits
#define MAX_MAP_HULLS		4		// MAX_HULLS

#define SURF_NOCULL			BIT( 0 )		// two-sided polygon (e.g. 'water4b')
#define SURF_PLANEBACK		BIT( 1 )		// plane should be negated
#define SURF_DRAWSKY		BIT( 2 )		// sky surface
#define SURF_DRAWTURB_QUADS		BIT( 3 )		// all subidivided polygons are quads 
#define SURF_DRAWTURB		BIT( 4 )		// warp surface
#define SURF_DRAWTILED		BIT( 5 )		// face without lighmap
#define SURF_CONVEYOR		BIT( 6 )		// scrolled texture (was SURF_DRAWBACKGROUND)
#define SURF_UNDERWATER		BIT( 7 )		// caustics
#define SURF_TRANSPARENT		BIT( 8 )		// it's a transparent texture (was SURF_DONTWARP)

// user-defined flags
#define SURF_PORTAL			BIT( 9 )		// portal surface
#define SURF_SCREEN			BIT( 10 )		// screen surface
#define SURF_MOVIE			BIT( 11 )		// screen movie
#define SURF_CHROME			BIT( 12 )		// has a chrome effect
#define SURF_LANDSCAPE		BIT( 13 )		// this is multi-layered texture surface
#define SURF_NODRAW			BIT( 14 )		// failed to create shader for this surface
#define SURF_NODLIGHT		BIT( 15 )		// failed to create dlight shader for this surface

#define SURF_FULLBRIGHT		BIT( 25 )		// completely ignore lighting on this brush
#define SURF_OF_SUBMODEL		BIT( 26 )		// this face is owned by submodel (to differentiate from world faces)
#define SURF_LOCAL_SPACE		BIT( 27 )		// if bmodel has origin this surf is in local space (used for sorting)
#define SURF_LM_UPDATE		BIT( 28 )		// lightmap was changed, need to reload
#define SURF_DM_UPDATE		BIT( 29 )		// deluxmap was changed, need to reload

#define SURF_REFLECT		BIT( 31 )		// reflect surface (mirror)

// lightstyle management
#define LM_STYLES			4		// MAXLIGHTMAPS
#define LS_NORMAL			0x00
#define LS_UNUSED			0xFE
#define LS_NONE			0xFF

#define MAX_MAP_MODELS		512		// can be increased up to 2048 if needed
#define MAX_MAP_BRUSHES		32768		// unsigned short limit
#define MAX_MAP_ENTITIES		8192		// can be increased up to 32768 if needed
#define MAX_MAP_ENTSTRING		0x80000		// 512 kB should be enough
#define MAX_MAP_PLANES		65536		// can be increased without problems
#define MAX_MAP_NODES		32767		// because negative shorts are leafs
#define MAX_MAP_CLIPNODES		32767		// because negative shorts are contents
#define MAX_MAP_LEAFS		32767		// signed short limit
#define MAX_MAP_VERTS		65535		// unsigned short limit
#define MAX_MAP_ELEMS		MAX_MAP_VERTS * 5	// should be enough
#define MAX_MAP_FACES		65535		// unsigned short limit
#define MAX_MAP_MARKSURFACES		65535		// unsigned short limit
#define MAX_MAP_TEXINFO		MAX_MAP_FACES	// in theory each face may have personal texinfo
#define MAX_MAP_EDGES		0x100000		// can be increased but not needed
#define MAX_MAP_SURFEDGES		0x200000		// can be increased but not needed
#define MAX_MAP_TEXTURES		2048		// can be increased but not needed
#define MAX_MAP_MIPTEX		0x2000000		// 32 Mb internal textures data
#define MAX_MAP_LIGHTING		0x2000000		// 32 Mb lightmap raw data (can contain deluxemaps)
#define MAX_MAP_VISIBILITY		0x800000		// 8 Mb visdata

// quake lump ordering
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_TEXTURES		2		// internal textures
#define LUMP_VERTEXES		3
#define LUMP_VISIBILITY		4
#define LUMP_NODES			5
#define LUMP_TEXINFO		6
#define LUMP_FACES			7
#define LUMP_LIGHTING		8
#define LUMP_CLIPNODES		9
#define LUMP_LEAFS			10
#define LUMP_MARKSURFACES		11
#define LUMP_EDGES			12
#define LUMP_SURFEDGES		13
#define LUMP_MODELS			14		// internal submodels
#define HEADER_LUMPS		15

// extra version 4
#define LUMP_LIGHTVECS		0	// deluxemap data
#define LUMP_FACEINFO		1	// landscape and lightmap resolution info
#define LUMP_CUBEMAPS		2	// cubemap description
#define LUMP_VERTNORMALS		3	// phong shaded vertex normals
#define LUMP_LEAF_LIGHTING		4	// store vertex lighting for statics
#define LUMP_WORLDLIGHTS		5	// list of all the virtual and real lights (used to relight models in-game)
#define LUMP_COLLISION		6	// physics engine collision hull dump (userdata)
#define LUMP_AINODEGRAPH		7	// node graph that stored into the bsp (userdata)
#define LUMP_SHADOWMAP		8	// contains shadow map for direct light
#define LUMP_VERTEX_LIGHT		9	// store vertex lighting for statics
#define LUMP_UNUSED0		10	// one lump reserved for me
#define LUMP_UNUSED1		11	// one lump reserved for me
#define EXTRA_LUMPS			12	// count of the extra lumps

// texture flags
#define TEX_SPECIAL			BIT( 0 )	// sky or slime, no lightmap or 256 subdivision
#define TEX_WORLD_LUXELS		BIT( 1 )	// alternative lightmap matrix will be used (luxels per world units instead of luxels per texels)
#define TEX_AXIAL_LUXELS		BIT( 2 )	// force world luxels to axial positive scales
#define TEX_EXTRA_LIGHTMAP		BIT( 3 )	// bsp31 legacy - using 8 texels per luxel instead of 16 texels per luxel

// ambient sound types
enum
{
	AMBIENT_WATER = 0,		// waterfall
	AMBIENT_SKY,		// wind
	AMBIENT_SLIME,		// never used in quake
	AMBIENT_LAVA,		// never used in quake
	NUM_AMBIENTS,		// automatic ambient sounds
};

//
// BSP File Structures
//

typedef struct
{
	int	fileofs;
	int	filelen;
} dlump_t;

typedef struct
{
	int	version;
	dlump_t	lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	int	id;	// must be little endian XASH
	int	version;
	dlump_t	lumps[EXTRA_LUMPS];	
} dextrahdr_t;

typedef struct
{
	vec3_t	mins;
	vec3_t	maxs;
	vec3_t	origin;			// for sounds or lights
	int	headnode[MAX_MAP_HULLS];
	int	visleafs;			// not including the solid leaf 0
	int	firstface;
	int	numfaces;
} dmodel_t;

typedef struct
{
	int	nummiptex;
	int	dataofs[4];		// [nummiptex]
} dmiptexlump_t;

typedef struct
{
	vec3_t	point;
} dvertex_t;

typedef struct
{
	vec3_t	normal;
	float	dist;
	int	type;			// PLANE_X - PLANE_ANYZ ?
} dplane_t;

typedef struct
{
	int	planenum;
	short	children[2];		// negative numbers are -(leafs + 1), not nodes
	short	mins[3];			// for sphere culling
	short	maxs[3];
	word	firstface;
	word	numfaces;			// counting both sides
} dnode_t;

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct
{
	int	contents;
	int	visofs;			// -1 = no visibility info

	short	mins[3];			// for frustum culling
	short	maxs[3];
	word	firstmarksurface;
	word	nummarksurfaces;

	// automatic ambient sounds
	byte	ambient_level[NUM_AMBIENTS];	// ambient sound level (0 - 255)
} dleaf_t;

typedef struct
{
	int	planenum;
	short	children[2];		// negative numbers are contents
} dclipnode_t;

typedef struct
{
	float	vecs[2][4];		// texmatrix [s/t][xyz offset]
	int	miptex;
	short	flags;
	short	faceinfo;			// -1 no face info otherwise dfaceinfo_t
} dtexinfo_t;

typedef struct
{
	char		landname[16];	// name of decsription in mapname_land.txt
	unsigned short	texture_step;	// default is 16, pixels\luxels ratio
	unsigned short	max_extent;	// default is 16, subdivision step ((texture_step * max_extent) - texture_step)
	short		groupid;		// to determine equal landscapes from various groups, -1 - no group
} dfaceinfo_t;

typedef word	dmarkface_t;		// leaf marksurfaces indexes
typedef int	dsurfedge_t;		// map surfedges
typedef word	dvertnorm_t;		// map vert normals

// NOTE: that edge 0 is never used, because negative edge nums
// are used for counterclockwise use of the edge in a face
typedef struct
{
	word	v[2];			// vertex numbers
} dedge_t;

typedef struct
{
	word	planenum;
	short	side;

	int	firstedge;		// we must support > 64k edges
	short	numedges;
	short	texinfo;

	// lighting info
	byte	styles[LM_STYLES];
	int	lightofs;			// start of [numstyles*surfsize] samples
} dface_t;

#define VLIGHTIDENT		(('T'<<24)+('I'<<16)+('L'<<8)+'V') // little-endian "XASH"
#define VLIGHT_VERSION	1

#define VERTEXNORMAL_CONE_INNER_ANGLE	DEG2RAD( 7.275 )

typedef struct
{
	byte		color[6][3];		// 6 sides 1x1 (single pixel per side)
} dlightcube_t;

typedef struct
{
	dlightcube_t	ambient;
	short		origin[3];
	short		leafnum;			// leaf that contain this sample
} dleafsample_t;

typedef struct
{
	float		normal[3];
} dnormal_t;

typedef struct
{
	byte		light[LM_STYLES][3];	// lightvalue
	byte		deluxe[LM_STYLES][3];	// deluxe vectors
	byte		shadow[LM_STYLES];		// shadowmap
} dvertlight_t;

typedef struct
{
	int		submodel_offset;		// hack to determine submodel
	int		vertex_offset;
} dvlightofs_t;

typedef struct
{
	int		modelCRC;			// catch for model changes
	int		numverts;
	byte		styles[LM_STYLES];
	dvlightofs_t	submodels[32];		// MAXSTUDIOMODELS
	dvertlight_t	verts[3];			// variable sized
} dmodellight_t;

typedef struct
{
	int		ident;			// to differentiate from previous lump LUMP_LEAF_LIGHTING
	int		version;			// data package version
	int		nummodels;
	int		dataofs[4];		// [nummodels]
} dvlightlump_t;

#define NORMIDENT		(('T'<<24)+('B'<<16)+('T'<<8)+'Q') // little-endian "QTBN"

typedef struct
{
	int		ident;			// to differentiate from non-indexed normals storage
	int		numnormals;		// dvertnorm[numsurfedges] dnormals[numnormals]
} dnormallump_t;

#endif//BSPFILE_H
