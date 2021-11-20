#pragma once
#include <math.h>

// NOTE: PhysX mathlib is conflicted with standard min\max
#define Q_min( a, b )		(((a) < (b)) ? (a) : (b))
#define Q_max( a, b )		(((a) > (b)) ? (a) : (b))
#define Q_recip( a )		((float)(1.0f / (float)(a)))
#define Q_floor( a )		((float)(long)(a))
#define Q_ceil( a )			((float)(long)((a) + 1))
#define Q_abs( a )			(fabsf((a)))
#define Q_round( x, y )		(floor( x / y + 0.5 ) * y )
#define Q_square( a )		((a) * (a))
#define Q_sign( x )			( x >= 0 ? 1.0 : -1.0 )
