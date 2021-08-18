/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// winding.c

#include "qvis.h"

void pw( winding_t *w )
{
	for( int i = 0; w && i < w->numpoints; i++ )
		Msg( "(%5.1f, %5.1f, %5.1f)\n", w->p[i][0], w->p[i][1], w->p[i][2] );
}

/*
=============
AllocWinding
=============
*/
winding_t	*AllocWinding( int numpoints )
{
	if( numpoints > MAX_POINTS_ON_WINDING )
		COM_FatalError( "AllocWinding: MAX_POINTS_ON_WINDING limit exceeded\n" );

	return (winding_t *)Mem_Alloc( (int)((winding_t *)0)->p[numpoints], C_WINDING );
}

/*
=============
FreeWinding
=============
*/
void FreeWinding( winding_t *w )
{
	// simple sentinel by Carmack
	if( *(unsigned *)w == 0xDEADC0DE )
		COM_FatalError( "FreeWinding: freed a freed winding\n" );
	*(unsigned *)w = 0xDEADC0DE;

	Mem_Free( w, C_WINDING );
}

/*
=============
AllocStackWinding
=============
*/
static winding_t *AllocStackWinding( pstack_t *stack )
{
	for( int i = 0; i < 3; i++ )
	{
		if( stack->freewindings[i] )
		{
			stack->freewindings[i] = 0;
			return &stack->windings[i];
		}
	}

	COM_FatalError( "AllocStackWinding: failed\n" );

	return NULL;
}

/*
=============
FreeStackWinding
=============
*/
static void FreeStackWinding( winding_t *w, pstack_t *stack )
{
	int	i = w - stack->windings;

	if( i < 0 || i > 2 )
		return; // not from local

	if( stack->freewindings[i] )
		COM_FatalError( "FreeStackWinding: allready free\n" );
	stack->freewindings[i] = 1;
}

/*
============
WindingPlane
============
*/
void WindingPlane( winding_t *w, vec3_t normal, vec_t *dist )
{
	vec3_t	v1, v2;

	if( w && w->numpoints >= 3 )
	{
		VectorSubtract( w->p[2], w->p[1], v1 );
		VectorSubtract( w->p[0], w->p[1], v2 );
		CrossProduct( v2, v1, normal );
		VectorNormalize( normal );
		*dist = DotProduct( w->p[0], normal );
	}
	else
	{
		VectorClear( normal );
		*dist = 0.0;
	}
}

/*
==============
ChopWinding

==============
*/
winding_t	*ChopWindingEpsilon( winding_t *in, pstack_t *stack, plane_t *split, vec_t epsilon )
{
	vec_t	dists[MAX_POINTS_ON_WINDING+4];
	int	sides[MAX_POINTS_ON_WINDING+4];
	int	counts[3];
	vec_t	dot;
	int	i, j;
	vec_t	*p1, *p2;
	vec3_t	mid;
	winding_t	*neww;

	counts[0] = counts[1] = counts[2] = 0;

	if( in->numpoints > MAX_POINTS_ON_WINDING )
		COM_FatalError( "ChopWinding: MAX_POINTS_ON_WINDING limit exceeded\n" );

	// determine sides for each point
	for( i = 0; i < in->numpoints; i++ )
	{
		dot = DotProduct( in->p[i], split->normal );
		dot -= split->dist;
		dists[i] = dot;

		if( dot > epsilon )
			sides[i] = SIDE_FRONT;
		else if( dot < -epsilon )
			sides[i] = SIDE_BACK;
		else sides[i] = SIDE_ON;

		counts[sides[i]]++;
	}

	if( counts[SIDE_ON] == in->numpoints )
		return in;

	if( !counts[1] )
		return in; // completely on front side
	
	if( !counts[0] )
	{
		FreeStackWinding( in, stack );
		return NULL;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	
	neww = AllocStackWinding( stack );
	neww->numpoints = 0;

	for( i = 0; i < in->numpoints; i++ )
	{
		p1 = in->p[i];

		if( neww->numpoints == MAX_POINTS_ON_STACK_WINDING )
		{
			FreeStackWinding( neww, stack );
			return in; // can't chop -- fall back to original
		}

		if( sides[i] == SIDE_ON )
		{
			VectorCopy( p1, neww->p[neww->numpoints] );
			neww->numpoints++;
			continue;
		}
	
		if( sides[i] == SIDE_FRONT )
		{
			VectorCopy( p1, neww->p[neww->numpoints] );
			neww->numpoints++;
		}
		
		if(( sides[i+1] == SIDE_ON ) | ( sides[i+1] == sides[i] ))  // | instead of || for branch optimization
			continue;
			
		if( neww->numpoints == MAX_POINTS_ON_STACK_WINDING )
		{
			FreeStackWinding( neww, stack );
			return in; // can't chop -- fall back to original
		}

		// generate a split point
		p2 = in->p[(i + 1) % in->numpoints];
		
		dot = dists[i] / (dists[i] - dists[i+1]);

		for( j = 0; j < 3; j++ )
		{	
			// avoid round off error when possible
			if( split->normal[j] == 1 )
				mid[j] = split->dist;
			else if( split->normal[j] == -1 )
				mid[j] = -split->dist;
			else mid[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}
			
		VectorCopy( mid, neww->p[neww->numpoints] );
		neww->numpoints++;
	}
	
	// free the original winding
	FreeStackWinding( in, stack );
	
	return neww;
}