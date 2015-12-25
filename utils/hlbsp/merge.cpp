#include "bsp5.h"

//  TryMerge
//  MergeFaceToList
//  FreeMergeListScraps
//  MergePlaneFaces
//  MergeAll

#define CONTINUOUS_EPSILON	ON_EPSILON

// =====================================================================================
//  TryMerge
//      If two polygons share a common edge and the edges that meet at the
//      common points are both inside the other polygons, merge them
//      Returns NULL if the faces couldn't be merged, or the new face.
//      The originals will NOT be freed.
// =====================================================================================
static face_t*  TryMerge(face_t* f1, face_t* f2)
{
    vec_t*          p1;
    vec_t*          p2;
    vec_t*          p3;
    vec_t*          p4;
    vec_t*          back;
    face_t*         newf;
    int             i;
    int             j;
    int             k;
    int             l;
    vec3_t          normal;
    vec3_t          delta;
    vec3_t          planenormal;
    vec_t           dot;
    dplane_t*       plane;
    bool            keep1;
    bool            keep2;

    if (f1->numpoints == -1 || f2->numpoints == -1)
    {
        return NULL;
    }    
    if (f1->texturenum != f2->texturenum)
    {
        return NULL;
    }
    if (f1->contents != f2->contents)
    {
        return NULL;
    }
#ifdef HLBSP_TryMerge_PLANENUM_FIX
	if (f1->planenum != f2->planenum)
	{
		return NULL;
	}
	if (f1->facestyle != f2->facestyle)
	{
		return NULL;
	}
#endif
#ifdef ZHLT_DETAILBRUSH
	if (f1->detaillevel != f2->detaillevel)
	{
		return NULL;
	}
#endif

    //
    // find a common edge
    //      
    p1 = p2 = NULL;                                        // shut up the compiler
    j = 0;

    for (i = 0; i < f1->numpoints; i++)
    {
        p1 = f1->pts[i];
        p2 = f1->pts[(i + 1) % f1->numpoints];
        for (j = 0; j < f2->numpoints; j++)
        {
            p3 = f2->pts[j];
            p4 = f2->pts[(j + 1) % f2->numpoints];
            for (k = 0; k < 3; k++)
            {
#ifdef HLBSP_TryMerge_PRECISION_FIX
                if (fabs(p1[k] - p4[k]) > ON_EPSILON)
                {
                    break;
                }
                if (fabs(p2[k] - p3[k]) > ON_EPSILON)
                {
                    break;
                }
#else
                if (fabs(p1[k] - p4[k]) > EQUAL_EPSILON)
                {
                    break;
                }
                if (fabs(p2[k] - p3[k]) > EQUAL_EPSILON)
                {
                    break;
                }
#endif
            }
            if (k == 3)
            {
                break;
            }
        }
        if (j < f2->numpoints)
        {
            break;
        }
    }

    if (i == f1->numpoints)
    {
        return NULL;                                       // no matching edges
    }

    //
    // check slope of connected lines
    // if the slopes are colinear, the point can be removed
    //
    plane = &g_dplanes[f1->planenum];
    VectorCopy(plane->normal, planenormal);

    back = f1->pts[(i + f1->numpoints - 1) % f1->numpoints];
    VectorSubtract(p1, back, delta);
    CrossProduct(planenormal, delta, normal);
    VectorNormalize(normal);

    back = f2->pts[(j + 2) % f2->numpoints];
    VectorSubtract(back, p1, delta);
    dot = DotProduct(delta, normal);
    if (dot > CONTINUOUS_EPSILON)
    {
        return NULL;                                       // not a convex polygon
    }
    keep1 = dot < -CONTINUOUS_EPSILON;

    back = f1->pts[(i + 2) % f1->numpoints];
    VectorSubtract(back, p2, delta);
    CrossProduct(planenormal, delta, normal);
    VectorNormalize(normal);

    back = f2->pts[(j + f2->numpoints - 1) % f2->numpoints];
    VectorSubtract(back, p2, delta);
    dot = DotProduct(delta, normal);
    if (dot > CONTINUOUS_EPSILON)
    {
        return NULL;                                       // not a convex polygon
    }
    keep2 = dot < -CONTINUOUS_EPSILON;

    //
    // build the new polygon
    //
    if (f1->numpoints + f2->numpoints > MAXEDGES)
    {
        //              Error ("TryMerge: too many edges!");
        return NULL;
    }

    newf = NewFaceFromFace(f1);

    // copy first polygon
    for (k = (i + 1) % f1->numpoints; k != i; k = (k + 1) % f1->numpoints)
    {
        if (k == (i + 1) % f1->numpoints && !keep2)
        {
            continue;
        }

        VectorCopy(f1->pts[k], newf->pts[newf->numpoints]);
        newf->numpoints++;
    }

    // copy second polygon
    for (l = (j + 1) % f2->numpoints; l != j; l = (l + 1) % f2->numpoints)
    {
        if (l == (j + 1) % f2->numpoints && !keep1)
        {
            continue;
        }
        VectorCopy(f2->pts[l], newf->pts[newf->numpoints]);
        newf->numpoints++;
    }

    return newf;
}

// =====================================================================================
//  MergeFaceToList
// =====================================================================================
static face_t*  MergeFaceToList(face_t* face, face_t* list)
{
    face_t*         newf;
    face_t*         f;

    for (f = list; f; f = f->next)
    {
        //CheckColinear (f);            
        newf = TryMerge(face, f);
        if (!newf)
        {
            continue;
        }
        FreeFace(face);
        f->numpoints = -1;                                 // merged out
        return MergeFaceToList(newf, list);
    }

    // didn't merge, so add at start
    face->next = list;
    return face;
}

// =====================================================================================
//  FreeMergeListScraps
// =====================================================================================
static face_t*  FreeMergeListScraps(face_t* merged)
{
    face_t*         head;
    face_t*         next;

    head = NULL;
    for (; merged; merged = next)
    {
        next = merged->next;
        if (merged->numpoints == -1)
        {
            FreeFace(merged);
        }
        else
        {
            merged->next = head;
            head = merged;
        }
    }

    return head;
}

// =====================================================================================
//  MergePlaneFaces
// =====================================================================================
void            MergePlaneFaces(surface_t* plane)
{
    face_t*         f1;
    face_t*         next;
    face_t*         merged;

    merged = NULL;

    for (f1 = plane->faces; f1; f1 = next)
    {
        next = f1->next;
        merged = MergeFaceToList(f1, merged);
    }

    // chain all of the non-empty faces to the plane
    plane->faces = FreeMergeListScraps(merged);
}

// =====================================================================================
//  MergeAll
// =====================================================================================
void            MergeAll(surface_t* surfhead)
{
    surface_t*      surf;
    int             mergefaces;
    face_t*         f;

    Verbose("---- MergeAll ----\n");

    mergefaces = 0;
    for (surf = surfhead; surf; surf = surf->next)
    {
        MergePlaneFaces(surf);
        for (f = surf->faces; f; f = f->next)
        {
            mergefaces++;
        }
    }

    Verbose("%i mergefaces\n", mergefaces);
}

