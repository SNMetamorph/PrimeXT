#ifndef BSPFILE_H__
#define BSPFILE_H__
#include "cmdlib.h" //--vluzacn

#if _MSC_VER >= 1000
#pragma once
#endif

// upper design bounds

#define MAX_MAP_HULLS            4
// hard limit

#define MAX_MAP_MODELS         1024 //400 //vluzacn
// variable, but 400 brush entities is very stressful on the engine and network code as it is

#define MAX_MAP_BRUSHES       32768
// arbitrary, but large numbers of brushes generally require more lightmap's than the compiler can handle

#define MAX_ENGINE_ENTITIES   16384 //1024 //vluzacn
#define MAX_MAP_ENTITIES      16384 //2048 //vluzacn
// hard limit, in actuallity it is too much, as temporary entities in the game plus static map entities can overflow

#define MAX_MAP_ENTSTRING   (2048*1024) //(512*1024) //vluzacn
// abitrary, 512Kb of string data should be plenty even with TFC FGD's

#define MAX_MAP_PLANES      65536 // TODO: This can be larger, because although faces can only use plane 0~32767, clipnodes can use plane 0-65535. --vluzacn
#define MAX_INTERNAL_MAP_PLANES (512*1024)
// (from email): I have been building a rather complicated map, and using your latest 
// tools (1.61) it seemed to compile fine.  However, in game, the engine was dropping
// a lot of faces from almost every FUNC_WALL, and also caused a strange texture 
// phenomenon in software mode (see attached screen shot).  When I compiled with v1.41,
// I noticed that it hit the MAX_MAP_PLANES limit of 32k.  After deleting some brushes
// I was able to bring the map under the limit, and all of the previous errors went away.

#define MAX_MAP_NODES        32767
// hard limit (negative short's are used as contents values)
#define MAX_MAP_CLIPNODES    32767
// hard limit (negative short's are used as contents values)

#ifdef ZHLT_MAX_MAP_LEAFS
#define MAX_MAP_LEAFS        32760
#define MAX_MAP_LEAFS_ENGINE 8192
// No problem has been observed in testmap or reported, except when viewing the map from outside (some leafs missing, no crash).
// This problem indicates that engine's MAX_MAP_LEAFS is 8192 (for reason, see: Quake - gl_model.c - Mod_Init).
// I don't know if visleafs > 8192 will cause Mod_DecompressVis overflow.
#else
#define MAX_MAP_LEAFS         8192
// hard limit (halflife depends on it to setup pvs bits correctly)
#endif

#define MAX_MAP_VERTS        65535
#define MAX_MAP_FACES        65535 // This ought to be 32768, otherwise faces(in world) can become invisible. --vluzacn
#ifdef ZHLT_WARNWORLDFACES
#define MAX_MAP_WORLDFACES   32768
#endif
#define MAX_MAP_MARKSURFACES 65535
// hard limit (data structures store them as unsigned shorts)

#define MAX_MAP_TEXTURES      4096 //512 //vluzacn
// hard limit (halflife limitation) // I used 2048 different textures in a test map and everything looks fine in both opengl and d3d mode.

#define MAX_MAP_TEXINFO      32767
// hard limit (face.texinfo is signed short)
#ifdef HLCSG_HLBSP_REDUCETEXTURE
#define MAX_INTERNAL_MAP_TEXINFO 262144
#endif

#define MAX_MAP_EDGES       256000
#define MAX_MAP_SURFEDGES   512000
// arbtirary

#define DEFAULT_MAX_MAP_MIPTEX      0x2000000 //0x400000 //vluzacn
// 4Mb of textures is enough especially considering the number of people playing the game
// still with voodoo1 and 2 class cards with limited local memory.

#define DEFAULT_MAX_MAP_LIGHTDATA	0x3000000 //0x600000 //vluzacn
// arbitrary

#define MAX_MAP_VISIBILITY  0x800000 //0x200000 //vluzacn
// arbitrary

// these are for entity key:value pairs
#define MAX_KEY                 128 //32 //vluzacn
#define MAX_VAL               4096 // the name used to be MAX_VALUE //vluzacn
// quote from yahn: 'probably can raise these values if needed'

// texture size limit

#define MAX_TEXTURE_SIZE     1572864 //((256 * 256 * sizeof(short) * 3) / 2) //stop compiler from warning 512*512 texture. --vluzacn
// this is arbitrary, and needs space for the largest realistic texture plus
// room for its mipmaps.'  This value is primarily used to catch damanged or invalid textures
// in a wad file

#ifdef ZHLT_XASH2
#define TEXTURE_STEP        8
#define MAX_SURFACE_EXTENT  64
#else
#define TEXTURE_STEP        16 // this constant was previously defined in lightmap.cpp. --vluzacn
#define MAX_SURFACE_EXTENT  16 // if lightmap extent exceeds 16, the map will not be able to load in 'Software' renderer and HLDS. //--vluzacn
#endif

#ifdef ZHLT_LARGERANGE
#ifdef ZHLT_XASH
#define ENGINE_ENTITY_RANGE 32768.0
#else
#define ENGINE_ENTITY_RANGE 4096.0
#endif
#endif
//=============================================================================

#ifdef ZHLT_XASH2
#define BSPVERSION  31
#else
#define BSPVERSION  30
#endif
#define TOOLVERSION 2


//
// BSP File Structures
//


typedef struct
{
    int             fileofs, filelen;
}
lump_t;

#define LUMP_ENTITIES      0
#define LUMP_PLANES        1
#define LUMP_TEXTURES      2
#define LUMP_VERTEXES      3
#define LUMP_VISIBILITY    4
#define LUMP_NODES         5
#define LUMP_TEXINFO       6
#define LUMP_FACES         7
#define LUMP_LIGHTING      8
#define LUMP_CLIPNODES     9
#define LUMP_LEAFS        10
#define LUMP_MARKSURFACES 11
#define LUMP_EDGES        12
#define LUMP_SURFEDGES    13
#define LUMP_MODELS       14
#ifdef ZHLT_XASH2
#define LUMP_CLIPNODES2   15
#define LUMP_CLIPNODES3   16
#define HEADER_LUMPS      17
#else
#define HEADER_LUMPS      15
#endif

//#define LUMP_MISCPAD      -1
//#define LUMP_ZEROPAD      -2

typedef struct
{
    float           mins[3], maxs[3];
    float           origin[3];
    int             headnode[MAX_MAP_HULLS];
    int             visleafs;                              // not including the solid leaf 0
    int             firstface, numfaces;
}
dmodel_t;

typedef struct
{
    int             version;
    lump_t          lumps[HEADER_LUMPS];
}
dheader_t;

typedef struct
{
    int             nummiptex;
    int             dataofs[4];                            // [nummiptex]
}
dmiptexlump_t;

#define MIPLEVELS   4
typedef struct miptex_s
{
    char            name[16];
    unsigned        width, height;
    unsigned        offsets[MIPLEVELS];                    // four mip maps stored
}
miptex_t;

typedef struct
{
    float           point[3];
}
dvertex_t;

typedef struct
{
    float           normal[3];
    float           dist;
    planetypes      type;                                  // PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
}
dplane_t;

typedef enum
{
    CONTENTS_EMPTY = -1,
    CONTENTS_SOLID = -2,
    CONTENTS_WATER = -3,
    CONTENTS_SLIME = -4,
    CONTENTS_LAVA = -5,
    CONTENTS_SKY = -6,
    CONTENTS_ORIGIN = -7,                                  // removed at csg time
#ifndef HLCSG_CUSTOMHULL
    CONTENTS_CLIP = -8,                                    // changed to contents_solid
#endif

    CONTENTS_CURRENT_0 = -9,
    CONTENTS_CURRENT_90 = -10,
    CONTENTS_CURRENT_180 = -11,
    CONTENTS_CURRENT_270 = -12,
    CONTENTS_CURRENT_UP = -13,
    CONTENTS_CURRENT_DOWN = -14,

    CONTENTS_TRANSLUCENT = -15,
    CONTENTS_HINT = -16,     // Filters down to CONTENTS_EMPTY by bsp, ENGINE SHOULD NEVER SEE THIS

#ifdef ZHLT_NULLTEX
    CONTENTS_NULL = -17,     // AJM  // removed in csg and bsp, VIS or RAD shouldnt have to deal with this, only clip planes!
#endif

#ifdef ZHLT_DETAIL   // AJM
    CONTENTS_DETAIL = -18,  
#endif

#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
	CONTENTS_BOUNDINGBOX = -19, // similar to CONTENTS_ORIGIN
#endif

#ifdef HLCSG_EMPTYBRUSH
	CONTENTS_TOEMPTY = -32,
#endif
}
contents_t;

// !!! if this is changed, it must be changed in asm_i386.h too !!!
typedef struct
{
    int             planenum;
    short           children[2];                           // negative numbers are -(leafs+1), not nodes
    short           mins[3];                               // for sphere culling
    short           maxs[3];
    unsigned short  firstface;
    unsigned short  numfaces;                              // counting both sides
}
dnode_t;

typedef struct
{
    int             planenum;
    short           children[2];                           // negative numbers are contents
}
dclipnode_t;

typedef struct texinfo_s
{
    float           vecs[2][4];                            // [s/t][xyz offset]
    int             miptex;
    int             flags;
}
texinfo_t;

#define TEX_SPECIAL     1                                  // sky or slime or null, no lightmap or 256 subdivision
#ifdef ZHLT_HIDDENSOUNDTEXTURE
#define TEX_SHOULDHIDE  16384 // this flag is temporary; it might be set after CSG, but will be dropped after BSP
#endif

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
    unsigned short  v[2];                                  // vertex numbers
}
dedge_t;

#define MAXLIGHTMAPS    4
typedef struct
{
    unsigned short	planenum;
    short           side;

    int             firstedge;                             // we must support > 64k edges
    short           numedges;
    short           texinfo;

    // lighting info
    byte            styles[MAXLIGHTMAPS];
    int             lightofs;                              // start of [numstyles*surfsize] samples
}
dface_t;

#define AMBIENT_WATER   0
#define AMBIENT_SKY     1
#define AMBIENT_SLIME   2
#define AMBIENT_LAVA    3

#define NUM_AMBIENTS            4                  // automatic ambient sounds

// leaf 0 is the generic CONTENTS_SOLID leaf, used for all solid areas
// all other leafs need visibility info
typedef struct
{
    int             contents;
    int             visofs;                                // -1 = no visibility info

    short           mins[3];                               // for frustum culling
    short           maxs[3];

    unsigned short  firstmarksurface;
    unsigned short  nummarksurfaces;

    byte            ambient_level[NUM_AMBIENTS];
}
dleaf_t;

//============================================================================

#define ANGLE_UP    -1.0 //#define ANGLE_UP    -1 //--vluzacn
#define ANGLE_DOWN  -2.0 //#define ANGLE_DOWN  -2 //--vluzacn

//
// BSP File Data
//

extern int      g_nummodels;
extern dmodel_t g_dmodels[MAX_MAP_MODELS];
extern int      g_dmodels_checksum;

extern int      g_visdatasize;
extern byte     g_dvisdata[MAX_MAP_VISIBILITY];
extern int      g_dvisdata_checksum;

extern int      g_lightdatasize;
extern byte*    g_dlightdata;
extern int      g_dlightdata_checksum;

extern int      g_texdatasize;
extern byte*    g_dtexdata;                                  // (dmiptexlump_t)
extern int      g_dtexdata_checksum;

extern int      g_entdatasize;
extern char     g_dentdata[MAX_MAP_ENTSTRING];
extern int      g_dentdata_checksum;

extern int      g_numleafs;
extern dleaf_t  g_dleafs[MAX_MAP_LEAFS];
extern int      g_dleafs_checksum;

extern int      g_numplanes;
extern dplane_t g_dplanes[MAX_INTERNAL_MAP_PLANES];
extern int      g_dplanes_checksum;

extern int      g_numvertexes;
extern dvertex_t g_dvertexes[MAX_MAP_VERTS];
extern int      g_dvertexes_checksum;

extern int      g_numnodes;
extern dnode_t  g_dnodes[MAX_MAP_NODES];
extern int      g_dnodes_checksum;

extern int      g_numtexinfo;
#ifdef HLCSG_HLBSP_REDUCETEXTURE
extern texinfo_t g_texinfo[MAX_INTERNAL_MAP_TEXINFO];
#else
extern texinfo_t g_texinfo[MAX_MAP_TEXINFO];
#endif
extern int      g_texinfo_checksum;

extern int      g_numfaces;
extern dface_t  g_dfaces[MAX_MAP_FACES];
extern int      g_dfaces_checksum;

#ifdef ZHLT_XASH2
extern int      g_numclipnodes[MAX_MAP_HULLS - 1];
extern dclipnode_t g_dclipnodes[MAX_MAP_HULLS - 1][MAX_MAP_CLIPNODES];
extern int      g_dclipnodes_checksum[MAX_MAP_HULLS - 1];
#else
extern int      g_numclipnodes;
extern dclipnode_t g_dclipnodes[MAX_MAP_CLIPNODES];
extern int      g_dclipnodes_checksum;
#endif

extern int      g_numedges;
extern dedge_t  g_dedges[MAX_MAP_EDGES];
extern int      g_dedges_checksum;

extern int      g_nummarksurfaces;
extern unsigned short g_dmarksurfaces[MAX_MAP_MARKSURFACES];
extern int      g_dmarksurfaces_checksum;

extern int      g_numsurfedges;
extern int      g_dsurfedges[MAX_MAP_SURFEDGES];
extern int      g_dsurfedges_checksum;

extern void     DecompressVis(const byte* src, byte* const dest, const unsigned int dest_length);
extern int      CompressVis(const byte* const src, const unsigned int src_length, byte* dest, unsigned int dest_length);

extern void     LoadBSPImage(dheader_t* header);
extern void     LoadBSPFile(const char* const filename);
extern void     WriteBSPFile(const char* const filename);
extern void     PrintBSPFileSizes();
#ifdef ZHLT_64BIT_FIX
#ifdef PLATFORM_CAN_CALC_EXTENT
extern void		WriteExtentFile (const char *const filename);
extern bool		CalcFaceExtents_test ();
#else
extern void		LoadExtentFile (const char *const filename);
#endif
extern void		GetFaceExtents (int facenum, int mins_out[2], int maxs_out[2]);
#endif
#ifdef ZHLT_EMBEDLIGHTMAP
extern int		ParseImplicitTexinfoFromTexture (int miptex);
extern int		ParseTexinfoForFace (const dface_t *f);
extern void		DeleteEmbeddedLightmaps ();
#endif

//
// Entity Related Stuff
//

typedef struct epair_s
{
    struct epair_s* next;
    char*           key;
    char*           value;
}
epair_t;

typedef struct
{
    vec3_t          origin;
    int             firstbrush;
    int             numbrushes;
    epair_t*        epairs;
}
entity_t;

extern int      g_numentities;
extern entity_t g_entities[MAX_MAP_ENTITIES];

extern void            ParseEntities();
extern void            UnparseEntities();

#ifdef ZHLT_DELETEKEY
extern void            DeleteKey(entity_t* ent, const char* const key);
#endif
extern void            SetKeyValue(entity_t* ent, const char* const key, const char* const value);
extern const char*     ValueForKey(const entity_t* const ent, const char* const key);
extern int             IntForKey(const entity_t* const ent, const char* const key);
extern vec_t           FloatForKey(const entity_t* const ent, const char* const key);
extern void            GetVectorForKey(const entity_t* const ent, const char* const key, vec3_t vec);

extern entity_t* FindTargetEntity(const char* const target);
extern epair_t* ParseEpair();
extern entity_t* EntityForModel(int modnum);

//
// Texture Related Stuff
//

extern int      g_max_map_miptex;
extern int		g_max_map_lightdata;
extern void     dtexdata_init();
extern void CDECL dtexdata_free();

extern char*    GetTextureByNumber(int texturenumber);

#endif //BSPFILE_H__

