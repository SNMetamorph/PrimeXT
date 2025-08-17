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

#include <stdint.h>
#ifndef PXBSP_COMPILING
#include "color24.h"
#include "const.h"
#endif

/*
==============================================================================

BRUSH MODELS

.bsp contain level static geometry with including PVS and lightning info
==============================================================================
*/
// header
#define Q1BSP_VERSION		29	// quake1 regular version (beta is 28)
#define HLBSP_VERSION		30	// half-life regular version
#define P2BSP_VERSION		32	// P2:Savior 32-bit limits BSP version

#define IDEXTRAHEADER		(('H'<<24)+('S'<<16)+('A'<<8)+'X') // little-endian "XASH"
#define EXTRA_VERSION		4	// ver. 1 was occupied by old versions of XashXT, ver. 2 was occupied by old vesrions of P2:savior
					// ver. 3 was occupied by experimental versions of P2:savior change fmt
#define EXTRA_VERSION_OLD		2	// extra version 2 (P2:Savior regular version) to get minimal backward compatibility

// worldcraft predefined angles
#define ANGLE_UP			-1
#define ANGLE_DOWN			-2

// bmodel limits
#define MAX_MAP_HULLS		4		// MAX_HULLS
#define MIPLEVELS			4		// software renderer mipmap count

#define WORLD_MINS			-32768
#define WORLD_MAXS			32768
#define WORLD_SIZE			( WORLD_MAXS - WORLD_MINS )
#define BOGUS_RANGE			WORLD_SIZE * 1.74	// half-diagonal

// these are for entity key:value pairs
#define MAX_KEY			64
#define MAX_VALUE			4096

// lightstyle management
#define MAXLIGHTMAPS		4		// max lightstyles per each face
#define MAXSTYLES 			64		// very fundamental thing
#define LS_NORMAL			0x00
#define LS_SKY			0x14		// light_environment style
#define LS_UNUSED			0xFE
#define LS_NONE			0xFF

#define VERTEXNORMAL_CONE_INNER_ANGLE	DEG2RAD( 7.275 )

#define MAX_MAP_MODELS			8192		// default limit is 1024
#define MAX_MAP_ENTITIES		32768		// can be increased up to 32768 if needed
#define MAX_MAP_ENTSTRING		0x200000	// 2 mB should be enough
#define MAX_MAP_PLANES			65536		// can be increased without problems
#define MAX_MAP_NODES			32767		// because negative shorts are leafnums
#define MAX_MAP_CLIPNODES		32767		// because negative shorts are contents
#define MAX_MAP_CLIPNODES32		1048576		// can be increased but not needed
#define MAX_MAP_LEAFS			32768		// signed short limit (GoldSrc have internal limit at 8192)
#define MAX_MAP_VERTS			65535		// unsigned short limit
#define MAX_MAP_FACES			65535		// unsigned short limit
#define MAX_MAP_MARKSURFACES	65535		// unsigned short limit
#define MAX_MAP_TEXINFO			32768		// because signed short
#define MAX_MAP_FACEINFO		8192		// can be increased but not needs
#define MAX_MAP_EDGES			0x100000	// can be increased but not needed
#define MAX_MAP_SURFEDGES		0x200000	// can be increased but not needed
#define MAX_MAP_TEXTURES		8192		// can be increased but not needed
#define MAX_MAP_MIPTEX			0x2000000	// 32 Mb internal textures data
#define MAX_MAP_LIGHTING		0x7a12000	// 120 Mb lightmap raw data (can contain deluxemaps)
#define MAX_MAP_VISIBILITY		0x2000000	// 32 Mb visdata
#define MAX_MAP_VISLIGHTDATA	0x4000000	// 64 Mb for lights visibility
#define MAX_MAP_CUBEMAPS		1024
#define MAX_MAP_LEAFLIGHTS		0x40000		// can be increased
#define MAX_MAP_WORLDLIGHTS		65535		// including a light surfaces too

// quake lump ordering
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_TEXTURES		2	// internal textures
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
#define LUMP_MODELS			14	// internal submodels
#define HEADER_LUMPS		15

// extra version 4
#define LUMP_LIGHTVECS			0	// deluxemap data
#define LUMP_FACEINFO			1	// landscape and lightmap resolution info
#define LUMP_CUBEMAPS			2	// cubemap description
#define LUMP_VERTNORMALS		3	// phong shaded vertex normals
#define LUMP_LEAF_LIGHTING		4	// store vertex lighting for statics
#define LUMP_WORLDLIGHTS		5	// list of all the virtual and real lights (used to relight models in-game)
#define LUMP_EXTDATA_PRIMEXT	6	// extended information for forthcoming PrimeXT features (formerly LUMP_COLLISION, was unused)
#define LUMP_AINODEGRAPH		7	// node graph that stored into the bsp (unused)
#define LUMP_SHADOWMAP			8	// contains shadow map for direct light (unused in P2/PrimeXT)
#define LUMP_VERTEX_LIGHT		9	// store vertex lighting for statics
#define LUMP_VISLIGHTDATA		10	// how many lights affects to faces
#define LUMP_SURFACE_LIGHT		11	// models lightmapping
#define EXTRA_LUMPS				12	// count of the extra lumps

// texture flags
#define TEX_SPECIAL			BIT( 0 )	// sky or slime, no lightmap or 256 subdivision
#define TEX_WORLD_LUXELS		BIT( 1 )	// alternative lightmap matrix will be used (luxels per world units instead of luxels per texels)
#define TEX_AXIAL_LUXELS		BIT( 2 )	// force world luxels to axial positive scales
#define TEX_EXTRA_LIGHTMAP		BIT( 3 )	// bsp31 legacy - using 8 texels per luxel instead of 16 texels per luxel
#define TEX_NOSHADOW		BIT( 4 )
#define TEX_NODIRT			BIT( 5 )
#define TEX_SCROLL			BIT( 6 )	// Doom special FX

// ambient sound types
enum
{
	AMBIENT_WATER = 0,		// waterfall
	AMBIENT_SKY,		// wind
	AMBIENT_SLIME,		// never used in quake
	AMBIENT_LAVA,		// never used in quake
	NUM_AMBIENTS,		// automatic ambient sounds
};

// leaf contents
#define CONTENTS_NONE		0
#define CONTENTS_EMPTY		-1
#define CONTENTS_SOLID		-2
#define CONTENTS_WATER		-3
#define CONTENTS_SLIME		-4
#define CONTENTS_LAVA		-5
#define CONTENTS_SKY		-6
// reserved
// reserved
#define CONTENTS_CURRENT_0		-9
#define CONTENTS_CURRENT_90		-10
#define CONTENTS_CURRENT_180		-11
#define CONTENTS_CURRENT_270		-12
#define CONTENTS_CURRENT_UP		-13
#define CONTENTS_CURRENT_DOWN		-14
#define CONTENTS_TRANSLUCENT		-15
// user contents that never present into bsp
#define CONTENTS_LADDER		-16
#define CONTENTS_FLYFIELD		-17
#define CONTENTS_GRAVITY_FLYFIELD	-18
#define CONTENTS_FOG		-19
#define CONTENTS_SPECIAL1		-20
#define CONTENTS_SPECIAL2		-21
#define CONTENTS_SPECIAL3		-22
#define CONTENTS_SPOTLIGHT		-23	// in of cone of spotlight

//
// BSP File Structures
//

typedef struct
{
	int32_t	fileofs;
	int32_t	filelen;
} dlump_t;

typedef struct
{
	int32_t	version;
	dlump_t	lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	int32_t	id;			// must be little endian XASH
	int32_t	version;
	dlump_t	lumps[EXTRA_LUMPS];	
} dextrahdr_t;

typedef struct
{
	float	mins[3];
	float	maxs[3];
	float	origin[3];		// for sounds or lights
	int32_t	headnode[MAX_MAP_HULLS];
	int32_t	visleafs;			// not including the solid leaf 0
	int32_t	firstface;
	int32_t	numfaces;
} dmodel_t;

typedef struct
{
	int32_t	nummiptex;
	int32_t	dataofs[4];		// [nummiptex]
} dmiptexlump_t;

typedef struct miptex_s
{
	char		name[16];
	uint32_t	width;
	uint32_t	height;
	uint32_t	offsets[MIPLEVELS];		// four mip maps stored
} miptex_t;

typedef struct
{
	float	point[3];
} dvertex_t;

typedef struct
{
	float	normal[3];
	float	dist;
	int32_t	type;
} dplane_t;

typedef struct
{
	int32_t	planenum;			// allow planes >= 65535
	int16_t	children[2];		// negative numbers are -(leafs + 1), not nodes
	int16_t	mins[3];			// for sphere culling
	int16_t	maxs[3];
	uint16_t firstface;
	uint16_t numfaces;			// counting both sides
} dnode_t;

typedef struct
{
	int32_t	planenum;
	int32_t	children[2];		// negative numbers are -(leafs+1), not nodes
	float	mins[3];			// for sphere culling
	float	maxs[3];
	int32_t	firstface;
	int32_t	numfaces;			// counting both sides
} dnode32_t;

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct
{
	int32_t	contents;
	int32_t	visofs;			// -1 = no visibility info

	int16_t		mins[3];			// for frustum culling
	int16_t		maxs[3];
	uint16_t	firstmarksurface;
	uint16_t	nummarksurfaces;

	// automatic ambient sounds
	uint8_t	ambient_level[NUM_AMBIENTS];	// ambient sound level (0 - 255)
} dleaf_t;

typedef struct
{
	int32_t	contents;
	int32_t	visofs;			// -1 = no visibility info

	float	mins[3];			// for frustum culling
	float	maxs[3];

	int32_t	firstmarksurface;
	int32_t	nummarksurfaces;

	uint8_t	ambient_level[NUM_AMBIENTS];
} dleaf32_t;

typedef struct
{
	int32_t	planenum;			// allow planes >= 65535
	int16_t	children[2];		// negative numbers are contents
} dclipnode_t;

typedef struct
{
	int32_t	planenum;
	int32_t	children[2];		// negative numbers are contents
} dclipnode32_t;

typedef struct
{
	float	vecs[2][4];		// texmatrix [s/t][xyz offset]
	int32_t	miptex;
	int16_t	flags;
	int16_t	faceinfo;			// -1 no face info otherwise dfaceinfo_t
} dtexinfo_t;

typedef uint16_t	dmarkface_t;		// leaf marksurfaces indexes
typedef int32_t		dmarkface32_t;		// leaf marksurfaces indexes
typedef int32_t		dsurfedge_t;		// map surfedges
typedef int16_t		dvertnorm_t;		// map vert normals

// NOTE: that edge 0 is never used, because negative edge nums
// are used for counterclockwise use of the edge in a face
typedef struct
{
	uint16_t v[2];			// vertex numbers
} dedge_t;

typedef struct
{
	int32_t	v[2];			// vertex numbers
} dedge32_t;

typedef struct
{
	word	planenum;
	int16_t	side;

	int32_t	firstedge;		// we must support > 64k edges
	int16_t	numedges;
	int16_t	texinfo;

	// lighting info
	uint8_t	styles[MAXLIGHTMAPS];
	int32_t	lightofs;			// start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
	int32_t	planenum;
	int32_t	side;

	int32_t	firstedge;		// we must support > 64k edges
	int32_t	numedges;
	int32_t	texinfo;

	// lighting info
	uint8_t	styles[MAXLIGHTMAPS];
	int32_t	lightofs;			// start of [numstyles*surfsize] samples
} dface32_t;

//============================================================================
typedef struct
{
	int16_t	origin[3];	// position of light snapped to the nearest integer
	int16_t	size;		// cubemap side size
} dcubemap_t;

typedef struct
{
	float	normal[3];
} dnormal_t;

typedef struct
{
	char		landname[16];	// name of decsription in mapname_land.txt
	uint16_t	texture_step;	// default is 16, pixels\luxels ratio
	uint16_t	max_extent;	// default is 16, subdivision step ((texture_step * max_extent) - texture_step)
	int16_t		groupid;		// to determine equal landscapes from various groups, -1 - no group
} dfaceinfo_t;

typedef struct
{
	uint8_t		color[6][3];		// 6 sides 1x1 (single pixel per side)
} dlightcube_t;

typedef struct
{
	dlightcube_t	ambient;
	int16_t			origin[3];
	int16_t			leafnum;			// leaf that contain this sample
} dleafsample_t;

typedef enum
{
	emit_ignored = -1,
	emit_surface,
	emit_point,
	emit_spotlight,
	emit_skylight
} emittype_t;

typedef enum
{
	falloff_quake = 0,		// linear (x) (DEFAULT)
	falloff_inverse,		// inverse (1/x), scaled by 1/128
	falloff_inverse2,		// inverse square (1/(x^2)), scaled by 1/(128^2)
	falloff_infinite,		// no attenuation, same brightness at any distance
	falloff_localmin,		// no attenuation, non-additive minlight effect within line of sight of the light source.
	falloff_inverse2a,		// inverse square, with distance adjusted. (1/(x+128)^2), scaled by 1/(128^2)
	falloff_valve,		// special case for valve attenuation style (unreachable by "delay" field)
} fallofftype_t;

#define DWL_FLAGS_INAMBIENTCUBE		0x0001	// This says that the light was put into the per-leaf ambient cubes.

typedef struct
{
	uint8_t		emittype;
	uint8_t		style;
	uint8_t		flags;			// will be set in ComputeLeafAmbientLighting
	int16_t		origin[3];		// light abs origin
	float		intensity[3];	// RGB
	float		normal[3];		// for surfaces and spotlights
	float		stopdot;		// for spotlights
	float		stopdot2;		// for spotlights
	float		fade;			// falloff scaling for linear and inverse square falloff (0.5 = farther, 2.0 = shorter etc)
	float		radius;			// light radius
	int16_t		leafnum;		// light linked into this leaf
	uint8_t		falloff;		// falloff style 0 = default (inverse square), 1 = inverse falloff, 2 = inverse square
	uint16_t	facenum;		// face number for emit_surface
	int16_t		modelnumber;	// g-cont. we can't link lights with entities by entity number so we link it by bmodel number
} dworldlight_t;

#define VLIGHTIDENT		(('T'<<24)+('I'<<16)+('L'<<8)+'V') // little-endian "VLIT"
#define VLIGHT_VERSION	1

#define FLIGHTIDENT		(('P'<<24)+('A'<<16)+('M'<<8)+'L') // little-endian "LMAP"
#define FLIGHT_VERSION	2

#include "color24.h"

typedef struct
{
	color24		light[MAXLIGHTMAPS];	// lightvalue
	color24		deluxe[MAXLIGHTMAPS];	// deluxe vectors
	uint8_t		shadow[MAXLIGHTMAPS];	// shadowmap
} dvertlight_t;

typedef struct
{
	uint8_t		styles[MAXLIGHTMAPS];
	int32_t		lightofs;			// -1 no lightdata
} dfacelight_t;

typedef struct
{
	int32_t		submodel_offset;		// hack to determine submodel
	int32_t		vertex_offset;
} dvlightofs_t;

typedef struct
{
	int32_t		submodel_offset;		// hack to determine submodel
	int32_t		surface_offset;
} dflightofs_t;

typedef struct
{
	uint32_t	modelCRC;			// catch for model changes
	int32_t		numverts;
	uint8_t		styles[MAXLIGHTMAPS];
	dvlightofs_t	submodels[32];		// MAXSTUDIOMODELS
	dvertlight_t	verts[3];			// variable sized
} dmodelvertlight_t;

typedef struct
{
	uint32_t	modelCRC;			// catch for model changes
	int32_t		numfaces;
	uint8_t		styles[MAXLIGHTMAPS];
	int16_t		texture_step;
	float		origin[3];
	float		angles[3];
	float		scale[3];
	dflightofs_t	submodels[32];		// MAXSTUDIOMODELS
	dfacelight_t	faces[3];			// variable sized
} dmodelfacelight_t;

typedef struct
{
	int32_t		ident;			// to differentiate from previous lump LUMP_LEAF_LIGHTING
	int32_t		version;			// data package version
	int32_t		nummodels;
	int32_t		dataofs[4];		// [nummodels]
} dvlightlump_t;

#define NORMIDENT		(('T'<<24)+('B'<<16)+('T'<<8)+'Q') // little-endian "QTBN"

typedef struct
{
	int32_t		ident;			// to differentiate from non-indexed normals storage
	int32_t		numnormals;		// dvertnorm[numsurfedges] dnormals[numnormals]
} dnormallump_t;

#endif//BSPFILE_H