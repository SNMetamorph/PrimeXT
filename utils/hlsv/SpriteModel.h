/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#ifndef INCLUDED_SPRITEMODEL
#define INCLUDED_SPRITEMODEL

#include "mathlib.h"

/*
==============================================================================

SPRITE MODELS

Alias models are position independent, so the cache manager can move them.
==============================================================================
*/

#include "sprite.h"

typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float		up, down, left, right;
	int		gl_texturenum;
} mspriteframe_t;

typedef struct
{
	int		numframes;
	float		*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	frametype_t	type;
	mspriteframe_t	*frameptr;
} mspriteframedesc_t;

typedef struct
{
	short		type;
	short		texFormat;
	int		maxwidth;
	int		maxheight;
	int		numframes;
	int		radius;
	int		facecull;
	int		synctype;
	mspriteframedesc_t	frames[1];
} msprite_t;

class SpriteModel
{
public:
	msprite_t		*getSpriteHeader () const { return m_pspritehdr; }
	void		updateTimings( float flTime, float flFrametime ) { m_flTime = flTime; m_flFrameTime = flFrametime; }
	float		getCurrentTime( void ) { return m_flTime; }
	void		UploadTexture( byte *data, int width, int height, byte *palette, int name, int size = 768, bool has_alpha = false );
	int		GetNumFrames( void ) { return m_pspritehdr ? m_pspritehdr->numframes : 0; }
	void		FreeSprite( void );
	msprite_t		*LoadSprite( const char *modelname );
	bool		SaveSprite( const char *modelname );
	void		DrawSprite( void );
	int		setFrame( int newframe );

	void		ExtractBbox( Vector &mins, Vector &maxs );
	void		GetMovement( Vector &delta );

	void		centerView( bool reset );
	void		updateSprite( void );
private:
	// global settings
	float		m_flTime;
	float		m_flFrameTime;

	// entity settings
	byte		m_prevblending[2];
	float		m_sequencetime;
	float		m_yaw;

	float		m_flLerpfrac;	// lerp frames
	float		m_dt;
	int		m_frame;

	matrix4x4		m_protationmatrix;
	matrix4x4		m_viewVectors;

	// internal data
	msprite_t		*m_pspritehdr;	// pointer to sourcemodel
	int		m_iFileSize;	// real size of model footprint

	byte		m_palette[1024];
	vec3_t		m_spritemins;
	vec3_t		m_spritemaxs;
	int		m_loadframe;

	void 		SetupTransform( void );
	bool		AllowLerping( void );

	// loading stuff
	dframetype_t	*LoadSpriteFrame( void *pin, mspriteframe_t **ppframe );
	dframetype_t	*LoadSpriteGroup( void *pin, mspriteframe_t **ppframe );

	// frame compute
	mspriteframe_t	*GetSpriteFrame( int frame );
	float		GetSpriteFrameInterpolant( int frame, mspriteframe_t **old, mspriteframe_t **cur );

	// draw
	void		DrawQuad( mspriteframe_t *frame, const vec3_t &org, const vec3_t &v_right, const vec3_t &v_up, float scale );
};

extern SpriteModel	g_spriteModel;
extern byte	palette_q1[];

#endif