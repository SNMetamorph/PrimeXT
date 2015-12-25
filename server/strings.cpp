/*
strings.cpp - game strings pool
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "extdll.h"
#include "util.h"
#include <stringlib.h>

CStringPool g_GameStringPool;

static bool StrLess( const char * const &pszLeft, const char * const &pszRight )
{
	return ( Q_strcmp( pszLeft, pszRight) < 0 );
}

CStringPool::CStringPool() : m_Strings( 32, 256, &StrLess )
{
	MakeEmptyString();
}

CStringPool :: ~CStringPool()
{
	FreeAll();
}

unsigned int CStringPool :: Count() const
{
	return m_Strings.Count();
}

const char *CStringPool :: FindString( string_t iString )
{
	if( m_Strings.IsValidIndex( iString ) )
		return m_Strings[iString];

	return NULL;
}

string_t CStringPool :: AllocString( const char *pszValue )
{
	unsigned short i = m_Strings.Find( pszValue );

	if( i != m_Strings.InvalidIndex( ))
		return i;

	return m_Strings.Insert( strdup( pszValue ));
}

void CStringPool :: MakeEmptyString( void )
{
	// empty string is always should be set at index 0
	unsigned short i = AllocString( "" );

	if( i != 0 ) ALERT( at_error, "Empty string has bad index %i!\n", i );
}

void CStringPool :: FreeAll( void )
{
	unsigned short i = m_Strings.FirstInorder();

	while( i != m_Strings.InvalidIndex() )
	{
		free( (void *)m_Strings[i] );
		i = m_Strings.NextInorder(i);
	}

	m_Strings.Purge();
}

void CStringPool :: Dump( void )
{
	for( unsigned int i = 0; i < m_Strings.Count(); i++ )
	{
		Msg( "  %d (0x%x) : %s\n", i, m_Strings[i], m_Strings[i] );
	}

	Msg( "\nSize:  %d items\n", m_Strings.Count() );
}

void CStringPool :: DumpSorted( void )
{
	for( int i = m_Strings.FirstInorder(); i != m_Strings.InvalidIndex(); i = m_Strings.NextInorder(i) )
	{
		Msg( "  %d (0x%x) : %s\n", i, m_Strings[i], m_Strings[i] );
	}

	Msg( "\nSize:  %d items\n", m_Strings.Count() );
}
