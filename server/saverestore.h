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
// Implementation in UTIL.CPP
#ifndef SAVERESTORE_H
#define SAVERESTORE_H

#include "eiface.h"

class CBaseEntity;

class CSaveRestoreBuffer
{
public:
	CSaveRestoreBuffer( void );
	CSaveRestoreBuffer( SAVERESTOREDATA *pdata );
	~CSaveRestoreBuffer( void );

	int		EntityIndex( entvars_t *pevLookup );
	int		EntityIndex( edict_t *pentLookup );
	int		EntityIndex( EOFFSET eoLookup );
	int		EntityIndex( CBaseEntity *pEntity );

	int		EntityFlags( int entityIndex, int flags ) { return EntityFlagsSet( entityIndex, 0 ); }
	int		EntityFlagsSet( int entityIndex, int flags );

	edict_t		*EntityFromIndex( int entityIndex );

	unsigned short	TokenHash( const char *pszToken );
	Vector GetLandmark() const { return ( m_pdata->fUseLandmark ) ? m_pdata->vecLandmarkOffset : g_vecZero; }
	Vector		modelSpaceOffset; // used only for globaly entity brushes modelled in different coordinate systems.
	Vector		modelOriginOffset;
protected:
	SAVERESTOREDATA	*m_pdata;
	void		BufferRewind( int size );
	unsigned int	HashString( const char *pszToken );
};


class CSave : public CSaveRestoreBuffer
{
public:
	CSave( SAVERESTOREDATA *pdata ) : CSaveRestoreBuffer( pdata ) {};
	int	WriteAll( const void *pLeafObject, DATAMAP *pLeafMap ) { return DoWriteAll( pLeafObject, pLeafMap, pLeafMap ); }
	int	DoWriteAll( const void *pLeafObject, DATAMAP *pLeafMap, DATAMAP *pCurMap );

	void	WriteShort( const char *pname, const short *value, int count );
	void	WriteInt( const char *pname, const int *value, int count );		// Save an int
	void	WriteFloat( const char *pname, const float *value, int count );	// Save a float
	void	WriteTime( const char *pname, const float *value, int count );	// Save a float (timevalue)
	void	WriteData( const char *pname, int size, const char *pdata );		// Save a binary data block
	void	WriteString( const char *pname, const char *pstring );			// Save a null-terminated string
	void	WriteString( const char *pname, const int *stringId, int count );	// Save a null-terminated string (engine string)
	void	WriteVector( const char *pname, const Vector &value );				// Save a vector
	void	WriteVector( const char *pname, const float *value, int count );	// Save a vector
	void	WritePositionVector( const char *pname, const Vector &value );		// Offset for landmark if necessary
	void	WritePositionVector( const char *pname, const float *value, int count );	// array of pos vectors
	void	WriteFunction( DATAMAP *pRootMap, const char *pname, void **value, int count );	// Save a function pointer
	int	WriteEntVars( const char *pname, DATAMAP *pMap, entvars_t *pev );	// Save entvars_t (entvars_t)
	int	WriteFields( const char *pname, const void *pBaseData, DATAMAP *pMap, TYPEDESCRIPTION *pFields, int fieldCount );

private:
	void	Log( DATAMAP *pMap, const char *pName, const char *pFieldName, FIELDTYPE fieldType, void *value, int count );
	int	DataEmpty( const char *pdata, int size );
	void	BufferField( const char *pname, int size, const char *pdata );
	void	BufferString( char *pdata, int len );
	void	BufferData( const char *pdata, int size );
	void	BufferHeader( const char *pname, int size );
};

typedef struct 
{
	unsigned short	size;
	unsigned short	token;
	char		*pData;
} HEADER;

class CRestore : public CSaveRestoreBuffer
{
public:
	CRestore( SAVERESTOREDATA *pdata ) : CSaveRestoreBuffer( pdata ) { m_global = 0; m_precache = TRUE; modelOriginOffset = modelSpaceOffset = g_vecZero; }
	int	ReadAll( void *pLeafObject, DATAMAP *pLeafMap )	{ return DoReadAll( pLeafObject, pLeafMap, pLeafMap ); }
	int	DoReadAll( void *pLeafObject, DATAMAP *pLeafMap, DATAMAP *pCurMap );
	
	int	ReadEntVars( const char *pname, DATAMAP *pMap, entvars_t *pev ); // entvars_t
	int	ReadFields( const char *pname, const void *pBaseData, DATAMAP *pMap, TYPEDESCRIPTION *pFields, int fieldCount );
	int	ReadField( const void *pBaseData, DATAMAP *pMap, TYPEDESCRIPTION *pFields, int fieldCount, int startField, int size, char *pName, void *pData );
	int	ReadFunction( DATAMAP *pMap, void **pValue, const char *pszFunctionName );
	int	ReadInt( void );
	short	ReadShort( void );
	int	ReadNamedInt( const char *pName );
	char	*ReadNamedString( const char *pName );
	int	Empty( void ) { return (m_pdata == NULL) || ((m_pdata->pCurrentData-m_pdata->pBaseData)>=m_pdata->bufferSize); }
	inline	void SetGlobalMode( int global ) { m_global = global; }
	void	PrecacheMode( BOOL mode ) { m_precache = mode; }
	BOOL	IsGlobalMode( void ) { return m_global; }
private:
	char	*BufferPointer( void );
	void	BufferReadBytes( char *pOutput, int size );
	void	BufferSkipBytes( int bytes );
	void	BufferReadHeader( HEADER *pheader );

	int	m_global;		// Restoring a global entity?
	BOOL	m_precache;
};

#define MAX_ENTITYARRAY 	64

typedef struct globalentity_s globalentity_t;

struct globalentity_s
{
	DECLARE_SIMPLE_DATADESC();

	char		name[64];
	char		levelName[32];
	GLOBALESTATE	state;
	float		global_time;
	globalentity_t	*pNext;
};

class CGlobalState
{
	DECLARE_SIMPLE_DATADESC();
public:
			CGlobalState();
	void		Reset( void );
	void		ClearStates( void );
	void		EntityAdd( string_t globalname, string_t mapName, GLOBALESTATE state, float time = 0.0f );
	void		EntitySetState( string_t globalname, GLOBALESTATE state );
	void		EntitySetTime( string_t globalname, float time );
	void		EntityUpdate( string_t globalname, string_t mapname );
	const globalentity_t *EntityFromTable( string_t globalname );
	GLOBALESTATE	EntityGetState( string_t globalname );
	float		EntityGetTime( string_t globalname );
	int		EntityInTable( string_t globalname ) { return (Find( globalname ) != NULL) ? 1 : 0; }
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	void		DumpGlobals( void );

	static TYPEDESCRIPTION m_SaveData[];
private:
	globalentity_t	*Find( string_t globalname );
	globalentity_t	*m_pList;
	int		m_listCount;
};

extern CGlobalState gGlobalState;

#endif//SAVERESTORE_H
