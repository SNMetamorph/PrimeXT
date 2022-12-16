/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/


#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "scriplib.h"
#include "filesystem.h"
#include "polylib.h"
#include "threads.h"
#include "bspfile.h"
#include "shaders.h"
#include "port.h"
#include "utlarray.h"
#include <stdint.h>

#ifndef DOUBLEVEC_T
#error you must add -dDOUBLEVEC_T to the project!
#endif

#define PLANE_HASHES		8192
#define MAX_SWITCHED_LIGHTS		32 

// supported map formats
enum
{
	BRUSH_UNKNOWN = 0,
	BRUSH_WORLDCRAFT_21,	// quake worldcraft <= 2.1
	BRUSH_WORLDCRAFT_22,	// half-life worldcraft >= 2.2
	BRUSH_RADIANT,
	BRUSH_QUARK,
	BRUSH_COUNT
};

// this part is shared with bsp
#define FSIDE_NODRAW	BIT( 0 )	// don't store face into bsp
#define FSIDE_HINT		BIT( 1 )	// this is a hint for bsp partition
#define FSIDE_SOLIDHINT	BIT( 2 )	// solid hint
#define FSIDE_SKY		BIT( 3 )	// eliminate this face after CSG

// this part a private to csg
#define FSIDE_USED		BIT( 4 )	// just for statistics
#define FSIDE_NOLIGHTMAP	BIT( 5 )	// cause to set TEX_SPECIAL
#define FSIDE_SKIP		BIT( 6 )	// eliminate this face after CSG
#define FSIDE_NOSHADOW	BIT( 7 )	// eliminate this face after CSG
#define FSIDE_NODIRT	BIT( 8 )	// eliminate this face after CSG
#define FSIDE_PATCH		BIT( 9 )	// ignore worldluxels
#define FSIDE_SCROLL	BIT( 10 )	// doom scroll-side effect

// brush flags
#define FBRUSH_CLIPONLY	BIT( 0 )	// this is clipbrush
#define FBRUSH_NOCLIP	BIT( 1 )	// only visible hull
#define FBRUSH_NOCSG	BIT( 2 )	// ignore CSG operations for this brush
#define FBRUSH_REMOVE	BIT( 3 )	// this brush should be removed at parse-time
#define FBRUSH_PATCH	BIT( 4 )	// it's part of patch
#define FBRUSH_PRECISIONCLIP	BIT( 5 )	// force old reliable code in some cases

#define CONTENTS_NULL	-99	// helper content

// !!! if this is changed, it must be changed in bsp5.h too !!!
typedef struct
{
	vec3_t		normal;
	vec3_t		origin;
	vec_t		dist;
	int		type;
	int		hash_chain;	// hash chain (in BSP it was called a outplanenum )
} plane_t;

typedef union
{
	struct
	{
		vec_t	UAxis[3];
		vec_t	VAxis[3];
		vec_t	shift[2];
		vec_t	scale[2];
		vec_t	rotate;
	}; // Worldcraft, Hammer

	struct
	{
		vec_t	vecs[2][4];
	}; // QuArK
	struct
	{
		vec_t	matrix[2][3];
	}; // Radiant (brush primitives)
} texvecs_t;

typedef struct side_s
{
	char		name[64];		// path to texture or shader
	vec_t		vecs[2][4];	// texture vectors
	vec_t		planepts[3][3];	// source points
	shaderInfo_t	*shader;		// shader settings
	short		faceinfo;		// terrain stuff
	int		planenum;		// plane from points
	int		contents;		// side contents
	int		flags;		// side settings
} side_t;

typedef struct bface_s
{
	struct bface_s *next;
	int			planenum;
	plane_t		*plane;
	winding_t	*w;
	int			texinfo;
	int			flags;		// side settings
	int8_t		contents[2];	// 0 = front side
	vec3_t		mins, maxs;
} bface_t;

typedef struct
{
	vec3_t		mins, maxs;
	bface_t		*faces;
} brushhull_t;

typedef struct brush_s
{
	// debug info
	int		originalentitynum;	// debug info
	int		originalbrushnum;	// debug info
	int		entitynum;	// upcast to entity

	// brush settings
	int		flags;		// brush settings
	int8_t	detaillevel;
	int		contents;
	int		csg_detaillevel( int hull ) const	{ return (hull == 0) ? detaillevel : 0; }

	CUtlArray<side_t>	sides;		// source data
	brushhull_t	hull[MAX_MAP_HULLS];// output data
} brush_t;

typedef struct mapent_s
{
// this part shared with entity_t
	epair_t		*epairs;		// head
	epair_t		*tail;		// tail
	vec3_t		origin;
	vec3_t		absmin;
	vec3_t		absmax;

// this part is unique for mapent_t
	int		firstbrush;
	int		numbrushes;
	CUtlArray<brush_t>	brushes;		// parsed brushes (valid only at LoadMapFile)
} mapent_t;

typedef struct
{
	char		name[64];
	int		width, height;
	byte		*data;
	size_t		datasize;
	int		wadnum;	// wadlist[wadnum]
} mipentry_t;

extern CUtlArray<mapent_t>	g_mapentities;
extern plane_t		g_mapplanes[MAX_INTERNAL_MAP_PLANES];
extern int		g_nummapplanes;
extern brush_t		g_mapbrushes[MAX_MAP_BRUSHES];
extern int		g_nummapbrushes;
extern int		g_numparsedentities;
extern int		g_numparsedbrushes;
extern int		g_world_luxels;

extern bool		g_noclip;
extern bool		g_wadtextures;
extern bool		g_nullifytrigger;
extern bool		g_compatibility_goldsrc;
extern bool		g_onlyents;
extern vec_t	g_csgepsilon;

extern char		g_pszWadInclude[MAX_TEXFILES][64];
extern int		g_nWadInclude;
extern int		c_outfaces;

//=============================================================================

// textures.c

void TEX_LoadTextures( CUtlArray<mapent_t> *entities, bool merge );
void TEX_FreeTextures( void );
void WriteMiptex( void );
void TEX_GetSize( const char *name, int *width, int *height );
const char *TEX_GetMiptexNameByHash( int hash );
void TextureAxisFromNormal( vec3_t normal, vec3_t xv, vec3_t yv, bool brush_primitive );
int TexinfoForSide( plane_t *plane, side_t *s, const vec3_t origin );
short FaceinfoForTexinfo( const char *landname, const int in_texture_step, const int in_max_extent, const int groupid );
void TextureAxisFromSide( const side_t *side, vec3_t xv, vec3_t yv, bool brush_primitive );

//=============================================================================

// brush.c

int PlaneFromPoints( const vec_t *p0, const vec_t *p1, const vec_t *p2 );
int BrushContents( mapent_t *mapent, brush_t *b );
brush_t *Brush_LoadEntity( mapent_t *ent, int hullnum );
int PlaneTypeForNormal( const vec3_t normal );
void CreateBrush( int brushnum, int threadnum = -1 );
brush_t *AllocBrush( mapent_t *entity );
side_t *AllocSide( brush_t *brush );
void CreateBrushFaces( brush_t *b );
void DeleteBrushFaces( brush_t *b );
void ProcessAutoOrigins( void );
void RestoreModelOrigins( void );
void DumpBrushPlanes( void );
void FreeHullFaces( void );

//=============================================================================

// patch.c
void ParsePatch( mapent_t *mapent, short entindex, short faceinfo, short &brush_type );

//=============================================================================
// utils.c

bool InsertASEModel( const char *modelName, mapent_t *mapent, short entindex, short faceinfo );
void TEX_LoadTGA( const char *texname, mipentry_t *tex );

// map.c

void LoadMapFile( const char *filename );
void FreeMapEntity( mapent_t *mapent );
void UnparseMapEntities( void );
void FreeMapEntities( void );
mapent_t *MapEntityForModel( const int modnum );
mapent_t *FindTargetMapEntity( CUtlArray<mapent_t> *entities, const char *target );
bool IncludeMapFile( const char *filename, CUtlArray<mapent_t> *entities, int index, bool external_map );

//=============================================================================

// csg4.c

bface_t *AllocFace( void );
void FreeFace( bface_t *f );
bface_t *NewFaceFromFace( const bface_t *in );
bface_t *CopyFace( const bface_t *f );
void UnlinkFaces( bface_t **head, bface_t *face = NULL );
void EmitFace( int hull, const bface_t *f, int detaillevel = 0 );
void EmitDetailBrush( int hull, const bface_t *faces );
void ChopEntityBrushes( mapent_t *mapent );
void WriteMapBrushes( brush_t *b, bface_t *outside );

//=============================================================================

// hullfile.c

void CheckHullFile( const char *filename );
