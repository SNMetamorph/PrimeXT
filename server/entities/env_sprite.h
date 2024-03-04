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

#define SF_SPRITE_STARTON		0x0001
#define SF_SPRITE_ONCE			0x0002
#define SF_SPRITE_TEMPORARY		0x8000

class CSprite : public CPointEntity
{
	DECLARE_CLASS( CSprite, CPointEntity );
public:
	void Spawn( void );
	void Precache( void );
	void Activate( void );

	int ObjectCaps( void )
	{ 
		int flags = 0;
		if ( pev->spawnflags & SF_SPRITE_TEMPORARY )
			flags = FCAP_DONT_SAVE;
		return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | flags; 
	}

	void AnimateThink( void );
	void ExpandThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Animate( float frames );
	void Expand( float scaleSpeed, float fadeSpeed );
	void SpriteInit( const char *pSpriteName, const Vector &origin );
	void UpdateTrainPath( void );

	virtual STATE GetState( void ) { return (pev->effects & EF_NODRAW) ? STATE_OFF : STATE_ON; };

	inline void SetAttachment( edict_t *pEntity, int attachment )
	{
		if ( pEntity )
		{
			pev->skin = ENTINDEX(pEntity);
			pev->body = attachment;
			pev->aiment = pEntity;
			pev->movetype = MOVETYPE_FOLLOW;
		}
	}

	void TurnOff( void );
	void TurnOn( void );
	inline float Frames( void ) { return m_maxFrame; }
	inline void SetTransparency( int rendermode, int r, int g, int b, int a, int fx )
	{
		pev->rendermode = rendermode;
		pev->rendercolor.x = r;
		pev->rendercolor.y = g;
		pev->rendercolor.z = b;
		pev->renderamt = a;
		pev->renderfx = fx;
	}

	inline void SetTexture( int spriteIndex ) { pev->modelindex = spriteIndex; }
	inline void SetScale( float scale ) { pev->scale = scale; }
	inline void SetColor( int r, int g, int b ) { pev->rendercolor.x = r; pev->rendercolor.y = g; pev->rendercolor.z = b; }
	inline void SetBrightness( int brightness ) { pev->renderamt = brightness; }

	inline void AnimateAndDie( float framerate ) 
	{ 
		SetThink( &CSprite::AnimateUntilDead); 
		pev->framerate = framerate;
		pev->dmgtime = gpGlobals->time + (m_maxFrame / framerate); 
		pev->nextthink = gpGlobals->time; 
	}

	void AnimateUntilDead( void );
	void SpriteMove( void );

	DECLARE_DATADESC();

	static CSprite *SpriteCreate( const char *pSpriteName, const Vector &origin, BOOL animate );

	int	m_activated;
	float	m_lastTime;
	float	m_maxFrame;
};
