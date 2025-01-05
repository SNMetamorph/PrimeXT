/*
r_particle.h - Laurie Cheers Aurora Particle System
First implementation of 02/08/02 November235
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

#ifndef GL_PARTICLE_H
#define GL_PARTICLE_H

class CParticleType;
class CParticleSystem;

#include "randomrange.h"

struct CParticle
{
	CParticle		*nextpart;
	CParticle		*m_pOverlay;	// for making multi-layered particles
	CParticleType	*pType;

	Vector		origin;
	Vector		velocity;
	Vector		accel;
	Vector		m_vecWind;

	cl_entity_t	*m_pEntity; // if not null, this particle is tied to the given entity

	float		m_fRed;
	float		m_fGreen;
	float		m_fBlue;
	float		m_fRedStep;
	float		m_fGreenStep;
	float		m_fBlueStep;

	float		m_fAlpha;
	float		m_fAlphaStep;

	float		frame;
	float		m_fFrameStep;

	float		m_fAngle;
	float		m_fAngleStep;

	float		m_fSize;
	float		m_fSizeStep;

	float		m_fDrag;

	float		age;
	float		age_death;
	float		age_spray;
};

class CParticleType
{
public:
	CParticleType( CParticleType *pNext = NULL );

	// here is a particle system. Add a (set of) particles according to this type, and initialise their values.
	CParticle	*CreateParticle( CParticleSystem *pSys );

	// initialise this particle. Does not define velocity or age.
	void InitParticle( CParticle *pPart, CParticleSystem *pSys );
	
	bool		m_bIsDefined; // is this CParticleType just a placeholder?

	int		m_iRenderMode;
	int		m_iDrawCond;
	RandomRange	m_Bounce;
	RandomRange	m_BounceFriction;
	bool		m_bBouncing;

	RandomRange	m_Life;

	RandomRange	m_StartAlpha;
	RandomRange	m_EndAlpha;
	RandomRange	m_StartRed;
	RandomRange	m_EndRed;
	RandomRange	m_StartGreen;
	RandomRange	m_EndGreen;
	RandomRange	m_StartBlue;
	RandomRange	m_EndBlue;

	RandomRange	m_StartSize;
	RandomRange	m_SizeDelta;
	RandomRange	m_EndSize;

	RandomRange	m_StartFrame;
	RandomRange	m_EndFrame;
	RandomRange	m_FrameRate; // incompatible with EndFrame
	bool		m_bEndFrame;

	RandomRange	m_StartAngle;
	RandomRange	m_AngleDelta;

	RandomRange	m_SprayRate;
	RandomRange	m_SprayForce;
	RandomRange	m_SprayPitch;
	RandomRange	m_SprayYaw;
	RandomRange	m_SprayRoll;

	CParticleType	*m_pSprayType;

	RandomRange	m_Gravity;
	RandomRange	m_WindStrength;
	RandomRange	m_WindYaw;

	SpriteHandle	m_hSprite;
	CParticleType	*m_pOverlayType;

	RandomRange	m_Drag;

	CParticleType	*m_pNext;
	char		m_szName[32];
};

typedef enum
{
	AURORA_REMOVE = 0,
	AURORA_INVISIBLE,
	AURORA_DRAW,
} AURSTATE;

class CParticleSystem
{
public:
	CParticleSystem( cl_entity_t *ent, const char *szFilename, int attachment = 0, float lifetime = 0.0f );
	~CParticleSystem( void );

	void AllocateParticles( int iParticles );
	void CalculateDistance( void );
	CParticleType *GetType( const char *szName );
	CParticleType *AddPlaceholderType( const char *szName );
	CParticleType *ParseType( char *&szFile );

	cl_entity_t *GetEntity() { return m_pEntity; }

	// General functions
	AURSTATE UpdateSystem( float frametime );	// If this function returns false, the manager deletes the system
	void DrawSystem( void );
	CParticle *ActivateParticle( void );		// adds one of the free particles to the active list, and returns it for initialisation.
						// MUST CHECK WHETHER THIS RESULT IS NULL!
	// returns false if the particle has died
	bool UpdateParticle( CParticle *part, float frametime );
	void DrawParticle( CParticle *part, const Vector &right, const Vector &up );

	// Utility functions that have to be public
	bool ParticleIsVisible( CParticle* part );

	void MarkForDeletion( void );

	static float CosLookup( int angle ) { return angle < 0 ? c_fCosTable[angle+360] : c_fCosTable[angle]; }
	static float SinLookup( int angle ) { return angle < -90 ? c_fCosTable[angle+450] : c_fCosTable[angle+90]; }

	// Pointer to next system for linked list structure	
	CParticleSystem	*m_pNextSystem;

	CParticle		*m_pActiveParticle;
	float		m_fViewerDist;
	cl_entity_t	*m_pEntity;
	int 		m_iEntAttachment;
	int		m_iKillCondition;
	int		m_iLightingModel;
	matrix3x3		entityMatrix;
	int		m_fHasProjectionLighting;
	float		m_fLifeTime;	// for auto-removed particles
	Vector		m_vecAbsMin;
	Vector		m_vecAbsMax;
	bool		enable;

private:
	static float	c_fCosTable[360 + 90];
	static bool	c_bCosTableInit;

	// the block of allocated particles
	CParticle		*m_pAllParticles;

	// First particles in the linked list for the active particles and the dead particles
	CParticle		*m_pFreeParticle;
	CParticle		*m_pMainParticle; // the "source" particle.

	CParticleType	*m_pFirstType;
	CParticleType	*m_pMainType;
};

class CParticleSystemManager
{
public:
	CParticleSystemManager( void );
	~CParticleSystemManager( void );
	void AddSystem( CParticleSystem* );
	CParticleSystem *FindSystem( CParticleSystem *pFirstSystem, cl_entity_t *pEntity );
	void MarkSystemForDeletion( CParticleSystem *pSys );
	void UpdateSystems( void );
	void ClearSystems( void );
	void SortSystems( void );
private:
	CParticleSystem	*m_pFirstSystem;
};

extern CParticleSystemManager	g_pParticleSystems; // buz

#endif//GL_PARTICLE_H