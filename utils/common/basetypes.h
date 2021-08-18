/*
basetypes.h - aliases for some base types for convenience
Copyright (C) 2012 Uncle Mike

This file is part of XashNT source code.

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

//#define HLFX_BUILD

#ifdef _WIN32
#pragma warning( disable : 4244 )	// MIPS
#pragma warning( disable : 4018 )	// signed/unsigned mismatch
#pragma warning( disable : 4305 )	// truncation from const double to float
#endif

typedef unsigned char	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned int	uint;
typedef unsigned long	ulong;

typedef unsigned char	uint8;
typedef signed char		int8;
typedef __int16		int16;
typedef unsigned __int16	uint16;
typedef __int32		int32;
typedef unsigned __int32	uint32;
typedef __int64		int64;
typedef unsigned __int64	uint64;

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
#define NO_RETURN __declspec( noreturn )
#define ALIGN16   __declspec(align(16))

// a simple string implementation
#define MAX_STRING		256
typedef char		string[MAX_STRING];

// !!! if this is changed, it must be changed in alert.h too !!!
enum
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_ERROR,		// "-dev 2", shows critical warnings 
	D_WARN,		// "-dev 3", shows not critical system warnings
	D_REPORT,		// "-dev 4", show system reports for advanced users and engine developers
	D_NOTE		// "-dev 5", show system notifications for engine developers
};

#define DXT_ENCODE_DEFAULT		0	// don't use custom encoders
#define DXT_ENCODE_COLOR_YCoCg	0x1A01	// make sure that value dosn't collide with anything
#define DXT_ENCODE_ALPHA_1BIT		0x1A02	// normal 1-bit alpha
#define DXT_ENCODE_ALPHA_8BIT		0x1A03	// normal 8-bit alpha
#define DXT_ENCODE_ALPHA_SDF		0x1A04	// signed distance field
#define DXT_ENCODE_NORMAL_AG_PARABOLOID	0x1A07	// paraboloid projection

#endif//BASETYPES_H