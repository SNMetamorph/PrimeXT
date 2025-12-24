/*
basetypes.h - aliases for some base types for convenience
Copyright (C) 2012 Uncle Mike
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef BASETYPES_H
#define BASETYPES_H

#include "build.h"
#include <stdint.h>
#include <cstddef>

#if XASH_MSVC
#pragma warning( disable : 4244 )	// MIPS
#pragma warning( disable : 4018 )	// signed/unsigned mismatch
#pragma warning( disable : 4305 )	// truncation from const double to float
#endif

typedef uint8_t		byte;
typedef uint16_t	word;
typedef uint32_t	dword;
typedef uint32_t	uint;

typedef uint8_t 	uint8;
typedef int8_t 		int8;
typedef int16_t 	int16;
typedef uint16_t 	uint16;
typedef int32_t 	int32;
typedef uint32_t 	uint32;
typedef int64_t 	int64;
typedef uint64_t 	uint64;

typedef uint32_t    poolhandle_t;
typedef int64		longtime_t;

#undef true
#undef false

#ifndef __cplusplus
typedef enum { false, true }	qboolean;
#else 
typedef int qboolean;
#endif

// We need to inform the compiler that Host_Error() and Sys_Error() will
// never return, so any conditions that leeds to them being called are
// guaranteed to be false in the following code
#define NO_RETURN [[noreturn]]
#define ALIGN16   alignas(16)

// a simple string implementation
#define MAX_STRING	256
typedef char		string[MAX_STRING];

// !!! if this is changed, it must be changed in alert.h too !!!
enum
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_ERROR,	// "-dev 2", shows critical warnings 
	D_WARN,		// "-dev 3", shows not critical system warnings
	D_REPORT,	// "-dev 4", show system reports for advanced users and engine developers
	D_NOTE		// "-dev 5", show system notifications for engine developers
};

#define DXT_ENCODE_DEFAULT		0	// don't use custom encoders
#define DXT_ENCODE_COLOR_YCoCg	0x1A01	// make sure that value dosn't collide with anything
#define DXT_ENCODE_ALPHA_1BIT		0x1A02	// normal 1-bit alpha
#define DXT_ENCODE_ALPHA_8BIT		0x1A03	// normal 8-bit alpha
#define DXT_ENCODE_ALPHA_SDF		0x1A04	// signed distance field
#define DXT_ENCODE_NORMAL_AG_PARABOLOID	0x1A07	// paraboloid projection

#define Swap32( x ) (((uint32_t)((( x ) & 255 ) << 24 )) + ((uint32_t)(((( x ) >> 8 ) & 255 ) << 16 )) + ((uint32_t)((( x ) >> 16 ) & 255 ) << 8 ) + ((( x ) >> 24 ) & 255 ))
#define Swap16( x ) ((uint16_t)((((uint16_t)( x ) >> 8 ) & 255 ) + (((uint16_t)( x ) & 255 ) << 8 )))
#define Swap32Store( x ) ( x = Swap32( x ))
#define Swap16Store( x ) ( x = Swap16( x ))

#ifdef XASH_BIG_ENDIAN
	#define LittleLong( x )    Swap32( x )
	#define LittleShort( x )   Swap16( x )
	#define LittleLongSW( x )  Swap32Store( x )
	#define LittleShortSW( x ) Swap16Store( x )
	#define LittleFloat( x )   SwapFloat( x )
	#define BigLong( x )  ( x )
	#define BigShort( x ) ( x )
	#define BigFloat( x ) ( x )
#else
	#define LittleLong( x )  ( x )
	#define LittleShort( x )  ( x )
	#define LittleFloat( x )  ( x )
	#define LittleLongSW( x )
	#define LittleShortSW( x )
	#define BigLong( x )  Swap32( x )
	#define BigShort( x ) Swap16( x )
	#define BigFloat( x ) SwapFloat( x )
#endif

#endif // BASETYPES_H
