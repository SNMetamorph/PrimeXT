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

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include <time.h>
#include "shake.h"
#include "decals.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "studio.h"
#include "trace.h"
#include "utldict.h"
#include "render_api.h"
#include "user_messages.h"

//-----------------------------------------------------------------------------
// Entity creation factory
//-----------------------------------------------------------------------------
class CEntityFactoryDictionary : public IEntityFactoryDictionary
{
public:
	CEntityFactoryDictionary();

	virtual void InstallFactory( IEntityFactory *pFactory, const char *pClassName );
	virtual CBaseEntity *Create( const char *pClassName, entvars_t *pev = NULL );
	virtual void Destroy( const char *pClassName, CBaseEntity *pNetworkable );
	virtual const char *GetCannonicalName( const char *pClassName );
	void ReportEntitySizes();

private:
	IEntityFactory *FindFactory( const char *pClassName );
public:
	CUtlDict< IEntityFactory *, unsigned short > m_Factories;
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
IEntityFactoryDictionary *EntityFactoryDictionary()
{
	static CEntityFactoryDictionary s_EntityFactory;
	return &s_EntityFactory;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEntityFactoryDictionary::CEntityFactoryDictionary() : m_Factories( true, 0, 128 )
{
}

//-----------------------------------------------------------------------------
// Finds a new factory
//-----------------------------------------------------------------------------
IEntityFactory *CEntityFactoryDictionary::FindFactory( const char *pClassName )
{
	unsigned short nIndex = m_Factories.Find( pClassName );
	if ( nIndex == m_Factories.InvalidIndex() )
		return NULL;
	return m_Factories[nIndex];
}


//-----------------------------------------------------------------------------
// Install a new factory
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::InstallFactory( IEntityFactory *pFactory, const char *pClassName )
{
	assert( FindFactory( pClassName ) == NULL );
	m_Factories.Insert( pClassName, pFactory );
}


//-----------------------------------------------------------------------------
// Instantiate something using a factory
//-----------------------------------------------------------------------------
CBaseEntity *CEntityFactoryDictionary::Create( const char *pClassName, entvars_t *pev )
{
	IEntityFactory *pFactory = FindFactory( pClassName );

	if( !pFactory )
	{
		ALERT( at_warning, "Attempted to create unknown entity type %s!\n", pClassName );
		return NULL;
	}

	return pFactory->Create( pClassName, pev );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
const char *CEntityFactoryDictionary::GetCannonicalName( const char *pClassName )
{
	return m_Factories.GetElementName( m_Factories.Find( pClassName ) );
}

//-----------------------------------------------------------------------------
// Destroy a networkable
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary :: Destroy( const char *pClassName, CBaseEntity *pEntity )
{
	IEntityFactory *pFactory = FindFactory( pClassName );

	if( !pFactory )
	{
		ALERT( at_warning, "Attempted to destroy unknown entity type %s!\n", pClassName );
		return;
	}

	pFactory->Destroy( pEntity );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary :: ReportEntitySizes()
{
	for( int i = m_Factories.First(); i != m_Factories.InvalidIndex(); i = m_Factories.Next( i ))
	{
		ALERT( at_console, " %s: %d bytes\n", m_Factories.GetElementName( i ), m_Factories[i]->GetEntitySize());
	}
}

void DumpEntityNames_f( void )
{
	CEntityFactoryDictionary *dict = (CEntityFactoryDictionary *)EntityFactoryDictionary();

	if( !dict )
	{
		ALERT( at_error, "Entity Factory not installed!\n" );
		return;
	}

	for( int i = dict->m_Factories.First(); i != dict->m_Factories.InvalidIndex(); i = dict->m_Factories.Next( i ))
	{
		ALERT( at_console, "%s\n", dict->m_Factories.GetElementName( i ));
	}
}

void DumpEntitySizes_f( void )
{
	((CEntityFactoryDictionary*)EntityFactoryDictionary())->ReportEntitySizes();
}

void DumpStrings_f( void )
{
	g_GameStringPool.Dump();
}

extern "C" DLLEXPORT void custom(entvars_t *pev) 
{
	EntityFactoryDictionary()->Create(STRING(pev->classname), pev);
}

// This does the necessary casting / extract to grab a pointer to a member function as a void *
// UNDONE: Cast to BASEPTR or something else here?
#define EXTRACT_VOID_FUNCTIONPTR(x)		(*(void **)(&(x)))
//-----------------------------------------------------------------------------
// Purpose: Search this datamap for the name of this member function
//			This is used to save/restore function pointers (convert pointer to text)
// Input  : *function - pointer to member function
// Output : const char * - function name
//-----------------------------------------------------------------------------
const char *UTIL_FunctionToName( DATAMAP *pMap, void *function )
{
	while( pMap )
	{
		for( int i = 0; i < pMap->dataNumFields; i++ )
		{
			if( pMap->dataDesc[i].flags & FTYPEDESC_FUNCTIONTABLE )
			{
				ASSERT( sizeof(pMap->dataDesc[i].func) == sizeof(void *));
				void *pTest = EXTRACT_VOID_FUNCTIONPTR( pMap->dataDesc[i].func );
				if( pTest == function )
					return pMap->dataDesc[i].fieldName;
			}
		}

		pMap = pMap->baseMap;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Search the datamap for a function named pName
//			This is used to save/restore function pointers (convert text back to pointer)
// Input  : *pName - name of the member function
//-----------------------------------------------------------------------------
void *UTIL_FunctionFromName( DATAMAP *pMap, const char *pName )
{
	while( pMap )
	{
		for( int i = 0; i < pMap->dataNumFields; i++ )
		{
			ASSERT( sizeof(pMap->dataDesc[i].func) == sizeof(void *));

			if( pMap->dataDesc[i].flags & FTYPEDESC_FUNCTIONTABLE )
			{
				if( FStrEq( pName, pMap->dataDesc[i].fieldName ))
				{
					return EXTRACT_VOID_FUNCTIONPTR( pMap->dataDesc[i].func );
				}
			}
		}
		pMap = pMap->baseMap;
	}

	ALERT( at_error, "Failed to find function %s\n", pName );

	return NULL;
}
 
void Msg( const char *szText, ... )
{
	va_list szCommand;
	static char value[1024];

	va_start( szCommand, szText );
	Q_vsnprintf( value, sizeof( value ), szText, szCommand );
	va_end( szCommand ); 
 
	g_engfuncs.pfnAlertMessage( at_console, value );
}

edict_t *ENT( CBaseEntity *pEntity )
{
	if( pEntity )
		return ENT( pEntity->pev );
	return NULL;
}

BOOL FNullEnt( CBaseEntity *ent )
{
	return (ent == NULL) || FNullEnt( ent->edict( ));
}

BOOL FClassnameIs( CBaseEntity *pEnt, const char *szClassname )
{ 
	return FStrEq( pEnt->GetClassname(), szClassname );
}

//=========================================================
//	calculate brush model origin
//=========================================================
Vector VecBModelOrigin( entvars_t* pevBModel )
{
	return pevBModel->absmin + ( pevBModel->size * 0.5f );
}

int g_groupmask = 0;
int g_groupop = 0;

// Normal overrides
void UTIL_SetGroupTrace( int groupmask, int op )
{
	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

void UTIL_UnsetGroupTrace( void )
{
	g_groupmask		= 0;
	g_groupop		= 0;

	ENGINE_SETGROUPMASK( 0, 0 );
}

// Smart version, it'll clean itself up when it pops off stack
UTIL_GroupTrace::UTIL_GroupTrace( int groupmask, int op )
{
	m_oldgroupmask	= g_groupmask;
	m_oldgroupop	= g_groupop;

	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

UTIL_GroupTrace::~UTIL_GroupTrace( void )
{
	g_groupmask		=	m_oldgroupmask;
	g_groupop		=	m_oldgroupop;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

#ifdef	DEBUG
edict_t *DBG_EntOfVars( const entvars_t *pev )
{
	if (pev->pContainingEntity != NULL)
		return pev->pContainingEntity;
	ALERT(at_console, "entvars_t pContainingEntity is NULL, calling into engine");
	edict_t* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);
	if (pent == NULL)
		ALERT(at_console, "DAMN!  Even the engine couldn't FindEntityByVars!");
	((entvars_t *)pev)->pContainingEntity = pent;
	return pent;
}
#endif //DEBUG


#ifdef	DEBUG
	void
DBG_AssertFunction(
	BOOL		fExpr,
	const char*	szExpr,
	const char*	szFile,
	int			szLine,
	const char*	szMessage)
	{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != NULL)
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine);
	ALERT(at_console, szOut);
	}
#endif	// DEBUG


// Button sound table. 
// Also used by CBaseDoor to get 'touched' door lock/unlock sounds
const char *UTIL_ButtonSound( int sound )
{ 
	const char *pszSound;

	int m_iSoundString = UTIL_LoadSoundPreset( sound );

	switch( m_iSoundString )
	{
	case 0: pszSound = "common/null.wav"; break;
	case 1: pszSound = "buttons/button1.wav"; break;
	case 2: pszSound = "buttons/button2.wav"; break;
	case 3: pszSound = "buttons/button3.wav"; break;
	case 4: pszSound = "buttons/button4.wav"; break;
	case 5: pszSound = "buttons/button5.wav"; break;
	case 6: pszSound = "buttons/button6.wav"; break;
	case 7: pszSound = "buttons/button7.wav"; break;
	case 8: pszSound = "buttons/button8.wav"; break;
	case 9: pszSound = "buttons/button9.wav"; break;
	case 10: pszSound = "buttons/button10.wav"; break;
	case 11: pszSound = "buttons/button11.wav"; break;
	case 12: pszSound = "buttons/latchlocked1.wav"; break;
	case 13: pszSound = "buttons/latchunlocked1.wav";	break;
	case 14: pszSound = "buttons/lightswitch2.wav"; break;

// next 6 slots reserved for any additional sliding button sounds we may add
	
	case 21: pszSound = "buttons/lever1.wav"; break;
	case 22: pszSound = "buttons/lever2.wav"; break;
	case 23: pszSound = "buttons/lever3.wav"; break;
	case 24: pszSound = "buttons/lever4.wav"; break;
	case 25: pszSound = "buttons/lever5.wav"; break;
	default: pszSound = STRING( UTIL_PrecacheSound( m_iSoundString )); break;
	}

	return pszSound;
}

const char *GetStringForUseType( USE_TYPE useType )
{
	switch( useType )
	{
	case USE_ON: return "USE_ON";
	case USE_OFF: return "USE_OFF";
	case USE_TOGGLE: return "USE_TOGGLE";
	case USE_SET: return "USE_SET";
	case USE_RESET: return "USE_RESET";
	case USE_REMOVE: return "USE_REMOVE";
	default: return "UNKNOWN USE_TYPE!";
	}
}

const char *GetStringForState( STATE state )
{
	switch( state )
	{
	case STATE_ON: return "ON";
	case STATE_OFF: return "OFF";
	case STATE_TURN_ON: return "TURN ON";
	case STATE_TURN_OFF: return "TURN OFF";
	case STATE_IN_USE: return "IN USE";
	case STATE_DEAD: return "DEAD";
	default: return "UNKNOWN STATE!";
	}
}

STATE GetStateForString( const char *string )
{
	if( !Q_stricmp( string, "ON" ))
		return STATE_ON;
	else if( !Q_stricmp( string, "OFF" ))
		return STATE_OFF;
	else if( !Q_stricmp( string, "TURN ON" ))
		return STATE_TURN_ON;
	else if( !Q_stricmp( string, "TURN OFF" ))
		return STATE_TURN_OFF;
	else if( !Q_stricmp( string, "IN USE" ))
		return STATE_IN_USE;
	else if( !Q_stricmp( string, "DEAD" ))
		return STATE_DEAD;
	else if( Q_isdigit( string ))
		return (STATE)Q_atoi( string );

	// assume error
	ALERT( at_error, "Unknown state '%s' specified\n", string );

	return (STATE)-1;
}

void PrintStringForDamage( int dmgbits )
{
	char szDmgBits[256];
	Q_strcat( szDmgBits, "DAMAGE: " );

	if( dmgbits & DMG_GENERIC ) Q_strcat( szDmgBits, "Generic " );
	if( dmgbits & DMG_CRUSH ) Q_strcat( szDmgBits, "Crush "  );
	if( dmgbits & DMG_BULLET ) Q_strcat( szDmgBits, "Bullet "   );
	if( dmgbits & DMG_SLASH ) Q_strcat( szDmgBits, "Slash " );
	if( dmgbits & DMG_BURN ) Q_strcat( szDmgBits, "Burn " );
	if( dmgbits & DMG_FREEZE ) Q_strcat( szDmgBits, "Freeze "  );
	if( dmgbits & DMG_FALL ) Q_strcat( szDmgBits, "Fall " );
	if( dmgbits & DMG_BLAST ) Q_strcat( szDmgBits, "Blast "  );
	if( dmgbits & DMG_CLUB ) Q_strcat( szDmgBits, "Club "   );
	if( dmgbits & DMG_SHOCK ) Q_strcat( szDmgBits, "Shock " );
	if( dmgbits & DMG_SONIC ) Q_strcat( szDmgBits, "Sonic " );
	if( dmgbits & DMG_ENERGYBEAM ) Q_strcat( szDmgBits, "Energy Beam " );
	if( dmgbits & DMG_NEVERGIB ) Q_strcat( szDmgBits, "Never Gib " );
	if( dmgbits & DMG_ALWAYSGIB ) Q_strcat( szDmgBits, "Always Gib " );
	if( dmgbits & DMG_DROWN ) Q_strcat( szDmgBits, "Drown " );
	if( dmgbits & DMG_PARALYZE ) Q_strcat( szDmgBits, "Paralyze Gas " );
	if( dmgbits & DMG_NERVEGAS ) Q_strcat( szDmgBits, "Nerve Gas " );
	if( dmgbits & DMG_POISON ) Q_strcat( szDmgBits, "Poison "  );
	if( dmgbits & DMG_RADIATION ) Q_strcat( szDmgBits, "Radiation " );
	if( dmgbits & DMG_DROWNRECOVER ) Q_strcat( szDmgBits, "Drown Recover "  );
	if( dmgbits & DMG_ACID ) Q_strcat( szDmgBits, "Acid "   );
	if( dmgbits & DMG_SLOWBURN ) Q_strcat( szDmgBits, "Slow Burn " );
	if( dmgbits & DMG_SLOWFREEZE ) Q_strcat( szDmgBits, "Slow Freeze " );
	if( dmgbits & DMG_MORTAR ) Q_strcat( szDmgBits, "Mortar "  );
	if( dmgbits & DMG_NUCLEAR ) Q_strcat( szDmgBits, "Nuclear Explode " );		

	Msg( "%s\n", szDmgBits );
}

const char* GetContentsString( int contents )
{
	switch( contents )
	{
	case -1: return "EMPTY";
	case -2: return "SOLID";
	case -3: return "WATER";
	case -4: return "SLIME";
	case -5: return "LAVA";
	case -6: return "SKY";
	case -16: return "LADDER";
	case -17: return "FLYFIELD";
	case -18: return "GRAVITY_FLYFIELD";
	case -19: return "FOG";
	case -20: return "SPECIAL 1";
	case -21: return "SPECIAL 2";
	case -22: return "SPECIAL 3";
	default: return "NO CONTENTS!";
	}
}

const char* GetStringForGlobalState( GLOBALESTATE state )
{
	switch( state )
	{
	case GLOBAL_ON: return "GLOBAL ON";
	case GLOBAL_OFF: return "GLOBAL OFF";
	case GLOBAL_DEAD: return "GLOBAL DEAD";
	default: return "UNKNOWN STATE!";
	}
}

BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{
	return g_pGameRules->GetNextBestWeapon( pPlayer, pCurrentWeapon );
}

// ripped this out of the engine
float	UTIL_AngleMod(float a)
{
	if (a < 0)
	{
		a = a + 360 * ((int)(a / 360) + 1);
	}
	else if (a >= 360)
	{
		a = a - 360 * ((int)(a / 360));
	}
	// a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		if ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		if ( delta <= -180 )
			delta += 360;
	}
	return delta;
}

Vector UTIL_VecToAngles( const Vector &vec )
{
	Vector angles;

	VectorAngles( vec, angles );

	return angles;
}
	
void UTIL_MoveToOrigin( edict_t *pent, const Vector &vecGoal, float flDist, int iMoveType )
{
	float rgfl[3];
	vecGoal.CopyToArray(rgfl);
	MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType ); 
}

void UTIL_Teleport( CBaseEntity *pSource, TeleportListEntry_t &entry, const Vector *newOrigin, const Vector *newAngles, const Vector *newVelocity )
{
	CBaseEntity *pTeleport = entry.pEntity;
	Vector prevOrigin = pTeleport->GetAbsOrigin();
	Vector prevAngles = pTeleport->GetAbsAngles();

	pTeleport->m_iTeleportFilter = TRUE;
	pTeleport->MakeNonSolid();

	// i'm teleporting myself
	if( pSource == pTeleport )
	{
		if( newAngles )
		{
			pTeleport->SetLocalAngles( *newAngles );

			if( pTeleport->IsPlayer() )
			{
				CBasePlayer *pPlayer = (CBasePlayer *)pTeleport;
				pPlayer->SnapEyeAngles( *newAngles );
			}

			if( pTeleport->IsRigidBody( ))
				WorldPhysic->SetAngles( pTeleport, *newAngles );
		}

		if( newVelocity )
		{
			pTeleport->SetAbsVelocity( *newVelocity );
			pTeleport->SetBaseVelocity( g_vecZero );

			if( pTeleport->IsRigidBody( ))
			{
				WorldPhysic->SetVelocity( pTeleport, *newVelocity );

				// HACKHACK reset avelocity for rigid body
				if( *newVelocity == g_vecZero )
					WorldPhysic->SetAvelocity( pTeleport, g_vecZero );
			}
		}

		if( newOrigin )
		{
			SetBits( pTeleport->pev->effects, EF_NOINTERP );
			UTIL_SetOrigin( pTeleport, *newOrigin );

			if( pTeleport->IsRigidBody( ))
				WorldPhysic->SetOrigin( pTeleport, *newOrigin );

			// teleport physics shadow too
			switch( pTeleport->m_iActorType )
			{
			case ACTOR_KINEMATIC:
				WorldPhysic->TeleportActor( pTeleport );
				break;
			case ACTOR_CHARACTER:
				WorldPhysic->TeleportCharacter( pTeleport );
				break;
			}
		}
	}
	else
	{
		// My parent is teleporting, just update my position & physics
		pTeleport->CalcAbsolutePosition();

		// teleport physics shadow too
		switch( pTeleport->m_iActorType )
		{
		case ACTOR_KINEMATIC:
			WorldPhysic->TeleportActor( pTeleport );
			break;
		case ACTOR_CHARACTER:
			WorldPhysic->TeleportCharacter( pTeleport );
			break;
		}
	}

	pTeleport->RestoreSolid();
	pTeleport->RelinkEntity( TRUE, NULL, TRUE );	// need to move back in solid list
	pTeleport->ClearGroundEntity();
	pTeleport->OnTeleport();		// call event
	pTeleport->m_iTeleportFilter = FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector an angle from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, Vector &vecAngles )
{
	if ( pEntity == NULL )
		return;

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	matrix4x4 matEntityToParent = matrix3x4( vecPosition, vecAngles );

	// concatenate with our parent's transform
	matrix4x4 matResult, matParentToWorld;
	
	if( pEntity->m_hParent != NULL )
	{
		matParentToWorld = pEntity->GetParentToWorldTransform();
	}
	else
	{
		matParentToWorld = pEntity->EntityToWorldTransform();
	}

	matResult = matParentToWorld.ConcatTransforms( matEntityToParent );
	vecPosition = matResult.GetOrigin();
	vecAngles = matResult.GetAngles();
}

int UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask )
{
	edict_t *pEdict = INDEXENT( 1 );
	CBaseEntity *pEntity;
	int count = 0;

	if ( !pEdict )
		return count;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( flagMask && !(pEdict->v.flags & flagMask) )	// Does it meet the criteria?
			continue;

		if ( mins.x > pEdict->v.absmax.x ||
			 mins.y > pEdict->v.absmax.y ||
			 mins.z > pEdict->v.absmax.z ||
			 maxs.x < pEdict->v.absmin.x ||
			 maxs.y < pEdict->v.absmin.y ||
			 maxs.z < pEdict->v.absmin.z )
			 continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}

	return count;
}


int UTIL_MonstersInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius )
{
	edict_t *pEdict = INDEXENT( 1 );
	CBaseEntity *pEntity;
	int count = 0;
	float distance, delta;
	float radiusSquared = radius * radius;

	if ( !pEdict )
		return count;

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( !(pEdict->v.flags & (FL_CLIENT|FL_MONSTER)) )	// Not a client/monster ?
			continue;

		// Use origin for X & Y since they are centered for all monsters
		// Now X
		delta = center.x - pEdict->v.origin.x;//(pEdict->v.absmin.x + pEdict->v.absmax.x)*0.5;
		delta *= delta;

		if ( delta > radiusSquared )
			continue;
		distance = delta;
		
		// Now Y
		delta = center.y - pEdict->v.origin.y;//(pEdict->v.absmin.y + pEdict->v.absmax.y)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		// Now Z
		delta = center.z - (pEdict->v.absmin.z + pEdict->v.absmax.z)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}


	return count;
}


CBaseEntity *UTIL_FindEntityInSphere( CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius )
{
	edict_t	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}


CBaseEntity *UTIL_FindEntityByString( CBaseEntity *pStartEntity, const char *szKeyword, const char *szValue )
{
	edict_t	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_BY_STRING( pentEntity, szKeyword, szValue );

	if (!FNullEnt(pentEntity))
		return CBaseEntity::Instance(pentEntity);
	return NULL;
}

CBaseEntity *UTIL_FindEntityByClassname( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "classname", szName );
}

CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "targetname", szName );
}

CBaseEntity *UTIL_FindEntityByTargetname( CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pActivator )
{
	if( FStrEq( szName, "*locus" ))
	{
		if( pActivator && ( pStartEntity == NULL || pActivator->eoffset() > pStartEntity->eoffset( )))
			return pActivator;
		return NULL;
	}
	return UTIL_FindEntityByTargetname( pStartEntity, szName );
}

CBaseEntity *UTIL_FindEntityByTarget( CBaseEntity *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "target", szName );
}

CBaseEntity *UTIL_FindEntityByMonsterTarget( CBaseEntity *pStartEntity, const char *szName )
{
	if( !szName || !*szName )
		return NULL;

	int e = 0;

	if (pStartEntity)
		e = pStartEntity->entindex();

	for ( e++; e < gpGlobals->maxEntities; e++ )
	{
		edict_t *pEdict = INDEXENT( e );

		if( !pEdict || pEdict->free )	// Not in use
			continue;

		CBaseMonster *pMonster = CBaseEntity::GetMonsterPointer( pEdict );

		if( !pMonster )
			continue;

		if( !Q_strcmp( szName, STRING( pMonster->m_iszTriggerTarget )) )
			return pMonster;
	}

	return NULL;
}

CBaseEntity *UTIL_FindEntityGeneric( const char *szWhatever, const Vector &vecSrc, float flRadius )
{
	CBaseEntity *pEntity = NULL;

	pEntity = UTIL_FindEntityByTargetname( NULL, szWhatever );
	if (pEntity)
		return pEntity;

	CBaseEntity *pSearch = NULL;
	float flMaxDist2 = flRadius * flRadius;
	while ((pSearch = UTIL_FindEntityByClassname( pSearch, szWhatever )) != NULL)
	{
		float flDist2 = (pSearch->GetAbsOrigin() - vecSrc).Length();
		flDist2 = flDist2 * flDist2;
		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}
	return pEntity;
}


// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBaseEntity	*UTIL_PlayerByIndex( int playerIndex )
{
	CBaseEntity *pPlayer = NULL;

	if ( playerIndex > 0 && playerIndex <= gpGlobals->maxClients )
	{
		edict_t *pPlayerEdict = INDEXENT( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->free )
		{
			pPlayer = CBaseEntity::Instance( pPlayerEdict );
		}
	}
	
	return pPlayer;
}


void UTIL_MakeVectors( const Vector &vecAngles )
{
	MAKE_VECTORS( vecAngles );
}


void UTIL_MakeAimVectors( const Vector &vecAngles )
{
	float rgflVec[3];
	vecAngles.CopyToArray(rgflVec);
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS(rgflVec);
}


#define SWAP(a,b,temp)	((temp)=(a),(a)=(b),(b)=(temp))

void UTIL_MakeInvVectors( const Vector &vec, globalvars_t *pgv )
{
	MAKE_VECTORS(vec);

	float tmp;
	pgv->v_right = pgv->v_right * -1;

	SWAP(pgv->v_forward.y, pgv->v_right.x, tmp);
	SWAP(pgv->v_forward.z, pgv->v_up.x, tmp);
	SWAP(pgv->v_right.z, pgv->v_up.y, tmp);
}


void UTIL_EmitAmbientSound( edict_t *entity, const Vector &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch )
{
	float rgfl[3];
	vecOrigin.CopyToArray(rgfl);

	if (samp && *samp == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(samp, name) >= 0)
			EMIT_AMBIENT_SOUND(entity, rgfl, name, vol, attenuation, fFlags, pitch);
	}
	else
		EMIT_AMBIENT_SOUND(entity, rgfl, samp, vol, attenuation, fFlags, pitch);
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}

static short FixedSigned16( float value, float scale )
{
	int output;

	output = value * scale;

	if ( output > 32767 )
		output = 32767;

	if ( output < -32768 )
		output = -32768;

	return (short)output;
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// UNDONE: Allow caller to shake clients not ONGROUND?
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, bool bAllowInAir )
{
	int			i;
	float		localAmplitude;
	ScreenShake	shake;

	shake.duration = FixedUnsigned16( duration, 1<<12 );		// 4.12 fixed
	shake.frequency = FixedUnsigned16( frequency, 1<<8 );	// 8.8 fixed

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer || ( !FBitSet( pPlayer->pev->flags, FL_ONGROUND ) && !bAllowInAir )) // Don't shake if not onground
			continue;

		localAmplitude = 0;

		if ( radius <= 0 )
			localAmplitude = amplitude;
		else
		{
			Vector delta = center - pPlayer->GetAbsOrigin();
			float distance = delta.Length();
	
			// Had to get rid of this falloff - it didn't work well
			if ( distance < radius )
				localAmplitude = amplitude;//radius - distance;
		}
		if ( localAmplitude )
		{
			shake.amplitude = FixedUnsigned16( localAmplitude, 1<<12 );		// 4.12 fixed
			
			MESSAGE_BEGIN( MSG_ONE, gmsgShake, NULL, pPlayer->edict() );		// use the magic #1 for "one client"
				
				WRITE_SHORT( shake.amplitude );				// shake amount
				WRITE_SHORT( shake.duration );				// shake lasts this long
				WRITE_SHORT( shake.frequency );				// shake noise frequency

			MESSAGE_END();
		}
	}
}



void UTIL_ScreenShakeAll( const Vector &center, float amplitude, float frequency, float duration )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, 0 );
}


void UTIL_ScreenFadeBuild( ScreenFade &fade, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	fade.duration = FixedUnsigned16( fadeTime, 1<<12 );		// 4.12 fixed
	fade.holdTime = FixedUnsigned16( fadeHold, 1<<12 );		// 4.12 fixed
	fade.r = (int)color.x;
	fade.g = (int)color.y;
	fade.b = (int)color.z;
	fade.a = alpha;
	fade.fadeFlags = flags;
}


void UTIL_ScreenFadeWrite( const ScreenFade &fade, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgFade, NULL, pEntity->edict() );		// use the magic #1 for "one client"
		
		WRITE_SHORT( fade.duration );		// fade lasts this long
		WRITE_SHORT( fade.holdTime );		// fade lasts this long
		WRITE_SHORT( fade.fadeFlags );		// fade type (in / out)
		WRITE_BYTE( fade.r );				// fade red
		WRITE_BYTE( fade.g );				// fade green
		WRITE_BYTE( fade.b );				// fade blue
		WRITE_BYTE( fade.a );				// fade blue

	MESSAGE_END();
}


void UTIL_ScreenFadeAll( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	int			i;
	ScreenFade	fade;


	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
	
		UTIL_ScreenFadeWrite( fade, pPlayer );
	}
}


void UTIL_ScreenFade( CBaseEntity *pEntity, const Vector &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	ScreenFade	fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );
	UTIL_ScreenFadeWrite( fade, pEntity );
}


void UTIL_HudMessage( CBaseEntity *pEntity, const hudtextparms_t &textparms, const char *pMessage )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pEntity->edict() );
		WRITE_BYTE( TE_TEXTMESSAGE );
		WRITE_BYTE( textparms.channel & 0xFF );

		WRITE_SHORT( FixedSigned16( textparms.x, 1<<13 ) );
		WRITE_SHORT( FixedSigned16( textparms.y, 1<<13 ) );
		WRITE_BYTE( textparms.effect );

		WRITE_BYTE( textparms.r1 );
		WRITE_BYTE( textparms.g1 );
		WRITE_BYTE( textparms.b1 );
		WRITE_BYTE( textparms.a1 );

		WRITE_BYTE( textparms.r2 );
		WRITE_BYTE( textparms.g2 );
		WRITE_BYTE( textparms.b2 );
		WRITE_BYTE( textparms.a2 );

		WRITE_SHORT( FixedUnsigned16( textparms.fadeinTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.fadeoutTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.holdTime, 1<<8 ) );

		if ( textparms.effect == 2 )
			WRITE_SHORT( FixedUnsigned16( textparms.fxTime, 1<<8 ) );
		
		if ( strlen( pMessage ) < 512 )
		{
			WRITE_STRING( pMessage );
		}
		else
		{
			char tmp[512];
			strncpy( tmp, pMessage, 511 );
			tmp[511] = 0;
			WRITE_STRING( tmp );
		}
	MESSAGE_END();
}

void UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage )
{
	int			i;

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_HudMessage( pPlayer, textparms, pMessage );
	}
}

					 
void UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgTextMsg );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgTextMsg, NULL, client );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void UTIL_SayText( const char *pText, CBaseEntity *pEntity )
{
	if ( !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, pEntity->edict() );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}

void UTIL_SayTextAll( const char *pText, CBaseEntity *pEntity )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}


char *UTIL_dtos1( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos2( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos3( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos4( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

void UTIL_ShowMessage( const char *pString, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgHudText, NULL, pEntity->edict() );
	WRITE_STRING( pString );
	MESSAGE_END();
}


void UTIL_ShowMessageAll( const char *pString )
{
	int		i;

	// loop through all players

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
			UTIL_ShowMessage( pString, pPlayer );
	}
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE) | (ignoreGlass?0x100:0), pentIgnore, ptr );
}


void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), pentIgnore, ptr );
}


void UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr )
{
	TRACE_HULL( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), hullNumber, pentIgnore, ptr );
}

void UTIL_TraceModel( const Vector &vecStart, const Vector &vecEnd, int hullNumber, edict_t *pentModel, TraceResult *ptr )
{
	g_engfuncs.pfnTraceModel( vecStart, vecEnd, hullNumber, pentModel, ptr );
}

void UTIL_TraceEntity( CBaseEntity *pEntity, const Vector &vecStart, const Vector &vecEnd, TraceResult *ptr )
{
	// setup the improved trace filter (don't collide with entity parent)
	pEntity->m_pRootParent = pEntity->GetRootParent();
	pEntity->m_iParentFilter = TRUE;

	if( pEntity->m_iActorType == ACTOR_DYNAMIC || pEntity->m_iActorType == ACTOR_VEHICLE )
		WorldPhysic->SweepEntity( pEntity, vecStart, vecEnd, ptr );
	else if( pEntity->pev->movetype == MOVETYPE_FLYMISSILE )
		TRACE_MONSTER_HULL( pEntity->edict(), vecStart, vecEnd, missile, pEntity->edict(), ptr ); 
	else if( pEntity->pev->solid == SOLID_TRIGGER || pEntity->pev->solid == SOLID_NOT ) // only clip against bmodels
		TRACE_MONSTER_HULL( pEntity->edict(), vecStart, vecEnd, ignore_monsters, pEntity->edict(), ptr ); 
	else TRACE_MONSTER_HULL( pEntity->edict(), vecStart, vecEnd, dont_ignore_monsters, pEntity->edict(), ptr );

	pEntity->m_iParentFilter = FALSE;
}

TraceResult UTIL_GetGlobalTrace( )
{
	TraceResult tr;

	tr.fAllSolid		= gpGlobals->trace_allsolid;
	tr.fStartSolid		= gpGlobals->trace_startsolid;
	tr.fInOpen			= gpGlobals->trace_inopen;
	tr.fInWater			= gpGlobals->trace_inwater;
	tr.flFraction		= gpGlobals->trace_fraction;
	tr.flPlaneDist		= gpGlobals->trace_plane_dist;
	tr.pHit			= gpGlobals->trace_ent;
	tr.vecEndPos		= gpGlobals->trace_endpos;
	tr.vecPlaneNormal	= gpGlobals->trace_plane_normal;
	tr.iHitgroup		= gpGlobals->trace_hitgroup;
	return tr;
}

/*
==================
UTIL_CopyTraceToGlobal

share local trace with global variables
==================
*/
void UTIL_CopyTraceToGlobal( TraceResult *trace )
{
	gpGlobals->trace_allsolid = trace->fAllSolid;
	gpGlobals->trace_startsolid = trace->fStartSolid;
	gpGlobals->trace_fraction = trace->flFraction;
	gpGlobals->trace_plane_dist = trace->flPlaneDist;
	gpGlobals->trace_inopen = trace->fInOpen;
	gpGlobals->trace_inwater = trace->fInWater;
	gpGlobals->trace_endpos = trace->vecEndPos;
	gpGlobals->trace_plane_normal = trace->vecPlaneNormal;
	gpGlobals->trace_hitgroup = trace->iHitgroup;
	gpGlobals->trace_flags = 0;	// clear trace flags

	if( !FNullEnt( trace->pHit ))
		gpGlobals->trace_ent = trace->pHit;
	else gpGlobals->trace_ent = ENT(0); // world
}

void UTIL_CopyTraceToGlobal( trace_t *trace )
{
	gpGlobals->trace_allsolid = trace->allsolid;
	gpGlobals->trace_startsolid = trace->startsolid;
	gpGlobals->trace_fraction = trace->fraction;
	gpGlobals->trace_plane_dist = trace->plane.dist;
	gpGlobals->trace_inopen = trace->inopen;
	gpGlobals->trace_inwater = trace->inwater;
	gpGlobals->trace_endpos = trace->endpos;
	gpGlobals->trace_plane_normal = trace->plane.normal;
	gpGlobals->trace_hitgroup = trace->hitgroup;
	gpGlobals->trace_flags = 0;	// clear trace flags

	if( !FNullEnt( trace->ent ))
		gpGlobals->trace_ent = trace->ent;
	else gpGlobals->trace_ent = ENT(0); // world
}

/*
=============
UTIL_ConvertTrace

convert trace_t to TraceResult
=============
*/
void UTIL_ConvertTrace( TraceResult *dst, trace_t *src )
{
	dst->fAllSolid = src->allsolid;
	dst->fStartSolid = src->startsolid;
	dst->fInOpen = src->inopen;
	dst->fInWater = src->inwater;
	dst->flFraction = src->fraction;
	dst->vecEndPos = src->endpos;
	dst->flPlaneDist = src->plane.dist;
	dst->vecPlaneNormal = src->plane.normal;
	dst->pHit = src->ent;
	dst->iHitgroup = src->hitgroup;

	// reset trace flags
	gpGlobals->trace_flags = 0;
}
	
void UTIL_SetSize( entvars_t *pev, const Vector &vecMin, const Vector &vecMax )
{
	SET_SIZE( ENT(pev), vecMin, vecMax );
}
	
float UTIL_VecToYaw( const Vector &vec )
{
	return VEC_TO_YAW(vec);
}

void UTIL_SetOrigin( CBaseEntity *pEntity, const Vector &vecOrigin )
{
	pEntity->SetLocalOrigin( vecOrigin );
	pEntity->RelinkEntity( TRUE );
}

void UTIL_SetAngles( CBaseEntity *pEntity, const Vector &vecAngles )
{
	pEntity->SetLocalAngles( vecAngles );
}

void UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount )
{
	PARTICLE_EFFECT( vecOrigin, vecDirection, (float)ulColor, (float)ulCount );
}

void UTIL_CreateAuroraSystem(CBaseEntity *pPlayer, CBaseEntity *pParent, const char *aurFile, int attachment)
{
	MESSAGE_BEGIN(pPlayer ? MSG_ONE : MSG_ALL, gmsgParticle, NULL, pPlayer ? pPlayer->pev : NULL);
	WRITE_ENTITY(pParent->entindex());
	WRITE_STRING(aurFile);
	WRITE_BYTE(attachment);
	MESSAGE_END();
}

float UTIL_Approach( float target, float value, float speed )
{
	float delta = target - value;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_ApproachAngle( float target, float value, float speed )
{
	target = UTIL_AngleMod( target );
	value = UTIL_AngleMod( value );
	
	float delta = target - value;

	// Speed is assumed to be positive
	if ( speed < 0 )
		speed = -speed;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_AngleDistance( float next, float cur )
{
	float delta = next - cur;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	return delta;
}


float UTIL_SplineFraction( float value, float scale )
{
	value = scale * value;
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}


char* UTIL_VarArgs( char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	return string;	
}
	
Vector UTIL_GetAimVector( edict_t *pent, float flSpeed )
{
	Vector tmp;
	GET_AIM_VECTOR(pent, flSpeed, tmp);
	return tmp;
}

int UTIL_IsMasterTriggered( string_t sMaster, CBaseEntity *pActivator )
{
	const char *szMaster;
	CBaseEntity *pMaster, *pOldMaster;
	int i, j, numMasters = 0;
	int MASTER_STATE = 1;
	char szBuf[128];

	if( sMaster )
	{
//		ALERT( at_console, "IsMasterTriggered(%s, %s \"%s\")\n", STRING( sMaster ), pActivator->GetClassname(), pActivator->GetTargetname());

		pMaster = NULL;

		do {
			int reverse = false;
			int found = false;

			szMaster = STRING( sMaster );
			if( szMaster[0] == '~' ) // inverse master
			{
				reverse = true;
				szMaster++;
			}

			pOldMaster = pMaster;
			pMaster = UTIL_FindEntityByTargetname( pMaster, szMaster );

			if( !pMaster )
			{
				for( i = 0; szMaster[i]; i++ )
				{
					if( szMaster[i] == '(' )
					{
						for( j = i+1; szMaster[j]; j++ )
						{
							if( szMaster[j] == ')' )
							{
								Q_strncpy( szBuf, szMaster + i + 1, (j - i) - 1 );
								szBuf[(j-i)-1] = 0;
								pActivator = UTIL_FindEntityByTargetname( NULL, szBuf );
								found = true;
								break;
							}
						}

						if( !found )
						{
							ALERT( at_error, "Missing ')' in master \"%s\"\n", szMaster );
							return FALSE;
						}
						break;
					}
				}

				if( !found && !numMasters ) // no ( found
				{
					return TRUE;
				}

				Q_strncpy( szBuf, szMaster, i );
				szBuf[i] = 0;
				pMaster = UTIL_FindEntityByTargetname( pOldMaster, szBuf );
			}

			if( pMaster )
			{
				// some entities can't be used as masters
				if( pMaster->ObjectCaps() & FCAP_NOT_MASTER )
					continue;
 
				if( pMaster->GetState( pActivator ) == STATE_ON )
				{
					if( reverse )
						return FALSE;
				}
				else
				{
					if( !reverse )
						return FALSE;
				}
				numMasters++;
			}

		} while( pMaster != NULL );
	}

	// if the entity has no master (or the master is missing), just say yes.
	return TRUE;
}

//========================================================================
// UTIL_FireTargets - supported prefix "+", "-", ">", "<".
// supported also this and self pointers - e.g. "fadein(mywall)"
//========================================================================
void UTIL_FireTargets( string_t targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( targetName != NULL_STRING )
		UTIL_FireTargets( STRING( targetName ), pActivator, pCaller, useType, value );	
}

void UTIL_FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	const char *inputTargetName = targetName;
	CBaseEntity *inputActivator = pActivator;
	CBaseEntity *pTarget = NULL;
	int i, j, found = false;
	char szBuf[128];

	if( !targetName || !*targetName )
		return;
	
	if( targetName[0] == '+' )
	{
		targetName++;
		useType = USE_ON;
	}
	else if( targetName[0] == '-' )
	{
		targetName++;
		useType = USE_OFF;
	}
 	else if( targetName[0] == '<' )
	{
		targetName++;
		useType = USE_SET;
	}
	else if( targetName[0] == '>' )
	{
		targetName++;
		useType = USE_RESET;
	}	
	else if( targetName[0] == '!' )
	{
		targetName++;
		useType = USE_REMOVE;
	}

	pTarget = UTIL_FindEntityByTargetname( pTarget, targetName, pActivator );

	// smart field name?
	if( !pTarget )
	{
		// try to extract value from name (it's more usefully than "locus" specifier)
		for( i = 0; targetName[i]; i++ )
		{
			if( targetName[i] == '.' )
			{
				// value specifier
				value = Q_atof( &targetName[i+1] );
				Q_strncpy( szBuf, targetName, sizeof( szBuf ));
				szBuf[i] = 0;
				targetName = szBuf;
				pTarget = UTIL_FindEntityByTargetname( NULL, targetName, inputActivator );						
				break;
			}
		}

		// try to extract activator specified
		if( !pTarget )
		{
			for( i = 0; targetName[i]; i++ )
			{
				if( targetName[i] == '(' )
				{
					i++;
					for( j = i; targetName[j]; j++ )
					{
						if( targetName[j] == ')' )
						{
							Q_strncpy( szBuf, targetName+i, j - i + 1 );
							pActivator = UTIL_FindEntityByTargetname( NULL, szBuf, inputActivator );
							if( !pActivator ) return; // it's a locus specifier, but the locus is invalid.
							found = true;
							break;
						}
					}

					if( !found )
						ALERT( at_error, "Missing ')' in targetname: %s", inputTargetName );
					break;
				}
			}

			// no, it's not a locus specifier.
			if( !found ) return;

			Q_strncpy( szBuf, targetName, i );
			targetName = szBuf;
			pTarget = UTIL_FindEntityByTargetname( NULL, targetName, inputActivator );

			// it's a locus specifier all right, but the target's invalid.
			if( !pTarget ) return;
		}
	}

	ALERT( at_aiconsole, "Firing: (%s) with %s and value %g\n", targetName, GetStringForUseType( useType ), value );

	//start firing targets
	do
	{
		if(!( pTarget->pev->flags & FL_KILLME ))
		{
			if( useType == USE_REMOVE ) UTIL_Remove( pTarget );
			else pTarget->Use( pActivator, pCaller, useType, value );
		}

		pTarget = UTIL_FindEntityByTargetname( pTarget, targetName, inputActivator );

	} while( pTarget != NULL );
}

BOOL UTIL_ShouldShowBlood( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			if ( CVAR_GET_FLOAT("violence_hblood") != 0 )
				return TRUE;
		}
		else
		{
			if ( CVAR_GET_FLOAT("violence_ablood") != 0 )
				return TRUE;
		}
	}
	return FALSE;
}

int UTIL_PointContents(	const Vector &vec )
{
	return POINT_CONTENTS(vec);
}

void UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BLOODSTREAM );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( direction.x );
		WRITE_COORD( direction.y );
		WRITE_COORD( direction.z );
		WRITE_BYTE( color );
		WRITE_BYTE( Q_min( amount, 255 ) );
	MESSAGE_END();
}				

void UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_Language == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRules->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 2;
	}

	if ( amount > 255 )
		amount = 255;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BLOODSPRITE );
		WRITE_COORD( origin.x);								// pos
		WRITE_COORD( origin.y);
		WRITE_COORD( origin.z);
		WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
		WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
		WRITE_BYTE( color );								// color index into host_basepal
		WRITE_BYTE( Q_min( Q_max( 3, amount / 10 ), 16 ) );		// size
	MESSAGE_END();
}				

Vector UTIL_RandomBloodVector( void )
{
	Vector direction;

	direction.x = RANDOM_FLOAT ( -1, 1 );
	direction.y = RANDOM_FLOAT ( -1, 1 );
	direction.z = RANDOM_FLOAT ( 0, 1 );

	return direction;
}


void UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor )
{
	if (UTIL_ShouldShowBlood(bloodColor))
	{
		if (bloodColor == BLOOD_COLOR_RED)
			UTIL_TraceCustomDecal(pTrace, "redblood", RANDOM_FLOAT(0.0f, 360.0f));
		else
			UTIL_TraceCustomDecal(pTrace, "yellowblood", RANDOM_FLOAT(0.0f, 360.0f));
	}
}

void UTIL_BloodStudioDecalTrace(TraceResult *pTrace, int bloodColor)
{
	if (UTIL_ShouldShowBlood(bloodColor))
	{
		if (bloodColor == BLOOD_COLOR_RED)
			UTIL_StudioDecalTrace(pTrace, "redblood"); // TODO make it more varying
		else
			UTIL_StudioDecalTrace(pTrace, "yellowblood");
	}
}

bool UTIL_TraceCustomDecal(TraceResult *pTrace, const char *name, float angle, int persistent) // Wargon: Значение по умолчанию прописано в util.h.
{
	short entityIndex;
	short modelIndex = 0;
	byte flags = 0;

	if (!name || !*name)
		return FALSE;

	if (pTrace->flFraction == 1.0)
		return FALSE;

	// Only decal BSP models
	if (pTrace->pHit)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(pTrace->pHit);
		if (pEntity && !pEntity->IsBSPModel())
			return FALSE;
		entityIndex = ENTINDEX(pTrace->pHit);
	}
	else
		entityIndex = 0;

	edict_t *pEdict = INDEXENT(entityIndex);
	if (pEdict) modelIndex = pEdict->v.modelindex;

	if (persistent)
		flags |= FDECAL_PERMANENT;

	angle = anglemod(angle);

	MESSAGE_BEGIN(persistent ? MSG_INIT : MSG_BROADCAST, gmsgCustomDecal);
	WRITE_COORD(pTrace->vecEndPos.x);
	WRITE_COORD(pTrace->vecEndPos.y);
	WRITE_COORD(pTrace->vecEndPos.z);
	WRITE_COORD(pTrace->vecPlaneNormal.x * 8192);
	WRITE_COORD(pTrace->vecPlaneNormal.y * 8192);
	WRITE_COORD(pTrace->vecPlaneNormal.z * 8192);
	WRITE_SHORT(entityIndex);
	WRITE_SHORT(modelIndex);
	WRITE_STRING(name);
	WRITE_BYTE(flags);
	WRITE_ANGLE(angle);
	MESSAGE_END();

	return TRUE;
}

void UTIL_DecalTrace(TraceResult *pTrace, const char *decalName)
{
	short entityIndex;
	int index;
	int message;

	index = DECAL_INDEX(decalName);

	if (index < 0)
		return;

	if (pTrace->flFraction == 1.0)
		return;

	// Only decal BSP models
	if ( pTrace->pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pTrace->pHit );
		if ( pEntity && !pEntity->IsBSPModel() )
			return;
		entityIndex = ENTINDEX( pTrace->pHit );
	}
	else 
		entityIndex = 0;

	message = TE_DECAL;
	if ( entityIndex != 0 )
	{
		if ( index > 255 )
		{
			message = TE_DECALHIGH;
			index -= 256;
		}
	}
	else
	{
		message = TE_WORLDDECAL;
		if ( index > 255 )
		{
			message = TE_WORLDDECALHIGH;
			index -= 256;
		}
	}
	
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( message );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_BYTE( index );
		if ( entityIndex )
			WRITE_SHORT( entityIndex );
	MESSAGE_END();
}

void UTIL_RestoreCustomDecal(const Vector &vecPos, const Vector &vecNormal, int entityIndex, int modelIndex, const char *name, int flags, float angle)
{
	MESSAGE_BEGIN(MSG_INIT, gmsgCustomDecal);
	WRITE_COORD(vecPos.x);
	WRITE_COORD(vecPos.y);
	WRITE_COORD(vecPos.z);
	WRITE_COORD(vecNormal.x * 8192);
	WRITE_COORD(vecNormal.y * 8192);
	WRITE_COORD(vecNormal.z * 8192);
	WRITE_SHORT(entityIndex);
	WRITE_SHORT(modelIndex);
	WRITE_STRING(name);
	WRITE_BYTE(flags);
	WRITE_ANGLE(angle);
	MESSAGE_END();
}

BOOL UTIL_StudioDecalTrace(TraceResult *pTrace, const char *name, int flags)
{
	short entityIndex;

	if (!name || !*name)
		return FALSE;

	if (pTrace->flFraction == 1.0f)
		return FALSE;

	CBaseEntity *pEntity;

	// Only decal Studio models
	if (pTrace->pHit)
	{
		pEntity = CBaseEntity::Instance(pTrace->pHit);

		if (!pEntity || !GET_MODEL_PTR(pEntity->edict()))
			return FALSE; // not a studiomodel?

		entityIndex = ENTINDEX(pTrace->pHit);
	}
	else
	{
		return FALSE;
	}

	Vector vecNormal = pTrace->vecPlaneNormal;
	Vector vecEnd = pTrace->vecEndPos;
	Vector scale = Vector(1.0f, 1.0f, 1.0f);

	if (FBitSet(pEntity->pev->iuser1, CF_STATIC_ENTITY))
	{
		if (pEntity->pev->startpos != g_vecZero)
			scale = Vector(1.0f / pEntity->pev->startpos.x, 1.0f / pEntity->pev->startpos.y, 1.0f / pEntity->pev->startpos.z);
	}

	// studio decals is always converted into local space to avoid troubles with precision and modelscale
	matrix4x4 mat = matrix4x4(pEntity->pev->origin, pEntity->pev->angles, scale);
	vecNormal = mat.VectorIRotate(vecNormal);
	vecEnd = mat.VectorITransform(vecEnd);
	SetBits(flags, FDECAL_LOCAL_SPACE); // now it's in local space

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgStudioDecal);
	WRITE_COORD(vecEnd.x);		// write pos
	WRITE_COORD(vecEnd.y);
	WRITE_COORD(vecEnd.z);
	WRITE_COORD(vecNormal.x * 1000.0f);	// write normal
	WRITE_COORD(vecNormal.y * 1000.0f);
	WRITE_COORD(vecNormal.z * 1000.0f);
	WRITE_SHORT(entityIndex);
	WRITE_SHORT(pEntity->pev->modelindex);
	WRITE_STRING(name);		// decal texture
	WRITE_BYTE(flags);

	// write model state for correct restore
	WRITE_SHORT(pEntity->pev->sequence);
	WRITE_SHORT((int)(pEntity->pev->frame * 8.0f));
	WRITE_BYTE(pEntity->pev->blending[0]);
	WRITE_BYTE(pEntity->pev->blending[1]);
	WRITE_BYTE(pEntity->pev->controller[0]);
	WRITE_BYTE(pEntity->pev->controller[1]);
	WRITE_BYTE(pEntity->pev->controller[2]);
	WRITE_BYTE(pEntity->pev->controller[3]);

	WRITE_BYTE(pEntity->pev->body);
	WRITE_BYTE(pEntity->pev->skin);

	if (FBitSet(pEntity->pev->iuser1, CF_STATIC_ENTITY))
		WRITE_SHORT(pEntity->pev->colormap);
	else WRITE_SHORT(0);
	MESSAGE_END();

	return TRUE;
}

void UTIL_RestoreStudioDecal(const Vector &vecEnd, const Vector &vecNormal, int entityIndex, int modelIndex, const char *name, int flags, modelstate_t *state, int lightcache, const Vector &scale)
{
	MESSAGE_BEGIN(MSG_INIT, gmsgStudioDecal);
	WRITE_COORD(vecEnd.x);		// write pos
	WRITE_COORD(vecEnd.y);
	WRITE_COORD(vecEnd.z);
	WRITE_COORD(vecNormal.x * 1000.0f);	// write normal
	WRITE_COORD(vecNormal.y * 1000.0f);
	WRITE_COORD(vecNormal.z * 1000.0f);
	WRITE_SHORT(entityIndex);
	WRITE_SHORT(modelIndex);
	WRITE_STRING(name);	// decal texture
	WRITE_BYTE(flags);

	// write model state for correct restore
	WRITE_SHORT(state->sequence);
	WRITE_SHORT(state->frame);	// already premultiplied by 8
	WRITE_BYTE(state->blending[0]);
	WRITE_BYTE(state->blending[1]);
	WRITE_BYTE(state->controller[0]);
	WRITE_BYTE(state->controller[1]);
	WRITE_BYTE(state->controller[2]);
	WRITE_BYTE(state->controller[3]);
	WRITE_BYTE(state->body);
	WRITE_BYTE(state->skin);
	WRITE_SHORT(lightcache);

	// model scale
	WRITE_COORD(scale.x * 1000.0f);
	WRITE_COORD(scale.y * 1000.0f);
	WRITE_COORD(scale.z * 1000.0f);
	MESSAGE_END();
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber )
{
	int index = decalNumber;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_PLAYERDECAL );
		WRITE_BYTE ( playernum );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_SHORT( (short)ENTINDEX(pTrace->pHit) );
		WRITE_BYTE( index );
	MESSAGE_END();
}

void UTIL_GunshotDecalTrace( TraceResult *pTrace, const char *decalName )
{
	if (pTrace->flFraction == 1.0)
		return;

	UTIL_TraceCustomDecal(pTrace, decalName);
}


void UTIL_Sparks( const Vector &position )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_SPARKS );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
	MESSAGE_END();
}

//
// Makes flagged buttons spark when turned off
//
void UTIL_DoSpark( entvars_t *pev, const Vector &location )
{
	Vector tmp = location + pev->size * 0.5;

	UTIL_Sparks( tmp );

	float flVolume = RANDOM_FLOAT( 0.25f, 0.75f ) * 0.4; // random volume range

	switch( RANDOM_LONG( 0, 5 ))
	{
	case 0: EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/spark1.wav", flVolume, ATTN_STATIC ); break;
	case 1: EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/spark2.wav", flVolume, ATTN_STATIC ); break;
	case 2: EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/spark3.wav", flVolume, ATTN_STATIC ); break;
	case 3: EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/spark4.wav", flVolume, ATTN_STATIC ); break;
	case 4: EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_STATIC ); break;
	case 5: EMIT_SOUND( ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_STATIC ); break;
	}
}

void UTIL_Ricochet( const Vector &position, float scale )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_ARMOR_RICOCHET );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
		WRITE_BYTE( (int)(scale*10) );
	MESSAGE_END();
}


BOOL UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return TRUE;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !stricmp( pTeamName1, pTeamName2 ) )	// Same Team?
			return TRUE;
	}

	return FALSE;
}

//LRC - moved here from barney.cpp
BOOL UTIL_IsFacing( entvars_t *pevTest, const Vector &reference )
{
	Vector vecDir = (reference - pevTest->origin);
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	Vector forward, angle;
	angle = pevTest->v_angle;
	angle.x = 0;
	UTIL_MakeVectorsPrivate( angle, forward, NULL, NULL );
	// He's facing me, he meant it
	if ( DotProduct( forward, vecDir ) > 0.96 )	// +/- 15 degrees or so
	{
		return TRUE;
	}
	return FALSE;
}

void UTIL_StringToVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j+1;j < 3; j++)
			pVector[j] = 0;
	}
}


void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

Vector UTIL_ClampVectorToBox( const Vector &input, const Vector &clampSize )
{
	Vector sourceVector = input;

	if ( sourceVector.x > clampSize.x )
		sourceVector.x -= clampSize.x;
	else if ( sourceVector.x < -clampSize.x )
		sourceVector.x += clampSize.x;
	else
		sourceVector.x = 0;

	if ( sourceVector.y > clampSize.y )
		sourceVector.y -= clampSize.y;
	else if ( sourceVector.y < -clampSize.y )
		sourceVector.y += clampSize.y;
	else
		sourceVector.y = 0;
	
	if ( sourceVector.z > clampSize.z )
		sourceVector.z -= clampSize.z;
	else if ( sourceVector.z < -clampSize.z )
		sourceVector.z += clampSize.z;
	else
		sourceVector.z = 0;

	return sourceVector.Normalize();
}


float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z = minz;

	if (UTIL_PointContents(midUp) != CONTENTS_WATER)
		return minz;

	midUp.z = maxz;
	if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff/2.0;
		if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}


extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model

void UTIL_Bubbles( Vector mins, Vector maxs, int count )
{
	Vector mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, mid );
		WRITE_BYTE( TE_BUBBLES );
		WRITE_COORD( mins.x );	// mins
		WRITE_COORD( mins.y );
		WRITE_COORD( mins.z );
		WRITE_COORD( maxs.x );	// maxz
		WRITE_COORD( maxs.y );
		WRITE_COORD( maxs.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}

void UTIL_BubbleTrail( Vector from, Vector to, int count )
{
	float flHeight = UTIL_WaterLevel( from,  from.z, from.z + 256 );
	flHeight = flHeight - from.z;

	if (flHeight < 8)
	{
		flHeight = UTIL_WaterLevel( to,  to.z, to.z + 256 );
		flHeight = flHeight - to.z;
		if (flHeight < 8)
			return;

		// UNDONE: do a ploink sound
		flHeight = flHeight + to.z - from.z;
	}

	if (count > 255) 
		count = 255;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BUBBLETRAIL );
		WRITE_COORD( from.x );	// mins
		WRITE_COORD( from.y );
		WRITE_COORD( from.z );
		WRITE_COORD( to.x );	// maxz
		WRITE_COORD( to.y );
		WRITE_COORD( to.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}

/*
QuakeEd only writes a single float for angles (bad idea),
so up and down are just constant angles.
*/
void UTIL_SetMovedir( CBaseEntity *pEnt )
{
	Vector angles = pEnt->pev->angles;
	pEnt->pev->angles = g_vecZero;

	if( angles == Vector( 0, -1, 0 ))
	{
		pEnt->SetMoveDir( Vector( 0, 0, 1 ));
	}
	else if( angles == Vector( 0, -2, 0 ))
	{
		pEnt->SetMoveDir( Vector( 0, 0, -1 ));
	}
	else
	{
		UTIL_MakeVectors( angles );
		pEnt->SetMoveDir( gpGlobals->v_forward );
	}
}

void UTIL_Remove( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return;

	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
}


BOOL UTIL_IsValidEntity( edict_t *pent )
{
	if ( !pent || pent->free || (pent->v.flags & FL_KILLME) )
		return FALSE;
	return TRUE;
}


void UTIL_PrecacheOther( const char *szClassname )
{
	CBaseEntity *pEntity = CreateEntityByName( szClassname );

	if( !pEntity )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}

	pEntity->Precache( );
	REMOVE_ENTITY( pEntity->edict( ));
}

int UTIL_PrecacheSound( string_t s )
{
	return UTIL_PrecacheSound( STRING( s ));
}

int UTIL_PrecacheSound( const char* s )
{
	if( !s || !*s )
	{
		ALERT( at_warning, "UTIL_PrecacheSound: sound not specified!\n" );
		return MAKE_STRING( "common/null.wav" ); // set null sound
	}

	if( *s == '!' )
	{
		// sentence - just make string
          	return MAKE_STRING( s );
	}

	char path[256];
	
	// NOTE: Engine function as predicted for sound folder
	// But COMPARE_FILE_TIME don't known about this. Set it manualy
	Q_snprintf( path, sizeof( path ), "sound/%s", s );

	int iCompare;

	// verify file exists
	// g-cont. idea! use COMPARE_FILE_TIME instead of LOAD_FILE_FOR_ME
	if( COMPARE_FILE_TIME( path, path, &iCompare ))
	{
		g_engfuncs.pfnPrecacheSound( s );
		return MAKE_STRING( s );
	}

	const char *ext = UTIL_FileExtension( s );

	if( FStrEq( ext, "wav" ) || FStrEq( ext, "mp3" ))
		ALERT( at_warning, "sound \"%s\" not found!\n", path );
	else ALERT( at_error, "invalid name \"%s\"!\n", s );

	return MAKE_STRING( "common/null.wav" ); // set null sound
}

unsigned short UTIL_PrecacheMovie( string_t iString, int allow_sound )
{
	return UTIL_PrecacheMovie( STRING( iString ), allow_sound );
}

unsigned short UTIL_PrecacheMovie( const char *s, int allow_sound )
{
	int iCompare;
	char path[64], temp[64];

	Q_snprintf( path, sizeof( path ), "media/%s", s );
	Q_snprintf( temp, sizeof( temp ), "%s%s", allow_sound ? "*" : "", s );

	// verify file exists
	// g-cont. idea! use COMPARE_FILE_TIME instead of LOAD_FILE_FOR_ME
	if( COMPARE_FILE_TIME( path, path, &iCompare ))
	{
		return g_engfuncs.pfnPrecacheGeneric( temp );
	}

	ALERT( at_console, "Warning: video (%s) not found!\n", path );

	return 0;
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( char *fmt, ... )
{
	va_list			argptr;
	static char		string[1024];
	
	va_start ( argptr, fmt );
	vsprintf ( string, fmt, argptr );
	va_end   ( argptr );

	// Print to server console
	ALERT( at_logged, "%s", string );
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir )
{
	Vector2D	vec2LOS;

	vec2LOS = ( vecCheck - vecSrc ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	return DotProduct (vec2LOS , ( vecDir.Make2D() ) );
}


//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest )
{
	int i = 0;

	while ( pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}

void UTIL_SetView( string_t iszViewEntity )
{
	if( g_pGameRules->IsMultiplayer())
		return;

	CBaseEntity *pPlayer = UTIL_PlayerByIndex( 1 );
	UTIL_SetView( pPlayer, iszViewEntity );
}

void UTIL_SetView( CBaseEntity *pActivator, string_t iszViewEntity )
{
          CBaseEntity *pViewEnt = NULL;
          
	if( iszViewEntity )
	{
		pViewEnt = UTIL_FindEntityByString( NULL, "targetname", STRING( iszViewEntity ));
		if( !pViewEnt ) pViewEnt = UTIL_FindEntityByString( NULL, "classname", STRING( iszViewEntity ));
	}

	UTIL_SetView( pActivator, pViewEnt );
}

void UTIL_SetView( CBaseEntity *pActivator, CBaseEntity *pViewEnt )
{
	if( !pActivator || !pActivator->IsPlayer( ))
	{
		if( !pActivator )
			ALERT( at_error, "UTIL_SetView: pActivator == NULL!\n" );
		else ALERT( at_error, "UTIL_SetView: pActivator not a player!\n" ); 
		return;
	}

	if( !pViewEnt ) pViewEnt = pActivator;
	SET_VIEW( pActivator->edict(), pViewEnt->edict() );
}

//=========================================================
// UTIL_FindClientInPVS - find a first visible client
//=========================================================
CBaseEntity *UTIL_FindClientInPVS( edict_t *pEdict )
{
	edict_t *client = FIND_CLIENT_IN_PVS (pEdict);

	if( !FNullEnt(client))
	{
		return CBaseEntity::Instance( client );
	}

	return NULL;
}

modtype_t UTIL_GetModelType( int modelindex )
{
	if( !g_fPhysicInitialized )
		return mod_bad;

	model_t *mod = (model_t *)MODEL_HANDLE( modelindex );
	if( mod )
		return (modtype_t)mod->type;

	return mod_bad;
}

angletype_t UTIL_GetSpriteType( int modelindex )
{
	if( UTIL_GetModelType( modelindex ) != mod_sprite )
		return (angletype_t)-1; // bad sprite

	model_t *mod = (model_t *)MODEL_HANDLE( modelindex );
	msprite_t *spr = (msprite_t *)mod->cache.data;

	return (angletype_t)spr->type;
}

BOOL UITL_ExternalBmodel( int modelindex )
{
	if( !g_fPhysicInitialized || modelindex <= 1 )
		return FALSE;

	model_t *mod = (model_t *)MODEL_HANDLE( modelindex );

	// brush models can be stored as individual bsp-files and go across the transition
	if( mod && mod->type == mod_brush && mod->name[0] != '*' )
		return TRUE;

	return FALSE;	
}

/*
===================
UTIL_GetModelBounds
===================
*/
void UTIL_GetModelBounds( int modelIndex, Vector &mins, Vector &maxs )
{
	if( modelIndex <= 0 ) return;

	model_t *mod = (model_t *)MODEL_HANDLE( modelIndex );

	if( mod )
	{
		mins = mod->mins;
		maxs = mod->maxs;
	}
	else
	{
		ALERT( at_error, "UTIL_GetModelBounds: NULL model %i\n", modelIndex );
		mins = maxs = g_vecZero;
	}
}

void UTIL_SetSize( CBaseEntity *pEntity, const Vector &min, const Vector &max )
{
	// check bounds
	if( min.x > max.x || min.y > max.y || min.z > max.z )
	{
		ALERT( at_error, "UTIL_SetSize: %s backwards mins/maxs\n", pEntity->GetClassname( ));
		return;
	}

	pEntity->pev->size = (max - min);
	pEntity->pev->mins = min;
	pEntity->pev->maxs = max;

	pEntity->RelinkEntity( FALSE, NULL );
}

/*
============
UTIL_CanRotate

Allows to change entity angles?
NOTE: some bmodels without origin brush can't be rotated properly
============
*/
BOOL UTIL_CanRotate( CBaseEntity *pEntity )
{
	model_t	*mod;

	if( !g_fPhysicInitialized )
		return TRUE;

	mod = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( !mod || mod->type != mod_brush )
		return TRUE;

	// NOTE: flag 2 it's a internal engine flag (see model.c for details)
	// we can recalc real model origin here but this check is faster :)
	return (mod->flags & 2) ? TRUE : FALSE;
}

/*
============
UTIL_CanRotateBmodel

Allows to change entity angles?
NOTE: some bmodels without origin brush can't be rotated properly
============
*/
BOOL UTIL_CanRotateBModel( CBaseEntity *pEntity )
{
	model_t	*mod;

	if( !g_fPhysicInitialized )
		return (pEntity->pev->solid == SOLID_BSP);

	mod = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( !mod || mod->type != mod_brush )
		return FALSE;

	// NOTE: flag 2 it's a internal engine flag (see model.c for details)
	// we can recalc real model origin here but this check is faster :)
	return (mod->flags & 2) ? TRUE : FALSE;
}

/*
============
UTIL_AllowHitboxTrace

Allows to trace this entity by hitboxes?
============
*/
BOOL UTIL_AllowHitboxTrace( CBaseEntity *pEntity )
{
	model_t	*mod;

	if( !g_fPhysicInitialized )
		return (pEntity->pev->solid == SOLID_BBOX);

	mod = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );
	if( !mod || mod->type != mod_studio )
		return FALSE;

	// NOTE: flag 2 it's a internal engine flag (see model.c for details)
	// we can recalc real model origin here but this check is faster :)
	return (mod->flags & STUDIO_TRACE_HITBOX) ? TRUE : FALSE;
}

/*
====================
UTIL_RecursiveWalkNodes

Mins and maxs enclose the entire area swept by the move
====================
*/
static void UTIL_RecursiveWalkNodes( areaclip_t *clip, areanode_t *node )
{
	link_t	*l, *start, *next;

	// touch linked edicts
	if( clip->area_type == AREA_SOLID )
		start = &node->solid_edicts;
	else if( clip->area_type == AREA_TRIGGERS )
		start = &node->trigger_edicts;
	else if( clip->area_type == AREA_WATER )
		start = &node->solid_edicts;
	else return;

	// touch linked edicts
	for( l = start->next; l != start; l = next )
	{
		next = l->next;

		CBaseEntity *pCheck = CBaseEntity :: Instance( EDICT_FROM_AREA( l ));

		if( clip->area_type == AREA_SOLID && pCheck->pev->solid == SOLID_NOT )
			continue;

		if( clip->area_type == AREA_WATER && pCheck->pev->solid != SOLID_NOT )
			continue;

		if( pCheck && pCheck->AreaIntersect( clip->vecAbsMin, clip->vecAbsMax ))
		{
			if( clip->m_pfnCallback )
				clip->m_pfnCallback( pCheck );
		}
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->vecAbsMax[node->axis] > node->dist )
		UTIL_RecursiveWalkNodes( clip, node->children[0] );
	if( clip->vecAbsMin[node->axis] < node->dist )
		UTIL_RecursiveWalkNodes( clip, node->children[1] );
}

void UTIL_AreaNode(	Vector vecAbsMin, Vector vecAbsMax, int type, AREACHECK pfnCallback )
{
	areaclip_t clip;

	clip.vecAbsMin = vecAbsMin;
	clip.vecAbsMax = vecAbsMax;
	clip.m_pfnCallback = pfnCallback;
	clip.area_type = type;

	UTIL_RecursiveWalkNodes( &clip, GET_AREANODE() );
}

//-----------------------------------------------------------------------------
// Drops an entity onto the floor
//-----------------------------------------------------------------------------
int UTIL_DropToFloor( CBaseEntity *pEntity )
{
	ASSERT( pEntity != NULL );

	// Assume no ground
//	pEntity->ClearGroundEntity();

	TraceResult trace;
	Vector vecStart = pEntity->GetAbsOrigin();
	Vector vecEnd = pEntity->GetAbsOrigin() - Vector( 0, 0, 256 );

	if( FBitSet( pEntity->pev->iuser1, CF_STATIC_ENTITY )) TRACE_LINE( vecStart, vecEnd, TRUE, pEntity->edict(), &trace );
	else TRACE_MONSTER_HULL( pEntity->edict(), vecStart, vecEnd, dont_ignore_monsters, pEntity->edict(), &trace );

	if( trace.fAllSolid )
		return -1;

	if( trace.flFraction == 1.0f )
		return 0;

	pEntity->SetAbsOrigin( trace.vecEndPos );
	pEntity->SetGroundEntity( trace.pHit );

	return 1;
}

/*
==================
UTIL_HullForBsp

forcing to select BSP hull
==================
*/
hull_t *UTIL_HullForBsp( CBaseEntity *pEntity, const Vector &mins, const Vector &maxs, Vector &offset )
{
	hull_t *hull;
	model_t *model;
	Vector size;
			
	// decide which clipping hull to use, based on the size
	model = (model_t *)MODEL_HANDLE( pEntity->pev->modelindex );

	if( !model || model->type != mod_brush )
		HOST_ERROR( "Entity %i SOLID_BSP with a non bsp model %i\n", pEntity->entindex(), model->type );

	size = maxs - mins;

	if( size[0] <= 8.0f || model->flags & 4 ) // a MODEL_LIQUID flag
	{
		hull = &model->hulls[0];
		offset = hull->clip_mins; 
	}
	else
	{
		if( size[0] <= 36.0f )
		{
			if( size[2] <= 36.0f )
				hull = &model->hulls[3];
			else hull = &model->hulls[1];
		}
		else hull = &model->hulls[2];

		offset = hull->clip_mins - mins;
	}

	offset += pEntity->GetAbsOrigin();

	return hull;
}

/*
==================
UTIL_HullPointContents

==================
*/
int UTIL_HullPointContents( hull_t *hull, int num, const Vector &p )
{
	mplane_t *plane;

	if( !hull || !hull->planes )	// in case we restore saved game while map is changed
		return CONTENTS_NONE;

	while( num >= 0 )
	{
		plane = &hull->planes[hull->clipnodes[num].planenum];
		num = hull->clipnodes[num].children[PlaneDiff( p, plane ) < 0];
	}
	return num;
}

trace_t UTIL_CombineTraces( trace_t *cliptrace, trace_t *trace, CBaseEntity *pTouch )
{
	if( trace->allsolid || trace->startsolid || trace->fraction < cliptrace->fraction )
	{
		trace->ent = pTouch->edict();
		
		if( cliptrace->startsolid )
		{
			*cliptrace = *trace;
			cliptrace->startsolid = true;
		}
		else *cliptrace = *trace;
	}

	return *cliptrace;
}

void UTIL_WaterMove( CBaseEntity *pEntity )
{
	float	drownlevel;
	int	waterlevel;
	int	watertype;
	int	flags;

	if( pEntity->pev->movetype == MOVETYPE_NOCLIP )
	{
		pEntity->pev->air_finished = gpGlobals->time + 12.0f;
		return;
	}

	if(( pEntity->pev->flags & FL_MONSTER ) && pEntity->pev->health <= 0.0f )
		return;

	drownlevel = (pEntity->pev->deadflag == DEAD_NO) ? 3.0 : 1.0;
	waterlevel = pEntity->pev->waterlevel;
	watertype = pEntity->pev->watertype;
	flags = pEntity->pev->flags;

	if( !( flags & FL_GODMODE ))
	{
		if((( flags & FL_SWIM ) && waterlevel > drownlevel ) || waterlevel <= drownlevel )
		{
			if( pEntity->pev->air_finished > gpGlobals->time && pEntity->pev->pain_finished > gpGlobals->time )
			{
				pEntity->pev->dmg += 2;

				if( pEntity->pev->dmg < 15 )
					pEntity->pev->dmg = 10; // quake1 original code
				pEntity->pev->pain_finished = gpGlobals->time + 1.0f;
			}
		}
		else
		{
			pEntity->pev->air_finished = gpGlobals->time + 12.0f;
			pEntity->pev->dmg = 2;
		}
	}

	if( !waterlevel )
	{
		if( flags & FL_INWATER )
		{
			// leave the water.
			switch( RANDOM_LONG( 0, 3 ))
			{
			case 0:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade1.wav", 1.0f, ATTN_NORM );
				break;
			case 1:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade2.wav", 1.0f, ATTN_NORM );
				break;
			case 2:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade3.wav", 1.0f, ATTN_NORM );
				break;
			case 3:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade4.wav", 1.0f, ATTN_NORM );
				break;
			}

			pEntity->pev->flags = flags & ~FL_INWATER;
		}

		pEntity->pev->air_finished = gpGlobals->time + 12.0f;
		return;
	}

	if( watertype == CONTENTS_LAVA )
	{
		if((!( flags & FL_GODMODE )) && pEntity->pev->dmgtime < gpGlobals->time )
		{
			if( pEntity->pev->radsuit_finished < gpGlobals->time )
				pEntity->pev->dmgtime = gpGlobals->time + 0.2f;
			else pEntity->pev->dmgtime = gpGlobals->time + 1.0f;
		}
	}
	else if( watertype == CONTENTS_SLIME )
	{
		if((!( flags & FL_GODMODE )) && pEntity->pev->dmgtime < gpGlobals->time )
		{
			if( pEntity->pev->radsuit_finished < gpGlobals->time )
				pEntity->pev->dmgtime = gpGlobals->time + 1.0;
			// otherwise radsuit is fully protect entity from slime
		}
	}

	if(!( flags & FL_INWATER ))
	{
		if( watertype == CONTENTS_WATER )
		{
			// entering the water
			switch( RANDOM_LONG( 0, 3 ))
			{
			case 0:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade1.wav", 1.0f, ATTN_NORM );
				break;
			case 1:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade2.wav", 1.0f, ATTN_NORM );
				break;
			case 2:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade3.wav", 1.0f, ATTN_NORM );
				break;
			case 3:
				EMIT_SOUND( pEntity->edict(), CHAN_BODY, "player/pl_wade4.wav", 1.0f, ATTN_NORM );
				break;
			}
		}

		pEntity->pev->flags = flags | FL_INWATER;
		pEntity->pev->dmgtime = 0.0f;
	}

	if(!( flags & FL_WATERJUMP ) && !pEntity->IsRigidBody( ))
	{
		pEntity->pev->velocity += pEntity->pev->velocity * ( pEntity->pev->waterlevel * -0.8f * gpGlobals->frametime );
	}
}

int UTIL_LoadSoundPreset( int iString )
{
	if( iString != NULL_STRING )
	{
		const char *pString = STRING( iString );

		if( Q_isdigit( pString ))
			return Q_atoi( pString );
	}

	return iString;
}

/*
====================
Sys LoadGameDLL

====================
*/
bool Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunc_t *fcts )
{
	if( !handle ) return false;

	const dllfunc_t *gamefunc;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->func = NULL;

	char dllpath[128];
	char szDirName[MAX_PATH];

	GET_GAME_DIR( szDirName );

	// is direct path used ?
	if( dllname[0] == '*' ) Q_strncpy( dllpath, dllname + 1, sizeof( dllpath ));
	else Q_snprintf( dllpath, sizeof( dllpath ), "%s/bin/%s", szDirName, dllname );

	dllhandle_t dllhandle = LoadLibrary( dllpath );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
	{
		if( !( *gamefunc->func = (void *)Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_FreeLibrary( &dllhandle );
			return false;
		}
	}          

	ALERT( at_aiconsole, "%s loaded succesfully!\n", (dllname[0] == '*') ? (dllname+1) : (dllname));
	*handle = dllhandle;

	return true;
}

void *Sys_GetProcAddress( dllhandle_t handle, const char *name )
{
	return (void *)GetProcAddress( handle, name );
}

void Sys_FreeLibrary( dllhandle_t *handle )
{
	if( !handle || !*handle )
		return;

	FreeLibrary( *handle );
	*handle = NULL;
}
