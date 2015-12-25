#include "csg.h"

vec_t           g_BrushUnionThreshold = DEFAULT_BRUSH_UNION_THRESHOLD;

static Winding* NewWindingFromPlane(const brushhull_t* const hull, const int planenum)
{
    Winding*        winding;
    Winding*        front;
    Winding*        back;
    bface_t*        face;
    plane_t*        plane;

    plane = &g_mapplanes[planenum];
    winding = new Winding(plane->normal, plane->dist);

    for (face = hull->faces; face; face = face->next)
    {
        plane = &g_mapplanes[face->planenum];
        winding->Clip(plane->normal, plane->dist, &front, &back);
        delete winding;

        if (front)
        {
            delete front;
        }
        if (back)
        {
            winding = back;
        }
        else
        {
            Developer(DEVELOPER_LEVEL_ERROR, "NewFaceFromPlane returning NULL");
            return NULL;
        }
    }

    return winding;
}

static void     AddFaceToList(bface_t** head, bface_t* newface)
{
    hlassert(newface);
    hlassert(newface->w);
    if (!*head)
    {
        *head = newface;
        return;
    }
    else
    {
        bface_t*        node = *head;

        while (node->next)
        {
            node = node->next;
        }
        node->next = newface;
        newface->next = NULL;
    }
}

static int      NumberOfHullFaces(const brushhull_t* const hull)
{
    int             x;
    bface_t*        face;

    if (!hull->faces)
    {
        return 0;
    }

    for (x = 0, face = hull->faces; face; face = face->next, x++)
    {                                                  // counter
    }

    return x;
}

// Returns false if union of brushes is obviously zero
static void     AddPlaneToUnion(brushhull_t* hull, const int planenum)
{
    bool            need_new_face = false;

    bface_t*        new_face_list;

    bface_t*        face;
    bface_t*        next;

    plane_t*        split;
    Winding*        front;
    Winding*        back;

    new_face_list = NULL;

    next = NULL;

    hlassert(hull);

    if (!hull->faces)
    {
        return;
    }
    hlassert(hull->faces->w);

    for (face = hull->faces; face; face = next)
    {
        hlassert(face->w);
        next = face->next;

        // Duplicate plane, ignore
        if (face->planenum == planenum)
        {
            AddFaceToList(&new_face_list, CopyFace(face));
            continue;
        }

        split = &g_mapplanes[planenum];
        face->w->Clip(split->normal, split->dist, &front, &back);

        if (front)
        {
            delete front;
            need_new_face = true;

            if (back)
            {                                              // Intersected the face
                delete face->w;
                face->w = back;
                AddFaceToList(&new_face_list, CopyFace(face));
            }
        }
        else
        {
            // Completely missed it, back is identical to face->w so it is destroyed
            if (back)
            {
                delete back;
                AddFaceToList(&new_face_list, CopyFace(face));
            }
        }
        hlassert(face->w);
    }

    FreeFaceList(hull->faces);
    hull->faces = new_face_list;

    if (need_new_face && (NumberOfHullFaces(hull) > 2))
    {
        Winding*        new_winding = NewWindingFromPlane(hull, planenum);

        if (new_winding)
        {
            bface_t*        new_face = (bface_t*)Alloc(sizeof(bface_t));

            new_face->planenum = planenum;
            new_face->w = new_winding;

            new_face->next = hull->faces;
            hull->faces = new_face;
        }
    }
}

static vec_t    CalculateSolidVolume(const brushhull_t* const hull)
{
    // calculate polyhedron origin
    // subdivide face winding into triangles

    // for each face
    // calculate volume of triangle of face to origin
    // add subidivided volume chunk to total

    int             x = 0;
    vec_t           volume = 0.0;
    vec_t           inverse;
    vec3_t          midpoint = { 0.0, 0.0, 0.0 };

    bface_t*        face;

    for (face = hull->faces; face; face = face->next, x++)
    {
        vec3_t          facemid;

        face->w->getCenter(facemid);
        VectorAdd(midpoint, facemid, midpoint);
        Developer(DEVELOPER_LEVEL_MESSAGE, "Midpoint for face %d is %f %f %f\n", x, facemid[0], facemid[1], facemid[2]);
    }

    inverse = 1.0 / x;

    VectorScale(midpoint, inverse, midpoint);

    Developer(DEVELOPER_LEVEL_MESSAGE, "Midpoint for hull is %f %f %f\n", midpoint[0], midpoint[1], midpoint[2]);

    for (face = hull->faces; face; face = face->next, x++)
    {
        plane_t*        plane = &g_mapplanes[face->planenum];
        vec_t           area = face->w->getArea();
        vec_t           dist = DotProduct(plane->normal, midpoint);

        dist -= plane->dist;
        dist = fabs(dist);

        volume += area * dist / 3.0;
    }

    Developer(DEVELOPER_LEVEL_MESSAGE, "Volume for brush is %f\n", volume);

    return volume;
}

static void     DumpHullWindings(const brushhull_t* const hull)
{
    int             x = 0;
    bface_t*        face;

    for (face = hull->faces; face; face = face->next)
    {
        Developer(DEVELOPER_LEVEL_MEGASPAM, "Winding %d\n", x++);
        face->w->Print();
        Developer(DEVELOPER_LEVEL_MEGASPAM, "\n");
    }
}

static bool     isInvalidHull(const brushhull_t* const hull)
{
    int             x = 0;
    bface_t*        face;

    vec3_t          mins = { 99999.0, 99999.0, 99999.0 };
    vec3_t          maxs = { -99999.0, -99999.0, -99999.0 };

    for (face = hull->faces; face; face = face->next)
    {
        unsigned int    y;
        Winding*        winding = face->w;

        for (y = 0; y < winding->m_NumPoints; y++)
        {
            VectorCompareMinimum(mins, winding->m_Points[y], mins);
            VectorCompareMaximum(maxs, winding->m_Points[y], maxs);
        }
    }

    for (x = 0; x < 3; x++)
    {
        if ((mins[x] < (-BOGUS_RANGE / 2)) || (maxs[x] > (BOGUS_RANGE / 2)))
        {
            return true;
        }
    }
    return false;
}

void            CalculateBrushUnions(const int brushnum)
{
    int             bn, hull;
    brush_t*        b1;
    brush_t*        b2;
    brushhull_t*    bh1;
    brushhull_t*    bh2;
    entity_t*       e;

    b1 = &g_mapbrushes[brushnum];
    e = &g_entities[b1->entitynum];

    for (hull = 0; hull < 1 /* NUM_HULLS */ ; hull++)
    {
        bh1 = &b1->hulls[hull];
        if (!bh1->faces)                                   // Skip it if it is not in this hull
        {
            continue;
        }

        for (bn = brushnum + 1; bn < e->numbrushes; bn++)
        {                                                  // Only compare if b2 > b1, tests are communitive
            b2 = &g_mapbrushes[e->firstbrush + bn];
            bh2 = &b2->hulls[hull];

            if (!bh2->faces)                               // Skip it if it is not in this hull
            {
                continue;
            }
            if (b1->contents != b2->contents)
            {
                continue;                                  // different contents, ignore
            }

            Developer(DEVELOPER_LEVEL_SPAM, "Processing hull %d brush %d and brush %d\n", hull, brushnum, bn);

            {
                brushhull_t     union_hull;
                bface_t*        face;

                union_hull.bounds = bh1->bounds;

                union_hull.faces = CopyFaceList(bh1->faces);

                for (face = bh2->faces; face; face = face->next)
                {
                    AddPlaneToUnion(&union_hull, face->planenum);
                }

                // union was clipped away (no intersection)
                if (!union_hull.faces)
                {
                    continue;
                }

                if (g_developer >= DEVELOPER_LEVEL_MESSAGE)
                {
                    Log("\nUnion windings\n");
                    DumpHullWindings(&union_hull);

                    Log("\nBrush %d windings\n", brushnum);
                    DumpHullWindings(bh1);

                    Log("\nBrush %d windings\n", bn);
                    DumpHullWindings(bh2);
                }


                {
                    vec_t           volume_brush_1;
                    vec_t           volume_brush_2;
                    vec_t           volume_brush_union;
                    vec_t           volume_ratio_1;
                    vec_t           volume_ratio_2;

                    if (isInvalidHull(&union_hull))
                    {
                        FreeFaceList(union_hull.faces);
                        continue;
                    }

                    volume_brush_union = CalculateSolidVolume(&union_hull);
                    volume_brush_1 = CalculateSolidVolume(bh1);
                    volume_brush_2 = CalculateSolidVolume(bh2);

                    volume_ratio_1 = volume_brush_union / volume_brush_1;
                    volume_ratio_2 = volume_brush_union / volume_brush_2;

                    if ((volume_ratio_1 > g_BrushUnionThreshold) || (g_developer >= DEVELOPER_LEVEL_MESSAGE))
                    {
                        volume_ratio_1 *= 100.0;
                        Warning("Entity %d : Brush %d intersects with brush %d by %2.3f percent", 
#ifdef HLCSG_COUNT_NEW
							b1->originalentitynum, b1->originalbrushnum, b2->originalbrushnum, 
#else
							b1->entitynum, brushnum, bn, 
#endif
							volume_ratio_1);
                    }
                    if ((volume_ratio_2 > g_BrushUnionThreshold) || (g_developer >= DEVELOPER_LEVEL_MESSAGE))
                    {
                        volume_ratio_2 *= 100.0;
                        Warning("Entity %d : Brush %d intersects with brush %d by %2.3f percent", 
#ifdef HLCSG_COUNT_NEW
							b1->originalentitynum, b2->originalbrushnum, b1->originalbrushnum, 
#else
							b1->entitynum, bn, brushnum, 
#endif
							volume_ratio_2);
                    }
                }

                FreeFaceList(union_hull.faces);
            }
        }
    }
}

