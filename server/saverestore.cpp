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

===== saverestore.cpp ========================================================

  Save\restore implementation.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"

#define ENTVARS_COUNT	ARRAYSIZE( gEntvarsDescription )

TYPEDESCRIPTION gEntvarsDescription[] = 
{
	DEFINE_ENTITY_FIELD( classname, FIELD_STRING ),
	DEFINE_ENTITY_GLOBAL_FIELD( globalname, FIELD_STRING ),
	
	DEFINE_ENTITY_FIELD( origin, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( oldorigin, FIELD_VECTOR ),	// same as origin but not subtracted from landmark pos !!!
	DEFINE_ENTITY_FIELD( velocity, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( basevelocity, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( movedir, FIELD_VECTOR ),

	DEFINE_ENTITY_FIELD( angles, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( avelocity, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( punchangle, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( v_angle, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( endpos, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( startpos, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( fixangle, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( idealpitch, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( pitch_speed, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( ideal_yaw, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( yaw_speed, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( modelindex, FIELD_INTEGER ),
	DEFINE_ENTITY_GLOBAL_FIELD( model, FIELD_MODELNAME ),

	DEFINE_ENTITY_FIELD( viewmodel, FIELD_MODELNAME ),
	DEFINE_ENTITY_FIELD( weaponmodel, FIELD_MODELNAME ),

	DEFINE_ENTITY_FIELD( absmin, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_FIELD( absmax, FIELD_POSITION_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( mins, FIELD_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( maxs, FIELD_VECTOR ),
	DEFINE_ENTITY_GLOBAL_FIELD( size, FIELD_VECTOR ),

	DEFINE_ENTITY_FIELD( ltime, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( nextthink, FIELD_TIME ),

	DEFINE_ENTITY_FIELD( solid, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( movetype, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( skin, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( body, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( effects, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( gravity, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( friction, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( light_level, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( frame, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( scale, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( sequence, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( animtime, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( framerate, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( controller, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( blending, FIELD_SHORT ),

	DEFINE_ENTITY_FIELD( rendermode, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( renderamt, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( rendercolor, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( renderfx, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( health, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( frags, FIELD_FLOAT ),
//	DEFINE_ENTITY_FIELD( weapons, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( takedamage, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( deadflag, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( view_ofs, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( button, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( impulse, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( chain, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( dmg_inflictor, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( enemy, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( aiment, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( owner, FIELD_EDICT ),
	DEFINE_ENTITY_FIELD( groundentity, FIELD_EDICT ),

	DEFINE_ENTITY_FIELD( spawnflags, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( flags, FIELD_FLOAT ),

	DEFINE_ENTITY_FIELD( colormap, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( team, FIELD_INTEGER ),

	DEFINE_ENTITY_FIELD( max_health, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( teleport_time, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( armortype, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( armorvalue, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( waterlevel, FIELD_INTEGER ),
	DEFINE_ENTITY_FIELD( watertype, FIELD_INTEGER ),

	// Having these fields be local to the individual levels makes it easier to test those levels individually.
	DEFINE_ENTITY_GLOBAL_FIELD( target, FIELD_STRING ),
	DEFINE_ENTITY_GLOBAL_FIELD( targetname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( netname, FIELD_STRING ),
	DEFINE_ENTITY_FIELD( message, FIELD_STRING ),

	DEFINE_ENTITY_FIELD( dmg_take, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( dmg_save, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( dmg, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( dmgtime, FIELD_TIME ),

	DEFINE_ENTITY_FIELD( noise, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise1, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise2, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( noise3, FIELD_SOUNDNAME ),
	DEFINE_ENTITY_FIELD( speed, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( air_finished, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( pain_finished, FIELD_TIME ),
	DEFINE_ENTITY_FIELD( radsuit_finished, FIELD_TIME ),

	// pose params
	DEFINE_ENTITY_FIELD( vuser1, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( vuser2, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( vuser3, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( vuser4, FIELD_VECTOR ),
	DEFINE_ENTITY_FIELD( iuser1, FIELD_INTEGER ),		// custom shared flags
	DEFINE_ENTITY_FIELD( iuser2, FIELD_INTEGER ),		// custom variable
	DEFINE_ENTITY_FIELD( iuser3, FIELD_INTEGER ),		// vertex light cachenum
	DEFINE_ENTITY_FIELD( iuser4, FIELD_INTEGER ),		// attachment number
	DEFINE_ENTITY_FIELD( fuser1, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( fuser2, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( fuser3, FIELD_FLOAT ),
	DEFINE_ENTITY_FIELD( fuser4, FIELD_FLOAT ),
};

// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
static int gSizes[FIELD_TYPECOUNT] = 
{
	FIELD_SIZE( FIELD_FLOAT ),
	FIELD_SIZE( FIELD_STRING ),
	FIELD_SIZE( FIELD_ENTITY ),
	FIELD_SIZE( FIELD_CLASSPTR ),
	FIELD_SIZE( FIELD_EHANDLE ),
	FIELD_SIZE( FIELD_EVARS ),
	FIELD_SIZE( FIELD_EDICT ),
	FIELD_SIZE( FIELD_VECTOR ),
	FIELD_SIZE( FIELD_POSITION_VECTOR ),
	FIELD_SIZE( FIELD_POINTER ),
	FIELD_SIZE( FIELD_INTEGER ),
	FIELD_SIZE( FIELD_FUNCTION ),
	FIELD_SIZE( FIELD_BOOLEAN ),
	FIELD_SIZE( FIELD_SHORT ),
	FIELD_SIZE( FIELD_CHARACTER ),
	FIELD_SIZE( FIELD_TIME ),
	FIELD_SIZE( FIELD_MODELNAME ),
	FIELD_SIZE( FIELD_SOUNDNAME ),
	FIELD_SIZE( FIELD_VOID ),
};

// Base class includes common SAVERESTOREDATA pointer, and manages the entity table
CSaveRestoreBuffer :: CSaveRestoreBuffer( void )
{
	m_pdata = NULL;
}

CSaveRestoreBuffer :: CSaveRestoreBuffer( SAVERESTOREDATA *pdata )
{
	m_pdata = pdata;
}

CSaveRestoreBuffer :: ~CSaveRestoreBuffer( void )
{
}

int CSaveRestoreBuffer :: EntityIndex( CBaseEntity *pEntity )
{
	if ( pEntity == NULL )
		return -1;
	return EntityIndex( pEntity->pev );
}

int CSaveRestoreBuffer :: EntityIndex( entvars_t *pevLookup )
{
	if ( pevLookup == NULL )
		return -1;
	return EntityIndex( ENT( pevLookup ) );
}

int CSaveRestoreBuffer :: EntityIndex( EOFFSET eoLookup )
{
	return EntityIndex( ENT( eoLookup ) );
}

int CSaveRestoreBuffer :: EntityIndex( edict_t *pentLookup )
{
	if ( !m_pdata || pentLookup == NULL )
		return -1;

	int i;
	ENTITYTABLE *pTable;

	for ( i = 0; i < m_pdata->tableCount; i++ )
	{
		pTable = m_pdata->pTable + i;
		if ( pTable->pent == pentLookup )
			return i;
	}
	return -1;
}

edict_t *CSaveRestoreBuffer :: EntityFromIndex( int entityIndex )
{
	if ( !m_pdata || entityIndex < 0 )
		return NULL;

	int i;
	ENTITYTABLE *pTable;

	for ( i = 0; i < m_pdata->tableCount; i++ )
	{
		pTable = m_pdata->pTable + i;
		if ( pTable->id == entityIndex )
			return pTable->pent;
	}
	return NULL;
}

int CSaveRestoreBuffer :: EntityFlagsSet( int entityIndex, int flags )
{
	if ( !m_pdata || entityIndex < 0 )
		return 0;
	if ( entityIndex > m_pdata->tableCount )
		return 0;

	m_pdata->pTable[ entityIndex ].flags |= flags;

	return m_pdata->pTable[ entityIndex ].flags;
}

void CSaveRestoreBuffer :: BufferRewind( int size )
{
	if ( !m_pdata )
		return;

	if ( m_pdata->size < size )
		size = m_pdata->size;

	m_pdata->pCurrentData -= size;
	m_pdata->size -= size;
}

#ifndef _WIN32
extern "C" {
unsigned _rotr ( unsigned val, int shift)
{
        register unsigned lobit;        /* non-zero means lo bit set */
        register unsigned num = val;    /* number to rotate */

        shift &= 0x1f;                  /* modulo 32 -- this will also make
                                           negative shifts work */

        while (shift--) {
                lobit = num & 1;        /* get high bit */
                num >>= 1;              /* shift right one bit */
                if (lobit)
                        num |= 0x80000000;  /* set hi bit if lo bit was set */
        }

        return num;
}
}
#endif

unsigned int CSaveRestoreBuffer :: HashString( const char *pszToken )
{
	unsigned int	hash = 0;

	while ( *pszToken )
		hash = _rotr( hash, 4 ) ^ *pszToken++;

	return hash;
}

unsigned short CSaveRestoreBuffer :: TokenHash( const char *pszToken )
{
	unsigned short	hash = (unsigned short)(HashString( pszToken ) % (unsigned)m_pdata->tokenCount );
	
#if _DEBUG
	static int tokensparsed = 0;
	tokensparsed++;
	if ( !m_pdata->tokenCount || !m_pdata->pTokens )
		ALERT( at_error, "No token table array in TokenHash()!" );
#endif

	for ( int i=0; i<m_pdata->tokenCount; i++ )
	{
#if _DEBUG
		static qboolean beentheredonethat = FALSE;
		if ( i > 50 && !beentheredonethat )
		{
			beentheredonethat = TRUE;
			ALERT( at_error, "CSaveRestoreBuffer :: TokenHash() is getting too full!" );
		}
#endif

		int	index = hash + i;
		if ( index >= m_pdata->tokenCount )
			index -= m_pdata->tokenCount;

		if ( !m_pdata->pTokens[index] || strcmp( pszToken, m_pdata->pTokens[index] ) == 0 )
		{
			m_pdata->pTokens[index] = (char *)pszToken;
			return index;
		}
	}
		
	// Token hash table full!!! 
	// [Consider doing overflow table(s) after the main table & limiting linear hash table search]
	ALERT( at_error, "CSaveRestoreBuffer :: TokenHash() is COMPLETELY FULL!" );
	return 0;
}

void CSave :: Log( DATAMAP *pMap, const char *pName, const char *pFieldName, FIELDTYPE fieldType, void *value, int count )
{
	// Check to see if we are logging.
	static char szBuf[1024];
	static char szTempBuf[256];

	// Save the name.
	Q_snprintf( szBuf, sizeof( szBuf ), "%s->%s ", pName, pFieldName );

	for ( int iCount = 0; iCount < count; iCount++ )
	{
		switch ( fieldType )
		{
		case FIELD_SHORT:
			{
				short *pValue = ( short* )( value );
				short nValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%d", nValue );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		case FIELD_FLOAT:
		case FIELD_TIME:
			{
				float *pValue = ( float* )( value );
				float flValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%f", flValue );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		case FIELD_BOOLEAN:
			{
				bool *pValue = ( bool* )( value );
				bool bValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%d", ( int )( bValue ) );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		case FIELD_INTEGER:
			{
				int *pValue = ( int* )( value );
				int nValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%d", nValue );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		case FIELD_STRING:
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
			{
				string_t *pValue = ( string_t* )( value );
				string_t sValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%s", ( char* )STRING( sValue ) );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;					
			}
		case FIELD_VECTOR:
		case FIELD_POSITION_VECTOR:
			{
				Vector *pValue = ( Vector* )( value );
				Vector vecValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "(%f %f %f)", vecValue.x, vecValue.y, vecValue.z );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		case FIELD_CHARACTER:
			{
				if( count != 1 ) break;
				char *pValue = ( char* )( value );
				char chValue = pValue[iCount];
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%d", chValue );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		case FIELD_FUNCTION:
			{
				void **pValue = (void **)value;
				const char *funcName = UTIL_FunctionToName( pMap, *pValue );
				Q_snprintf( szTempBuf, sizeof( szTempBuf ), "%s", funcName );
				Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
				break;
			}
		default:
			break;
		}

		// Add space data.
		if ( ( iCount + 1 ) != count )
		{
			Q_strncpy( szTempBuf, " ", sizeof( szTempBuf ) );
			Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
		}
		else
		{
			Q_strncpy( szTempBuf, "\n", sizeof( szTempBuf ) );
			Q_strncat( szBuf, szTempBuf, sizeof( szTempBuf ));
		}
	}

	ALERT( at_console, szBuf );
}

void CSave :: WriteData( const char *pname, int size, const char *pdata )
{
	BufferField( pname, size, pdata );
}

void CSave :: WriteShort( const char *pname, const short *data, int count )
{
	BufferField( pname, sizeof(short) * count, (const char *)data );
}

void CSave :: WriteInt( const char *pname, const int *data, int count )
{
	BufferField( pname, sizeof(int) * count, (const char *)data );
}

void CSave :: WriteFloat( const char *pname, const float *data, int count )
{
	BufferField( pname, sizeof(float) * count, (const char *)data );
}

void CSave :: WriteTime( const char *pname, const float *data, int count )
{
	int i;
	Vector tmp, input;

	BufferHeader( pname, sizeof(float) * count );
	for ( i = 0; i < count; i++ )
	{
		float tmp = data[0];

		// Always encode time as a delta from the current time so it can be re-based if loaded in a new level
		// Times of 0 are never written to the file, so they will be restored as 0, not a relative time
		if ( m_pdata )
			tmp -= m_pdata->time;

		BufferData( (const char *)&tmp, sizeof(float) );
		data ++;
	}
}

void CSave :: WriteString( const char *pname, const char *pdata )
{
#ifdef TOKENIZE
	short	token = (short)TokenHash( pdata );
	WriteShort( pname, &token, 1 );
#else
	BufferField( pname, strlen(pdata) + 1, pdata );
#endif
}

void CSave :: WriteString( const char *pname, const int *stringId, int count )
{
	int i, size;

#ifdef TOKENIZE
	short	token = (short)TokenHash( STRING( *stringId ) );
	WriteShort( pname, &token, 1 );
#else
	size = 0;
	for ( i = 0; i < count; i++ )
		size += strlen( STRING( stringId[i] ) ) + 1;

	BufferHeader( pname, size );
	for ( i = 0; i < count; i++ )
	{
		const char *pString = STRING(stringId[i]);
		BufferData( pString, strlen(pString)+1 );
	}
#endif
}

void CSave :: WriteVector( const char *pname, const Vector &value )
{
	WriteVector( pname, &value.x, 1 );
}

void CSave :: WriteVector( const char *pname, const float *value, int count )
{
	BufferHeader( pname, sizeof(float) * 3 * count );
	BufferData( (const char *)value, sizeof(float) * 3 * count );
}

void CSave :: WritePositionVector( const char *pname, const Vector &value )
{
	if ( m_pdata && m_pdata->fUseLandmark )
	{
		Vector tmp = value - m_pdata->vecLandmarkOffset;
		WriteVector( pname, tmp );
	}

	WriteVector( pname, value );
}

void CSave :: WritePositionVector( const char *pname, const float *value, int count )
{
	int i;
	Vector tmp, input;

	BufferHeader( pname, sizeof(float) * 3 * count );
	for ( i = 0; i < count; i++ )
	{
		Vector tmp( value[0], value[1], value[2] );

		if ( m_pdata && m_pdata->fUseLandmark )
			tmp = tmp - m_pdata->vecLandmarkOffset;

		BufferData( (const char *)&tmp.x, sizeof(float) * 3 );
		value += 3;
	}
}

void CSave :: WriteFunction( DATAMAP *pRootMap, const char *pname, void **data, int count )
{
	const char *functionName = UTIL_FunctionToName( pRootMap, *data );

	if ( functionName )
		BufferField( pname, strlen(functionName) + 1, functionName );
	else
		ALERT( at_error, "Invalid function pointer in class %s!\n", pRootMap->dataClassName );
}

void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd )
{
	TYPEDESCRIPTION *pField;

	for ( int i = 0; i < ENTVARS_COUNT; i++ )
	{
		pField = &gEntvarsDescription[i];

		if ( !Q_stricmp( pField->fieldName, pkvd->szKeyName ) )
		{
			switch( pField->fieldType )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				(*(int *)((char *)pev + pField->fieldOffset)) = ALLOC_STRING( pkvd->szValue );
				break;

			case FIELD_TIME:
			case FIELD_FLOAT:
				(*(float *)((char *)pev + pField->fieldOffset)) = atof( pkvd->szValue );
				break;

			case FIELD_INTEGER:
				(*(int *)((char *)pev + pField->fieldOffset)) = atoi( pkvd->szValue );
				break;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				UTIL_StringToVector( (float *)((char *)pev + pField->fieldOffset), pkvd->szValue );
				break;
			case FIELD_EVARS:
			case FIELD_CLASSPTR:
			case FIELD_EDICT:
			case FIELD_ENTITY:
			case FIELD_POINTER:
			default:	ALERT( at_error, "Bad field in entity!!\n" );
				break;
			}

			// g-cont. HACKHACK to set origin and angles properly
			if( FStrEq( pField->fieldName, "origin" ))
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( pev );
				if( pEntity ) pEntity->SetAbsOrigin( pev->origin );
			}
			else if( FStrEq( pField->fieldName, "angles" ))
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( pev );
				if( pEntity )
				{
					if( pEntity->ObjectCaps() & FCAP_SET_MOVEDIR )
					{
						UTIL_SetMovedir( pEntity );
					}
					else if( pEntity->ObjectCaps() & FCAP_HOLD_ANGLES )
					{
						pEntity->m_vecTempAngles = pev->angles;
						pev->angles = g_vecZero;
					}
					pEntity->SetAbsAngles( pev->angles );
					pEntity->m_fSetAngles = TRUE;
				}
			}

			pkvd->fHandled = TRUE;
			return;
		}
	}
}

int CSave :: WriteEntVars( const char *pname, DATAMAP *pMap, entvars_t *pev )
{
	return WriteFields( pname, pev, pMap, gEntvarsDescription, ENTVARS_COUNT );
}

//-------------------------------------
// Purpose: Recursively saves all the classes in an object, in reverse order (top down)
// Output : int 0 on failure, 1 on success

int CSave::DoWriteAll( const void *pLeafObject, DATAMAP *pLeafMap, DATAMAP *pCurMap )
{
	// save base classes first
	if ( pCurMap->baseMap )
	{
		int status = DoWriteAll( pLeafObject, pLeafMap, pCurMap->baseMap );
		if( !status )
			return status;
	}

	return WriteFields( pCurMap->dataClassName, pLeafObject, pLeafMap, pCurMap->dataDesc, pCurMap->dataNumFields );
}

int CSave :: WriteFields( const char *pname, const void *pBaseData, DATAMAP *pMap, TYPEDESCRIPTION *pFields, int fieldCount )
{
	int i, j, actualCount, emptyCount;
	int entityArray[MAX_ENTITYARRAY];
	TYPEDESCRIPTION *pTest;

	// Precalculate the number of empty fields
	emptyCount = 0;
	for ( i = 0; i < fieldCount; i++ )
	{
		void *pOutputData;
		pOutputData = ((char *)pBaseData + pFields[i].fieldOffset );
		if ( DataEmpty( (const char *)pOutputData, pFields[i].fieldSize * gSizes[pFields[i].fieldType] ) )
			emptyCount++;
	}

	// Empty fields will not be written, write out the actual number of fields to be written
	actualCount = fieldCount - emptyCount;
	WriteInt( pname, &actualCount, 1 );

	for ( i = 0; i < fieldCount; i++ )
	{
		void *pOutputData;
		pTest = &pFields[ i ];
		pOutputData = ((char *)pBaseData + pTest->fieldOffset );

		// UNDONE: Must we do this twice?
		if ( DataEmpty( (const char *)pOutputData, pTest->fieldSize * gSizes[pTest->fieldType] ) )
			continue;

#ifdef _DEBUG
		// Log( pMap, pname, pTest->fieldName, pTest->fieldType, pOutputData, pTest->fieldSize );
#endif
		switch( pTest->fieldType )
		{
		case FIELD_FLOAT:
			WriteFloat( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_TIME:
			WriteTime( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_STRING:
			WriteString( pTest->fieldName, (int *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_CLASSPTR:
		case FIELD_EVARS:
		case FIELD_EDICT:
		case FIELD_ENTITY:
		case FIELD_EHANDLE:
			if ( pTest->fieldSize > MAX_ENTITYARRAY )
				ALERT( at_error, "Can't save more than %d entities in an array!!!\n", MAX_ENTITYARRAY );
			for ( j = 0; j < pTest->fieldSize; j++ )
			{
				switch( pTest->fieldType )
				{
					case FIELD_EVARS:
						entityArray[j] = EntityIndex( ((entvars_t **)pOutputData)[j] );
					break;
					case FIELD_CLASSPTR:
						entityArray[j] = EntityIndex( ((CBaseEntity **)pOutputData)[j] );
					break;
					case FIELD_EDICT:
						entityArray[j] = EntityIndex( ((edict_t **)pOutputData)[j] );
					break;
					case FIELD_ENTITY:
						entityArray[j] = EntityIndex( ((EOFFSET *)pOutputData)[j] );
					break;
					case FIELD_EHANDLE:
						entityArray[j] = EntityIndex( (CBaseEntity *)(((EHANDLE *)pOutputData)[j]) );
					break;
				}
			}
			WriteInt( pTest->fieldName, entityArray, pTest->fieldSize );
			break;
		case FIELD_POSITION_VECTOR:
			WritePositionVector( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_VECTOR:
			WriteVector( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_BOOLEAN:
		case FIELD_INTEGER:
			WriteInt( pTest->fieldName, (int *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_SHORT:
			WriteData( pTest->fieldName, 2 * pTest->fieldSize, ((char *)pOutputData) );
			break;
		case FIELD_CHARACTER:
			WriteData( pTest->fieldName, pTest->fieldSize, ((char *)pOutputData) );
			break;
		// For now, just write the address out, we're not going to change memory while doing this yet!
		case FIELD_POINTER:
			WriteInt( pTest->fieldName, (int *)(char *)pOutputData, pTest->fieldSize );
			break;
		case FIELD_FUNCTION:
			WriteFunction( pMap, pTest->fieldName, (void **)pOutputData, pTest->fieldSize );
			break;
		default:
			ALERT( at_error, "Bad field type\n" );
			break;
		}
	}

	return 1;
}

void CSave :: BufferString( char *pdata, int len )
{
	char c = 0;

	BufferData( pdata, len );		// Write the string
	BufferData( &c, 1 );			// Write a null terminator
}

int CSave :: DataEmpty( const char *pdata, int size )
{
	for ( int i = 0; i < size; i++ )
	{
		if ( pdata[i] )
			return 0;
	}
	return 1;
}

void CSave :: BufferField( const char *pname, int size, const char *pdata )
{
	BufferHeader( pname, size );
	BufferData( pdata, size );
}

void CSave :: BufferHeader( const char *pname, int size )
{
	short	hashvalue = TokenHash( pname );
	if ( size > 1<<(sizeof(short)*8) )
		ALERT( at_error, "CSave :: BufferHeader() size parameter exceeds 'short'!" );
	BufferData( (const char *)&size, sizeof(short) );
	BufferData( (const char *)&hashvalue, sizeof(short) );
}

void CSave :: BufferData( const char *pdata, int size )
{
	if ( !m_pdata )
		return;

	if ( m_pdata->size + size > m_pdata->bufferSize )
	{
		ALERT( at_error, "Save/Restore overflow!" );
		m_pdata->size = m_pdata->bufferSize;
		return;
	}

	memcpy( m_pdata->pCurrentData, pdata, size );
	m_pdata->pCurrentData += size;
	m_pdata->size += size;
}


// --------------------------------------------------------------
//
// CRestore
//
// --------------------------------------------------------------

int CRestore::ReadField( const void *pBaseData, DATAMAP *pMap, TYPEDESCRIPTION *pFields, int fieldCount, int startField, int size, char *pName, void *pData )
{
	int i, j, stringCount, fieldNumber, entityIndex;
	TYPEDESCRIPTION *pTest;
	float	time, timeData;
	Vector	position;
	edict_t	*pent;
	char	*pString;

	time = 0;
	position = Vector(0,0,0);

	if ( m_pdata )
	{
		time = m_pdata->time;
		if ( m_pdata->fUseLandmark )
			position = m_pdata->vecLandmarkOffset;
	}

	for ( i = 0; i < fieldCount; i++ )
	{
		fieldNumber = (i+startField)%fieldCount;
		pTest = &pFields[ fieldNumber ];
		if ( !stricmp( pTest->fieldName, pName ) )
		{
			if ( !m_global || !(pTest->flags & FTYPEDESC_GLOBAL) )
			{
				for ( j = 0; j < pTest->fieldSize; j++ )
				{
					void *pOutputData = ((char *)pBaseData + pTest->fieldOffset + (j*gSizes[pTest->fieldType]) );
					void *pInputData = (char *)pData + j * gSizes[pTest->fieldType];

					switch( pTest->fieldType )
					{
					case FIELD_TIME:
						timeData = *(float *)pInputData;
						// Re-base time variables
						timeData += time;
						*((float *)pOutputData) = timeData;
						break;
					case FIELD_FLOAT:
						*((float *)pOutputData) = *(float *)pInputData;
						break;
					case FIELD_MODELNAME:
					case FIELD_SOUNDNAME:
					case FIELD_STRING:
						// Skip over j strings
						pString = (char *)pData;
						for ( stringCount = 0; stringCount < j; stringCount++ )
						{
							while (*pString)
								pString++;
							pString++;
						}
						pInputData = pString;
						if ( strlen( (char *)pInputData ) == 0 )
							*((int *)pOutputData) = 0;
						else
						{
							int string;

							string = ALLOC_STRING( (char *)pInputData );
							
							*((int *)pOutputData) = string;

							if ( !FStringNull( string ) && m_precache )
							{
								if ( pTest->fieldType == FIELD_MODELNAME )
									PRECACHE_MODEL( (char *)STRING( string ) );
								else if ( pTest->fieldType == FIELD_SOUNDNAME )
									PRECACHE_SOUND( (char *)STRING( string ) );
							}
						}
						break;
					case FIELD_EVARS:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((entvars_t **)pOutputData) = VARS(pent);
						else
							*((entvars_t **)pOutputData) = NULL;
						break;
					case FIELD_CLASSPTR:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((CBaseEntity **)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((CBaseEntity **)pOutputData) = NULL;
						break;
					case FIELD_EDICT:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						*((edict_t **)pOutputData) = pent;
						break;
					case FIELD_EHANDLE:
						// Input and Output sizes are different!
						pOutputData = (char *)pOutputData + j*(sizeof(EHANDLE) - gSizes[pTest->fieldType]);
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((EHANDLE *)pOutputData) = CBaseEntity::Instance(pent);
						else
							*((EHANDLE *)pOutputData) = NULL;
						break;
					case FIELD_ENTITY:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((EOFFSET *)pOutputData) = OFFSET(pent);
						else
							*((EOFFSET *)pOutputData) = 0;
						break;
					case FIELD_VECTOR:
						((float *)pOutputData)[0] = ((float *)pInputData)[0];
						((float *)pOutputData)[1] = ((float *)pInputData)[1];
						((float *)pOutputData)[2] = ((float *)pInputData)[2];
						break;
					case FIELD_POSITION_VECTOR:
						((float *)pOutputData)[0] = ((float *)pInputData)[0] + position.x;
						((float *)pOutputData)[1] = ((float *)pInputData)[1] + position.y;
						((float *)pOutputData)[2] = ((float *)pInputData)[2] + position.z;
						break;
					case FIELD_BOOLEAN:
					case FIELD_INTEGER:
						*((int *)pOutputData) = *(int *)pInputData;
						break;
					case FIELD_SHORT:
						*((short *)pOutputData) = *(short *)pInputData;
						break;
					case FIELD_CHARACTER:
						*((char *)pOutputData) = *(char *)pInputData;
						break;
					case FIELD_POINTER:
						*((void **)pOutputData) = *(void **)pInputData;
						break;
					case FIELD_FUNCTION:
						ReadFunction( pMap, (void **)pOutputData, (const char *)pInputData );
						break;
					default:
						ALERT( at_error, "Bad field type\n" );
					}
				}
			}
			return fieldNumber;
		}
	}

	return -1;
}

int CRestore::ReadEntVars( const char *pname, DATAMAP *pMap, entvars_t *pev )
{
	return ReadFields( pname, pev, pMap, gEntvarsDescription, ENTVARS_COUNT );
}

int CRestore::ReadFields( const char *pname, const void *pBaseData, DATAMAP *pMap, TYPEDESCRIPTION *pFields, int fieldCount )
{
	int		lastField, fileCount;
	HEADER	header;

	// First entry should be an int
	ASSERT(ReadShort() == sizeof(int));

	// Check the struct name
	if (ReadShort() != TokenHash(pname))			// Field Set marker
	{
		//ALERT( at_error, "Expected %s found %s!\n", pname, BufferPointer() );
		BufferRewind(2 * sizeof(short));
		return 0;
	}

	// Skip over the struct name
	fileCount = ReadInt();						// Read field count
	lastField = 0;								// Make searches faster, most data is read/written in the same order

	// Clear out base data
	for (int i = 0; i < fieldCount; i++)
	{
		// Don't clear global fields
		if (!m_global || !(pFields[i].flags & FTYPEDESC_GLOBAL))
			memset(((char *)pBaseData + pFields[i].fieldOffset), 0, pFields[i].fieldSize * gSizes[pFields[i].fieldType]);
	}

	for (int i = 0; i < fileCount; i++)
	{
		BufferReadHeader(&header);
		lastField = ReadField(pBaseData, pMap, pFields, fieldCount, lastField, header.size, m_pdata->pTokens[header.token], header.pData);
		lastField++;
	}
	
	return 1;
}

//-------------------------------------
// Purpose: Recursively restores all the classes in an object, in reverse order (top down)
// Output : int 0 on failure, 1 on success

int CRestore::DoReadAll( void *pLeafObject, DATAMAP *pLeafMap, DATAMAP *pCurMap )
{
	// restore base classes first
	if ( pCurMap->baseMap )
	{
		int status = DoReadAll( pLeafObject, pLeafMap, pCurMap->baseMap );
		if ( !status )
			return status;
	}

	return ReadFields( pCurMap->dataClassName, pLeafObject, pLeafMap, pCurMap->dataDesc, pCurMap->dataNumFields );
}

void CRestore::BufferReadHeader( HEADER *pheader )
{
	ASSERT( pheader!=NULL );
	pheader->size = ReadShort();				// Read field size
	pheader->token = ReadShort();				// Read field name token
	pheader->pData = BufferPointer();			// Field Data is next
	BufferSkipBytes( pheader->size );			// Advance to next field
}

short CRestore::ReadShort( void )
{
	short tmp = 0;

	BufferReadBytes( (char *)&tmp, sizeof(short) );

	return tmp;
}

int CRestore::ReadInt( void )
{
	int tmp = 0;

	BufferReadBytes( (char *)&tmp, sizeof(int) );

	return tmp;
}

int CRestore::ReadNamedInt( const char *pName )
{
	HEADER header;

	BufferReadHeader( &header );
	return ((int *)header.pData)[0];
}

char *CRestore::ReadNamedString( const char *pName )
{
	HEADER header;

	BufferReadHeader( &header );
#ifdef TOKENIZE
	return (char *)(m_pdata->pTokens[*(short *)header.pData]);
#else
	return (char *)header.pData;
#endif
}

char *CRestore::BufferPointer( void )
{
	if ( !m_pdata )
		return NULL;

	return m_pdata->pCurrentData;
}

void CRestore::BufferReadBytes( char *pOutput, int size )
{
	ASSERT( m_pdata !=NULL );

	if ( !m_pdata || Empty() )
		return;

	if ( (m_pdata->size + size) > m_pdata->bufferSize )
	{
		ALERT( at_error, "Restore overflow!" );
		m_pdata->size = m_pdata->bufferSize;
		return;
	}

	if ( pOutput )
		memcpy( pOutput, m_pdata->pCurrentData, size );
	m_pdata->pCurrentData += size;
	m_pdata->size += size;
}

int CRestore :: ReadFunction( DATAMAP *pMap, void **pValue, const char *pszFunctionName )
{
	if ( strlen( (char *)pszFunctionName ) == 0 )
		*pValue = NULL;
	else
		*pValue = UTIL_FunctionFromName( pMap, pszFunctionName );

	return 0;
}

void CRestore::BufferSkipBytes( int bytes )
{
	BufferReadBytes( NULL, bytes );
}
