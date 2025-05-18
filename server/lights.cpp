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
/*

===== lights.cpp ========================================================

  spawn and think functions for editor-placed lights

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CLight : public CPointEntity
{
	DECLARE_CLASS( CLight, CPointEntity );
public:
	void	KeyValue( KeyValueData* pkvd ); 
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );

	DECLARE_DATADESC();

	int ObjectCaps( void ) { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_NOT_MASTER | FCAP_IGNORE_PARENT; }
	virtual STATE GetState( void ) { return m_iState; };

	string_t	GetStyle( void ) { return m_iszCurrentStyle; };
	void	SetStyle( string_t iszPattern );
	void	SetCorrectStyle( void );

private:
	bool CustomStyleUsed();

	STATE m_iState;		// current state
	int	m_iOnStyle;	// style to use while on
	int	m_iOffStyle;	// style to use while off
	int	m_iTurnOnStyle;	// style to use while turning on
	int	m_iTurnOffStyle;	// style to use while turning off
	int	m_iTurnOnTime;	// time taken to turn on
	int	m_iTurnOffTime;	// time taken to turn off
	string_t	m_iszPattern;	// custom style to use while on
	string_t	m_iszCurrentStyle;	// current style string
};

LINK_ENTITY_TO_CLASS( light, CLight );

BEGIN_DATADESC( CLight )
	DEFINE_FIELD( m_iState, FIELD_INTEGER ),
	DEFINE_KEYFIELD( m_iszPattern, FIELD_STRING, "pattern" ),
	DEFINE_FIELD( m_iszCurrentStyle, FIELD_STRING ),
	DEFINE_KEYFIELD( m_iOnStyle, FIELD_INTEGER, "m_iOnStyle" ),
	DEFINE_KEYFIELD( m_iOffStyle, FIELD_INTEGER, "m_iOffStyle" ),
	DEFINE_KEYFIELD( m_iTurnOnStyle, FIELD_INTEGER, "m_iTurnOnStyle" ),
	DEFINE_KEYFIELD( m_iTurnOffStyle, FIELD_INTEGER, "m_iTurnOffStyle" ),
	DEFINE_KEYFIELD( m_iTurnOnTime, FIELD_INTEGER, "m_iTurnOnTime" ),
	DEFINE_KEYFIELD( m_iTurnOffTime, FIELD_INTEGER, "m_iTurnOffTime" ),
END_DATADESC()


//
// Cache user-entity-field values until spawn is called.
//
void CLight :: KeyValue( KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iOnStyle"))
	{
		m_iOnStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iOffStyle"))
	{
		m_iOffStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnStyle"))
	{
		m_iTurnOnStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffStyle"))
	{
		m_iTurnOffStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnTime"))
	{
		m_iTurnOnTime = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffTime"))
	{
		m_iTurnOffTime = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
 	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		Vector angles = GetAbsAngles();
		angles.x = Q_atof(pkvd->szValue);
		SetAbsAngles( angles );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pattern"))
	{
		m_iszPattern = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "firetarget"))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		BaseClass::KeyValue( pkvd );
	}
}

void CLight :: SetStyle ( string_t iszPattern )
{
	if (!CustomStyleUsed()) // if it's using a global style, don't change it
		return;

	m_iszCurrentStyle = iszPattern;
	LIGHT_STYLE(m_iStyle, (char *)STRING( iszPattern ));
}

// regardless of what's been set by trigger_lightstyle ents, set the style I think I need
void CLight :: SetCorrectStyle ( void )
{
	if (CustomStyleUsed())
	{
		switch (m_iState)
		{
		case STATE_ON:
			if (m_iszPattern) // custom styles have priority over standard ones
				SetStyle( m_iszPattern );
			else if (m_iOnStyle)
				SetStyle(GetStdLightStyle(m_iOnStyle));
			else
				SetStyle(MAKE_STRING("m"));
			break;
		case STATE_OFF:
			if (m_iOffStyle)
				SetStyle(GetStdLightStyle(m_iOffStyle));
			else
				SetStyle(MAKE_STRING("a"));
			break;
		case STATE_TURN_ON:
			if (m_iTurnOnStyle)
				SetStyle(GetStdLightStyle(m_iTurnOnStyle));
			else
				SetStyle(MAKE_STRING("a"));
			break;
		case STATE_TURN_OFF:
			if (m_iTurnOffStyle)
				SetStyle(GetStdLightStyle(m_iTurnOffStyle));
			else
				SetStyle(MAKE_STRING("m"));
			break;
		}
	}
	else
	{
		m_iszCurrentStyle = GetStdLightStyle( m_iStyle );
	}
}

bool CLight::CustomStyleUsed()
{
	return m_iStyle >= 32;
}

void CLight :: Think( void )
{
	switch( GetState( ))
	{
	case STATE_TURN_ON:
		m_iState = STATE_ON;
		UTIL_FireTargets( pev->target, this, this, USE_ON, 0 );
		break;
	case STATE_TURN_OFF:
		m_iState = STATE_OFF;
		UTIL_FireTargets( pev->target, this, this, USE_OFF, 0 );
		break;
	}

	SetCorrectStyle();
}

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) LIGHT_START_OFF
Non-displayed light.
Default light value is 300
Default style is 0
If targeted, it will toggle between on or off.
*/
void CLight :: Spawn( void )
{
	if( FStringNull( pev->targetname ))
	{
		// inert light
		REMOVE_ENTITY( edict() );
		return;
	}
	
	if( FBitSet( pev->spawnflags, SF_LIGHT_START_OFF ))
		m_iState = STATE_OFF;
	else
		m_iState = STATE_ON;

	SetCorrectStyle();
}

void CLight :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (CustomStyleUsed())
	{
		if ( !ShouldToggle( useType ))
			return;

		switch( GetState( ))
		{
		case STATE_ON:
		case STATE_TURN_ON:
			if (m_iTurnOffTime)
			{
				m_iState = STATE_TURN_OFF;
				SetNextThink( m_iTurnOffTime );
			}
			else
				m_iState = STATE_OFF;
			break;
		case STATE_OFF:
		case STATE_TURN_OFF:
			if (m_iTurnOnTime)
			{
				m_iState = STATE_TURN_ON;
				SetNextThink( m_iTurnOnTime );
			}
			else
				m_iState = STATE_ON;
			break;
		}
		SetCorrectStyle();
	}
}

//
// shut up spawn functions for new spotlights
//
LINK_ENTITY_TO_CLASS( light_spot, CLight );


class CEnvLight : public CLight
{
	DECLARE_CLASS( CEnvLight, CLight );
public:
	void	KeyValue( KeyValueData* pkvd ); 
	void	Spawn( void );
};

LINK_ENTITY_TO_CLASS( light_environment, CEnvLight );

void CEnvLight::KeyValue( KeyValueData* pkvd )
{
	if (FStrEq(pkvd->szKeyName, "_light"))
	{
		int r, g, b, v, j;
		char szColor[64];
		j = sscanf( pkvd->szValue, "%d %d %d %d\n", &r, &g, &b, &v );
		if (j == 1)
		{
			g = b = r;
		}
		else if (j == 4)
		{
			r = r * (v / 255.0);
			g = g * (v / 255.0);
			b = b * (v / 255.0);
		}

		// simulate qrad direct, ambient,and gamma adjustments, as well as engine scaling
		r = pow( r / 114.0, 0.6 ) * 264;
		g = pow( g / 114.0, 0.6 ) * 264;
		b = pow( b / 114.0, 0.6 ) * 264;

		pkvd->fHandled = TRUE;
		sprintf( szColor, "%d", r );
		CVAR_SET_STRING( "sv_skycolor_r", szColor );
		sprintf( szColor, "%d", g );
		CVAR_SET_STRING( "sv_skycolor_g", szColor );
		sprintf( szColor, "%d", b );
		CVAR_SET_STRING( "sv_skycolor_b", szColor );
	}
	else
	{
		CLight::KeyValue( pkvd );
	}
}


void CEnvLight :: Spawn( void )
{
	char szVector[64];
	UTIL_MakeAimVectors( GetAbsAngles() );

	sprintf( szVector, "%f", gpGlobals->v_forward.x );
	CVAR_SET_STRING( "sv_skyvec_x", szVector );
	sprintf( szVector, "%f", gpGlobals->v_forward.y );
	CVAR_SET_STRING( "sv_skyvec_y", szVector );
	sprintf( szVector, "%f", gpGlobals->v_forward.z );
	CVAR_SET_STRING( "sv_skyvec_z", szVector );

	CLight::Spawn( );
}

//**********************************************************
//LRC- the CTriggerLightstyle entity - changes the style of a light temporarily.
//**********************************************************
class CLightFader : public CPointEntity
{
	DECLARE_CLASS( CLightFader, CPointEntity );
public:
	void FadeThink( void );
	void WaitThink( void );

	DECLARE_DATADESC();

	CLight	*m_pLight;
	char	m_cFrom;
	char	m_cTo;
	char	m_szCurStyle[256];
	float	m_fEndTime;
	string_t	m_iszPattern;
	float	m_fStep;
	int	m_iWait;
	int	m_iHardwareLerped;
};

LINK_ENTITY_TO_CLASS( lightfader, CLightFader );

BEGIN_DATADESC( CLightFader )
	DEFINE_FIELD( m_pLight, FIELD_CLASSPTR ),
	DEFINE_FIELD( m_cFrom, FIELD_CHARACTER ),
	DEFINE_FIELD( m_cTo, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( m_szCurStyle, FIELD_CHARACTER ),
	DEFINE_FIELD( m_fEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_iszPattern, FIELD_STRING ),
	DEFINE_FIELD( m_fStep, FIELD_FLOAT ),
	DEFINE_FIELD( m_iWait, FIELD_INTEGER ),
	DEFINE_FIELD( m_iHardwareLerped, FIELD_BOOLEAN ),
	DEFINE_FUNCTION( FadeThink ),
	DEFINE_FUNCTION( WaitThink ),
END_DATADESC()

void CLightFader::FadeThink( void )
{
	if( m_fEndTime > gpGlobals->time )
	{
		if( !m_iHardwareLerped )
		{
			m_szCurStyle[0] = m_cTo + (char)((m_cFrom - m_cTo) * (m_fEndTime - gpGlobals->time) * m_fStep);
			m_szCurStyle[1] = 0; // null terminator
			m_pLight->SetStyle(MAKE_STRING(m_szCurStyle));
		}
		SetNextThink( 0.1 );
	}
	else
	{
		// fade is finished
		m_pLight->SetStyle(m_iszPattern);
		if (m_iWait > -1)
		{
			// wait until it's time to switch off
			SetThink( &CLightFader:: WaitThink );
			SetNextThink( m_iWait );
		}
		else
		{
			// we've finished, kill the fader
			SetThink( &CLightFader:: SUB_Remove );
			SetNextThink( 0.1 );
		}
	}
}

// we've finished. revert the light and kill the fader.
void CLightFader::WaitThink( void )
{
	if( m_iszPattern )
		m_pLight->SetStyle( m_iszPattern );
	else m_pLight->SetCorrectStyle();
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 0.1 );
}

class CTriggerLightstyle : public CPointEntity
{
	DECLARE_CLASS( CTriggerLightstyle, CPointEntity );
public:
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();
private:
	char	m_szOldPattern[256];
	string_t	m_iszPattern;
	int	m_iFade;
	int	m_iWait;
};

LINK_ENTITY_TO_CLASS( trigger_lightstyle, CTriggerLightstyle );

BEGIN_DATADESC( CTriggerLightstyle )
	DEFINE_KEYFIELD( m_iszPattern, FIELD_STRING, "pattern" ),
	DEFINE_KEYFIELD( m_iFade, FIELD_INTEGER, "m_iFade" ),
	DEFINE_KEYFIELD( m_iWait, FIELD_INTEGER, "m_iWait" ),
	DEFINE_AUTO_ARRAY( m_szOldPattern, FIELD_CHARACTER ),
END_DATADESC()

void CTriggerLightstyle::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pattern"))
	{
		m_iszPattern = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFade"))
	{
		m_iFade = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iWait"))
	{
		m_iWait = Q_atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTriggerLightstyle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = NULL;
	if ( !pev->target )
		return;

	// ALERT( at_console, "Lightstyle change for: (%s)\n", STRING(pev->target) );
	bool fFisrt = true;

	while( 1 )
	{
		pTarget = UTIL_FindEntityByTargetname(pTarget,STRING(pev->target), pActivator);
		if (FNullEnt(pTarget))
			break;

		int iszPattern;
		if (m_iszPattern)
			iszPattern = m_iszPattern;
		else
			iszPattern = GetStdLightStyle(m_iStyle);

		// not a light entity?
		if (!FClassnameIs(pTarget->pev, "light") && !FClassnameIs(pTarget->pev, "light_spot") && !FClassnameIs(pTarget->pev, "light_environment"))
		{
			if (pTarget->m_iStyle >= 32)
				LIGHT_STYLE( pTarget->m_iStyle, (char*)STRING( iszPattern ));
		}
		else
		{
			CLight *pLight = (CLight *)pTarget;

			if( m_iFade )
			{
				CLightFader *pFader = GetClassPtr( (CLightFader*)NULL );
				pFader->pev->classname = MAKE_STRING( "lightfader" );
				pFader->m_pLight = pLight;
				pFader->m_cFrom = ((char*)STRING(pLight->GetStyle()))[0];
				pFader->m_cTo = ((char*)STRING(iszPattern))[0];
				pFader->m_iszPattern = iszPattern;
				pFader->m_fEndTime = gpGlobals->time + m_iFade;
				pFader->m_fStep = 1.0f / m_iFade;
				pFader->m_iWait = m_iWait;
				pFader->SetThink( &CLightFader::FadeThink );
				pFader->SetNextThink( 0.1 );

				float	time = 1.0;
				float	end = time + m_iFade;
				char	lerpedPattern[256];
				char	*lpPattern = lerpedPattern;
				pFader->m_iHardwareLerped = TRUE;

				while( end > time )
				{
					*lpPattern = pFader->m_cTo + (char)((pFader->m_cFrom - pFader->m_cTo) * (end - time) * pFader->m_fStep);
					time += 0.09; // to prevent loop the style
					lpPattern++;

					// exceed hardware pattern length?
					if(( lpPattern - lerpedPattern ) > 250 )
					{
						pFader->m_iHardwareLerped = FALSE;
						break;
					}
				}
				*lpPattern = '\0';

				if( pFader->m_iHardwareLerped )
				{
					// build the lerped sequence and let the engine lerping the lightstyle
					pFader->m_pLight->SetStyle( ALLOC_STRING( lerpedPattern ));
				}
			}
			else
			{
				if( fFisrt )
				{
					// save old pattern in case we needs to be restore it
					Q_strncpy( m_szOldPattern, GET_LIGHT_STYLE( pLight->m_iStyle ), 256 ); 
					fFisrt = false;
				}

				pLight->SetStyle( iszPattern );

				if( m_iWait != -1 )
				{
					CLightFader *pFader = GetClassPtr( (CLightFader*)NULL );
					pFader->pev->classname = MAKE_STRING( "lightfader" );
					pFader->m_pLight = pLight;
					// i'm hope somebody don't delete this entity from map :-)
					pFader->m_iszPattern = MAKE_STRING( m_szOldPattern );
					pFader->SetThink( &CLightFader::WaitThink );
					pFader->SetNextThink( m_iWait );
				}
			}
		}
	}
}

// buz: dynamic light entity
#define SF_DYNLIGHT_STARTOFF		1
#define SF_DYNLIGHT_NOSHADOW		2
#define SF_DYNLIGHT_NOBUMP		4
#define SF_DYNLIGHT_FLARE		8

class CDynamicLight : public CPointEntity
{
	DECLARE_CLASS(CDynamicLight, CPointEntity);
public:
	void Spawn();
	void Precache();
	void KeyValue(KeyValueData *pkvd);
	STATE GetState(void);
	void Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value);
	void SetStyle(string_t iszPattern);
	void UpdatePVSPoint(void);
	void UpdateState();
	void CineThink(void);
	void PVSThink(void);

	DECLARE_DATADESC();
private:
	bool CustomStyleUsed();
	void UpdateStyle();

	int	m_iOnStyle;			// style to use while on
	int	m_iOffStyle;		// style to use while off
	int	m_iTurnOnStyle;		// style to use while turning on
	int	m_iTurnOffStyle;	// style to use while turning off
	int	m_iTurnOnTime;		// time taken to turn on
	int	m_iTurnOffTime;		// time taken to turn off
	string_t m_iszCurrentStyle;	// current style string
	float m_flBrightness = 2.0f;
};

LINK_ENTITY_TO_CLASS(env_dynlight, CDynamicLight);

BEGIN_DATADESC( CDynamicLight )
	DEFINE_FIELD( m_iOnStyle, FIELD_INTEGER ),
	DEFINE_FIELD( m_iOffStyle, FIELD_INTEGER ),
	DEFINE_FIELD( m_iTurnOnStyle, FIELD_INTEGER ),
	DEFINE_FIELD( m_iTurnOffStyle, FIELD_INTEGER ),
	DEFINE_FIELD( m_iTurnOnTime, FIELD_INTEGER ),
	DEFINE_FIELD( m_iTurnOffTime, FIELD_INTEGER ),
	DEFINE_FIELD( m_iszCurrentStyle, FIELD_STRING ),
	DEFINE_FIELD( m_flBrightness, FIELD_FLOAT ),
	DEFINE_FUNCTION( CineThink ),
	DEFINE_FUNCTION( PVSThink ),
END_DATADESC()

void CDynamicLight::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	Precache();
	UpdateState();

	if (pev->sequence)
		pev->renderfx = kRenderFxCinemaLight;	// dynamic light with avi-texture
	else
		pev->renderfx = kRenderFxDynamicLight;	// dynamic light

	SET_MODEL(ENT(pev), "sprites/null.spr"); // should be visible to send to client

	pev->renderamt = pev->renderamt / 8;

	// spotlight
	if (pev->scale)
	{
		// create second PVS point
		pev->enemy = Create("info_target", pev->origin, g_vecZero, edict())->edict();
		SET_MODEL(pev->enemy, "sprites/null.spr"); // allow to collect visibility info
		UTIL_SetSize(VARS(pev->enemy), Vector(-8, -8, -8), Vector(8, 8, 8));

		// to prevent disapperaing from PVS (renderamt is premultiplied by 0.125)
		UTIL_SetSize(pev, Vector(-pev->renderamt, -pev->renderamt, -pev->renderamt), Vector(pev->renderamt, pev->renderamt, pev->renderamt));
	}
	else
	{
		// to prevent disapperaing from PVS (renderamt is premultiplied by 0.125)
		UTIL_SetSize(pev, Vector(-pev->renderamt * 4, -pev->renderamt * 4, -pev->renderamt * 4), Vector(pev->renderamt * 4, pev->renderamt * 4, pev->renderamt * 4));
	}

	if (pev->spawnflags & SF_DYNLIGHT_NOSHADOW)
		pev->effects |= EF_NOSHADOW;
	if (pev->spawnflags & SF_DYNLIGHT_NOBUMP)
		pev->effects |= EF_NOBUMP;
	if (pev->spawnflags & SF_DYNLIGHT_FLARE)
		pev->effects |= EF_LENSFLARE;

	if (pev->spawnflags & SF_DYNLIGHT_STARTOFF)
	{
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else if (pev->sequence)
	{
		SetThink(&CDynamicLight::CineThink);
		SetNextThink(0.1f);
	}
	else if (pev->scale)
	{
		SetThink(&CDynamicLight::PVSThink);
		SetNextThink(0.1f);
	}
}

void CDynamicLight::Precache()
{
	PRECACHE_MODEL("sprites/null.spr");

	if (!FStringNull(pev->message))
	{
		const char *filepath = STRING(pev->message);
		if (UTIL_ValidMovieFileExtension(filepath))
		{
			// 0 if movie not found
			pev->sequence = UTIL_PrecacheMovie(pev->message);
		}
	}
}

void CDynamicLight::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "brightness"))
	{
		m_flBrightness = Q_atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
		UpdateState();
	}
	else if (FStrEq(pkvd->szKeyName, "m_iOnStyle"))
	{
		m_iOnStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iOffStyle"))
	{
		m_iOffStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnStyle"))
	{
		m_iTurnOnStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffStyle"))
	{
		m_iTurnOffStyle = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnTime"))
	{
		m_iTurnOnTime = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffTime"))
	{
		m_iTurnOffTime = Q_atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else BaseClass::KeyValue(pkvd);
}

STATE CDynamicLight::GetState(void)
{
	if (pev->effects & EF_NODRAW)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CDynamicLight::Use(CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value)
{
	if (useType == USE_ON)
		pev->effects &= ~EF_NODRAW;
	else if (useType == USE_OFF)
		pev->effects |= EF_NODRAW;
	else if (useType == USE_TOGGLE)
	{
		if (pev->effects & EF_NODRAW)
			pev->effects &= ~EF_NODRAW;
		else pev->effects |= EF_NODRAW;
	}

	if (pev->effects & EF_NODRAW)
	{
		DontThink();
	}
	else if (pev->sequence)
	{
		SetThink(&CDynamicLight::CineThink);
		SetNextThink(0.1f);
	}
	else if (pev->scale)
	{
		SetThink(&CDynamicLight::PVSThink);
		SetNextThink(0.1f);
	}
}

void CDynamicLight::SetStyle(string_t iszPattern)
{
	// if it's using a global style, don't change it
	if (!CustomStyleUsed()) 
		return;

	m_iszCurrentStyle = iszPattern;
	LIGHT_STYLE(m_iStyle, (char *)STRING(iszPattern));
}

void CDynamicLight::UpdatePVSPoint(void)
{
	TraceResult tr;
	Vector forward;

	UTIL_MakeVectorsPrivate(pev->angles, forward, NULL, NULL);
	Vector vecSrc = pev->origin + forward * 8.0f;
	Vector vecEnd = vecSrc + forward * (pev->renderamt * 8.0f);
	UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, edict(), &tr);

	// this is our second PVS point
	CBaseEntity *pVisHelper = CBaseEntity::Instance(pev->enemy);
	if (pVisHelper) UTIL_SetOrigin(pVisHelper, tr.vecEndPos + tr.vecPlaneNormal * 8.0f);
}

void CDynamicLight::UpdateState()
{
	UpdateStyle();
	pev->framerate = m_flBrightness;
	pev->skin = m_iStyle & 0x3F; // store lightstyle index in first 6 bits
}

void CDynamicLight::CineThink(void)
{
	UpdatePVSPoint();
	UpdateState();

	// update as 30 frames per second
	pev->fuser2 += CIN_FRAMETIME;
	SetNextThink(CIN_FRAMETIME);
}

void CDynamicLight::PVSThink(void)
{
	UpdatePVSPoint();
	UpdateState();

	// static light should be updated in case
	// moving platform under them
	SetNextThink(m_hParent.Get() ? 0.01f : 0.1f);
}

bool CDynamicLight::CustomStyleUsed()
{
	return m_iStyle >= 32;
}

void CDynamicLight::UpdateStyle()
{
	if (CustomStyleUsed())
	{
		switch (GetState())
		{
			case STATE_ON:
				if (m_iOnStyle)
					SetStyle(GetStdLightStyle(m_iOnStyle));
				else
					SetStyle(MAKE_STRING("m"));
				break;
			case STATE_OFF:
				if (m_iOffStyle)
					SetStyle(GetStdLightStyle(m_iOffStyle));
				else
					SetStyle(MAKE_STRING("a"));
				break;
			case STATE_TURN_ON:
				if (m_iTurnOnStyle)
					SetStyle(GetStdLightStyle(m_iTurnOnStyle));
				else
					SetStyle(MAKE_STRING("a"));
				break;
			case STATE_TURN_OFF:
				if (m_iTurnOffStyle)
					SetStyle(GetStdLightStyle(m_iTurnOffStyle));
				else
					SetStyle(MAKE_STRING("m"));
				break;
		}
	}
	else
	{
		m_iszCurrentStyle = GetStdLightStyle(m_iStyle);
	}
}
