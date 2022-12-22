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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "customentity.h"
#include "effects.h"
#include "weapons.h"
#include "player.h"
#include "decals.h"
#include "func_break.h"
#include "shake.h"
#include "studio.h"
#include "gamerules.h"
#include "trains.h"

#define SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired
#define SF_FUNNEL_REVERSE		1 // funnel effect repels particles instead of attracting them.

// =================== INFO_TARGET ==============================================

#define SF_TARGET_HACK_VISIBLE	BIT( 0 )

class CInfoTarget : public CPointEntity
{
	DECLARE_CLASS( CInfoTarget, CPointEntity );
public:
	void Spawn( void );
	void Precache( void );
};

LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );

void CInfoTarget :: Precache( void )
{
	if( FBitSet( pev->spawnflags, SF_TARGET_HACK_VISIBLE ))
		PRECACHE_MODEL( "sprites/null.spr" );
}

void CInfoTarget :: Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	SetBits( m_iFlags, MF_POINTENTITY );

	if( FBitSet( pev->spawnflags, SF_TARGET_HACK_VISIBLE ))
	{
		SET_MODEL( edict(), "sprites/null.spr" );
		UTIL_SetSize( pev, g_vecZero, g_vecZero );
	}
}

class CBubbling : public CBaseEntity
{
	DECLARE_CLASS( CBubbling, CBaseEntity );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	
	void	FizzThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();

	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	int	m_density;
	int	m_frequency;
	int	m_bubbleModel;
	int	m_state;
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbling );

BEGIN_DATADESC( CBubbling )
	DEFINE_KEYFIELD( m_density, FIELD_INTEGER, "density" ),
	DEFINE_KEYFIELD( m_frequency, FIELD_INTEGER, "frequency" ),
	DEFINE_FIELD( m_state, FIELD_INTEGER ),
	DEFINE_FUNCTION( FizzThink ),
END_DATADESC()


#define SF_BUBBLES_STARTOFF		0x0001

void CBubbling::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), STRING(pev->model) );		// Set size

	pev->solid = SOLID_NOT;							// Remove model & collisions
	pev->renderamt = 0;								// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;
	int speed = pev->speed > 0 ? pev->speed : -pev->speed;

	// HACKHACK!!! - Speed in rendercolor
	pev->rendercolor.x = speed >> 8;
	pev->rendercolor.y = speed & 255;
	pev->rendercolor.z = (pev->speed < 0) ? 1 : 0;


	if ( !(pev->spawnflags & SF_BUBBLES_STARTOFF) )
	{
		SetThink( &CBubbling::FizzThink );
		pev->nextthink = gpGlobals->time + 2.0;
		m_state = 1;
	}
	else 
		m_state = 0;
}

void CBubbling::Precache( void )
{
	m_bubbleModel = PRECACHE_MODEL("sprites/bubble.spr");			// Precache bubble sprite
}


void CBubbling::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, m_state ) )
		m_state = !m_state;

	if ( m_state )
	{
		SetThink( &CBubbling::FizzThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		SetThink( NULL );
		pev->nextthink = 0;
	}
}


void CBubbling::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "density"))
	{
		m_density = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		m_frequency = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "current"))
	{
		pev->speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}


void CBubbling::FizzThink( void )
{
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, VecBModelOrigin(pev) );
		WRITE_BYTE( TE_FIZZ );
		WRITE_SHORT( (short)ENTINDEX( edict() ) );
		WRITE_SHORT( (short)m_bubbleModel );
		WRITE_BYTE( m_density );
	MESSAGE_END();

	if ( m_frequency > 19 )
		pev->nextthink = gpGlobals->time + 0.5;
	else
		pev->nextthink = gpGlobals->time + 2.5 - (0.1 * m_frequency);
}

// --------------------------------------------------
// 
// Beams
//
// --------------------------------------------------
LINK_ENTITY_TO_CLASS( beam, CBeam );

BEGIN_DATADESC( CBeam )
	DEFINE_FUNCTION( TriggerTouch ),
END_DATADESC()

void CBeam::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT; // Remove model & collisions
}

void CBeam::Precache( void )
{
	if ( pev->owner )
		SetStartEntity( ENTINDEX( pev->owner ) );
	if ( pev->aiment )
		SetEndEntity( ENTINDEX( pev->aiment ) );
}

void CBeam::SetStartEntity( int entityIndex ) 
{ 
	pev->sequence = (entityIndex & 0x0FFF) | ((pev->sequence&0xF000)<<12); 
	pev->owner = INDEXENT( entityIndex );
}

void CBeam::SetEndEntity( int entityIndex ) 
{ 
	pev->skin = (entityIndex & 0x0FFF) | ((pev->skin&0xF000)<<12); 
	pev->aiment = INDEXENT( entityIndex );
}

// These don't take attachments into account
const Vector &CBeam::GetStartPos( void )
{
	if ( GetType() == BEAM_ENTS )
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));
		return pEntity->GetLocalOrigin();
	}
	return GetLocalOrigin();
}

const Vector &CBeam::GetEndPos( void )
{
	int type = GetType();

	if( type == BEAM_POINTS || type == BEAM_HOSE )
		return m_vecEndPos;

	CBaseEntity *pEntity;
	pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));
	if( pEntity ) return pEntity->GetLocalOrigin();

	return m_vecEndPos;
}

void CBeam::SetAbsStartPos( const Vector &pos )
{
	if( m_hParent == NULL )
	{
		SetStartPos( pos );
		return;
	}

	matrix4x4	worldToBeam = EntityToWorldTransform();
	Vector vecLocalPos = worldToBeam.VectorITransform( pos );

	SetStartPos( vecLocalPos );
}

void CBeam::SetAbsEndPos( const Vector &pos )
{
	if( m_hParent == NULL )
	{
		SetEndPos( pos );
		return;
	}

	matrix4x4	worldToBeam = EntityToWorldTransform();
	Vector vecLocalPos = worldToBeam.VectorITransform( pos );

	SetEndPos( vecLocalPos );
}

// These don't take attachments into account
const Vector &CBeam::GetAbsStartPos( void ) const
{
	if( GetType() == BEAM_ENTS && GetStartEntity( ))
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetStartEntity() ));

		if( pEntity )
			return pEntity->GetAbsOrigin();
		return GetAbsOrigin();
	}
	return GetAbsOrigin();
}

const Vector &CBeam::GetAbsEndPos( void ) const
{
	if( GetType() != BEAM_POINTS && GetType() != BEAM_HOSE && GetEndEntity() ) 
	{
		CBaseEntity *pEntity;
		pEntity = CBaseEntity :: Instance( INDEXENT( GetEndEntity() ));

		if( pEntity )
			return pEntity->GetAbsOrigin();
	}

	if( const_cast<CBeam*>(this)->m_hParent == NULL )
		return m_vecEndPos;

	matrix4x4	beamToWorld = EntityToWorldTransform();
	const_cast<CBeam*>(this)->m_vecAbsEndPos = beamToWorld.VectorTransform( m_vecEndPos );

	return m_vecAbsEndPos;
}

CBeam *CBeam::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam = GetClassPtr( (CBeam *)NULL );
	pBeam->pev->classname = MAKE_STRING( "beam" );
	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}

void CBeam::BeamInit( const char *pSpriteName, int width )
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING( pSpriteName );
	SetTexture( PRECACHE_MODEL( (char *)pSpriteName ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}

void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::PointEntInit( const Vector &start, int endIndex )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( int startIndex, int endIndex )
{
	SetType( BEAM_ENTS );
	SetStartEntity( startIndex );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::RelinkBeam( void )
{
	const Vector &startPos = GetAbsStartPos();
	const Vector &endPos = GetAbsEndPos();

	pev->mins.x = Q_min( startPos.x, endPos.x );
	pev->mins.y = Q_min( startPos.y, endPos.y );
	pev->mins.z = Q_min( startPos.z, endPos.z );
	pev->maxs.x = Q_max( startPos.x, endPos.x );
	pev->maxs.y = Q_max( startPos.y, endPos.y );
	pev->maxs.z = Q_max( startPos.z, endPos.z );
	pev->mins = pev->mins - GetAbsOrigin();
	pev->maxs = pev->maxs - GetAbsOrigin();

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	RelinkEntity( TRUE );
}

void CBeam::TriggerTouch( CBaseEntity *pOther )
{
	if( FBitSet( pOther->pev->flags, ( FL_CLIENT|FL_MONSTER ) ))
	{
		if( pev->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance( pev->owner );
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
		ALERT( at_console, "Firing targets!!!\n" );
	}
}

CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	if( !szName || !*szName )
		return NULL;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;

	while(( pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL )
	{
		total++;
		if( RANDOM_LONG( 0, total - 1 ) < 1 )
			pEntity = pNewEntity;
	}
	return pEntity;
}

void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if( FBitSet( pev->spawnflags, ( SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND )))
	{
		if( FBitSet( pev->spawnflags, SF_BEAM_SPARKSTART ))
		{
			UTIL_Sparks( start );
		}
		if( FBitSet( pev->spawnflags, SF_BEAM_SPARKEND ))
		{
			UTIL_Sparks( end );
		}
	}
}


class CLightning : public CBeam
{
	DECLARE_CLASS( CLightning, CBeam );
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );

	void	StrikeThink( void );
	void	DamageThink( void );
	void	RandomArea( void );
	void	RandomPoint( const Vector &vecSrc );
	void	Zap( const Vector &vecSrc, const Vector &vecDest );
	void	StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	inline BOOL ServerSide( void )
	{
		if ( m_life == 0 && !( pev->spawnflags & SF_BEAM_RING ))
			return TRUE;
		return FALSE;
	}

	DECLARE_DATADESC();

	void	BeamUpdateVars( void );

	int	m_active;
	int	m_iszStartEntity;
	int	m_iszEndEntity;
	float	m_life;
	int	m_boltWidth;
	int	m_noiseAmplitude;
	int	m_speed;
	float	m_restrike;
	int	m_spriteTexture;
	int	m_iszSpriteName;
	int	m_frameStart;

	float	m_radius;
};

LINK_ENTITY_TO_CLASS( env_lightning, CLightning );
LINK_ENTITY_TO_CLASS( env_beam, CLightning );

// UNDONE: Jay -- This is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	DECLARE_CLASS( CTripBeam, CLightning );
public:
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trip_beam, CTripBeam );

void CTripBeam::Spawn( void )
{
	BaseClass::Spawn();
	SetTouch( &CBeam::TriggerTouch );
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif

BEGIN_DATADESC( CLightning )
	DEFINE_FIELD( m_active, FIELD_INTEGER ),
	DEFINE_FIELD( m_spriteTexture, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszStartEntity, FIELD_STRING, "LightningStart" ),
	DEFINE_KEYFIELD( m_iszEndEntity, FIELD_STRING, "LightningEnd" ),
	DEFINE_KEYFIELD( m_boltWidth, FIELD_INTEGER, "BoltWidth" ),
	DEFINE_KEYFIELD( m_noiseAmplitude, FIELD_INTEGER, "NoiseAmplitude" ),
	DEFINE_KEYFIELD( m_speed, FIELD_INTEGER, "TextureScroll" ),
	DEFINE_KEYFIELD( m_restrike, FIELD_FLOAT, "StrikeTime" ),
	DEFINE_KEYFIELD( m_iszSpriteName, FIELD_STRING, "texture" ),
	DEFINE_KEYFIELD( m_frameStart, FIELD_INTEGER, "framestart" ),
	DEFINE_KEYFIELD( m_radius, FIELD_FLOAT, "Radius" ),
	DEFINE_KEYFIELD( m_life, FIELD_FLOAT, "life" ),
	DEFINE_FUNCTION( StrikeThink ),
	DEFINE_FUNCTION( DamageThink ),
	DEFINE_FUNCTION( StrikeUse ),
	DEFINE_FUNCTION( ToggleUse ),
END_DATADESC()

void CLightning::Spawn( void )
{
	if( FStringNull( m_iszSpriteName ))
	{
		SetThink( &CBaseEntity::SUB_Remove );
		return;
	}

	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	pev->dmgtime = gpGlobals->time;

	//LRC- a convenience for mappers. Will this mess anything up?
	if (pev->rendercolor == g_vecZero)
		pev->rendercolor = Vector(255, 255, 255);

	if ( ServerSide() )
	{
		SetThink( NULL );
		if ( pev->dmg > 0 )
		{
			SetThink( &CLightning::DamageThink );
			pev->nextthink = gpGlobals->time + 0.1;
		}
		if ( pev->targetname )
		{
			if ( !(pev->spawnflags & SF_BEAM_STARTON) )
			{
				pev->effects |= EF_NODRAW;
				m_active = 0;
				DontThink();
			}
			else
				m_active = 1;
		
			SetUse( &CLightning::ToggleUse );
		}
	}
	else
	{
		m_active = 0;
		if ( !FStringNull(pev->targetname) )
		{
			SetUse( &CLightning::StrikeUse );
		}
		if ( FStringNull(pev->targetname) || FBitSet(pev->spawnflags, SF_BEAM_STARTON) )
		{
			SetThink( &CLightning::StrikeThink );
			pev->nextthink = gpGlobals->time + 1.0;
		}
	}
}

void CLightning::Precache( void )
{
	m_spriteTexture = PRECACHE_MODEL( (char *)STRING(m_iszSpriteName) );

	BaseClass::Precache();
}


void CLightning::Activate( void )
{
	if ( ServerSide() )
		BeamUpdateVars();
}


void CLightning::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LightningStart"))
	{
		m_iszStartEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEnd"))
	{
		m_iszEndEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "life"))
	{
		m_life = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "BoltWidth"))
	{
		m_boltWidth = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		m_noiseAmplitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		m_speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StrikeTime"))
	{
		m_restrike = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		m_frameStart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Radius"))
	{
		m_radius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeam::KeyValue( pkvd );
}


void CLightning::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;
	if ( m_active )
	{
		m_active = 0;
		SUB_UseTargets( this, USE_OFF, 0 ); //LRC
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else
	{
		m_active = 1;
		SUB_UseTargets( this, USE_ON, 0 ); //LRC
		pev->effects &= ~EF_NODRAW;
		DoSparks( GetAbsStartPos(), GetAbsEndPos() );
		if ( pev->dmg > 0 )
		{
			pev->nextthink = gpGlobals->time;
			pev->dmgtime = gpGlobals->time;
		}
	}
}

void CLightning::StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;

	if ( m_active )
	{
		m_active = 0;
		SetThink( NULL );
	}
	else
	{
		SetThink( &CLightning::StrikeThink );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	if ( !FBitSet( pev->spawnflags, SF_BEAM_TOGGLE ) )
		SetUse( NULL );
}

int IsPointEntity( CBaseEntity *pEnt )
{
	if ( pEnt->m_hParent != NULL )
		return 0;

	//LRC- follow (almost) any entity that has a model
	if (pEnt->pev->modelindex && !(pEnt->pev->flags & FL_CUSTOMENTITY))
		return 0;

	return 1;
}

void CLightning::StrikeThink( void )
{
	if ( m_life != 0 && m_restrike != -1) //LRC non-restriking beams! what an idea!
	{
		if ( pev->spawnflags & SF_BEAM_RANDOM )
			SetNextThink( m_life + RANDOM_FLOAT( 0, m_restrike ) );
		else
			SetNextThink( m_life + m_restrike );
	}
	m_active = 1;

	if (FStringNull(m_iszEndEntity))
	{
		if (FStringNull(m_iszStartEntity))
		{
			RandomArea( );
		}
		else
		{
			CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
			if (pStart != NULL)
				RandomPoint( pStart->GetAbsOrigin() );
			else
				ALERT( at_aiconsole, "env_beam: unknown entity \"%s\"\n", STRING(m_iszStartEntity) );
		}
		return;
	}

	CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
	CBaseEntity *pEnd = RandomTargetname( STRING(m_iszEndEntity) );

	if ( pStart != NULL && pEnd != NULL )
	{
		if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
		{
			if ( pev->spawnflags & SF_BEAM_RING)
			{
				// don't work
				return;
			}
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
			{
				if ( !IsPointEntity( pEnd ) )	// One point entity must be in pEnd
				{
					CBaseEntity *pTemp;
					pTemp = pStart;
					pStart = pEnd;
					pEnd = pTemp;
				}
				if ( !IsPointEntity( pStart ) )	// One sided
				{
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( pStart->entindex() );
					WRITE_COORD( pEnd->GetAbsOrigin().x);
					WRITE_COORD( pEnd->GetAbsOrigin().y);
					WRITE_COORD( pEnd->GetAbsOrigin().z);
				}
				else
				{
					WRITE_BYTE( TE_BEAMPOINTS);
					WRITE_COORD( pStart->GetAbsOrigin().x);
					WRITE_COORD( pStart->GetAbsOrigin().y);
					WRITE_COORD( pStart->GetAbsOrigin().z);
					WRITE_COORD( pEnd->GetAbsOrigin().x);
					WRITE_COORD( pEnd->GetAbsOrigin().y);
					WRITE_COORD( pEnd->GetAbsOrigin().z);
				}


			}
			else
			{
				if ( pev->spawnflags & SF_BEAM_RING)
					WRITE_BYTE( TE_BEAMRING );
				else
					WRITE_BYTE( TE_BEAMENTS );
				WRITE_SHORT( pStart->entindex() );
				WRITE_SHORT( pEnd->entindex() );
			}

			WRITE_SHORT( m_spriteTexture );
			WRITE_BYTE( m_frameStart ); // framestart
			WRITE_BYTE( (int)pev->framerate); // framerate
			WRITE_BYTE( (int)(m_life*10.0) ); // life
			WRITE_BYTE( m_boltWidth );  // width
			WRITE_BYTE( m_noiseAmplitude );   // noise
			WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
			WRITE_BYTE( pev->renderamt );	// brightness
			WRITE_BYTE( m_speed );		// speed
		MESSAGE_END();
		DoSparks( pStart->GetAbsOrigin(), pEnd->GetAbsOrigin() );
		if ( pev->dmg || !FStringNull(pev->target))
		{
			TraceResult tr;
			UTIL_TraceLine( pStart->GetAbsOrigin(), pEnd->GetAbsOrigin(), dont_ignore_monsters, NULL, &tr );
			if( pev->dmg ) BeamDamageInstant( &tr, pev->dmg );

			//LRC - tripbeams
			CBaseEntity* pTrip;
			if (!FStringNull(pev->target) && (pTrip = GetTripEntity( &tr )) != NULL)
				UTIL_FireTargets( pev->target, pTrip, this, USE_TOGGLE, 0);
		}
	}
}

CBaseEntity *CBeam::GetTripEntity( TraceResult *ptr )
{
	CBaseEntity *pTrip;

	if( ptr->flFraction == 1.0 || ptr->pHit == NULL )
		return NULL;

	pTrip = CBaseEntity::Instance( ptr->pHit );
	if( pTrip == NULL )
		return NULL;

	if( FStringNull( pev->netname ))
	{
		if( pTrip->pev->flags & (FL_CLIENT|FL_MONSTER))
			return pTrip;
		else
			return NULL;
	}
	else if( FClassnameIs( pTrip->pev, STRING( pev->netname )))
		return pTrip;
	else if( FStrEq( pTrip->GetTargetname(), STRING( pev->netname )))
		return pTrip;

	return NULL;
}

void CBeam::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();

	if( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( ptr->pHit );
		if ( pHit )
		{
			if( pev->dmg > 0 )
			{  
				ClearMultiDamage();
				pHit->TraceAttack( pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - GetAbsOrigin()).Normalize(), ptr, DMG_ENERGYBEAM );
				ApplyMultiDamage( pev, pev );
				if ( pev->spawnflags & SF_BEAM_DECALS )
				{
					if ( pHit->IsBSPModel() )
						UTIL_DecalTrace( ptr, "{bigshot1" );
				}
			}
			else
			{
				//LRC - beams that heal people
				pHit->TakeHealth( -(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC );
			}
		}
	}

	pev->dmgtime = gpGlobals->time;
}


void CLightning::DamageThink( void )
{
	SetNextThink( 0.1 );
	TraceResult tr;
	UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
	BeamDamage( &tr );

	//LRC - tripbeams
	if (!FStringNull(pev->target))
	{
		// nicked from monster_tripmine:
		//HACKHACK Set simple box using this really nice global!
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
		CBaseEntity *pTrip = GetTripEntity( &tr );
		if (pTrip)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				UTIL_FireTargets( pev->target, pTrip, this, USE_TOGGLE, 0);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}
}

void CLightning::Zap( const Vector &vecSrc, const Vector &vecDest )
{
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_SHORT( m_spriteTexture );
		WRITE_BYTE( m_frameStart ); // framestart
		WRITE_BYTE( (int)pev->framerate); // framerate
		WRITE_BYTE( (int)(m_life*10.0) ); // life
		WRITE_BYTE( m_boltWidth );  // width
		WRITE_BYTE( m_noiseAmplitude );   // noise
		WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
		WRITE_BYTE( pev->renderamt );	// brightness
		WRITE_BYTE( m_speed );		// speed
	MESSAGE_END();

	DoSparks( vecSrc, vecDest );
}

void CLightning::RandomArea( void )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecSrc = GetAbsOrigin();

		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if (tr1.flFraction == 1.0)
			continue;

		Vector vecDir2;
		do {
			vecDir2 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		} while (DotProduct(vecDir1, vecDir2 ) > 0);
		vecDir2 = vecDir2.Normalize();
		TraceResult		tr2;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir2 * m_radius, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction == 1.0)
			continue;

		if ((tr1.vecEndPos - tr2.vecEndPos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine( tr1.vecEndPos, tr2.vecEndPos, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction != 1.0)
			continue;

		Zap( tr1.vecEndPos, tr2.vecEndPos );

		break;
	}
}


void CLightning::RandomPoint( const Vector &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if ((tr1.vecEndPos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.flFraction == 1.0)
			continue;

		Zap( vecSrc, tr1.vecEndPos );
		break;
	}
}



void CLightning::BeamUpdateVars( void )
{
	int pointStart, pointEnd;
	int beamType;

	CBaseEntity *pStart = UTIL_FindEntityByTargetname( NULL, STRING( m_iszStartEntity ));
	CBaseEntity *pEnd = UTIL_FindEntityByTargetname( NULL, STRING( m_iszEndEntity ));
	if( !pStart || !pEnd ) return;

	pointStart = IsPointEntity( pStart );
	pointEnd = IsPointEntity( pEnd );

	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	pev->flags |= FL_CUSTOMENTITY;
	pev->model = m_iszSpriteName;
	SetTexture( m_spriteTexture );

	beamType = BEAM_ENTS;

	if( pointStart || pointEnd )
	{
		if( !pointStart )	// One point entity must be in pStart
		{
			CBaseEntity *pTemp;
			// Swap start & end
			pTemp = pStart;
			pStart = pEnd;
			pEnd = pTemp;
			int swap = pointStart;
			pointStart = pointEnd;
			pointEnd = swap;
		}

		if( !pointEnd )
			beamType = BEAM_ENTPOINT;
		else
			beamType = BEAM_POINTS;
	}

	SetType( beamType );

	if( beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE )
	{
		SetStartPos( pStart->GetAbsOrigin( ));

		if ( beamType == BEAM_POINTS || beamType == BEAM_HOSE )
			SetEndPos( pEnd->GetAbsOrigin( ));
		else
			SetEndEntity( pEnd->entindex( ));
	}
	else
	{
		SetStartEntity( pStart->entindex( ));
		SetEndEntity( pEnd->entindex( ));
	}

	RelinkBeam();

	SetWidth( m_boltWidth );
	SetNoise( m_noiseAmplitude );
	SetFrame( m_frameStart );
	SetScrollRate( m_speed );

	if( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
	else if( pev->spawnflags & SF_BEAM_SOLID )
		SetFlags( BEAM_FSOLID );
}

LINK_ENTITY_TO_CLASS( env_laser, CLaser );

BEGIN_DATADESC( CLaser )
	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),
	DEFINE_KEYFIELD( m_iStoppedBy, FIELD_INTEGER, "m_iStoppedBy" ),
	DEFINE_KEYFIELD( m_iProjection, FIELD_INTEGER, "m_iProjection" ),
	DEFINE_KEYFIELD( m_iszSpriteName, FIELD_STRING, "EndSprite" ),
	DEFINE_FIELD( m_firePosition, FIELD_POSITION_VECTOR ),
	DEFINE_AUTO_ARRAY( m_pReflectedBeams, FIELD_CLASSPTR ),
	DEFINE_FUNCTION( StrikeThink ),
END_DATADESC()

void CLaser::Spawn( void )
{
	if ( FStringNull( pev->model ) )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		return;
	}

	pev->solid = SOLID_NOT; // Remove model & collisions
	Precache( );

	SetThink( &CLaser::StrikeThink );
	pev->flags |= FL_CUSTOMENTITY;

	SetBits( pev->spawnflags, SF_BEAM_INITIALIZE );
}

void CLaser::Precache( void )
{
	pev->modelindex = PRECACHE_MODEL( (char *)STRING( pev->model ));

	if ( m_iszSpriteName )
	{
		const char *ext = UTIL_FileExtension( STRING( m_iszSpriteName ));

		if( FStrEq( ext, "spr" ))
			PRECACHE_MODEL( (char *)STRING( m_iszSpriteName ));
	}
}

void CLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LaserTarget"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		SetNoise( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		SetScrollRate( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "EndSprite"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		pev->frame = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iProjection"))
	{
		m_iProjection = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStoppedBy"))
	{
		m_iStoppedBy = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CLaser :: Activate( void )
{
	if( !FBitSet( pev->spawnflags, SF_BEAM_INITIALIZE ))
		return;

	ClearBits( pev->spawnflags, SF_BEAM_INITIALIZE );
	PointsInit( GetLocalOrigin(), GetLocalOrigin() );

	if ( m_iszSpriteName )
	{
		CBaseEntity *pTemp = UTIL_FindEntityByTargetname(NULL, STRING( m_iszSpriteName ));

		if( pTemp == NULL )
		{
			m_pSprite = CSprite::SpriteCreate( STRING(m_iszSpriteName), GetLocalOrigin(), TRUE );
			if (m_pSprite)
				m_pSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
		}
		else if( !FClassnameIs( pTemp->pev, "env_sprite" ))
		{
			ALERT(at_error, "env_laser \"%s\" found endsprite %s, but can't use: not an env_sprite\n", GetTargetname(), STRING(m_iszSpriteName));
			m_pSprite = NULL;
		}
		else
		{
			// use an env_sprite defined by the mapper
			m_pSprite = (CSprite *)pTemp;
			m_pSprite->pev->movetype = MOVETYPE_NOCLIP;
		}
	}
	else
	{
		m_pSprite = NULL;
          }

	if ( pev->targetname && !(pev->spawnflags & SF_BEAM_STARTON) )
		TurnOff();
	else
		TurnOn();
}

void CLaser::TurnOff( void )
{
	pev->effects |= EF_NODRAW;
	pev->nextthink = 0;

	if ( m_pSprite )
		m_pSprite->TurnOff();

	// remove all unused beams
	for( int i = 0; i < MAX_REFLECTED_BEAMS; i++ )
	{
		if( m_pReflectedBeams[i] )
		{
			UTIL_Remove( m_pReflectedBeams[i] );
			m_pReflectedBeams[i] = NULL;
		}
	}
}

void CLaser::TurnOn( void )
{
	pev->effects &= ~EF_NODRAW;

	if ( m_pSprite )
		m_pSprite->TurnOn();

	if( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
	else if( pev->spawnflags & SF_BEAM_SOLID )
		SetFlags( BEAM_FSOLID );

	pev->dmgtime = gpGlobals->time;
	SetNextThink( 0 );
}

void CLaser::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int active = (GetState() == STATE_ON);

	if ( !ShouldToggle( useType, active ) )
		return;

	if ( active )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

bool CLaser::ShouldReflect( TraceResult &tr )
{
	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

	if( !pHit )
		return false;

	if( UTIL_GetModelType( pHit->pev->modelindex ) != mod_brush )
		return false;

	Vector vecStart = tr.vecEndPos + tr.vecPlaneNormal * -2;	// mirror thickness
	Vector vecEnd = tr.vecEndPos + tr.vecPlaneNormal * 2;
	const char *pTextureName = TRACE_TEXTURE( pHit->edict(), vecStart, vecEnd );

	if( pTextureName != NULL && !Q_strnicmp( pTextureName, "reflect", 7 ))
		return true; // valid surface

	return false;
}

void CLaser::FireAtPoint( const Vector &startpos, TraceResult &tr )
{
	SetAbsEndPos( tr.vecEndPos );

	if ( m_pSprite )
		UTIL_SetOrigin( m_pSprite, tr.vecEndPos );

	if(( pev->nextthink != 0.0f ) || ( gpGlobals->time >= ( pev->dmgtime + 0.1f )))
	{
		BeamDamage( &tr );
		DoSparks( startpos, tr.vecEndPos );
	}

	if( m_iProjection > 1 )
	{
		IGNORE_GLASS iIgnoreGlass;
		if( m_iStoppedBy % 2 ) // if it's an odd number
			iIgnoreGlass = ignore_glass;
		else
			iIgnoreGlass = dont_ignore_glass;

		IGNORE_MONSTERS iIgnoreMonsters;
		if( m_iStoppedBy <= 1 )
			iIgnoreMonsters = dont_ignore_monsters;
		else if( m_iStoppedBy <= 3 )
			iIgnoreMonsters = missile;
		else
			iIgnoreMonsters = ignore_monsters;

		Vector vecDir = (tr.vecEndPos - startpos).Normalize();
		edict_t *pentIgnore = tr.pHit;
		Vector vecSrc, vecDest;
		TraceResult tRef = tr;
		int i = 0;
		float n;

		// NOTE: determine texture name. We can reflect laser only at texture with name "reflect"
		for( i = 0; i < MAX_REFLECTED_BEAMS; i++ )
		{
			if( !ShouldReflect( tRef )) break; // no more reflect
			n = -DotProduct( tRef.vecPlaneNormal, vecDir );			
			if( n < 0.5f ) break;

			vecDir = 2.0 * tRef.vecPlaneNormal * n + vecDir;
			vecSrc = tRef.vecEndPos + vecDir;
			vecDest = vecSrc + vecDir * 4096;

			if( !m_pReflectedBeams[i] ) // allocate new reflected beam
			{
				m_pReflectedBeams[i] = BeamCreate( GetModel(), pev->scale );
				m_pReflectedBeams[i]->pev->dmgtime = gpGlobals->time;
				m_pReflectedBeams[i]->pev->dmg = pev->dmg;
			}

			UTIL_TraceLine( vecSrc, vecDest, iIgnoreMonsters, iIgnoreGlass, pentIgnore, &tRef );

			CBaseEntity *pHit = CBaseEntity::Instance( tRef.pHit );
			pentIgnore = tRef.pHit;

			// hit a some bmodel (probably it picked by player)
			if( pHit && pHit->IsPushable( ))
			{
				// do additional trace for func_pushable (find a valid normal)
				UTIL_TraceModel( vecSrc, vecDest, point_hull, tRef.pHit, &tRef );
			}

			m_pReflectedBeams[i]->SetStartPos( vecSrc );
			m_pReflectedBeams[i]->SetEndPos( tRef.vecEndPos );

			// all other settings come from primary beam
			m_pReflectedBeams[i]->SetFlags( GetFlags());
			m_pReflectedBeams[i]->SetWidth( GetWidth());
			m_pReflectedBeams[i]->SetNoise( GetNoise());
			m_pReflectedBeams[i]->SetBrightness( GetBrightness());
			m_pReflectedBeams[i]->SetFrame( GetFrame());
			m_pReflectedBeams[i]->SetScrollRate( GetScrollRate());
			m_pReflectedBeams[i]->SetColor( pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z );

			if(( pev->nextthink != 0.0f ) || ( gpGlobals->time >= ( m_pReflectedBeams[i]->pev->dmgtime + 0.1f )))
			{
				m_pReflectedBeams[i]->BeamDamage( &tRef );
				m_pReflectedBeams[i]->DoSparks( vecSrc, tRef.vecEndPos );
			}
		}

		// ALERT( at_console, "%i beams used\n", i+1 );

		// remove all unused beams
		for( ; i < MAX_REFLECTED_BEAMS; i++ )
		{
			if( m_pReflectedBeams[i] )
			{
				UTIL_Remove( m_pReflectedBeams[i] );
				m_pReflectedBeams[i] = NULL;
			}
		}
	}
}

void CLaser::StrikeThink( void )
{
	Vector startpos = GetAbsStartPos();

	CBaseEntity *pEnd = RandomTargetname( STRING( pev->message ));

	if( pEnd )
	{
		m_firePosition = pEnd->GetAbsOrigin();
	}
	else if( m_iProjection )
	{
		UTIL_MakeVectors( GetAbsAngles() );
		m_firePosition = startpos + gpGlobals->v_forward * 4096;
	}
	else
	{
		m_firePosition = startpos;	// just in case
	}

	TraceResult tr;

	//UTIL_TraceLine( GetAbsOrigin(), m_firePosition, dont_ignore_monsters, NULL, &tr );
	IGNORE_GLASS iIgnoreGlass;
	if( m_iStoppedBy % 2 ) // if it's an odd number
		iIgnoreGlass = ignore_glass;
	else
		iIgnoreGlass = dont_ignore_glass;

	IGNORE_MONSTERS iIgnoreMonsters;
	if( m_iStoppedBy <= 1 )
		iIgnoreMonsters = dont_ignore_monsters;
	else if( m_iStoppedBy <= 3 )
		iIgnoreMonsters = missile;
	else
		iIgnoreMonsters = ignore_monsters;

	if( m_iProjection )
	{
		UTIL_TraceLine( startpos, m_firePosition, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );

		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

		// make additional trace for pushables
		if( pHit && pHit->IsPushable( ))
		{
			// do additional trace for func_pushable (find a valid normal)
			UTIL_TraceModel( startpos, m_firePosition, point_hull, tr.pHit, &tr );
		}

	}
	else
	{
		UTIL_TraceLine( startpos, m_firePosition, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );
	}

	FireAtPoint( startpos, tr );

	// LRC - tripbeams
	// g-cont. FIXME: get support for m_iProjection = 2
	if( pev->target && m_iProjection != 2 )
	{
		// nicked from monster_tripmine:
		// HACKHACK Set simple box using this really nice global!
//		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine( startpos, m_firePosition, dont_ignore_monsters, NULL, &tr );
		CBaseEntity *pTrip = GetTripEntity( &tr );
		if( pTrip )
		{
			if( !FBitSet( pev->spawnflags, SF_BEAM_TRIPPED ))
			{
				UTIL_FireTargets( pev->target, pTrip, this, USE_TOGGLE, 0 );
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}

	// moving or reflected laser is thinking every frame
	if( m_iProjection > 1 || m_hParent != NULL || ( pEnd && pEnd->m_hParent != NULL ))
		SetNextThink( 0 );
	else
		SetNextThink( 0.1 );
}

class CGlow : public CPointEntity
{
	DECLARE_CLASS( CGlow, CPointEntity );
public:
	void Spawn( void );
	void Think( void );
	void Animate( float frames );

	DECLARE_DATADESC();

	float m_lastTime;
	float m_maxFrame;
};

LINK_ENTITY_TO_CLASS( env_glow, CGlow );

BEGIN_DATADESC( CGlow )
	DEFINE_FIELD( m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( m_maxFrame, FIELD_FLOAT ),
END_DATADESC()

void CGlow::Spawn( void )
{
	pev->solid	= SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->frame	= 0;

	PRECACHE_MODEL( (char *)STRING(pev->model) );
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( m_maxFrame > 1.0 && pev->framerate != 0 )
		pev->nextthink	= gpGlobals->time + 0.1;

	m_lastTime = gpGlobals->time;
}


void CGlow::Think( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	pev->nextthink = gpGlobals->time + 0.1;
	m_lastTime = gpGlobals->time;
}

void CGlow::Animate( float frames )
{ 
	if ( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );
}

LINK_ENTITY_TO_CLASS( env_sprite, CSprite );
LINK_ENTITY_TO_CLASS( env_spritetrain, CSprite );

BEGIN_DATADESC( CSprite )
	DEFINE_FIELD( m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( m_maxFrame, FIELD_FLOAT ),
	DEFINE_FIELD( m_activated, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( AnimateThink ),
	DEFINE_FUNCTION( ExpandThink ),
	DEFINE_FUNCTION( AnimateUntilDead ),
	DEFINE_FUNCTION( SpriteMove ),
END_DATADESC()

void CSprite::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->frame = 0;

	if( FClassnameIs( pev, "env_spritetrain" ))
		pev->movetype = MOVETYPE_NOCLIP;
	else
		pev->movetype = MOVETYPE_NONE;

	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( pev->targetname && !(pev->spawnflags & SF_SPRITE_STARTON) )
		TurnOff();
	else
		TurnOn();
	
	// Worldcraft only sets y rotation, copy to Z
	if ( UTIL_GetSpriteType( pev->modelindex ) != SPR_ORIENTED && GetLocalAngles().y != 0 && GetLocalAngles().z == 0 )
	{
		Vector angles = GetLocalAngles();

		angles.z = angles.y;
		angles.y = 0;

		SetLocalAngles( angles );
	}
}

void CSprite::Activate( void )
{
	// Not yet active, so teleport to first target
	if( !m_activated && FClassnameIs( pev, "env_spritetrain" ))
	{
		m_activated = TRUE;

		m_pGoalEnt = GetNextTarget();
		if( m_pGoalEnt )
			UTIL_SetOrigin( this, m_pGoalEnt->GetLocalOrigin() );
	}
}

void CSprite::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );

	// Reset attachment after save/restore
	if ( pev->aiment )
		SetAttachment( pev->aiment, pev->body );
	else
	{
		// Clear attachment
		pev->skin = 0;
		pev->body = 0;
	}
}

void CSprite::SpriteInit( const char *pSpriteName, const Vector &origin )
{
	pev->model = MAKE_STRING(pSpriteName);
	SetLocalOrigin( origin );
	Spawn();
}

CSprite *CSprite::SpriteCreate( const char *pSpriteName, const Vector &origin, BOOL animate )
{
	CSprite *pSprite = GetClassPtr( (CSprite *)NULL );
	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->pev->classname = MAKE_STRING("env_sprite");
	pSprite->pev->solid = SOLID_NOT;
	pSprite->pev->movetype = MOVETYPE_NOCLIP;
	if ( animate )
		pSprite->TurnOn();

	return pSprite;
}

void CSprite::AnimateThink( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	pev->nextthink = gpGlobals->time + 0.1;
	m_lastTime = gpGlobals->time;
}

void CSprite::AnimateUntilDead( void )
{
	if ( gpGlobals->time > pev->dmgtime )
	{
		UTIL_Remove(this);
	}
	else
	{
		AnimateThink();
		pev->nextthink = gpGlobals->time;
	}
}

void CSprite::Expand( float scaleSpeed, float fadeSpeed )
{
	pev->speed = scaleSpeed;
	pev->health = fadeSpeed;
	SetThink( &CSprite::ExpandThink );

	pev->nextthink	= gpGlobals->time;
	m_lastTime	= gpGlobals->time;
}


void CSprite::ExpandThink( void )
{
	float frametime = gpGlobals->time - m_lastTime;
	pev->scale += pev->speed * frametime;
	pev->renderamt -= pev->health * frametime;
	if ( pev->renderamt <= 0 )
	{
		pev->renderamt = 0;
		UTIL_Remove( this );
	}
	else
	{
		pev->nextthink		= gpGlobals->time + 0.1;
		m_lastTime			= gpGlobals->time;
	}
}


void CSprite::Animate( float frames )
{ 
	pev->frame += frames;
	if ( pev->frame > m_maxFrame )
	{
		if( pev->spawnflags & SF_SPRITE_ONCE && !FClassnameIs( pev, "env_spritetrain" ))
		{
			TurnOff();
		}
		else
		{
			if ( m_maxFrame > 0 )
				pev->frame = fmod( pev->frame, m_maxFrame );
		}
	}
}

void CSprite::UpdateTrainPath( void )
{
	if( !m_pGoalEnt ) return;

	Vector delta = m_pGoalEnt->GetLocalOrigin() - GetLocalOrigin();
	pev->frags = delta.Length();
	pev->movedir = delta.Normalize();
}

void CSprite::SpriteMove( void )
{
	// Not moving on a path, return
	if( !m_pGoalEnt )
	{
		TurnOff();
		return;
	}

	if( m_lastTime < gpGlobals->time )
	{
		Animate( pev->framerate * (gpGlobals->time - m_lastTime) );
		m_lastTime = gpGlobals->time + 0.1f;
	}

	if( pev->dmgtime > gpGlobals->time )
	{
		SetNextThink( 0 );
		return; // wait for path_corner delay
	}

	// Subtract movement from the previous frame
	pev->frags -= pev->speed * gpGlobals->frametime;

	// Have we moved enough to reach the target?
	if( pev->frags <= 0 )
	{
teleport_sprite:
		if( m_pGoalEnt->pev->speed != 0 )
			pev->speed = m_pGoalEnt->pev->speed;

		// Fire the passtarget if there is one
		if ( m_pGoalEnt->pev->message )
		{
			UTIL_FireTargets( STRING( m_pGoalEnt->pev->message ), this, this, USE_TOGGLE, 0 );
			if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_FIREONCE ) )
				m_pGoalEnt->pev->message = NULL_STRING;
		}

		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_WAITFORTRIG ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNextTarget();
			UpdateTrainPath();

			SetThink( &CSprite::AnimateThink );
			m_lastTime = gpGlobals->time;
			SetLocalVelocity( g_vecZero );
			SetNextThink( 0 );
			return;
		}

		float flDelay = m_pGoalEnt->GetDelay();

		if( flDelay != 0.0f )
		{
			m_pGoalEnt = m_pGoalEnt->GetNextTarget();
			UpdateTrainPath();

			SetLocalVelocity( g_vecZero );
			pev->dmgtime = gpGlobals->time + flDelay;
			SetNextThink( 0 );
			return;
		}

		if ( FBitSet( m_pGoalEnt->pev->spawnflags, SF_CORNER_TELEPORT ) )
		{
			m_pGoalEnt = m_pGoalEnt->GetNextTarget();

			if( m_pGoalEnt )
				UTIL_SetOrigin( this, m_pGoalEnt->GetLocalOrigin( )); 

			goto teleport_sprite;
		}
		
		// Time to go to the next target
		m_pGoalEnt = m_pGoalEnt->GetNextTarget();

		// Set up next corner
		if ( !m_pGoalEnt )
		{
			SetLocalVelocity( g_vecZero );
		}
		else
		{
			UpdateTrainPath();
		}
	}

	SetLocalVelocity( pev->movedir * pev->speed );
	SetNextThink( 0 );
}

void CSprite::TurnOff( void )
{
	if( !FClassnameIs( pev, "env_spritetrain" ))
		SUB_UseTargets( this, USE_OFF, 0 ); //LRC

	SetLocalVelocity( g_vecZero );
	pev->effects |= EF_NODRAW;
	pev->nextthink = 0;
}

void CSprite::TurnOn( void )
{
	if( !FClassnameIs( pev, "env_spritetrain" ))
		SUB_UseTargets( this, USE_ON, 0 ); //LRC

	pev->effects &= ~EF_NODRAW;

	if( FClassnameIs( pev, "env_spritetrain" ))
	{
		SetThink( &CSprite::SpriteMove );
		SetNextThink( 0 );
	}
	else if ( (pev->framerate && m_maxFrame > 1.0) || (pev->spawnflags & SF_SPRITE_ONCE) )
	{
		SetThink( &CSprite::AnimateThink );
		pev->nextthink = gpGlobals->time;
		m_lastTime = gpGlobals->time;
	}
	pev->frame = 0;
}


void CSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on;

	if( FClassnameIs( pev, "env_spritetrain" ))
		on = (GetLocalVelocity() != g_vecZero);
	else
		on = !(pev->effects & EF_NODRAW);

	if ( ShouldToggle( useType, on ) )
	{
		if ( on )
		{
			TurnOff();
		}
		else
		{
			TurnOn();
		}
	}
}

// Shooter particle
class CShot : public CSprite
{
	DECLARE_CLASS( CShot, CSprite );
public:
	void Touch( CBaseEntity *pOther )
	{
		if( pev->teleport_time > gpGlobals->time )
			return;

		// don't fire too often in collisions!
		// teleport_time is the soonest this can be touched again.
		pev->teleport_time = gpGlobals->time + 0.1;

		if( pev->netname )
			UTIL_FireTargets( pev->netname, this, this, USE_TOGGLE, 0 );
		if( pev->message && pOther != g_pWorld )
			UTIL_FireTargets( pev->message, pOther, this, USE_TOGGLE, 0 );
	};
};

LINK_ENTITY_TO_CLASS( shot, CShot ); // enable save\restore

class CGibShooter : public CBaseDelay
{
	DECLARE_CLASS( CGibShooter, CBaseDelay );
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void ShootThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & (~FCAP_ACROSS_TRANSITION)) | FCAP_SET_MOVEDIR; };

	virtual CBaseEntity *CreateGib( void );

	DECLARE_DATADESC();

	int	m_iGibs;
	int	m_iGibCapacity;
	int	m_iGibMaterial;
	int	m_iGibModelIndex;
	float	m_flGibVelocity;
	float	m_flVariance;
	float	m_flGibLife;
	string_t	m_iszTargetname;
	string_t	m_iszSpawnTarget;
	int	m_iBloodColor;
};

BEGIN_DATADESC( CGibShooter )
	DEFINE_KEYFIELD( m_iGibs, FIELD_INTEGER, "m_iGibs" ),
	DEFINE_KEYFIELD( m_iGibCapacity, FIELD_INTEGER, "m_iGibs" ),
	DEFINE_FIELD( m_iGibMaterial, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGibModelIndex, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_flGibVelocity, FIELD_FLOAT, "m_flVelocity" ),
	DEFINE_KEYFIELD( m_flVariance, FIELD_FLOAT, "m_flVariance" ),
	DEFINE_KEYFIELD( m_flGibLife, FIELD_FLOAT, "m_flGibLife" ),
	DEFINE_KEYFIELD( m_iszTargetname, FIELD_STRING, "m_iszTargetName" ),
	DEFINE_KEYFIELD( m_iszSpawnTarget, FIELD_STRING, "m_iszSpawnTarget" ),
	DEFINE_KEYFIELD( m_iBloodColor, FIELD_INTEGER, "m_iBloodColor" ),
	DEFINE_FUNCTION( ShootThink ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( gibshooter, CGibShooter );

void CGibShooter :: Precache ( void )
{
	if ( g_Language == LANGUAGE_GERMAN )
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/germanygibs.mdl");
	}
	else if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/agibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/hgibs.mdl");
	}
}

void CGibShooter::KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iGibs" ))
	{
		m_iGibs = m_iGibCapacity = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flVelocity" ))
	{
		m_flGibVelocity = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flVariance" ))
	{
		m_flVariance = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flGibLife" ))
	{
		m_flGibLife = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTargetName"))
	{
		m_iszTargetname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSpawnTarget"))
	{
		m_iszSpawnTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( pkvd );
	}
}

void CGibShooter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	SetThink( &CGibShooter::ShootThink );
	SetNextThink( 0 );
}

void CGibShooter::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	// is mapper forgot set angles?
	if( pev->movedir == g_vecZero )
		pev->movedir = Vector( 1, 0, 0 );

	if ( m_flDelay == 0 )
	{
		m_flDelay = 0.1;
	}

	if ( m_flGibLife == 0 )
	{
		m_flGibLife = 25;
	}

	pev->body = MODEL_FRAMES( m_iGibModelIndex );
}


CBaseEntity *CGibShooter :: CreateGib( void )
{
	if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
		return NULL;

	CGib *pGib = GetClassPtr( (CGib *)NULL );

	if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		pGib->Spawn( "models/agibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
	}
	else if (m_iBloodColor)
	{
		pGib->Spawn( "models/hgibs.mdl" );
		pGib->m_bloodColor = m_iBloodColor;
	}
	else
	{
		pGib->Spawn( "models/hgibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_RED;
	}

	if ( pev->body <= 1 )
	{
		ALERT ( at_aiconsole, "GibShooter Body is <= 1!\n" );
	}

	pGib->pev->body = RANDOM_LONG ( 1, pev->body - 1 );// avoid throwing random amounts of the 0th gib. (skull).

	return pGib;
}


void CGibShooter :: ShootThink ( void )
{
	SetNextThink( m_flDelay );

	Vector vecShootDir = EntityToWorldTransform().VectorRotate( pev->movedir );
	UTIL_MakeVectors( GetAbsAngles( )); // g-cont. was missed into original game

	vecShootDir = vecShootDir + gpGlobals->v_right * RANDOM_FLOAT( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + gpGlobals->v_forward * RANDOM_FLOAT( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + gpGlobals->v_up * RANDOM_FLOAT( -1, 1) * m_flVariance;

	vecShootDir = vecShootDir.Normalize();
	CBaseEntity *pShot = CreateGib();
	
	if( pShot )
	{
		pShot->SetAbsOrigin( GetAbsOrigin( ));
		pShot->SetAbsVelocity( vecShootDir * m_flGibVelocity );

		if( pShot->m_iActorType == ACTOR_DYNAMIC )
			WorldPhysic->AddForce( pShot, vecShootDir * m_flGibVelocity * ( 1.0f / gpGlobals->frametime ) * 0.1f );

		// custom particles already set this
		if( FClassnameIs( pShot->pev, "gib" ))
		{
			CGib *pGib = (CGib *)pShot;

			Vector vecAvelocity = pGib->GetLocalAvelocity();	
			vecAvelocity.x = RANDOM_FLOAT( 100, 200 );
			vecAvelocity.y = RANDOM_FLOAT( 100, 300 );
			pGib->SetLocalAvelocity( vecAvelocity );

			float thinkTime = pGib->pev->nextthink - gpGlobals->time;
			pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT( 0.95, 1.05 )); // +/- 5%

			if( pGib->m_lifeTime < thinkTime )
			{
				pGib->SetNextThink( pGib->m_lifeTime );
				pGib->m_lifeTime = 0;
			}
                    }

		pShot->pev->targetname = m_iszTargetname;

		if( m_iszSpawnTarget )
			UTIL_FireTargets( m_iszSpawnTarget, pShot, this, USE_TOGGLE, 0 );		
	}

	if( --m_iGibs <= 0 )
	{
		if( FBitSet( pev->spawnflags, SF_GIBSHOOTER_REPEATABLE ))
		{
			m_iGibs = m_iGibCapacity;
			SetThink( NULL );
			SetNextThink( 0 );
		}
		else
		{
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( 0 );
		}
	}
}


class CEnvShooter : public CGibShooter
{
	DECLARE_CLASS( CEnvShooter, CGibShooter );

	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );

	DECLARE_DATADESC();

	virtual CBaseEntity *CreateGib( void );

	string_t m_iszTouch;
	string_t m_iszTouchOther;
	int m_iPhysics;
	float m_fFriction;
	Vector m_vecSize;
};

BEGIN_DATADESC( CEnvShooter )
	DEFINE_KEYFIELD( m_iszTouch, FIELD_STRING, "m_iszTouch" ),
	DEFINE_KEYFIELD( m_iszTouchOther, FIELD_STRING, "m_iszTouchOther" ),
	DEFINE_KEYFIELD( m_iPhysics, FIELD_INTEGER, "m_iPhysics" ),
	DEFINE_KEYFIELD( m_fFriction, FIELD_FLOAT, "m_fFriction" ),
	DEFINE_KEYFIELD( m_vecSize, FIELD_VECTOR, "m_vecSize" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

void CEnvShooter :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "shootmodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shootsounds"))
	{
		int iNoise = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		switch( iNoise )
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;
		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouch"))
	{
		m_iszTouch = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouchOther"))
	{
		m_iszTouchOther = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPhysics"))
	{
		m_iPhysics = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFriction"))
	{
		m_fFriction = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_vecSize"))
	{
		UTIL_StringToVector( m_vecSize, pkvd->szValue);
		m_vecSize = m_vecSize / 2;
		pkvd->fHandled = TRUE;
	}
	else
	{
		CGibShooter::KeyValue( pkvd );
	}
}

void CEnvShooter :: Precache ( void )
{
	m_iGibModelIndex = PRECACHE_MODEL( GetModel() );

	CBreakable::MaterialSoundPrecache( (Materials)m_iGibMaterial );
}

void CEnvShooter::Spawn( void )
{
	int iBody = pev->body;
	BaseClass::Spawn();
	pev->body = iBody;
}

CBaseEntity *CEnvShooter :: CreateGib( void )
{
	if( m_iPhysics <= 1 )
	{
		// normal gib or sticky gib
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->Spawn( GetModel() );

		if( m_iPhysics == 1 )
		{
			// sticky gib
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			pGib->SetTouch( &CGib :: StickyGibTouch );
		}

		if( m_iBloodColor )
			pGib->m_bloodColor = m_iBloodColor;
		else pGib->m_bloodColor = DONT_BLEED;
		pGib->m_material = m_iGibMaterial;

		pGib->pev->rendermode = pev->rendermode;
		pGib->pev->renderamt = pev->renderamt;
		pGib->pev->rendercolor = pev->rendercolor;
		pGib->pev->renderfx = pev->renderfx;
		pGib->pev->effects = pev->effects & ~EF_NODRAW;
		pGib->pev->scale = pev->scale;
		pGib->pev->skin = pev->skin;

		// g-cont. random body select
		if( pGib->pev->body == -1 )
		{
			int numBodies = MODEL_FRAMES( pGib->pev->modelindex );
			pGib->pev->body = RANDOM_LONG( 0, numBodies - 1 );
		}
		else if( pev->body > 0 )
			pGib->pev->body = RANDOM_LONG( 0, pev->body - 1 );

		// g-cont. random skin select
		if( pGib->pev->skin == -1 )
		{
			studiohdr_t *pstudiohdr = (studiohdr_t *)(GET_MODEL_PTR( ENT(pGib->pev) ));

			// NOTE: this code works fine only under XashXT because GoldSource doesn't merge texture and model files
			// into one model_t slot. So this will not working with models which keep textures seperate
			if( pstudiohdr && pstudiohdr->numskinfamilies > 0 )
			{
				pGib->pev->skin = RANDOM_LONG( 0, pstudiohdr->numskinfamilies - 1 );
			}
			else pGib->pev->skin = 0; // reset to default
		}

		return pGib;
	}
          
	// special shot
	CShot *pShot = GetClassPtr( (CShot*)NULL );
	pShot->pev->classname = MAKE_STRING( "shot" );
	pShot->pev->solid = SOLID_SLIDEBOX;
	pShot->SetLocalAvelocity( GetLocalAvelocity( ));
	SET_MODEL( ENT( pShot->pev ), STRING( pev->model ));
	UTIL_SetSize( pShot->pev, -m_vecSize, m_vecSize );
	pShot->pev->renderamt = pev->renderamt;
	pShot->pev->rendermode = pev->rendermode;
	pShot->pev->rendercolor = pev->rendercolor;
	pShot->pev->renderfx = pev->renderfx;
	pShot->pev->netname = m_iszTouch;
	pShot->pev->message = m_iszTouchOther;
	pShot->pev->effects = pev->effects & ~EF_NODRAW;
	pShot->pev->skin = pev->skin;
	pShot->pev->body = pev->body;
	pShot->pev->scale = pev->scale;
	pShot->pev->frame = pev->frame;
	pShot->pev->framerate = pev->framerate;
	pShot->pev->friction = m_fFriction;

	// g-cont. random body select
	if( pShot->pev->body == -1 )
	{
		int numBodies = MODEL_FRAMES( pShot->pev->modelindex );
		pShot->pev->body = RANDOM_LONG( 0, numBodies - 1 );
	}
	else if( pev->body > 0 )
		pShot->pev->body = RANDOM_LONG( 0, pev->body - 1 );

	// g-cont. random skin select
	if( pShot->pev->skin == -1 )
	{
		studiohdr_t *pstudiohdr = (studiohdr_t *)(GET_MODEL_PTR( ENT(pShot->pev) ));

		// NOTE: this code works fine only under XashXT because GoldSource doesn't merge texture and model files
		// into one model_t slot. So this will not working with models which keep textures seperate
		if( pstudiohdr && pstudiohdr->numskinfamilies > 0 )
		{
			pShot->pev->skin = RANDOM_LONG( 0, pstudiohdr->numskinfamilies - 1 );
		}
		else pShot->pev->skin = 0; // reset to default
	}

	switch( m_iPhysics )
	{
	case 2:	
		pShot->pev->movetype = MOVETYPE_NOCLIP;
		pShot->pev->solid = SOLID_NOT;
		break;
	case 3:
		pShot->pev->movetype = MOVETYPE_FLYMISSILE;
		break;
	case 4:
		pShot->pev->movetype = MOVETYPE_BOUNCEMISSILE;
		break;
	case 5:
		pShot->pev->movetype = MOVETYPE_TOSS;
		break;
	case 6:
		// FIXME: tune NxGroupMask for each body to avoid collision with this particles
		// because so small bodies can be pushed through floor
		pShot->pev->solid = SOLID_NOT;
		if( WorldPhysic->Initialized( ))
			pShot->pev->movetype = MOVETYPE_PHYSIC;
		pShot->SetAbsOrigin( GetAbsOrigin( ));
		pShot->m_pUserData = WorldPhysic->CreateBodyFromEntity( pShot );
		break;
	default:
		pShot->pev->movetype = MOVETYPE_BOUNCE;
		break;
	}

	if( pShot->pev->framerate && UTIL_GetModelType( pShot->pev->modelindex ) == mod_sprite )
	{
		pShot->m_maxFrame = (float)MODEL_FRAMES( pShot->pev->modelindex ) - 1;
		if( pShot->m_maxFrame > 1.0f )
		{
			if( m_flGibLife )
			{
				pShot->pev->dmgtime = gpGlobals->time + m_flGibLife;
				pShot->SetThink( &CSprite::AnimateUntilDead );
			}
			else
			{
				pShot->SetThink( &CSprite::AnimateThink );
			}

			pShot->SetNextThink( 0 );
			pShot->m_lastTime = gpGlobals->time;
			return pShot;
		}
	}

	// if it's not animating
	if( m_flGibLife )
	{
		pShot->SetThink( &CBaseEntity::SUB_Remove );
		pShot->SetNextThink( m_flGibLife );
	}

	return pShot;
}

class CTestEffect : public CBaseDelay
{
	DECLARE_CLASS( CTestEffect, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );

	void	TestThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();

	int	m_iLoop;
	int	m_iBeam;
	CBeam	*m_pBeam[24];
	float	m_flBeamTime[24];
	float	m_flStartTime;
};

LINK_ENTITY_TO_CLASS( test_effect, CTestEffect );

BEGIN_DATADESC( CTestEffect )
	DEFINE_FUNCTION( TestThink ),
END_DATADESC()

void CTestEffect::Spawn( void )
{
	Precache( );
}

void CTestEffect::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CTestEffect::TestThink( void )
{
	int i;
	float t = (gpGlobals->time - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam *pbeam = CBeam::BeamCreate( "sprites/lgtning.spr", 100 );

		TraceResult		tr;

		Vector vecSrc = GetAbsOrigin();
		Vector vecDir = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir = vecDir.Normalize();
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * 128, ignore_monsters, ENT(pev), &tr);

		pbeam->PointsInit( vecSrc, tr.vecEndPos );
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor( 255, 180, 100 );
		pbeam->SetWidth( 100 );
		pbeam->SetScrollRate( 12 );
		
		m_flBeamTime[m_iBeam] = gpGlobals->time;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;
	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->time - m_flBeamTime[i]) / ( 3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness( 255 * t );
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		pev->nextthink = gpGlobals->time + 0.1;
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove( m_pBeam[i] );
		}
		m_flStartTime = gpGlobals->time;
		m_iBeam = 0;
		// pev->nextthink = gpGlobals->time;
		SetThink( NULL );
	}
}


void CTestEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CTestEffect::TestThink );
	pev->nextthink = gpGlobals->time + 0.1;
	m_flStartTime = gpGlobals->time;
}



// Blood effects
class CBlood : public CPointEntity
{
	DECLARE_CLASS( CBlood, CPointEntity );
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_SET_MOVEDIR; }
	void	KeyValue( KeyValueData *pkvd );

	inline	int Color( void ) { return pev->impulse; }
	inline	float BloodAmount( void ) { return pev->dmg; }

	inline	void SetColor( int color ) { pev->impulse = color; }
	inline	void SetBloodAmount( float amount ) { pev->dmg = amount; }
	
	Vector	Direction( void );
	Vector	BloodPosition( CBaseEntity *pActivator );
};

LINK_ENTITY_TO_CLASS( env_blood, CBlood );

#define SF_BLOOD_RANDOM		0x0001
#define SF_BLOOD_STREAM		0x0002
#define SF_BLOOD_PLAYER		0x0004
#define SF_BLOOD_DECAL		0x0008

void CBlood::Spawn( void )
{
	pev->solid	= SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->frame	= 0;
}

void CBlood::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color = atoi(pkvd->szValue);
		switch( color )
		{
		case 1:
			SetColor( BLOOD_COLOR_YELLOW );
			break;
		default:
			SetColor( BLOOD_COLOR_RED );
			break;
		}

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "amount"))
	{
		SetBloodAmount( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

Vector CBlood::Direction( void )
{
	if ( pev->spawnflags & SF_BLOOD_RANDOM )
		return UTIL_RandomBloodVector();
	
	return pev->movedir;
}

Vector CBlood::BloodPosition( CBaseEntity *pActivator )
{
	if ( pev->spawnflags & SF_BLOOD_PLAYER )
	{
		edict_t *pPlayer;

		if ( pActivator && pActivator->IsPlayer() )
		{
			pPlayer = pActivator->edict();
		}
		else
			pPlayer = INDEXENT( 1 );
		if ( pPlayer )
			return (pPlayer->v.origin + pPlayer->v.view_ofs) + Vector( RANDOM_FLOAT(-10,10), RANDOM_FLOAT(-10,10), RANDOM_FLOAT(-10,10) );
	}

	return GetAbsOrigin();
}


void CBlood::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_BLOOD_STREAM )
		UTIL_BloodStream( BloodPosition(pActivator), Direction(), (Color() == BLOOD_COLOR_RED) ? 70 : Color(), BloodAmount() );
	else
		UTIL_BloodDrips( BloodPosition(pActivator), Direction(), Color(), BloodAmount() );

	if ( pev->spawnflags & SF_BLOOD_DECAL )
	{
		Vector forward = Direction();
		Vector start = BloodPosition( pActivator );
		TraceResult tr;

		UTIL_TraceLine( start, start + forward * BloodAmount() * 2, ignore_monsters, NULL, &tr );
		if ( tr.flFraction != 1.0 )
			UTIL_BloodDecalTrace( &tr, Color() );
	}
}



// Screen shake
class CShake : public CPointEntity
{
	DECLARE_CLASS( CShake, CPointEntity );
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	float Amplitude( void ) { return pev->scale; }
	inline	float Frequency( void ) { return pev->dmg_save; }
	inline	float Duration( void ) { return pev->dmg_take; }
	inline	float Radius( void ) { return pev->dmg; }

	inline	void SetAmplitude( float amplitude ) { pev->scale = amplitude; }
	inline	void SetFrequency( float frequency ) { pev->dmg_save = frequency; }
	inline	void SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void SetRadius( float radius ) { pev->dmg = radius; }
private:
};

LINK_ENTITY_TO_CLASS( env_shake, CShake );

// pev->scale is amplitude
// pev->dmg_save is frequency
// pev->dmg_take is duration
// pev->dmg is radius
// radius of 0 means all players
// NOTE: UTIL_ScreenShake() will only shake players who are on the ground

#define SF_SHAKE_EVERYONE	0x0001		// Don't check radius
// UNDONE: These don't work yet
#define SF_SHAKE_DISRUPT	0x0002		// Disrupt controls
#define SF_SHAKE_INAIR		0x0004		// Shake players in air

void CShake::Spawn( void )
{
	pev->solid	= SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->frame	= 0;
	
	if ( pev->spawnflags & SF_SHAKE_EVERYONE )
		pev->dmg = 0;
}


void CShake::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "amplitude"))
	{
		SetAmplitude( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		SetFrequency( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		SetRadius( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CShake::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	UTIL_ScreenShake( GetAbsOrigin(), Amplitude(), Frequency(), Duration(), Radius(), FBitSet( pev->spawnflags, SF_SHAKE_INAIR ) ? true : false );
}


class CFade : public CPointEntity
{
	DECLARE_CLASS( CFade, CPointEntity );
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	float Duration( void ) { return pev->dmg_take; }
	inline	float HoldTime( void ) { return pev->dmg_save; }

	inline	void SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void SetHoldTime( float hold ) { pev->dmg_save = hold; }
};

LINK_ENTITY_TO_CLASS( env_fade, CFade );

// pev->dmg_take is duration
// pev->dmg_save is hold duration
#define SF_FADE_IN			0x0001		// Fade in, not out
#define SF_FADE_MODULATE		0x0002		// Modulate, don't blend
#define SF_FADE_ONLYONE		0x0004
#define SF_FADE_PERMANENT		0x0008		// LRC - hold permanently

void CFade::Spawn( void )
{
	pev->solid	= SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->frame	= 0;
}


void CFade::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CFade::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int fadeFlags = 0;
	
	if ( !(pev->spawnflags & SF_FADE_IN) )
		fadeFlags |= FFADE_OUT;

	if ( pev->spawnflags & SF_FADE_MODULATE )
		fadeFlags |= FFADE_MODULATE;

	if ( pev->spawnflags & SF_FADE_PERMANENT )	//LRC
		fadeFlags |= FFADE_STAYOUT;				//LRC

	if ( pev->spawnflags & SF_FADE_ONLYONE )
	{
		if ( pActivator->IsNetClient() )
		{
			UTIL_ScreenFade( pActivator, pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags );
		}
	}
	else
	{
		UTIL_ScreenFadeAll( pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags );
	}
	SUB_UseTargets( this, USE_TOGGLE, 0 );
}

#define SF_POSTFX_CONTROLLER_OUT		0x0001		// Fade out
#define SF_POSTFX_CONTROLLER_ONLYONE	0x0002      // Setting applies only for activator, not for all players

class CEnvPostFxController : public CPointEntity
{
	DECLARE_CLASS( CEnvPostFxController, CPointEntity );
public:
	void	Spawn();
	void	Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void	KeyValue(KeyValueData *pkvd);

private:
	DECLARE_DATADESC();

	float	m_flFadeInTime;
	float	m_flBrightness;
	float	m_flSaturation;
	float	m_flContrast;
	float	m_flRedLevel;
	float	m_flGreenLevel;
	float	m_flBlueLevel;
	float	m_flVignetteScale;
	float	m_flFilmGrainScale;
	float	m_flColorAccentScale;
	Vector	m_vecAccentColor;
};

LINK_ENTITY_TO_CLASS( env_postfx_controller, CEnvPostFxController );

BEGIN_DATADESC( CEnvPostFxController )
	DEFINE_FIELD( m_flFadeInTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBrightness, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSaturation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flContrast, FIELD_FLOAT ),
	DEFINE_FIELD( m_flRedLevel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flGreenLevel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBlueLevel, FIELD_FLOAT ),
	DEFINE_FIELD( m_flVignetteScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFilmGrainScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_flColorAccentScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecAccentColor, FIELD_VECTOR ),
END_DATADESC()

void CEnvPostFxController::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->frame = 0;
	m_vecAccentColor = Vector(
		pev->rendercolor.x / 255.f,
		pev->rendercolor.y / 255.f,
		pev->rendercolor.z / 255.f
	);
}

void CEnvPostFxController::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "brightness"))
	{
		m_flBrightness = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "saturation"))
	{
		m_flSaturation = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "contrast"))
	{
		m_flContrast = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "red_level"))
	{
		m_flRedLevel = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "green_level"))
	{
		m_flGreenLevel = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "blue_level"))
	{
		m_flBlueLevel = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "vignette_scale"))
	{
		m_flVignetteScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "film_grain_scale"))
	{
		m_flFilmGrainScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "color_accent_scale"))
	{
		m_flColorAccentScale = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fade_in_time"))
	{
		m_flFadeInTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else {
		CPointEntity::KeyValue(pkvd);
	}
}

void CEnvPostFxController::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	bool skipMessage = false;
	bool fadeOut = false;

	if (pev->spawnflags & SF_POSTFX_CONTROLLER_OUT) {
		fadeOut = true;
	}

	if (pev->spawnflags & SF_POSTFX_CONTROLLER_ONLYONE)
	{
		if (pActivator->IsNetClient()) {
			MESSAGE_BEGIN(MSG_ONE, gmsgPostFxSettings, NULL, pActivator->edict());
		}
		else {
			skipMessage = true;
		}
	}
	else {
		MESSAGE_BEGIN(MSG_ALL, gmsgPostFxSettings);
	}

	if (!skipMessage)
	{
		WRITE_FLOAT(m_flFadeInTime);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flBrightness);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flSaturation);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flContrast);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flRedLevel);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flGreenLevel);
		WRITE_FLOAT(fadeOut ? 1.0f : m_flBlueLevel);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flVignetteScale);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flFilmGrainScale);
		WRITE_FLOAT(fadeOut ? 0.0f : m_flColorAccentScale);
		WRITE_FLOAT(fadeOut ? 1.0f : m_vecAccentColor.x);
		WRITE_FLOAT(fadeOut ? 1.0f : m_vecAccentColor.y);
		WRITE_FLOAT(fadeOut ? 1.0f : m_vecAccentColor.z);
		MESSAGE_END();
	}

	SUB_UseTargets( this, USE_TOGGLE, 0 );
}

class CMessage : public CPointEntity
{
	DECLARE_CLASS( CMessage, CPointEntity );
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );
};

LINK_ENTITY_TO_CLASS( env_message, CMessage );

void CMessage::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	switch( pev->impulse )
	{
	case 1: // Medium radius
		pev->speed = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		pev->speed = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		pev->speed = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		pev->speed = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if ( pev->scale <= 0 )
		pev->scale = 1.0;
}


void CMessage::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CMessage::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "messagesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagevolume"))
	{
		pev->scale = atof(pkvd->szValue) * 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messageattenuation"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}

void CMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pPlayer = NULL;

	if ( pev->spawnflags & SF_MESSAGE_ALL )
		UTIL_ShowMessageAll( STRING(pev->message) );
	else
	{
		if ( pActivator && pActivator->IsPlayer() )
			pPlayer = pActivator;
		else
		{
			pPlayer = CBaseEntity::Instance( INDEXENT( 1 ) );
		}
		if ( pPlayer )
			UTIL_ShowMessage( STRING(pev->message), pPlayer );
	}
	if ( pev->noise )
	{
		EMIT_SOUND( edict(), CHAN_BODY, STRING(pev->noise), pev->scale, pev->speed );
	}
	if ( pev->spawnflags & SF_MESSAGE_ONCE )
		UTIL_Remove( this );

	SUB_UseTargets( this, USE_TOGGLE, 0 );
}



//=========================================================
// FunnelEffect
//=========================================================
class CEnvFunnel : public CBaseDelay
{
	DECLARE_CLASS( CEnvFunnel, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int	m_iSprite;	// Don't save, precache
};

void CEnvFunnel :: Precache ( void )
{
	if (pev->netname)
		m_iSprite = PRECACHE_MODEL ( (char*)STRING(pev->netname) );
	else
		m_iSprite = PRECACHE_MODEL ( "sprites/flare6.spr" );
}

LINK_ENTITY_TO_CLASS( env_funnel, CEnvFunnel );

void CEnvFunnel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector absOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_LARGEFUNNEL );
		WRITE_COORD( absOrigin.x );
		WRITE_COORD( absOrigin.y );
		WRITE_COORD( absOrigin.z );
		WRITE_SHORT( m_iSprite );

		if ( pev->spawnflags & SF_FUNNEL_REVERSE )// funnel flows in reverse?
		{
			WRITE_SHORT( 1 );
		}
		else
		{
			WRITE_SHORT( 0 );
		}


	MESSAGE_END();

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

void CEnvFunnel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
}

//=========================================================
// Beverage Dispenser
// overloaded pev->frags, is now a flag for whether or not a can is stuck in the dispenser. 
// overloaded pev->health, is now how many cans remain in the machine.
//=========================================================
class CEnvBeverage : public CBaseDelay
{
	DECLARE_CLASS( CEnvBeverage, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CEnvBeverage :: Precache ( void )
{
	PRECACHE_MODEL( "models/can.mdl" );
	PRECACHE_SOUND( "weapons/g_bounce3.wav" );
}

LINK_ENTITY_TO_CLASS( env_beverage, CEnvBeverage );

void CEnvBeverage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->frags != 0 || pev->health <= 0 )
	{
		// no more cans while one is waiting in the dispenser, or if I'm out of cans.
		return;
	}

	CBaseEntity *pCan = CBaseEntity::Create( "item_sodacan", GetAbsOrigin(), GetAbsAngles(), edict() );	

	if ( pev->skin == 6 )
	{
		// random
		pCan->pev->skin = RANDOM_LONG( 0, 5 );
	}
	else
	{
		pCan->pev->skin = pev->skin;
	}

	pev->frags = 1;
	pev->health--;

	//SetThink( &SUB_Remove);
	//pev->nextthink = gpGlobals->time;
}

void CEnvBeverage::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->frags = 0;

	if ( pev->health == 0 )
	{
		pev->health = 10;
	}
}

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseEntity
{
	DECLARE_CLASS( CItemSoda, CBaseEntity );
public:
	void	Spawn( void );
	void	Precache( void );
	void	CanThink ( void );
	void	CanTouch ( CBaseEntity *pOther );

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CItemSoda )
	DEFINE_FUNCTION( CanThink ),
	DEFINE_FUNCTION( CanTouch ),
END_DATADESC()

void CItemSoda :: Precache ( void )
{
	PRECACHE_MODEL( "models/can.mdl" );
}

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

void CItemSoda::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;

	if( WorldPhysic->Initialized( ))
		pev->movetype = MOVETYPE_PHYSIC;
	else pev->movetype = MOVETYPE_TOSS;

	SET_MODEL ( ENT(pev), "models/can.mdl" );
	UTIL_SetSize ( pev, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );
	
	SetThink( &CItemSoda::CanThink);
	pev->nextthink = gpGlobals->time + 0.5;

	m_pUserData = WorldPhysic->CreateBodyFromEntity( this );
}

void CItemSoda::CanThink ( void )
{
	EMIT_SOUND (ENT(pev), CHAN_WEAPON, "weapons/g_bounce3.wav", 1, ATTN_NORM );

	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize ( pev, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );
	SetThink( NULL );
	SetTouch( &CItemSoda::CanTouch );
}

void CItemSoda::CanTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth( 1, DMG_GENERIC );// a bit of health.

	if ( !FNullEnt( pev->owner ) )
	{
		// tell the machine the can was taken
		pev->owner->v.frags = 0;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

// =================== ENV_SKY ==============================================
#define SF_ENVSKY_START_OFF	BIT( 0 )

class CEnvSky : public CPointEntity
{
	DECLARE_CLASS( CEnvSky, CPointEntity );
public: 
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	inline void SetFovAngle(float angle) { pev->fuser2 = angle; }
};

LINK_ENTITY_TO_CLASS( env_sky, CEnvSky );

void CEnvSky :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "fov" ))
	{
		SetFovAngle(Q_atof( pkvd->szValue ));
		pkvd->fHandled = TRUE;
	}
	else CPointEntity::KeyValue( pkvd );
}

void CEnvSky::Spawn( void )
{
	if( !FBitSet( pev->spawnflags, SF_ENVSKY_START_OFF ))
		pev->effects |= (EF_MERGE_VISIBILITY|EF_SKYCAMERA);

	SET_MODEL( edict(), "sprites/null.spr" );
	SetBits( m_iFlags, MF_POINTENTITY );
	RelinkEntity( FALSE );
}

void CEnvSky::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int m_active = FBitSet( pev->effects, EF_SKYCAMERA );

	if ( !ShouldToggle( useType, m_active ))
		return;

	if ( m_active )
	{
		pev->effects &= ~(EF_MERGE_VISIBILITY|EF_SKYCAMERA);
	}
	else
	{
		pev->effects |= (EF_MERGE_VISIBILITY|EF_SKYCAMERA);
	}
}

// =================== ENV_PARTICLE ==============================================
#define SF_PARTICLE_START_ON		BIT( 0 )

class CBaseParticle : public CPointEntity
{
	DECLARE_CLASS( CBaseParticle, CPointEntity );
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual STATE GetState( void ) { return (pev->body != 0) ? STATE_ON : STATE_OFF; };
	void StartMessage( CBasePlayer *pPlayer );
};

LINK_ENTITY_TO_CLASS( env_particle, CBaseParticle );

void CBaseParticle :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "aurora" ))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "attachment" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pev->impulse = bound( 1, pev->impulse, 4 );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CBaseParticle :: StartMessage( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, pPlayer->pev );
		WRITE_ENTITY( entindex() );
		WRITE_STRING( STRING( pev->message ));
		WRITE_BYTE( pev->impulse );
	MESSAGE_END();
}

void CBaseParticle::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->renderamt = 128;
	pev->rendermode = kRenderTransTexture;
	pev->body = (pev->spawnflags & SF_PARTICLE_START_ON) != 0; // 'body' determines whether the effect is active or not

	SET_MODEL( edict(), "sprites/null.spr" );
	UTIL_SetSize( pev, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ));
	RelinkEntity();
}

void CBaseParticle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_TOGGLE )
	{
		if( GetState() == STATE_ON )
			useType = USE_OFF;
		else useType = USE_ON;
	}

	if( useType == USE_ON )
	{
		pev->body = 1;
	}
	else if( useType == USE_OFF )
	{
		pev->body = 0;
	}
}

// =================== ENV_COUNTER ==============================================

class CDecLED : public CBaseDelay
{
	DECLARE_CLASS( CDecLED, CBaseDelay );
public:
	void	Spawn( void );
	void	Precache( void );
	void	CheckState( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	float 	Frames( void ) { return MODEL_FRAMES( pev->modelindex ) - 1; }
	float	curframe( void ) { return Q_rint( pev->frame ); }
private:
	bool	keyframeset;
	float	flashtime;
	bool	recursive; // to prevent infinite loop with USE_RESET
};

LINK_ENTITY_TO_CLASS( env_counter, CDecLED );

void CDecLED :: Precache( void )
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("sprites/decimal.spr");
}

void CDecLED :: Spawn( void )
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "sprites/decimal.spr");
         
	// Smart Field System ©
          if( !pev->renderamt ) pev->renderamt = 200; // light transparency
	if( !pev->rendermode ) pev->rendermode = kRenderTransAdd;
	if( !pev->frags ) pev->frags = Frames();
	if( !keyframeset ) pev->impulse = -1;

	CheckState();

	SetBits( m_iFlags, MF_POINTENTITY );
	pev->effects |= EF_NOINTERP;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	Vector angles = GetLocalAngles();

	angles.x = -angles.x;
	angles.y = 180 - angles.y;
	angles.y = -angles.y;

	SetLocalAngles( angles );	
	RelinkEntity( FALSE );

	// allow to switch frames for studio models
	pev->framerate = -1.0f;
	pev->button = 1;
}

void CDecLED :: CheckState( void )
{
	switch( (int)pev->dmg )
	{
	case 1:
		if( pev->impulse >= curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	case 2:
		if( pev->impulse <= curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	default:
		if( pev->impulse == curframe() )
			m_iState = STATE_ON;
		else m_iState = STATE_OFF;
		break;
	}
}

void CDecLED :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "maxframe" ))
	{
		pev->frags = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "keyframe" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
		keyframeset = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "type" ))
	{
		pev->dmg = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CDecLED :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( useType == USE_ON || useType == USE_TOGGLE )
	{
		if( pev->frame >= pev->frags ) 
		{
			UTIL_FireTargets( pev->target, pActivator, this, useType, value );
			pev->frame = 0;
		}
		else pev->frame++;
		
		pev->button = 1;
		ClearBits( pev->effects, EF_NODRAW );
		flashtime = gpGlobals->time + 0.5f;
	}
	else if( useType == USE_OFF )
	{
		if( pev->frame <= 0 ) 
		{
         			UTIL_FireTargets( pev->target, pActivator, this, useType, value );
			pev->frame = pev->frags;
		}
		else pev->frame--;

		pev->button = 0;
		ClearBits( pev->effects, EF_NODRAW );
		flashtime = gpGlobals->time + 0.5f;
	}
	else if( useType == USE_SET ) // set custom frame
	{ 
		if( value != 0.0f )
		{
			if( value == -1 ) value = 0; // reset frame
			pev->frame = fmod( value, pev->frags + 1 );
                             	float next = value / ( pev->frags + 1 );
                             	if( next ) UTIL_FireTargets( pev->target, pActivator, this, useType, next );
	         		ClearBits( pev->effects, EF_NODRAW );
			flashtime = gpGlobals->time + 0.5f;
			pev->button = 1;
		}
		else if( gpGlobals->time > flashtime )
		{
			if( pev->button )
			{
				SetBits( pev->effects, EF_NODRAW );
				pev->button = 0;
			}
			else
			{
				ClearBits( pev->effects, EF_NODRAW );
				pev->button = 1;
			}
		}
	}
	else if( useType == USE_RESET ) // immediately reset
	{ 
		pev->frame = 0;
		pev->button = 1;
		pev->effects &= ~EF_NODRAW;

		if( !recursive )
		{
			recursive = TRUE;
			UTIL_FireTargets( pev->target, pActivator, this, useType, 0 );
			recursive = FALSE;
		}
		else ALERT( at_error, "%s has infinite loop in chain\n", STRING( pev->classname ));
	}

	// check state after changing
	CheckState();
}

//=================================================================
// env_model: like env_sprite, except you can specify a sequence.
//=================================================================
#define SF_ENVMODEL_OFF				1
#define SF_ENVMODEL_DROPTOFLOOR		2
#define SF_ENVMODEL_SOLID			4
#define SF_ENVMODEL_NEWLIGHTING		8
#define SF_ENVMODEL_NODYNSHADOWS	16

class CEnvModel : public CBaseAnimating
{
	DECLARE_CLASS(CEnvModel, CBaseAnimating);
public:
	void Spawn();
	void Precache();
	void EXPORT Think();
	void KeyValue(KeyValueData *pkvd);
	STATE GetState();
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	virtual int	ObjectCaps() { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetSequence();
	void AutoSetSize();

	DECLARE_DATADESC();

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;
};

LINK_ENTITY_TO_CLASS(env_model, CEnvModel);

BEGIN_DATADESC(CEnvModel)
	DEFINE_KEYFIELD(m_iszSequence_On, FIELD_STRING, "m_iszSequence_On"),
	DEFINE_KEYFIELD(m_iszSequence_Off, FIELD_STRING, "m_iszSequence_Off"),
	DEFINE_KEYFIELD(m_iAction_On, FIELD_INTEGER, "m_iAction_On"),
	DEFINE_KEYFIELD(m_iAction_Off, FIELD_INTEGER, "m_iAction_Off"),
END_DATADESC()

void CEnvModel::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszSequence_On"))
	{
		m_iszSequence_On = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSequence_Off"))
	{
		m_iszSequence_Off = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_On"))
	{
		m_iAction_On = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_Off"))
	{
		m_iAction_Off = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue(pkvd);
	}
}

void CEnvModel::Spawn(void)
{
	Precache();
	SET_MODEL(edict(), STRING(pev->model));
	RelinkEntity(true);

	SetBoneController(0, 0);
	SetBoneController(1, 0);
	SetSequence();

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_SOLID))
	{
		if (UTIL_AllowHitboxTrace(this))
			pev->solid = SOLID_BBOX;
		else 
			pev->solid = SOLID_SLIDEBOX;
		AutoSetSize();
	}

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_DROPTOFLOOR))
	{
		Vector origin = GetLocalOrigin();
		origin.z += 1;
		SetLocalOrigin(origin);
		UTIL_DropToFloor(this);
	}

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_NEWLIGHTING) && !m_hParent.Get())
	{
		// tell the client about static entity
		SetBits(pev->iuser1, CF_STATIC_ENTITY);
	}

	if (FBitSet(pev->spawnflags, SF_ENVMODEL_NODYNSHADOWS))
	{
		SetBits(pev->effects, EF_NOSHADOW);
	}

	SetNextThink(0.1);
}

void CEnvModel::Precache(void)
{
	PRECACHE_MODEL(GetModel());
}

STATE CEnvModel::GetState(void)
{
	if (pev->spawnflags & SF_ENVMODEL_OFF)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CEnvModel::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (ShouldToggle(useType, !(pev->spawnflags & SF_ENVMODEL_OFF)))
	{
		if (pev->spawnflags & SF_ENVMODEL_OFF)
			pev->spawnflags &= ~SF_ENVMODEL_OFF;
		else
			pev->spawnflags |= SF_ENVMODEL_OFF;

		SetSequence();
		SetNextThink(0.1);
	}
}

void CEnvModel::Think(void)
{
	int iTemp;

	//	ALERT(at_console, "env_model Think fr=%f\n", pev->framerate);

	StudioFrameAdvance(); // set m_fSequenceFinished if necessary

//	if (m_fSequenceLoops)
//	{
//		SetNextThink( 1E6 );
//		return; // our work here is done.
//	}
	if (m_fSequenceFinished && !m_fSequenceLoops)
	{
		if (pev->spawnflags & SF_ENVMODEL_OFF)
			iTemp = m_iAction_Off;
		else
			iTemp = m_iAction_On;

		switch (iTemp)
		{
			//		case 1: // loop
			//			pev->animtime = gpGlobals->time;
			//			m_fSequenceFinished = FALSE;
			//			m_flLastEventCheck = gpGlobals->time;
			//			pev->frame = 0;
			//			break;
			case 2: // change state
				if (pev->spawnflags & SF_ENVMODEL_OFF)
					pev->spawnflags &= ~SF_ENVMODEL_OFF;
				else
					pev->spawnflags |= SF_ENVMODEL_OFF;
				SetSequence();
				break;
			default: //remain frozen
				return;
		}
	}
	SetNextThink(0.1);
}

void CEnvModel::SetSequence(void)
{
	int iszSequence;

	if (pev->spawnflags & SF_ENVMODEL_OFF)
		iszSequence = m_iszSequence_Off;
	else
		iszSequence = m_iszSequence_On;

	if (!iszSequence) {
		pev->sequence = 0;
	}
	else {
		pev->sequence = LookupSequence(STRING(iszSequence));
	}

	if (pev->sequence == -1)
	{
		if (pev->targetname)
			ALERT(at_error, "env_model %s: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSequence));
		else
			ALERT(at_error, "env_model: unknown sequence \"%s\"\n", STRING(pev->targetname), STRING(iszSequence));
		pev->sequence = 0;
	}

	pev->frame = 0;
	ResetSequenceInfo();

	if (pev->spawnflags & SF_ENVMODEL_OFF)
	{
		if (m_iAction_Off == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
	else
	{
		if (m_iAction_On == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
}

// automatically set collision box
void CEnvModel::AutoSetSize()
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR(edict());

	if (pstudiohdr == NULL)
	{
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));
		ALERT(at_error, "CEnvModel::AutoSetSize: unable to fetch model pointer\n");
		return;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize(pev, pseqdesc[pev->sequence].bbmin, pseqdesc[pev->sequence].bbmax);
}

// =================== ENV_STATIC ==============================================
#define SF_STATIC_SOLID			BIT(0)
#define SF_STATIC_DROPTOFLOOR	BIT(1)
#define SF_STATIC_NOSHADOW		BIT(2)	// reserved for pxrad, disables shadows on lightmap
#define SF_STATIC_NOVLIGHT		BIT(4)	// reserved for pxrad, disables vertex lighting
#define SF_STATIC_NODYNSHADOWS	BIT(5)

class CEnvStatic : public CBaseEntity
{
	DECLARE_CLASS( CEnvStatic, CBaseEntity );
public:
	void Spawn( void );
	void Precache( void );
	void AutoSetSize( void );
	virtual int ObjectCaps( void ) { return FCAP_IGNORE_PARENT; }
	void SetObjectCollisionBox( void );
	void KeyValue( KeyValueData *pkvd );

	int m_iVertexLightCache;
	int m_iSurfaceLightCache;
};

LINK_ENTITY_TO_CLASS( env_static, CEnvStatic );

void CEnvStatic :: Precache( void )
{
	PRECACHE_MODEL( GetModel() );
}

void CEnvStatic :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq(pkvd->szKeyName, "xform"))
	{
		UTIL_StringToVector( (float*)pev->startpos, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "vlight_cache"))
	{
		m_iVertexLightCache = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "flight_cache"))
	{
		m_iSurfaceLightCache = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue( pkvd );
}

void CEnvStatic :: Spawn( void )
{
	if( pev->startpos == g_vecZero )
		pev->startpos = Vector( pev->scale, pev->scale, pev->scale );

	if (m_iSurfaceLightCache)
	{
		SetBits(pev->iuser1, CF_STATIC_LIGHTMAPPED);
		pev->colormap = m_iSurfaceLightCache;
	}
	else
	{
		pev->colormap = m_iVertexLightCache;
	}

	// check xform values
	if( pev->startpos.x < 0.01f ) pev->startpos.x = 1.0f;
	if( pev->startpos.y < 0.01f ) pev->startpos.y = 1.0f;
	if( pev->startpos.z < 0.01f ) pev->startpos.z = 1.0f;
	if( pev->startpos.x > 16.0f ) pev->startpos.x = 16.0f;
	if( pev->startpos.y > 16.0f ) pev->startpos.y = 16.0f;
	if( pev->startpos.z > 16.0f ) pev->startpos.z = 16.0f;

	Precache();
	SET_MODEL( edict(), GetModel() );

	// tell the client about static entity
	SetBits( pev->iuser1, CF_STATIC_ENTITY );

	if( FBitSet( pev->spawnflags, SF_STATIC_SOLID ))
	{
		if( WorldPhysic->Initialized( ))
			pev->solid = SOLID_CUSTOM;
		pev->movetype = MOVETYPE_NONE;
		AutoSetSize();
	}

	if( FBitSet( pev->spawnflags, SF_STATIC_DROPTOFLOOR ))
	{
		Vector origin = GetLocalOrigin();
		origin.z += 1;
		SetLocalOrigin( origin );
		UTIL_DropToFloor( this );
	}
	else
	{
		UTIL_SetOrigin( this, GetLocalOrigin( ));
	}

	if (FBitSet(pev->spawnflags, SF_STATIC_NODYNSHADOWS)) {
		SetBits(pev->effects, EF_NOSHADOW);
	}

	if( FBitSet( pev->spawnflags, SF_STATIC_SOLID ))
	{
		m_pUserData = WorldPhysic->CreateStaticBodyFromEntity( this );
		RelinkEntity( TRUE );
	}
	else
	{
		// remove from server
		MAKE_STATIC( edict() );
	}
}

void CEnvStatic :: SetObjectCollisionBox( void )
{
	// expand for rotation
	TransformAABB( EntityToWorldTransform(), pev->mins, pev->maxs, pev->absmin, pev->absmax );

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}

// automatically set collision box
void CEnvStatic :: AutoSetSize( void )
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( edict() );

	if( pstudiohdr == NULL )
	{
		UTIL_SetSize( pev, Vector( -10, -10, -10 ), Vector( 10, 10, 10 ));
		ALERT( at_error, "env_model: unable to fetch model pointer!\n" );
		return;
	}

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize( pev, pseqdesc[pev->sequence].bbmin * pev->startpos, pseqdesc[pev->sequence].bbmax * pev->startpos );
}

class CStaticDecal : public CPointEntity
{
public:
	void KeyValue(KeyValueData *pkvd)
	{
		if (FStrEq(pkvd->szKeyName, "texture"))
		{
			pev->netname = ALLOC_STRING(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else CBaseEntity::KeyValue(pkvd);
	}

	int PasteDecal(int dir)
	{
		TraceResult tr;
		Vector vecdir = g_vecZero;
		vecdir[dir % 3] = (dir & 1) ? 32.0f : -32.0f;
		UTIL_TraceLine(pev->origin, pev->origin + vecdir, ignore_monsters, edict(), &tr);
		return UTIL_TraceCustomDecal(&tr, STRING(pev->netname), pev->angles.y, TRUE);
	}

	void Spawn(void)
	{
		if (pev->skin <= 0 || pev->skin > 6)
		{
			// try all directions
			int i;
			for (i = 0; i < 6; i++)
			{
				if (PasteDecal(i)) 
					break;
			}
			if (i == 6) ALERT(at_warning, "failed to place decal %s\n", STRING(pev->netname));
		}
		else
		{
			// try specified direction
			PasteDecal(pev->skin - 1);
		}

		// NOTE: don't need to keep this entity
		// with new custom decal save\restore system
		UTIL_Remove(this);
	}
};

LINK_ENTITY_TO_CLASS(env_static_decal, CStaticDecal);
//LINK_ENTITY_TO_CLASS(infodecal, CStaticDecal);	// now an alias

#define SF_REMOVE_ON_FIRE		0x0001
#define SF_KILL_CENTER		0x0002

class CEnvWarpBall : public CBaseEntity
{
	DECLARE_CLASS( CEnvWarpBall, CBaseEntity );
public:
	void Precache( void );
	void Spawn( void ) { Precache(); }
	void Think( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return BaseClass :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	Vector vecOrigin;
};

LINK_ENTITY_TO_CLASS( env_warpball, CEnvWarpBall );

void CEnvWarpBall :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "radius"))
	{
		pev->button = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "warp_target"))
	{
		pev->message = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "damage_delay"))
	{
		pev->frags = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvWarpBall::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_MODEL( "sprites/Fexplo1.spr" );
	PRECACHE_SOUND( "debris/beamstart2.wav" );
	PRECACHE_SOUND( "debris/beamstart7.wav" );
}

void CEnvWarpBall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam *pBeam;
	CBaseEntity *pEntity = UTIL_FindEntityByTargetname ( NULL, STRING(pev->message));
	edict_t *pos;
		
	if( pEntity ) // target found ?
	{
		vecOrigin = pEntity->GetAbsOrigin();
		pos = pEntity->edict();
	}
	else
	{         //use as center
		vecOrigin = GetAbsOrigin();
		pos = edict();
	}	

	EMIT_SOUND( pos, CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM );
	UTIL_ScreenShake( vecOrigin, 6, 160, 1.0, pev->button );
	CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", vecOrigin, TRUE );
	pSpr->SetParent( this );
	pSpr->AnimateAndDie( 18 );
	pSpr->SetTransparency(kRenderGlow,  77, 210, 130,  255, kRenderFxNoDissipation);
	EMIT_SOUND( pos, CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM );
	int iBeams = RANDOM_LONG(20, 40);
	while (iDrawn < iBeams && iTimes < (iBeams * 3))
	{
		vecDest = pev->button * (Vector(RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1)).Normalize());
		UTIL_TraceLine( vecOrigin, vecOrigin + vecDest, ignore_monsters, NULL, &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate("sprites/lgtning.spr", 200);
			pBeam->PointsInit( vecOrigin, tr.vecEndPos );
			pBeam->SetColor( 20, 243, 20 );
			pBeam->SetNoise( 65 );
			pBeam->SetBrightness( 220 );
			pBeam->SetWidth( 30 );
			pBeam->SetScrollRate( 35 );
//			pBeam->SetParent( this );
			pBeam->SetThink( &CBeam:: SUB_Remove );
			pBeam->pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.5, 1.6);
		}
		iTimes++;
	}
	pev->nextthink = gpGlobals->time + pev->frags;
}

void CEnvWarpBall::Think( void )
{
	SUB_UseTargets( this, USE_TOGGLE, 0);
 
	if ( pev->spawnflags & SF_KILL_CENTER )
	{
		CBaseEntity *pMonster = NULL;

		while ((pMonster = UTIL_FindEntityInSphere( pMonster, vecOrigin, 72 )) != NULL)
		{
			if ( FBitSet( pMonster->pev->flags, FL_MONSTER ) || FClassnameIs( pMonster->pev, "player"))
				pMonster->TakeDamage ( pev, pev, 100, DMG_GENERIC );
		}
	}
	if ( pev->spawnflags & SF_REMOVE_ON_FIRE ) UTIL_Remove( this );
}

// =================== ENV_RAIN ==============================================

class CEnvRain : public CPointEntity
{
	DECLARE_CLASS( CEnvRain, CPointEntity );
public:
	void KeyValue( KeyValueData *pkvd )
	{
		if( FStrEq( pkvd->szKeyName, "m_flDistance" ))
		{
			pev->frags = Q_atof( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else if( FStrEq( pkvd->szKeyName, "m_iMode" ))
		{
			pev->impulse = Q_atoi( pkvd->szValue );
			pkvd->fHandled = TRUE;
		}
		else
		{
			CPointEntity::KeyValue( pkvd );
		}
	}
};

LINK_ENTITY_TO_CLASS( env_rain, CEnvRain );

// =================== ENV_RAINMODIFY ==============================================

#define SF_RAIN_CONSTANT	BIT( 0 )

class CEnvRainModify : public CPointEntity
{
	DECLARE_CLASS( CEnvRainModify, CPointEntity );
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_rainmodify, CEnvRainModify );

void CEnvRainModify :: Spawn( void )
{
	if( g_pGameRules->IsMultiplayer() || FStringNull( pev->targetname ))
		SetBits( pev->spawnflags, SF_RAIN_CONSTANT );
}

void CEnvRainModify :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "m_iDripsPerSecond" ))
	{
		pev->impulse = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flWindX" ))
	{
		pev->fuser1 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flWindY" ))
	{
		pev->fuser2 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flRandX" ))
	{
		pev->fuser3 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flRandY" ))
	{
		pev->fuser4 = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if( FStrEq( pkvd->szKeyName, "m_flTime" ))
	{
		pev->dmg = Q_atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	
	{
		BaseClass::KeyValue( pkvd );
	}
}

void CEnvRainModify :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( FBitSet( pev->spawnflags, SF_RAIN_CONSTANT ))
		return; // constant

	CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(1);

	if( pev->dmg )
	{
		// write to 'ideal' settings
		pPlayer->m_iRainIdealDripsPerSecond = pev->impulse;
		pPlayer->m_flRainIdealRandX = pev->fuser3;
		pPlayer->m_flRainIdealRandY = pev->fuser4;
		pPlayer->m_flRainIdealWindX = pev->fuser1;
		pPlayer->m_flRainIdealWindY = pev->fuser2;

		pPlayer->m_flRainEndFade = gpGlobals->time + pev->dmg;
		pPlayer->m_flRainNextFadeUpdate = gpGlobals->time + 1.0f;
	}
	else
	{
		pPlayer->m_iRainDripsPerSecond = pev->impulse;
		pPlayer->m_flRainRandX = pev->fuser3;
		pPlayer->m_flRainRandY = pev->fuser4;
		pPlayer->m_flRainWindX = pev->fuser1;
		pPlayer->m_flRainWindY = pev->fuser2;

		pPlayer->m_bRainNeedsUpdate = true;
	}
}
