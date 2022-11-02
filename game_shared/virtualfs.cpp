//=======================================================================
//			Copyright (C) XashXT Group 2014
//		  virtualfs.h - Virtual FileSystem that writes into memory 
//=======================================================================
#include "virtualfs.h"
#include "stringlib.h"

CVirtualFS :: CVirtualFS()
{
	CVirtualFS::State &state = m_CurrentState;
	state.m_iBuffSize = FS_MEM_BLOCK; // can be resized later
	state.m_pBuffer = new byte[state.m_iBuffSize];
	memset(state.m_pBuffer, 0, state.m_iBuffSize );
	state.m_iLength = state.m_iOffset = 0;
}

CVirtualFS :: CVirtualFS( const byte *file, size_t size )
{
	CVirtualFS::State &state = m_CurrentState;
	if( !file || size <= 0 )
	{
		state.m_iBuffSize = state.m_iOffset = state.m_iLength = 0;
		state.m_pBuffer = NULL;
		return;
    }

	state.m_iLength = state.m_iBuffSize = size;
	state.m_pBuffer = new byte[state.m_iBuffSize];
	memcpy(state.m_pBuffer, file, state.m_iBuffSize );
	state.m_iOffset = 0;
}

CVirtualFS :: ~CVirtualFS()
{
	delete [] m_CurrentState.m_pBuffer;
}

size_t CVirtualFS :: Read( void *out, size_t size )
{
	CVirtualFS::State &state = m_CurrentState;
	if( !state.m_pBuffer || !out || size <= 0 )
		return 0;

	// check for enough room
	if (state.m_iOffset >= state.m_iLength )
		return 0; // hit EOF

	size_t read_size = 0;

	if(state.m_iOffset + size <= state.m_iLength )
	{
		memcpy( out, state.m_pBuffer + state.m_iOffset, size );
		state.m_iOffset += size;
		read_size = size;
	}
	else
	{
		size_t reduced_size = state.m_iLength - state.m_iOffset;
		memcpy( out, state.m_pBuffer + state.m_iOffset, reduced_size );
		state.m_iOffset += reduced_size;
		read_size = reduced_size;
	}

	return read_size;
}

size_t CVirtualFS :: Write( const void *in, size_t size )
{
	CVirtualFS::State &state = m_CurrentState;
	if( !state.m_pBuffer ) 
		return -1;

	if ( state.m_iOffset + size >= state.m_iBuffSize )
	{
		size_t newsize = state.m_iOffset + size + FS_MEM_BLOCK;

		if ( state.m_iBuffSize < newsize )
		{
			// reallocate buffer now
			state.m_pBuffer = (byte *)realloc(state.m_pBuffer, newsize );
			memset(state.m_pBuffer + state.m_iBuffSize, 0, newsize - state.m_iBuffSize );
			state.m_iBuffSize = newsize; // update buffsize
		}
	}

	// write into buffer
	memcpy(state.m_pBuffer + state.m_iOffset, in, size );
	state.m_iOffset += size;

	if(state.m_iOffset > state.m_iLength )
		state.m_iLength = state.m_iOffset;

	return state.m_iLength;
}

size_t CVirtualFS :: Insert( const void *in, size_t size )
{
	CVirtualFS::State &state = m_CurrentState;
	if( !state.m_pBuffer ) 
		return -1;

	if(state.m_iLength + size >= state.m_iBuffSize )
	{
		size_t newsize = state.m_iLength + size + FS_MEM_BLOCK;

		if(state.m_iBuffSize < newsize )
		{
			// reallocate buffer now
			state.m_pBuffer = (byte *)realloc(state.m_pBuffer, newsize );
			memset(state.m_pBuffer + state.m_iBuffSize, 0, newsize - state.m_iBuffSize );
			state.m_iBuffSize = newsize; // update buffsize
		}
	}

	// backup right part
	size_t rp_size = state.m_iLength - state.m_iOffset;
	byte *backup = new byte[rp_size];
	memcpy( backup, state.m_pBuffer + state.m_iOffset, rp_size );

	// insert into buffer
	memcpy(state.m_pBuffer + state.m_iOffset, in, size);
	state.m_iOffset += size;

	// write right part buffer
	memcpy(state.m_pBuffer + state.m_iOffset, backup, rp_size);
	delete [] backup;

	if((state.m_iOffset + rp_size ) > state.m_iLength )
		state.m_iLength = state.m_iOffset + rp_size;

	return state.m_iLength;
}

size_t CVirtualFS :: Print( const char *message )
{
	return Write( message, Q_strlen( message ));
}

size_t CVirtualFS :: IPrint( const char *message )
{
	return Insert( message, Q_strlen( message ));
}

size_t CVirtualFS :: Printf( const char *fmt, ... )
{
	size_t result;
	va_list args;

	va_start( args, fmt );
	result = VPrintf( fmt, args );
	va_end( args );

	return result;
}

size_t CVirtualFS :: IPrintf( const char *fmt, ... )
{
	size_t result;
	va_list args;

	va_start( args, fmt );
	result = IVPrintf( fmt, args );
	va_end( args );

	return result;
}

size_t CVirtualFS :: VPrintf( const char *fmt, va_list ap )
{
	size_t	buff_size = FS_MSG_BLOCK;
	char	*tempbuff;
	size_t	len;

	while( 1 )
	{
		tempbuff = new char[buff_size];
		len = Q_vsprintf( tempbuff, fmt, ap );
		if( len >= 0 && len < buff_size )
			break;
		delete [] tempbuff;
		buff_size <<= 1;
	}

	len = Write( tempbuff, len );
	delete [] tempbuff;

	return len;
}

size_t CVirtualFS :: IVPrintf( const char *fmt, va_list ap )
{
	size_t	buff_size = FS_MSG_BLOCK;
	char	*tempbuff;
	size_t	len;

	while( 1 )
	{
		tempbuff = new char[buff_size];
		len = Q_vsprintf( tempbuff, fmt, ap );
		if( len >= 0 && len < buff_size )
			break;
		delete [] tempbuff;
		buff_size <<= 1;
	}

	len = Insert( tempbuff, len );
	delete [] tempbuff;

	return len;
}

int CVirtualFS :: Getc( void )
{
	char c;

	if( !Read( &c, 1 ))
		return EOF;
	return (byte)c;
}

int CVirtualFS :: Gets( char *string, size_t size )
{
	size_t	end = 0;
	int	c;

	while( 1 )
	{
		c = Getc();

		if( c == '\r' || c == '\n' || c < 0 )
			break;

		if( end < ( size - 1 ))
			string[end++] = c;
	}

	string[end] = 0;

	// remove \n following \r
	if( c == '\r' )
	{
		c = Getc();
		if( c != '\n' ) Seek( -1, SEEK_CUR ); // rewind
	}

	return c;
}

int CVirtualFS :: Seek( size_t offset, int whence )
{
	CVirtualFS::State &state = m_CurrentState;
	// Compute the file offset
	switch( whence )
	{
	case SEEK_CUR:
		offset += state.m_iOffset;
		break;
	case SEEK_SET:
		break;
	case SEEK_END:
		offset += state.m_iLength;
		break;
	default: 
		return -1;
	}

	if(( offset < 0 ) || ( offset > state.m_iLength ))
		return -1;

	state.m_iOffset = offset;

	return 0;
}
