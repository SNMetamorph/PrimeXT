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

#include "env_laser.h"
#include "env_sprite.h"
#include "customentity.h"

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
