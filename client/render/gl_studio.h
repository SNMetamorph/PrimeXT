/*
gl_studio.h - studio model rendering
this code written for Paranoia 2: Savior modification
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_STUDIO_H
#define GL_STUDIO_H

#include "pmtrace.h"
#include "studio.h"
#include <utllinkedlist.h>
#include <utlarray.h>
#include <stringlib.h>
#include "gl_decals.h"
#include "gl_studiodecal.h"
#include "gl_debug.h"
#include "gl_cubemap.h"
#include "trace.h"
#include "bs_defs.h"
#include "ikcontext.h"
#include "jigglebones.h"
#include "tbnfile.h"
#include "studio_material.h"

#define EVENT_CLIENT	5000		// less than this value it's a server-side studio events
#define MAX_MODEL_MESHES	(MAXSTUDIOBODYPARTS * MAXSTUDIOMODELS)
#define SHADE_LAMBERT	1.495f
#define MAXARRAYVERTS	320000		// max vertices per studio submodel

#define MAX_SEQBLENDS	8		// must be power of two
#define MASK_SEQBLENDS	(MAX_SEQBLENDS - 1)

#define MF_STATIC_BOUNDS	BIT( 0 )	// this model is a env_static. don't recalc bounds every frame
#define MF_CUSTOM_LIGHTGRID	BIT( 1 )	// model has special attahaments that was accepted as light probe start points
#define MF_VERTEX_LIGHTING	BIT( 2 )	// model has the custom vertex lighting (loading from level)
#define MF_SURFACE_LIGHTING	BIT( 3 )	// model has the custom surface lighting (loading from level)
#define MF_VL_BAD_CACHE		BIT( 4 )	// for some reasons this model can't be instanced with a vertex lighting (bad crc, mismatch vertexcount etc)
#define MF_INIT_SMOOTHSTAIRS	BIT( 5 )
#define MF_ATTACHMENTS_DONE	BIT( 6 )
#define MF_POSITION_CHANGED	BIT( 7 )

// attachment flags
#define AF_FORCE_RECALC	BIT( 0 )
#define AF_LOCAL_SPACE	BIT( 1 )

// studiorenderer modes
#define DRAWSTUDIO_NORMAL	0	// as default
#define DRAWSTUDIO_VIEWMODEL	1	// rendeging viewmodel
#define DRAWSTUDIO_RUNEVENTS	2	// run events of studiomodel

#define LIGHTSTATIC_NONE	0
#define LIGHTSTATIC_VERTEX	1
#define LIGHTSTATIC_SURFACE	2

#define TBNSTATE_INACTIVE	0
#define TBNSTATE_LOADING	1
#define TBNSTATE_GENERATE	2

// holds temporary data
typedef struct
{
	int		firstvertex;
	int		numvertices;
	int		firstindex;
	int		numindices;
	int		lightmapnum;
} StudioMesh_t;

// new blending sequence system
typedef struct
{
	float		blendtime;	// time to blend between current and previous sequence
	int		sequence;		// previous sequence number		
	float		cycle;		// cycle where sequence was changed
	float		fadeout;
	bool		gaitseq;
} mstudioblendseq_t;

typedef struct
{
	float	frame;
	int		sequence;
	float	gaitframe;
	int		gaitsequence;
	float	lastgaitframe;
	float	prevgaitframe;

	// for smooth stair climbing
	float		stairtime;
	float		stairoldz;
} mstudiolerp_t;

// source vertex data (106 bytes here)
typedef struct xvert_s
{
	Vector		vertex;			// position
	Vector		normal;			// normal
	Vector		tangent;			// tangent
	Vector		binormal;			// binormal
	float		stcoord[4];		// ST texture coords
	float		lmcoord0[4];		// LM texture coords
	float		lmcoord1[4];		// LM texture coords
	char		boneid[4];		// control bones		
	byte		weight[4];		// boneweights
	float		light[MAXLIGHTMAPS];	// packed color
	float		deluxe[MAXLIGHTMAPS];	// packed lightdir
	byte		styles[MAXLIGHTMAPS];	// lightstyles
	word		m_MeshVertexIndex;		// index into the mesh's vertex list (decals only)
} svert_t;

typedef void (*pfnCreateStudioBuffer)( vbomesh_t *pOut, svert_t *arrayxvert );
typedef void (*pfnBindStudioBuffer)( vbomesh_t *pOut, int attrFlags );

typedef struct
{
	pfnCreateStudioBuffer	CreateBuffer;
	pfnBindStudioBuffer		BindBuffer;
	const char*		BufferName;	// debug
} mesh_loader_t;

class CBaseBoneSetup : public CStudioBoneSetup
{
public:
	virtual void debugMsg( char *szFmt, ... );
	virtual mstudioanim_t *GetAnimSourceData( mstudioseqdesc_t *pseqdesc );
	virtual void debugLine( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest = false, float duration = 0.0f );
};

/*
====================
CStudioModelRenderer

====================
*/
class CStudioModelRenderer
{
public:
	// Construction/Destruction
	CStudioModelRenderer( void );
	virtual ~CStudioModelRenderer( void );

	// Initialization
	void Init( void );

	void VidInit( void );

	// Look up animation data for sequence
	mstudioanim_t *StudioGetAnim ( model_t *pModel, mstudioseqdesc_t *pseqdesc );

	// precache vertexlit mesh
	void CreateStudioCacheVL( const char *modelname, int cacheID );

	// precache lightmapped mesh
	void CreateStudioCacheFL( const char *modelname, int cacheID );

	// throw all the meshes when the engine is shutting down
	void FreeStudioCacheVL( void );
	void FreeStudioCacheFL( void );
private:
	// Local interfaces

	// Extract bbox from current sequence
	int StudioExtractBbox ( studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs );

	// Compute a full bounding box for current sequence
	int StudioComputeBBox ( void );

	float CalcStairSmoothValue( float oldz, float newz, float smoothtime, float smoothvalue );

	// Returns current entity valid origin/angles
	const Vector StudioGetOrigin();
	const Vector StudioGetAngles();

	// Interpolate model position and angles and set up matrices
	void StudioSetUpTransform( void );

	// Set up model bone positions
	void StudioSetupBones( void );	

	// External simulation for bones
	void StudioCalcBonesExternal( Vector pos[], Vector4D q[] );

	// Find final attachment points
	void StudioCalcAttachments( matrix3x4 bones[] );

	void AddBlendSequence( int oldseq, int newseq, float prevframe, bool gaitseq = false );

	void BlendSequence( Vector pos[], Vector4D q[], mstudioblendseq_t *pseqblend );

	void UpdateIKLocks( CIKContext *pIK );

	void CalculateIKLocks( CIKContext *pIK );
	
	// Merge cached bones with current bones for model
	void StudioMergeBones( matrix3x4 &transform, matrix3x4 bones[], matrix3x4 cached_bones[], model_t *pModel, model_t *pParentModel );

	// Determine interpolation fraction
	float StudioEstimateInterpolant();

	// Determine current gaitframe for rendering
	float StudioEstimateGaitFrame ( mstudioseqdesc_t *pseqdesc );

	// Determine current frame for rendering
	float StudioEstimateFrame ( mstudioseqdesc_t *pseqdesc );

	void StudioInterpolateControllers( cl_entity_t *e, float dadt );

	void StudioInterpolatePoseParams( cl_entity_t *e, float dadt );

	// Apply special effects to transform matrix
	void StudioFxTransform( cl_entity_t *ent, matrix3x4 &transform );

	void ComputeSkinMatrix( mstudioboneweight_t *boneweights, const matrix3x4 worldtransform[], matrix3x4 &result );

	void ComputeSkinMatrix( svert_t *vertex, const matrix3x4 worldtransform[], matrix3x4 &result );

	int StudioCheckLOD( void );

	//calc bodies and get pointers to him
	int StudioSetupModel ( int bodypart, mstudiomodel_t **ppsubmodel, msubmodel_t **ppvbomodel );

	void DrawMeshFromBuffer( const vbomesh_t *mesh );

	// Process studio client events
	void StudioClientEvents( void );

	void StudioLighting( float *lv, int bone, int flags, const Vector &normal );

	void StudioStaticLight( cl_entity_t *ent, mstudiolight_t *light );
	void CacheVertexLight( cl_entity_t *ent );
	void CacheSurfaceLight( cl_entity_t *ent );
	void StudioFormatAttachment( Vector &point );

	word ChooseStudioProgram( studiohdr_t *phdr, mstudiomaterial_t *mat, bool lightpass );

	void AddMeshToDrawList( studiohdr_t *phdr, vbomesh_t *mesh, bool lightpass );

	void AddBodyPartToDrawList( studiohdr_t *phdr, mbodypart_t *bodyparts, int bodypart, bool lightpass );

	bool CheckBoneCache( float f );

	word ShaderSceneForward( mstudiomaterial_t *mat, int lightmode, bool bone_weighting, int numbones );
	word ShaderLightForward( CDynLight *dl, mstudiomaterial_t *mat, bool bone_weighting, int numbones );
	word ShaderSceneDepth( mstudiomaterial_t *mat, bool bone_weighting, int numbones );
	word ShaderDecalForward( studiodecal_t *pDecal, bool has_vertexlight );

	// Debug drawing
	void StudioDrawDebug( cl_entity_t *e );

	void StudioDrawHulls( int iHitbox = -1 );

	void StudioDrawAbsBBox( void );

	void StudioDrawBones( void );

	void StudioDrawAttachments( bool bCustomFov );

	void StudioDrawBodyPartsBBox();

	// intermediate structure. Used only for build unique submodels
	struct TmpModel_t
	{
		char		name[64];
		mstudiomodel_t	*pmodel;
		msubmodel_t	*pout;
	};

	struct BoneCache_t
	{
		float		frame;		// product of StudioEstimateFrame, not a curstate.frame!
		short		sequence;
		byte		blending[2];
		byte		controller[4];		
		byte		mouthopen;
		matrix3x4		transform;	// cached transform because ent->angles\ent->origin doesn't contains interpolation info
		float		poseparam[MAXSTUDIOPOSEPARAM];

		// special fields for player
		short		gaitsequence;
		float		gaitframe;
	};

	struct StudioAttachment_t
	{
		char		name[MAXSTUDIONAME];
		matrix3x4		local;		// local position
		Vector		origin;		// attachment pos
		Vector		angles;		// VectorAngles
		Vector		dir;		// old method
	};

	typedef struct
	{
		int		m_VertCount;		// Number of used vertices
		int		m_Indices[2][7];		// Indices into the clip verts array of the used vertices
		bool		m_Pass;			// Helps us avoid copying the m_Indices array by using double-buffering
		int		m_ClipVertCount;		// Add vertices we've started with and had to generate due to clipping
		svert_t		m_ClipVerts[16];
		int		m_ClipFlags[16];		// Union of the decal triangle clip flags above for each vert
	} DecalClipState_t;

	typedef CUtlArray<studiodecal_t> StudioDecalList_t;
	typedef CUtlArray<int> CIntVector;

	enum
	{
		MESHLOADER_BASE = 0,
		MESHLOADER_BASEBUMP,
		MESHLOADER_VLIGHT, // TODO remove it because usused?
		MESHLOADER_VLIGHTBUMP,
		MESHLOADER_WEIGHT,
		MESHLOADER_WEIGHTBUMP,
		MESHLOADER_LIGHTMAP,
		MESHLOADER_LIGHTMAPBUMP,
		MESHLOADER_GENERIC,
		MESHLOADER_COUNT,
	};

	struct ModelInstance_t
	{
		cl_entity_t		*m_pEntity;

		// Need to store off the model. When it changes, we lose all instance data..
		model_t			*m_pModel;
		StudioDecalList_t		m_DecalList;	// new decal list for each instance
		int			m_DecalCount;	// just used as timestamp for calculate decal depth
		int			info_flags;

		// NOTE: each decal applied while model has some pose
		// keep it in this array for each model and dump into
		// savefile to properly restored decal positions
		CUtlArray<modelstate_t>	pose_stamps;

		mstudiocache_t		*m_VlCache;	// valid only for env_statics that have vertexlight data
		mstudiocache_t		*m_FlCache;	// valid only for env_statics that have surfacelight data
		byte			styles[MAXLIGHTMAPS];// actual only if MF_VERTEX_LIGHTING|MF_SURFACE_LIGHTING bit is set

		// bounds info
		Vector			absmin;
		Vector			absmax;
		float			radius;
		Vector			origin;		// transformed origin

		BoneCache_t		bonecache;	// just compare to avoid recalc bones every frame

		// attachments
		StudioAttachment_t		attachment[MAXSTUDIOATTACHMENTS];
		int			numattachments;

		byte			m_controller[MAXSTUDIOCONTROLLERS];
		float			m_poseparameter[MAXSTUDIOPOSEPARAM];	// blends for traditional studio models
		float			m_oldposeparams[MAXSTUDIOPOSEPARAM];	// latched values
		mstudioblendseq_t		m_seqblend[MAX_SEQBLENDS];
		int			m_current_seqblend;

		// sequence blends stuff
		mstudiolerp_t		lerp;

		CJiggleBones		*m_pJiggleBones;
		CIKContext		m_ik;
		// attached cubemaps
		mcubemap_t		*cubemap[2];
		float			lerpFactor;

		mstudiomaterial_t		*materials;	// shaders cache

		// light interpolation
		mstudiolight_t		oldlight;
		mstudiolight_t		newlight;
		mstudiolight_t		light;		// cached light values

		float			lighttimecheck;
		bool			light_update;
		byte			lights[MAXDYNLIGHTS]; // dynamic lights cache

		matrix3x4			m_protationmatrix;
		matrix3x4			m_pbones[MAXSTUDIOBONES];	// bone to pose
		matrix3x4			m_pwpnbones[MAXSTUDIOBONES];
		Vector4D			m_glstudiobones[MAXSTUDIOBONES*3];	// GLSL-friendly compacted matrix4x3
		Vector4D			m_glweaponbones[MAXSTUDIOBONES*3];	// GLSL-friendly compacted matrix4x3

		// external bones data
		Radian		m_externalBonesAngles[MAXSTUDIOBONES];
		Vector		m_externalBonesOrigin[MAXSTUDIOBONES];
		bool		m_bExternalBones;
		float		m_flLastExternalBonesUpdate;

		// GLSL cached arrays
		Vector4D			m_studioquat[MAXSTUDIOBONES];
		Vector			m_studiopos[MAXSTUDIOBONES];
		Vector4D			m_weaponquat[MAXSTUDIOBONES];
		Vector			m_weaponpos[MAXSTUDIOBONES];
		GLfloat			m_glmatrix[16];

		unsigned int		cached_frame;			// to avoid compute bones more than once per frame
		unsigned int		visframe;				// model is visible this frame
	};

	struct DecalBuildInfo_t
	{
		// this part is constant all time while decal is build
		const DecalGroupEntry*	m_pTexInfo;
		modelstate_t*		modelState;
		dmodelvertlight_t*		modelLight;
		Vector			vecLocalNormal;
		Vector			vecLocalEnd;
		Vector2D			vecDecalScale;
		bool			m_UseClipVert;
		int			decalDepth;
		byte			decalFlags;
		int			poseState;
		studiodecal_t		*current;
		float			m_Radius;

		// this part is changed for each mesh
		vbomesh_t*		m_pModelMesh;
		StudioMesh_t*		m_pDecalMesh;
		dvertlight_t*		m_pVertexLight;
		DecalVertexInfo_t*		m_pVertexInfo;
		CUtlArray<svert_t>		m_Vertices;
		CUtlArray<unsigned int>	m_Indices;
	};

	// keep model instances for each entity
	CUtlLinkedList< ModelInstance_t, word > m_ModelInstances;

	// decal stuff
	bool ComputePoseToDecal( const Vector &vecStart, const Vector &vecEnd );
	void AddDecalToModel( DecalBuildInfo_t& buildInfo );
	void AddDecalToMesh( DecalBuildInfo_t& buildInfo );
	void AllocDecalForMesh( DecalBuildInfo_t& build );
	void ProjectDecalOntoMesh( DecalBuildInfo_t& build );
	bool IsFrontFacing( const svert_t *vert );
	void DecalCreateBuffer( DecalBuildInfo_t& build, studiodecal_t *pDecal );
	bool TransformToDecalSpace( DecalBuildInfo_t& build, const svert_t *vert, Vector2D& uv );
	matrix3x3 GetDecalRotateTransform( byte vertexBone );
	void AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 );
	int ComputeClipFlags( Vector2D const& uv );
	bool ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags );
	void ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int vertIndex, svert_t *out );
	void ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val );
	int IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val );
	word AddVertexToDecal( DecalBuildInfo_t& build, int vertIndex );
	word AddVertexToDecal( DecalBuildInfo_t& build, svert_t *vert );
	void AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState );
	void ComputeDecalTBN( DecalBuildInfo_t& build );
	void PurgeDecals( ModelInstance_t *inst );
	void PurgeDecals( cl_entity_t *pEnt );
	bool StudioSetEntity( cl_entity_t *pEnt );
	bool StudioSetEntity( CSolidEntry *entry );
	bool IsModelInstanceValid( ModelInstance_t *inst );
	bool StudioSetupInstance( void );
	void DestroyInstance( word handle );
	void SetDecalUniforms( studiodecal_t *pDecal );
	void DrawDecal( CSolidEntry *entry, GLenum cull = GL_FRONT );

	mstudiocache_t *CreateStudioCache( void *dml = NULL, int lightmode = LIGHTSTATIC_NONE );
	void DeleteStudioCache( mstudiocache_t **ppcache );
	void DestroyMeshCache( void );

	void CreateStudioCacheVL( dmodelvertlight_t *dml, int cacheID );
	void CreateStudioCacheFL( dmodelfacelight_t *dml, int cacheID );
	void PrecacheStudioShaders( void );
	void LoadStudioMaterials( void );
	void FreeStudioMaterials( void );

	void UpdateInstanceMaterials( void );
	void ClearInstanceData( bool create );

	// set uniforms data for specified shader
	void DrawSingleMesh( CSolidEntry *mesh, bool force, bool specialPass );

	void SetupSubmodelVerts( const mstudiomodel_t *pSubModel, const matrix3x4 bones[], void *dml, int lightmode );
	void MeshCreateBuffer( vbomesh_t *pDst, const mstudiomesh_t *pSrc, const StudioMesh_t *pMeshInfo, int lightmode );
	void AllocLightmapsForMesh( StudioMesh_t *pCurMesh, const dmodelfacelight_t *dfl );
	bool CalcLightmapAxis( mstudiosurface_t *surf, const dfacelight_t *fl, const dmodelfacelight_t *dfl );
	static void CreateBufferBaseGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferBaseGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferBaseBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferBaseBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferVLightGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferVLightGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferVLightBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferVLightBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferWeightGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferWeightGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferWeightBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferWeightBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferLightMapGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferLightMapGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferLightMapBumpGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferLightMapBumpGL30( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferGenericGL21( vbomesh_t *pOut, svert_t *arrayxvert );
	static void CreateBufferGenericGL30( vbomesh_t *pOut, svert_t *arrayxvert );

	static void BindBufferBaseGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferBaseGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferBaseBumpGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferBaseBumpGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferVLightGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferVLightGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferVLightBumpGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferVLightBumpGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferWeightGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferWeightGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferWeightBumpGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferWeightBumpGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferLightMapGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferLightMapGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferLightMapBumpGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferLightMapBumpGL30( vbomesh_t *pOut, int attrFlags );
	static void BindBufferGenericGL21( vbomesh_t *pOut, int attrFlags );
	static void BindBufferGenericGL30( vbomesh_t *pOut, int attrFlags );

	static void CreateIndexBuffer( vbomesh_t *pOut, unsigned int *arrayelems );
	static void BindIndexBuffer( vbomesh_t *pOut );

	static int SortSolidMeshes( const CSolidEntry *a, const CSolidEntry *b );

	unsigned int ComputeAttribFlags( int numbones, bool has_bumpmap, bool has_boneweights, bool has_vertexlight, bool has_lightmap );
	unsigned int SelectMeshLoader( int numbones, bool has_bumpmap, bool has_boneweights, bool has_vertexlight, bool has_lightmap );

	static mesh_loader_t	m_pfnMeshLoaderGL21[MESHLOADER_COUNT];
	static mesh_loader_t	m_pfnMeshLoaderGL30[MESHLOADER_COUNT];

	bool			m_fShootDecal;	// disable all interpolations and bonecache
	float			m_flViewmodelFov;	// custom fov for viewmodel
	int			m_iDrawModelType;	// various modes of rendering

	// Cvars that studio model code needs to reference
	cvar_t			*m_pCvarHiModels;	// Use high quality models?	
	cvar_t			*m_pCvarDrawViewModel;
	cvar_t			*m_pCvarHand;	// handness
	cvar_t			*m_pCvarViewmodelFov;
	cvar_t			*m_pCvarCompatible;
	cvar_t			*m_pCvarLodScale;
	cvar_t			*m_pCvarLodBias;

	CBaseBoneSetup		m_boneSetup;

	// current mesh material
	mstudiomaterial_t		*m_pCurrentMaterial;

	ModelInstance_t		*m_pModelInstance;

	// Pointer to header block for studio model data
	studiohdr_t		*m_pStudioHeader;
	mstudiocache_t		*m_pStudioCache;			// only valid while mesh is build

	msubmodel_t 		*m_pVboModel;
	mstudiomodel_t		*m_pSubModel;
	StudioMesh_t		m_pTempMesh[MAXSTUDIOSKINS];		// temp structure

	// used by PushEntity\PopEntity
	entity_state_t		m_savestate;
	Vector			m_saveorigin;
	Vector			m_saveangles;

	Vector			m_bonelightvecs[MAXSTUDIOBONES];	// debug used this
	Vector			m_arrayverts[MAXARRAYVERTS];
	svert_t			m_arrayxvert[MAXARRAYVERTS];
	dmodeltbn_t		*m_tbnverts;			// variable sized
	unsigned int		m_arrayelems[MAXARRAYVERTS*3];
	unsigned int		m_nNumArrayVerts;
	unsigned int		m_nNumArrayElems;
	unsigned int		m_nNumLightVerts;
	unsigned int		m_nNumLightFaces;
	unsigned int		m_nNumTBNVerts;
	unsigned int		m_nNumTempVerts;			// used to conversion to fan sequence

	// decal building stuff
	matrix3x4			m_pdecaltransform[MAXSTUDIOBONES];	// decal->world
	matrix3x4			m_pworldtransform[MAXSTUDIOBONES];	// world->decal

	// firstperson legs stuff
	int			m_iTBNState;
public:
	void	DestroyAllModelInstances( void );

	int	StudioGetBounds( cl_entity_t *e, Vector bounds[2] );
	int	StudioGetBounds( CSolidEntry *entry, Vector bounds[2] );
	int	StudioGetBounds( CSolidEntry *entry, CBoundingBox &bounds );
	CBoundingBox StudioGetMeshBounds( ModelInstance_t *inst, const vbomesh_t *mesh );
	bool	StudioLoadTBN( void );
	bool	StudioSaveTBN( void );

	void	PushEntityState( cl_entity_t *ent );
	void	PopEntityState( cl_entity_t *ent );
	void	EntityToModelState( modelstate_t *state, const cl_entity_t *ent );
	void	ModelStateToEntity( cl_entity_t *ent, const modelstate_t *state );
	void	StudioDecalShoot( const Vector &vecNorm, const Vector &vecEnd, const char *name, cl_entity_t *ent, int flags, modelstate_t *state );
	void	StudioDecalShoot( struct pmtrace_s *tr, const char *name, bool visent = false );
	int		StudioDecalList( decallist_t *pList, int count );
	void	StudioClearDecals( void );
	void	RemoveAllDecals( int entityIndex );

	void	UpdateLatchedVars( cl_entity_t *e, qboolean reset );

	void	ProcessUserData( model_t *mod, qboolean create, const byte *buffer );

	void	LoadLocalMatrix( int bone, mstudioboneinfo_t *boneinfo );

	void	AddStudioModelToDrawList( cl_entity_t *e, bool update = false );

	void	StudioGetAttachment(const cl_entity_t *ent, int iAttachment, Vector *pos, Vector *ang, Vector *dir);

	int		StudioGetAttachmentNumber(const cl_entity_t *ent, const char *attachment);

	void	StudioSetBonesExternal(const cl_entity_t *ent, const Vector pos[], const Radian ang[]);

	float	StudioSequenceDuration(const cl_entity_t *ent, int sequence);

	// Process viewmodel events (at start the frame so muzzleflashes will be correct added)
	void	RunViewModelEvents( void );

	bool	ComputeCustomFov( matrix4x4 &projMatrix, matrix4x4 &worldViewProjMatrix );

	void	RestoreNormalFov( matrix4x4 &projMatrix, matrix4x4 &worldViewProjMatrix );

	// Draw view model (at end the frame)
	void	DrawViewModel( void );

	void	RenderSolidStudioList( void );

	void	RenderShadowStudioList( void );

	void	RenderDebugStudioList( bool bViewModel );

	void	RenderDynLightList( bool solid );

	void	RenderTransMesh( CTransEntry *entry );

	void	BuildMeshListForLight( CDynLight *pl, bool solid );

	void	DrawLightForMeshList( CDynLight *pl );

	int	CacheCount( void ) { return m_ModelInstances.Count(); }

	void	ClearLightCache( void );
};

extern CStudioModelRenderer g_StudioRenderer;

// implementation of drawing funcs
inline void R_RunViewmodelEvents( void ) { g_StudioRenderer.RunViewModelEvents(); }
inline void R_DrawViewModel( void ) 
{ 
	GL_DEBUG_SCOPE();
	ZoneScoped;
	g_StudioRenderer.DrawViewModel(); 
}
inline void R_ProcessStudioData( model_t *mod, qboolean create, const byte *buffer )
{
	if( mod->type == mod_studio )
		g_StudioRenderer.ProcessUserData( mod, create, buffer );
}

inline int R_CreateStudioDecalList( decallist_t *pList, int count )
{
	return g_StudioRenderer.StudioDecalList( pList, count );
}

inline void R_ClearStudioDecals( void )
{
	g_StudioRenderer.StudioClearDecals();
}

inline int R_StudioGetBounds( cl_entity_t *e, Vector bounds[2] )
{
	return g_StudioRenderer.StudioGetBounds( e, bounds );
}

inline int R_StudioGetBounds( CSolidEntry *entry, Vector bounds[2] )
{
	return g_StudioRenderer.StudioGetBounds( entry, bounds );
}

inline void R_RenderSolidStudioList( void )
{
	ZoneScoped;
	GL_DEBUG_SCOPE();
	g_StudioRenderer.RenderSolidStudioList();
}

inline void R_RenderTransMesh( CTransEntry *entry )
{
	g_StudioRenderer.RenderTransMesh( entry );
}

inline void R_RenderLightForTransMeshes( void )
{
	GL_DEBUG_SCOPE();
	g_StudioRenderer.RenderDynLightList( false );
}

inline void R_RenderShadowStudioList( void )
{
	g_StudioRenderer.RenderShadowStudioList();
}

inline void R_RenderDebugStudioList( bool bViewModel )
{
	ZoneScoped;
	GL_DEBUG_SCOPE();
	g_StudioRenderer.RenderDebugStudioList( bViewModel );
}

inline void R_AddStudioToDrawList( cl_entity_t *e, bool update = false )
{
	g_StudioRenderer.AddStudioModelToDrawList( e, update );
}

inline void R_StudioClearLightCache( void )
{
	g_StudioRenderer.ClearLightCache();
}

inline void R_StudioAttachmentPosAng( const cl_entity_t *ent, int num, Vector *pos, Vector *ang )
{
	g_StudioRenderer.StudioGetAttachment( ent, num, pos, ang, NULL );
}

inline void R_StudioAttachmentPosDir( const cl_entity_t *ent, int num, Vector *pos, Vector *dir )
{
	g_StudioRenderer.StudioGetAttachment( ent, num, pos, NULL, dir );
}

inline Vector R_StudioAttachmentPos( const cl_entity_t *ent, int num )
{
	Vector pos = g_vecZero;

	g_StudioRenderer.StudioGetAttachment( ent, num, &pos, NULL, NULL );

	return pos;
}

inline Vector R_StudioAttachmentAng( const cl_entity_t *ent, int num )
{
	Vector ang = g_vecZero;

	g_StudioRenderer.StudioGetAttachment( ent, num, NULL, &ang, NULL );

	return ang;
}

inline float R_StudioSequenceDuration(const cl_entity_t *ent, int sequence)
{
	return g_StudioRenderer.StudioSequenceDuration(ent, sequence);
}

inline int R_StudioLookupAttachment(const cl_entity_t *ent, const char *name)
{
	return g_StudioRenderer.StudioGetAttachmentNumber(ent, name);
}

inline void R_UpdateLatchedVars( cl_entity_t *e, qboolean reset )
{
	g_StudioRenderer.UpdateLatchedVars( e, reset );
}

inline void R_StudioSetBonesExternal( const cl_entity_t *ent, const Vector pos[], const Radian ang[] )
{
	g_StudioRenderer.StudioSetBonesExternal( ent, pos, ang );
}

#endif// GL_STUDIO_H