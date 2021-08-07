#pragma once
#ifndef _MSC_VER
#include <linux/limits.h>
#define _forceinline inline __attribute__((always_inline))
#define __cdecl
#define _cdecl
#define __single_inheritance
#define RESTRICT
#define _inline inline
typedef int BOOL;
#define FALSE 0
#define TRUE (!FALSE)
#define _alloca alloca
#define MAX_PATH PATH_MAX
#else
#define NOMINMAX
#include <Windows.h>
#endif
