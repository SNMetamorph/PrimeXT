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
#include <utllinkedlist.h>
#include <utlarray.h>

// "hand" variable values
#define RIGHT_HAND		0.0f
#define LEFT_HAND		1.0f
#define HIDE_WEAPON		2.0f

#define EVENT_CLIENT		5000	// less than this value it's a server-side studio events
#define DECAL_TRANSPARENT_THRESHOLD	230	// transparent decals draw with GL_MODULATE

#define MESH_GLOWSHELL		BIT( 0 )	// scaled mesh by normals
#define MESH_CHROME			BIT( 1 )	// using chrome texcoords instead of mesh texcoords
#define MESH_NOCOLORS		BIT( 2 )	// ignore color buffer
#define MESH_NOTEXCOORDS		BIT( 3 )	// ignore texcoords buffer
#define MESH_NONORMALS		BIT( 4 )	// ignore normals buffer
#define MESH_DRAWARRAY		BIT( 5 )	// using glDrawArrays instead of glBegin method
#define MESH_COLOR_LIGHTING		BIT( 6 )	// get colors from m_lightvalues
#define MESH_COLOR_ENTITY		BIT( 7 )	// get colors from pev->rendercolor
#define MESH_ALPHA_ENTITY		BIT( 8 )	// get alpha from pev->renderamt

enum
{
	DECAL_CLIP_MINUSU	= 0x1,
	DECAL_CLIP_MINUSV	= 0x2,
	DECAL_CLIP_PLUSU	= 0x4,
	DECAL_CLIP_PLUSV	= 0x8,
};

typedef enum
{
	STUDIO_PASS_NORMAL = 0,		// one pass. combine vertex lighting and diffuse texture
	STUDIO_PASS_SHADOW,			// shadow pass. store vertices only (an optional enable texture mask for STUDIO_NF_TRANSPARENT)
	STUDIO_PASS_AMBIENT,		// lighting pass. build ambient map without texture (vertex color only)
	STUDIO_PASS_LIGHT,			// lighting pass. draw projected light, attenuation texture and shadow map
	STUDIO_PASS_DIFFUSE,		// diffuse pass. combine lighting info with diffuse texture
	STUDIO_PASS_GLOWSHELL,		// special case for kRenderFxGlowShell
	STUDIO_PASS_FOG,			// fog pass. project fog texture onto mesh
} StudioPassMode;

struct CStudioLight
{
	Vector		lightVec;			// light vector
	Vector		lightColor;		// ambient light color

	Vector		blightVec[MAXSTUDIOBONES];	// ambient lightvectors per bone
	Vector		elightVec[MAX_ELIGHTS][MAXSTUDIOBONES];
	Vector		elightColor[MAX_ELIGHTS];	// ambient entity light colors
	int		numElights;
};

// used for build decal projection
struct DecalMesh_t
{
	int	firstvertex;
	int	numvertices;
};

struct DecalVertex_t
{
	mstudiomesh_t *GetMesh( studiohdr_t *pHdr )
	{
		if ((m_Body == 0xFFFF) || (m_Model == 0xFFFF) || (m_Mesh == 0xFFFF))
		{
			return NULL;
		}

		mstudiobodyparts_t *pBody = (mstudiobodyparts_t *)((byte *)pHdr + pHdr->bodypartindex) + m_Body;
		mstudiomodel_t *pModel = (mstudiomodel_t *)((byte *)pHdr + pBody->modelindex) + m_Model;
		return (mstudiomesh_t *)((byte *)pHdr + pModel->meshindex) + m_Mesh;
	}

	mstudiomodel_t *GetModel( studiohdr_t *pHdr )
	{
		if ((m_Body == 0xFFFF) || (m_Model == 0xFFFF))
		{
			return NULL;
		}

		mstudiobodyparts_t *pBody = (mstudiobodyparts_t *)((byte *)pHdr + pHdr->bodypartindex) + m_Body;
		return (mstudiomodel_t *)((byte *)pHdr + pBody->modelindex) + m_Model;
	}

	Vector	m_Position;
	Vector	m_Normal;
	Vector2D	m_TexCoord;

	word	m_MeshVertexIndex;	// index into the mesh's vertex list
	word	m_Body;
	word	m_Model;
	word	m_Mesh;
	byte	m_Bone;		// bone that transform this vertex
};

struct DecalClipState_t
{
	// Number of used vertices
	int		m_VertCount;

	// Indices into the clip verts array of the used vertices
	int		m_Indices[2][7];

	// Helps us avoid copying the m_Indices array by using double-buffering
	bool		m_Pass;

	// Add vertices we've started with and had to generate due to clipping
	int		m_ClipVertCount;
	DecalVertex_t	m_ClipVerts[16];

	// Union of the decal triangle clip flags above for each vert
	int		m_ClipFlags[16];
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
	virtual void Init( void );

	// public Interfaces
	virtual int StudioDrawModel( int flags );
	virtual int StudioDrawPlayer( int flags, struct entity_state_s *pplayer );
private:
	// Local interfaces

	// Look up animation data for sequence
	virtual mstudioanim_t *StudioGetAnim ( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc );

	// Extract bbox from current sequence
	virtual int StudioExtractBbox ( cl_entity_t *e, studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs );

	// Compute a full bounding box for current sequence
	virtual int StudioComputeBBox ( cl_entity_t *e, Vector bbox[8] );

	// Interpolate model position and angles and set up matrices
	virtual void StudioSetUpTransform ( void );

	// Set up model bone positions
	virtual void StudioSetupBones ( void );	

	// Find final attachment points
	virtual void StudioCalcAttachments ( void );
	
	// Save bone matrices and names
	virtual void StudioSaveBones( void );

	// Restore bone matrices and names
	virtual void StudioRestoreBones( void );

	// Merge cached bones with current bones for model
	virtual void StudioMergeBones ( model_t *m_pSubModel );

	// Determine interpolation fraction
	virtual float StudioEstimateInterpolant( void );

	// Determine current frame for rendering
	virtual float StudioEstimateFrame ( mstudioseqdesc_t *pseqdesc );

	// Apply special effects to transform matrix
	virtual void StudioFxTransform( cl_entity_t *ent, matrix3x4 &transform );

	// Spherical interpolation of bones
	virtual void StudioSlerpBones ( Vector4D q1[], Vector pos1[], Vector4D q2[], Vector pos2[], float s );

	// Compute bone adjustments ( bone controllers )
	virtual void StudioCalcBoneAdj ( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen );

	// Get bone quaternions
	virtual void StudioCalcBoneQuaterion ( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, Vector4D &q );

	// Get bone positions
	virtual void StudioCalcBonePosition ( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, Vector &pos );

	// Compute rotations
	virtual void StudioCalcRotations ( Vector pos[], Vector4D q[], mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f );

	// Compute chrome per bone
	virtual void StudioSetupChrome( float *pchrome, int bone, Vector normal );

	// Send bones and verts to renderer
	virtual void StudioRenderModel ( void );
          
	//calc bodies and get pointers to him
	virtual int StudioSetupModel ( int bodypart, void **ppbodypart, void **ppsubmodel );

	virtual int StudioCheckBBox( void );

	// Finalize rendering
	virtual void StudioRenderFinal( void );

	virtual void StudioSetRenderMode( const int rendermode );

	// Player specific data
	// Determine pitch and blending amounts for players
	virtual void StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int &pBlend, float &pPitch );

	// Estimate gait frame for player
	virtual void StudioEstimateGait( entity_state_t *pplayer );

	// Process movement of player
	virtual void StudioProcessGait( entity_state_t *pplayer );

	// Process studio client events
	virtual void StudioClientEvents( void );

	virtual void StudioDynamicLight( cl_entity_t *ent, alight_t *lightinfo );

	virtual void StudioEntityLight( alight_t *lightinfo );

	virtual void StudioSetupLighting( alight_t *lightinfo );

	virtual void StudioLighting( Vector &lv, int bone, int flags, Vector normal );

	virtual void StudioDrawPoints( void );

	virtual void StudioDrawPointsFog( void );

	virtual void StudioDrawMeshes( mstudiotexture_t *ptexture, short *pskinref, StudioPassMode drawPass );

	virtual void StudioDrawMesh( short *ptricmds, float s, float t, int nDrawFlags, int nFaceFlags );

	virtual void StudioFormatAttachment( Vector &point, bool bInverse );

	// Setup the rendermode, smooth model etc
	virtual void StudioSetupRenderer( int rendermode );

	// Restore renderer state
	virtual void StudioRestoreRenderer( void );

	// Debug drawing
	void StudioDrawHulls( int iHitbox );

	void StudioDrawAbsBBox( void );

	void StudioDrawBones( void );

	void StudioDrawAttachments( void );

	struct BoneCache_t
	{
		float	frame;		// product of StudioEstimateFrame, not a curstate.frame!
		short	sequence;
		byte	blending[2];
		byte	controller[4];		
		byte	mouthopen;
		matrix3x4	transform;	// cached transform because ent->angles\ent->origin doesn't contains interpolation info

		// special fields for player
		short	gaitsequence;
		float	gaitframe;

		bool	vertexCacheValid;	// set true if cache is valid
		short	numBones;		// totally used bones
		matrix3x4	*bones;		// immediately goes after struct
	};

	struct VertCache_t
	{
		byte	body; // check body for invalidate vertex cache
		Vector	*verts[MAXSTUDIOMODELS]; // transformed and cached vertices
		Vector	*norms[MAXSTUDIOMODELS]; // transformed and cached normals
		unsigned int cacheFlags; // set for each bodypart
	};

	struct StudioAttachment_t
	{
		Vector		dir;		// attachment dir
		Vector		pos;		// attachment pos
	};

	struct ModelInstance_t
	{
		cl_entity_t	*m_pEntity;

		// Need to store off the model. When it changes, we lose all instance data..
		model_t		*m_pModel;
		word		m_DecalHandle;
		int		m_DecalCount;	// just used as timestamp for calculate decal depth

		// attachments
		StudioAttachment_t	attachment[MAXSTUDIOATTACHMENTS];

		// NOTE: can add all data who linked with current entity
		// e.g. bonecache, lightcache, remap textures etc
		BoneCache_t	*cache;		// cache goes into engine memory. So we don't care about it
		VertCache_t	*vertcache;
	};

	struct Decal_t
	{
		int	m_IndexCount;
		int	m_VertexCount;

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

	struct DecalMaterial_t
	{
		int		decalTexture;
		DecalIndexList_t	m_Indices;
		DecalVertexList_t	m_Vertices;
		DecalList_t	m_Decals;
	};

	struct DecalModelList_t
	{
		word		m_FirstMaterial;
		DecalHistoryList_t	m_DecalHistory;
	};

	struct DecalVertexInfo_t
	{
		Vector2D	m_UV;
		word	m_VertexIndex;	// index into the DecalVertex_t list
		bool	m_FrontFacing;
		bool	m_InValidArea;
	};

	struct DecalBuildInfo_t
	{
		studiohdr_t*	m_pStudioHeader;
		mstudiomesh_t*	m_pMesh;
		DecalMesh_t*	m_pDecalMesh;
		DecalMaterial_t*	m_pDecalMaterial;
		float		m_Radius;
		DecalVertexInfo_t*	m_pVertexInfo;
		int		m_Body;
		int		m_Mesh;
		int		m_Model;
		word		m_FirstVertex;
		word		m_VertexCount;
		bool		m_UseClipVert;
	};

	// Stores all decals for a particular material and lod
	CUtlLinkedList< DecalMaterial_t, word >	m_DecalMaterial;

	// Stores all decal lists that have been made
	CUtlLinkedList< DecalModelList_t, word > m_DecalList;

	// keep model instances for each entity
	CUtlLinkedList< ModelInstance_t, word > m_ModelInstances;

	// decal stuff
	virtual bool ComputePoseToDecal( const Vector &vecStart, const Vector &vecEnd );
	virtual void AddDecalToModel( DecalBuildInfo_t& buildInfo );
	virtual void AddDecalToMesh( DecalBuildInfo_t& buildInfo );
	virtual void ProjectDecalOntoMesh( DecalBuildInfo_t& build );
	virtual bool IsFrontFacing( const Vector& norm, byte vertexBone );
	virtual bool TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, byte vertexBone, Vector2D& uv );
	virtual void AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 );
	virtual int ComputeClipFlags( Vector2D const& uv );
	virtual bool ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags );
	virtual void ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int meshIndex, DecalVertex_t& decalVertex );
	virtual void ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val );
	virtual int IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val );
	virtual word AddVertexToDecal( DecalBuildInfo_t& build, int meshIndex );
	virtual word AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert );
	virtual void AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState );
	virtual int GetDecalMaterial( DecalModelList_t& decalList, int decalTexture );
	virtual bool ShouldRetireDecal( DecalMaterial_t* pDecalMaterial, DecalHistoryList_t const& decalHistory );
	virtual int AddDecalToMaterialList( DecalMaterial_t* pMaterial );
	virtual void RetireDecal( DecalHistoryList_t& historyList );
	virtual void DestroyDecalList( word handle );
	virtual word CreateDecalList( void );
	virtual bool IsModelInstanceValid( word handle, bool skipDecals = false );
	virtual word CreateInstance( cl_entity_t *pEnt );
	virtual void DestroyInstance( word handle );
	virtual void DrawDecal( word handle, studiohdr_t *pStudioHdr );
	virtual void ComputeDecalTransform( DecalMaterial_t& decalMaterial );
	virtual void DrawDecalMaterial( DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr );

	virtual void CreateBoneCache( word handle );
	virtual void DestroyBoneCache( word handle );
	virtual bool CheckBoneCache( cl_entity_t *ent, float f );

	virtual void CreateVertexCache( word handle );
	virtual void DestroyVertexCache( word handle );
	virtual bool CheckVertexCache( cl_entity_t *ent );

	virtual bool StudioLightingIntersect( void );

	Vector			studio_mins, studio_maxs;
	float			studio_radius;

	// Client clock
	double			m_clTime;
	// Old Client clock
	double			m_clOldTime;			

	// Do interpolation?
	int			m_fDoInterp;			
	// Do gait estimation?
	int			m_fGaitEstimation;		

	// Current render frame #
	int			m_nFrameCount;

	bool			m_fDrawViewModel;
	float			m_flViewmodelFov;

	bool			m_fHasProjectionLighting;

	// Cvars that studio model code needs to reference
	cvar_t			*m_pCvarHiModels;	// Use high quality models?	
	cvar_t			*m_pCvarLerping;	// Use lerping for animation?
	cvar_t			*m_pCvarLambert;	// lambert value for model lighting
	cvar_t			*m_pCvarLighting;	// lighting mode
	cvar_t			*m_pCvarDrawViewModel;
	cvar_t			*m_pCvarHand;	// handness
	cvar_t			*m_pCvarViewmodelFov;
	cvar_t			*m_pCvarStudioCache;

	// The entity which we are currently rendering.
	cl_entity_t		*m_pCurrentEntity;		

	// The model for the entity being rendered
	model_t			*m_pRenderModel;

	// Player info for current player, if drawing a player
	player_info_t		*m_pPlayerInfo;

	// The index of the player being drawn
	int			m_nPlayerIndex;

	// Current model rendermode
	int			m_iRenderMode;

	// The player's gait movement
	float			m_flGaitMovement;

	// Pointer to header block for studio model data
	studiohdr_t		*m_pStudioHeader;
	
	// Pointers to current body part and submodel
	mstudiobodyparts_t 		*m_pBodyPart;
	mstudiomodel_t		*m_pSubModel;
	int			m_iBodyPartIndex;

	// Palette substition for top and bottom of model
	int			m_nTopColor;			
	int			m_nBottomColor;

	// set force flags (e.g. chrome)
	int			m_nForceFaceFlags;

	//
	// Sprite model used for drawing studio model chrome
	model_t			*m_pChromeSprite;

	CStudioLight		*m_pLightInfo;

	Vector			m_lightvalues[MAXSTUDIOVERTS];
	float			*m_pvlightvalues;

	int			m_chromeAge[MAXSTUDIOBONES];	// last time chrome vectors were updated
	float			m_chrome[MAXSTUDIOVERTS][2];	// texture coords for surface normals
	Vector			m_chromeRight[MAXSTUDIOBONES];// chrome vector "right" in bone reference frames
	Vector			m_chromeUp[MAXSTUDIOBONES];	// chrome vector "up" in bone reference frames

	Vector			*m_verts;
	Vector			*m_norms;
	Vector			g_verts[MAXSTUDIOVERTS];
	Vector			g_norms[MAXSTUDIOVERTS];
	Vector			m_arrayverts[MAXARRAYVERTS];
	Vector			m_arraynorms[MAXARRAYVERTS];
	Vector2D			m_arraycoord[MAXARRAYVERTS];
	byte			m_vertexbone[MAXARRAYVERTS];
	byte			m_arraycolor[MAXARRAYVERTS][4];
	unsigned int		m_arrayelems[MAXARRAYVERTS*6];
	unsigned int		m_nNumArrayVerts;
	unsigned int		m_nNumArrayElems;

	// Caching
	// Number of bones in bone cache
	int			m_nCachedBones; 
	// Names of cached bones
	char			m_nCachedBoneNames[MAXSTUDIOBONES][32];

	// Cached bone & light transformation matrices
	matrix3x4			m_rgCachedBoneTransform [MAXSTUDIOBONES];
	
	// Model render counters ( from engine )
	int			m_nStudioModelCount;
	int			m_nModelsDrawn;

	// Matrices
	// Model to world transformation
	matrix3x4			m_protationmatrix;	

	// Concatenated bone and light transforms
	matrix3x4			*m_pbonetransform;	// pointer to each individual array
	matrix3x4			m_pbonestransform[MAXSTUDIOBONES];
	matrix3x4			m_pdecaltransform[MAXSTUDIOBONES];

	// engine stuff (backend)
public:
	void DestroyAllModelInstances( void );

	void	StudioDecalShoot( const Vector &vecStart, const Vector &vecEnd, int decalTex, cl_entity_t *ent, int flags, modelstate_t *state );
	int	StudioDecalList( decallist_t *pList, int count, qboolean changelevel );
	void	StudioClearDecals( void );
	void	RemoveAllDecals( int entityIndex );

	// Draw generic studiomodel (player too)
	void	DrawStudioModelInternal( cl_entity_t *e, qboolean follow_entity );

	void	StudioGetAttachment( const cl_entity_t *ent, int iAttachment, Vector *pos, Vector *dir );

	// Process viewmodel events (at start the frame so muzzleflashes will be correct added)
	void	RunViewModelEvents( void );

	// Draw view model (at end the frame)
	void	DrawViewModel( void );

	int	CacheCount( void ) { return m_ModelInstances.Count(); }
};

extern CStudioModelRenderer g_StudioRenderer;

// implementation of drawing funcs
inline void R_DrawStudioModel( cl_entity_t *e ) { g_StudioRenderer.DrawStudioModelInternal( e, false ); }
inline void R_RunViewmodelEvents( void ) { g_StudioRenderer.RunViewModelEvents(); }
inline void R_DrawViewModel( void ) { g_StudioRenderer.DrawViewModel(); }
inline void R_StudioDecalShoot( int decalTexture, cl_entity_t *ent, const float *start, const float *pos, int flags, modelstate_t *state )
{
	g_StudioRenderer.StudioDecalShoot((float *)start, (float *)pos, decalTexture, ent, flags, state );
}

inline int R_CreateStudioDecalList( decallist_t *pList, int count, qboolean changelevel )
{
	return g_StudioRenderer.StudioDecalList( pList, count, changelevel );
}

inline void R_ClearStudioDecals( void )
{
	g_StudioRenderer.StudioClearDecals();
}

inline void R_StudioAttachmentPosDir( const cl_entity_t *ent, int num, Vector *pos, Vector *dir )
{
	g_StudioRenderer.StudioGetAttachment( ent, num, pos, dir );
}

inline Vector R_StudioAttachmentPos( const cl_entity_t *ent, int num )
{
	Vector pos = g_vecZero;

	g_StudioRenderer.StudioGetAttachment( ent, num, &pos, NULL );

	return pos;
}

inline Vector R_StudioAttachmentDir( const cl_entity_t *ent, int num )
{
	Vector dir = g_vecZero;

	g_StudioRenderer.StudioGetAttachment( ent, num, NULL, &dir );

	return dir;
}

#endif// R_STUDIO_H
