/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// comiple-time settings
#define BSPVERSION			HLBSP_VERSION
#define COMPILERSTRING		"^1P2ST ^3v0.98^7"
#define TOOLNAME			"^1P2:Savior^7 tools"
#define VERSIONSTRING		"^3v.0.98^7"

// compile-time configuration like ZHLT and VHLT
#define HLCSG_SKYFIXEDSTYLE		// P2:Savior used to differentiate sky light from other light sources
#define HLVIS_SORT_PORTALS		// sort portals before flow
#define HLVIS_MERGE_PORTALS		// merge visibility between portals
#define HLRAD_COMPRESS_TRANSFERS	// saves 80% memory
#define HLRAD_RIGHTROUND		// when you go down, when you go down-down!
#define HLRAD_TestLine_EDGE_FIX	// remove shadow artifacts in some cases

// P2:Savior
#ifdef HLRAD_PSAVIOR
#define HLRAD_REFLECTIVITY
#define HLRAD_DELUXEMAPPING		// store deluxe vectors for bump-mapping
//#define HLRAD_SHADOWMAPPING		// store shadow intensity for bump-mapping
#define HLRAD_VERTEXLIGHTING		// vertex lighting for static models
#define HLRAD_LIGHTMAPMODELS		// calculate lightmaps on the models
#define HLRAD_AMBIENTCUBES		// leaf ambient lighting
#define HLRAD_COMPUTE_VISLIGHTMATRIX	// XashNT experiments
#define HLRAD_EXTERNAL_TEXTURES	// allow to loading external textures
#define HLRAD_SHRINK_MEMORY		// TESTTEST
#define HLRAD_RAYTRACE		// TESTTEST
#endif

// Paranoia compatible
#ifdef HLRAD_PARANOIA
#define HLRAD_PARANOIA_BUMP		// compute Paranoia-style bump (three lightstyles)
#define HLRAD_DELUXEMAPPING		// store deluxe vectors for bump-mapping
#define HLRAD_VERTEXLIGHTING		// vertex lighting for static models
#endif

// GoldSrc compatible
#ifdef HLRAD_GOLDSRC
#define HLRAD_REFLECTIVITY
#define HLRAD_VERTEXLIGHTING		// vertex lighting for static models
#define HLRAD_AMBIENTCUBES		// leaf ambient lighting
#endif

// epsilon values (critical stuff)
#define NORMAL_EPSILON   		1e-5
#define DIR_EPSILON			1e-4
#define DIST_EPSILON       		4e-2
#define EQUAL_EPSILON    		4e-3
#define ON_EPSILON			4e-2
#define POINT_EPSILON		(ON_EPSILON * 0.5)
#define CSGCHOP_EPSILON		ON_EPSILON
#define PRTCHOP_EPSILON		ON_EPSILON
#define MICROVOLUME			0.1
#define T_EPSILON			ON_EPSILON
#define HULL_VISIBLE		0

#define IsLiquidContents( cnt )	( cnt == CONTENTS_WATER || cnt == CONTENTS_SLIME || cnt == CONTENTS_LAVA )
#define CHECKVISBIT( vis, b )		((b) >= 0 ? (byte)((vis)[(b) >> 3] & (1 << ((b) & 7))) : (byte)false )
#define SETVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] |= (1 << ((b) & 7))) : (byte)false )
#define CLEARVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] &= ~(1 << ((b) & 7))) : (byte)false )

/*
==============================================================================

BRUSH MODELS

.bsp contain level static geometry with including PVS and lightning info
==============================================================================
*/
#include "..\..\common\bspfile.h"

// header
#define Q1BSP_VERSION		29	// quake1 regular version (beta is 28)
#define HLBSP_VERSION		30	// half-life regular version

#define IDEXTRAHEADER		(('H'<<24)+('S'<<16)+('A'<<8)+'X') // little-endian "XASH"
#define EXTRA_VERSION		4	// ver. 1 was occupied by old versions of XashXT, ver. 2 was occupied by old vesrions of P2:savior
					// ver. 3 was occupied by experimental versions of P2:savior change fmt
#define EXTRA_VERSION_OLD		2	// extra version 2 (P2:Savior regular version) to get minimal backward compatibility

// upper design bounds
#define MAX_MAP_HULLS		4

#define MAXPOINTS			28	// don't let a base face get past this
					// because it can be split more later

#define MAX_TEXFILES		128	// max connected wad-files
#define MAX_MAP_BRUSHES		68000	// even ad_sepulcher never reach this value
#define MAX_MAP_ENTSTRING		0x200000	// 2 mB should be enough
#define MAX_INTERNAL_MAP_PLANES	524288	// internal compiler limit
#define MAX_INTERNAL_MAP_TEXINFO	262144	// internal compiler limit
#define MAX_MAP_NORMS		32768	// sphere with step as two degrees (180*180=32400)
#define MAX_MAP_VERTNORMS		0x200000	// can be increased but not needed
#define MAX_MAP_PORTALS		65536

// lightmap resolution
#define TEXTURE_STEP		16		// if lightmap extent exceeds 16, the map will not be able to load in 'Software Mode'
#define WORLD_TEXTURE_STEP		1
#define TEXTURE_EXTRA_STEP		8
#define MAX_SURFACE_EXTENT		16		// renderer and HLDS. //--vluzacn
#define MAX_EXTRA_EXTENTS		64

#define MIN_CUSTOM_SURFACE_EXTENT	8
#define MAX_CUSTOM_SURFACE_EXTENT	127		// don't increase this because GoldSrc default lightmap page resolution is 128x128
#define MAX_MODEL_SURFACE_EXTENT	511
#define MIN_CUSTOM_TEXTURE_STEP	2
#define MAX_CUSTOM_TEXTURE_STEP	64
#define MIN_STUDIO_TEXTURE_STEP	1
#define MAX_STUDIO_TEXTURE_STEP	256

// edges limit per face (software renderer reqiured that)
#define MAX_VERTS_ON_FACE		MAXPOINTS

// temporary contents (not stored into BSP)
#define CONTENTS_ORIGIN		-7		// removed at csg time
#define CONTENTS_VISBLOCKER		-8		// removed at bsp time

//============================================================================
// the utilities get to be lazy and just use large static arrays

extern int		g_nummodels;
extern dmodel_t		g_dmodels[MAX_MAP_MODELS];

extern int		g_visdatasize;
extern byte		g_dvisdata[MAX_MAP_VISIBILITY];

extern int		g_lightdatasize;
extern byte		*g_dlightdata;

extern int		g_deluxdatasize;
extern byte		*g_ddeluxdata;

extern int		g_shadowdatasize;
extern byte		*g_dshadowdata;

extern int		g_texdatasize;
extern byte		*g_dtexdata; // (dmiptexlump_t)

extern int		g_vislightdatasize;
extern byte		*g_dvislightdata;

extern int		g_entdatasize;
extern char		g_dentdata[MAX_MAP_ENTSTRING];

extern int		g_numleafs;
extern int		g_numvisleafs;
extern dleaf_t		g_dleafs[MAX_MAP_LEAFS];

extern int		g_numplanes;
extern dplane_t		g_dplanes[MAX_INTERNAL_MAP_PLANES];

extern int		g_numvertexes;
extern dvertex_t		g_dvertexes[MAX_MAP_VERTS];

extern int		g_numnodes;
extern dnode_t		g_dnodes[MAX_MAP_NODES];

extern int		g_numtexinfo;
extern dtexinfo_t		g_texinfo[MAX_INTERNAL_MAP_TEXINFO];

extern int		g_numfaces;
extern dface_t		g_dfaces[MAX_MAP_FACES];

extern int		g_numclipnodes;
extern dclipnode_t		g_dclipnodes[MAX_MAP_CLIPNODES];
extern dclipnode32_t	g_dclipnodes32[MAX_MAP_CLIPNODES32];

extern int		g_numedges;
extern dedge_t		g_dedges[MAX_MAP_EDGES];

extern int		g_nummarksurfaces;
extern dmarkface_t		g_dmarksurfaces[MAX_MAP_MARKSURFACES];

extern int		g_numsurfedges;
extern dsurfedge_t		g_dsurfedges[MAX_MAP_SURFEDGES];

extern int		g_numfaceinfo;
extern dfaceinfo_t		g_dfaceinfo[MAX_MAP_FACEINFO];

extern int		g_numcubemaps;
extern dcubemap_t		g_dcubemaps[MAX_MAP_CUBEMAPS];

extern int		g_numworldlights;
extern dworldlight_t	g_dworldlights[MAX_MAP_WORLDLIGHTS];

extern int		g_numleaflights;
extern dleafsample_t	g_dleaflights[MAX_MAP_LEAFLIGHTS];

extern int		g_vlightdatasize;
extern byte		*g_dvlightdata; // (dvlightlump_t)

extern int		g_flightdatasize;
extern byte		*g_dflightdata; // (dflightlump_t)

extern int		g_normaldatasize;
extern byte		*g_dnormaldata;

extern vec3_t		g_hull_size[MAX_MAP_HULLS][2];
extern bool		g_found_extradata;
extern char		g_wadpath[1024];

void DecompressVis( byte *in, byte *decompressed );
int CompressVis( const byte *src, const uint src_length, byte *dest, uint dest_length );

void LoadBSPFile( const char *filename );
void WriteBSPFile( const char *filename );
void PrintBSPFileSizes( void );

const char *ContentsToString( int type );
int GetSurfaceExtent( const dface_t *f );
int GetTextureStep( const dface_t *f, bool force = false );
int GetSurfaceExtent( const dtexinfo_t *tex );
int GetTextureStep( const dtexinfo_t *tex, bool force = false );
word GetSurfaceGroupId( const dtexinfo_t *tex );
word GetSurfaceGroupId( const dface_t *f );
const char *GetTextureByTexinfo( int texinfo );
void LightMatrixFromTexMatrix( const dtexinfo_t *tx, float lmvecs[2][4] );
int MapPlaneTypeForNormal( const vec3_t normal );
int GetFaceContents( const char *name );
void MakeAxial( float normal[3] );
int SnapNormal( vec3_t normal );

//===============
#define STRUCT_FROM_LINK( l, t, m )	((t *)((byte *)l - (int)&(((t *)0)->m)))
#define ENTITY_FROM_AREA( l )		STRUCT_FROM_LINK( l, entity_t, area )
#define TFACE_FROM_AREA( l )		STRUCT_FROM_LINK( l, tface_t, area )

#define AREA_NODES			BIT( AREA_MAX_DEPTH + 1 )
#define AREA_MAX_DEPTH		10	// maximum depth
#define AREA_MIN_DEPTH		4	// default depth

typedef enum
{
	mod_unknown = 0,
	mod_brush,
	mod_studio,
	mod_alias,
} modtype_t;

typedef struct epair_s
{
	struct epair_s	*next;
	struct epair_s	*prev;	// used to save fields in the right ordering
	char		*key;
	char		*value;
} epair_t;

typedef struct link_s
{
	struct link_s	*prev;
	struct link_s	*next;
} link_t;

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		solid_edicts;
} areanode_t;

typedef struct
{
	areanode_t	areanodes[AREA_NODES];
	int		numareanodes;
} aabb_tree_t;

typedef struct entity_s
{
// this part shared with mapentity_t
	epair_t		*epairs;	// head
	epair_t		*tail;	// tail
	vec3_t		origin;
	vec3_t		absmin;
	vec3_t		absmax;

// this part is unique for entity_t
	// rad trace
	link_t		area;		// linked to a division node or leaf
	modtype_t		modtype;
	void		*cache;		// rad usage
} entity_t;

extern int	g_numentities;
extern entity_t	g_entities[MAX_MAP_ENTITIES];

void ParseEntities( void );
void UnparseEntities( void );
void FreeEntities( void );
void FreeEntity( entity_t *mapent );
void CheckHullFile( const char *filename );
void CopyEpairs( entity_t *dst, entity_t *src );
void InsertLinkBefore( epair_t *e, entity_t *mapent );
void RenameKey( entity_t *ent, const char *key, const char *name );
void SetKeyValue( entity_t *ent, const char *key, const char *value );
char *ValueForKey( entity_t *ent, const char *key, bool check = false ); // will return "" if not present
#define CheckKey( ent, key )	ValueForKey( ent, key, true )
void RemoveKey( entity_t *ent, const char *key );
bool BoolForKey( entity_t *ent, const char *key );
int IntForKey( entity_t *ent, const char *key );
vec_t FloatForKey( entity_t *ent, const char *key );
int GetVectorForKey( entity_t *ent, const char *key, vec3_t vec );
void SetVectorForKey( entity_t *ent, const char *key, vec3_t vec, bool precise = false );
entity_t *FindTargetEntity( const char *target );
entity_t *EntityForModel( const int modnum );
dmodel_t *ModelForEntity( entity_t *ent );
epair_t *ParseEpair( void );

void ClearLink( link_t *l );
void RemoveLink( link_t *l );
void InsertLinkBefore( link_t *l, link_t *before );
areanode_t *CreateAreaNode( aabb_tree_t *tree, int depth, int maxdepth, vec3_t mins, vec3_t maxs );
void StripTrailing( char *e );
