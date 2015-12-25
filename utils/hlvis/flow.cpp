#include "vis.h"

// =====================================================================================
//  CheckStack
// =====================================================================================
#ifdef USE_CHECK_STACK
static void     CheckStack(const leaf_t* const leaf, const threaddata_t* const thread)
{
    pstack_t*       p;

    for (p = thread->pstack_head.next; p; p = p->next)
    {
        if (p->leaf == leaf)
            Error("CheckStack: leaf recursion");
    }
}
#endif

// =====================================================================================
//  AllocStackWinding
// =====================================================================================
inline static winding_t* AllocStackWinding(pstack_t* const stack)
{
    int             i;

    for (i = 0; i < 3; i++)
    {
        if (stack->freewindings[i])
        {
            stack->freewindings[i] = 0;
            return &stack->windings[i];
        }
    }

    Error("AllocStackWinding: failed");

    return NULL;
}

// =====================================================================================
//  FreeStackWinding
// =====================================================================================
inline static void     FreeStackWinding(const winding_t* const w, pstack_t* const stack)
{
    int             i;

    i = w - stack->windings;

    if (i < 0 || i > 2)
        return;                                            // not from local

    if (stack->freewindings[i])
        Error("FreeStackWinding: allready free");
    stack->freewindings[i] = 1;
}

// =====================================================================================
//  ChopWinding
// =====================================================================================
inline winding_t*      ChopWinding(winding_t* const in, pstack_t* const stack, const plane_t* const split)
{
    vec_t           dists[128];
    int             sides[128];
    int             counts[3];
    vec_t           dot;
    int             i;
    vec3_t          mid;
    winding_t*      neww;

    counts[0] = counts[1] = counts[2] = 0;

    if (in->numpoints > (sizeof(sides) / sizeof(*sides)))
    {
        Error("Winding with too many sides!");
    }

    // determine sides for each point
    for (i = 0; i < in->numpoints; i++)
    {
        dot = DotProduct(in->points[i], split->normal);
        dot -= split->dist;
        dists[i] = dot;
        if (dot > ON_EPSILON)
        {
            sides[i] = SIDE_FRONT;
        }
        else if (dot < -ON_EPSILON)
        {
            sides[i] = SIDE_BACK;
        }
        else
        {
            sides[i] = SIDE_ON;
        }
        counts[sides[i]]++;
    }

    if (!counts[1])
    {
        return in;                                         // completely on front side
    }

    if (!counts[0])
    {
        FreeStackWinding(in, stack);
        return NULL;
    }

    sides[i] = sides[0];
    dists[i] = dists[0];

    neww = AllocStackWinding(stack);

    neww->numpoints = 0;

    for (i = 0; i < in->numpoints; i++)
    {
        vec_t* p1 = in->points[i];

        if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
        {
            Warning("ChopWinding : rejected(1) due to too many points\n");
            FreeStackWinding(neww, stack);
            return in;                                     // can't chop -- fall back to original
        }

        if (sides[i] == SIDE_ON)
        {
            VectorCopy(p1, neww->points[neww->numpoints]);
            neww->numpoints++;
            continue;
        }
        else if (sides[i] == SIDE_FRONT)
        {
            VectorCopy(p1, neww->points[neww->numpoints]);
            neww->numpoints++;
        }

        if ((sides[i + 1] == SIDE_ON) | (sides[i + 1] == sides[i])) // | instead of || for branch optimization
        {
            continue;
        }

        if (neww->numpoints == MAX_POINTS_ON_FIXED_WINDING)
        {
            Warning("ChopWinding : rejected(2) due to too many points\n");
            FreeStackWinding(neww, stack);
            return in;                                     // can't chop -- fall back to original
        }

        // generate a split point
        {
            unsigned tmp = i + 1;
            if (tmp >= in->numpoints)
            {
                tmp = 0;
            }
            const vec_t* p2 = in->points[tmp];

            dot = dists[i] / (dists[i] - dists[i + 1]);

            const vec_t* normal = split->normal;
            const vec_t dist = split->dist;
            unsigned int j;
            for (j = 0; j < 3; j++)
            {                                                  // avoid round off error when possible
                if (normal[j] < (1.0 - NORMAL_EPSILON))
                {
                    if (normal[j] > (-1.0 + NORMAL_EPSILON))
                    {
                        mid[j] = p1[j] + dot * (p2[j] - p1[j]);
                    }
                    else
                    {
                        mid[j] = -dist;
                    }
                }
                else
                {
                    mid[j] = dist;
                }
            }
        }

        VectorCopy(mid, neww->points[neww->numpoints]);
        neww->numpoints++;
    }

    // free the original winding
    FreeStackWinding(in, stack);

    return neww;
}

// =====================================================================================
//  AddPlane
// =====================================================================================
#ifdef RVIS_LEVEL_2
inline static void AddPlane(pstack_t* const stack, const plane_t* const split)
{
    int     j;
    
    if (stack->clipPlaneCount)
    {
        for (j = 0; j < stack->clipPlaneCount; j++)
        {
            if (fabs((stack->clipPlane[j]).dist - split->dist) <= EQUAL_EPSILON &&
                VectorCompare((stack->clipPlane[j]).normal, split->normal))
            {
                return;
            }
        }
    }
    stack->clipPlane[stack->clipPlaneCount] = *split;
    stack->clipPlaneCount++;
}
#endif

// =====================================================================================
//  ClipToSeperators
//      Source, pass, and target are an ordering of portals.
//      Generates seperating planes canidates by taking two points from source and one
//      point from pass, and clips target by them.
//      If the target argument is NULL, then a list of clipping planes is built in
//      stack instead.
//      If target is totally clipped away, that portal can not be seen through.
//      Normal clip keeps target on the same side as pass, which is correct if the
//      order goes source, pass, target.  If the order goes pass, source, target then
//      flipclip should be set.
// =====================================================================================
inline static winding_t* ClipToSeperators(
    const winding_t* const source,
    const winding_t* const pass, 
    winding_t* const a_target,
    const bool flipclip, 
    pstack_t* const stack)
{
    int             i, j, k, l;
    plane_t         plane;
    vec3_t          v1, v2;
    float           d;
    int             counts[3];
    bool            fliptest;
    winding_t*      target = a_target;

    const unsigned int numpoints = source->numpoints;

    // check all combinations       
    for (i=0, l=1; i < numpoints; i++, l++)
    {
        if (l == numpoints)
        {
            l = 0;
        }

        VectorSubtract(source->points[l], source->points[i], v1);

        // fing a vertex of pass that makes a plane that puts all of the
        // vertexes of pass on the front side and all of the vertexes of
        // source on the back side
        for (j = 0; j < pass->numpoints; j++)
        {
            VectorSubtract(pass->points[j], source->points[i], v2);
            CrossProduct(v1, v2, plane.normal);
            if (VectorNormalize(plane.normal) < ON_EPSILON)
            {
                continue;
            }
            plane.dist = DotProduct(pass->points[j], plane.normal);

            // find out which side of the generated seperating plane has the
            // source portal
            fliptest = false;
            for (k = 0; k < numpoints; k++)
            {
                if ((k == i) | (k == l)) // | instead of || for branch optimization
                {
                    continue;
                }
                d = DotProduct(source->points[k], plane.normal) - plane.dist;
                if (d < -ON_EPSILON)
                {                                          // source is on the negative side, so we want all
                    // pass and target on the positive side
                    fliptest = false;
                    break;
                }
                else if (d > ON_EPSILON)
                {                                          // source is on the positive side, so we want all
                    // pass and target on the negative side
                    fliptest = true;
                    break;
                }
            }
            if (k == numpoints)
            {
                continue;                                  // planar with source portal
            }

            // flip the normal if the source portal is backwards
            if (fliptest)
            {
                VectorSubtract(vec3_origin, plane.normal, plane.normal);
                plane.dist = -plane.dist;
            }

            // if all of the pass portal points are now on the positive side,
            // this is the seperating plane
            counts[0] = counts[1] = counts[2] = 0;
            for (k = 0; k < pass->numpoints; k++)
            {
                if (k == j)
                {
                    continue;
                }
                d = DotProduct(pass->points[k], plane.normal) - plane.dist;
                if (d < -ON_EPSILON)
                {
                    break;
                }
                else if (d > ON_EPSILON)
                {
                    counts[0]++;
                }
                else
                {
                    counts[2]++;
                }
            }
            if (k != pass->numpoints)
            {
                continue;                                  // points on negative side, not a seperating plane
            }

            if (!counts[0])
            {
                continue;                                  // planar with seperating plane
            }

            // flip the normal if we want the back side
            if (flipclip)
            {
                VectorSubtract(vec3_origin, plane.normal, plane.normal);
                plane.dist = -plane.dist;
            }

	    if (target != NULL)
	    {
            // clip target by the seperating plane
            target = ChopWinding(target, stack, &plane);
            if (!target)
            {
                return NULL;                               // target is not visible
            }
	    }
	    else
	    {
        	AddPlane(stack, &plane);
	    }

#ifdef RVIS_LEVEL_1
            break; /* Antony was here */
#endif
        }
    }

    return target;
}

// =====================================================================================
//  RecursiveLeafFlow
//      Flood fill through the leafs
//      If src_portal is NULL, this is the originating leaf
// =====================================================================================
inline static void     RecursiveLeafFlow(const int leafnum, const threaddata_t* const thread, const pstack_t* const prevstack)
{
    pstack_t        stack;
    leaf_t*         leaf;

    leaf = &g_leafs[leafnum];
#ifdef USE_CHECK_STACK
    CheckStack(leaf, thread);
#endif

    {
        const unsigned offset = leafnum >> 3;
        const unsigned bit = (1 << (leafnum & 7));
    
        // mark the leaf as visible
        if (!(thread->leafvis[offset] & bit))
        {
            thread->leafvis[offset] |= bit;
            thread->base->numcansee++;
        }
    }

#ifdef USE_CHECK_STACK
    prevstack->next = &stack;
    stack.next = NULL;
#endif
    stack.head = prevstack->head;
    stack.leaf = leaf;
    stack.portal = NULL;
#ifdef RVIS_LEVEL_2
    stack.clipPlaneCount = -1;
    stack.clipPlane = NULL;
#endif

    // check all portals for flowing into other leafs       
    unsigned i;
    portal_t** plist = leaf->portals;

    for (i = 0; i < leaf->numportals; i++, plist++)
    {
        portal_t* p = *plist;

#if ZHLT_ZONES
        portal_t * head_p = stack.head->portal;
        if (g_Zones->check(head_p->zone, p->zone))
        {
            continue;
        }
#endif

        {
            const unsigned offset = p->leaf >> 3;
            const unsigned bit = 1 << (p->leaf & 7);

            if (!(stack.head->mightsee[offset] & bit))
            {
                continue;                                      // can't possibly see it
            }
            if (!(prevstack->mightsee[offset] & bit))
            {
                continue;                                      // can't possibly see it
            }
        }

        // if the portal can't see anything we haven't allready seen, skip it
        {
            long* test;

            if (p->status == stat_done)
            {
                test = (long*)p->visbits;
            }
            else
            {
                test = (long*)p->mightsee;
            }
    
            {
                const int bitlongs = g_bitlongs;

                {
                    long* prevmight = (long*)prevstack->mightsee;
                    long* might = (long*)stack.mightsee;
                
                    unsigned j;
                    for (j = 0; j < bitlongs; j++, test++, might++, prevmight++)
                    {
                        (*might) = (*prevmight) & (*test);
                    }
                }
        
                {
                    long* might = (long*)stack.mightsee;
                    long* vis = (long*)thread->leafvis;
                    unsigned j;
                    for (j = 0; j < bitlongs; j++, might++, vis++)
                    {
                        if ((*might) & ~(*vis))
                        {
                            break;
                        }
                    }
            
                    if (j == g_bitlongs)
                    {                                                  // can't see anything new
                        continue;
                    }
                }
            }
        }

        // get plane of portal, point normal into the neighbor leaf
        stack.portalplane = &p->plane;
        plane_t             backplane;
        VectorSubtract(vec3_origin, p->plane.normal, backplane.normal);
        backplane.dist = -p->plane.dist;

        if (VectorCompare(prevstack->portalplane->normal, backplane.normal))
        {
            continue;                                      // can't go out a coplanar face
        }

        stack.portal = p;
#ifdef USE_CHECK_STACK
        stack.next = NULL;
#endif
        stack.freewindings[0] = 1;
        stack.freewindings[1] = 1;
        stack.freewindings[2] = 1;

        stack.pass = ChopWinding(p->winding, &stack, thread->pstack_head.portalplane);
        if (!stack.pass)
        {
            continue;
        }

        stack.source = ChopWinding(prevstack->source, &stack, &backplane);
        if (!stack.source)
        {
            continue;
        }

        if (!prevstack->pass)
        {                                                  // the second leaf can only be blocked if coplanar
            RecursiveLeafFlow(p->leaf, thread, &stack);
            continue;
        }

        stack.pass = ChopWinding(stack.pass, &stack, prevstack->portalplane);
        if (!stack.pass)
        {
            continue;
        }

#ifdef RVIS_LEVEL_2
        if (stack.clipPlaneCount == -1)
        {
            stack.clipPlaneCount = 0;
            stack.clipPlane = (plane_t*)alloca(sizeof(plane_t) * prevstack->source->numpoints * prevstack->pass->numpoints);

            ClipToSeperators(prevstack->source, prevstack->pass, NULL, false, &stack);
            ClipToSeperators(prevstack->pass, prevstack->source, NULL, true, &stack);
        }

        if (stack.clipPlaneCount > 0)
        {
            unsigned j;
            for (j = 0; j < stack.clipPlaneCount && stack.pass != NULL; j++)
            {
                stack.pass = ChopWinding(stack.pass, &stack, &(stack.clipPlane[j]));
            }

            if (stack.pass == NULL)
            continue;
        }
#else

        stack.pass = ClipToSeperators(stack.source, prevstack->pass, stack.pass, false, &stack);
        if (!stack.pass)
        {
            continue;
        }

        stack.pass = ClipToSeperators(prevstack->pass, stack.source, stack.pass, true, &stack);
        if (!stack.pass)
        {
            continue;
        }
#endif

        if (g_fullvis)
        {
            stack.source = ClipToSeperators(stack.pass, prevstack->pass, stack.source, false, &stack);
            if (!stack.source)
            {
                continue;
            }

            stack.source = ClipToSeperators(prevstack->pass, stack.pass, stack.source, true, &stack);
            if (!stack.source)
            {
                continue;
            }
        }

        // flow through it for real
        RecursiveLeafFlow(p->leaf, thread, &stack);
    }

#ifdef RVIS_LEVEL_2
#if 0
    if (stack.clipPlane != NULL)
    {
        free(stack.clipPlane);
    }
#endif
#endif
}

// =====================================================================================
//  PortalFlow
// =====================================================================================
void            PortalFlow(portal_t* p)
{
    threaddata_t    data;
    unsigned        i;

    if (p->status != stat_working)
        Error("PortalFlow: reflowed");

    p->visbits = (byte*)calloc(1, g_bitbytes);

    memset(&data, 0, sizeof(data));
    data.leafvis = p->visbits;
    data.base = p;

    data.pstack_head.head = &data.pstack_head;
    data.pstack_head.portal = p;
    data.pstack_head.source = p->winding;
    data.pstack_head.portalplane = &p->plane;
    for (i = 0; i < g_bitlongs; i++)
    {
        ((long*)data.pstack_head.mightsee)[i] = ((long*)p->mightsee)[i];
    }
    RecursiveLeafFlow(p->leaf, &data, &data.pstack_head);

#ifdef ZHLT_NETVIS
    p->fromclient = g_clientid;
#endif
    p->status = stat_done;
#ifdef ZHLT_NETVIS
    Flag_VIS_DONE_PORTAL(g_visportalindex);
#endif
}

// =====================================================================================
//  SimpleFlood
//      This is a rough first-order aproximation that is used to trivially reject some
//      of the final calculations.
// =====================================================================================
static void     SimpleFlood(byte* const srcmightsee, const int leafnum, byte* const portalsee, unsigned int* const c_leafsee)
{
    unsigned        i;
    leaf_t*         leaf;
    portal_t*       p;

    {
        const unsigned  offset = leafnum >> 3;
        const unsigned  bit = (1 << (leafnum & 7));
    
        if (srcmightsee[offset] & bit)
        {
            return;
        }
        else
        {
            srcmightsee[offset] |= bit;
        }
    }

    (*c_leafsee)++;
    leaf = &g_leafs[leafnum];

    for (i = 0; i < leaf->numportals; i++)
    {
        p = leaf->portals[i];
        if (!portalsee[p - g_portals])
        {
            continue;
        }
        SimpleFlood(srcmightsee, p->leaf, portalsee, c_leafsee);
    }
}

#define PORTALSEE_SIZE (MAX_PORTALS*2)
#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable: 4100)                             // unreferenced formal parameter
#endif

#ifdef HLVIS_MAXDIST
#ifndef HLVIS_MAXDIST_NEW
// AJM: MVD
// =====================================================================================
//  BlockVis
// =====================================================================================
void			BlockVis(int unused)
{
	int i, j, k, l, m;
	portal_t *p;
	visblocker_t *v;
	visblocker_t *v2;
	leaf_t *leaf;
	
	while(1)
	{
		i = GetThreadWork();
		if(i == -1)
			break;

		v = &g_visblockers[i];

		// See which visblockers we need
		for(j = 0; j < v->numnames; j++)
		{
			// Find visblocker
			if(!(v2 = GetVisBlock(v->blocknames[j])))
				continue;

			// For each leaf in v2, eliminate visibility from v1
			for(k = 0; k < v->numleafs; k++)
			{
				leaf = &g_leafs[v->blockleafs[k]];
				
				for(l = 0; l < leaf->numportals; l++)
				{
					p = leaf->portals[l];
					
					for(m = 0; m < v2->numleafs; m++)
					{
						const unsigned offset = v2->blockleafs[m] >> 3;
						const unsigned bit = (1 << (v2->blockleafs[m] & 7));
	
						p->mightsee[offset] &= ~bit;
					}
				}
			}
		}
	}
}

// AJM: MVD
// =====================================================================================
//  GetSplitPortal
//      This function returns a portal on leaf1 that sucessfully seperates leaf1
//      and leaf2
// =====================================================================================
static portal_t	*GetSplitPortal(leaf_t *leaf1, leaf_t *leaf2)
{
	int i, k, l;

	portal_t	*p1;
	portal_t	*t;

	float check_dist;

	for(i = 0, p1 = leaf1->portals[0]; i < leaf1->numportals; i++, p1++)
	{
		hlassert(p1->winding->numpoints >= 3);
		
		// Check to make sure all the points on the other leaf are in front of the portal plane
		for(k = 0, t = leaf2->portals[0]; k < leaf2->numportals; k++, t++)
		{
			for(l = 0; l < t->winding->numpoints; l++)
			{
				check_dist = DotProduct(t->winding->points[l], p1->plane.normal) - p1->plane.dist;
				
				// We make the assumption that all portals face away from their parent leaf
				if(check_dist < -ON_EPSILON)
					goto PostLoop;
			}
		}

PostLoop:
		// If we didn't check all the leaf2 portals, then this leaf1 portal doesn't work		
		if(k < leaf2->numportals)
			continue;

		// If we reach this point, we found a good portal
		return p1;
	}

	// Didn't find any
	return NULL;
}

// AJM: MVD
// =====================================================================================
//  MakeSplitPortalList
//      This function returns a portal on leaf1 that sucessfully seperates leaf1
//      and leaf2
// =====================================================================================
static void		MakeSplitPortalList(leaf_t *leaf1, leaf_t *leaf2, portal_t **portals, int *num_portals)
{
	int i, k, l;

	portal_t	*p1;
	portal_t	*t;

	*num_portals = 0;

	float check_dist;

	portal_t p_list[MAX_PORTALS_ON_LEAF];
	int c_portal = 0;

	if(*portals)
		delete [] *portals;

	for(i = 0, p1 = leaf1->portals[0]; i < leaf1->numportals; i++, p1++)
	{
		hlassert(p1->winding->numpoints >= 3);
		
		// Check to make sure all the points on the other leaf are in front of the portal plane
		for(k = 0, t = leaf2->portals[0]; k < leaf2->numportals; k++, t++)
		{
			for(l = 0; l < t->winding->numpoints; l++)
			{
				check_dist = DotProduct(t->winding->points[l], p1->plane.normal) - p1->plane.dist;
				
				// We make the assumption that all portals face away from their parent leaf
				if(check_dist < -ON_EPSILON)
					goto PostLoop;
			}
		}

PostLoop:
		// If we didn't check all the leaf2 portals, then this leaf1 portal doesn't work		
		if(k < leaf2->numportals)
			continue;

		// If we reach this point, we found a good portal
		memcpy(&p_list[c_portal++], p1, sizeof(portal_t));

		if(c_portal >= MAX_PORTALS_ON_LEAF)
			Error("c_portal > MAX_PORTALS_ON_LEAF");
	}

	if(!c_portal)
		return;

	*num_portals = c_portal;

	*portals = new portal_t[c_portal];
	memcpy(*portals, p_list, c_portal * sizeof(portal_t));
}

// AJM: MVD
// =====================================================================================
//  DisjointLeafVis
//      This function returns TRUE if neither leaf can see the other
//      Returns FALSE otherwise
// =====================================================================================
static bool			DisjointLeafVis(int leaf1, int leaf2)
{
	leaf_t *l = g_leafs + leaf1;
	leaf_t *tl = g_leafs + leaf2;

	const unsigned offset_l = leaf1 >> 3;
	const unsigned bit_l = (1 << (leaf1 & 7));

	const unsigned offset_tl = leaf2 >> 3;
	const unsigned bit_tl = (1 << (leaf2 & 7));

	for(int k = 0; k < l->numportals; k++)
	{
		for(int m = 0; m < tl->numportals; m++)
		{
			if(l->portals[k]->mightsee[offset_tl] & bit_tl)
				goto RetFalse;
			if(tl->portals[m]->mightsee[offset_l] & bit_l)
				goto RetFalse;

			if(l->portals[k]->status != stat_none)
			{
				if(l->portals[k]->visbits[offset_tl] & bit_tl)
					goto RetFalse;
			}
			if(tl->portals[m]->status != stat_none)
			{
				if(tl->portals[m]->visbits[offset_l] & bit_l)
					goto RetFalse;
			}
		}
	}

	return true;

RetFalse:
	return false;
}

// AJM: MVD
// =====================================================================================
//  GetPortalBounds
//      This function take a portal and finds its bounds
//      parallel to the normal of the portal.  They will face inwards
// =====================================================================================
static void			GetPortalBounds(portal_t *p, plane_t **bounds)
{
	int i;
	vec3_t vec1, vec2;

	hlassert(p->winding->numpoints >= 3);

	if(*bounds)
		delete [] *bounds;

	*bounds = new plane_t[p->winding->numpoints];

	// Loop through each set of points and create a plane boundary for each
	for(i = 0; i < p->winding->numpoints; i++)
	{
		VectorSubtract(p->winding->points[(i + 1) % p->winding->numpoints],p->winding->points[i],vec1);

		// Create inward normal for this boundary
		CrossProduct(p->plane.normal, vec1, vec2);
		VectorNormalize(vec2);

		VectorCopy(vec2, (*bounds)[i].normal);
		(*bounds)[i].dist = DotProduct(p->winding->points[i], vec2);
	}
}

// AJM: MVD
// =====================================================================================
//  ClipWindingsToBounds
//      clips all the windings with all the planes (including original face) and outputs 
//      what's left int "out"
// =====================================================================================
static void			ClipWindingsToBounds(winding_t *windings, int numwindings, plane_t *bounds, int numbounds, plane_t &original_plane, winding_t **out, int &num_out)
{
	hlassert(windings);
	hlassert(bounds);

	winding_t out_windings[MAX_PORTALS_ON_LEAF];
	num_out = 0;

	int h, i;

	*out = NULL;

	Winding wind;

	for(h = 0; h < numwindings; h++)
	{					
		// For each winding...
		// Create a winding with CWinding

		wind.initFromPoints(windings[h].points, windings[h].numpoints);

		// Clip winding to original plane
		wind.Chop(original_plane.normal, original_plane.dist);

		for(i = 0; i < numbounds, wind.Valid(); i++)
		{
			// For each bound...
			// Chop the winding to the bounds
			wind.Chop(bounds[i].normal, bounds[i].dist);
		}
		
		if(wind.Valid())
		{
			// We have a valid winding, copy to array
			wind.CopyPoints(&out_windings[num_out].points[0], out_windings[num_out].numpoints);

			num_out++;
		}
	}

	if(!num_out)		// Everything was clipped away
		return;

	// Otherwise, create out
	*out = new winding_t[num_out];

	memcpy(*out, out_windings, num_out * sizeof(winding_t));
}

// AJM: MVD
// =====================================================================================
//  GenerateWindingList
//      This function generates a list of windings for a leaf through its portals
// =====================================================================================
static void			GenerateWindingList(leaf_t *leaf, winding_t **winds)
{
	

	winding_t windings[MAX_PORTALS_ON_LEAF];
	int numwinds = 0;

	int i;

	for(i = 0; i < leaf->numportals; i++)
	{
		memcpy(&windings[numwinds++], leaf->portals[i]->winding, sizeof(winding_t));
	}

	if(!numwinds)
		return;

	*winds = new winding_t[numwinds];
	memcpy(*winds, &windings, sizeof(winding_t) * numwinds);
}

// AJM: MVD
// =====================================================================================
//  CalcPortalBoundsAndClipPortals
// =====================================================================================
static void			CalcPortalBoundsAndClipPortals(portal_t *portal, leaf_t *leaf, winding_t **out, int &numout)
{
	plane_t *bounds = NULL;
	winding_t *windings = NULL;

	GetPortalBounds(portal, &bounds);
	GenerateWindingList(leaf, &windings);

	ClipWindingsToBounds(windings, leaf->numportals, bounds, portal->winding->numpoints, portal->plane, out, numout);

	delete bounds;
	delete windings;
}

// AJM: MVD
// =====================================================================================
//  GetShortestDistance
//      Gets the shortest distance between both leaves
// =====================================================================================
static float		GetShortestDistance(leaf_t *leaf1, leaf_t *leaf2)
{
	winding_t *final = NULL;
	int num_finals = 0;

	int i, x, y;
	float check;

	for(i = 0; i < leaf1->numportals; i++)
	{
		CalcPortalBoundsAndClipPortals(leaf1->portals[i], leaf2, &final, num_finals);

		// Minimum point distance
		for(x = 0; x < num_finals; x++)
		{
			for(y = 0; y < final[x].numpoints; y++)
			{
				check = DotProduct(leaf1->portals[i]->plane.normal, final[x].points[y]) - leaf1->portals[i]->plane.dist;

				if(check <= g_maxdistance)
					return check;
			}
		}

		delete final;
	}

	// Switch leaf 1 and 2
	for(i = 0; i < leaf2->numportals; i++)
	{
		CalcPortalBoundsAndClipPortals(leaf2->portals[i], leaf1, &final, num_finals);

		// Minimum point distance
		for(x = 0; x < num_finals; x++)
		{
			for(y = 0; y < final[x].numpoints; y++)
			{
				check = DotProduct(leaf2->portals[i]->plane.normal, final[x].points[y]) - leaf2->portals[i]->plane.dist;

				if(check <= g_maxdistance)
					return check;
			}
		}

		delete final;
	}

	return 9E10;
}

// AJM: MVD
// =====================================================================================
//  CalcSplitsAndDotProducts
//      This function finds the splits of the leaf, and generates windings (if applicable)
// =====================================================================================
static float		CalcSplitsAndDotProducts(plane_t *org_split_plane, leaf_t *leaf1, leaf_t *leaf2, plane_t *bounds, int num_bounds)
{
	int i, j, k, l;
	
	portal_t *splits = NULL;
	int num_splits;

	float dist;
	float min_dist = 999999999.999;

	vec3_t i_points[MAX_POINTS_ON_FIXED_WINDING * MAX_PORTALS_ON_LEAF * 2];
	vec3_t delta;
	int num_points = 0;

	// First get splits
	MakeSplitPortalList(leaf1, leaf2, &splits, &num_splits);

	if(!num_splits)
		return min_dist;

	// If the number of splits = 1, then clip the plane using the boundary windings
	if(num_splits == 1)
	{
		Winding wind(splits[0].plane.normal, splits[0].plane.dist);

		for(i = 0;  i < num_bounds; i++)
		{
			wind.Chop(bounds[i].normal, bounds[i].dist);
		}

		// The wind is chopped - get closest dot product
		for(i = 0; i < wind.m_NumPoints; i++)
		{
			dist = DotProduct(wind.m_Points[i], org_split_plane->normal) - org_split_plane->dist;

			min_dist = qmin(min_dist, dist);
		}

		return min_dist;
	}

	// In this case, we have more than one split point, and we must calculate all intersections
	// Properties of convex objects allow us to assume that these intersections will be the closest
	// points to the other leaf, and our other checks before this eliminate exception cases

	// Loop through each split portal, and using an inside loop, loop through every OTHER split portal
	// Common portal points in more than one split portal are intersections!
	for(i = 0; i < num_splits; i++)
	{
		for(j = 0; j < num_splits; j++)
		{
			if(i == j)
			{
				continue;
			}

			// Loop through each point on both portals
			for(k = 0; k < splits[i].winding->numpoints; k++)
			{
				for(l = 0; l < splits[j].winding->numpoints; l++)
				{
					VectorSubtract(splits[i].winding->points[k], splits[j].winding->points[l], delta);

					if(VectorLength(delta) < EQUAL_EPSILON)
					{
						memcpy(i_points[num_points++], splits[i].winding->points[k], sizeof(vec3_t));
					}
				}
			}
		}
	}

	// Loop through each intersection point and check
	for(i = 0; i < num_points; i++)
	{
		dist = DotProduct(i_points[i], org_split_plane->normal) - org_split_plane->dist;

		min_dist = qmin(min_dist, dist);
	}

	if(splits)
		delete [] splits;

	return min_dist;			
}

#endif
#endif // HLVIS_MAXDIST

// =====================================================================================
//  BasePortalVis
// =====================================================================================
void            BasePortalVis(int unused)
{
    int             i, j, k;
    portal_t*       tp;
    portal_t*       p;
    float           d;
    winding_t*      w;
    byte            portalsee[PORTALSEE_SIZE];
    const int       portalsize = (g_numportals * 2);

#ifdef ZHLT_NETVIS
    {
        i = unused;
#else
    while (1)
    {
        i = GetThreadWork();
        if (i == -1)
            break;
#endif
        p = g_portals + i;

        p->mightsee = (byte*)calloc(1, g_bitbytes);

        memset(portalsee, 0, portalsize);

#if ZHLT_ZONES
        UINT32 zone = p->zone;
#endif

        for (j = 0, tp = g_portals; j < portalsize; j++, tp++)
        {
            if (j == i)
            {
                continue;
            }
#if ZHLT_ZONES
            if (g_Zones->check(zone, tp->zone))
            {
                continue;
            }
#endif

            w = tp->winding;
            for (k = 0; k < w->numpoints; k++)
            {
                d = DotProduct(w->points[k], p->plane.normal) - p->plane.dist;
                if (d > ON_EPSILON)
                {
                    break;
                }
            }
            if (k == w->numpoints)
            {
                continue;                                  // no points on front
            }


            w = p->winding;
            for (k = 0; k < w->numpoints; k++)
            {
                d = DotProduct(w->points[k], tp->plane.normal) - tp->plane.dist;
                if (d < -ON_EPSILON)
                {
                    break;
                }
            }
            if (k == w->numpoints)
            {
                continue;                                  // no points on front
            }


            portalsee[j] = 1;
        }

        SimpleFlood(p->mightsee, p->leaf, portalsee, &p->nummightsee);
        Verbose("portal:%4i  nummightsee:%4i \n", i, p->nummightsee);
    }
}

#ifdef HLVIS_MAXDIST
#ifdef HLVIS_MAXDIST_NEW
bool BestNormalFromWinding (const vec3_t *points, int numpoints, vec3_t &normal_out)
{
	const vec3_t *pt1, *pt2, *pt3;
	int k;
	vec3_t d, normal, edge;
	vec_t dist, maxdist;
	if (numpoints < 3)
	{
		return false;
	}
	pt1 = &points[0];
	maxdist = -1;
	for (k = 0; k < numpoints; k++)
	{
		if (&points[k] == pt1)
		{
			continue;
		}
		VectorSubtract (points[k], *pt1, edge);
		dist = DotProduct (edge, edge);
		if (dist > maxdist)
		{
			maxdist = dist;
			pt2 = &points[k];
		}
	}
	if (maxdist <= ON_EPSILON * ON_EPSILON)
	{
		return false;
	}
	maxdist = -1;
	VectorSubtract (*pt2, *pt1, edge);
	VectorNormalize (edge);
	for (k = 0; k < numpoints; k++)
	{
		if (&points[k] == pt1 || &points[k] == pt2)
		{
			continue;
		}
		VectorSubtract (points[k], *pt1, d);
		CrossProduct (edge, d, normal);
		dist = DotProduct (normal, normal);
		if (dist > maxdist)
		{
			maxdist = dist;
			pt3 = &points[k];
		}
	}
	if (maxdist <= ON_EPSILON * ON_EPSILON)
	{
		return false;
	}
	VectorSubtract (*pt3, *pt1, d);
	CrossProduct (edge, d, normal);
	VectorNormalize (normal);
	if (pt3 < pt2)
	{
		VectorScale (normal, -1, normal);
	}
	VectorCopy (normal, normal_out);
	return true;
}

vec_t WindingDist (const winding_t *w[2])
{
	vec_t minsqrdist = 99999999.0 * 99999999.0;
	vec_t sqrdist;
	int a, b;
	// point to point
	for (a = 0; a < w[0]->numpoints; a++)
	{
		for (b = 0; b < w[1]->numpoints; b++)
		{
			vec3_t v;
			VectorSubtract (w[0]->points[a], w[1]->points[b], v);
			sqrdist = DotProduct (v, v);
			if (sqrdist < minsqrdist)
			{
				minsqrdist = sqrdist;
			}
		}
	}
	int side;
	// point to edge
	for (side = 0; side < 2; side++)
	{
		for (a = 0; a < w[side]->numpoints; a++)
		{
			for (b = 0; b < w[!side]->numpoints; b++)
			{
				const vec3_t &p = w[side]->points[a];
				const vec3_t &p1 = w[!side]->points[b];
				const vec3_t &p2 = w[!side]->points[(b + 1) % w[!side]->numpoints];
				vec3_t delta;
				vec_t frac;
				vec3_t v;
				VectorSubtract (p2, p1, delta);
				if (VectorNormalize (delta) <= ON_EPSILON)
				{
					continue;
				}
				frac = DotProduct (p, delta) - DotProduct (p1, delta);
				if (frac <= ON_EPSILON || frac >= (DotProduct (p2, delta) - DotProduct (p1, delta)) - ON_EPSILON)
				{
					// p1 or p2 is closest to p
					continue;
				}
				VectorMA (p1, frac, delta, v);
				VectorSubtract (p, v, v);
				sqrdist = DotProduct (v, v);
				if (sqrdist < minsqrdist)
				{
					minsqrdist = sqrdist;
				}
			}
		}
	}
	// edge to edge
	for (a = 0; a < w[0]->numpoints; a++)
	{
		for (b = 0; b < w[1]->numpoints; b++)
		{
			const vec3_t &p1 = w[0]->points[a];
			const vec3_t &p2 = w[0]->points[(a + 1) % w[0]->numpoints];
			const vec3_t &p3 = w[1]->points[b];
			const vec3_t &p4 = w[1]->points[(b + 1) % w[1]->numpoints];
			vec3_t delta1;
			vec3_t delta2;
			vec3_t normal;
			vec3_t normal1;
			vec3_t normal2;
			VectorSubtract (p2, p1, delta1);
			VectorSubtract (p4, p3, delta2);
			CrossProduct (delta1, delta2, normal);
			if (!VectorNormalize (normal))
			{
				continue;
			}
			CrossProduct (normal, delta1, normal1); // same direction as delta2
			CrossProduct (delta2, normal, normal2); // same direction as delta1
			if (VectorNormalize (normal1) <= ON_EPSILON || VectorNormalize (normal2) <= ON_EPSILON)
			{
				continue;
			}
			if (DotProduct (p3, normal1) >= DotProduct (p1, normal1) - ON_EPSILON ||
				DotProduct (p4, normal1) <= DotProduct (p1, normal1) + ON_EPSILON ||
				DotProduct (p1, normal2) >= DotProduct (p3, normal2) - ON_EPSILON ||
				DotProduct (p2, normal2) <= DotProduct (p3, normal2) + ON_EPSILON )
			{
				// the edges are not crossing when viewed along normal
				continue;
			}
			sqrdist = DotProduct (p3, normal) - DotProduct (p1, normal);
			sqrdist = sqrdist * sqrdist;
			if (sqrdist < minsqrdist)
			{
				minsqrdist = sqrdist;
			}
		}
	}
	// point to face and edge to face
	for (side = 0; side < 2; side++)
	{
		vec3_t planenormal;
		vec_t planedist;
		vec3_t *boundnormals;
		vec_t *bounddists;
		if (!BestNormalFromWinding (w[!side]->points, w[!side]->numpoints, planenormal))
		{
			continue;
		}
		planedist = DotProduct (planenormal, w[!side]->points[0]);
		hlassume (boundnormals = (vec3_t *)malloc (w[!side]->numpoints * sizeof (vec3_t)), assume_NoMemory);
		hlassume (bounddists = (vec_t *)malloc (w[!side]->numpoints * sizeof (vec_t)), assume_NoMemory);
		// build boundaries
		for (b = 0; b < w[!side]->numpoints; b++)
		{
			vec3_t v;
			const vec3_t &p1 = w[!side]->points[b];
			const vec3_t &p2 = w[!side]->points[(b + 1) % w[!side]->numpoints];
			VectorSubtract (p2, p1, v);
			CrossProduct (v, planenormal, boundnormals[b]);
			if (!VectorNormalize (boundnormals[b]))
			{
				bounddists[b] = 1.0;
			}
			else
			{
				bounddists[b] = DotProduct (p1, boundnormals[b]);
			}
		}
		for (a = 0; a < w[side]->numpoints; a++)
		{
			const vec3_t &p = w[side]->points[a];
			for (b = 0; b < w[!side]->numpoints; b++)
			{
				if (DotProduct (p, boundnormals[b]) - bounddists[b] >= -ON_EPSILON)
				{
					break;
				}
			}
			if (b < w[!side]->numpoints)
			{
				continue;
			}
			sqrdist = DotProduct (p, planenormal) - planedist;
			sqrdist = sqrdist * sqrdist;
			if (sqrdist < minsqrdist)
			{
				minsqrdist = sqrdist;
			}
		}
		for (a = 0; a < w[side]->numpoints; a++)
		{
			const vec3_t &p1 = w[side]->points[a];
			const vec3_t &p2 = w[side]->points[(a + 1) % w[side]->numpoints];
			vec_t dist1 = DotProduct (p1, planenormal) - planedist;
			vec_t dist2 = DotProduct (p2, planenormal) - planedist;
			vec3_t delta;
			vec_t frac;
			vec3_t v;
			if (dist1 > ON_EPSILON && dist2 < -ON_EPSILON || dist1 < -ON_EPSILON && dist2 > ON_EPSILON)
			{
				frac = dist1 / (dist1 - dist2);
				VectorSubtract (p2, p1, delta);
				VectorMA (p1, frac, delta, v);
				for (b = 0; b < w[!side]->numpoints; b++)
				{
					if (DotProduct (v, boundnormals[b]) - bounddists[b] >= -ON_EPSILON)
					{
						break;
					}
				}
				if (b < w[!side]->numpoints)
				{
					continue;
				}
				minsqrdist = 0;
			}
		}
		free (boundnormals);
		free (bounddists);
	}
	return (sqrt (minsqrdist));
}
#endif
// AJM: MVD
// =====================================================================================
//  MaxDistVis
// =====================================================================================
void	MaxDistVis(int unused)
{
	int i, j, k, m;
	int a, b, c, d;
	leaf_t	*l;
	leaf_t	*tl;
	plane_t	*boundary = NULL;
	vec3_t delta;

	float new_dist;

	unsigned offset_l;
	unsigned bit_l;

	unsigned offset_tl;
	unsigned bit_tl;
	
	while(1)
	{
		i = GetThreadWork();
		if (i == -1)
			break;

		l = &g_leafs[i];

		for(j = i + 1, tl = g_leafs + j; j < g_portalleafs; j++, tl++)
		{
#ifdef HLVIS_MAXDIST_NEW

			offset_l = i >> 3;
			bit_l = (1 << (i & 7));

			offset_tl = j >> 3;
			bit_tl = (1 << (j & 7));

			{
				bool visible = false;
				for (k = 0; k < l->numportals; k++)
				{
					if (l->portals[k]->visbits[offset_tl] & bit_tl)
					{
						visible = true;
					}
				}
				for (m = 0; m < tl->numportals; m++)
				{	
					if (tl->portals[m]->visbits[offset_l] & bit_l)
					{
						visible = true;
					}
				}
				if (!visible)
				{
					goto NoWork;
				}
			}
			
			// rough check
			{
				vec3_t v;
				vec_t dist;
				const winding_t *w;
				const leaf_t *leaf[2] = {l, tl};
				vec3_t center[2];
				vec_t radius[2];
				int side, count[2];
				for (side = 0; side < 2; side++)
				{
					count[side] = 0;
					VectorClear (center[side]);
					for (a = 0; a < leaf[side]->numportals; a++)
					{
						w = leaf[side]->portals[a]->winding;
						for (b = 0; b < w->numpoints; b++)
						{
							VectorAdd (w->points[b], center[side], center[side]);
							count[side]++;
						}
					}
				}
				if (!count[0] && !count[1])
				{
					goto Work;
				}
				for (side = 0; side < 2; side++)
				{
					VectorScale (center[side], 1.0 / (vec_t)count[side], center[side]);
					radius[side] = 0;
					for (a = 0; a < leaf[side]->numportals; a++)
					{
						w = leaf[side]->portals[a]->winding;
						for (b = 0; b < w->numpoints; b++)
						{
							VectorSubtract (w->points[b], center[side], v);
							dist = DotProduct (v, v);
							radius[side] = qmax (radius[side], dist);
						}
					}
					radius[side] = sqrt (radius[side]);
				}
				VectorSubtract (center[0], center[1], v);
				dist = VectorLength (v);
				if (qmax (dist - radius[0] - radius[1], 0) >= g_maxdistance - ON_EPSILON)
				{
					goto Work;
				}
				if (dist + radius[0] + radius[1] < g_maxdistance - ON_EPSILON)
				{
					goto NoWork;
				}
			}

			// exact check
			{
				vec_t mindist = 9999999999;
				vec_t dist;
				for (k = 0; k < l->numportals; k++)
				{
					for (m = 0; m < tl->numportals; m++)
					{
						const winding_t *w[2];
						w[0] = l->portals[k]->winding;
						w[1] = tl->portals[m]->winding;
						dist = WindingDist (w);
						mindist = qmin (dist, mindist);
					}
				}
				if (mindist >= g_maxdistance - ON_EPSILON)
				{
					goto Work;
				}
				else
				{
					goto NoWork;
				}
			}
#else
			if(j == i)		// Ideally, should never be true
			{
				continue;
			}

			// If they already can't see each other, no use checking
			if(DisjointLeafVis(i, j))
			{
				continue;
			}

			new_dist = GetShortestDistance(l, tl);

			if(new_dist <= g_maxdistance)
				continue;

			// Try out our NEW, IMPROVED ALGORITHM!!!!
			
			// Get a portal on Leaf 1 that completely seperates the two leafs
			/*split = GetSplitPortal(l, tl);

			if(!split)
				continue;

			// We have a split, so create the bounds
			GetPortalBounds(split, &boundary);

			// Now get the dot product for all points on the other leaf
			max_dist = 999999999.999;

			/// Do the first check if mode is >= 2
			if(g_mdmode >= 2)
			{
				for(k = 0; k < tl->numportals; k++)
				{
					for(m = 0; m < tl->portals[k]->winding->numpoints; m++)
					{
						for(n = 0; n < split->winding->numpoints; n++) // numpoints of split portals = number of boundaries
						{
							dist = DotProduct(tl->portals[k]->winding->points[m], boundary[n].normal) - boundary[n].dist;
	
							if(dist < -ON_EPSILON)
							{
								// Outside boundaries
								//max_dot = MaxDotProduct(tl->portals[k]->winding->points[m], boundary, split->winding->numpoints);

								//max_dist = qmin(max_dist, max_dot);

								// Break so we don't do inside boundary check
								break;
							}
						}
						if(n < split->winding->numpoints)
							continue;

						// We found a point that's inside all the boundries!
						new_dist = DotProduct(tl->portals[k]->winding->points[m], split->plane.normal) - split->plane.dist;

						max_dist = qmin(max_dist, new_dist);
					}
				}
			}

			// This is now a special check.  If Leaf 2 has a split plane, we generate a polygon by clipping the plane
			// with the borders.  We then get the minimum dot products.  If more than one split plane, use intersection.
			// Only do this is g_mdmode is 3
			if(g_mdmode >= 3)	// For future mode expansion
			{
				new_dist = CalcSplitsAndDotProducts(&split->plane, tl, l, boundary, split->winding->numpoints);

				max_dist = qmin(max_dist, new_dist);
			}*/

			// Third and final check.  If the whole of leaf2 is outside of leaf1 boundaries, this one will catch it
			// Basic "every point to every point" type of deal :)
			// This is done by default all the time
			for(a = 0; a < l->numportals; a++)
			{
				for(b = 0; b < tl->numportals; b++)
				{
					for(c = 0; c < l->portals[a]->winding->numpoints; c++)
					{
						for(d = 0; d < tl->portals[b]->winding->numpoints; d++)
						{
							VectorSubtract(l->portals[a]->winding->points[c], tl->portals[b]->winding->points[d], delta);

							if(VectorLength(delta) <= g_maxdistance)
								goto NoWork;
						}
					}
				}
			}
#endif

#ifdef HLVIS_MAXDIST_NEW
Work:
			ThreadLock ();
			for (k = 0; k < l->numportals; k++)
			{
				l->portals[k]->visbits[offset_tl] &= ~bit_tl;
			}
			for (m = 0; m < tl->numportals; m++)
			{
				tl->portals[m]->visbits[offset_l] &= ~bit_l;
			}
			ThreadUnlock ();
#else
			offset_l = i >> 3;
			bit_l = (1 << (i & 7));

			offset_tl = j >> 3;
			bit_tl = (1 << (j & 7));

			for(k = 0; k < l->numportals; k++)
			{
				for(m = 0; m < tl->numportals; m++)
				{
					if(l->portals[k]->status != stat_none)
						l->portals[k]->visbits[offset_tl] &= ~bit_tl;
					else
						l->portals[k]->mightsee[offset_tl] &= ~bit_tl;
					
					if(tl->portals[m]->status != stat_none)
						tl->portals[m]->visbits[offset_l] &= ~bit_l;
					else
						tl->portals[m]->mightsee[offset_l] &= ~bit_l;
				}
			}
#endif
			
NoWork:
			continue;	// Hack to keep label from causing compile error
		}
	}

	// Release potential memory
	if(boundary)
		delete [] boundary;
}
#endif // HLVIS_MAXDIST

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

