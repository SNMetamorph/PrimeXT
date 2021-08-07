//=======================================================================
//			Copyright (C) XashXT Group 2014
//		  virtualfs.h - Virtual FileSystem that writes into memory 
//=======================================================================
#ifndef VIRTUALFS_H
#define VIRTUALFS_H
#include <stdio.h>
#include "const.h"

#define FS_MEM_BLOCK	65535
#define FS_MSG_BLOCK	8192

class CVirtualFS
{
public:
	CVirtualFS();
	CVirtualFS( const byte *file, size_t size );
	~CVirtualFS();

	size_t Read( void *out, size_t size );
	size_t Write( const void *in, size_t size );
	size_t Insert( const void *in, size_t size );
	size_t Print( const char *message );
	size_t IPrint( const char *message );
	size_t Printf( const char *fmt, ... );
	size_t IPrintf( const char *fmt, ... );
	size_t VPrintf( const char *fmt, va_list ap );
	size_t IVPrintf( const char *fmt, va_list ap );
	char *GetBuffer( void ) { return (char *)m_pBuffer; };
	size_t GetSize( void ) { return m_iLength; };
	size_t Tell( void ) { return m_iOffset; }
	bool Eof( void ) { return (m_iOffset == m_iLength) ? true : false; }
	int Seek( size_t offset, int whence );
	int Gets( char *string, size_t size );
	int Getc( void );
private:
	byte	*m_pBuffer;	// file buffer
	size_t	m_iBuffSize;	// real buffer size
	size_t	m_iOffset;	// buffer position
	size_t	m_iLength;	// buffer current size
};

#endif//VIRTUALFS_H
