//#pragma warning(disable: 4018) // '<' : signed/unsigned mismatch

#include "bsp5.h"

node_t          g_outside_node;                            // portals outside the world face this

//=============================================================================

/*
 * =============
 * AddPortalToNodes
 * =============
 */
void            AddPortalToNodes(portal_t* p, node_t* front, node_t* back)
{
    if (p->nodes[0] || p->nodes[1])
    {
        Error("AddPortalToNode: allready included");
    }

    p->nodes[0] = front;
    p->next[0] = front->portals;
    front->portals = p;

    p->nodes[1] = back;
    p->next[1] = back->portals;
    back->portals = p;
}

/*
 * =============
 * RemovePortalFromNode
 * =============
 */
void            RemovePortalFromNode(portal_t* portal, node_t* l)
{
    portal_t**      pp;
    portal_t*       t;

    // remove reference to the current portal
    pp = &l->portals;
    while (1)
    {
        t = *pp;
        if (!t)
        {
            Error("RemovePortalFromNode: portal not in leaf");
        }

        if (t == portal)
        {
            break;
        }

        if (t->nodes[0] == l)
        {
            pp = &t->next[0];
        }
        else if (t->nodes[1] == l)
        {
            pp = &t->next[1];
        }
        else
        {
            Error("RemovePortalFromNode: portal not bounding leaf");
        }
    }

    if (portal->nodes[0] == l)
    {
        *pp = portal->next[0];
        portal->nodes[0] = NULL;
    }
    else if (portal->nodes[1] == l)
    {
        *pp = portal->next[1];
        portal->nodes[1] = NULL;
    }
}

//============================================================================

/*
 * ================
 * MakeHeadnodePortals
 * 
 * The created portals will face the global g_outside_node
 * ================
 */
void            MakeHeadnodePortals(node_t* node, const vec3_t mins, const vec3_t maxs)
{
    vec3_t          bounds[2];
    int             i, j, n;
    portal_t*       p;
    portal_t*       portals[6];
    dplane_t        bplanes[6];
    dplane_t*       pl;

    // pad with some space so there will never be null volume leafs
    for (i = 0; i < 3; i++)
    {
        bounds[0][i] = mins[i] - SIDESPACE;
        bounds[1][i] = maxs[i] + SIDESPACE;
    }

    g_outside_node.contents = CONTENTS_SOLID;
    g_outside_node.portals = NULL;

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 2; j++)
        {
            n = j * 3 + i;

            p = AllocPortal();
            portals[n] = p;

            pl = &bplanes[n];
            memset(pl, 0, sizeof(*pl));
            if (j)
            {
                pl->normal[i] = -1;
                pl->dist = -bounds[j][i];
            }
            else
            {
                pl->normal[i] = 1;
                pl->dist = bounds[j][i];
            }
            p->plane = *pl;
            p->winding = new Winding(*pl);
            AddPortalToNodes(p, node, &g_outside_node);
        }
    }

    // clip the basewindings by all the other planes
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 6; j++)
        {
            if (j == i)
            {
                continue;
            }
            portals[i]->winding->Clip(bplanes[j], true);
        }
    }
}

/*
 * ==============================================================================
 * 
 * PORTAL FILE GENERATION
 * 
 * ==============================================================================
 */

static FILE*    pf;
#ifdef HLBSP_VIEWPORTAL
static FILE *pf_view;
extern bool g_viewportal;
#endif
static int      num_visleafs;                              // leafs the player can be in
static int      num_visportals;

static void     WritePortalFile_r(const node_t* const node)
{
    unsigned int    i;
    portal_t*       p;
    Winding*        w;
    dplane_t        plane2;

#ifdef ZHLT_DETAILBRUSH
	if (!node->isportalleaf)
#else
    if (!node->contents)
#endif
    {
        WritePortalFile_r(node->children[0]);
        WritePortalFile_r(node->children[1]);
        return;
    }

    if (node->contents == CONTENTS_SOLID)
    {
        return;
    }

    for (p = node->portals; p;)
    {
        w = p->winding;
        if (w && p->nodes[0] == node)
        {
            if (p->nodes[0]->contents == p->nodes[1]->contents)
            {
                // write out to the file

                // sometimes planes get turned around when they are very near
                // the changeover point between different axis.  interpret the
                // plane the same way vis will, and flip the side orders if needed
                w->getPlane(plane2);
                if (DotProduct(p->plane.normal, plane2.normal) < 1.0 - ON_EPSILON)
                {                                          // backwards...
#ifdef ZHLT_WINDING_FIX
					if (DotProduct(p->plane.normal, plane2.normal) > -1.0 + ON_EPSILON)
					{
						Warning ("Colinear portal @");
						w->Print ();
					}
					else
					{
						Warning ("Backward portal @");
						w->Print ();
					}
#endif
                    fprintf(pf, "%u %i %i ", w->m_NumPoints, p->nodes[1]->visleafnum, p->nodes[0]->visleafnum);
                }
                else
                {
                    fprintf(pf, "%u %i %i ", w->m_NumPoints, p->nodes[0]->visleafnum, p->nodes[1]->visleafnum);
                }

                for (i = 0; i < w->m_NumPoints; i++)
                {
                    fprintf(pf, "(%f %f %f) ", w->m_Points[i][0], w->m_Points[i][1], w->m_Points[i][2]);
                }
                fprintf(pf, "\n");
#ifdef HLBSP_VIEWPORTAL
				if (g_viewportal)
				{
					vec3_t center, center1, center2;
					vec3_t from = {0.0, 0.0, -65536};
					w->getCenter (center);
					VectorMA (center, 0.5, p->plane.normal, center1);
					VectorMA (center, -0.5, p->plane.normal, center2);
					fprintf (pf_view, "%5.2f %5.2f %5.2f\n", from[0], from[1], from[2]);
					fprintf (pf_view, "%5.2f %5.2f %5.2f\n", center1[0], center1[1], center1[2]);
					for (i = 0; i < w->m_NumPoints; i++)
					{
						vec_t *p1, *p2;
						p1 = w->m_Points[i];
						p2 = w->m_Points[(i+1)%w->m_NumPoints];
						fprintf (pf_view, "%5.2f %5.2f %5.2f\n", p1[0], p1[1], p1[2]);
						fprintf (pf_view, "%5.2f %5.2f %5.2f\n", p2[0], p2[1], p2[2]);
						fprintf (pf_view, "%5.2f %5.2f %5.2f\n", center2[0], center2[1], center2[2]);
						fprintf (pf_view, "%5.2f %5.2f %5.2f\n", center1[0], center1[1], center1[2]);
					}
				}
#endif
            }
        }

        if (p->nodes[0] == node)
        {
            p = p->next[0];
        }
        else
        {
            p = p->next[1];
        }
    }

}

/*
 * ================
 * NumberLeafs_r
 * ================
 */
static void     NumberLeafs_r(node_t* node)
{
    portal_t*       p;

#ifdef ZHLT_DETAILBRUSH
	if (!node->isportalleaf)
#else
    if (!node->contents)
#endif
    {                                                      // decision node
        node->visleafnum = -99;
        NumberLeafs_r(node->children[0]);
        NumberLeafs_r(node->children[1]);
        return;
    }

    if (node->contents == CONTENTS_SOLID)
    {                                                      // solid block, viewpoint never inside
        node->visleafnum = -1;
        return;
    }

    node->visleafnum = num_visleafs++;

    for (p = node->portals; p;)
    {
        if (p->nodes[0] == node)                           // only write out from first leaf
        {
            if (p->nodes[0]->contents == p->nodes[1]->contents)
            {
                num_visportals++;
            }
            p = p->next[0];
        }
        else
        {
            p = p->next[1];
        }
    }
}

#ifdef ZHLT_DETAILBRUSH
static int CountChildLeafs_r (node_t *node)
{
	if (node->planenum == -1)
	{ // dleaf
		if (node->iscontentsdetail)
		{ // solid
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{ // node
		int count = 0;
		count += CountChildLeafs_r (node->children[0]);
		count += CountChildLeafs_r (node->children[1]);
		return count;
	}
}
static void WriteLeafCount_r (node_t *node)
{
	if (!node->isportalleaf)
	{
		WriteLeafCount_r (node->children[0]);
		WriteLeafCount_r (node->children[1]);
	}
	else
	{
		if (node->contents == CONTENTS_SOLID)
		{
			return;
		}
		int count = CountChildLeafs_r (node);
		fprintf (pf, "%i\n", count);
	}
}
#endif
/*
 * ================
 * WritePortalfile
 * ================
 */
void            WritePortalfile(node_t* headnode)
{
    // set the visleafnum field in every leaf and count the total number of portals
    num_visleafs = 0;
    num_visportals = 0;
    NumberLeafs_r(headnode);

    // write the file
    pf = fopen(g_portfilename, "w");
    if (!pf)
    {
        Error("Error writing portal file %s", g_portfilename);
    }
#ifdef HLBSP_VIEWPORTAL
	if (g_viewportal)
	{
		char filename[_MAX_PATH];
		safe_snprintf(filename, _MAX_PATH, "%s_portal.pts", g_Mapname);
		pf_view = fopen (filename, "w");
		if (!pf_view)
		{
			Error ("Couldn't open %s", filename);
		}
		Log ("Writing '%s' ...\n", filename);
	}
#endif

    fprintf(pf, "%i\n", num_visleafs);
    fprintf(pf, "%i\n", num_visportals);

#ifdef ZHLT_DETAILBRUSH
	WriteLeafCount_r (headnode);
#endif
    WritePortalFile_r(headnode);
    fclose(pf);
#ifdef HLBSP_VIEWPORTAL
	if (g_viewportal)
	{
		fclose (pf_view);
	}
#endif
    Log("BSP generation successful, writing portal file '%s'\n", g_portfilename);
}

//===================================================

void            FreePortals(node_t* node)
{
    portal_t*       p;
    portal_t*       nextp;

#ifdef ZHLT_DETAILBRUSH
	if (!node->isportalleaf)
#else
    if (node->planenum != -1)
#endif
    {
        FreePortals(node->children[0]);
        FreePortals(node->children[1]);
        return;
    }

    for (p = node->portals; p; p = nextp)
    {
        if (p->nodes[0] == node)
        {
            nextp = p->next[0];
        }
        else
        {
            nextp = p->next[1];
        }
        RemovePortalFromNode(p, p->nodes[0]);
        RemovePortalFromNode(p, p->nodes[1]);
        delete p->winding;
        FreePortal(p);
    }
}

