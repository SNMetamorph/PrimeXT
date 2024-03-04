/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define SF_BEAM_STARTON		BIT( 0 )
#define SF_BEAM_TOGGLE		BIT( 1 )
#define SF_BEAM_RANDOM		BIT( 2 )
#define SF_BEAM_RING		BIT( 3 )
#define SF_BEAM_SPARKSTART		BIT( 4 )
#define SF_BEAM_SPARKEND		BIT( 5 )
#define SF_BEAM_DECALS		BIT( 6 )
#define SF_BEAM_SHADEIN		BIT( 7 )
#define SF_BEAM_SHADEOUT		BIT( 8 )
#define SF_BEAM_SOLID		BIT( 9 )

#define SF_BEAM_MOVEABLE		0x4000	// fix tripmine bug
#define SF_BEAM_TEMPORARY		0x8000
#define SF_BEAM_TRIPPED		BIT( 29 )
#define SF_BEAM_INITIALIZE		BIT( 30 )

class CBeam : public CBaseEntity
{
	DECLARE_CLASS( CBeam, CBaseEntity );
public:
	void	Spawn( void );
	void	Precache( void );
	int	ObjectCaps( void )
	{ 
		int flags = 0;
		if ( pev->spawnflags & SF_BEAM_TEMPORARY )
			flags = FCAP_DONT_SAVE;
		if ( pev->spawnflags & SF_BEAM_MOVEABLE )
			return (BaseClass :: ObjectCaps()) | flags; 			
		return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | flags; 
	}

	void TriggerTouch( CBaseEntity *pOther );

	// These functions are here to show the way beams are encoded as entities.
	// Encoding beams as entities simplifies their management in the client/server architecture
	inline void SetType( int type ) { pev->rendermode = (pev->rendermode & 0xF0) | (type&0x0F); }
	inline void SetFlags( int flags ) { pev->rendermode = (pev->rendermode & 0x0F) | (flags&0xF0); }
	inline void SetStartPos( const Vector& pos ) { SetLocalOrigin( pos ); }
	inline void SetEndPos( const Vector& pos ) { InvalidatePhysicsState( FL_ABSTRANSFORM ); m_vecEndPos = pos; }
	void SetStartEntity( int entityIndex );
	void SetEndEntity( int entityIndex );

	inline void SetStartAttachment( int attachment ) { pev->sequence = (pev->sequence & 0x0FFF) | ((attachment&0xF)<<12); }
	inline void SetEndAttachment( int attachment ) { pev->skin = (pev->skin & 0x0FFF) | ((attachment&0xF)<<12); }

	inline void SetTexture( int spriteIndex ) { pev->modelindex = spriteIndex; }
	inline void SetWidth( int width ) { pev->scale = width; }
	inline void SetNoise( int amplitude ) { pev->body = amplitude; }
	inline void SetColor( int r, int g, int b ) { pev->rendercolor.x = r; pev->rendercolor.y = g; pev->rendercolor.z = b; }
	inline void SetBrightness( int brightness ) { pev->renderamt = brightness; }
	inline void SetFrame( float frame ) { pev->frame = frame; }
	inline void SetScrollRate( int speed ) { pev->animtime = speed; }

	inline int GetType( void ) const { return pev->rendermode & 0x0F; }
	inline int GetFlags( void ) const { return pev->rendermode & 0xF0; }
	inline int GetStartEntity( void ) const { return pev->sequence & 0xFFF; }
	inline int GetEndEntity( void ) const { return pev->skin & 0xFFF; }

	const Vector &GetStartPos( void );
	const Vector &GetEndPos( void );

	// this will change things so the abs position matches the requested spot
	void SetAbsStartPos( const Vector &pos );
	void SetAbsEndPos( const Vector &pos );

	const Vector &GetAbsStartPos( void ) const;
	const Vector &GetAbsEndPos( void ) const;

	Vector Center( void ) { return (GetStartPos() + GetEndPos()) * 0.5; }; // center point of beam

	CBaseEntity *GetTripEntity( TraceResult *ptr );

	inline int  GetTexture( void ) { return pev->modelindex; }
	inline int  GetWidth( void ) { return pev->scale; }
	inline int  GetNoise( void ) { return pev->body; }
	inline int  GetBrightness( void ) { return pev->renderamt; }
	inline int  GetFrame( void ) { return pev->frame; }
	inline int  GetScrollRate( void ) { return pev->animtime; }

	// Call after you change start/end positions
	void		RelinkBeam( void );
//	void		SetObjectCollisionBox( void );

	void		DoSparks( const Vector &start, const Vector &end );
	CBaseEntity	*RandomTargetname( const char *szName );
	void		BeamDamage( TraceResult *ptr );
	// Init after BeamCreate()
	void		BeamInit( const char *pSpriteName, int width );
	void		PointsInit( const Vector &start, const Vector &end );
	void		PointEntInit( const Vector &start, int endIndex );
	void		EntsInit( int startIndex, int endIndex );
	void		HoseInit( const Vector &start, const Vector &direction );

	DECLARE_DATADESC();

	static CBeam *BeamCreate( const char *pSpriteName, int width );

	inline void LiveForTime( float time ) { SetThink( &CBaseEntity::SUB_Remove); SetNextThink( time ); }
	inline void BeamDamageInstant( TraceResult *ptr, float damage ) 
	{ 
		pev->dmg = damage; 
		pev->dmgtime = gpGlobals->time - 1;
		BeamDamage(ptr); 
	}
private:
	Vector	m_vecAbsEndPos;	// temp store beam end pos so we don't need save\restore
};
