/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#include "nihilanth_energy_ball.h"

LINK_ENTITY_TO_CLASS( nihilanth_energy_ball, CNihilanthHVR );

BEGIN_DATADESC( CNihilanthHVR )
	DEFINE_FIELD( m_flIdealVel, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecIdeal, FIELD_VECTOR ),
	DEFINE_FIELD( m_pNihilanth, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_hTouch, FIELD_EHANDLE ),
	DEFINE_FIELD( m_nFrames, FIELD_INTEGER ),
	DEFINE_FUNCTION( HoverThink ),
	DEFINE_FUNCTION( DissipateThink ),
	DEFINE_FUNCTION( ZapThink ),
	DEFINE_FUNCTION( TeleportThink ),
	DEFINE_FUNCTION( TeleportTouch ),
	DEFINE_FUNCTION( RemoveTouch ),
	DEFINE_FUNCTION( BounceTouch ),
	DEFINE_FUNCTION( ZapTouch ),
END_DATADESC()

//=========================================================
// Controller bouncy ball attack
//=========================================================



void CNihilanthHVR :: Spawn( void )
{
	Precache( );

	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->scale = 3.0;
}


void CNihilanthHVR :: Precache( void )
{
	PRECACHE_MODEL("sprites/flare6.spr");
	PRECACHE_MODEL("sprites/nhth1.spr");
	PRECACHE_MODEL("sprites/exit1.spr");
	PRECACHE_MODEL("sprites/tele1.spr");
	PRECACHE_MODEL("sprites/animglow01.spr");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_MODEL("sprites/muzzleflash3.spr");
	PRECACHE_SOUND("debris/zap4.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("x/x_teleattack1.wav");
}



void CNihilanthHVR :: CircleInit( CBaseEntity *pTarget )
{
	pev->movetype = MOVETYPE_NOCLIP;
	pev->solid = SOLID_NOT;

	// SET_MODEL(edict(), "sprites/flare6.spr");
	// pev->scale = 3.0;
	// SET_MODEL(edict(), "sprites/xspark4.spr");
	SET_MODEL(edict(), "sprites/muzzleflash3.spr");
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 224;
	pev->rendercolor.z = 192;
	pev->scale = 2.0;
	m_nFrames = 1;
	pev->renderamt = 255;

	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( this, GetAbsOrigin() );

	SetThink( &CNihilanthHVR::HoverThink );
	SetTouch( &CNihilanthHVR::BounceTouch );
	pev->nextthink = gpGlobals->time + 0.1;
	
	m_hTargetEnt = pTarget;
}


CBaseEntity *CNihilanthHVR::RandomClassname( const char *szName )
{
	int total = 0;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;
	while ((pNewEntity = UTIL_FindEntityByClassname( pNewEntity, szName )) != NULL)
	{
		total++;
		if (RANDOM_LONG(0,total-1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
}

void CNihilanthHVR :: HoverThink( void  )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (m_hTargetEnt != NULL)
	{
		CircleTarget( m_hTargetEnt->GetAbsOrigin() + Vector( 0, 0, 16 * N_SCALE ) );
	}
	else
	{
		UTIL_Remove( this );
	}


	if (RANDOM_LONG( 0, 99 ) < 5)
	{
/*
		CBaseEntity *pOther = RandomClassname( STRING(pev->classname) );

		if (pOther && pOther != this)
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMENTS );
				WRITE_SHORT( this->entindex() );
				WRITE_SHORT( pOther->entindex() );
				WRITE_SHORT( g_sModelIndexLaser );
				WRITE_BYTE( 0 ); // framestart
				WRITE_BYTE( 0 ); // framerate
				WRITE_BYTE( 10 ); // life
				WRITE_BYTE( 80 );  // width
				WRITE_BYTE( 80 );   // noise
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 128 );   // r, g, b
				WRITE_BYTE( 64 );   // r, g, b
				WRITE_BYTE( 255 );	// brightness
				WRITE_BYTE( 30 );		// speed
			MESSAGE_END();
		}
*/
/*
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMENTS );
			WRITE_SHORT( this->entindex() );
			WRITE_SHORT( m_hTargetEnt->entindex() + 0x1000 );
			WRITE_SHORT( g_sModelIndexLaser );
			WRITE_BYTE( 0 ); // framestart
			WRITE_BYTE( 0 ); // framerate
			WRITE_BYTE( 10 ); // life
			WRITE_BYTE( 80 );  // width
			WRITE_BYTE( 80 );   // noise
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 128 );   // r, g, b
			WRITE_BYTE( 64 );   // r, g, b
			WRITE_BYTE( 255 );	// brightness
			WRITE_BYTE( 30 );		// speed
		MESSAGE_END();
*/
	}

	pev->frame = ((int)pev->frame + 1) % m_nFrames;
}




void CNihilanthHVR :: ZapInit( CBaseEntity *pEnemy )
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL( edict(), "sprites/nhth1.spr" );

	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->scale = 2.0;

	SetAbsVelocity( (pEnemy->GetAbsOrigin() - GetAbsOrigin()).Normalize() * 200 );

	m_hEnemy = pEnemy;
	SetThink( &CNihilanthHVR::ZapThink );
	SetTouch( &CNihilanthHVR::ZapTouch );
	SetNextThink( 0.1 );

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "debris/zap4.wav", 1, ATTN_NORM, 0, 100 );
}

void CNihilanthHVR :: ZapThink( void  )
{
	SetNextThink( 0.05 );

	// check world boundaries
	if (m_hEnemy == NULL || !IsInWorld( FALSE ))
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

	if (GetAbsVelocity().Length() < 2000)
	{
		SetAbsVelocity( GetAbsVelocity() * 1.2 );
	}


	// MovetoTarget( m_hEnemy->Center( ) );

	if ((m_hEnemy->Center() - GetAbsOrigin()).Length() < 256)
	{
		TraceResult tr;

		UTIL_TraceLine( GetAbsOrigin(), m_hEnemy->Center(), dont_ignore_monsters, edict(), &tr );

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity != NULL && pEntity->pev->takedamage)
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.nihilanthZap, GetAbsVelocity(), &tr, DMG_SHOCK );
			ApplyMultiDamage( pev, pev );
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMENTPOINT );
			WRITE_SHORT( entindex() );
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_SHORT( g_sModelIndexLaser );
			WRITE_BYTE( 0 ); // frame start
			WRITE_BYTE( 10 ); // framerate
			WRITE_BYTE( 3 ); // life
			WRITE_BYTE( 20 );  // width
			WRITE_BYTE( 20 );   // noise
			WRITE_BYTE( 64 );   // r, g, b
			WRITE_BYTE( 196 );   // r, g, b
			WRITE_BYTE( 255);   // r, g, b
			WRITE_BYTE( 255 );	// brightness
			WRITE_BYTE( 10 );		// speed
		MESSAGE_END();

		UTIL_EmitAmbientSound( edict(), tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );

		SetTouch( NULL );
		UTIL_Remove( this );
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	pev->frame = (int)(pev->frame + 1) % 11;
	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( entindex( ) );		// entity, attachment
		WRITE_COORD( vecOrigin.x );		// origin
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( 128 );	// radius
		WRITE_BYTE( 128 );	// R
		WRITE_BYTE( 128 );	// G
		WRITE_BYTE( 255 );	// B
		WRITE_BYTE( 10 );	// life * 10
		WRITE_COORD( 128 ); // decay
	MESSAGE_END();

	// Crawl( );
}


void CNihilanthHVR::ZapTouch( CBaseEntity *pOther )
{
	UTIL_EmitAmbientSound( edict(), GetAbsOrigin(), "weapons/electro4.wav", 1.0, ATTN_NORM, 0, RANDOM_LONG( 90, 95 ) );

	RadiusDamage( pev, pev, 50, CLASS_NONE, DMG_SHOCK );
	SetAbsVelocity( g_vecZero );

	SetTouch( NULL );
	UTIL_Remove( this );
	SetNextThink( 0.2 );
}

void CNihilanthHVR::TeleportInit( CNihilanth *pOwner, CBaseEntity *pEnemy, CBaseEntity *pTarget, CBaseEntity *pTouch )
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;

	Vector vecVelocity = GetAbsVelocity();
	vecVelocity.z *= 0.2f;
	SetAbsVelocity( vecVelocity );

	SET_MODEL(edict(), "sprites/exit1.spr");

	m_pNihilanth = pOwner;
	m_hEnemy = pEnemy;
	m_hTargetEnt = pTarget;
	m_hTouch = pTouch;

	SetThink( &CNihilanthHVR::TeleportThink );
	SetTouch( &CNihilanthHVR::TeleportTouch );
	SetNextThink( 0.1 );

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "x/x_teleattack1.wav", 1, 0.2, 0, 100 );
}


void CNihilanthHVR :: GreenBallInit( )
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->scale = 1.0;

	SET_MODEL(edict(), "sprites/exit1.spr");

	SetTouch( &CNihilanthHVR::RemoveTouch );
}


void CNihilanthHVR :: TeleportThink( void  )
{
	pev->nextthink = gpGlobals->time + 0.1;

	// check world boundaries
	if (m_hEnemy == NULL || !m_hEnemy->IsAlive() || !IsInWorld( FALSE ))
	{
		STOP_SOUND(edict(), CHAN_WEAPON, "x/x_teleattack1.wav" );
		UTIL_Remove( this );
		return;
	}

	if ((m_hEnemy->Center() - GetAbsOrigin()).Length() < 128)
	{
		STOP_SOUND(edict(), CHAN_WEAPON, "x/x_teleattack1.wav" );
		UTIL_Remove( this );

		if (m_hTargetEnt != NULL)
			m_hTargetEnt->Use( m_hEnemy, m_hEnemy, USE_ON, 1.0 );

		if ( m_hTouch != NULL && m_hEnemy != NULL )
			m_hTouch->Touch( m_hEnemy );
	}
	else 
	{
		MovetoTarget( m_hEnemy->Center( ) );
	}

	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( entindex( ) );		// entity, attachment
		WRITE_COORD( vecOrigin.x );		// origin
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( 256 );	// radius
		WRITE_BYTE( 0 );	// R
		WRITE_BYTE( 255 );	// G
		WRITE_BYTE( 0 );	// B
		WRITE_BYTE( 10 );	// life * 10
		WRITE_COORD( 256 ); // decay
	MESSAGE_END();

	pev->frame = (int)(pev->frame + 1) % 20;
}


void CNihilanthHVR :: AbsorbInit( void  )
{
	SetThink( &CNihilanthHVR::DissipateThink );
	pev->renderamt = 255;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMENTS );
		WRITE_SHORT( this->entindex() );
		WRITE_SHORT( m_hTargetEnt->entindex() + 0x1000 );
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 ); // framestart
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 50 ); // life
		WRITE_BYTE( 80 );  // width
		WRITE_BYTE( 80 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 128 );   // r, g, b
		WRITE_BYTE( 64 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 30 );		// speed
	MESSAGE_END();
}

void CNihilanthHVR::TeleportTouch( CBaseEntity *pOther )
{
	CBaseEntity *pEnemy = m_hEnemy;

	if (pOther == pEnemy)
	{
		if (m_hTargetEnt != NULL)
			m_hTargetEnt->Use( pEnemy, pEnemy, USE_ON, 1.0 );

		if (m_hTouch != NULL && pEnemy != NULL )
			m_hTouch->Touch( pEnemy );
	}
	else
	{
		m_pNihilanth->MakeFriend( GetAbsOrigin() );
	}

	SetTouch( NULL );
	STOP_SOUND(edict(), CHAN_WEAPON, "x/x_teleattack1.wav" );
	UTIL_Remove( this );
}


void CNihilanthHVR :: DissipateThink( void  )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if (pev->scale > 5.0)
		UTIL_Remove( this );

	pev->renderamt -= 2;
	pev->scale += 0.1;

	if (m_hTargetEnt != NULL)
	{
		CircleTarget( m_hTargetEnt->GetAbsOrigin() + Vector( 0, 0, 4096 ) );
	}
	else
	{
		UTIL_Remove( this );
	}

	Vector vecOrigin = GetAbsOrigin();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( entindex( ) );		// entity, attachment
		WRITE_COORD( vecOrigin.x );		// origin
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( pev->renderamt );	// radius
		WRITE_BYTE( 255 );	// R
		WRITE_BYTE( 192 );	// G
		WRITE_BYTE( 64 );	// B
		WRITE_BYTE( 2 );	// life * 10
		WRITE_COORD( 0 ); // decay
	MESSAGE_END();
}


BOOL CNihilanthHVR :: CircleTarget( Vector vecTarget )
{
	BOOL fClose = FALSE;

	Vector vecDest = vecTarget;
	Vector vecEst = GetAbsOrigin() + GetAbsVelocity() * 0.5f;
	Vector vecSrc = GetAbsOrigin();
	vecDest.z = 0;
	vecEst.z = 0;
	vecSrc.z = 0;
	float d1 = (vecDest - vecSrc).Length() - 24 * N_SCALE;
	float d2 = (vecDest - vecEst).Length() - 24 * N_SCALE;

	if (m_vecIdeal == g_vecZero)
	{
		m_vecIdeal = GetAbsVelocity();
	}

	if (d1 < 0 && d2 <= d1)
	{
		// ALERT( at_console, "too close\n");
		m_vecIdeal = m_vecIdeal - (vecDest - vecSrc).Normalize() * 50;
	}
	else if (d1 > 0 && d2 >= d1)
	{
		// ALERT( at_console, "too far\n");
		m_vecIdeal = m_vecIdeal + (vecDest - vecSrc).Normalize() * 50;
	}

	SetLocalAvelocity( Vector( 0, 0, d1 * 20 ));

	if (d1 < 32)
	{
		fClose = TRUE;
	}

	m_vecIdeal = m_vecIdeal + Vector( RANDOM_FLOAT( -2, 2 ), RANDOM_FLOAT( -2, 2 ), RANDOM_FLOAT( -2, 2 ));
	m_vecIdeal = Vector( m_vecIdeal.x, m_vecIdeal.y, 0 ).Normalize( ) * 200
		/* + Vector( -m_vecIdeal.y, m_vecIdeal.x, 0 ).Normalize( ) * 32 */
		+ Vector( 0, 0, m_vecIdeal.z );
	// m_vecIdeal = m_vecIdeal + Vector( -m_vecIdeal.y, m_vecIdeal.x, 0 ).Normalize( ) * 2;

	// move up/down
	d1 = vecTarget.z - GetAbsOrigin().z;
	if (d1 > 0 && m_vecIdeal.z < 200)
		m_vecIdeal.z += 20;
	else if (d1 < 0 && m_vecIdeal.z > -200)
		m_vecIdeal.z -= 20;

	SetAbsVelocity( m_vecIdeal );

	// ALERT( at_console, "%.0f %.0f %.0f\n", m_vecIdeal.x, m_vecIdeal.y, m_vecIdeal.z );
	return fClose;
}


void CNihilanthHVR :: MovetoTarget( Vector vecTarget )
{
	if (m_vecIdeal == g_vecZero)
	{
		m_vecIdeal = GetAbsVelocity();
	}

	// accelerate
	float flSpeed = m_vecIdeal.Length();
	if (flSpeed > 300)
	{
		m_vecIdeal = m_vecIdeal.Normalize( ) * 300;
	}
	m_vecIdeal = m_vecIdeal + (vecTarget - GetAbsOrigin()).Normalize() * 300;
	SetAbsVelocity( m_vecIdeal );
}

void CNihilanthHVR :: Crawl( void  )
{
	Vector vecAim = Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize( );
	Vector vecPnt = GetAbsOrigin() + GetAbsVelocity() * 0.2 + vecAim * 128;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMENTPOINT );
		WRITE_SHORT( entindex() );
		WRITE_COORD( vecPnt.x);
		WRITE_COORD( vecPnt.y);
		WRITE_COORD( vecPnt.z);
		WRITE_SHORT( g_sModelIndexLaser );
		WRITE_BYTE( 0 ); // frame start
		WRITE_BYTE( 10 ); // framerate
		WRITE_BYTE( 3 ); // life
		WRITE_BYTE( 20 );  // width
		WRITE_BYTE( 80 );   // noise
		WRITE_BYTE( 64 );   // r, g, b
		WRITE_BYTE( 128 );   // r, g, b
		WRITE_BYTE( 255);   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 10 );		// speed
	MESSAGE_END();
}


void CNihilanthHVR::RemoveTouch( CBaseEntity *pOther )
{
	STOP_SOUND(edict(), CHAN_WEAPON, "x/x_teleattack1.wav" );
	UTIL_Remove( this );
}

void CNihilanthHVR::BounceTouch( CBaseEntity *pOther )
{
	Vector vecDir = m_vecIdeal.Normalize( );

	TraceResult tr = UTIL_GetGlobalTrace( );

	float n = -DotProduct(tr.vecPlaneNormal, vecDir);

	vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;

	m_vecIdeal = vecDir * m_vecIdeal.Length();
}
