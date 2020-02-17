/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef EXTDLL_H
#define EXTDLL_H

#include "port.h"
//
// Global header file for extension DLLs
//

// Allow "DEBUG" in addition to default "_DEBUG"
#ifdef _DEBUG
#define DEBUG 1
#endif

// Silence certain warnings
#pragma warning(disable : 4305)		// int or float data truncation
#pragma warning(disable : 4201)		// nameless struct/union
#pragma warning(disable : 4514)		// unreferenced inline function removed
#pragma warning(disable : 4100)		// unreferenced formal parameter

// Prevent tons of unused windows definitions
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOMINMAX
#include "windows.h"
#else // _WIN32
#define FALSE 0
#define TRUE (!FALSE)
typedef unsigned long ULONG;
typedef unsigned char BYTE;
typedef int BOOL;
#define MAX_PATH PATH_MAX
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#ifndef Q_min
#define Q_min(a,b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef Q_max
#define Q_max(a,b)  (((a) > (b)) ? (a) : (b))
//#define _vsnprintf(a,b,c,d) vsnprintf(a,b,c,d)
#endif
#endif //_WIN32

// Misc C-runtime library headers
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mathlib.h>

// Header file containing definition of globalvars_t and entvars_t
typedef int	string_t;		// from engine's pr_comp.h;

//typedef HMODULE	dllhandle_t;
typedef void* dllhandle_t;

typedef struct dllfunc_s
{
	const char *name;
	void	**func;
} dllfunc_t;

// Vector class
#include "vector.h"

// Matrix class
#include "matrix.h"

// Shared engine/DLL constants
#include "const.h"
#include "progdefs.h"
#include "edict.h"

// Shared header describing protocol between engine and DLLs
#include "eiface.h"

// Shared header between the client DLL and the game DLLs
#include "cdll_dll.h"

#endif //EXTDLL_H

