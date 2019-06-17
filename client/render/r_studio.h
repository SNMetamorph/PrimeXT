/*
r_studio.h - studio model rendering
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef R_STUDIO_H
#define R_STUDIO_H

#include "r_const.h"
#include "pmtrace.h"
#include "studio.h"
#include "r_studiodecal.h"
#include <utllinkedlist.h>
#include <utlarray.h>
#include "jigglebones.h"
#include "bs_defs.h"
#include "ikcontext.h"

#define EVENT_CLIENT	5000	// less than this value it's a server-side studio events
#define MAX_MODEL_MESHES	(MAXSTUDIOBODYPARTS * MAXSTUDIOMODELS)

#define MAX_SEQBLENDS	8		// must be power of two
#define MASK_SEQBLENDS	(MAX_SEQBLENDS - 1)

#define MAXARRAYVERTS	65536

#define MF_STATIC_BOUNDS	BIT( 0 )	// this model is a env_static. don't recalc bounds every frame
#define MF_VERTEX_LIGHTING	BIT( 1 )	// model has the custom vertex lighting (loading from level)
#define MF_VL_BAD_CACHE	BIT( 2 )	// for some reasons this model can't be instanced with a vertex lighting (bad crc, mismatch vertexcount etc)
#define MF_INIT_SMOOTHSTAIRS	BIT( 3 )
#define MF_ATTACHMENTS_DONE	BIT( 4 )

#define QSORT_MAX_STACKDEPTH		(MAX_MODEL_MESHES)

#define AF_FORCE_RECALC	BIT( 0 )
#define AF_LOCAL_SPACE	BIT( 1 )

#define R_MeshCopy( in, out ) \
	( \
	( out ).mesh = ( in ).mesh, \
	( out ).hProgram = ( in ).hProgram, \
	( out ).parent = ( in ).parent, \
	( out ).model = ( in ).model, \
	( out ).additive = ( in ).additive \
	)

#define R_MeshCmp( mb1, mb2 ) \
	( \
	( mb1 ).additive > ( mb2 ).additive ? true : false \
	)

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
	float		frame;
	int		sequence;
	float		gaitframe;
	int		gaitsequence;

	// for smooth stair climbing
	float		stairtime;
	float		stairoldz;
} mstudiolerp_t;

typedef struct
{
	int		ambientlight;	// clip at 128
	int		shadelight;	// clip at 192 - ambientlight
	Vector		color;
	Vector		plightvec;
} mstudiolight_t;

// 52 bytes here
typedef struct xvert_s
{
	Vector		vertex;			// position
	Vector		normal;			// normal
	short		stcoord[2];		// ST texture coords (half-float)
	char		boneid[4];		// control bones		
	byte		weight[4];		// boneweights
	float		light[MAXLIGHTMAPS];	// packed color + unused entry (used for static vertex lighting)
} svert_t;

#pragma pack(1)
// no boneweights, no vertexlight, 26 bytes
typedef struct
{
	Vector		vertex;			// position
	Vector		normal;			// normal
	short		stcoord[2];		// ST texture coords
	char		boneid[4];		// control bones
} svert_v0_t;

// single bone, has vertex lighting, 30 bytes
typedef struct
{
	Vector		vertex;			// position
	Vector		normal;			// normal
	short		stcoord[2];		// ST texture coords
	float		light[MAXLIGHTMAPS];	// packed color
} svert_v1_t;

// have boneweights, no vertexlight, 30 bytes
typedef struct
{
	Vector		vertex;			// position
	Vector		normal;			// normal
	short		stcoord[2];		// ST texture coords
	char		boneid[4];		// control bones
	byte		weight[4];		// boneweights
} svert_v2_t;

// includes all posible combination, slowest, 38 bytes here
typedef struct
{
	Vector		vertex;			// position
	Vector		normal;			// normal
	short		stcoord[2];		// ST texture coords
	char		boneid[4];		// control bones		
	byte		weight[4];		// boneweights
	float		light[MAXLIGHTMAPS];	// packed color
} svert_v3_t;
#pragma pack()

// 14 bytes here
typedef struct
{
	struct vbomesh_s	*mesh;
	unsigned short	hProgram;		// handle to glsl program
	cl_entity_t	*parent;		// pointer to parent entity
	model_t		*model;
	bool		additive;		// additive mesh
} gl_studiomesh_t;

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

	// public Interfaces
	virtual int StudioDrawModel( int flags );

	// Look up animation data for sequence
	mstudioanim_t *StudioGetAnim ( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc );

	// precache vertexlit mesh
	void CreateMeshCacheVL( const char *modelname, int cacheID );

	// throw all the meshes when the engine is shutting down
	void FreeMeshCacheVL( void );
private:
	// Local interfaces

	// Extract bbox from current sequence
	virtual int StudioExtractBbox ( cl_entity_t *e, studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs );

	// Compute a full bounding box for current sequence
	virtual int StudioComputeBBox ( cl_entity_t *e );

	float CalcStairSmoothValue( float oldz, float newz, float smoothtime, float smoothvalue );

	// Interpolate model position and angles and set up matrices
	virtual void StudioSetUpTransform ( void );

	// Set up model bone positions
	virtual void StudioSetupBones ( void );

	// external simulation for bones
	virtual void StudioCalcBonesProcedural( Vector pos[], Vector4D q[] );

	// Find final attachment points
	virtual void StudioCalcAttachments ( matrix3x4 bones[], int local_space );

	virtual void AddBlendSequence( int oldseq, int newseq, float prevframe, bool gaitseq = false );

	virtual void BlendSequence( Vector pos[], Vector4D q[], mstudioblendseq_t *pseqblend );

	virtual void UpdateIKLocks( CIKContext *pIK );

	virtual void CalculateIKLocks( CIKContext *pIK );
	
	// Merge cached bones with current bones for model
	virtual void StudioMergeBones ( matrix3x4 bones[], matrix3x4 cached_bones[], model_t *pModel, model_t *pParentModel );

	// Determine interpolation fraction
	virtual float StudioEstimateInterpolant( void );

	// Determine current gaitframe for rendering
	virtual float StudioEstimateGaitFrame ( mstudioseqdesc_t *pseqdesc );

	// Determine current frame for rendering
	virtual float StudioEstimateFrame ( mstudioseqdesc_t *pseqdesc );

	virtual void StudioInterpolateBlends( cl_entity_t *e, float dadt );

	virtual void StudioInterpolatePoseParams( cl_entity_t *e, float dadt );

	// Apply special effects to transform matrix
	virtual void StudioFxTransform( cl_entity_t *ent, matrix3x4 &transform );

	virtual void ComputeSkinMatrix( mstudioboneweight_t *boneweights, const matrix3x4 worldtransform[], matrix3x4 &result );

	// Compute chrome per bone
	virtual void StudioSetupChrome( float *pchrome, int bone, const Vector &normal );

	// Send bones and verts to renderer
	virtual void StudioRenderModel ( void );
          
	//calc bodies and get pointers to him
	virtual int StudioSetupModel ( int bodypart, void **ppbodypart, void **ppsubmodel );

	virtual void StudioSetRenderMode( const int rendermode );

	// Player specific data
	// Determine pitch and blending amounts for players
	virtual void StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int &pBlend, float &pPitch );

	// Process studio client events
	virtual void StudioClientEvents( void );

	virtual void StudioLighting( float *lv, int bone, int flags, const Vector &normal );

	virtual void StudioDrawPoints( void );

	inline void StudioDrawMeshChrome( short *ptricmds, float s, float t, float scale );

	virtual void StudioStaticLight( cl_entity_t *ent );

	virtual void StudioFormatAttachment( Vector &point );

	virtual word ChooseStudioProgram( studiohdr_t *phdr, mstudiomaterial_t *mat, bool lightpass );

	virtual void AddMeshToDrawList( studiohdr_t *phdr, const vbomesh_t *mesh, bool lightpass, bool solidPass );

	virtual void AddBodyPartToDrawList( studiohdr_t *phdr, mbodypart_t *bodyparts, int bodypart, bool lightpass, bool solid );

	inline void DrawMeshFromBuffer( const vbomesh_t *mesh );

	inline void QSortStudioMeshes( gl_studiomesh_t *meshes, int Li, int Ri );

	// Setup the rendermode, smooth model etc
	virtual void StudioSetupRenderer( int rendermode );

	// Restore renderer state
	virtual void StudioRestoreRenderer( void );

	// Debug drawing
	void StudioDrawHulls( void );

	void StudioDrawAbsBBox( void );

	void StudioDrawBones( void );

	void StudioDrawAttachments( void );

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
		Vector		angles;		// attachment angle
	};

	struct ModelInstance_t
	{
		cl_entity_t	*m_pEntity;	// upcast helper

		// Need to store off the model. When it changes, we lose all instance data..
		model_t		*m_pModel;
		word		m_DecalHandle;
		int		m_DecalCount;	// just used as timestamp for calculate decal depth
		int		info_flags;

		mvbocache_t	*m_VlCache;	// valid only for env_statics that have vertexlight data
		byte		styles[4];	// actual only if MF_VERTEX_LIGHTING bit is set

		mstudiolight_t	lighting;

		// bounds info
		Vector		bbox[8];
		Vector		absmin;
		Vector		absmax;
		float		radius;

		BoneCache_t	bonecache;	// just compare to avoid recalc bones every frame

		// attachments
		StudioAttachment_t	attachment[MAXSTUDIOATTACHMENTS];
		int		numattachments;

		byte		m_controller[MAXSTUDIOCONTROLLERS];
		float		m_poseparameter[MAXSTUDIOPOSEPARAM];	// blends for traditional studio models
		mstudioblendseq_t	m_seqblend[MAX_SEQBLENDS];
		int		m_current_seqblend;

		// sequence blends stuff
		mstudiolerp_t	lerp;

		CJiggleBones	*m_pJiggleBones;
		CIKContext	m_ik;

		mstudiomaterial_t	*materials;			// shaders cache
		matrix3x4		m_protationmatrix;
		matrix3x4		m_plightmatrix;			// light transform
		matrix3x4		m_pbones[MAXSTUDIOBONES];		// bone to pose
		matrix3x4		m_pwpnbones[MAXSTUDIOBONES];

		Radian		m_procangles[MAXSTUDIOBONES];
		Vector		m_procorigin[MAXSTUDIOBONES];
		bool		m_bProceduralBones;
		float		m_flLastBoneUpdate;

		// GLSL cached arrays
		Vector4D		m_studioquat[MAXSTUDIOBONES];
		Vector		m_studiopos[MAXSTUDIOBONES];
		Vector4D		m_weaponquat[MAXSTUDIOBONES];
		Vector		m_weaponpos[MAXSTUDIOBONES];

		int		cached_frame;			// to avoid compute bones more than once per frame
		int		visframe;				// model is visible this frame
	};

	struct Decal_t
	{
		int		m_IndexCount;
		int		m_VertexCount;

		// used for decal serialize
		modelstate_t	state;
		int		depth;
		Vector		vecLocalStart;
		Vector		vecLocalEnd;
		byte		flags;		// decal shared flags
	};

	struct DecalHistory_t
	{
		word	m_Material;
		word	m_Decal;
	};

	typedef CUtlLinkedList<DecalVertex_t, word> DecalVertexList_t;
	typedef CUtlArray<word> DecalIndexList_t;
	typedef CUtlLinkedList<Decal_t, word> DecalList_t;
	typedef CUtlLinkedList<DecalHistory_t, word> DecalHistoryList_t;
	typedef CUtlArray<int> CIntVector;

	struct DecalMaterial_t
	{
		word		hProgram;		// cached shader
		int		decalTexture;
		int		modelTexture;	// studio diffuse
		int		flags;		// mesh flags
		DecalIndexList_t	m_Indices;
		DecalVertexList_t	m_Vertices;
		DecalList_t	m_Decals;
	};

	struct DecalModelList_t
	{
		word		m_FirstMaterial;
		DecalHistoryList_t	m_DecalHistory;
	};

	struct DecalBuildInfo_t
	{
		// this part is constant all time while decal is build
		int			m_iDecalMaterial;
		int			m_iDecalTexture;
		float			m_Radius;
		modelstate_t		*modelState;
		dmodellight_t		*modelLight;
		int			decalDepth;
		Vector			vecLocalStart;
		Vector			vecLocalEnd;
		byte			decalFlags;

		// this part is may be potentially changed for each mesh
		mstudiomesh_t*		m_pMesh;
		DecalMesh_t*		m_pDecalMesh;
		DecalMaterial_t*		m_pDecalMaterial;
		DecalVertexInfo_t*		m_pVertexInfo;
		int			m_Body;
		int			m_Mesh;
		int			m_Model;
		word			m_FirstVertex;
		word			m_VertexCount;
		int			m_PrevIndexCount;
		bool			m_UseClipVert;
	};

	// Stores all decals for a particular material and lod
	CUtlLinkedList< DecalMaterial_t, word >	m_DecalMaterial;

	// Stores all decal lists that have been made
	CUtlLinkedList< DecalModelList_t, word > m_DecalList;

	// keep model instances for each entity
	CUtlLinkedList< ModelInstance_t, word > m_ModelInstances;

	// decal stuff
	bool ComputePoseToDecal( const Vector &vecStart, const Vector &vecEnd );
	void AddDecalToModel( DecalBuildInfo_t& buildInfo );
	void AddDecalToMesh( DecalBuildInfo_t& buildInfo );
	void ProjectDecalOntoMesh( DecalBuildInfo_t& build );
	bool IsFrontFacing( const Vector& norm, mstudioboneweight_t *boneWeight );
	bool TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, mstudioboneweight_t *boneWeight, Vector2D& uv );
	matrix3x3 GetDecalRotateTransform( byte vertexBone );
	void AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 );
	int ComputeClipFlags( Vector2D const& uv );
	bool ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags );
	void ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int meshIndex, DecalVertex_t& decalVertex );
	void ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val );
	int IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val );
	word AddVertexToDecal( DecalBuildInfo_t& build, int meshIndex );
	word AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert );
	void AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState );
	int GetDecalMaterial( DecalModelList_t& decalList, int decalTexture, mstudiomaterial_t *mat, bool create = false );
	bool ShouldRetireDecal( DecalMaterial_t* pDecalMaterial, DecalHistoryList_t const& decalHistory );
	void CreateDecalForMaterial( DecalBuildInfo_t& buildInfo );
	int AddDecalToMaterialList( DecalMaterial_t* pMaterial );
	void RetireDecal( DecalHistoryList_t& historyList );
	void DestroyDecalList( word handle );
	word CreateDecalList( void );
	bool StudioSetEntity( cl_entity_t *pEnt );
	bool StudioSetEntity( gl_studiomesh_t *entry );
	virtual bool IsModelInstanceValid( ModelInstance_t *inst );
	bool StudioSetupInstance( void );
	virtual void DestroyInstance( word handle );
	void DrawDecal( cl_entity_t *e );
	void ComputeDecalTransform( DecalMaterial_t& decalMaterial, const matrix3x4 bones[] );
	void DrawDecalMaterial( DecalMaterial_t& decalMaterial, const matrix3x4 bones[] );

	virtual void DrawStudioMeshes( void );
	virtual void DrawStudioMeshesShadow( void );
	virtual void StudioDrawShell( void );
	virtual int StudioCheckLOD( void );

	// glow shell stuff
	virtual void StudioBuildNormalTable( void );
	virtual void StudioGenerateNormals( void );

	virtual bool CheckBoneCache( float f );
	virtual void RenderDynLightList( void );
	virtual void AddStudioToLightList( struct plight_s *pl );
	virtual void BuildMeshListForLight( struct plight_s *pl );
	virtual void DrawLightForMeshList( struct plight_s *pl );

	virtual mvbocache_t	*CreateMeshCache( dmodellight_t *dml = NULL );
	virtual void ReleaseVBOCache( mvbocache_t **ppcache );
	virtual void DestroyMeshCache( void );

	virtual void CreateMeshCacheVL( dmodellight_t *dml, int cacheID );

	virtual void LoadStudioMaterials( void );
	virtual void FreeStudioMaterials( void );

	virtual void UpdateInstanceMaterials( void );
	virtual void ClearInstanceData( bool create );

	void MeshCreateBuffer( vbomesh_t *pDst, const mstudiomesh_t *pSrc, const mstudiomodel_t *pSubModel, const matrix3x4 bones[], dmodellight_t *dml = NULL );
	void UploadBufferBase( vbomesh_t *pOut, svert_t *arrayxvert );
	void UploadBufferVLight( vbomesh_t *pOut, svert_t *arrayxvert );
	void UploadBufferWeight( vbomesh_t *pOut, svert_t *arrayxvert );
	void UploadBufferGeneric( vbomesh_t *pOut, svert_t *arrayxvert, bool vertex_light );

	// Do interpolation?
	int			m_fDoInterp;			
	bool			m_fShootDecal;
	bool			m_fDrawViewModel;
	float			m_flViewmodelFov;

	// Cvars that studio model code needs to reference
	cvar_t			*m_pCvarHiModels;	// Use high quality models?	
	cvar_t			*m_pCvarDrawViewModel;
	cvar_t			*m_pCvarHand;	// handness
	cvar_t			*m_pCvarViewmodelFov;
	cvar_t			*m_pCvarGlowShellFreq;
	cvar_t			*m_pCvarCompatible;
	cvar_t			*m_pCvarLodScale;
	cvar_t			*m_pCvarLodBias;

	CBaseBoneSetup		m_boneSetup;

	// The entity which we are currently rendering.
	cl_entity_t		*m_pCurrentEntity;		

	// The model for the entity being rendered
	model_t			*m_pRenderModel;

	ModelInstance_t		*m_pModelInstance;

	// Player info for current player, if drawing a player
	player_info_t		*m_pPlayerInfo;

	// The index of the player being drawn
	int			m_nPlayerIndex;

	// Pointer to header block for studio model data
	studiohdr_t		*m_pStudioHeader;
	
	// Pointers to current body part and submodel
	mstudiobodyparts_t 		*m_pBodyPart;
	mstudiomodel_t		*m_pSubModel;

	// Palette substition for top and bottom of model
	int			m_nTopColor;			
	int			m_nBottomColor;

	//
	// Sprite model used for drawing studio model chrome
	model_t			*m_pChromeSprite;

	Vector			m_chromeOrigin;
	int			m_chromeAge[MAXSTUDIOBONES];	// last time chrome vectors were updated
	float			m_chrome[MAXSTUDIOVERTS][2];	// texture coords for surface normals
	Vector			m_chromeRight[MAXSTUDIOBONES];// chrome vector "right" in bone reference frames
	Vector			m_chromeUp[MAXSTUDIOBONES];	// chrome vector "up" in bone reference frames
	int			m_normaltable[MAXSTUDIOVERTS];// glowshell stuff
	int			m_chromeCount;

	// decal building stuff
	matrix3x4			m_pdecaltransform[MAXSTUDIOBONES];
	DecalVertex_t		m_decalverts[MAXSTUDIOVERTS];
	matrix3x4			m_pworldtransform[MAXSTUDIOBONES];		// decal transform
	Vector			m_bonelightvecs[MAXSTUDIOBONES];		// used for decal lighting

	// intermediate arrays	
	Vector			m_verts[MAXSTUDIOVERTS];
	Vector			m_norms[MAXSTUDIOVERTS];
	Vector			m_arrayverts[MAXARRAYVERTS];
	Vector4D			m_arraycoord[MAXARRAYVERTS];
	byte			m_arraycolor[MAXARRAYVERTS][4];
	unsigned int		m_arrayelems[MAXARRAYVERTS*6];
	int			m_nNumArrayVerts;
	int			m_nNumArrayElems;
	int			m_nNumLightVerts;

	gl_studiomesh_t		m_DrawMeshes[MAX_MODEL_MESHES];
	int			m_nNumDrawMeshes;

	gl_studiomesh_t		m_LightMeshes[MAX_MODEL_MESHES];
	int			m_nNumLightMeshes;

	// engine stuff (backend)
public:
	void	DestroyAllModelInstances( void );

	void	StudioDecalShoot( const Vector &vecStart, const Vector &vecEnd, int decalTex, cl_entity_t *ent, int flags, modelstate_t *state );
	int	StudioDecalList( decallist_t *pList, int count );
	void	StudioClearDecals( void );
	void	RemoveAllDecals( int entityIndex );

	void	ProcessUserData( model_t *mod, qboolean create, const byte *buffer );

	void	LoadLocalMatrix( int bone, mstudioboneinfo_t *boneinfo );

	int	GetEntityRenderMode( cl_entity_t *ent );

	void	StudioSetBonesExternal( const cl_entity_t *ent, const Vector pos[], const Radian ang[] );

	// Draw generic studiomodel (player too)
	void	DrawStudioModelInternal( cl_entity_t *e );

	void	StudioGetAttachment( const cl_entity_t *ent, int iAttachment, Vector *pos, Vector *dir, int flags = 0 );

	int	StudioGetAttachmentNumber( const cl_entity_t *ent, const char *attachment );

	float	StudioSequenceDuration( const cl_entity_t *ent, int sequence );

	// Process viewmodel events (at start the frame so muzzleflashes will be correct added)
	void	RunViewModelEvents( void );

	// Draw view model (at end the frame)
	void	DrawViewModel( void );

	int	CacheCount( void ) { return m_ModelInstances.Count(); }

	void	ClearLightCache( void );
};

extern CStudioModelRenderer g_StudioRenderer;

// implementation of drawing funcs
inline void R_DrawStudioModel( cl_entity_t *e ) { g_StudioRenderer.DrawStudioModelInternal( e ); }
inline void R_RunViewmodelEvents( void ) { g_StudioRenderer.RunViewModelEvents(); }
inline void R_DrawViewModel( void ) { g_StudioRenderer.DrawViewModel(); }
inline void R_StudioDecalShoot( int decalTexture, cl_entity_t *ent, const float *start, const float *pos, int flags, modelstate_t *state )
{
	g_StudioRenderer.StudioDecalShoot((float *)start, (float *)pos, decalTexture, ent, flags, state );
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

inline int R_GetEntityRenderMode( cl_entity_t *ent )
{
	return g_StudioRenderer.GetEntityRenderMode( ent );
}

inline void R_StudioAttachmentTransform( const cl_entity_t *ent, int num, Vector *origin, Vector *angles, int flags = 0 )
{
	g_StudioRenderer.StudioGetAttachment( ent, num, origin, angles, flags );
}

inline Vector R_StudioAttachmentOrigin( const cl_entity_t *ent, int num, int flags = 0 )
{
	Vector origin = g_vecZero;

	g_StudioRenderer.StudioGetAttachment( ent, num, &origin, NULL, flags );

	return origin;
}

inline Vector R_StudioAttachmentAngles( const cl_entity_t *ent, int num, int flags = 0 )
{
	Vector angles = g_vecZero;

	g_StudioRenderer.StudioGetAttachment( ent, num, NULL, &angles, flags );

	return angles;
}

inline int R_StudioLookupAttachment( const cl_entity_t *ent, const char *name )
{
	return g_StudioRenderer.StudioGetAttachmentNumber( ent, name );
}

inline void R_StudioClearLightCache( void )
{
	g_StudioRenderer.ClearLightCache();
}

inline void R_StudioSetBonesExternal( const cl_entity_t *ent, const Vector pos[], const Radian ang[] )
{
	g_StudioRenderer.StudioSetBonesExternal( ent, pos, ang );
}

inline float R_StudioSequenceDuration( const cl_entity_t *ent, int sequence )
{
	return g_StudioRenderer.StudioSequenceDuration( ent, sequence );
}

#endif// R_STUDIO_H
