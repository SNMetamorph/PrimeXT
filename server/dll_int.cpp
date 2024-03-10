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

===== dll_int.cpp ========================================================

  Entity classes exported by Halflife.

*/

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include	"game.h"
#include	"gamerules.h"
#include	"build.h"
#include	"filesystem_utils.h"

// Holds engine functionality callbacks
enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;
server_physics_api_t g_physfuncs;

#if XASH_WIN32
#define GIVEFNPTRSTODLL_CALLDECL WINAPI
#else
#define GIVEFNPTRSTODLL_CALLDECL
#endif

extern "C" 
{
	void DLLEXPORT GIVEFNPTRSTODLL_CALLDECL GiveFnptrsToDll(enginefuncs_t *pengfuncsFromEngine, globalvars_t *pGlobals)
	{
		memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
		g_iXashEngineBuildNumber = (int)CVAR_GET_FLOAT("build"); // 0 for old builds or GoldSrc
		if (g_iXashEngineBuildNumber <= 0)
			g_iXashEngineBuildNumber = (int)CVAR_GET_FLOAT("buildnum");
		gpGlobals = pGlobals;

		if (!fs::Initialize()) {
			g_engfuncs.pfnAlertMessage(at_error, "Failed to initialize filesystem interface in server library\n");
		}
	}
}

static DLL_FUNCTIONS gFunctionTable = 
{
	GameDLLInit,		// pfnGameInit
	DispatchSpawn,		// pfnSpawn
	DispatchThink,		// pfnThink
	DispatchUse,		// pfnUse
	DispatchTouch,		// pfnTouch
	DispatchBlocked,		// pfnBlocked
	DispatchKeyValue,		// pfnKeyValue
	DispatchSave,		// pfnSave
	DispatchRestore,		// pfnRestore
	DispatchObjectCollsionBox,	// pfnAbsBox

	SaveWriteFields,		// pfnSaveWriteFields
	SaveReadFields,		// pfnSaveReadFields

	SaveGlobalState,		// pfnSaveGlobalState
	RestoreGlobalState,		// pfnRestoreGlobalState
	ResetGlobalState,		// pfnResetGlobalState

	ClientConnect,		// pfnClientConnect
	ClientDisconnect,		// pfnClientDisconnect
	ClientKill,		// pfnClientKill
	ClientPutInServer,		// pfnClientPutInServer
	ClientCommand,		// pfnClientCommand
	ClientUserInfoChanged,	// pfnClientUserInfoChanged
	ServerActivate,		// pfnServerActivate
	ServerDeactivate,		// pfnServerDeactivate

	PlayerPreThink,		// pfnPlayerPreThink
	PlayerPostThink,		// pfnPlayerPostThink

	StartFrame,		// pfnStartFrame
	ParmsNewLevel,		// pfnParmsNewLevel
	ParmsChangeLevel,		// pfnParmsChangeLevel

	GetGameDescription,		// pfnGetGameDescription    Returns string describing current .dll game.
	PlayerCustomization,	// pfnPlayerCustomization   Notifies .dll of new customization for player.

	SpectatorConnect,		// pfnSpectatorConnect      Called when spectator joins server
	SpectatorDisconnect,	// pfnSpectatorDisconnect   Called when spectator leaves the server
	SpectatorThink,		// pfnSpectatorThink        Called when spectator sends a command packet (usercmd_t)

	Sys_Error,		// pfnSys_Error             Called when engine has encountered an error

	PM_Move,			// pfnPM_Move
	PM_Init,			// pfnPM_Init               Server version of player movement initialization
	nullptr,		// pfnPM_FindTextureType
	
	SetupVisibility,		// pfnSetupVisibility       Set up PVS and PAS for networking for this client
	UpdateClientData,		// pfnUpdateClientData      Set up data sent only to specific client
	AddToFullPack,		// pfnAddToFullPack
	CreateBaseline,		// pfnCreateBaseline        Tweak entity baseline for network encoding, allows setup of player baselines, too.
	RegisterEncoders,		// pfnRegisterEncoders      Callbacks for network encoding
	GetWeaponData,		// pfnGetWeaponData
	CmdStart,			// pfnCmdStart
	CmdEnd,			// pfnCmdEnd
	ConnectionlessPacket,	// pfnConnectionlessPacket
	GetHullBounds,		// pfnGetHullBounds
	CreateInstancedBaselines,	// pfnCreateInstancedBaselines
	InconsistentFile,		// pfnInconsistentFile
	AllowLagCompensation,	// pfnAllowLagCompensation
};

NEW_DLL_FUNCTIONS gNewDLLFunctions =
{
	OnFreeEntPrivateData,	// pfnOnFreeEntPrivateData
	GameDLLShutdown,		// pfnGameShutdown
	ShouldCollide,		// pfnShouldCollide
};

#if XASH_WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	return TRUE;
}
#endif

int GetEntityAPI(DLL_FUNCTIONS *pFunctionTable, int interfaceVersion)
{
	if (!pFunctionTable || interfaceVersion != INTERFACE_VERSION)
	{
		return FALSE;
	}

	if (!CVAR_GET_POINTER("host_gameloaded"))
		return FALSE; // not a Xash3D

	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return TRUE;
}

int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable || *interfaceVersion != INTERFACE_VERSION)
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	if (!CVAR_GET_POINTER("host_gameloaded"))
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE; // not a Xash3D
	}

	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return TRUE;
}

int GetNewDLLFunctions( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
{
	if( !pFunctionTable || *interfaceVersion != NEW_DLL_FUNCTIONS_VERSION )
	{
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	if (!CVAR_GET_POINTER("host_gameloaded"))
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE; // not a Xash3D
	}

	memcpy( pFunctionTable, &gNewDLLFunctions, sizeof( gNewDLLFunctions ));
	return TRUE;
}

//=======================================================================
//			ENGINE <-> GAME
//=======================================================================
int DispatchSpawn( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if( pEntity )
	{
//		ALERT( at_aiconsole, "DispatchSpawn: %s\n", pEntity->GetClassname());
		// Initialize these or entities who don't link to the world won't have anything in here
		pEntity->pev->absmin = pEntity->GetAbsOrigin() - Vector( 1.0f, 1.0f, 1.0f );
		pEntity->pev->absmax = pEntity->GetAbsOrigin() + Vector( 1.0f, 1.0f, 1.0f );

		pEntity->Spawn();

		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.
		pEntity = (CBaseEntity *)GET_PRIVATE( pent );

		if( pEntity )
		{
			if ( g_pGameRules && !g_pGameRules->IsAllowedToSpawn( pEntity ))
				return -1; // return that this entity should be deleted
			if ( pEntity->pev->flags & FL_KILLME )
				return -1;
		}

		// Handle global stuff here
		if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
//				ALERT( at_console, "Added global entity %s (%s)\n", STRING(pEntity->pev->classname), STRING(pEntity->pev->globalname) );
			}
		}

	}

	return 0;
}

void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
	if ( !pkvd || !pentKeyvalue )
		return;

	EntvarsKeyvalue( VARS(pentKeyvalue), pkvd );

	// If the key was an entity variable, or there's no class set yet, don't look for the object, it may
	// not exist yet.
	if ( pkvd->fHandled || pkvd->szClassName == NULL )
		return;

	// Get the actualy entity object
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentKeyvalue);

	if ( !pEntity )
		return;

	pEntity->KeyValue( pkvd );
}

// HACKHACK -- this is a hack to keep the node graph entity from "touching" things (like triggers)
// while it builds the graph
BOOL gTouchDisabled = FALSE;
void DispatchTouch( edict_t *pentTouched, edict_t *pentOther )
{
	if ( gTouchDisabled )
		return;

	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentTouched);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if ( pEntity && pOther && ! ((pEntity->pev->flags | pOther->pev->flags) & FL_KILLME) )
		pEntity->Touch( pOther );
}

void DispatchUse( edict_t *pentUsed, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pentUsed);
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE(pentOther);

	if (pEntity && !(pEntity->pev->flags & FL_KILLME) )
		pEntity->Use( pOther, pOther, USE_TOGGLE, 0 );
}

void DispatchThink( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if (pEntity)
	{
		if( FBitSet( pEntity->pev->flags, FL_DORMANT ))
			ALERT( at_error, "Dormant entity %s is thinking!!\n", pEntity->GetClassname() );
				
		pEntity->Think();
	}
}

void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE( pentBlocked );
	CBaseEntity *pOther = (CBaseEntity *)GET_PRIVATE( pentOther );

	if (pEntity)
		pEntity->Blocked( pOther );
}

void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);
	
	if ( pEntity && pSaveData )
	{
		ENTITYTABLE *pTable = &pSaveData->pTable[ pSaveData->currentIndex ];

		if ( pTable->pent != pent )
			ALERT( at_error, "ENTITY TABLE OR INDEX IS WRONG!!!!\n" );

		if ( pEntity->ObjectCaps() & FCAP_DONT_SAVE )
			return;

		pTable->location = pSaveData->size;		// Remember entity position for file I/O
		pTable->classname = pEntity->pev->classname;	// Remember entity class for respawn

		CSave saveHelper( pSaveData );
		pEntity->Save( saveHelper );

		pTable->size = pSaveData->size - pTable->location;	// Size of entity block is data size written to block
	}
}

// Find the matching global entity.  Spit out an error if the designer made entities of
// different classes with the same global name
CBaseEntity *FindGlobalEntity( string_t classname, string_t globalname )
{
	edict_t *pent = FIND_ENTITY_BY_STRING( NULL, "globalname", STRING(globalname) );
	CBaseEntity *pReturn = CBaseEntity::Instance( pent );
	if ( pReturn )
	{
		if ( !FClassnameIs( pReturn->pev, STRING(classname) ) )
		{
			ALERT( at_console, "Global entity found %s, wrong class %s\n", STRING(globalname), STRING(pReturn->pev->classname) );
			pReturn = NULL;
		}
	}

	return pReturn;
}

int DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	gpGlobals->vecLandmarkOffset = g_vecZero;

	if ( pEntity && pSaveData )
	{
		entvars_t tmpVars;
		ENTITYTABLE *pTable = &pSaveData->pTable[pSaveData->currentIndex];

		CRestore restoreHelper( pSaveData );
		if ( globalEntity )
		{
			CRestore tmpRestore( pSaveData );
			tmpRestore.PrecacheMode( 0 );
			tmpRestore.ReadEntVars( "ENTVARS", pEntity->GetDataDescMap(), &tmpVars );

			// HACKHACK - reset the save pointers, we're going to restore for real this time
			pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
			pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;
			// -------------------

			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( tmpVars.globalname );
			
			// Don't overlay any instance of the global that isn't the latest
			// pSaveData->szCurrentMapName is the level this entity is coming from
			// pGlobla->levelName is the last level the global entity was active in.
			// If they aren't the same, then this global update is out of date.
			if ( !FStrEq( pSaveData->szCurrentMapName, pGlobal->levelName ) )
				return 0;

			// Compute the new global offset
			gpGlobals->vecLandmarkOffset = pSaveData->vecLandmarkOffset;
			CBaseEntity *pNewEntity = FindGlobalEntity( tmpVars.classname, tmpVars.globalname );
			if ( pNewEntity )
			{
//				ALERT( at_console, "Overlay %s with %s\n", STRING(pNewEntity->pev->classname), STRING(tmpVars.classname) );
				// Tell the restore code we're overlaying a global entity from another level
				restoreHelper.SetGlobalMode( 1 );	// Don't overwrite global fields

				if( UTIL_GetModelType( pNewEntity->pev->modelindex ) == mod_brush )
				{
					// calculate model offsets to update the childrens
					restoreHelper.modelSpaceOffset = tmpVars.mins - pNewEntity->pev->mins;
					restoreHelper.modelOriginOffset = tmpVars.oldorigin - pNewEntity->pev->oldorigin;
				}

				pSaveData->vecLandmarkOffset = (pSaveData->vecLandmarkOffset - pNewEntity->pev->mins) + tmpVars.mins;
				pEntity = pNewEntity;// we're going to restore this data OVER the old entity
				pent = ENT( pEntity->pev );
				pTable->pent = pent;

				// Update the global table to say that the global definition of this entity should come from this level
				gGlobalState.EntityUpdate( pEntity->pev->globalname, gpGlobals->mapname );
			}
			else
			{
				// This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
				// or call EntityUpdate() to move it to this level, we haven't changed global state at all.
				return 0;
			}
		}

		if ( pEntity->ObjectCaps() & FCAP_MUST_SPAWN )
		{
			pEntity->Restore( restoreHelper );
			pEntity->Spawn();
		}
		else
		{
			pEntity->Restore( restoreHelper );
			pEntity->Precache( );
		}

		restoreHelper.modelOriginOffset = g_vecZero;
		restoreHelper.modelSpaceOffset = g_vecZero;

		// Again, could be deleted, get the pointer again.
		pEntity = (CBaseEntity *)GET_PRIVATE(pent);

		// Is this an overriding global entity (coming over the transition), or one restoring in a level
		if ( globalEntity )
		{
			pSaveData->vecLandmarkOffset = gpGlobals->vecLandmarkOffset;

			if ( pEntity )
			{
				pEntity->RelinkEntity( TRUE );
				pEntity->OverrideReset();
			}
		}
		else if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING(gpGlobals->mapname), pGlobal->levelName ) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				ALERT( at_error, "Global Entity %s (%s) not in table!!!\n", STRING(pEntity->pev->globalname), STRING(pEntity->pev->classname) );
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobals->mapname, GLOBAL_ON );
			}
		}
		else if( gpGlobals->changelevel )
		{
			if( pTable->flags & ( FENTTABLE_MOVEABLE|FENTTABLE_PLAYER ))
			{
				// do reset for moveable entities
				pEntity->TransferReset();
			}
		}
	}
	return 0;
}

void DispatchCreateEntitiesInRestoreList( SAVERESTOREDATA *pSaveData, int levelMask, qboolean create_world )
{
	ENTITYTABLE *pTable;
	CBaseEntity *pEntity;
	edict_t *pent;

	// create entity list
	for( int i = 0; i < pSaveData->tableCount; i++ )
	{
		pTable = &pSaveData->pTable[i];
		pEntity = NULL;
		pent = NULL;

		if( pTable->classname != NULL_STRING && pTable->size && ( !FBitSet( pTable->flags, FENTTABLE_REMOVED ) || !create_world ))
		{
			int	active = FBitSet( pTable->flags, levelMask ) ? 1 : 0;

			if( create_world )
				active = 1;

			if( pTable->id == 0 && create_world ) // worldspawn
			{
				ASSERT( i == 0 );

				edict_t *ed = INDEXENT( pTable->id );

				memset( &ed->v, 0, sizeof( entvars_t ));
				ed->v.pContainingEntity = ed;
				ed->free = false;
				pEntity = GetClassPtr( (CWorld *)VARS(ed));
			}
			else if(( pTable->id > 0 ) && ( pTable->id <= gpGlobals->maxClients ))
			{
				if( !FBitSet( pTable->flags, FENTTABLE_PLAYER ))
					ALERT( at_warning, "ENTITY IS NOT A PLAYER: %d\n", i );

				edict_t *ed = INDEXENT( pTable->id );

				// create the player
				if( active && ed != NULL )
					pEntity = CreateEntityByName( STRING( pTable->classname ), VARS( ed ));
			}
			else if( active )
			{
				pEntity = CreateEntityByName( STRING( pTable->classname ));
			}

			pent = (pEntity) ? ENT(pEntity) : NULL;
		}

		pTable->pent = pent;
	}
}

void DispatchObjectCollsionBox( edict_t *pent )
{
	CBaseEntity *pEntity = (CBaseEntity *)GET_PRIVATE(pent);

	if( pEntity )
	{
		pEntity->CalcAbsolutePosition();
		pEntity->SetObjectCollisionBox();
	}
}

void OnFreeEntPrivateData( edict_s *pEdict )
{
	if (pEdict && pEdict->pvPrivateData)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(pEdict);
		pEntity->UpdateOnRemove();
		pEntity->~CBaseEntity();
	}
}

void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, ENGTYPEDESCRIPTION *pInFields, int fieldCount )
{
	TYPEDESCRIPTION	pRemapFields[64];
	TYPEDESCRIPTION	*pFields, *pField;

	pFields = pField = pRemapFields;

	ASSERT( fieldCount <= 64 );

	// convert short engine decsription into long game description
	for( int i = 0; i < fieldCount; i++, pField++ )
	{
		pField->fieldType = pInFields[i].fieldType;
		pField->fieldName = pInFields[i].fieldName;
		pField->fieldOffset = pInFields[i].fieldOffset;
		pField->fieldSize = pInFields[i].fieldSize;
		pField->flags = pInFields[i].flags;
	}

	CSave saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, NULL, pFields, fieldCount );
}

void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, ENGTYPEDESCRIPTION *pInFields, int fieldCount )
{
	TYPEDESCRIPTION	pRemapFields[64];
	TYPEDESCRIPTION	*pFields, *pField;

	pFields = pField = pRemapFields;

	ASSERT( fieldCount <= 64 );

	// convert short engine decsription into long game description
	for( int i = 0; i < fieldCount; i++, pField++ )
	{
		pField->fieldType = pInFields[i].fieldType;
		pField->fieldName = pInFields[i].fieldName;
		pField->fieldOffset = pInFields[i].fieldOffset;
		pField->fieldSize = pInFields[i].fieldSize;
		pField->flags = pInFields[i].flags;
	}

	CRestore restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, NULL, pFields, fieldCount );
}
