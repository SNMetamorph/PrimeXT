//=======================================================================
//			Copyright (C) XashXT Group 2011
//		         	mapents.cpp - spawn map entities 
//=======================================================================

#include	"extdll.h"
#include  "util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include	"utlarray.h"

struct CSpawnEntry
{
	CBaseEntity	*m_pEntity;
	int		m_nDepth;		// parent depth
};

// creates an entity by string name, but does not spawn it
CBaseEntity *CreateEntityByName( const char *className, entvars_t *pev )
{
	return EntityFactoryDictionary()->Create( className, pev );
}

static int ED_CompareByHierarchyDepth( CSpawnEntry *pEnt1, CSpawnEntry *pEnt2 )
{
	if( pEnt1->m_nDepth == pEnt2->m_nDepth )
		return 0;

	if( pEnt1->m_nDepth > pEnt2->m_nDepth )
		return 1;

	return -1;
}

static int ED_ComputeSpawnHierarchyDepth_r( CBaseEntity *pEntity )
{
	if( !pEntity )
		return 1;

	if( pEntity->m_iParent == NULL_STRING )
		return 1;

	CBaseEntity *pParent = UTIL_FindEntityByTargetname( NULL, STRING( pEntity->m_iParent ));

	if( !pParent )
		return 1;

	if( pParent == pEntity )
	{
		ALERT( at_warning, "LEVEL DESIGN ERROR: Entity %s is parented to itself!\n", pEntity->GetTargetname());
		return 1;
	}

	return 1 + ED_ComputeSpawnHierarchyDepth_r( pParent );
}

static void ED_ComputeSpawnHierarchyDepth( CSpawnEntry *pSpawnList, int nEntities )
{
	// NOTE: This isn't particularly efficient, but so what? It's at the beginning of time
	// I did it this way because it simplified the parent setting in hierarchy (basically
	// eliminated questions about whether you should transform origin from global to local or not)
	for( int nEntity = 0; nEntity < nEntities; nEntity++ )
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

		if( pEntity && !pEntity->IsDormant( ))
			pSpawnList[nEntity].m_nDepth = ED_ComputeSpawnHierarchyDepth_r( pEntity );
		else
			pSpawnList[nEntity].m_nDepth = 1;
	}
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
CBaseEntity *ED_ParseEdict( char **pfile, edict_t *ent )
{
	KeyValueData	pkvd[256]; // per one entity
	int		i, numpairs = 0;
	const char	*classname = NULL;
	char		token[2048];

	// go through all the dictionary pairs
	while( 1 )
	{	
		char	keyname[256];

		// parse key
		if(( *pfile = COM_ParseFile( *pfile, token )) == NULL )
			HOST_ERROR( "ED_ParseEdict: EOF without closing brace\n" );

		if( token[0] == '}' ) break; // end of desc

		Q_strncpy( keyname, token, sizeof( keyname ));

		// parse value	
		if(( *pfile = COM_ParseFile( *pfile, token )) == NULL ) 
			HOST_ERROR( "ED_ParseEdict: EOF without closing brace\n" );

		if( token[0] == '}' )
			HOST_ERROR( "ED_ParseEdict: closing brace without data\n" );

		// ignore attempts to set key ""
		if( !keyname[0] ) continue;

		// "wad" field is completely ignored in XashXT
		if( !Q_strcmp( keyname, "wad" ))
			continue;

		// ignore attempts to set value ""
		if( !token[0] ) continue;

		// create keyvalue strings
		pkvd[numpairs].szClassName = (char *)classname;	// unknown at this moment
		pkvd[numpairs].szKeyName = copystring( keyname );
		pkvd[numpairs].szValue = copystring( token );
		pkvd[numpairs].fHandled = false;		

		if( !Q_strcmp( keyname, "classname" ) && classname == NULL )
			classname = pkvd[numpairs].szValue;
		if( ++numpairs >= 256 ) break;
	}

	CBaseEntity *pEntity;
	string_t iszClassName = ALLOC_STRING( classname ); // need to have a valid copy of this string

	if( ent )
	{
		// initialize world
		pEntity = CreateEntityByName( STRING( iszClassName ), VARS( ent ));

		// make sure what world is really initailized
		if( !pEntity ) HOST_ERROR( "Can't initialize world!\n" );
		pEntity->m_iParent = NULL_STRING; // don't allow a parent on the first entity (worldspawn)
	}
	else
	{
		// any other entity
		pEntity = CreateEntityByName( STRING( iszClassName ));
	}

	if( !pEntity || ENT( pEntity )->free || FBitSet( pEntity->pev->flags, FL_KILLME ))
	{
		// release allocated strings
		for( i = 0; i < numpairs; i++ )
		{
			freestring( pkvd[i].szKeyName );
			freestring( pkvd[i].szValue );
		}
		return NULL;
	}

	for( i = 0; i < numpairs; i++ )
	{
		if( !Q_strcmp( pkvd[i].szKeyName, "angle" ))
		{
			float	flYawAngle = Q_atof( pkvd[i].szValue );

			freestring( pkvd[i].szKeyName ); // will be replace with 'angles'
			freestring( pkvd[i].szValue ); // release old value, so we don't need these
			pkvd[i].szKeyName = copystring( "angles" );

			if( flYawAngle >= 0.0f )
				pkvd[i].szValue = copystring( va( "%g %g %g", pEntity->pev->angles.x, flYawAngle, pEntity->pev->angles.z ));
			else if( flYawAngle == -1.0f )
				pkvd[i].szValue = copystring( "-90 0 0" );
			else if( flYawAngle == -2.0f )
				pkvd[i].szValue = copystring( "90 0 0" );
			else pkvd[i].szValue = copystring( "0 0 0" ); // technically an error
		}

		if( !Q_strcmp( pkvd[i].szKeyName, "light" ))
		{
			freestring( pkvd[i].szKeyName );
			pkvd[i].szKeyName = copystring( "light_level" );
		}

		if( !pkvd[i].fHandled )
		{
			pkvd[i].szClassName = (char *)classname;
			DispatchKeyValue( pEntity->edict(), &pkvd[i] );
		}

		// no reason to keep this data
		freestring( pkvd[i].szKeyName );
		freestring( pkvd[i].szValue );
	}

	return pEntity;
}

int ED_SpawnEdict( edict_t *ent )
{
	if( DispatchSpawn( ent ) < 0 )
	{
		// game rejected the spawn and not marked for delete
		if( !FBitSet( ent->v.flags, FL_KILLME ))
		{
			REMOVE_ENTITY( ent );
			return 0;
		}
	}
	return 1;
}

int DispatchSpawnEntities( const char *mapname, char *entities )
{
	if( !entities ) return 0; // probably this never happens

	char token[2048];
	int inhibited = 0;
	CSpawnEntry pSpawnList[4096];	// max XashXT edicts
	int nEntity, nEntities = 0;
	BOOL create_world = TRUE;

	// parse ents
	while(( entities = COM_ParseFile( entities, token )) != NULL )
	{
		if( token[0] != '{' )
			HOST_ERROR( "ED_LoadFromFile: found %s when expecting {\n", token );

		edict_t *ent = NULL;

		if( create_world )
			ent = INDEXENT( 0 ); // already initialized by engine

		CBaseEntity *pEntity = ED_ParseEdict( &entities, ent );
		if( !pEntity ) continue;

		if( pEntity->ObjectCaps() & FCAP_IGNORE_PARENT )
			pEntity->m_iParent = NULL_STRING; // clear parent for this entity

		if( create_world )
		{
			if( DispatchSpawn( ent ) < 0 )
				HOST_ERROR( "can't spawn the world\n" );
			create_world = false;
		}
		else if( pEntity->m_iParent == NULL_STRING && pEntity->pev->targetname == NULL_STRING )
		{
			// it's doesn't have parent and can't be parent himself
			// so we can spawn this immediately
			if( !ED_SpawnEdict( pEntity->edict( )))
				inhibited++;
		}
		else
		{
			// queue up this entity for spawning
			pSpawnList[nEntities].m_pEntity = pEntity;
			pSpawnList[nEntities].m_nDepth = 0;
			nEntities++;
		}
	}

	ED_ComputeSpawnHierarchyDepth( pSpawnList, nEntities );

	// Sort the entities (other than the world) by hierarchy depth, in order to spawn them in
	// that order. This insures that each entity's parent spawns before it does so that
	// it can properly set up anything that relies on hierarchy.
	qsort( &pSpawnList[0], nEntities, sizeof( pSpawnList[0] ), (int (__cdecl *)(const void *, const void *))ED_CompareByHierarchyDepth );

	// Set up entity movement hierarchy in reverse hierarchy depth order. This allows each entity
	// to use its parent's world spawn origin to calculate its local origin.
	for( nEntity = nEntities - 1; nEntity >= 0; nEntity--)
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

		if( pEntity && !pEntity->HasAttachment( ))
		{
			CBaseEntity *pParent = UTIL_FindEntityByTargetname( NULL, STRING( pEntity->m_iParent ));

			if( pParent && pParent->edict( ))
			{
				ALERT( at_aiconsole, "%s linked with %s\n", pEntity->GetClassname(), pParent->GetClassname());
				pEntity->SetParent( pParent ); 
			}
		}
	}

	// Spawn all the entities in hierarchy depth order so that parents spawn before their children.
	for( nEntity = 0; nEntity < nEntities; nEntity++ )
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

		if( pEntity )
		{
			if( !ED_SpawnEdict( pEntity->edict( )))
				inhibited++;
		}
	}

	// Set up last remaining entities that linked with model attachments
	for( nEntity = nEntities - 1; nEntity >= 0; nEntity--)
	{
		CBaseEntity *pEntity = pSpawnList[nEntity].m_pEntity;

		// make sure this entity is really want attachement
		if( pEntity && pEntity->HasAttachment( ))
		{
			ALERT( at_aiconsole, "%s attached with %s\n", pEntity->GetClassname(), STRING( pEntity->m_iParent ));
			pEntity->SetParent( pEntity->m_iParent, 0 ); 
		}
	}


	ALERT( at_console, "\n%i entities inhibited\n", inhibited );

	return 1;	// we done
}
