/*
gl_rpart.h - quake-like particles
this code written for Paranoia 2: Savior modification
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_RPART_H
#define GL_RPART_H

#include "randomrange.h"

#define MAX_PARTICLES		8192
#define MAX_PARTINFOS		256	// various types of part-system

// built-in particle-system flags
#define FPART_BOUNCE		(1<<0)	// makes a bouncy particle
#define FPART_FRICTION		(1<<1)
#define FPART_VERTEXLIGHT		(1<<2)	// give some ambient light for it
#define FPART_STRETCH		(1<<3)
#define FPART_UNDERWATER		(1<<4)
#define FPART_INSTANT		(1<<5)
#define FPART_ADDITIVE		(1<<6)
#define FPART_NOTWATER		(1<<7)	// don't spawn in water

class CQuakePart
{
public:
	Vector		m_vecOrigin;	// position for current frame
	Vector		m_vecLastOrg;	// position from previous frame

	Vector		m_vecVelocity;	// linear velocity
	Vector		m_vecAccel;
	Vector		m_vecColor;
	Vector		m_vecColorVelocity;
	float		m_flAlpha;
	float		m_flAlphaVelocity;
	float		m_flRadius;
	float		m_flRadiusVelocity;
	float		m_flLength;
	float		m_flLengthVelocity;
	float		m_flRotation;	// texture ROLL angle
	float		m_flBounceFactor;

	CQuakePart	*pNext;		// linked list
	int		m_hTexture;

	float		m_flTime;
	int		m_iFlags;

	bool		Evaluate( float gravity );
};

typedef enum
{
	NORMAL_IGNORE = 0,
	NORMAL_OFFSET,
	NORMAL_DIRECTION,
	NORMAL_OFS_DIR,
};

class CQuakePartInfo
{
public:
	char		m_szName[32];	// effect name

	struct model_s	*m_pSprite;	// sprite
	int		m_hTexture;	// tga texture

	RandomRange	offset[3];
	RandomRange	velocity[3];
	RandomRange	accel[3];
	RandomRange	color[3];
	RandomRange	colorVel[3];
	RandomRange	alpha;
	RandomRange	alphaVel;
	RandomRange	radius;
	RandomRange	radiusVel;
	RandomRange	length;
	RandomRange	lengthVel;
	RandomRange	rotation;
	RandomRange	bounce;
	RandomRange	frame;
	RandomRange	count;		// particle count

	int		normal;		// how to use normal
	int		flags;		// particle flags
};

class CQuakePartSystem
{
	CQuakePart	*m_pActiveParticles;
	CQuakePart	*m_pFreeParticles;
	CQuakePart	m_pParticles[MAX_PARTICLES];

	CQuakePartInfo	m_pPartInfo[MAX_PARTINFOS];
	int		m_iNumPartInfo;

	// private partsystem shaders
	int		m_hDefaultParticle;
	int		m_hSparks;
	int		m_hSmoke;
	int		m_hWaterSplash;

	cvar_t		*m_pAllowParticles;
	cvar_t		*m_pParticleLod;
public:
			CQuakePartSystem( void );
	virtual		~CQuakePartSystem( void );

	void		Clear( void );
	void		Update( void );
	void		FreeParticle( CQuakePart *pCur );
	CQuakePart	*AllocParticle( void );
	bool		AddParticle( CQuakePart *src, int texture = 0, int flags = 0 );
	void		ParsePartInfos( const char *filename );
	bool		ParsePartInfo( CQuakePartInfo *info, char *&pfile );
	bool		ParseRandomVector( char *&pfile, RandomRange out[3] );
	int		ParseParticleFlags( char *pfile );
	CQuakePartInfo	*FindPartInfo( const char *name );
	void		CreateEffect( const char *name, const Vector &origin, const Vector &normal );

	// example presets
	void		ExplosionParticles( const Vector &pos );
	void		BulletParticles( const Vector &org, const Vector &dir );
	void		BubbleParticles( const Vector &org, int count, float magnitude );
	void		SparkParticles( const Vector &org, const Vector &dir );
	void		RicochetSparks( const Vector &org, float scale );
	void		SmokeParticles( const Vector &pos, int count );
	void		GunSmoke( const Vector &pos, int count );
};

extern CQuakePartSystem	g_pParticles;

#endif//GL_RPART_H