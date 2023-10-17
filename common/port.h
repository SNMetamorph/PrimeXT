/*
port.h -- Portability Layer for Windows types
Copyright (C) 2015 Alibek Omarov

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#ifndef PORT_H
#define PORT_H

#include "build.h"

#if defined __i386__ &&  defined __GNUC__
#define GAME_EXPORT __attribute__((force_align_arg_pointer))
#else
#define GAME_EXPORT
#endif

#if !XASH_WIN32
	#if XASH_APPLE
		#include <sys/syslimits.h>
		#define OS_LIB_EXT "dylib"
		#define OPEN_COMMAND "open"
	#else
		#define OS_LIB_EXT "so"
		#define OPEN_COMMAND "xdg-open"
	#endif
	#define OS_LIB_PREFIX "lib"

	// Windows-specific
	#define _cdecl
	#define __cdecl
	#define __stdcall
	#define _inline	inline
	#define __single_inheritance
	#define RESTRICT
	#define FORCEINLINE inline __attribute__((always_inline))
	#define _forceinline FORCEINLINE
	#define FALSE false
	#define TRUE true
	#define _alloca alloca
	typedef int BOOL;
	typedef unsigned long ULONG;
	typedef unsigned char BYTE;

	#if XASH_POSIX
		#include <unistd.h>
		#include <dlfcn.h>
		#include <limits.h>

		#define PATH_SPLITTER "/"
		#define HAVE_DUP
		#define MAX_PATH PATH_MAX

		#define O_BINARY    0
		#define O_TEXT      0
		#define _mkdir( x ) mkdir( x, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH )
		#define LoadLibrary( x ) dlopen( x, RTLD_NOW )
		#define GetProcAddress( x, y ) dlsym( x, y )
		#define FreeLibrary( x ) dlclose( x )
	#elif XASH_DOS4GW
		#define PATH_SPLITTER "\\"
		#define LoadLibrary( x ) (0)
		#define GetProcAddress( x, y ) (0)
		#define FreeLibrary( x ) (0)
	#endif

	typedef void* HANDLE;
	typedef void* HMODULE;
	typedef void* HINSTANCE;

	typedef char* LPSTR;

	typedef struct tagPOINT
	{
		int x, y;
	} POINT;
#else // WIN32
	#define PATH_SPLITTER "\\"
	#ifdef __MINGW32__
		#define _inline static inline
		#define FORCEINLINE inline __attribute__((always_inline))
	#else
		#define FORCEINLINE __forceinline
	#endif

	#define alloca _alloca
	#define lseek _lseek
	#define WIN32_LEAN_AND_MEAN
	#define NOWINRES
	#define NOSERVICE
	#define NOMCX
	#define NOIME
	#define NOMINMAX
	#include <windows.h>
	#include <io.h>

	#define OS_LIB_PREFIX ""
	#define OS_LIB_EXT "dll"
	#define HAVE_DUP
#endif //WIN32

#ifndef XASH_LOW_MEMORY
#define XASH_LOW_MEMORY 0
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#if !defined MAX_PATH
#define MAX_PATH 4096 // 4k ought to be enough for anybody
#endif

#if __cplusplus >= 2011L
#define COMPILE_TIME_ASSERT static_assert
#endif

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#endif // PORT_H
