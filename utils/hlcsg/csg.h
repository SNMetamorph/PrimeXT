#ifndef HLCSG_H__
#define HLCSG_H__

#if _MSC_VER >= 1000
#pragma once
#endif

#pragma warning(disable: 4786)	// identifier was truncated to '255' characters in the browser information
#include <deque>
#include <string>
#include <map>

#include "cmdlib.h"
#include "messages.h"
#include "win32fix.h"
#include "log.h"
#include "hlassert.h"
#include "mathlib.h"
#include "scriplib.h"
#include "winding.h"
#include "threads.h"
#include "bspfile.h"
#include "blockmem.h"
#include "filelib.h"
#include "boundingbox.h"
// AJM: added in
#include "wadpath.h"
#ifdef ZHLT_PARAMFILE
#include "cmdlinecfg.h"
#endif

#ifndef DOUBLEVEC_T
#error you must add -dDOUBLEVEC_T to the project!
#endif

#define DEFAULT_BRUSH_UNION_THRESHOLD 0.0f
#ifdef ZHLT_WINDING_RemoveColinearPoints_VL
#define DEFAULT_TINY_THRESHOLD        0.0
#else
#define DEFAULT_TINY_THRESHOLD        0.5
#endif
#define DEFAULT_NOCLIP      false
#define DEFAULT_ONLYENTS    false
#define DEFAULT_WADTEXTURES true
#define DEFAULT_SKYCLIP     true
#define DEFAULT_CHART       false
#define DEFAULT_INFO        true

#ifdef HLCSG_PRECISIONCLIP // KGP
#ifdef HLCSG_CLIPTYPEPRECISE_EPSILON_FIX
#define FLOOR_Z 0.7 // Quake default
#else
#define FLOOR_Z 0.5
#endif
#define DEFAULT_CLIPTYPE clip_simple //clip_legacy //--vluzacn
#endif

#ifdef ZHLT_NULLTEX // AJM
#define DEFAULT_NULLTEX     true
#endif

#ifdef HLCSG_CLIPECONOMY // AJM
#ifdef HLCSG_CUSTOMHULL // default clip economy off
#define DEFAULT_CLIPNAZI    false
#else
#define DEFAULT_CLIPNAZI    true
#endif
#endif

#ifdef HLCSG_AUTOWAD //  AJM
#define DEFAULT_WADAUTODETECT false
#endif

#ifdef ZHLT_DETAIL // AJM
#define DEFAULT_DETAIL      true
#endif

#ifdef ZHLT_PROGRESSFILE // AJM
#define DEFAULT_PROGRESSFILE NULL // progress file is only used if g_progressfile is non-null
#endif
#ifdef HLCSG_SCALESIZE
#define DEFAULT_SCALESIZE -1.0 //dont scale
#endif
#ifdef HLCSG_KEEPLOG
#define DEFAULT_RESETLOG true
#endif
#ifdef HLCSG_OPTIMIZELIGHTENTITY
#define DEFAULT_NOLIGHTOPT false
#endif
#ifdef HLCSG_GAMETEXTMESSAGE_UTF8
#define DEFAULT_NOUTF8 false
#endif
#ifdef HLCSG_NULLIFYAAATRIGGER
#define DEFAULT_NULLIFYTRIGGER true
#endif

// AJM: added in
#define UNLESS(a)  if (!(a))

#ifdef SYSTEM_WIN32
#define DEFAULT_ESTIMATE    false
#endif

#ifdef SYSTEM_POSIX
#define DEFAULT_ESTIMATE    true
#endif

#ifdef ZHLT_LARGERANGE
#define BOGUS_RANGE    65534
#else
#define BOGUS_RANGE    8192
#endif

#ifdef HLCSG_HULLBRUSH
#define MAX_HULLSHAPES 128 // arbitrary

#endif
typedef struct
{
    vec3_t          normal;
    vec3_t          origin;
    vec_t           dist;
    planetypes      type;
} plane_t;



typedef struct
{
    vec3_t          UAxis;
    vec3_t          VAxis;
    vec_t           shift[2];
    vec_t           rotate;
    vec_t           scale[2];
} valve_vects;

typedef struct
{
    float           vects[2][4];
} quark_vects;

typedef union
{
    valve_vects     valve;
    quark_vects     quark;
}
vects_union;

extern int      g_nMapFileVersion;                         // map file version * 100 (ie 201), zero for pre-Worldcraft 2.0.1 maps

typedef struct
{
    char            txcommand;
    vects_union     vects;
    char            name[32];
} brush_texture_t;

typedef struct side_s
{
    brush_texture_t td;
#ifdef HLCSG_CUSTOMHULL
	bool			bevel;
#endif
#ifdef ZHLT_HIDDENSOUNDTEXTURE
	bool			shouldhide;
#endif
    vec_t           planepts[3][3];
} side_t;

typedef struct bface_s
{
    struct bface_s* next;
    int             planenum;
    plane_t*        plane;
    Winding*        w;
    int             texinfo;
    bool            used;                                  // just for face counting
    int             contents;
    int             backcontents;
#ifdef HLCSG_CUSTOMHULL
	bool			bevel; //used for ExpandBrush
#endif
    BoundingBox     bounds;
} bface_t;

// NUM_HULLS should be no larger than MAX_MAP_HULLS
#define NUM_HULLS 4

typedef struct
{
    BoundingBox     bounds;
    bface_t*        faces;
} brushhull_t;

typedef struct brush_s
{
#ifdef HLCSG_COUNT_NEW
	int				originalentitynum;
	int				originalbrushnum;
#endif
    int             entitynum;
    int             brushnum;

    int             firstside;
    int             numsides;

#ifdef HLCSG_CLIPECONOMY // AJM
    unsigned int    noclip; // !!!FIXME: this should be a flag bitfield so we can use it for other stuff (ie. is this a detail brush...)
#endif
#ifdef HLCSG_CUSTOMHULL
	unsigned int	cliphull;
	bool			bevel;
#endif
#ifdef ZHLT_DETAILBRUSH
	int				detaillevel;
	int				chopdown; // allow this brush to chop brushes of lower detail level
	int				chopup; // allow this brush to be chopped by brushes of higher detail level
#ifdef ZHLT_CLIPNODEDETAILLEVEL
	int				clipnodedetaillevel;
#endif
#ifdef HLCSG_COPLANARPRIORITY
	int				coplanarpriority;
#endif
#endif
#ifdef HLCSG_HULLBRUSH
	char *			hullshapes[NUM_HULLS]; // might be NULL
#endif

    int             contents;
    brushhull_t     hulls[NUM_HULLS];
} brush_t;

#ifdef HLCSG_HULLBRUSH
typedef struct
{
	vec3_t normal;
	vec3_t point;

	int numvertexes;
	vec3_t *vertexes;
} hullbrushface_t;

typedef struct
{
	vec3_t normals[2];
	vec3_t point;

	vec3_t vertexes[2];
	vec3_t delta; // delta has the same direction as CrossProduct(normals[0],normals[1])
} hullbrushedge_t;

typedef struct
{
	vec3_t point;
} hullbrushvertex_t;

typedef struct
{
	int numfaces;
	hullbrushface_t *faces;
	int numedges;
	hullbrushedge_t *edges;
	int numvertexes;
	hullbrushvertex_t *vertexes;
} hullbrush_t;

typedef struct
{
	char *id;
	bool disabled;
	int numbrushes; // must be 0 or 1
	hullbrush_t **brushes;
} hullshape_t;

#endif
#ifdef HLCSG_GAMETEXTMESSAGE_UTF8
extern char *	ANSItoUTF8 (const char *);
#endif

//=============================================================================
// map.c

extern int      g_nummapbrushes;
extern brush_t  g_mapbrushes[MAX_MAP_BRUSHES];

#define MAX_MAP_SIDES   (MAX_MAP_BRUSHES*6)

extern int      g_numbrushsides;
extern side_t   g_brushsides[MAX_MAP_SIDES];

#ifdef HLCSG_HULLBRUSH
extern hullshape_t g_defaulthulls[NUM_HULLS];
extern int		g_numhullshapes;
extern hullshape_t g_hullshapes[MAX_HULLSHAPES];

#endif
extern void     TextureAxisFromPlane(const plane_t* const pln, vec3_t xv, vec3_t yv);
extern void     LoadMapFile(const char* const filename);

//=============================================================================
// textures.c

typedef std::deque< std::string >::iterator WadInclude_i;
extern std::deque< std::string > g_WadInclude;  // List of substrings to wadinclude

extern void     WriteMiptex();
extern int      TexinfoForBrushTexture(const plane_t* const plane, brush_texture_t* bt, const vec3_t origin
#ifdef ZHLT_HIDDENSOUNDTEXTURE
					, bool shouldhide
#endif
					);
#ifdef HLCSG_HLBSP_VOIDTEXINFO
extern const char *GetTextureByNumber_CSG(int texturenumber);
#endif

//=============================================================================
// brush.c

extern brush_t* Brush_LoadEntity(entity_t* ent, int hullnum);
extern contents_t CheckBrushContents(const brush_t* const b);

extern void     CreateBrush(int brushnum);
#ifdef HLCSG_HULLBRUSH
extern void		CreateHullShape (int entitynum, bool disabled, const char *id, int defaulthulls);
extern void		InitDefaultHulls ();
#endif

//=============================================================================
// csg.c

extern bool     g_chart;
extern bool     g_onlyents;
extern bool     g_noclip;
extern bool     g_wadtextures;
extern bool     g_skyclip;
extern bool     g_estimate;         
extern const char* g_hullfile;        

#ifdef ZHLT_NULLTEX // AJM:
extern bool     g_bUseNullTex; 
#endif

#ifdef ZHLT_DETAIL // AJM
extern bool g_bDetailBrushes;
#endif

#ifdef HLCSG_CLIPECONOMY // AJM:
extern bool     g_bClipNazi; 
#endif

#ifdef HLCSG_PRECISIONCLIP // KGP
#define EnumPrint(a) #a
typedef enum{clip_smallest,clip_normalized,clip_simple,clip_precise,clip_legacy} cliptype;
extern cliptype g_cliptype;
extern const char*	GetClipTypeString(cliptype);
#ifndef HLCSG_CUSTOMHULL
#define TEX_BEVEL 32768
#endif
#endif

#ifdef ZHLT_PROGRESSFILE // AJM
extern char*    g_progressfile ;
#endif
#ifdef HLCSG_SCALESIZE
extern vec_t g_scalesize;
#endif
#ifdef HLCSG_KEEPLOG
extern bool g_resetlog;
#endif
#ifdef HLCSG_OPTIMIZELIGHTENTITY
extern bool g_nolightopt;
#endif
#ifdef HLCSG_GAMETEXTMESSAGE_UTF8
extern bool g_noutf8;
#endif
#ifdef HLCSG_NULLIFYAAATRIGGER
extern bool g_nullifytrigger;
#endif

extern vec_t    g_tiny_threshold;
extern vec_t    g_BrushUnionThreshold;

extern plane_t  g_mapplanes[MAX_INTERNAL_MAP_PLANES];
extern int      g_nummapplanes;

extern bface_t* NewFaceFromFace(const bface_t* const in);
extern bface_t* CopyFace(const bface_t* const f);

extern void     FreeFace(bface_t* f);

extern bface_t* CopyFaceList(bface_t* f);
extern void     FreeFaceList(bface_t* f);

extern void     GetParamsFromEnt(entity_t* mapent);

#ifndef HLCSG_ONLYENTS_NOWADCHANGE
//=============================================================================
// wadinclude.c
// passed 'filename' is extensionless, the function cats ".wic" at runtime

extern void     LoadWadincludeFile(const char* const filename);
extern void     SaveWadincludeFile(const char* const filename);
extern void     HandleWadinclude();
#endif

//=============================================================================
// brushunion.c
void            CalculateBrushUnions(int brushnum);
 
//============================================================================
// hullfile.cpp
extern vec3_t   g_hull_size[NUM_HULLS][2];
extern void     LoadHullfile(const char* filename);

#ifdef HLCSG_WADCFG_NEW
extern const char *g_wadcfgfile;
extern const char *g_wadconfigname;
extern void LoadWadcfgfile (const char *filename);
extern void LoadWadconfig (const char *filename, const char *configname);
#endif
#ifndef HLCSG_WADCFG_NEW
#ifdef HLCSG_WADCFG // AJM: 
//============================================================================
// wadcfg.cpp

extern void     LoadWadConfigFile();
extern void     ProcessWadConfiguration();
extern bool     g_bWadConfigsLoaded;
extern void     WadCfg_cleanup();

#define MAX_WAD_CFG_NAME 32
extern char     wadconfigname[MAX_WAD_CFG_NAME];

//JK: needed in wadcfg.cpp for *nix..
#ifndef SYSTEM_WIN32
extern char *g_apppath;
#endif

//JK: 
extern char *g_wadcfgfile;

#endif // HLCSG_WADCFG
#endif

#ifdef HLCSG_AUTOWAD
//============================================================================
// autowad.cpp      AJM

extern bool     g_bWadAutoDetect; 
#ifndef HLCSG_AUTOWAD_NEW
extern int      g_numUsedTextures;

#ifndef HLCSG_AUTOWAD_TEXTURELIST_FIX
extern void     GetUsedTextures();
#endif
//extern bool     autowad_IsUsedTexture(const char* const texname);
//extern bool     autowad_IsUsedWad(const char* const path);
//extern void     autowad_PurgeName(const char* const texname);
extern void     autowad_cleanup();
extern void     autowad_UpdateUsedWads();
#ifdef HLCSG_AUTOWAD_TEXTURELIST_FIX
extern void     autowad_PushName(const char *texname);
#endif
#endif

#endif // HLCSG_AUTOWAD

//=============================================================================
// properties.cpp

#ifdef HLCSG_NULLIFY_INVISIBLE // KGP
#include <string>
#include <set>
extern void properties_initialize(const char* filename);
extern std::set< std::string > g_invisible_items;
#endif

//============================================================================
#endif//HLCSG_H__

