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

===== world.cpp ========================================================

  precaches and defs for entities and other data that must always be available.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "soundent.h"
#include "client.h"
#include "decals.h"
#include "skill.h"
#include "effects.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "teamplay_gamerules.h"
#include "physcallback.h"
#include "meshdesc_factory.h"
#include "sv_materials.h"
#include "env_message.h"

extern CGraph WorldGraph;
extern CSoundEnt *pSoundEnt;

CBaseEntity			*g_pWorld;
extern CBaseEntity			*g_pLastSpawn;
DLL_GLOBAL edict_t			*g_pBodyQueueHead;
CGlobalState			gGlobalState;
extern DLL_GLOBAL	int		gDisplayTitle;

extern void W_Precache(void);

class CPhysicNull : public IPhysicLayer
{
public:
	virtual void	InitPhysic( void ) {}
	virtual void	FreePhysic( void ) {}
	virtual bool	Initialized( void ) { return false; }
	virtual void	*GetUtilLibrary( void ) { return NULL; } 
	virtual void	Update( float flTime ) {}
	virtual void	EndFrame( void ) {}
	virtual void	RemoveBody( struct edict_s *pEdict ) {}
	virtual void	RemoveBody( const void *pBody ) {}
	virtual void	*CreateBodyFromEntity( CBaseEntity *pEntity ) { return NULL; }
	virtual void	*CreateBoxFromEntity( CBaseEntity *pObject ) { return NULL; }
	virtual void	*CreateKinematicBodyFromEntity( CBaseEntity *pEntity ) { return NULL; }
	virtual void	*CreateStaticBodyFromEntity( CBaseEntity *pObject ) { return NULL; }
	virtual void	*CreateTriggerFromEntity( CBaseEntity *pEntity ) { return NULL; }
	virtual void	*CreateVehicle( CBaseEntity *pObject, string_t scriptName = 0 ) { return NULL; }
	virtual void	*RestoreBody( CBaseEntity *pEntity ) { return NULL; }
	virtual void	SaveBody( CBaseEntity *pObject ) {}
	virtual void	SetOrigin( CBaseEntity *pEntity, const Vector &origin ) {}
	virtual void	SetAngles( CBaseEntity *pEntity, const Vector &angles ) {}
	virtual void	SetVelocity( CBaseEntity *pEntity, const Vector &velocity ) {}
	virtual void	SetAvelocity( CBaseEntity *pEntity, const Vector &velocity ) {}
	virtual void	MoveObject( CBaseEntity *pEntity, const Vector &finalPos ) {}
	virtual void	RotateObject( CBaseEntity *pEntity, const Vector &finalAngle ) {}
	virtual void	SetLinearMomentum( CBaseEntity *pEntity, const Vector &velocity ) {}
	virtual void	AddImpulse( CBaseEntity *pEntity, const Vector &impulse, const Vector &position, float factor ) {}
	virtual void	AddForce( CBaseEntity *pEntity, const Vector &force ) {}
	virtual void	EnableCollision( CBaseEntity *pEntity, int fEnable ) {}
	virtual void	MakeKinematic( CBaseEntity *pEntity, int fEnable ) {}
	virtual void	UpdateVehicle( CBaseEntity *pObject ) {}
	virtual int	FLoadTree( char *szMapName ) { return 0; }
	virtual int	CheckBINFile( char *szMapName ) { return 0; }
	virtual int	BuildCollisionTree( char *szMapName ) { return 0; }
	virtual bool	UpdateEntityTransform( CBaseEntity *pEntity ) { return false; }
	virtual void	UpdateEntityAABB( CBaseEntity *pEntity ) {}
	virtual bool	UpdateActorPos( CBaseEntity *pEntity ) { return false; };
	virtual void	SetupWorld( void ) {}
	virtual void	FreeWorld( void ) {}
	virtual void	DebugDraw( void ) {}
	virtual void	DrawPSpeeds( void ) {}
	virtual void	TeleportCharacter( CBaseEntity *pEntity ) {}
	virtual void	TeleportActor( CBaseEntity *pEntity ) {}
	virtual void	MoveCharacter( CBaseEntity *pEntity ) {}
	virtual void	MoveKinematic( CBaseEntity *pEntity ) {}
	virtual void	SweepTest( CBaseEntity*, const Vector&, const Vector&, const Vector&, const Vector&, trace_t *tr ) { tr->allsolid = 0; }
	virtual void	SweepEntity( CBaseEntity*, const Vector &, const Vector &, TraceResult *tr ) { tr->fAllSolid = 0, tr->flFraction = 1.0f; }
	virtual bool	IsBodySleeping( CBaseEntity *pEntity ) { return true; } // entity is always sleeping while physics is not installed
	virtual void	*GetCookingInterface( void ) { return NULL; }
	virtual void	*GetPhysicInterface( void ) { return NULL; }
};

CPhysicNull NullPhysic;

#ifndef USE_PHYSICS_ENGINE
IPhysicLayer *WorldPhysic = &NullPhysic;
#endif// USE_PHYSICS_ENGINE

void GameInitNullPhysics( void )
{
	WorldPhysic = &NullPhysic;
}

//
// This must match the list in util.h
//
DLL_DECALLIST gDecals[] = {
	{ "{shot1",	0 },		// DECAL_GUNSHOT1 
	{ "{shot2",	0 },		// DECAL_GUNSHOT2
	{ "{shot3",0 },			// DECAL_GUNSHOT3
	{ "{shot4",	0 },		// DECAL_GUNSHOT4
	{ "{shot5",	0 },		// DECAL_GUNSHOT5
	{ "{lambda01", 0 },		// DECAL_LAMBDA1
	{ "{lambda02", 0 },		// DECAL_LAMBDA2
	{ "{lambda03", 0 },		// DECAL_LAMBDA3
	{ "{lambda04", 0 },		// DECAL_LAMBDA4
	{ "{lambda05", 0 },		// DECAL_LAMBDA5
	{ "{lambda06", 0 },		// DECAL_LAMBDA6
	{ "{scorch1", 0 },		// DECAL_SCORCH1
	{ "{scorch2", 0 },		// DECAL_SCORCH2
	{ "{blood1", 0 },		// DECAL_BLOOD1
	{ "{blood2", 0 },		// DECAL_BLOOD2
	{ "{blood3", 0 },		// DECAL_BLOOD3
	{ "{blood4", 0 },		// DECAL_BLOOD4
	{ "{blood5", 0 },		// DECAL_BLOOD5
	{ "{blood6", 0 },		// DECAL_BLOOD6
	{ "{yblood1", 0 },		// DECAL_YBLOOD1
	{ "{yblood2", 0 },		// DECAL_YBLOOD2
	{ "{yblood3", 0 },		// DECAL_YBLOOD3
	{ "{yblood4", 0 },		// DECAL_YBLOOD4
	{ "{yblood5", 0 },		// DECAL_YBLOOD5
	{ "{yblood6", 0 },		// DECAL_YBLOOD6
	{ "{break1", 0 },		// DECAL_GLASSBREAK1
	{ "{break2", 0 },		// DECAL_GLASSBREAK2
	{ "{break3", 0 },		// DECAL_GLASSBREAK3
	{ "{bigshot1", 0 },		// DECAL_BIGSHOT1
	{ "{bigshot2", 0 },		// DECAL_BIGSHOT2
	{ "{bigshot3", 0 },		// DECAL_BIGSHOT3
	{ "{bigshot4", 0 },		// DECAL_BIGSHOT4
	{ "{bigshot5", 0 },		// DECAL_BIGSHOT5
	{ "{spit1", 0 },		// DECAL_SPIT1
	{ "{spit2", 0 },		// DECAL_SPIT2
	{ "{bproof1", 0 },		// DECAL_BPROOF1
	{ "{gargstomp", 0 },	// DECAL_GARGSTOMP1,	// Gargantua stomp crack
	{ "{smscorch1", 0 },	// DECAL_SMALLSCORCH1,	// Small scorch mark
	{ "{smscorch2", 0 },	// DECAL_SMALLSCORCH2,	// Small scorch mark
	{ "{smscorch3", 0 },	// DECAL_SMALLSCORCH3,	// Small scorch mark
	{ "{mommablob", 0 },	// DECAL_MOMMABIRTH		// BM Birth spray
	{ "{mommablob", 0 },	// DECAL_MOMMASPLAT		// BM Mortar spray?? need decal
};

string_t GetStdLightStyle( int iStyle )
{
	switch (iStyle)
	{
	// 0 normal
	case 0: return MAKE_STRING("m");

	// 1 FLICKER (first variety)
	case 1: return MAKE_STRING("mmnmmommommnonmmonqnmmo");

	// 2 SLOW STRONG PULSE
	case 2: return MAKE_STRING("abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

	// 3 CANDLE (first variety)
	case 3: return MAKE_STRING("mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

	// 4 FAST STROBE
	case 4: return MAKE_STRING("mamamamamama");
	
	// 5 GENTLE PULSE 1
	case 5: return MAKE_STRING("jklmnopqrstuvwxyzyxwvutsrqponmlkj");
	
	// 6 FLICKER (second variety)
	case 6: return MAKE_STRING("nmonqnmomnmomomno");
	
	// 7 CANDLE (second variety)
	case 7: return MAKE_STRING("mmmaaaabcdefgmmmmaaaammmaamm");
	
	// 8 CANDLE (third variety)
	case 8: return MAKE_STRING("mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");
	
	// 9 SLOW STROBE (fourth variety)
	case 9: return MAKE_STRING("aaaaaaaazzzzzzzz");
	
	// 10 FLUORESCENT FLICKER
	case 10: return MAKE_STRING("mmamammmmammamamaaamammma");

	// 11 SLOW PULSE NOT FADE TO BLACK
	case 11: return MAKE_STRING("abcdefghijklmnopqrrqponmlkjihgfedcba");
	
	// 12 UNDERWATER LIGHT MUTATION
	// this light only distorts the lightmap - no contribution
	// is made to the brightness of affected surfaces
	case 12: return MAKE_STRING("mmnnmmnnnmmnn");

	// 13 OFF (LRC)
	case 13: return MAKE_STRING("a");

	// 14 SLOW FADE IN (LRC)
	case 14: return MAKE_STRING("aabbccddeeffgghhiijjkkllmmmmmmmmmmmmmm");

	// 15 MED FADE IN (LRC)
	case 15: return MAKE_STRING("abcdefghijklmmmmmmmmmmmmmmmmmmmmmmmmmm");

	// 16 FAST FADE IN (LRC)
	case 16: return MAKE_STRING("acegikmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm");

	// 17 SLOW FADE OUT (LRC)
	case 17: return MAKE_STRING("llkkjjiihhggffeeddccbbaaaaaaaaaaaaaaaa");

	// 18 MED FADE OUT (LRC)
	case 18: return MAKE_STRING("lkjihgfedcbaaaaaaaaaaaaaaaaaaaaaaaaaaa");

	// 19 FAST FADE OUT (LRC)
	case 19: return MAKE_STRING("kigecaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

	default: return MAKE_STRING("m");
	}
}

/*
==============================================================================

BODY QUE

==============================================================================
*/

#define SF_DECAL_NOTINDEATHMATCH		2048

class CDecal : public CBaseEntity
{
	DECLARE_CLASS( CDecal, CBaseEntity );
public:
	void	Spawn( void );
	void	KeyValue( KeyValueData *pkvd );
	void	StaticDecal( void );
	void	TriggerDecal( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( infodecal, CDecal );

BEGIN_DATADESC( CDecal )
	DEFINE_FUNCTION( StaticDecal ),
	DEFINE_FUNCTION( TriggerDecal ),
END_DATADESC()

// UNDONE:  These won't get sent to joining players in multi-player
void CDecal :: Spawn( void )
{
	if ( pev->skin < 0 || (gpGlobals->deathmatch && FBitSet( pev->spawnflags, SF_DECAL_NOTINDEATHMATCH )) )
	{
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if ( FStringNull ( pev->targetname ) )
	{
		SetThink( &CDecal::StaticDecal );
		// if there's no targetname, the decal will spray itself on as soon as the world is done spawning.
		pev->nextthink = gpGlobals->time;
	}
	else
	{
		// if there IS a targetname, the decal sprays itself on when it is triggered.
		SetThink( &CBaseEntity::SUB_DoNothing );
		SetUse( &CDecal::TriggerDecal);
	}
}

void CDecal :: TriggerDecal ( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// this is set up as a USE function for infodecals that have targetnames, so that the
	// decal doesn't get applied until it is fired. (usually by a scripted sequence)
	TraceResult trace;
	int entityIndex;
	Vector vecOrigin = GetAbsOrigin();

	UTIL_TraceLine( vecOrigin - Vector( 5, 5, 5 ), vecOrigin + Vector( 5, 5, 5 ), ignore_monsters, edict(), &trace );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_SHORT( (int)pev->skin );
		entityIndex = (short)ENTINDEX( trace.pHit );
		WRITE_SHORT( entityIndex );
		if( entityIndex )
			WRITE_SHORT( (int)VARS( trace.pHit )->modelindex );
	MESSAGE_END();

	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( 0.1 );
}


void CDecal :: StaticDecal( void )
{
	TraceResult trace;
	int entityIndex, modelIndex;
	Vector vecOrigin = GetAbsOrigin();

	UTIL_TraceLine( vecOrigin - Vector( 5, 5, 5 ), vecOrigin + Vector( 5, 5, 5 ), ignore_monsters, edict(), &trace );

	entityIndex = (short)ENTINDEX( trace.pHit );

	if( entityIndex )
		modelIndex = (int)VARS( trace.pHit )->modelindex;
	else
		modelIndex = 0;

	g_engfuncs.pfnStaticDecal( vecOrigin, (int)pev->skin, entityIndex, modelIndex );

	SUB_Remove();
}


void CDecal :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->skin = DECAL_INDEX( pkvd->szValue );
		
		// Found
		if ( pev->skin >= 0 )
			return;
		ALERT( at_console, "Can't find decal %s\n", pkvd->szValue );
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


// Body queue class here.... It's really just CBaseEntity
class CCorpse : public CBaseEntity
{
	DECLARE_CLASS( CCorpse, CBaseEntity );
public:
	virtual int ObjectCaps( void ) { return FCAP_DONT_SAVE; }	
};

LINK_ENTITY_TO_CLASS( bodyque, CCorpse );

static void InitBodyQue(void)
{
	CBaseEntity *pBodyQue = CreateEntityByName( "bodyque" );

	g_pBodyQueueHead = pBodyQue->edict();
	entvars_t *pev = pBodyQue->pev;
	
	// Reserve 3 more slots for dead bodies
	for ( int i = 0; i < 3; i++ )
	{
		pev->owner = CreateEntityByName( "bodyque" )->edict();
		pev = VARS(pev->owner);
	}
	
	pev->owner = g_pBodyQueueHead;
}


//
// make a body que entry for the given ent so the ent can be respawned elsewhere
//
// GLOBALS ASSUMED SET:  g_eoBodyQueueHead
//
void CopyToBodyQue( CBaseEntity *pCorpse ) 
{
	if (pCorpse->pev->effects & EF_NODRAW)
		return;

	CBaseEntity *pHead = CBaseEntity::Instance( g_pBodyQueueHead );

	pHead->SetLocalAngles( pCorpse->GetAbsAngles());
	pHead->SetLocalVelocity( pCorpse->GetAbsVelocity());
	UTIL_SetOrigin( pHead, pCorpse->GetAbsOrigin());
	UTIL_SetSize( pHead->pev, pCorpse->pev->mins, pCorpse->pev->maxs );

	pHead->pev->model		= pCorpse->pev->model;
	pHead->pev->modelindex	= pCorpse->pev->modelindex;
	pHead->pev->frame		= pCorpse->pev->frame;
	pHead->pev->colormap	= pCorpse->pev->colormap;
	pHead->pev->movetype	= MOVETYPE_TOSS;
	pHead->pev->flags		= 0;
	pHead->pev->deadflag	= pCorpse->pev->deadflag;
	pHead->pev->renderfx	= kRenderFxDeadPlayer;
	pHead->pev->renderamt	= ENTINDEX( pCorpse->edict() );

	pHead->pev->effects		= pCorpse->pev->effects | EF_NOINTERP;
	pHead->pev->sequence	= pCorpse->pev->sequence;
	pHead->pev->animtime	= pCorpse->pev->animtime;
	g_pBodyQueueHead		= pHead->pev->owner;
}

CGlobalState::CGlobalState( void )
{
	Reset();
}

void CGlobalState::Reset( void )
{
	m_pList = NULL; 
	m_listCount = 0;
}

globalentity_t *CGlobalState :: Find( string_t globalname )
{
	if ( !globalname )
		return NULL;

	globalentity_t *pTest;
	const char *pEntityName = STRING(globalname);

	
	pTest = m_pList;
	while ( pTest )
	{
		if ( FStrEq( pEntityName, pTest->name ) )
			break;
	
		pTest = pTest->pNext;
	}

	return pTest;
}


// This is available all the time now on impulse 104, remove later
//#ifdef _DEBUG
void CGlobalState :: DumpGlobals( void )
{
	static char *estates[] = { "Off", "On", "Dead" };
	globalentity_t *pTest;

	ALERT( at_console, "-- Globals --\n" );
	pTest = m_pList;
	while ( pTest )
	{
		ALERT( at_console, "%s: %s (%s)\n", pTest->name, pTest->levelName, estates[pTest->state] );
		pTest = pTest->pNext;
	}
}
//#endif


void CGlobalState :: EntityAdd( string_t globalname, string_t mapName, GLOBALESTATE state, float time )
{
	ASSERT( !Find(globalname) );

	globalentity_t *pNewEntity = (globalentity_t *)calloc( sizeof( globalentity_t ), 1 );
	ASSERT( pNewEntity != NULL );
	pNewEntity->pNext = m_pList;
	m_pList = pNewEntity;
	strcpy( pNewEntity->name, STRING( globalname ) );
	strcpy( pNewEntity->levelName, STRING(mapName) );
	pNewEntity->global_time = time;
	pNewEntity->state = state;
	m_listCount++;
}


void CGlobalState :: EntitySetState( string_t globalname, GLOBALESTATE state )
{
	globalentity_t *pEnt = Find( globalname );

	if ( !pEnt ) return;

	pEnt->state = state;
}

void CGlobalState :: EntitySetTime( string_t globalname, float time )
{
	globalentity_t *pEnt = Find( globalname );

	if ( !pEnt ) return;

	pEnt->global_time = time;
}

const globalentity_t *CGlobalState :: EntityFromTable( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );

	return pEnt;
}


GLOBALESTATE CGlobalState :: EntityGetState( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );
	if ( pEnt )
		return pEnt->state;

	return GLOBAL_OFF;
}

float CGlobalState :: EntityGetTime( string_t globalname )
{
	globalentity_t *pEnt = Find( globalname );
	if ( pEnt )
		return pEnt->global_time;

	return -1.0f;
}

// Global Savedata for Delay
BEGIN_SIMPLE_DATADESC( CGlobalState )
	DEFINE_FIELD( m_listCount, FIELD_INTEGER ),
END_DATADESC()

// Global Savedata for Delay
BEGIN_SIMPLE_DATADESC( globalentity_t )
	DEFINE_AUTO_ARRAY( name, FIELD_CHARACTER ),
	DEFINE_AUTO_ARRAY( levelName, FIELD_CHARACTER ),
	DEFINE_FIELD( state, FIELD_INTEGER ),
	DEFINE_FIELD( global_time, FIELD_FLOAT ), // to save global time instead of state
END_DATADESC()

int CGlobalState::Save( CSave &save )
{
	int i;
	globalentity_t *pEntity;

	if ( !save.WriteFields( "GLOBAL", this, NULL, m_DataMap.dataDesc, m_DataMap.dataNumFields ) )
		return 0;

	DATAMAP *pMap = &globalentity_t::m_DataMap;
	
	pEntity = m_pList;
	for ( i = 0; i < m_listCount && pEntity; i++ )
	{
		if ( !save.WriteFields( "GENT", pEntity, pMap, pMap->dataDesc, pMap->dataNumFields ) )
			return 0;

		pEntity = pEntity->pNext;
	}
	
	return 1;
}

int CGlobalState::Restore( CRestore &restore )
{
	int i, listCount;
	globalentity_t tmpEntity;


	ClearStates();
	if ( !restore.ReadFields( "GLOBAL", this, NULL, m_DataMap.dataDesc, m_DataMap.dataNumFields ) )
		return 0;

	DATAMAP *pMap = &globalentity_t::m_DataMap;
	
	listCount = m_listCount;	// Get new list count
	m_listCount = 0;				// Clear loaded data

	for ( i = 0; i < listCount; i++ )
	{
		if ( !restore.ReadFields( "GENT", &tmpEntity, pMap, pMap->dataDesc, pMap->dataNumFields ) )
			return 0;
		EntityAdd( MAKE_STRING(tmpEntity.name), MAKE_STRING(tmpEntity.levelName), tmpEntity.state );
	}
	return 1;
}

void CGlobalState::EntityUpdate( string_t globalname, string_t mapname )
{
	globalentity_t *pEnt = Find( globalname );

	if ( pEnt )
		strcpy( pEnt->levelName, STRING(mapname) );
}


void CGlobalState::ClearStates( void )
{
	globalentity_t *pFree = m_pList;
	while ( pFree )
	{
		globalentity_t *pNext = pFree->pNext;
		free( pFree );
		pFree = pNext;
	}
	Reset();
}


void SaveGlobalState( SAVERESTOREDATA *pSaveData )
{
	CSave saveHelper( pSaveData );
	gGlobalState.Save( saveHelper );
}


void RestoreGlobalState( SAVERESTOREDATA *pSaveData )
{
	CRestore restoreHelper( pSaveData );
	gGlobalState.Restore( restoreHelper );
}


void ResetGlobalState( void )
{
	gGlobalState.ClearStates();
	gInitHUD = TRUE;	// Init the HUD on a new game / load game
}

// moved CWorld class definition to cbase.h
//=======================
// CWorld
//
// This spawns first when each level begins.
//=======================

LINK_ENTITY_TO_CLASS( worldspawn, CWorld );

#define SF_WORLD_DARK		0x0001		// Fade from black at startup
#define SF_WORLD_TITLE		0x0002		// Display game title at startup
#define SF_WORLD_FORCETEAM	0x0004		// Force teams

extern DLL_GLOBAL BOOL		g_fGameOver;
float g_flWeaponCheat; 

void CWorld :: Spawn( void )
{
	LIGHT_STYLE(LS_SKY, "m"); // important! without it all studiomodel don't became darker in dynamic sun mode
	g_fGameOver = FALSE;
	Precache();
	SetModel(GetModel());

	// flush collision meshes cache on serverside
	auto &meshDescFactory = CMeshDescFactory::Instance();
	meshDescFactory.ClearCache();
}

void CWorld :: Precache( void )
{
	g_pLastSpawn = NULL;
	g_pWorld = this;	

	if ( pev->gravity > 0.0f )
		CVAR_SET_FLOAT( "sv_gravity", pev->gravity );
	else
		CVAR_SET_STRING("sv_gravity", "800"); // 67ft/sec

	CVAR_SET_STRING("sv_stepsize", "18");
	CVAR_SET_STRING("room_type", "0");// clear DSP

	// Set up game rules
	if (g_pGameRules)
	{
		delete g_pGameRules;
	}

	g_pGameRules = InstallGameRules( );

	//!!!UNDONE why is there so much Spawn code in the Precache function? I'll just keep it here 

	///!!!LATER - do we want a sound ent in deathmatch? (sjb)
	//pSoundEnt = CBaseEntity::Create( "soundent", g_vecZero, g_vecZero, edict() );
	pSoundEnt = GetClassPtr( ( CSoundEnt *)NULL );
	pSoundEnt->Spawn();

	if ( !pSoundEnt )
	{
		ALERT ( at_console, "**COULD NOT CREATE SOUNDENT**\n" );
	}

	InitBodyQue();
	
// init sentence group playback stuff from sentences.txt.
// ok to call this multiple times, calls after first are ignored.

	SENTENCEG_Init();

// init texture type array from materials.txt

	TEXTURETYPE_Init();


// the area based ambient sounds MUST be the first precache_sounds

// player precaches     
	W_Precache ();									// get weapon precaches

	ClientPrecache();

	COM_PrecacheMaterialsSounds();

// sounds used from C physics code
	PRECACHE_SOUND("common/null.wav");				// clears sound channels
	PRECACHE_MODEL("sprites/null.spr");

	PRECACHE_SOUND( "items/suitchargeok1.wav" );//!!! temporary sound for respawning weapons.
	PRECACHE_SOUND( "items/gunpickup2.wav" );// player picks up a gun.

	PRECACHE_SOUND( "common/bodydrop3.wav" );// dead bodies hitting the ground (animation events)
	PRECACHE_SOUND( "common/bodydrop4.wav" );
	
	g_Language = (int)CVAR_GET_FLOAT( "sv_language" );
	if ( g_Language == LANGUAGE_GERMAN )
	{
		PRECACHE_MODEL( "models/germangibs.mdl" );
	}
	else
	{
		PRECACHE_MODEL( "models/hgibs.mdl" );
		PRECACHE_MODEL( "models/agibs.mdl" );
	}

	PRECACHE_SOUND ("weapons/ric1.wav");
	PRECACHE_SOUND ("weapons/ric2.wav");
	PRECACHE_SOUND ("weapons/ric3.wav");
	PRECACHE_SOUND ("weapons/ric4.wav");
	PRECACHE_SOUND ("weapons/ric5.wav");
//
// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
//
	int i;

	for( i = 0; i <= 13; i++ )
	{
		LIGHT_STYLE( i, (char*)STRING( GetStdLightStyle( i )));
	}
	
	// styles 32-62 are assigned by the light program for switchable lights

	// 63 testing
	LIGHT_STYLE(63, "a");

	for( i = 0; i < ARRAYSIZE(gDecals); i++ )
		gDecals[i].index = DECAL_INDEX( gDecals[i].name );

// make sure the .BIN file is newer than the .BSP file.
	if ( !WorldPhysic->CheckBINFile ( ( char * )STRING( gpGlobals->mapname ) ) )
	{// BIN file is not present, or is older than the BSP file.
		WorldPhysic->BuildCollisionTree ( ( char * )STRING( gpGlobals->mapname ) );
	}
	else
	{// Load the node graph for this level
		if ( !WorldPhysic->FLoadTree ( (char *)STRING( gpGlobals->mapname ) ) )
		{// couldn't load, so alloc and prepare to build a graph.
			ALERT ( at_console, "*Error opening .BIN file\n" );
			WorldPhysic->BuildCollisionTree ( ( char * )STRING( gpGlobals->mapname ) );
		}
		else
		{
			ALERT ( at_console, "\n*Physic Initialized!\n" );
		}
	}

// set world before activate entities
	WorldPhysic->SetupWorld();

// init the WorldGraph.
	WorldGraph.InitGraph();

// make sure the .NOD file is newer than the .BSP file.
	if ( !WorldGraph.CheckNODFile ( ( char * )STRING( gpGlobals->mapname ) ) )
	{// NOD file is not present, or is older than the BSP file.
		WorldGraph.AllocNodes ();
	}
	else
	{// Load the node graph for this level
		if ( !WorldGraph.FLoadGraph ( (char *)STRING( gpGlobals->mapname ) ) )
		{// couldn't load, so alloc and prepare to build a graph.
			ALERT ( at_console, "*Error opening .NOD file\n" );
			WorldGraph.AllocNodes ();
		}
		else
		{
			ALERT ( at_console, "\n*Graph Loaded!\n" );
		}
	}

	if ( pev->speed > 0 )
		CVAR_SET_FLOAT( "sv_zmax", pev->speed );
	else
		CVAR_SET_FLOAT( "sv_zmax", 46341.f );

	// g-cont. moved here to right restore global WaveHeight on save\restore level
	CVAR_SET_FLOAT( "sv_wateramp", pev->scale );

	if ( pev->netname )
	{
		ALERT( at_aiconsole, "Chapter title: %s\n", STRING(pev->netname) );
		CBaseEntity *pEntity = CBaseEntity::Create( "env_message", g_vecZero, g_vecZero, NULL );
		if ( pEntity )
		{
			pEntity->SetThink( &CBaseEntity::SUB_CallUseToggle );
			pEntity->pev->message = pev->netname;
			pev->netname = 0;
			pEntity->pev->nextthink = gpGlobals->time + 0.3;
			pEntity->pev->spawnflags = SF_MESSAGE_ONCE;
		}
	}

	if ( pev->spawnflags & SF_WORLD_DARK )
		CVAR_SET_FLOAT( "v_dark", 1.0 );
	else
		CVAR_SET_FLOAT( "v_dark", 0.0 );

	pev->spawnflags &= ~SF_WORLD_DARK;		// g-cont. don't apply fade after save\restore
	
	if ( pev->spawnflags & SF_WORLD_TITLE )
		gDisplayTitle = TRUE;		// display the game title if this key is set
	else
		gDisplayTitle = FALSE;

	pev->spawnflags &= ~SF_WORLD_TITLE;		// g-cont. don't show logo after save\restore
	
	if ( pev->spawnflags & SF_WORLD_FORCETEAM )
	{
		CVAR_SET_FLOAT( "mp_defaultteam", 1 );
	}
	else
	{
		CVAR_SET_FLOAT( "mp_defaultteam", 0 );
	}

	// g-cont. moved here so cheats will working on restore level
	g_flWeaponCheat = CVAR_GET_FLOAT( "sv_cheats" );  // Is the impulse 101 command allowed?
	UPDATE_PACKED_FOG( pev->impulse );
}


//
// Just to ignore the "wad" field.
//
void CWorld :: KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq(pkvd->szKeyName, "skyname") )
	{
		// Sent over net now.
		CVAR_SET_STRING( "sv_skyname", pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "sounds") )
	{
		gpGlobals->cdAudioTrack = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "WaveHeight") )
	{
		// Sent over net now.
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "MaxRange") )
	{
		pev->speed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "chaptertitle") )
	{
		pev->netname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "fog") )
	{
		int fog_settings[4];
		UTIL_StringToIntArray( fog_settings, 4, pkvd->szValue );
		pev->impulse = (fog_settings[0]<<24)|(fog_settings[1]<<16)|(fog_settings[2]<<8)|fog_settings[3];
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "startdark") )
	{
		// UNDONE: This is a gross hack!!! The CVAR is NOT sent over the client/sever link
		// but it will work for single player
		int flag = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		if ( flag )
			pev->spawnflags |= SF_WORLD_DARK;
	}
	else if ( FStrEq(pkvd->szKeyName, "newunit") )
	{
		// Single player only.  Clear save directory if set
		if ( atoi(pkvd->szValue) )
			CVAR_SET_FLOAT( "sv_newunit", 1 );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "gametitle") )
	{
		if ( atoi(pkvd->szValue) )
			pev->spawnflags |= SF_WORLD_TITLE;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "mapteams") )
	{
		pev->team = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "defaultteam") )
	{
		if ( atoi(pkvd->szValue) )
		{
			pev->spawnflags |= SF_WORLD_FORCETEAM;
		}
		pkvd->fHandled = TRUE;
	}
	else
		BaseClass::KeyValue( pkvd );
}
