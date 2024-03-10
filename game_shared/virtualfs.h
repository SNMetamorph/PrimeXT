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

	template<class T> T Read() 
	{
		T temp;
		this->Read(reinterpret_cast<void*>(&temp), sizeof(T));
		return temp;
	}

	template<class T> size_t Write(const T &data)
	{
		return this->Write(reinterpret_cast<const void*>(&data), sizeof(T));
	}

	size_t Read( void *out, size_t size );
	size_t Write( const void *in, size_t size );
	size_t Insert( const void *in, size_t size );
	int Seek(size_t offset, int whence);
	int Gets(char *string, size_t size);
	int Getc();

	inline char *GetBuffer()	{ return (char *)m_CurrentState.m_pBuffer; };
	inline size_t GetSize()		{ return m_CurrentState.m_iLength; };
	inline size_t Tell()		{ return m_CurrentState.m_iOffset; };
	inline bool Eof()			{ return (m_CurrentState.m_iOffset == m_CurrentState.m_iLength) ? true : false; };
	inline void SaveState()		{ m_SavedState = m_CurrentState; };
	inline void RestoreState()	{ m_CurrentState = m_SavedState; };

	size_t Print(const char *message);
	size_t IPrint(const char *message);
	size_t Printf(const char *fmt, ...);
	size_t IPrintf(const char *fmt, ...);
	size_t VPrintf(const char *fmt, va_list ap);
	size_t IVPrintf(const char *fmt, va_list ap);

private:
	struct State 
	{
		byte	*m_pBuffer;		// file buffer
		size_t	m_iBuffSize;	// real buffer size
		size_t	m_iOffset;		// buffer position
		size_t	m_iLength;		// buffer current size
	};

	State m_CurrentState;
	State m_SavedState;
};

#endif//VIRTUALFS_H
