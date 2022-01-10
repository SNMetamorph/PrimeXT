//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxstring.cpp
// implementation: all
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//

#include <windows.h>
#include <mxstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



int
mx_strncasecmp (const char *s1, const char *s2, int count)
{
#ifdef WIN32
	return _strnicmp (s1, s2, count);
#else
	return strncasecmp (s1, s2, count);
#endif
}



int
mx_strcasecmp (const char *s1, const char *s2)
{
#ifdef WIN32
	return _stricmp (s1, s2);
#else
	return strcasecmp (s1, s2);
#endif
}

int mx_strlen( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		p++;
		len++;
	}
	return len;
}

char *mx_strlower (char *str)
{
	int i;
	for (i = strlen (str) - 1; i >= 0; i--)
		str[i] = tolower (str[i]);

	return str;
}

int mx_vsnprintf( char *buffer, size_t buffersize, const char *format, va_list args )
{
	size_t	result;

	result = _vsnprintf( buffer, buffersize, format, args );

	if( result < 0 || result >= buffersize )
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}
	return result;
}

int mx_snprintf( char *buffer, size_t buffersize, const char *format, ... )
{
	va_list	args;
	int	result;

	va_start( args, format );
	result = mx_vsnprintf( buffer, buffersize, format, args );
	va_end( args );

	return result;
}

int mx_sprintf( char *buffer, const char *format, ... )
{
	va_list	args;
	int	result;

	va_start( args, format );
	result = mx_vsnprintf( buffer, 99999, format, args );
	va_end( args );

	return result;
}

char mx_tolower( const char in )
{
	char	out;

	if( in >= 'A' && in <= 'Z' )
		out = in + 'a' - 'A';
	else out = in;

	return out;
}

char *mx_stristr( const char *string, const char *string2 )
{
	int	c, len;

	if( !string || !string2 ) return NULL;

	c = mx_tolower( *string2 );
	len = mx_strlen( string2 );

	while( string )
	{
		for( ; *string && mx_tolower( *string ) != c; string++ );

		if( *string )
		{
			if( !mx_strncasecmp( string, string2, len ))
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
}