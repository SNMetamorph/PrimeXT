//=======================================================================
//			Copyright (C) XashXT Group 2014
//		  virtualfs.h - Virtual FileSystem that writes into memory 
//=======================================================================
#include "virtualfs.h"
#include "stringlib.h"

CVirtualFS :: CVirtualFS()
{
	m_iBuffSize = FS_MEM_BLOCK; // can be resized later
	m_pBuffer = new byte[m_iBuffSize];
	memset( m_pBuffer, 0, m_iBuffSize );
	m_iLength = m_iOffset = 0;
}

CVirtualFS :: CVirtualFS( const byte *file, size_t size )
{
	if( !file || size <= 0 )
	{
		m_iBuffSize = m_iOffset = m_iLength = 0;
		m_pBuffer = NULL;
		return;
          }

	m_iLength = m_iBuffSize = size;
	m_pBuffer = new byte[m_iBuffSize];	
	memcpy( m_pBuffer, file, m_iBuffSize );
	m_iOffset = 0;
}

CVirtualFS :: ~CVirtualFS()
{
	delete [] m_pBuffer;
}

size_t CVirtualFS :: Read( void *out, size_t size )
{
	if( !m_pBuffer || !out || size <= 0 )
		return 0;

	// check for enough room
	if( m_iOffset >= m_iLength )
		return 0; // hit EOF

	size_t read_size = 0;

	if( m_iOffset + size <= m_iLength )
	{
		memcpy( out, m_pBuffer + m_iOffset, size );
		m_iOffset += size;
		read_size = size;
	}
	else
	{
		int reduced_size = m_iLength - m_iOffset;
		memcpy( out, m_pBuffer + m_iOffset, reduced_size );
		m_iOffset += reduced_size;
		read_size = reduced_size;
	}

	return read_size;
}

size_t CVirtualFS :: Write( const void *in, size_t size )
{
	if( !m_pBuffer ) return -1;

	if( m_iOffset + size >= m_iBuffSize )
	{
		size_t newsize = m_iOffset + size + FS_MEM_BLOCK;

		if( m_iBuffSize < newsize )
		{
			// reallocate buffer now
			m_pBuffer = (byte *)realloc( m_pBuffer, newsize );
			memset( m_pBuffer + m_iBuffSize, 0, newsize - m_iBuffSize );
			m_iBuffSize = newsize; // update buffsize
		}
	}

	// write into buffer
	memcpy( m_pBuffer + m_iOffset, in, size );
	m_iOffset += size;

	if( m_iOffset > m_iLength ) 
		m_iLength = m_iOffset;

	return m_iLength;
}

size_t CVirtualFS :: Insert( const void *in, size_t size )
{
	if( !m_pBuffer ) return -1;

	if( m_iLength + size >= m_iBuffSize )
	{
		size_t newsize = m_iLength + size + FS_MEM_BLOCK;

		if( m_iBuffSize < newsize )
		{
			// reallocate buffer now
			m_pBuffer = (byte *)realloc( m_pBuffer, newsize );
			memset( m_pBuffer + m_iBuffSize, 0, newsize - m_iBuffSize );
			m_iBuffSize = newsize; // update buffsize
		}
	}

	// backup right part
	size_t rp_size = m_iLength - m_iOffset;
	byte *backup = new byte[rp_size];
	memcpy( backup, m_pBuffer + m_iOffset, rp_size );

	// insert into buffer
	memcpy( m_pBuffer + m_iOffset, in, size );
	m_iOffset += size;

	// write right part buffer
	memcpy( m_pBuffer + m_iOffset, backup, rp_size );
	delete [] backup;

	if(( m_iOffset + rp_size ) > m_iLength ) 
		m_iLength = m_iOffset + rp_size;

	return m_iLength;
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
	// Compute the file offset
	switch( whence )
	{
	case SEEK_CUR:
		offset += m_iOffset;
		break;
	case SEEK_SET:
		break;
	case SEEK_END:
		offset += m_iLength;
		break;
	default: 
		return -1;
	}

	if(( offset < 0 ) || ( offset > m_iLength ))
		return -1;

	m_iOffset = offset;

	return 0;
}
