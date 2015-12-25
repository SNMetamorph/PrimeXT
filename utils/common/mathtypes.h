#ifndef MATHTYPES_H__
#define MATHTYPES_H__
#include "cmdlib.h" //--vluzacn

#if _MSC_VER >= 1000
#pragma once
#endif

typedef unsigned char byte;

#ifdef DOUBLEVEC_T
typedef double vec_t;
#else
typedef float vec_t;
#endif
typedef vec_t   vec3_t[3];                                 // x,y,z

#endif //MATHTYPES_H__

