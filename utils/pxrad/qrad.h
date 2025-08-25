/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "port.h"
#include <fcntl.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "polylib.h"
#include "threads.h"
#include "stringlib.h"
#include "file_system.h"
#include "utlarray.h"
#include "compatibility_mode.h"
#include "imagelib.h"

#define DEFAULT_FASTMODE		false
#define DEFAULT_EXTRAMODE		false
#define DEFAULT_TEXSCALE		true
#define DEFAULT_CHOP			128.0
#define DEFAULT_TEXCHOP			32.0
#define DEFAULT_DLIGHT_SCALE	1.0
#define DEFAULT_LIGHT_SCALE		1.0
#define DEFAULT_LERP_ENABLED	false
#define DEFAULT_WADTEXTURES		false
#define DEFAULT_DIRTMAPPING		false
#define DEFAULT_TEXREFLECTGAMMA 2.2	// turn back to linear space
#define DEFAULT_TEXREFLECTSCALE	1.0
#define DEFAULT_BLUR			1.0
#define DEFAULT_BOUNCE			8
#define DEFAULT_STUDIO_BOUNCE	4
#define DEFAULT_LIGHTCLIP		255
#define DEFAULT_SMOOTHVALUE		50.0
#define DEFAULT_INDIRECT_SUN	1.0
#define DEFAULT_GAMMA			(1.0 / 2.2)
#define DEFAULT_COMPAT_MODE		CompatibilityMode::PrimeXT
#define DLIGHT_THRESHOLD		0.0f
#define DEFAULT_GLOBAL_SCALE	0.5f
#define DEFAULT_LIGHTPROBE_EPSILON	0.1f
#define	STUDIO_SURFACE_HIT		-2
	
// worldcraft predefined angles
#define ANGLE_UP			-1
#define ANGLE_DOWN			-2

#define DIRECT_SCALE			0.1
#define DEFAULT_HUNT_OFFSET		0.5
#define DEFAULT_EDGE_WIDTH		0.8
#define HUNT_WALL_EPSILON		(ON_EPSILON * 3)
#define DIFFUSE_DIRECTION_SCALE	(2.0 / 3.0)
#define MINIMUM_PATCH_DISTANCE	ON_EPSILON
#define ACCURATEBOUNCE_THRESHOLD	4.0	// If the receiver patch is closer to emitter patch than
					// EXACTBOUNCE_THRESHOLD * emitter_patch->radius, calculate the exact visibility amount.
#define ACCURATEBOUNCE_DEFAULT_SKYLEVEL	5	// sample 1026 normals
#define SKYLEVELMAX				8
#define SKYLEVEL_SOFTSKYON		7
#define SKYLEVEL_SOFTSKYOFF		4
#define SKYLEVEL_FASTSKY		6
#define STUDIO_SAMPLES_PER_PASS	32
#define STUDIO_SAMPLES_SKY		256
#define SUNSPREAD_SKYLEVEL		7
#define SUNSPREAD_THRESHOLD		15.0
#define NUMVERTEXNORMALS		162
#define LF_SCALE				128.0	// TyrUtils magic value
#define DEFAULT_GAMMAMODE		0
#define FRAC_EPSILON			(1.0f / 32.0f)

#define MAX_SINGLEMAP			((MAX_CUSTOM_SURFACE_EXTENT+1) * (MAX_CUSTOM_SURFACE_EXTENT+1) * 3)
#define MAX_SINGLEMAP_MODEL		((MAX_MODEL_SURFACE_EXTENT+1) * (MAX_MODEL_SURFACE_EXTENT+1) * 3)
#define MAX_SUBDIVIDE			16384
#define MAX_TEXLIGHTS			1024

// Paranoia settings
#define LIGHTFLAG_NOT_NORMAL	2
#define LIGHTFLAG_NOT_RENDERER	4

#define HALF_FLOAT_TRANSFERS

enum
{
	STYLE_ORIGINAL_LIGHT	= 0,	// direct + indirect, used while gl_renderer is 0
	STYLE_BUMPED_LIGHT		= 1,
	STYLE_INDIRECT_LIGHT	= 2,	// store indirect lighting from patches
};

// buz
enum
{
	BUMP_BASELIGHT_MAP		= 1,	// indirect
	BUMP_LIGHTVECS_MAP		= 2,	// light vectors
	BUMP_ADDLIGHT_MAP		= 3,	// direct
	BUMP_BASELIGHT_STYLE	= 61,
	BUMP_ADDLIGHT_STYLE		= 62,
	BUMP_LIGHTVECS_STYLE	= 63,
};

typedef struct
{
	char		name[256];
	const char	*filename;
	vec3_t		value;
	vec_t		fade;
} texlight_t;

typedef CUtlArray<int> CIntVector;

typedef struct dlight_s
{
	// private part
	struct dlight_s	*next;
	vec_t		patch_area;
	vec_t		patch_emitter_range;
	struct patch_s	*patch;
	byte		*pvs;		// accumulated domain of the light
	int			flags;		// buz: how to work without flags???

	// sun spread stuff
	vec_t		*sunnormalweights;
	vec_t		sunspreadangle;
	int			numsunnormals;
	vec3_t		*sunnormals;

// this part is shared with dworldlight_t
	emittype_t	type;
	int			style;
	vec_t		fade;		// falloff scaling 1.0 = normal, 0.5 = farther, 2.0 = shorter etc
	byte		falloff;		// falloff style 0,2 = inverse square, 1 = inverse falloff
	word		lightnum;		// worldlight number

	vec3_t		origin;
	vec3_t		intensity;
	vec3_t		diffuse_intensity;	// skylight only
	vec3_t		normal;		// for surfaces and spotlights
	float		stopdot;		// for spotlights
	float		stopdot2;		// for spotlights
	float		lf_scale;		// 1.0 for half-life, 128.0 for quake
	bool		topatch;
	int			facenum;
	word		modelnum;
	float		radius;
} directlight_t;

typedef struct
{
	vec3_t		point;		// originally that called a surfpt
	bool		occluded;		// luxel was occluded
	vec3_t		position;
	int			surface;
} surfpt_t;

typedef struct
{
	union
	{
	int			surfnum;
	entity_t	*mapent;
	};

	// reuse it for studiomodels
	union
	{
	const dplane_t	*plane;	// faceplane
	struct tface_t	*tface;
	};
	union
	{
	dface_t			*face;
	struct tmesh_t	*mesh;
	};

	vec3_t		texorg;
	vec3_t		worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t		textoworld[2];	// world = texorg + s * textoworld[0]
	vec_t		exactmins[2];
	vec_t		exactmaxs[2];
	int		texmins[2];
	int		texsize[2];

	surfpt_t		*surfpt;		// product of CalcPoints
	int		numsurfpt;

	int		lmcache_side;
	int		lmcache_density;	// shared by both s and t direction
	int		lmcache_offset;	// shared by both s and t direction
	int		lmcachewidth;
	int		lmcacheheight;
	vec3_t		(*light)[MAXLIGHTMAPS];
#ifdef HLRAD_DELUXEMAPPING
	vec3_t		*normals;
	vec3_t		(*deluxe)[MAXLIGHTMAPS];
#ifdef HLRAD_SHADOWMAPPING
	vec_t		(*shadow)[MAXLIGHTMAPS];
#endif
#endif
} lightinfo_t;

typedef struct 
{
	vec3_t		facenormal;		// face normal
	int			numneighbors;	// neighboring faces that share vertices
	int			*neighbor;		// neighboring face list (max of 64)
	short		texmins[2];		// also used for face testing
	short		extents[2];
	short		lightmapmins[2];// lightmatrix
	short		lightextents[2];
} faceneighbor_t;

typedef struct
{
	dface_t		*faces[2];
	vec3_t		interface_normal;
	vec3_t		vertex_normal[2];
	bool		coplanar, smooth;
	vec_t		cos_normals_angle;
	matrix3x4	textotex[2];	// how we translate texture coordinates from one face to the other face
} edgeshare_t;

struct trace_t
{
	int		contents;
	float	fraction;		// time completed, 1.0 = didn't hit anything
	int		surface;		// return the facenum

	//gi from studiomodels
	vec3_t	light[MAXLIGHTMAPS];
	byte	styles[MAXLIGHTMAPS];
};

#ifdef HALF_FLOAT_TRANSFERS
#define TRANSFER_SCALE_VAL		(1.0f)
#define TRANSFER_SCALE_MAX		M_PI2
#else
#define TRANSFER_SCALE_VAL		(16384)
#define TRANSFER_SCALE_MAX		(65536.0f)
#endif

#define TRANSFER_SCALE		(1.0f / TRANSFER_SCALE_VAL)
#define INVERSE_TRANSFER_SCALE	(TRANSFER_SCALE_VAL)
#define MAX_COMPRESSED_TRANSFER_INDEX	( BIT( 12 ) - 1 )

typedef struct
{
	uint	size : 12;
	uint	index : 20;
} transfer_index_t;

#ifdef HALF_FLOAT_TRANSFERS
typedef half		transfer_data_t;
#else
typedef unsigned short	transfer_data_t;
#endif

#define PATCH_OUTSIDE	BIT( 0 )
#define PATCH_EMITLIGHT	BIT( 1 )		// is the emit_surface patch
#define PATCH_SOURCE	BIT( 2 )		// non subdivided patch
#define PATCH_CHILD		BIT( 3 )		// subdivised from parent patch

#define MAX_PATCHES		(65536 * 4)	// probably enough

typedef struct patch_s
{
	struct patch_s	*next;			// next in face
	vec3_t		origin;
	vec_t		area;
	vec_t		exposure;
	vec_t		fade;
	winding_t	*winding;
	vec_t		scale;			// scaling of texture in s & t
	byte		flags;
	vec_t		chop;			// smallest acceptable width of patch face
	int			leafnum;
	int			faceNumber;
	word		modelnum;
	word		lightnum;
	vec_t		emitter_range;		// Range from patch origin (cached info calculated from winding)
	byte		emitter_skylevel;		// The "skylevel" used for sampling of normals, when the receiver patch is
						// within the range of ACCURATEBOUNCE_THRESHOLD * this->radius.
	// input
	byte		emitstyle;		// for switchable texlights
	vec3_t		baselight;		// emissivity only, uses emitstyle
	vec3_t		reflectivity;		// Average RGB of texture, modified by material type.

	// light transport
	uint		iIndex;
	uint		iData;

	transfer_index_t	*tIndex;
	transfer_data_t		*tData;

	vec_t		trans_sum;

	// output
	byte		totalstyle[MAXLIGHTMAPS];	// gives the styles for use by the new switchable totallight values
	vec3_t		totallight[MAXLIGHTMAPS];	// accumulated by radiosity does NOT include light accounted for by direct lighting
	vec3_t		directlight[MAXLIGHTMAPS];	// direct light only
	vec3_t		samplelight[MAXLIGHTMAPS];
	vec_t		samples[MAXLIGHTMAPS];	// for averaging direct light
#ifdef HLRAD_DELUXEMAPPING
	vec3_t		totallight_dir[MAXLIGHTMAPS];
	vec3_t		directlight_dir[MAXLIGHTMAPS];
	vec3_t		samplelight_dir[MAXLIGHTMAPS];
#endif
} patch_t;

#ifdef HLRAD_SHRINK_MEMORY
typedef struct
{
	// in local luxel space
	bool		occluded;			// luxel was occluded
	int			surface;			// this sample can grow into another face
#ifdef HLRAD_DELUXEMAPPING
	hvec3_t		normal; 			// phong normal
#endif
	vec3_t		pos;				// in world units

	hvec3_t		light[MAXLIGHTMAPS];	// total lightvalue
#ifdef HLRAD_DELUXEMAPPING
	hvec3_t		deluxe[MAXLIGHTMAPS];	// direct lightnormal (ignoring occlusion)
#ifdef HLRAD_SHADOWMAPPING
	half		shadow[MAXLIGHTMAPS];	// shadow value (occlusion only)
#endif
#endif
} sample_t;
#else
typedef struct
{
	// in local luxel space
	bool		occluded;			// luxel was occluded
	int			surface;			// this sample can grow into another face
#ifdef HLRAD_DELUXEMAPPING
	vec3_t		normal; 			// phong normal
#endif
	vec3_t		pos;				// in world units

	vec3_t		light[MAXLIGHTMAPS];	// total lightvalue
#ifdef HLRAD_DELUXEMAPPING
	vec3_t		deluxe[MAXLIGHTMAPS];	// direct lightnormal (ignoring occlusion)
#ifdef HLRAD_SHADOWMAPPING
	vec_t		shadow[MAXLIGHTMAPS];	// shadow value (occlusion only)
#endif
#endif
} sample_t;
#endif

typedef struct
{
	unsigned short	numsamples;
	sample_t		*samples;
	vec3_t			average[MAXLIGHTMAPS];
	vec3_t			texlight[MAXLIGHTMAPS];
	short			width;
} facelight_t;

typedef struct
{
	vec3_t		diffuse[MAXLIGHTMAPS];
	vec3_t		average[MAXLIGHTMAPS];
	int			styles[MAXLIGHTMAPS];
	float		fraction;
	dface_t		*surf;
	bool		hitsky;
} lightpoint_t;

extern patch_t		*g_face_patches[MAX_MAP_FACES];
extern entity_t		*g_face_entity[MAX_MAP_FACES];
extern vec3_t		g_face_offset[MAX_MAP_FACES];		// for rotating bmodels
extern vec3_t		g_face_centroids[MAX_MAP_FACES];
extern faceneighbor_t	g_faceneighbor[MAX_MAP_FACES];
extern facelight_t		g_facelight[MAX_MAP_FACES];
extern vec_t		*g_skynormalsizes[SKYLEVELMAX+1];
extern int		g_numskynormals[SKYLEVELMAX+1];
extern vec3_t		*g_skynormals[SKYLEVELMAX+1];
extern int		g_overflowed_styles_onface[MAX_THREADS];
extern int		g_overflowed_styles_onpatch[MAX_THREADS];
extern int		g_direct_luxels[MAX_THREADS];
extern int		g_lighted_luxels[MAX_THREADS];
extern vec_t		g_anorms[NUMVERTEXNORMALS][3];
extern size_t		g_transfer_data_size[MAX_THREADS];
extern edgeshare_t		*g_edgeshare;
extern patch_t		*g_patches;
extern uint		g_num_patches;
extern vec3_t		g_reflectivity[MAX_MAP_TEXTURES];
//==============================================

//==============================================

extern bool		g_extra;
extern vec3_t	g_ambient;
extern bool		g_fastmode;
extern float	g_maxlight;
extern uint		g_numbounce;
extern vec_t	g_direct_scale;
extern vec_t	g_indirect_scale;
extern float	g_indirect_sun;
extern float	g_smoothing_threshold;
extern bool		g_lightbalance;
extern char		source[MAX_PATH];
extern bool		g_lerp_enabled;
extern bool		g_nomodelshadow;
extern vec_t	g_smoothvalue;
extern bool		g_drawsample;
extern bool		g_wadtextures;
extern bool		g_dirtmapping;
extern bool		g_onlylights;
extern int		g_numdlights;
extern uint		g_gammamode;
extern vec_t	g_gamma;
extern vec_t	g_blur;
extern size_t	g_compatibility_mode;

extern bool		g_hdrcompresslog;
extern bool		g_tonemap;
extern bool		g_accurate_trans;
extern bool		g_fastsky;
extern bool		g_nolerp;	//cause dm changed how g_lerp_enabled works
extern bool		g_envsky;
extern bool		g_solidsky;
extern bool		g_aa;
extern bool		g_delambert;
extern bool		g_worldspace;
extern bool		g_studiolegacy;
extern vec_t	g_scale;
extern rgbdata_t	*g_skytextures[6];
extern vec_t	g_lightprobeepsilon;
extern int		g_lightprobelevel;
extern directlight_t	*g_skylights[256];
extern int		g_numskylights;
extern uint		g_numstudiobounce;
extern bool		g_vertexblur;
extern int		g_studiogipasscounter;
extern vec3_t	*g_studioskynormals;
extern int		g_numstudioskynormals;
extern bool		g_noemissive;
extern int		g_skystyle;

//
// ambientcube.c
//
void ComputeLeafAmbientLighting( void );
bool R_GetDirectLightFromSurface( dface_t *surf, const vec3_t point, lightpoint_t *info, bool no_texlight = false );


//
// facepos.c
//
void TranslateWorldToTex( int facenum, matrix3x4 out );
void FindFacePositions( int facenum, int threadnum = -1 );
void FreeFacePositions( void );
bool FindNearestPosition( int facenum, const winding_t *w, vec_t s, vec_t t, vec3_t pos, vec_t *out_s, vec_t *out_t, vec_t *dist );
void CalcPositionsSize( void );

//
// dirtmap.c
//
void SetupDirt( void );
float GatherSampleDirt( int threadnum, int fn, const vec3_t pos, const vec3_t normal, entity_t *ignoreent );

//
// qrad.c
//
const dplane_t *GetPlaneFromFace( const dface_t *face );
const dplane_t *GetPlaneFromFace( const uint facenum );
dleaf_t *HuntForWorld( vec3_t point, const vec3_t plane_offset, const dplane_t *plane, int hunt_size, vec_t hunt_scale, vec_t hunt_offset );
vec_t CalcSightArea( const vec3_t receiver_origin, const vec3_t receiver_normal, const winding_t *emitter_winding, int skylevel );
void GetAlternateOrigin( const vec3_t pos, const vec3_t normal, const patch_t *patch, vec3_t origin );
vec_t BaseLightForFace( dface_t *f, vec3_t light, vec3_t reflectivity );
void InitWorldLightFromPatch( dworldlight_t *wl, patch_t *p );
void InitWorldLightFromDlight( directlight_t *dl, int leafnum );
int GetVisCache( int lastoffset, int offset, byte *pvs );
void SetDLightVis( directlight_t *dl, int leafnum );
void MergeDLightVis( directlight_t *dl, int leafnum );
winding_t	*WindingFromFace( const dface_t *f );
void CalcTransferSize( void );
void PairEdges( void );
void FreeSharedEdges( void );
void BuildFaceNeighbors( void );
void FreeFaceNeighbors( void );
void BuildDiffuseNormals( void );
void FreeDiffuseNormals( void );
void BuildFaceLights( int facenum, int threadnum = -1 );
void PrecompLightmapOffsets();
void FinalLightFace( int facenum, int threadnum = -1 );
bool PvsForOrigin( const vec3_t org, byte *pvs );
void CreateDirectLights (void);
void DeleteDirectLights (void);
vec_t PatchPlaneDist( patch_t *patch );
void GetPhongNormal( int facenum, const vec3_t spot, vec3_t phongnormal );
void GetPhongNormal2( int facenum, const vec3_t spot, vec3_t phongnormal );
void FreeFaceLights( void );
void ReduceLightmap( void );
void LoadEnvSkyTextures( void );
void FreeEnvSkyTextures( void );
void GetEnvSkyColor( const vec3_t dir, vec3_t color );
void GetSkyColor( const vec3_t dir, vec3_t color );
void ApplyLightPostprocess( vec3_t lb, const float minlight = 1.0f );
vec_t CalcFaceSolidAngle( dface_t *f, const vec3_t origin );

//
// lerp.c
//
extern void CreateTriangulations( int facenum, int threadnum );
extern void GetTriangulationPatches( int facenum, int *numpatches, const int **patches );
extern void InterpolateSampleLight( const vec3_t position, int surface, int numstyles, const int *styles, vec3_t *outs, vec3_t *outs_dir = NULL );
extern void FreeTriangulations( void );

//
// lightmap.c
//
void GatherSampleLight( int threadnum, int fn, const vec3_t pos, int leafnum, const vec3_t normal,
vec3_t *s_light, vec3_t *s_dir, vec_t *s_occ, byte *styles, byte *vislight, bool topatch, entity_t *ignoreent = NULL, const vec_t sky_visibility = 0.0f );
void TexelSpaceToWorld( const lightinfo_t *l, vec3_t world, const vec_t s, const vec_t t );
void WorldToTexelSpace( const lightinfo_t *l, const vec3_t world, vec_t &s, vec_t &t );
int ParseLightIntensity( const char *pLight, vec3_t intensity );
void TranslateWorldToTex( int facenum, matrix3x4 out );
void InitLightinfo( lightinfo_t *pl, int facenum );
vec_t *GetTotalLight( patch_t *patch, int style );
vec_t *GetDirectLight( patch_t *patch, int style );
vec_t *GetTotalDirection( patch_t *patch, int style );
void ScaleDirectLights( void );
void CreateFacelightDependencyList( void );
void FacePatchLights( int facenum, int threadnum = -1 );
void FreeFacelightDependencyList( void );
void CalcSampleSize( void );
void CalcLuxelsCount( void );

//
// vertexlight.c
//
void BuildVertexLights( void );
void VertexPatchLights( void );
void FinalLightVertex( void );
void VertexBlendGI( void );

//
// model_lightmaps.c
//
void BuildModelLightmaps( void );
void CalcModelSampleSize( void );
void ScaleModelDirectLights( void );
void PrecompModelLightmapOffsets( void );
void FinalModelLightFace( void );
void FreeModelFaceLights( void );
void ReduceModelLightmap( byte *oldlightdata, byte *olddeluxdata, byte *oldshadowdata );
void WriteModelLighting( void );

//
// alias.c
//
void LoadAlias( entity_t *ent, void *buffer, int fileLength, int flags );
void AliasGetBounds( entity_t *ent, vec3_t mins, vec3_t maxs );

//
// studio.c
//
void LoadStudio( entity_t *ent, void *buffer, int fileLength, int flags );
void StudioGetBounds( entity_t *ent, vec3_t mins, vec3_t maxs );

//
// textures.c
//
void TEX_LoadTextures( void );
miptex_t *GetTextureByMiptex( int miptex );
void TEX_FreeTextures( void );

//
// trace.c
//
void InitWorldTrace( void );
int TestLine( int threadnum, const vec3_t start, const vec3_t end, bool nomodels = false, entity_t *ignoreent = NULL );
void TestLine( int threadnum, const vec3_t start, const vec3_t stop, trace_t *trace, bool nomodels = true, entity_t *ignoreent = NULL );
void FreeWorldTrace( void );

dleaf_t *PointInLeaf( const vec3_t point );
dleaf_t *PointInLeaf2( const vec3_t point );
dvertex_t	*GetVertexByNumber( int vertexnum );
