// Copyright (C) 2000  Sean Cavanaugh
// This file is licensed under the terms of the Lesser GNU Public License
// (see LPGL.txt, or http://www.gnu.org/copyleft/lesser.txt)

#if 0 // linux fix --vluzacn
// AJM GNU
#ifdef __GNUC__ 
#define __int64 long long
#endif
#endif

#ifndef BASICTYPES_H__
#define BASICTYPES_H__

#if _MSC_VER > 1000
#pragma once
#endif /* _MSC_VER > 1000 */

#if defined(_WIN32) || defined(SYSTEM_WIN32)

#undef CHAR
#undef BYTE
#undef INT
#undef UINT
#undef INT8
#undef UINT8
#undef INT16
#undef UINT16
#undef INT32
#undef UINT32
#undef INT64
#undef UINT64
typedef char             CHAR;
typedef unsigned char    BYTE;
typedef signed int       INT;
typedef unsigned int     UINT;
typedef signed char      INT8;
typedef unsigned char    UINT8;
typedef signed short     INT16;
typedef unsigned short   UINT16;
typedef signed int       INT32;
typedef unsigned int     UINT32;
typedef signed __int64   INT64;
typedef unsigned __int64 UINT64;

#endif /* SYSTEM_WIN32 */

#ifdef SYSTEM_POSIX

#undef CHAR
#undef BYTE
#undef INT
#undef UINT
#undef INT8
#undef UINT8
#undef INT16
#undef UINT16
#undef INT32
#undef UINT32
#undef INT64
#undef UINT64
typedef char            CHAR;
typedef unsigned char   BYTE;
typedef signed int      INT;
typedef unsigned int    UINT;
typedef signed char     INT8;
typedef unsigned char   UINT8;
typedef signed short    INT16;
typedef unsigned short  UINT16;
typedef signed int      INT32;
typedef unsigned int    UINT32;
/* typedef __int64       INT64; */
/* typedef unsigned __int64 UINT64; */

#endif /* SYSTEM_POSIX */

#endif /* BASICTYPES_H__ */

