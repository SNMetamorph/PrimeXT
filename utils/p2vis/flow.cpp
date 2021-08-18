/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "qvis.h"
#include "threads.h"

int	c_chains;
int	c_portalskip, c_leafskip;
int	c_vistest, c_mighttest;
int	c_mightseeupdate;
int	active;

#ifdef _DEBUG
void CheckStack( leaf_t *leaf, threaddata_t *thread )
{
	pstack_t	*p, *p2;

	for( p = thread->pstack_head.next; p != NULL; p = p->next )
	{
		if( p->leaf == leaf )
			COM_FatalError( "CheckStack: leaf recursion\n" );

		for( p2 = thread->pstack_head.next; p2 != p; p2 = p2->next )
		{
			if( p2->leaf == p->leaf )
				COM_FatalError( "CheckStack: late leaf recursion\n" );
		}
	}
}
#endif

/*
==============
ClipToSeperators

Source, pass, and target are an ordering of portals.

Generates seperating planes canidates by taking two points from source and one
point from pass, and clips target by them.

If target is totally clipped away, that portal can not be seen through.

Normal clip keeps target on the same side as pass, which is correct if the
order goes source, pass, target.  If the order goes pass, source, target then
flipclip should be set.
==============
*/
static winding_t *ClipToSeperators( winding_t *source, winding_t *pass, winding_t *target, bool flipclip, pstack_t *stack )
{
	int	i, j, k, l;
	int	counts[3];
	bool	fliptest;
	vec3_t	v1, v2;
	plane_t	plane;
	vec_t	d;

	// check all combinations	
	for( i = 0; i < source->numpoints; i++ )
	{
		l = (i + 1) % source->numpoints;
		VectorSubtract( source->p[l], source->p[i], v1 );

		// fing a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for( j = 0; j < pass->numpoints; j++ )
		{
			VectorSubtract( pass->p[j], source->p[i], v2 );
			CrossProduct( v1, v2, plane.normal );

			if( VectorNormalize( plane.normal ) < VIS_EPSILON )
				continue;

			plane.dist = DotProduct( pass->p[j], plane.normal );
			fliptest = false;

			// find out which side of the generated seperating plane
			// has the source portal
			for( k = 0; k < source->numpoints; k++ )
			{
				if(( k == i ) | ( k == l )) // | instead of || for branch optimization
					continue;

				d = DotProduct( source->p[k], plane.normal ) - plane.dist;

				if( d < -VIS_EPSILON )
				{	
					// source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false;
					break;
				}
				else if( d > VIS_EPSILON )
				{	
					// source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}

			if( k == source->numpoints )
				continue;	// planar with source portal

			if( fliptest )
			{
				// flip the normal if the source portal is backwards
				VectorNegate( plane.normal, plane.normal );
				plane.dist = -plane.dist;
			}

			// if all of the pass portal points are now on the positive side,
			// this is the seperating plane
			counts[0] = counts[1] = counts[2] = 0;
			for( k = 0; k < pass->numpoints; k++ )
			{
				if( k == j ) continue;

				d = DotProduct( pass->p[k], plane.normal ) - plane.dist;

				if( d < -VIS_EPSILON )
					break;
				else if( d > VIS_EPSILON )
					counts[0]++;
				else counts[2]++;
			}

			if( k != pass->numpoints )
				continue; // points on negative side, not a seperating plane

			if( !counts[0] )
				continue;	// planar with seperating plane

			if( flipclip )
			{
				// flip the normal if we want the back side
				VectorNegate( plane.normal, plane.normal );
				plane.dist = -plane.dist;
			}

			stack->seperators[flipclip][stack->numseperators[flipclip]] = plane;

			if( ++stack->numseperators[flipclip] >= MAX_SEPERATORS )
				COM_FatalError( "MAX_SEPERATORS on stack\n" );

			// fast check first
			d = DotProduct( stack->portal->origin, plane.normal ) - plane.dist;

			// if completely at the back of the seperator plane
			if( d < -stack->portal->radius )
				return NULL;

			// if completely on the front of the seperator plane
			if( d > stack->portal->radius )
				break;

			// clip target by the seperating plane
			target = ChopWindingEpsilon( target, stack, &plane, VIS_EPSILON );
			if( !target ) return NULL; // target is not visible

			break; // optimization by Antony Suter
		}
	}
	
	return target;
}

/*
==================
RecursiveLeafFlow

Flood fill through the leafs
If src_portal is NULL, this is the originating leaf
==================
*/
inline static void RecursiveLeafFlow( int leafnum, threaddata_t *thread, pstack_t *prevstack )
{
	pstack_t	stack;
	plane_t	backplane;
	long	*test, *might;
	long	more, *vis;
	leaf_t 	*leaf;
	portal_t	*p;
	vec_t	d;

	leaf = &g_leafs[leafnum];
	c_chains++;

#ifdef _DEBUG
	CheckStack( leaf, thread );
#endif	
	// mark the leaf as visible
	if( !CHECKVISBIT( thread->leafvis, leafnum ))
	{
		SETVISBIT( thread->leafvis, leafnum );
		thread->base->numcansee++;
	}
	
	prevstack->next = &stack;
	stack.head = prevstack->head;
	stack.numseperators[0] = 0;
	stack.numseperators[1] = 0;
	stack.next = NULL;
	stack.leaf = leaf;
	stack.portal = NULL;

	might = (long *)stack.mightsee;
	vis = (long *)thread->leafvis;
	
	// check all portals for flowing into other leafs	
	for( int i = 0; i < leaf->numportals; i++ )
	{
		p = leaf->portals[i];

		if( !CHECKVISBIT( stack.head->mightsee, p->leaf ))
		{
			c_leafskip++;
			continue;	// can't possibly see it
		}

		if( !CHECKVISBIT( prevstack->mightsee, p->leaf ))
		{
			c_leafskip++;
			continue;	// can't possibly see it
		}

		// if the portal can't see anything we haven't allready seen, skip it
		if( p->status == stat_done )
		{
			test = (long *)p->visbits;
			c_vistest++;
		}
		else
		{
			test = (long *)p->mightsee;
			c_mighttest++;
		}

		more = 0;

		for( int j = 0; j < g_bitlongs; j++ )
		{
			might[j] = ((long *)prevstack->mightsee)[j] & test[j];
			more |= (might[j] & ~vis[j]);
		}
		
		if( !more )
		{	
			// can't see anything new
			c_portalskip++;
			continue;
		}

		// get plane of portal, point normal into the neighbor leaf
		stack.portalplane = p->plane;
		VectorNegate( p->plane.normal, backplane.normal );
		backplane.dist = -p->plane.dist;
			
		if( VectorCompareEpsilon( prevstack->portalplane.normal, backplane.normal, EQUAL_EPSILON ))
			continue;	// can't go out a coplanar face
	
		c_portalcheck++;
		
		stack.portal = p;
		stack.next = NULL;

		stack.freewindings[0] = 1;
		stack.freewindings[1] = 1;
		stack.freewindings[2] = 1;

		d = DotProduct( p->origin, thread->pstack_head.portalplane.normal );
		d -= thread->pstack_head.portalplane.dist;

		if( d < -p->radius )
		{
			continue;
		}
		else if( d > p->radius )
		{
			stack.pass = p->winding;
		}
		else	
		{
			stack.pass = ChopWindingEpsilon( p->winding, &stack, &thread->pstack_head.portalplane, VIS_EPSILON );
			if( !stack.pass ) continue;
		}

		d = DotProduct( thread->base->origin, p->plane.normal );
		d -= p->plane.dist;

		if( d > thread->base->radius )
		{
			continue;
		}
		else if( d < -thread->base->radius )
		{
			stack.source = prevstack->source;
		}
		else	
		{
			stack.source = ChopWindingEpsilon( prevstack->source, &stack, &backplane, VIS_EPSILON );
			// FIXME: shouldn't we create a new source origin and radius for fast checks?
			if( !stack.source ) continue;
		}

		if( !prevstack->pass )
		{	
			// the second leaf can only be blocked if coplanar
			RecursiveLeafFlow( p->leaf, thread, &stack );
			continue;
		}

		stack.pass = ChopWindingEpsilon( stack.pass, &stack, &prevstack->portalplane, VIS_EPSILON );
		if( !stack.pass ) continue;
		
		c_portaltest++;

		if( stack.numseperators[0] )
		{
			for( int n = 0; n < stack.numseperators[0]; n++ )
			{
				stack.pass = ChopWindingEpsilon( stack.pass, &stack, &stack.seperators[0][n], VIS_EPSILON );
				if( !stack.pass ) break; // target is not visible
			}

			if( n < stack.numseperators[0] )
				continue;
		}
		else stack.pass = ClipToSeperators( stack.source, prevstack->pass, stack.pass, false, &stack );
		if( !stack.pass ) continue;

		if( stack.numseperators[1] )
		{
			for( int n = 0; n < stack.numseperators[1]; n++ )
			{
				stack.pass = ChopWindingEpsilon( stack.pass, &stack, &stack.seperators[1][n], VIS_EPSILON );
				if( !stack.pass ) break; // target is not visible
			}
		}
		else stack.pass = ClipToSeperators( prevstack->pass, stack.source, stack.pass, true, &stack );
		if( !stack.pass ) continue;

		c_portalpass++;

		// flow through it for real
		RecursiveLeafFlow( p->leaf, thread, &stack );
		stack.next = NULL;
	}	
}

/*
=============
UpdateMightSee

Called after completing a portal and finding that the source leaf is no
longer visible from the dest leaf. Visibility is symetrical, so the reverse
must also be true. Update mightsee for any portals on the source leaf which
haven't yet started processing.

Called with the lock held.
=============
*/
static void UpdateMightsee( const leaf_t *source, const leaf_t *dest )
{
	int	leafnum = dest - g_leafs;
	portal_t	*p;

	for( int i = 0; i < source->numportals; i++ )
	{
		p = source->portals[i];

		if( p->status != stat_none )
			continue;

		if( CHECKVISBIT( p->mightsee, leafnum ))
		{
			CLEARVISBIT( p->mightsee, leafnum );
			c_mightseeupdate++;
			p->nummightsee--;
		}
	}
}

/*
=============
PortalCompleted

Mark the portal completed and propogate new vis information across
to the complementry portals.

Called with the lock held.
=============
*/
static void PortalCompleted( portal_t *completed )
{
	long	*might, *vis;
	int	leafnum;
	portal_t	*p, *p2;
	leaf_t	*myleaf;
	long	changed;

	ThreadLock();

	// for each portal on the leaf, check the leafs we eliminated from
	// mightsee during the full vis so far.
	myleaf = &g_leafs[completed->leaf];

	for( int i = 0; i < myleaf->numportals; i++ )
	{
		p = myleaf->portals[i];
		if( p->status != stat_done )
			continue;

		might = (long *)p->mightsee;
		vis = (long *)p->visbits;

		for( int j = 0; j < g_bitlongs; j++ )
		{
			changed = might[j] & ~vis[j];
			if( !changed ) continue;

			// if any of these changed bits are still visible
			// from another portal, we can't update yet.
			for( int k = 0; k < myleaf->numportals; k++ )
			{
				if( k == i ) continue;

				p2 = myleaf->portals[k];
				if( p2->status == stat_done )
					changed &= ~((long *)p2->visbits)[j];
				else changed &= ~((long *)p2->mightsee)[j];
				if( !changed ) break;
			}

			// update mightsee for any of the changed bits that survived
			while( changed )
			{
				int bit = ffsl( changed ) - 1;
				changed &= ~BIT( bit );
				leafnum = (j << 5) + bit;
				UpdateMightsee( g_leafs + leafnum, myleaf );
			}
		}
	}

	ThreadUnlock();
}

/*
===============
PortalFlow

===============
*/
void PortalFlow( int portalnum, int threadnum )
{
	threaddata_t	data;
	portal_t		*p;

	p = g_sorted_portals[portalnum];
	p->status = stat_working;
	
	p->visbits = (byte *)Mem_Alloc( g_bitbytes );
	memset( &data, 0, sizeof( data ));
	data.leafvis = p->visbits;
	data.base = p;

	data.pstack_head.head = &data.pstack_head;	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;

	for( int i = 0; i < g_bitlongs; i++ )
		((long *)data.pstack_head.mightsee)[i] = ((long *)p->mightsee)[i];

	RecursiveLeafFlow( p->leaf, &data, &data.pstack_head );
	p->status = stat_done;
#ifdef HLVIS_MERGE_PORTALS
	PortalCompleted( p );
#endif
}

/*
===============
PortalFlow

===============
*/
void PortalFlow( portal_t *p )
{
	threaddata_t	data;

	if( p->status != stat_working )
		COM_FatalError( "PortalFlow: reflowed\n" );

	p->visbits = (byte *)Mem_Alloc( g_bitbytes );
	memset( &data, 0, sizeof( data ));
	data.leafvis = p->visbits;
	data.base = p;

	data.pstack_head.head = &data.pstack_head;	
	data.pstack_head.portal = p;
	data.pstack_head.source = p->winding;
	data.pstack_head.portalplane = p->plane;

	for( int i = 0; i < g_bitlongs; i++ )
		((long *)data.pstack_head.mightsee)[i] = ((long *)p->mightsee)[i];

	RecursiveLeafFlow( p->leaf, &data, &data.pstack_head );
	p->status = stat_done;
#ifdef HLVIS_MERGE_PORTALS
	PortalCompleted( p );
#endif
}

/*
===============================================================================

This is a rough first-order aproximation that is used to trivially reject some
of the final calculations.

===============================================================================
*/
static void SimpleFlood( portal_t *srcportal, int leafnum, byte *portalsee, int *c_leafsee )
{
	leaf_t	*leaf;
	
	if( CHECKVISBIT( srcportal->mightsee, leafnum ))
		return;

	SETVISBIT( srcportal->mightsee, leafnum );
	(*c_leafsee)++;
	
	leaf = &g_leafs[leafnum];
	
	for( int i = 0; i < leaf->numportals; i++ )
	{
		portal_t	*p = leaf->portals[i];

		if( !portalsee[p - g_portals] )
			continue;

		SimpleFlood( srcportal, p->leaf, portalsee, c_leafsee );
	}
}

/*
==============
BasePortalVis
==============
*/
void BasePortalVis( int threadnum )
{
	byte	portalsee[MAX_MAP_PORTALS*2];
	int	i, j, k, c_leafsee;
	vec3_t	backnormal, dist;
	portal_t	*tp, *p;
	winding_t	*w;
	float	d;
	
	while( 1 )
	{
		if(( i = GetThreadWork()) == -1 )
			break;

		p = &g_portals[i];

		p->mightsee = (byte *)Mem_Alloc( g_bitbytes );
		memset( portalsee, 0, g_numportals * 2 );
		VectorNegate( p->plane.normal, backnormal );

		for( j = 0, tp = g_portals; j < g_numportals * 2; j++, tp++ )
		{
			if( j == i ) continue;

			if( VectorCompare( backnormal, tp->plane.normal ))
				continue;

			if( g_farplane > 0 )
			{
				VectorSubtract( tp->origin, p->origin, dist );
				if( VectorLength( dist ) - tp->radius - p->radius > g_farplane )
					continue;
			}

			w = tp->winding;

			for( k = 0; k < w->numpoints; k++ )
			{
				d = DotProduct( w->p[k], p->plane.normal ) - p->plane.dist;
				if( d > -VIS_EPSILON )
					break;
			}

			if( k == w->numpoints )
				continue;	// no points on front

			w = p->winding;

			for( k = 0; k < w->numpoints; k++ )
			{
				d = DotProduct( w->p[k], tp->plane.normal ) - tp->plane.dist;
				if( d < VIS_EPSILON )
					break;
			}

			if( k == w->numpoints )
				continue;	// no points on back

			portalsee[j] = 1;					
		}

		c_leafsee = 0;
		SimpleFlood( p, p->leaf, portalsee, &c_leafsee );
		p->nummightsee = c_leafsee;
	}
}