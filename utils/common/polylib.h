/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/


typedef struct
{
	int	numpoints;
	vec3_t	p[4];		// variable sized
} winding_t;

#define WindingSize( w )		(( w ) ? ((int)((winding_t *)0)->p[(w)->numpoints]) : 0)
#define MAX_POINTS_ON_WINDING		128
#define EDGE_LENGTH			0.2

winding_t	*AllocWinding( int points );
winding_t	*CopyWinding( const winding_t *w );
winding_t *CopyWinding( int numpoints, vec3_t *points );
void ReverseWinding( winding_t **inout );
void FreeWinding( winding_t *w );
vec_t WindingArea( const winding_t *w );
void WindingCenter( winding_t *w, vec3_t center );
vec_t WindingAreaAndBalancePoint( winding_t *w, vec3_t center );
bool ChopWindingInPlace( winding_t **inout, const vec3_t normal, vec_t dist, vec_t epsilon, bool keepon = true );
winding_t *TryMergeWindingEpsilon( winding_t *w1, winding_t *w2, const vec3_t planenormal, vec_t epsilon );
void ClipWindingEpsilon( winding_t *in, const vec3_t normal, vec_t dist, vec_t epsilon, winding_t **front, winding_t **back, bool keepon = false );
void DivideWindingEpsilon( winding_t *in, vec3_t normal, vec_t dist, vec_t eps, winding_t **fw, winding_t **bw, vec_t *fnorm = 0, bool keepon = true );
bool BiteWindingEpsilon( winding_t **in1, winding_t **in2, const vec3_t planenormal, vec_t epsilon );
bool PointInWindingEpsilon( const winding_t *w, const vec3_t planenormal, const vec3_t point, vec_t epsilon = 0.0, bool noedge = true );
vec_t WindingSnapPointEpsilon( const winding_t *w, const vec3_t planenormal, vec3_t point, vec_t width, vec_t maxmove );
void WindingSnapPoint( const winding_t *w, const vec3_t planenormal, vec3_t point );
winding_t	*BaseWindingForPlane( const vec3_t normal, vec_t dist );
void CheckWindingEpsilon( winding_t *w, vec_t epsilon, bool show_warnings );
void WindingPlane (winding_t *w, vec3_t normal, vec_t *dist );
void RemoveColinearPointsEpsilon( winding_t *w, vec_t epsilon );
int WindingOnPlaneSide( winding_t *w, const vec3_t normal, vec_t dist, vec_t epsilon );
void WindingBounds( winding_t *w, vec3_t mins, vec3_t maxs, bool merge = false );
void pw( winding_t *w );
