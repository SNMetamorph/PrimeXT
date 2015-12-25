#ifndef HLASSERT_H__
#define HLASSERT_H__
#include "cmdlib.h" //--vluzacn

#if _MSC_VER >= 1000
#pragma once
#endif

#ifdef SYSTEM_WIN32
#ifdef _DEBUG

#include "log.h"

#define assume(exp, message) {if (!(exp)) {Log("\n***** ERROR *****\nAssume '%s' failed\n at %s:%d\n %s\n\n", #exp, __FILE__, __LINE__, message);  __asm{int 3} }}
#define hlassert(exp) assume(exp, "")

#else // _DEBUG

#define assume(exp, message) {if (!(exp)) {Error("\nAssume '%s' failed\n at %s:%d\n %s\n\n", #exp, __FILE__, __LINE__, message);}}
#define hlassert(exp)

#endif // _DEBUG
#endif // SYSTEM_WIN32

#ifdef SYSTEM_POSIX
#ifdef _DEBUG

#include "log.h"

#define assume(exp, message) {if (!(exp)) {Log("\n***** ERROR *****\nAssume '%s' failed\n at %s:%d\n %s\n\n", #exp, __FILE__, __LINE__, message);  exit(-1); }}
#define hlassert(exp) assume(exp, "")

#else // _DEBUG

#define assume(exp, message) {if (!(exp)) {Error("\nAssume '%s' failed\n at %s:%d\n %s\n\n", #exp, __FILE__, __LINE__, message);}} //#define assume(exp, message) {if (!(exp)) {Error("\nAssume '%s' failed\n\n", #exp, __FILE__, __LINE__, message);}} //--vluzacn
#define hlassert(exp)

#endif // _DEBUG
#endif // SYSTEM_POSIX

#endif // SYSTEM_POSIX HLASSERT_H__

