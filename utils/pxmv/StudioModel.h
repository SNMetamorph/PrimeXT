/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#ifndef INCLUDED_STUDIOMODEL
#define INCLUDED_STUDIOMODEL

#ifndef byte
typedef unsigned char byte;
#endif // byte

#include "mathlib.h"
#include "studio.h"
#include "jigglebones.h"
#include <stringlib.h>
#include "bs_defs.h"
#include "ikcontext.h"
#include <array>

#define MAX_SEQBLENDS	4		// must be power of two
#define MASK_SEQBLENDS	(MAX_SEQBLENDS - 1)

// remapping info
#define PLATE_HUE_START	160
#define PLATE_HUE_END	191
#define SUIT_HUE_START	PLATE_HUE_END+1
#define SUIT_HUE_END	223

#define MAX_MUZZLEFLASHES	4	// per one frame
#define MASK_MUZZLEFLASHES	(MAX_MUZZLEFLASHES - 1)

#define CROSS_LENGTH	18.0f

#define MAX_EDITFIELDS	128

#define TYPE_ORIGIN		0
#define TYPE_BBOX		1
#define TYPE_CBOX		2
#define TYPE_EYEPOSITION	3
#define TYPE_ATTACHMENT	4
#define TYPE_HITBOX		5

#define RANDOM_LONG(MIN, MAX)	((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))
#define RANDOM_FLOAT(MIN,MAX)	(((float)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))

typedef struct
{
	float		blendtime;	// time to blend between current and previous sequence
	int		sequence;		// previous sequence number		
	byte		blending[2];	// blending values from previous sequence
	float		cycle;		// cycle where sequence was changed
	float		fadeout;
} blend_sequence_t;

typedef struct
{
	int		texture;
	float		rotate;
	vec3_t		origin;
	float		scale;
	float		time;
} muzzleflash_t;

typedef struct
{
	int		type;
	int		id;		// #id for mutilple count
	int		bone;		// name get
	int		hitgroup;		// hitbox uses
	vec3_t		origin;
	vec3_t		mins;		// bboxes only
	vec3_t		maxs;
} edit_field_t;

class CBaseBoneSetup : public CStudioBoneSetup
{
public:
	virtual void debugMsg( char *szFmt, ... );
	virtual mstudioanim_t *GetAnimSourceData( mstudioseqdesc_t *pseqdesc );
	virtual void debugLine( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest = false, float duration = 0.0f );
};

class StudioModel
{
public:
	studiohdr_t	*getStudioHeader () const { return m_pstudiohdr; }
	studiohdr_t	*getTextureHeader () const { return m_ptexturehdr; }
	studiohdr_t	*getAnimHeader (int i) const { return m_panimhdr[i]; }
	void		updateTimings( float flTime, float flFrametime ) { m_flTime = flTime; m_flFrameTime = flFrametime; }
	void		incrementFramecounter( void ) { m_iFramecounter++; };
	int		getNumBlendings( void );
	int		hasLocalBlending( void );
	float		getCurrentTime( void ) { return m_flTime; }

	void		UploadTexture( mstudiotexture_t *ptexture, byte *data, byte *pal, int name );
	void		PaletteHueReplace( byte *palSrc, int newHue, int start, int end );
	void		ComputeSkinMatrix( mstudioboneweight_t *boneweights, matrix3x4 &result );
	void		ComputeWeightColor( mstudioboneweight_t *boneweights, Vector &result );
	void		RemapTextures( void );
	void		FreeModel ();
	studiohdr_t	*LoadModel( char *modelname );
	bool		PostLoadModel ( char *modelname );
	bool		SaveModel ( char *modelname );
	void		DrawModel( bool bMirror = false );
	void		DrawModelUVMap();
	void		ConvertTexCoords( void );
	bool		AdvanceFrame( float dt );

	float		GetInterval( void );
	float		GetCycle( void );
	float		GetFrame( void );
	int		GetMaxFrame( void );
	int		SetFrame( int nFrame );
	void		ClientEvents( void );

	void		ExtractBbox( Vector &mins, Vector &maxs );

	void		SetOffset2D( float x, float y ) { offset_x = x, offset_y = y; }
	int		SetSequence( int iSequence );
	int		GetSequence( void );
	void		GetSequenceInfo( float *pflFrameRate, float *pflGroundSpeed );
	void		GetMovement( float &prevCycle, Vector &vecPos, Vector &vecAngles );
	float		GetDuration( int iSequence );
	float		GetDuration( void );
	void		SetBlendValue( int iBlender, int iValue );
	float		SetController( int iController, float flValue );

	int		LookupPoseParameter( char const *szName );
	float		SetPoseParameter( int iParameter, float flValue );
	float		SetPoseParameter( char const *szName, float flValue );
	float		GetPoseParameter( char const *szName );
	float		GetPoseParameter( int iParameter );
	bool 		GetPoseParameterRange( int iParameter, float *pflMin, float *pflMax );
	float*		GetPoseParameters();

	float		SetMouth( float flValue );
	float		SetBlending( int iBlender, float flValue );
	int		SetBodygroup( int iGroup, int iValue );
	int		SetSkin( int iValue );
	bool		SetEditType( int iType );
	bool		SetEditMode( int iMode );
	bool		AddEditField( int type, int id );
	void		ReadEditField( studiohdr_t *phdr, edit_field_t *ed );
	void		WriteEditField( studiohdr_t *phdr, edit_field_t *ed );
	void		UpdateEditFields( bool write_to_model );

	void		SetTopColor( int color );
	void		SetBottomColor( int color );

	void		scaleMeshes (float scale);
	void		scaleBones (float scale);

	void		centerView( bool reset );
	void		updateModel( void );
	void		editPosition( float step, int type );
	edit_field_t	*editField( void ) { return m_pedit; }
	const char	*getQCcode( void );

	float		m_prevGroundCycle;
private:
	// global settings
	float		m_flTime;
	float		m_flFrameTime;

	// entity settings
	int		m_sequence;	// sequence index
	float		m_dt;
	float		m_cycle;		// 0 to 1 animation playback index
	float		m_prevcycle;
	int		m_bodynum;	// bodypart selection	
	int		m_skinnum;	// skin group selection
	byte		m_controller[8];	// bone controllers
	byte		m_blending[2];	// animation blending
	byte		m_mouth;		// mouth position
	bool		m_owntexmodel;	// do we have a modelT.mdl ?
	float		offset_x, offset_y;
	bool		remap_textures;
	int		update_model;	// set to true if model was edited
	bool		sequence_reset;
	int		m_iFramecounter;

	// muzzle data
	muzzleflash_t	m_muzzleflash[MAX_MUZZLEFLASHES];
	int		m_current_muzzle;

	void		DrawSpriteQuad( const Vector &org, const Vector &right, const Vector &up, float scale );
	void		MuzzleFlash( int attachment, int type );
	void		RenderMuzzleFlash( muzzleflash_t *muzzle );

	// debug
	void		drawTransparentBox( Vector const &bbmin, Vector const &bbmax, const matrix3x4 &m, float const *color = NULL );
	void		drawTransform( matrix3x4 &m, float flLength = 4.0f );

	// internal data
	studiohdr_t	*m_pstudiohdr;
	mstudiomodel_t	*m_pmodel;

	studiohdr_t	*m_ptexturehdr;
	std::array<studiohdr_t*, 32> m_panimhdr;

	float		m_adj[MAXSTUDIOCONTROLLERS];

	float		m_poseparameter[MAXSTUDIOPOSEPARAM];

	Vector		m_pbonecolor[MAXSTUDIOBONES];		// animated is blue, procedural is green, jiggle is orange
	matrix3x4		m_pbonetransform[MAXSTUDIOBONES];	// bone transformation matrix
	matrix3x4		m_plocaltransform[MAXSTUDIOBONES];
	matrix3x4		m_pworldtransform[MAXSTUDIOBONES];

	blend_sequence_t	m_seqblend[MAX_SEQBLENDS];
	int		m_current_seqblend;

	CJiggleBones	*m_pJiggleBones;

	CIKContext	m_ik;

	edit_field_t	m_editfields[MAX_EDITFIELDS];
	int		m_numeditfields;
	edit_field_t	*m_pedit;

	void 		SetupTransform( bool bMirror = false );

	void		BlendSequence( Vector pos[], Vector4D q[], blend_sequence_t *seqblend );
	void		LoadLocalMatrix( int bone, mstudioboneinfo_t *boneinfo );
	void		SetUpBones( bool bMirror = false );

	void		DrawPoints( bool wireframe = false );
	void		DrawUVMapPoints();

	void		Lighting( Vector &lv, int bone, int flags, const Vector &normal );
	void		Chrome( Vector2D &chrome, int bone, const Vector &normal );

	void		SetupLighting( void );
	void		SetupModel( int bodypart );
};

extern StudioModel	g_studioModel;
extern CBaseBoneSetup g_boneSetup;

#endif // INCLUDED_STUDIOMODEL