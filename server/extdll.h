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
#include "mathlib.h"

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

#if XASH_WIN32 != 1
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#endif

// Misc C-runtime library headers
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mathlib.h>

// Header file containing definition of globalvars_t and entvars_t
typedef int	string_t;		// from engine's pr_comp.h;

#if XASH_WIN32 == 1
typedef HMODULE dllhandle_t;
#else
typedef void* dllhandle_t;
#endif

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

