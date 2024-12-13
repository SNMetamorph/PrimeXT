//=======================================================================
//			Copyright (C) XashXT Group 2011
//		         stringlib.cpp - safety string routines 
//=======================================================================
#include "port.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <mathlib.h>
#include <stringlib.h>

#pragma warning(disable : 4244)	// MIPS

void Q_strnupr( const char *in, char *out, size_t size_out )
{
	if( size_out == 0 ) return;

	while( *in && size_out > 1 )
	{
		if( *in >= 'a' && *in <= 'z' )
			*out++ = *in++ + 'A' - 'a';
		else *out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

void Q_strnlwr( const char *in, char *out, size_t size_out )
{
	if( size_out == 0 ) return;

	while( *in && size_out > 1 )
	{
		if( *in >= 'A' && *in <= 'Z' )
			*out++ = *in++ + 'a' - 'A';
		else *out++ = *in++;
		size_out--;
	}
	*out = '\0';
}

bool Q_isdigit( const char *str )
{
	if( str && *str )
	{
		while( isdigit( *str )) str++;
		if( !*str ) return true;
	}
	return false;
}

int Q_strlen( const char *string )
{
	if( !string ) return 0;

	int len = 0;
	const char *p = string;
	while( *p )
	{
		p++;
		len++;
	}
	return len;
}

char Q_toupper( const char in )
{
	char	out;

	if( in >= 'a' && in <= 'z' )
		out = in + 'A' - 'a';
	else out = in;

	return out;
}

char Q_tolower( const char in )
{
	char	out;

	if( in >= 'A' && in <= 'Z' )
		out = in + 'a' - 'A';
	else out = in;

	return out;
}

size_t Q_strncat( char *dst, const char *src, size_t size )
{
	if( !dst || !src || !size )
		return 0;

	char *d = dst;
	const char	*s = src;
	size_t n = size;
	size_t dlen;

	// find the end of dst and adjust bytes left but don't go past end
	while( n-- != 0 && *d != '\0' ) d++;
	dlen = d - dst;
	n = size - dlen;

	if( n == 0 ) return( dlen + Q_strlen( s ));

	while( *s != '\0' )
	{
		if( n != 1 )
		{
			*d++ = *s;
			n--;
		}
		s++;
	}

	*d = '\0';
	return( dlen + ( s - src )); // count does not include NULL
}

size_t Q_strncpy( char *dst, const char *src, size_t size )
{
	if( !dst || !src || !size )
		return 0;

	char *d = dst;
	const char	*s = src;
	size_t n = size;

	// copy as many bytes as will fit
	if( n != 0 && --n != 0 )
	{
		do
		{
			if(( *d++ = *s++ ) == 0 )
				break;
		} while( --n != 0 );
	}

	// not enough room in dst, add NULL and traverse rest of src
	if( n == 0 )
	{
		if( size != 0 )
			*d = '\0'; // NULL-terminate dst
		while( *s++ );
	}
	return ( s - src - 1 ); // count does not include NULL
}

char *copystring( const char *s )
{
	if( !s ) return NULL;

	char *b = new char[Q_strlen( s ) + 1];
	Q_strcpy( b, s );

	return b;
}

int Q_atoi( const char *str )
{
	int       val = 0;
	int	c, sign;

	if( !str ) return 0;

	// check for empty charachters in string
	while( str && *str == ' ' )
		str++;

	if( !str ) return 0;
	
	if( *str == '-' )
	{
		sign = -1;
		str++;
	}
	else sign = 1;
		
	// check for hex
	if( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ))
	{
		str += 2;
		while( 1 )
		{
			c = *str++;
			if( c >= '0' && c <= '9' ) val = (val<<4) + c - '0';
			else if( c >= 'a' && c <= 'f' ) val = (val<<4) + c - 'a' + 10;
			else if( c >= 'A' && c <= 'F' ) val = (val<<4) + c - 'A' + 10;
			else return val * sign;
		}
	}
	
	// check for character
	if( str[0] == '\'' )
		return sign * str[1];
	
	// assume decimal
	while( 1 )
	{
		c = *str++;
		if( c < '0' || c > '9' )
			return val * sign;
		val = val * 10 + c - '0';
	}
	return 0;
}

float Q_atof( const char *str )
{
	double	val = 0;
	int	c, sign, decimal, total;

	if( !str ) return 0.0f;

	// check for empty charachters in string
	while( str && *str == ' ' )
		str++;

	if( !str ) return 0.0f;
	
	if( *str == '-' )
	{
		sign = -1;
		str++;
	}
	else sign = 1;
		
	// check for hex
	if( str[0] == '0' && ( str[1] == 'x' || str[1] == 'X' ))
	{
		str += 2;
		while( 1 )
		{
			c = *str++;
			if( c >= '0' && c <= '9' ) val = (val * 16) + c - '0';
			else if( c >= 'a' && c <= 'f' ) val = (val * 16) + c - 'a' + 10;
			else if( c >= 'A' && c <= 'F' ) val = (val * 16) + c - 'A' + 10;
			else return val * sign;
		}
	}
	
	// check for character
	if( str[0] == '\'' ) return sign * str[1];
	
	// assume decimal
	decimal = -1;
	total = 0;
	while( 1 )
	{
		c = *str++;
		if( c == '.' )
		{
			decimal = total;
			continue;
		}

		if( c < '0' || c > '9' )
			break;
		val = val * 10 + c - '0';
		total++;
	}

	if( decimal == -1 )
		return val * sign;

	while( total > decimal )
	{
		val /= 10;
		total--;
	}
	
	return val * sign;
}

static void Q_atovn(const char *str, float *result, size_t size)
{
	char	buffer[256];
	char *pstr, *pfront;

	Q_strncpy(buffer, str, sizeof(buffer));
	memset(result, 0, sizeof(float) * size);
	pstr = pfront = buffer;

	for (size_t j = 0; j < size; j++)
	{
		result[j] = Q_atof(pfront);

		// valid separator is space
		while (*pstr && *pstr != ' ')
			pstr++;

		if (!*pstr) break;
		pstr++;
		pfront = pstr;
	}
}

vec3_t Q_atov(const char *str)
{
	vec3_t vec = g_vecZero;
	Q_atovn(str, &vec.x, 3);
	return vec;
}

vec2_t Q_atov2(const char *str)
{
	vec2_t vec = Vector2D( 0.0f, 0.0f );
	Q_atovn(str, &vec.x, 2);
	return vec;
}

char *Q_strchr( const char *s, char c )
{
	int	len = Q_strlen( s );

	while( len-- )
	{
		if( *++s == c )
			return (char *)s;
	}
	return 0;
}

char *Q_strrchr( const char *s, char c )
{
	int	len = Q_strlen( s );

	s += len;

	while( len-- )
	{
		if( *--s == c )
			return (char *)s;
	}
	return 0;
}

int Q_strnicmp( const char *s1, const char *s2, int n )
{
	int	c1, c2;

	if( s1 == NULL )
	{
		if( s2 == NULL ) return 0;
		else return -1;
	}
	else if( s2 == NULL )
		return 1;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if( !n-- ) return 0; // strings are equal until end point
		
		if( c1 != c2 )
		{
			if( c1 >= 'a' && c1 <= 'z' ) c1 -= ('a' - 'A');
			if( c2 >= 'a' && c2 <= 'z' ) c2 -= ('a' - 'A');
			if( c1 != c2 ) return c1 < c2 ? -1 : 1;
		}
	} while( c1 );

	// strings are equal
	return 0;
}

int Q_strncmp( const char *s1, const char *s2, int n )
{
	int		c1, c2;

	if( s1 == NULL )
	{
		if( s2 == NULL ) return 0;
		else return -1;
	}
	else if( s2 == NULL )
		return 1;
	
	do {
		c1 = *s1++;
		c2 = *s2++;

		// strings are equal until end point
		if( !n-- ) return 0;
		if( c1 != c2 ) return c1 < c2 ? -1 : 1;

	} while( c1 );
	
	// strings are equal
	return 0;
}

char *Q_strstr( const char *string, const char *string2 )
{
	int	c, len;

	if( !string || !string2 ) return NULL;

	c = *string2;
	len = Q_strlen( string2 );

	while( string )
	{
		for( ; *string && *string != c; string++ );

		if( *string )
		{
			if( !Q_strncmp( string, string2, len ))
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
}

char *Q_stristr( const char *string, const char *string2 )
{
	int	c, len;

	if( !string || !string2 ) return NULL;

	c = Q_tolower( *string2 );
	len = Q_strlen( string2 );

	while( string )
	{
		for( ; *string && Q_tolower( *string ) != c; string++ );

		if( *string )
		{
			if( !Q_strnicmp( string, string2, len ))
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
}

int Q_vsnprintf( char *buffer, size_t buffersize, const char *format, va_list args )
{
	int result = _vsnprintf( buffer, buffersize, format, args );
	if( result < 0 || result >= buffersize )
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}
	return result;
}

int Q_snprintf( char *buffer, size_t buffersize, const char *format, ... )
{
	va_list	args;
	int	result;

	va_start( args, format );
	result = Q_vsnprintf( buffer, buffersize, format, args );
	va_end( args );

	return result;
}

int Q_sprintf( char *buffer, const char *format, ... )
{
	va_list	args;
	int	result;

	va_start( args, format );
	result = Q_vsnprintf( buffer, 99999, format, args );
	va_end( args );

	return result;
}

/*
============
va

does a varargs printf into a temp buffer,
so I don't need to have varargs versions
of all text functions.
============
*/
char *va( const char *format, ... )
{
	va_list		argptr;
	static char	string[64][1024], *s;
	static int	stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 63;
	va_start( argptr, format );
	Q_vsnprintf( s, sizeof( string[0] ), format, argptr );
	va_end( argptr );

	return s;
}

char *Q_pretifymem( float value, int digitsafterdecimal )
{
	static char	output[8][32];
	static int	current;
	float		onekb = 1024.0f;
	float		onemb = onekb * onekb;
	char		suffix[8];
	char		*out = output[current];
	char		val[32], *i, *o, *dot;
	int		pos;

	current = ( current + 1 ) & ( 8 - 1 );

	// first figure out which bin to use
	if( value > onemb )
	{
		value /= onemb;
		Q_sprintf( suffix, " Mb" );
	}
	else if( value > onekb )
	{
		value /= onekb;
		Q_sprintf( suffix, " Kb" );
	}
	else Q_sprintf( suffix, " bytes" );

	// clamp to >= 0
	digitsafterdecimal = Q_max( digitsafterdecimal, 0 );

	// if it's basically integral, don't do any decimals
	if( fabs( value - (int)value ) < 0.00001 )
	{
		Q_sprintf( val, "%i%s", (int)value, suffix );
	}
	else
	{
		char fmt[32];

		// otherwise, create a format string for the decimals
		Q_sprintf( fmt, "%%.%if%s", digitsafterdecimal, suffix );
		Q_sprintf( val, fmt, value );
	}

	// copy from in to out
	i = val;
	o = out;

	// search for decimal or if it was integral, find the space after the raw number
	dot = Q_strstr( i, "." );
	if( !dot ) dot = Q_strstr( i, " " );

	pos = dot - i;	// compute position of dot
	pos -= 3;		// don't put a comma if it's <= 3 long

	while( *i )
	{
		// if pos is still valid then insert a comma every third digit, except if we would be
		// putting one in the first spot
		if( pos >= 0 && !( pos % 3 ))
		{
			// never in first spot
			if( o != out ) *o++ = ',';
		}

		pos--;		// count down comma position
		*o++ = *i++;	// copy rest of data as normal
	}
	*o = 0; // terminate

	return out;
}

void _Q_timestring( int seconds, char *msg, size_t size )
{
	int	nMin = seconds / 60;
	int	nSec = seconds - nMin * 60;
	int	nHour = nMin / 60;
	char	*ext[2] = { "", "s" };

	nMin -= nHour * 60;
	
	if( nHour > 0 ) 
		Q_snprintf( msg, size, "%d hour%s, %d minute%s, %d second%s", nHour, ext[nHour != 1], nMin, ext[nMin != 1], nSec, ext[nSec != 1] );
	else if ( nMin > 0 )
		Q_snprintf( msg, size, "%d minute%s, %d second%s", nMin, ext[nMin != 1], nSec, ext[nSec != 1] );
	else Q_snprintf( msg, size, "%d second%s", nSec, ext[nSec != 1] );
}

const char *UTIL_FileExtension( const char *in )
{
	const char *separator, *backslash, *colon, *dot;

	separator = Q_strrchr( in, '/' );
	backslash = Q_strrchr( in, '\\' );
	if( !separator || separator < backslash )
		separator = backslash;
	colon = Q_strrchr( in, ':' );
	if( !separator || separator < colon )
		separator = colon;
	dot = Q_strrchr( in, '.' );
	if( dot == NULL || (separator && ( dot < separator )))
		return "";
	return dot + 1;
}

bool UTIL_ValidMovieFileExtension( const char *filepath )
{
	const char *ext = UTIL_FileExtension(filepath);
	return (Q_stricmp(ext, "avi") == 0 || Q_stricmp(ext, "webm") == 0 || Q_stricmp(ext, "mp4") == 0);
}
