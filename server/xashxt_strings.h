//=======================================================================
//			Copyright (C) XashXT Group 201
//		         stringlib.h - safety string routines 
//=======================================================================
#ifndef STRINGS_H
#define STRINGS_H

#include <utlrbtree.h>

class CStringPool
{
public:
	CStringPool();
	~CStringPool();

	unsigned int Count() const;

	string_t AllocString( const char *pszValue );

	// searches for a string already in the pool
	const char *FindString( string_t iString );

	void MakeEmptyString( void );

	void FreeAll( void );

	void Dump( void );

	void DumpSorted( void );
protected:
	typedef CUtlRBTree<const char *, unsigned short> CStrSet;

	CStrSet m_Strings;
};

extern CStringPool g_GameStringPool;

#endif//STRINGS_H
