/*

    BINARY SPACE PARTITION    -aka-    B S P

    Code based on original code from Valve Software,
    Modified by Sean "Zoner" Cavanaugh (seanc@gearboxsoftware.com) with permission.
    Modified by Tony "Merl" Moore (merlinis@bigpond.net.au) [AJM]

*/

#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "bsp5.h"

/*

 NOTES


*/

#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
vec3_t          g_hull_size[NUM_HULLS][2] = 
{
    {// 0x0x0
        {0, 0, 0},          {0, 0, 0}
    }
    ,
    {// 32x32x72
        {-16, -16, -36},    {16, 16, 36}
    }
    ,                                                      
    {// 64x64x64
        {-32, -32, -32},    {32, 32, 32}
    }
    ,                                                      
    {// 32x32x36
        {-16, -16, -18},    {16, 16, 18}
    }                                                     
};
#endif
static FILE*    polyfiles[NUM_HULLS];
#ifdef ZHLT_DETAILBRUSH
static FILE*    brushfiles[NUM_HULLS];
#endif
int             g_hullnum = 0;

static face_t*  validfaces[MAX_INTERNAL_MAP_PLANES];

char            g_bspfilename[_MAX_PATH];
char            g_pointfilename[_MAX_PATH];
char            g_linefilename[_MAX_PATH];
char            g_portfilename[_MAX_PATH];
#ifdef ZHLT_64BIT_FIX
char			g_extentfilename[_MAX_PATH];
#endif

// command line flags
bool			g_noopt = DEFAULT_NOOPT;		// don't optimize BSP on write
#ifdef HLBSP_MERGECLIPNODE
bool			g_noclipnodemerge = DEFAULT_NOCLIPNODEMERGE;
#endif
bool            g_nofill = DEFAULT_NOFILL;      // dont fill "-nofill"
#ifdef HLBSP_FILL
bool			g_noinsidefill = DEFAULT_NOINSIDEFILL;
#endif
bool            g_notjunc = DEFAULT_NOTJUNC;
#ifdef HLBSP_BRINKHACK
bool			g_nobrink = DEFAULT_NOBRINK;
#endif
bool            g_noclip = DEFAULT_NOCLIP;      // no clipping hull "-noclip"
bool            g_chart = DEFAULT_CHART;        // print out chart? "-chart"
bool            g_estimate = DEFAULT_ESTIMATE;  // estimate mode "-estimate"
bool            g_info = DEFAULT_INFO;
bool            g_bLeakOnly = DEFAULT_LEAKONLY; // leakonly mode "-leakonly"
bool            g_bLeaked = false;
int             g_subdivide_size = DEFAULT_SUBDIVIDE_SIZE;

#ifdef ZHLT_NULLTEX // AJM
bool            g_bUseNullTex = DEFAULT_NULLTEX; // "-nonulltex"
#endif

#ifdef ZHLT_DETAIL // AJM
bool            g_bDetailBrushes = DEFAULT_DETAIL; // "-nodetail"
#endif

#ifdef ZHLT_PROGRESSFILE // AJM
char*           g_progressfile = DEFAULT_PROGRESSFILE; // "-progressfile path"
#endif

#ifdef HLBSP_REMOVEHULL2
bool g_nohull2 = false;
#endif

#ifdef HLBSP_VIEWPORTAL
bool g_viewportal = false;
#endif

#ifdef HLCSG_HLBSP_DOUBLEPLANE
dplane_t g_dplanes[MAX_INTERNAL_MAP_PLANES];
#endif


#ifdef ZHLT_INFO_COMPILE_PARAMETERS// AJM
// =====================================================================================
//  GetParamsFromEnt
//      this function is called from parseentity when it encounters the
//      info_compile_parameters entity. each tool should have its own version of this
//      to handle its own specific settings.
// =====================================================================================
void            GetParamsFromEnt(entity_t* mapent)
{
    int iTmp;

    Log("\nCompile Settings detected from info_compile_parameters entity\n");

    // verbose(choices) : "Verbose compile messages" : 0 = [ 0 : "Off" 1 : "On" ]
    iTmp = IntForKey(mapent, "verbose");
    if (iTmp == 1)
    {
        g_verbose = true;
    }
    else if (iTmp == 0)
    {
        g_verbose = false;
    }
    Log("%30s [ %-9s ]\n", "Compile Option", "setting");
    Log("%30s [ %-9s ]\n", "Verbose Compile Messages", g_verbose ? "on" : "off");

    // estimate(choices) :"Estimate Compile Times?" : 0 = [ 0: "Yes" 1: "No" ]
    if (IntForKey(mapent, "estimate"))
    {
        g_estimate = true;
    }
    else
    {
        g_estimate = false;
    }
    Log("%30s [ %-9s ]\n", "Estimate Compile Times", g_estimate ? "on" : "off");

	// priority(choices) : "Priority Level" : 0 = [	0 : "Normal" 1 : "High"	-1 : "Low" ]
	if (!strcmp(ValueForKey(mapent, "priority"), "1"))
    {
        g_threadpriority = eThreadPriorityHigh;
        Log("%30s [ %-9s ]\n", "Thread Priority", "high");
    }
    else if (!strcmp(ValueForKey(mapent, "priority"), "-1"))
    {
        g_threadpriority = eThreadPriorityLow;
        Log("%30s [ %-9s ]\n", "Thread Priority", "low");
    }

    /*
    hlbsp(choices) : "HLBSP" : 0 =
    [
       0 : "Off"
       1 : "Normal"
       2 : "Leakonly"
    ]
    */
    iTmp = IntForKey(mapent, "hlbsp");
    if (iTmp == 0)
    {
        Fatal(assume_TOOL_CANCEL,
            "%s flag was not checked in info_compile_parameters entity, execution of %s cancelled", g_Program, g_Program);
        CheckFatal();
    }
    else if (iTmp == 1)
    {
        g_bLeakOnly = false;
    }
    else if (iTmp == 2)
    {
        g_bLeakOnly = true;
    }
    Log("%30s [ %-9s ]\n", "Leakonly Mode", g_bLeakOnly ? "on" : "off");

	iTmp = IntForKey(mapent, "noopt");
	if(iTmp == 0)
	{
		g_noopt = false;
	}
	else
	{
		g_noopt = true;
	}

    /*
    nocliphull(choices) : "Generate clipping hulls" : 0 =
    [
        0 : "Yes"
        1 : "No"
    ]
    */
    iTmp = IntForKey(mapent, "nocliphull");
    if (iTmp == 0)
    {
        g_noclip = false;
    }
    else if (iTmp == 1)
    {
        g_noclip = true;
    }
    Log("%30s [ %-9s ]\n", "Clipping Hull Generation", g_noclip ? "off" : "on");

    //////////////////
    Verbose("\n");
}
#endif

// =====================================================================================
//  Extract File stuff (ExtractFile | ExtractFilePath | ExtractFileBase)
//
// With VS 2005 - and the 64 bit build, i had to pull 3 classes over from
// cmdlib.cpp even with the proper includes to get rid of the lnk2001 error
//
// amckern - amckern@yahoo.com
// =====================================================================================

// Code Deleted. --vluzacn

// =====================================================================================
//  NewFaceFromFace
//      Duplicates the non point information of a face, used by SplitFace and MergeFace.
// =====================================================================================
face_t*         NewFaceFromFace(const face_t* const in)
{
    face_t*         newf;

    newf = AllocFace();

    newf->planenum = in->planenum;
    newf->texturenum = in->texturenum;
    newf->original = in->original;
    newf->contents = in->contents;
#ifdef HLBSP_NewFaceFromFace_FIX
	newf->facestyle = in->facestyle;
#endif
#ifdef ZHLT_DETAILBRUSH
	newf->detaillevel = in->detaillevel;
#endif

    return newf;
}

// =====================================================================================
//  SplitFaceTmp
//      blah
// =====================================================================================
static void     SplitFaceTmp(face_t* in, const dplane_t* const split, face_t** front, face_t** back)
{
    vec_t           dists[MAXEDGES + 1];
    int             sides[MAXEDGES + 1];
    int             counts[3];
    vec_t           dot;
    int             i;
    int             j;
    face_t*         newf;
    face_t*         new2;
    vec_t*          p1;
    vec_t*          p2;
    vec3_t          mid;

    if (in->numpoints < 0)
    {
        Error("SplitFace: freed face");
    }
    counts[0] = counts[1] = counts[2] = 0;


    // determine sides for each point
    for (i = 0; i < in->numpoints; i++)
    {
        dot = DotProduct(in->pts[i], split->normal);
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
    sides[i] = sides[0];
    dists[i] = dists[0];

#ifdef HLBSP_SPLITFACE_FIX
	if (!counts[0] && !counts[1])
	{
		if (in->detaillevel)
		{
			// put front face in front node, and back face in back node.
			const dplane_t *faceplane = &g_dplanes[in->planenum];
			if (DotProduct (faceplane->normal, split->normal) > NORMAL_EPSILON) // usually near 1.0 or -1.0
			{
				*front = in;
				*back = NULL;
			}
			else
			{
				*front = NULL;
				*back = in;
			}
		}
		else
		{
			// not func_detail. front face and back face need to pair.
			vec_t sum = 0.0;
			for (i = 0; i < in->numpoints; i++)
			{
				dot = DotProduct(in->pts[i], split->normal);
				dot -= split->dist;
				sum += dot;
			}
			if (sum > NORMAL_EPSILON)
			{
				*front = in;
				*back = NULL;
			}
			else
			{
				*front = NULL;
				*back = in;
			}
		}
		return;
	}
#endif
    if (!counts[0])
    {
        *front = NULL;
        *back = in;
        return;
    }
    if (!counts[1])
    {
        *front = in;
        *back = NULL;
        return;
    }

    *back = newf = NewFaceFromFace(in);
    *front = new2 = NewFaceFromFace(in);

    // distribute the points and generate splits

    for (i = 0; i < in->numpoints; i++)
    {
        if (newf->numpoints > MAXEDGES || new2->numpoints > MAXEDGES)
        {
            Error("SplitFace: numpoints > MAXEDGES");
        }

        p1 = in->pts[i];

        if (sides[i] == SIDE_ON)
        {
            VectorCopy(p1, newf->pts[newf->numpoints]);
            newf->numpoints++;
            VectorCopy(p1, new2->pts[new2->numpoints]);
            new2->numpoints++;
            continue;
        }

        if (sides[i] == SIDE_FRONT)
        {
            VectorCopy(p1, new2->pts[new2->numpoints]);
            new2->numpoints++;
        }
        else
        {
            VectorCopy(p1, newf->pts[newf->numpoints]);
            newf->numpoints++;
        }

        if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
        {
            continue;
        }

        // generate a split point
        p2 = in->pts[(i + 1) % in->numpoints];

        dot = dists[i] / (dists[i] - dists[i + 1]);
        for (j = 0; j < 3; j++)
        {                                                  // avoid round off error when possible
            if (split->normal[j] == 1)
            {
                mid[j] = split->dist;
            }
            else if (split->normal[j] == -1)
            {
                mid[j] = -split->dist;
            }
            else
            {
                mid[j] = p1[j] + dot * (p2[j] - p1[j]);
            }
        }

        VectorCopy(mid, newf->pts[newf->numpoints]);
        newf->numpoints++;
        VectorCopy(mid, new2->pts[new2->numpoints]);
        new2->numpoints++;
    }

    if (newf->numpoints > MAXEDGES || new2->numpoints > MAXEDGES)
    {
        Error("SplitFace: numpoints > MAXEDGES");
    }
#ifdef HLBSP_REMOVECOLINEARPOINTS
	{
		Winding *wd = new Winding (newf->numpoints);
		int x;
		for (x = 0; x < newf->numpoints; x++)
		{
			VectorCopy (newf->pts[x], wd->m_Points[x]);
		}
		wd->RemoveColinearPoints ();
		newf->numpoints = wd->m_NumPoints;
		for (x = 0; x < newf->numpoints; x++)
		{
			VectorCopy (wd->m_Points[x], newf->pts[x]);
		}
		delete wd;
		if (newf->numpoints == 0)
		{
			*back = NULL;
		}
	}
	{
		Winding *wd = new Winding (new2->numpoints);
		int x;
		for (x = 0; x < new2->numpoints; x++)
		{
			VectorCopy (new2->pts[x], wd->m_Points[x]);
		}
		wd->RemoveColinearPoints ();
		new2->numpoints = wd->m_NumPoints;
		for (x = 0; x < new2->numpoints; x++)
		{
			VectorCopy (wd->m_Points[x], new2->pts[x]);
		}
		delete wd;
		if (new2->numpoints == 0)
		{
			*front = NULL;
		}
	}
#endif
}

// =====================================================================================
//  SplitFace
//      blah
// =====================================================================================
void            SplitFace(face_t* in, const dplane_t* const split, face_t** front, face_t** back)
{
    SplitFaceTmp(in, split, front, back);

    // free the original face now that is is represented by the fragments
    if (*front && *back)
    {
        FreeFace(in);
    }
}

// =====================================================================================
//  AllocFace
// =====================================================================================
face_t*         AllocFace()
{
    face_t*         f;

    f = (face_t*)malloc(sizeof(face_t));
    memset(f, 0, sizeof(face_t));

    f->planenum = -1;

    return f;
}

// =====================================================================================
//  FreeFace
// =====================================================================================
void            FreeFace(face_t* f)
{
    free(f);
}

// =====================================================================================
//  AllocSurface
// =====================================================================================
surface_t*      AllocSurface()
{
    surface_t*      s;

    s = (surface_t*)malloc(sizeof(surface_t));
    memset(s, 0, sizeof(surface_t));

    return s;
}

// =====================================================================================
//  FreeSurface
// =====================================================================================
void            FreeSurface(surface_t* s)
{
    free(s);
}

// =====================================================================================
//  AllocPortal
// =====================================================================================
portal_t*       AllocPortal()
{
    portal_t*       p;

    p = (portal_t*)malloc(sizeof(portal_t));
    memset(p, 0, sizeof(portal_t));

    return p;
}

// =====================================================================================
//  FreePortal
// =====================================================================================
void            FreePortal(portal_t* p) // consider: inline
{
    free(p);
}


#ifdef ZHLT_DETAILBRUSH
side_t *AllocSide ()
{
	side_t *s;
	s = (side_t *)malloc (sizeof (side_t));
	memset (s, 0, sizeof (side_t));
	return s;
}

void FreeSide (side_t *s)
{
	if (s->w)
	{
		delete s->w;
	}
	free (s);
	return;
}

side_t *NewSideFromSide (const side_t *s)
{
	side_t *news;
	news = AllocSide ();
	news->plane = s->plane;
	news->w = new Winding (*s->w);
	return news;
}

brush_t *AllocBrush ()
{
	brush_t *b;
	b = (brush_t *)malloc (sizeof (brush_t));
	memset (b, 0, sizeof (brush_t));
	return b;
}

void FreeBrush (brush_t *b)
{
	if (b->sides)
	{
		side_t *s, *next;
		for (s = b->sides; s; s = next)
		{
			next = s->next;
			FreeSide (s);
		}
	}
	free (b);
	return;
}

brush_t *NewBrushFromBrush (const brush_t *b)
{
	brush_t *newb;
	newb = AllocBrush ();
	side_t *s, **pnews;
	for (s = b->sides, pnews = &newb->sides; s; s = s->next, pnews = &(*pnews)->next)
	{
		*pnews = NewSideFromSide (s);
	}
	return newb;
}

void ClipBrush (brush_t **b, const dplane_t *split, vec_t epsilon)
{
	side_t *s, **pnext;
	Winding *w;
	for (pnext = &(*b)->sides, s = *pnext; s; s = *pnext)
	{
		if (s->w->Clip (*split, false, epsilon))
		{
			pnext = &s->next;
		}
		else
		{
			*pnext = s->next;
			FreeSide (s);
		}
	}
	if (!(*b)->sides)
	{ // empty brush
		FreeBrush (*b);
		*b = NULL;
		return;
	}
	w = new Winding (*split);
	for (s = (*b)->sides; s; s = s->next)
	{
		if (!w->Clip (s->plane, false, epsilon))
		{
			break;
		}
	}
	if (w->m_NumPoints == 0)
	{
		delete w;
	}
	else
	{
		s = AllocSide ();
		s->plane = *split;
		s->w = w;
		s->next = (*b)->sides;
		(*b)->sides = s;
	}
	return;
}

void SplitBrush (brush_t *in, const dplane_t *split, brush_t **front, brush_t **back)
	// 'in' will be freed
{
	in->next = NULL;
	bool onfront;
	bool onback;
	onfront = false;
	onback = false;
	side_t *s;
	for (s = in->sides; s; s = s->next)
	{
		switch (s->w->WindingOnPlaneSide (split->normal, split->dist, 2 * ON_EPSILON))
		{
		case SIDE_CROSS:
			onfront = true;
			onback = true;
			break;
		case SIDE_FRONT:
			onfront = true;
			break;
		case SIDE_BACK:
			onback = true;
			break;
		case SIDE_ON:
			break;
		}
		if (onfront && onback)
			break;
	}
	if (!onfront && !onback)
	{
		FreeBrush (in);
		*front = NULL;
		*back = NULL;
		return;
	}
	if (!onfront)
	{
		*front = NULL;
		*back = in;
		return;
	}
	if (!onback)
	{
		*front = in;
		*back = NULL;
		return;
	}
	*front = in;
	*back = NewBrushFromBrush (in);
	dplane_t frontclip = *split;
	dplane_t backclip = *split;
	VectorSubtract (vec3_origin, backclip.normal, backclip.normal);
	backclip.dist = -backclip.dist;
	ClipBrush (front, &frontclip, NORMAL_EPSILON);
	ClipBrush (back, &backclip, NORMAL_EPSILON);
	return;
}

#ifdef HLBSP_DETAILBRUSH_CULL
brush_t *BrushFromBox (const vec3_t mins, const vec3_t maxs)
{
	brush_t *b = AllocBrush ();
	dplane_t planes[6];
	int k;

	for (k = 0; k < 3; k++)
	{
		VectorFill (planes[k].normal, 0.0);
		planes[k].normal[k] = 1.0;
		planes[k].dist = mins[k];
		VectorFill (planes[k+3].normal, 0.0);
		planes[k+3].normal[k] = -1.0;
		planes[k+3].dist = -maxs[k];
	}
	b->sides = AllocSide ();
	b->sides->plane = planes[0];
	b->sides->w = new Winding (planes[0]);
	for (k = 1; k < 6; k++)
	{
		ClipBrush (&b, &planes[k], NORMAL_EPSILON);
		if (b == NULL)
		{
			break;
		}
	}
	return b;
}

void CalcBrushBounds (const brush_t *b, vec3_t &mins, vec3_t &maxs)
{
	VectorFill (mins, BOGUS_RANGE);
	VectorFill (maxs, -BOGUS_RANGE);
	for (side_t *s = b->sides; s; s = s->next)
	{
		vec3_t windingmins, windingmaxs;
		s->w->getBounds (windingmins, windingmaxs);
		VectorCompareMinimum (mins, windingmins, mins);
		VectorCompareMaximum (maxs, windingmaxs, maxs);
	}
}
#endif

#endif
// =====================================================================================
//  AllocNode
//      blah
// =====================================================================================
node_t*         AllocNode()
{
    node_t*         n;

    n = (node_t*)malloc(sizeof(node_t));
    memset(n, 0, sizeof(node_t));

    return n;
}

// =====================================================================================
//  AddPointToBounds
// =====================================================================================
void            AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs)
{
    int             i;
    vec_t           val;

    for (i = 0; i < 3; i++)
    {
        val = v[i];
        if (val < mins[i])
        {
            mins[i] = val;
        }
        if (val > maxs[i])
        {
            maxs[i] = val;
        }
    }
}

// =====================================================================================
//  AddFaceToBounds
// =====================================================================================
static void     AddFaceToBounds(const face_t* const f, vec3_t mins, vec3_t maxs)
{
    int             i;

    for (i = 0; i < f->numpoints; i++)
    {
        AddPointToBounds(f->pts[i], mins, maxs);
    }
}

// =====================================================================================
//  ClearBounds
// =====================================================================================
static void     ClearBounds(vec3_t mins, vec3_t maxs)
{
    mins[0] = mins[1] = mins[2] = 99999;
    maxs[0] = maxs[1] = maxs[2] = -99999;
}

// =====================================================================================
//  SurflistFromValidFaces
//      blah
// =====================================================================================
static surfchain_t* SurflistFromValidFaces()
{
    surface_t*      n;
    int             i;
    face_t*         f;
    face_t*         next;
    surfchain_t*    sc;

    sc = (surfchain_t*)malloc(sizeof(*sc));
    ClearBounds(sc->mins, sc->maxs);
    sc->surfaces = NULL;

    // grab planes from both sides
    for (i = 0; i < g_numplanes; i += 2)
    {
        if (!validfaces[i] && !validfaces[i + 1])
        {
            continue;
        }
        n = AllocSurface();
        n->next = sc->surfaces;
        sc->surfaces = n;
        ClearBounds(n->mins, n->maxs);
#ifdef ZHLT_DETAILBRUSH
		n->detaillevel = -1;
#endif
        n->planenum = i;

        n->faces = NULL;
        for (f = validfaces[i]; f; f = next)
        {
            next = f->next;
            f->next = n->faces;
            n->faces = f;
            AddFaceToBounds(f, n->mins, n->maxs);
#ifdef ZHLT_DETAILBRUSH
			if (n->detaillevel == -1 || f->detaillevel < n->detaillevel)
			{
				n->detaillevel = f->detaillevel;
			}
#endif
        }
        for (f = validfaces[i + 1]; f; f = next)
        {
            next = f->next;
            f->next = n->faces;
            n->faces = f;
            AddFaceToBounds(f, n->mins, n->maxs);
#ifdef ZHLT_DETAILBRUSH
			if (n->detaillevel == -1 || f->detaillevel < n->detaillevel)
			{
				n->detaillevel = f->detaillevel;
			}
#endif
        }

        AddPointToBounds(n->mins, sc->mins, sc->maxs);
        AddPointToBounds(n->maxs, sc->mins, sc->maxs);

        validfaces[i] = NULL;
        validfaces[i + 1] = NULL;
    }

    // merge all possible polygons

    MergeAll(sc->surfaces);

    return sc;
}

#ifdef ZHLT_NULLTEX// AJM
// =====================================================================================
//  CheckFaceForNull
//      Returns true if the passed face is facetype null
// =====================================================================================
bool            CheckFaceForNull(const face_t* const f)
{
#ifdef HLBSP_SKY_SOLID
	if (f->contents == CONTENTS_SKY)
    {
		const char *name = GetTextureByNumber (f->texturenum);
        if (strncasecmp(name, "sky", 3)) // for env_rain
			return true;
    }
#endif
    // null faces are only of facetype face_null if we are using null texture stripping
    if (g_bUseNullTex)
    {
#ifdef HLCSG_HLBSP_VOIDTEXINFO
		const char *name = GetTextureByNumber (f->texturenum);
		if (!strncasecmp(name, "null", 4))
			return true;
		return false;
#else
        texinfo_t*      info;
        miptex_t*       miptex;
        int             ofs;

        info = &g_texinfo[f->texturenum];
        ofs = ((dmiptexlump_t*)g_dtexdata)->dataofs[info->miptex];
        miptex = (miptex_t*)(&g_dtexdata[ofs]);

        if (!strcasecmp(miptex->name, "null"))
            return true;
	#ifdef HLCSG_CUSTOMHULL
        else if (!strncasecmp(miptex->name, "null", 4))
            return true;
	#else
        else
            return false;
	#endif
#endif
    }
    else // otherwise, under normal cases, null textured faces should be facetype face_normal
    {
        return false;
    }
}
// =====================================================================================
//Cpt_Andrew - UTSky Check
// =====================================================================================
bool            CheckFaceForEnv_Sky(const face_t* const f)
{
#ifdef HLCSG_HLBSP_VOIDTEXINFO
	const char *name = GetTextureByNumber (f->texturenum);
	if (!strncasecmp (name, "env_sky", 7))
		return true;
	return false;
#else
        texinfo_t*      info;
        miptex_t*       miptex;
        int             ofs;

        info = &g_texinfo[f->texturenum];
        ofs = ((dmiptexlump_t*)g_dtexdata)->dataofs[info->miptex];
        miptex = (miptex_t*)(&g_dtexdata[ofs]);

        if (!strcasecmp(miptex->name, "env_sky"))
            return true;
        else
            return false;
#endif
}
// =====================================================================================




#endif

#ifdef ZHLT_DETAIL
// =====================================================================================
//  CheckFaceForDetail
//      Returns true if the passed face is part of a detail brush
// =====================================================================================
bool            CheckFaceForDetail(const face_t* const f)
{
    if (f->contents == CONTENTS_DETAIL)
    {
        //Log("CheckFaceForDetail:: got a detail face");
        return true;
    }

    return false;
}
#endif

// =====================================================================================
//  CheckFaceForHint
//      Returns true if the passed face is facetype hint
// =====================================================================================
bool            CheckFaceForHint(const face_t* const f)
{
#ifdef HLCSG_HLBSP_VOIDTEXINFO
	const char *name = GetTextureByNumber (f->texturenum);
	if (!strncasecmp (name, "hint", 4))
		return true;
	return false;
#else
    texinfo_t*      info;
    miptex_t*       miptex;
    int             ofs;

    info = &g_texinfo[f->texturenum];
    ofs = ((dmiptexlump_t *)g_dtexdata)->dataofs[info->miptex];
    miptex = (miptex_t *)(&g_dtexdata[ofs]);

    if (!strcasecmp(miptex->name, "hint"))
    {
        return true;
    }
    else
    {
        return false;
    }
#endif
}

// =====================================================================================
//  CheckFaceForSkipt
//      Returns true if the passed face is facetype skip
// =====================================================================================
bool            CheckFaceForSkip(const face_t* const f)
{
#ifdef HLCSG_HLBSP_VOIDTEXINFO
	const char *name = GetTextureByNumber (f->texturenum);
	if (!strncasecmp (name, "skip", 4))
		return true;
	return false;
#else
    texinfo_t*      info;
    miptex_t*       miptex;
    int             ofs;

    info = &g_texinfo[f->texturenum];
    ofs = ((dmiptexlump_t*)g_dtexdata)->dataofs[info->miptex];
    miptex = (miptex_t*)(&g_dtexdata[ofs]);

    if (!strcasecmp(miptex->name, "skip"))
    {
        return true;
    }
    else
    {
        return false;
    }
#endif
}

#ifdef HLCSG_HLBSP_SOLIDHINT
bool CheckFaceForDiscardable (const face_t *f)
{
	const char *name = GetTextureByNumber (f->texturenum);
	if (!strncasecmp (name, "SOLIDHINT", 9))
		return true;
	return false;
}

#endif
// =====================================================================================
//  SetFaceType
// =====================================================================================
static          facestyle_e SetFaceType(face_t* f)
{
    if (CheckFaceForHint(f))
    {
        f->facestyle = face_hint;
    }
    else if (CheckFaceForSkip(f))
    {
        f->facestyle = face_skip;
    }
#ifdef ZHLT_NULLTEX         // AJM
    else if (CheckFaceForNull(f))
    {
        f->facestyle = face_null;
    }
#endif
#ifdef HLCSG_HLBSP_SOLIDHINT
	else if (CheckFaceForDiscardable (f))
	{
		f->facestyle = face_discardable;
	}
#endif

// =====================================================================================
//Cpt_Andrew - Env_Sky Check
// =====================================================================================
   //else if (CheckFaceForUTSky(f))
	else if (CheckFaceForEnv_Sky(f))
    {
        f->facestyle = face_null;
    }
// =====================================================================================


#ifdef ZHLT_DETAIL
    else if (CheckFaceForDetail(f))
    {
        //Log("SetFaceType::detail face\n");
        f->facestyle = face_detail;
    }
#endif
    else
    {
        f->facestyle = face_normal;
    }
    return f->facestyle;
}

// =====================================================================================
//  ReadSurfs
// =====================================================================================
static surfchain_t* ReadSurfs(FILE* file)
{
    int             r;
#ifdef ZHLT_DETAILBRUSH
	int				detaillevel;
#endif
    int             planenum, g_texinfo, contents, numpoints;
    face_t*         f;
    int             i;
    double          v[3];
    int             line = 0;
#ifdef HLCSG_HLBSP_DOUBLEPLANE
	double			inaccuracy, inaccuracy_count = 0.0, inaccuracy_total = 0.0, inaccuracy_max = 0.0;
#endif

    // read in the polygons
    while (1)
    {
#ifdef HLBSP_REMOVEHULL2
		if (file == polyfiles[2] && g_nohull2)
			break;
#endif
        line++;
#ifdef ZHLT_DETAILBRUSH
        r = fscanf(file, "%i %i %i %i %i\n", &detaillevel, &planenum, &g_texinfo, &contents, &numpoints);
#else
        r = fscanf(file, "%i %i %i %i\n", &planenum, &g_texinfo, &contents, &numpoints);
#endif
        if (r == 0 || r == -1)
        {
            return NULL;
        }
        if (planenum == -1)                                // end of model
        {
#ifdef HLCSG_HLBSP_DOUBLEPLANE
			Developer (DEVELOPER_LEVEL_MEGASPAM, "inaccuracy: average %.8f max %.8f\n", inaccuracy_total / inaccuracy_count, inaccuracy_max);
#endif
            break;
        }
#ifdef ZHLT_DETAILBRUSH
		if (r != 5)
#else
        if (r != 4)
#endif
        {
            Error("ReadSurfs (line %i): scanf failure", line);
        }
        if (numpoints > MAXPOINTS)
        {
            Error("ReadSurfs (line %i): %i > MAXPOINTS\nThis is caused by a face with too many verticies (typically found on end-caps of high-poly cylinders)\n", line, numpoints);
        }
        if (planenum > g_numplanes)
        {
            Error("ReadSurfs (line %i): %i > g_numplanes\n", line, planenum);
        }
        if (g_texinfo > g_numtexinfo)
        {
            Error("ReadSurfs (line %i): %i > g_numtexinfo", line, g_texinfo);
        }
#ifdef ZHLT_DETAILBRUSH
		if (detaillevel < 0)
		{
			Error("ReadSurfs (line %i): detaillevel %i < 0", line, detaillevel);
		}
#endif

        if (!strcasecmp(GetTextureByNumber(g_texinfo), "skip"))
        {
            Verbose("ReadSurfs (line %i): skipping a surface", line);

            for (i = 0; i < numpoints; i++)
            {
                line++;
                //Verbose("skipping line %d", line);
                r = fscanf(file, "%lf %lf %lf\n", &v[0], &v[1], &v[2]);
                if (r != 3)
                {
                    Error("::ReadSurfs (face_skip), fscanf of points failed at line %i", line);
                }
            }
            fscanf(file, "\n");
            continue;
        }

        f = AllocFace();
#ifdef ZHLT_DETAILBRUSH
		f->detaillevel = detaillevel;
#endif
        f->planenum = planenum;
        f->texturenum = g_texinfo;
        f->contents = contents;
        f->numpoints = numpoints;
        f->next = validfaces[planenum];
        validfaces[planenum] = f;

        SetFaceType(f);

        for (i = 0; i < f->numpoints; i++)
        {
            line++;
            r = fscanf(file, "%lf %lf %lf\n", &v[0], &v[1], &v[2]);
            if (r != 3)
            {
                Error("::ReadSurfs (face_normal), fscanf of points failed at line %i", line);
            }
            VectorCopy(v, f->pts[i]);
#ifdef HLCSG_HLBSP_DOUBLEPLANE
			 if (DEVELOPER_LEVEL_MEGASPAM <= g_developer)
			 {
				const dplane_t *plane = &g_dplanes[f->planenum];
				inaccuracy = fabs (DotProduct (f->pts[i], plane->normal) - plane->dist);
				inaccuracy_count++;
				inaccuracy_total += inaccuracy;
				inaccuracy_max = qmax (inaccuracy, inaccuracy_max);
			}
#endif
        }
        fscanf(file, "\n");
    }

    return SurflistFromValidFaces();
}
#ifdef ZHLT_DETAILBRUSH
static brush_t *ReadBrushes (FILE *file)
{
	brush_t *brushes = NULL;
	while (1)
	{
#ifdef HLBSP_REMOVEHULL2
		if (file == brushfiles[2] && g_nohull2)
			break;
#endif
		int r;
		int brushinfo;
		r = fscanf (file, "%i\n", &brushinfo);
		if (r == 0 || r == -1)
		{
			if (brushes == NULL)
			{
				Error ("ReadBrushes: no more models");
			}
			else
			{
				Error ("ReadBrushes: file end");
			}
		}
		if (brushinfo == -1)
		{
			break;
		}
		brush_t *b;
		b = AllocBrush ();
		b->next = brushes;
		brushes = b;
		side_t **psn;
		psn = &b->sides;
		while (1)
		{
			int planenum;
			int numpoints;
			r = fscanf (file, "%i %u\n", &planenum, &numpoints);
			if (r != 2)
			{
				Error ("ReadBrushes: get side failed");
			}
			if (planenum == -1)
			{
				break;
			}
			side_t *s;
			s = AllocSide ();
			s->plane = g_dplanes[planenum ^ 1];
			s->w = new Winding (numpoints);
			int x;
			for (x = 0; x < numpoints; x++)
			{
				double v[3];
				r = fscanf (file, "%lf %lf %lf\n", &v[0], &v[1], &v[2]);
				if (r != 3)
				{
					Error ("ReadBrushes: get point failed");
				}
				VectorCopy (v, s->w->m_Points[numpoints - 1 - x]);
			}
			s->next = NULL;
			*psn = s;
			psn = &s->next;
		}
	}
	return brushes;
}
#endif


// =====================================================================================
//  ProcessModel
// =====================================================================================
static bool     ProcessModel()
{
    surfchain_t*    surfs;
#ifdef ZHLT_DETAILBRUSH
	brush_t			*detailbrushes;
#endif
    node_t*         nodes;
    dmodel_t*       model;
    int             startleafs;

    surfs = ReadSurfs(polyfiles[0]);

    if (!surfs)
        return false;                                      // all models are done
#ifdef ZHLT_DETAILBRUSH
	detailbrushes = ReadBrushes (brushfiles[0]);
#endif

    hlassume(g_nummodels < MAX_MAP_MODELS, assume_MAX_MAP_MODELS);

    startleafs = g_numleafs;
    int modnum = g_nummodels;
    model = &g_dmodels[modnum];
    g_nummodels++;

//    Log("ProcessModel: %i (%i f)\n", modnum, model->numfaces);

	g_hullnum = 0; //vluzacn
#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
	VectorFill (model->mins, 99999);
	VectorFill (model->maxs, -99999);
	{
		if (surfs->mins[0] > surfs->maxs[0])
		{
			Developer (DEVELOPER_LEVEL_FLUFF, "model %d hull %d empty\n", modnum, g_hullnum);
		}
		else
		{
			vec3_t mins, maxs;
			int i;
			VectorSubtract (surfs->mins, g_hull_size[g_hullnum][0], mins);
			VectorSubtract (surfs->maxs, g_hull_size[g_hullnum][1], maxs);
			for (i = 0; i < 3; i++)
			{
				if (mins[i] > maxs[i])
				{
					vec_t tmp;
					tmp = (mins[i] + maxs[i]) / 2;
					mins[i] = tmp;
					maxs[i] = tmp;
				}
			}
			for (i = 0; i < 3; i++)
			{
				model->maxs[i] = qmax (model->maxs[i], maxs[i]);
				model->mins[i] = qmin (model->mins[i], mins[i]);
			}
		}
	}
#else
    VectorCopy(surfs->mins, model->mins);
    VectorCopy(surfs->maxs, model->maxs);
#endif

    // SolidBSP generates a node tree
    nodes = SolidBSP(surfs,
#ifdef ZHLT_DETAILBRUSH
		detailbrushes,
#endif
		modnum==0);

    // build all the portals in the bsp tree
    // some portals are solid polygons, and some are paths to other leafs
    if (g_nummodels == 1 && !g_nofill)                       // assume non-world bmodels are simple
    {
#ifdef HLBSP_FILL
		if (!g_noinsidefill)
			FillInside (nodes);
#endif
        nodes = FillOutside(nodes, (g_bLeaked != true), 0);                  // make a leakfile if bad
    }

    FreePortals(nodes);

    // fix tjunctions
    tjunc(nodes);

    MakeFaceEdges();

    // emit the faces for the bsp file
    model->headnode[0] = g_numnodes;
    model->firstface = g_numfaces;
#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
	bool novisiblebrushes = false;
	// model->headnode[0]<0 will crash HL, so must split it.
	if (nodes->planenum == -1)
	{
		novisiblebrushes = true;
		if (nodes->markfaces[0] != NULL)
			hlassume(false, assume_EmptySolid);
		if (g_numplanes == 0)
			Error ("No valid planes.\n");
		nodes->planenum = 0; // arbitrary plane
		nodes->children[0] = AllocNode ();
		nodes->children[0]->planenum = -1;
		nodes->children[0]->contents = CONTENTS_EMPTY;
#ifdef ZHLT_DETAILBRUSH
		nodes->children[0]->isdetail = false;
		nodes->children[0]->isportalleaf = true;
		nodes->children[0]->iscontentsdetail = false;
#endif
		nodes->children[0]->faces = NULL;
		nodes->children[0]->markfaces = (face_t**)calloc (1, sizeof(face_t*));
		VectorFill (nodes->children[0]->mins, 0);
		VectorFill (nodes->children[0]->maxs, 0);
		nodes->children[1] = AllocNode ();
		nodes->children[1]->planenum = -1;
		nodes->children[1]->contents = CONTENTS_EMPTY;
#ifdef ZHLT_DETAILBRUSH
		nodes->children[1]->isdetail = false;
		nodes->children[1]->isportalleaf = true;
		nodes->children[1]->iscontentsdetail = false;
#endif
		nodes->children[1]->faces = NULL;
		nodes->children[1]->markfaces = (face_t**)calloc (1, sizeof(face_t*));
		VectorFill (nodes->children[1]->mins, 0);
		VectorFill (nodes->children[1]->maxs, 0);
		nodes->contents = 0;
#ifdef ZHLT_DETAILBRUSH
		nodes->isdetail = false;
		nodes->isportalleaf = false;
#endif
		nodes->faces = NULL;
		nodes->markfaces = NULL;
		VectorFill (nodes->mins, 0);
		VectorFill (nodes->maxs, 0);
	}
#endif
    WriteDrawNodes(nodes);
    model->numfaces = g_numfaces - model->firstface;
    model->visleafs = g_numleafs - startleafs;

    if (g_noclip)
    {
		/*
			KGP 12/31/03 - store empty content type in headnode pointers to signify
			lack of clipping information in a way that doesn't crash the half-life
			engine at runtime.
		*/
		model->headnode[1] = CONTENTS_EMPTY;
		model->headnode[2] = CONTENTS_EMPTY;
		model->headnode[3] = CONTENTS_EMPTY;
#if defined (HLCSG_HLBSP_CUSTOMBOUNDINGBOX) || defined (HLCSG_HLBSP_ALLOWEMPTYENTITY)
		goto skipclip;
#else
        return true;
#endif
    }

    // the clipping hulls are simpler
    for (g_hullnum = 1; g_hullnum < NUM_HULLS; g_hullnum++)
    {
        surfs = ReadSurfs(polyfiles[g_hullnum]);
#ifdef ZHLT_DETAILBRUSH
		detailbrushes = ReadBrushes (brushfiles[g_hullnum]);
#endif
#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
		{
			int hullnum = g_hullnum;
			if (surfs->mins[0] > surfs->maxs[0])
			{
				Developer (DEVELOPER_LEVEL_MESSAGE, "model %d hull %d empty\n", modnum, hullnum);
			}
			else
			{
				vec3_t mins, maxs;
				int i;
				VectorSubtract (surfs->mins, g_hull_size[hullnum][0], mins);
				VectorSubtract (surfs->maxs, g_hull_size[hullnum][1], maxs);
				for (i = 0; i < 3; i++)
				{
					if (mins[i] > maxs[i])
					{
						vec_t tmp;
						tmp = (mins[i] + maxs[i]) / 2;
						mins[i] = tmp;
						maxs[i] = tmp;
					}
				}
				for (i = 0; i < 3; i++)
				{
					model->maxs[i] = qmax (model->maxs[i], maxs[i]);
					model->mins[i] = qmin (model->mins[i], mins[i]);
				}
			}
		}
#endif
        nodes = SolidBSP(surfs,
#ifdef ZHLT_DETAILBRUSH
			detailbrushes, 
#endif
			modnum==0);
        if (g_nummodels == 1 && !g_nofill)                   // assume non-world bmodels are simple
        {
            nodes = FillOutside(nodes, (g_bLeaked != true), g_hullnum);
        }
        FreePortals(nodes);
		/*
			KGP 12/31/03 - need to test that the head clip node isn't empty; if it is
			we need to set model->headnode equal to the content type of the head, or create
			a trivial single-node case where the content type is the same for both leaves
			if setting the content type is invalid.
		*/
		if(nodes->planenum == -1) //empty!
		{
			model->headnode[g_hullnum] = nodes->contents;
		}
		else
		{
#ifdef ZHLT_XASH2
	        model->headnode[g_hullnum] = g_numclipnodes[g_hullnum - 1];
#else
	        model->headnode[g_hullnum] = g_numclipnodes;
#endif
		    WriteClipNodes(nodes);
		}
    }
#if defined (HLCSG_HLBSP_CUSTOMBOUNDINGBOX) || defined (HLCSG_HLBSP_ALLOWEMPTYENTITY)
	skipclip:
#endif

#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
	{
		entity_t *ent;
		ent = EntityForModel (modnum);
		if (ent != &g_entities[0] && *ValueForKey (ent, "zhlt_minsmaxs"))
		{
			double origin[3], mins[3], maxs[3];
			VectorClear (origin);
			sscanf (ValueForKey (ent, "origin"), "%lf %lf %lf", &origin[0], &origin[1], &origin[2]);
			if (sscanf (ValueForKey (ent, "zhlt_minsmaxs"), "%lf %lf %lf %lf %lf %lf", &mins[0], &mins[1], &mins[2], &maxs[0], &maxs[1], &maxs[2]) == 6)
			{
				VectorSubtract (mins, origin, model->mins);
				VectorSubtract (maxs, origin, model->maxs);
			}
		}
	}
#endif
#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
	Developer (DEVELOPER_LEVEL_MESSAGE, "model %d - mins=(%g,%g,%g) maxs=(%g,%g,%g)\n", modnum,
		model->mins[0], model->mins[1], model->mins[2], model->maxs[0], model->maxs[1], model->maxs[2]);
	if (model->mins[0] > model->maxs[0])
	{
		entity_t *ent = EntityForModel (g_nummodels - 1);
		if (g_nummodels - 1 != 0 && ent == &g_entities[0])
		{
			ent = NULL;
		}
		Warning ("Empty solid entity: model %d (entity: classname \"%s\", origin \"%s\", targetname \"%s\")", 
			g_nummodels - 1, 
			(ent? ValueForKey (ent, "classname"): "unknown"), 
			(ent? ValueForKey (ent, "origin"): "unknown"), 
			(ent? ValueForKey (ent, "targetname"): "unknown"));
		VectorClear (model->mins); // fix "backward minsmaxs" in HL
		VectorClear (model->maxs);
	}
	else if (novisiblebrushes)
	{
		entity_t *ent = EntityForModel (g_nummodels - 1);
		if (g_nummodels - 1 != 0 && ent == &g_entities[0])
		{
			ent = NULL;
		}
		Warning ("No visible brushes in solid entity: model %d (entity: classname \"%s\", origin \"%s\", targetname \"%s\", range (%.0f,%.0f,%.0f) - (%.0f,%.0f,%.0f))", 
			g_nummodels - 1, 
			(ent? ValueForKey (ent, "classname"): "unknown"), 
			(ent? ValueForKey (ent, "origin"): "unknown"), 
			(ent? ValueForKey (ent, "targetname"): "unknown"), 
			model->mins[0], model->mins[1], model->mins[2], model->maxs[0], model->maxs[1], model->maxs[2]);
	}
#endif
    return true;
}

// =====================================================================================
//  Usage
// =====================================================================================
static void     Usage()
{
    Banner();

    Log("\n-= %s Options =-\n\n", g_Program);
#ifdef ZHLT_CONSOLE
	Log("    -console #     : Set to 0 to turn off the pop-up console (default is 1)\n");
#endif
#ifdef ZHLT_LANGFILE
	Log("    -lang file     : localization file\n");
#endif
    Log("    -leakonly      : Run BSP only enough to check for LEAKs\n");
    Log("    -subdivide #   : Sets the face subdivide size\n");
    Log("    -maxnodesize # : Sets the maximum portal node size\n\n");
    Log("    -notjunc       : Don't break edges on t-junctions     (not for final runs)\n");
#ifdef HLBSP_BRINKHACK
	Log("    -nobrink       : Don't smooth brinks                  (not for final runs)\n");
#endif
    Log("    -noclip        : Don't process the clipping hull      (not for final runs)\n");
    Log("    -nofill        : Don't fill outside (will mask LEAKs) (not for final runs)\n");
#ifdef HLBSP_FILL
	Log("    -noinsidefill  : Don't fill empty spaces\n");
#endif
	Log("    -noopt         : Don't optimize planes on BSP write   (not for final runs)\n");
#ifdef HLBSP_MERGECLIPNODE
	Log("    -noclipnodemerge: Don't optimize clipnodes\n");
#endif
    Log("    -texdata #     : Alter maximum texture memory limit (in kb)\n");
    Log("    -lightdata #   : Alter maximum lighting memory limit (in kb)\n");
    Log("    -chart         : display bsp statitics\n");
    Log("    -low | -high   : run program an altered priority level\n");
    Log("    -nolog         : don't generate the compile logfiles\n");
    Log("    -threads #     : manually specify the number of threads to run\n");
#ifdef SYSTEM_WIN32
    Log("    -estimate      : display estimated time during compile\n");
#endif
#ifdef ZHLT_PROGRESSFILE // AJM
    Log("    -progressfile path  : specify the path to a file for progress estimate output\n");
#endif
#ifdef SYSTEM_POSIX
    Log("    -noestimate    : do not display continuous compile time estimates\n");
#endif

#ifdef ZHLT_NULLTEX         // AJM
    Log("    -nonulltex     : Don't strip NULL faces\n");
#endif

#ifdef ZHLT_DETAIL // AJM
    Log("    -nodetail      : don't handle detail brushes\n");
#endif

#ifdef HLBSP_REMOVEHULL2
	Log("    -nohull2       : Don't generate hull 2 (the clipping hull for large monsters and pushables)\n");
#endif

#ifdef HLBSP_VIEWPORTAL
	Log("    -viewportal    : Show portal boundaries in 'mapname_portal.pts' file\n");
#endif

    Log("    -verbose       : compile with verbose messages\n");
    Log("    -noinfo        : Do not show tool configuration information\n");
    Log("    -dev #         : compile with developer message\n\n");
    Log("    mapfile        : The mapfile to compile\n\n");

    exit(1);
}

// =====================================================================================
//  Settings
// =====================================================================================
static void     Settings()
{
    char*           tmp;

    if (!g_info)
        return;

    Log("\nCurrent %s Settings\n", g_Program);
    Log("Name               |  Setting  |  Default\n" "-------------------|-----------|-------------------------\n");

    // ZHLT Common Settings
    if (DEFAULT_NUMTHREADS == -1)
    {
        Log("threads             [ %7d ] [  Varies ]\n", g_numthreads);
    }
    else
    {
        Log("threads             [ %7d ] [ %7d ]\n", g_numthreads, DEFAULT_NUMTHREADS);
    }

    Log("verbose             [ %7s ] [ %7s ]\n", g_verbose ? "on" : "off", DEFAULT_VERBOSE ? "on" : "off");
    Log("log                 [ %7s ] [ %7s ]\n", g_log ? "on" : "off", DEFAULT_LOG ? "on" : "off");
    Log("developer           [ %7d ] [ %7d ]\n", g_developer, DEFAULT_DEVELOPER);
    Log("chart               [ %7s ] [ %7s ]\n", g_chart ? "on" : "off", DEFAULT_CHART ? "on" : "off");
    Log("estimate            [ %7s ] [ %7s ]\n", g_estimate ? "on" : "off", DEFAULT_ESTIMATE ? "on" : "off");
    Log("max texture memory  [ %7d ] [ %7d ]\n", g_max_map_miptex, DEFAULT_MAX_MAP_MIPTEX);

    switch (g_threadpriority)
    {
    case eThreadPriorityNormal:
    default:
        tmp = "Normal";
        break;
    case eThreadPriorityLow:
        tmp = "Low";
        break;
    case eThreadPriorityHigh:
        tmp = "High";
        break;
    }
    Log("priority            [ %7s ] [ %7s ]\n", tmp, "Normal");
    Log("\n");

    // HLBSP Specific Settings
    Log("noclip              [ %7s ] [ %7s ]\n", g_noclip ? "on" : "off", DEFAULT_NOCLIP ? "on" : "off");
    Log("nofill              [ %7s ] [ %7s ]\n", g_nofill ? "on" : "off", DEFAULT_NOFILL ? "on" : "off");
#ifdef HLBSP_FILL
	Log("noinsidefill        [ %7s ] [ %7s ]\n", g_noinsidefill ? "on" : "off", DEFAULT_NOINSIDEFILL ? "on" : "off");
#endif
	Log("noopt               [ %7s ] [ %7s ]\n", g_noopt ? "on" : "off", DEFAULT_NOOPT ? "on" : "off");
#ifdef HLBSP_MERGECLIPNODE
	Log("no clipnode merging [ %7s ] [ %7s ]\n", g_noclipnodemerge? "on": "off", DEFAULT_NOCLIPNODEMERGE? "on": "off");
#endif
#ifdef ZHLT_NULLTEX // AJM
    Log("null tex. stripping [ %7s ] [ %7s ]\n", g_bUseNullTex ? "on" : "off", DEFAULT_NULLTEX ? "on" : "off" );
#endif
#ifdef ZHLT_DETAIL // AJM
    Log("detail brushes      [ %7s ] [ %7s ]\n", g_bDetailBrushes ? "on" : "off", DEFAULT_DETAIL ? "on" : "off" );
#endif
    Log("notjunc             [ %7s ] [ %7s ]\n", g_notjunc ? "on" : "off", DEFAULT_NOTJUNC ? "on" : "off");
#ifdef HLBSP_BRINKHACK
	Log("nobrink             [ %7s ] [ %7s ]\n", g_nobrink? "on": "off", DEFAULT_NOBRINK? "on": "off");
#endif
    Log("subdivide size      [ %7d ] [ %7d ] (Min %d) (Max %d)\n",
        g_subdivide_size, DEFAULT_SUBDIVIDE_SIZE, MIN_SUBDIVIDE_SIZE, MAX_SUBDIVIDE_SIZE);
    Log("max node size       [ %7d ] [ %7d ] (Min %d) (Max %d)\n",
        g_maxnode_size, DEFAULT_MAXNODE_SIZE, MIN_MAXNODE_SIZE, MAX_MAXNODE_SIZE);
#ifdef HLBSP_REMOVEHULL2
	Log("remove hull 2       [ %7s ] [ %7s ]\n", g_nohull2? "on": "off", "off");
#endif
    Log("\n\n");
}

// =====================================================================================
//  ProcessFile
// =====================================================================================
static void     ProcessFile(const char* const filename)
{
    int             i;
    char            name[_MAX_PATH];

    // delete existing files
    safe_snprintf(g_portfilename, _MAX_PATH, "%s.prt", filename);
    unlink(g_portfilename);

    safe_snprintf(g_pointfilename, _MAX_PATH, "%s.pts", filename);
    unlink(g_pointfilename);

    safe_snprintf(g_linefilename, _MAX_PATH, "%s.lin", filename);
    unlink(g_linefilename);

#ifdef ZHLT_64BIT_FIX
	safe_snprintf (g_extentfilename, _MAX_PATH, "%s.ext", filename);
	unlink (g_extentfilename);
#endif
    // open the hull files
    for (i = 0; i < NUM_HULLS; i++)
    {
                   //mapname.p[0-3]
		sprintf(name, "%s.p%i", filename, i);
        polyfiles[i] = fopen(name, "r");

        if (!polyfiles[i])
            Error("Can't open %s", name);
#ifdef ZHLT_DETAILBRUSH
		sprintf(name, "%s.b%i", filename, i);
		brushfiles[i] = fopen(name, "r");
		if (!brushfiles[i])
			Error("Can't open %s", name);
#endif
    }
#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
	{
		FILE			*f;
		char			name[_MAX_PATH];
		safe_snprintf (name, _MAX_PATH, "%s.hsz", filename);
		f = fopen (name, "r");
		if (!f)
		{
			Warning("Couldn't open %s", name);
		}
		else
		{
			float x1,y1,z1;
			float x2,y2,z2;
			for (i = 0; i < NUM_HULLS; i++)
			{
				int count;
				count = fscanf (f, "%f %f %f %f %f %f\n", &x1, &y1, &z1, &x2, &y2, &z2);
				if (count != 6)
				{
					Error ("Load hull size (line %i): scanf failure", i+1);
				}
				g_hull_size[i][0][0] = x1;
				g_hull_size[i][0][1] = y1;
				g_hull_size[i][0][2] = z1;
				g_hull_size[i][1][0] = x2;
				g_hull_size[i][1][1] = y2;
				g_hull_size[i][1][2] = z2;
			}
			fclose (f);
		}
	}
#endif

    // load the output of csg
    safe_snprintf(g_bspfilename, _MAX_PATH, "%s.bsp", filename);
    LoadBSPFile(g_bspfilename);
    ParseEntities();

    Settings(); // AJM: moved here due to info_compile_parameters entity

#ifdef HLCSG_HLBSP_DOUBLEPLANE
	{
		char name[_MAX_PATH];
		safe_snprintf (name, _MAX_PATH, "%s.pln", filename);
		FILE *planefile = fopen (name, "rb");
		if (!planefile)
		{
			Warning("Couldn't open %s", name);
#undef dplane_t
#undef g_dplanes
			for (i = 0; i < g_numplanes; i++)
			{
				plane_t *mp = &g_mapplanes[i];
				dplane_t *dp = &g_dplanes[i];
				VectorCopy (dp->normal, mp->normal);
				mp->dist = dp->dist;
				mp->type = dp->type;
			}
#define dplane_t plane_t
#define g_dplanes g_mapplanes
		}
		else
		{
			if (q_filelength (planefile) != int(g_numplanes * sizeof (dplane_t)))
			{
				Error ("Invalid plane data");
			}
			SafeRead (planefile, g_dplanes, g_numplanes * sizeof (dplane_t));
			fclose (planefile);
		}
	}
#endif
    // init the tables to be shared by all models
    BeginBSPFile();

    // process each model individually
    while (ProcessModel())
        ;

    // write the updated bsp file out
    FinishBSPFile();
#ifdef HLBSP_DELETETEMPFILES

	// Because the bsp file has been updated, these polyfiles are no longer valid.
    for (i = 0; i < NUM_HULLS; i++)
    {
		sprintf (name, "%s.p%i", filename, i);
		fclose (polyfiles[i]);
		polyfiles[i] = NULL;
		unlink (name);
#ifdef ZHLT_DETAILBRUSH
		sprintf(name, "%s.b%i", filename, i);
		fclose (brushfiles[i]);
		brushfiles[i] = NULL;
		unlink (name);
#endif
    }
#ifdef HLCSG_HLBSP_ALLOWEMPTYENTITY
	safe_snprintf (name, _MAX_PATH, "%s.hsz", filename);
	unlink (name);
#endif
#ifdef HLCSG_HLBSP_DOUBLEPLANE
	safe_snprintf (name, _MAX_PATH, "%s.pln", filename);
	unlink (name);
#endif
#endif
}

// =====================================================================================
//  main
// =====================================================================================
int             main(const int argc, char** argv)
{
    int             i;
    double          start, end;
    const char*     mapname_from_arg = NULL;

    g_Program = "hlbsp";

#ifdef ZHLT_PARAMFILE
	int argcold = argc;
	char ** argvold = argv;
	{
		int argc;
		char ** argv;
		ParseParamFile (argcold, argvold, argc, argv);
		{
#endif
#ifdef ZHLT_CONSOLE
	if (InitConsole (argc, argv) < 0)
		Usage();
#endif
    // if we dont have any command line argvars, print out usage and die
    if (argc == 1)
        Usage();

    // check command line args
    for (i = 1; i < argc; i++)
    {
        if (!strcasecmp(argv[i], "-threads"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                int             g_numthreads = atoi(argv[++i]);

                if (g_numthreads < 1)
                {
                    Log("Expected value of at least 1 for '-threads'\n");
                    Usage();
                }
            }
            else
            {
                Usage();
            }
        }
#ifdef ZHLT_CONSOLE
		else if (!strcasecmp(argv[i], "-console"))
		{
#ifndef SYSTEM_WIN32
			Warning("The option '-console #' is only valid for Windows.");
#endif
			if (i + 1 < argc)
				++i;
			else
				Usage();
		}
#endif
        else if (!strcasecmp(argv[i], "-notjunc"))
        {
            g_notjunc = true;
        }
#ifdef HLBSP_BRINKHACK
		else if (!strcasecmp (argv[i], "-nobrink"))
		{
			g_nobrink = true;
		}
#endif
        else if (!strcasecmp(argv[i], "-noclip"))
        {
            g_noclip = true;
        }
        else if (!strcasecmp(argv[i], "-nofill"))
        {
            g_nofill = true;
        }
#ifdef HLBSP_FILL
        else if (!strcasecmp(argv[i], "-noinsidefill"))
        {
            g_noinsidefill = true;
        }
#endif

#ifdef SYSTEM_WIN32
        else if (!strcasecmp(argv[i], "-estimate"))
        {
            g_estimate = true;
        }
#endif

#ifdef SYSTEM_POSIX
        else if (!strcasecmp(argv[i], "-noestimate"))
        {
            g_estimate = false;
        }
#endif

#ifdef ZHLT_NETVIS
        else if (!strcasecmp(argv[i], "-client"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_clientid = atoi(argv[++i]);
            }
            else
            {
                Usage();
            }
        }
#endif

#ifdef ZHLT_PROGRESSFILE // AJM
        else if (!strcasecmp(argv[i], "-progressfile"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_progressfile = argv[++i];
            }
            else
            {
            	Log("Error: -progressfile: expected path to progress file following parameter\n");
                Usage();
            }
        }
#endif

        else if (!strcasecmp(argv[i], "-dev"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_developer = (developer_level_t)atoi(argv[++i]);
            }
            else
            {
                Usage();
            }
        }
        else if (!strcasecmp(argv[i], "-verbose"))
        {
            g_verbose = true;
        }
        else if (!strcasecmp(argv[i], "-noinfo"))
        {
            g_info = false;
        }
        else if (!strcasecmp(argv[i], "-leakonly"))
        {
            g_bLeakOnly = true;
        }
        else if (!strcasecmp(argv[i], "-chart"))
        {
            g_chart = true;
        }
        else if (!strcasecmp(argv[i], "-low"))
        {
            g_threadpriority = eThreadPriorityLow;
        }
        else if (!strcasecmp(argv[i], "-high"))
        {
            g_threadpriority = eThreadPriorityHigh;
        }
        else if (!strcasecmp(argv[i], "-nolog"))
        {
            g_log = false;
        }

#ifdef ZHLT_NULLTEX // AJM
        else if (!strcasecmp(argv[i], "-nonulltex"))
        {
            g_bUseNullTex = false;
        }
#endif

#ifdef ZHLT_DETAIL // AJM
        else if (!strcasecmp(argv[i], "-nodetail"))
        {
            g_bDetailBrushes = false;
        }
#endif

#ifdef HLBSP_REMOVEHULL2
		else if (!strcasecmp (argv[i], "-nohull2"))
		{
			g_nohull2 = true;
		}
#endif

		else if (!strcasecmp(argv[i], "-noopt"))
		{
			g_noopt = true;
		}
#ifdef HLBSP_MERGECLIPNODE
		else if (!strcasecmp (argv[i], "-noclipnodemerge"))
		{
			g_noclipnodemerge = true;
		}
#endif
        else if (!strcasecmp(argv[i], "-subdivide"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_subdivide_size = atoi(argv[++i]);
                if (g_subdivide_size > MAX_SUBDIVIDE_SIZE)
                {
                    Warning
                        ("Maximum value for subdivide size is %i, '-subdivide %i' ignored",
                         MAX_SUBDIVIDE_SIZE, g_subdivide_size);
                    g_subdivide_size = MAX_SUBDIVIDE_SIZE;
                }
                else if (g_subdivide_size < MIN_SUBDIVIDE_SIZE)
                {
                    Warning
                        ("Mininum value for subdivide size is %i, '-subdivide %i' ignored",
                         MIN_SUBDIVIDE_SIZE, g_subdivide_size);
                    g_subdivide_size = MIN_SUBDIVIDE_SIZE; //MAX_SUBDIVIDE_SIZE; //--vluzacn
                }
            }
            else
            {
                Usage();
            }
        }
        else if (!strcasecmp(argv[i], "-maxnodesize"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_maxnode_size = atoi(argv[++i]);
                if (g_maxnode_size > MAX_MAXNODE_SIZE)
                {
                    Warning
                        ("Maximum value for max node size is %i, '-maxnodesize %i' ignored",
                         MAX_MAXNODE_SIZE, g_maxnode_size);
                    g_maxnode_size = MAX_MAXNODE_SIZE;
                }
                else if (g_maxnode_size < MIN_MAXNODE_SIZE)
                {
                    Warning
                        ("Mininimum value for max node size is %i, '-maxnodesize %i' ignored",
                         MIN_MAXNODE_SIZE, g_maxnode_size);
                    g_maxnode_size = MIN_MAXNODE_SIZE; //MAX_MAXNODE_SIZE; //vluzacn
                }
            }
            else
            {
                Usage();
            }
        }
#ifdef HLBSP_VIEWPORTAL
		else if (!strcasecmp (argv[i], "-viewportal"))
		{
			g_viewportal = true;
		}
#endif
        else if (!strcasecmp(argv[i], "-texdata"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                int             x = atoi(argv[++i]) * 1024;

                //if (x > g_max_map_miptex) //--vluzacn
                {
                    g_max_map_miptex = x;
                }
            }
            else
            {
                Usage();
            }
        }
        else if (!strcasecmp(argv[i], "-lightdata"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                int             x = atoi(argv[++i]) * 1024;

                //if (x > g_max_map_lightdata) //--vluzacn
                {
                    g_max_map_lightdata = x;
                }
            }
            else
            {
                Usage();
            }
        }
#ifdef ZHLT_LANGFILE
		else if (!strcasecmp (argv[i], "-lang"))
		{
			if (i + 1 < argc)
			{
				char tmp[_MAX_PATH];
#ifdef SYSTEM_WIN32
				GetModuleFileName (NULL, tmp, _MAX_PATH);
#else
				safe_strncpy (tmp, argv[0], _MAX_PATH);
#endif
				LoadLangFile (argv[++i], tmp);
			}
			else
			{
				Usage();
			}
		}
#endif
        else if (argv[i][0] == '-')
        {
            Log("Unknown option \"%s\"\n", argv[i]);
            Usage();
        }
        else if (!mapname_from_arg)
        {
            mapname_from_arg = argv[i];
        }
        else
        {
            Log("Unknown option \"%s\"\n", argv[i]);
            Usage();
        }
    }
#ifdef HLBSP_SUBDIVIDE_INMID
	if (g_subdivide_size % TEXTURE_STEP != 0)
	{
		Warning ("Subdivide size must be a multiple of %d", (int)TEXTURE_STEP);
		g_subdivide_size = TEXTURE_STEP * (g_subdivide_size / TEXTURE_STEP);
	}
#endif

    if (!mapname_from_arg)
    {
        Log("No mapfile specified\n");
        Usage();
    }

    safe_strncpy(g_Mapname, mapname_from_arg, _MAX_PATH);
    FlipSlashes(g_Mapname);
    StripExtension(g_Mapname);
    OpenLog(g_clientid);
    atexit(CloseLog);
    ThreadSetDefault();
    ThreadSetPriority(g_threadpriority);
#ifdef ZHLT_PARAMFILE
    LogStart(argcold, argvold);
	{
		int			 i;
		Log("Arguments: ");
		for (i = 1; i < argc; i++)
		{
			if (strchr(argv[i], ' '))
			{
				Log("\"%s\" ", argv[i]);
			}
			else
			{
				Log("%s ", argv[i]);
			}
		}
		Log("\n");
	}
#else
    LogStart(argc, argv);
#endif

    CheckForErrorLog();

#ifdef ZHLT_64BIT_FIX
#ifdef PLATFORM_CAN_CALC_EXTENT
	hlassume (CalcFaceExtents_test (), assume_first);
#endif
#endif
    dtexdata_init();
    atexit(dtexdata_free);
    //Settings();
    // END INIT

    // Load the .void files for allowable entities in the void
    {
#ifndef ZHLT_DEFAULTEXTENSION_FIX
        char            g_source[_MAX_PATH];
#endif
        char            strSystemEntitiesVoidFile[_MAX_PATH];
        char            strMapEntitiesVoidFile[_MAX_PATH];

#ifndef ZHLT_DEFAULTEXTENSION_FIX
        safe_strncpy(g_source, mapname_from_arg, _MAX_PATH);
        StripExtension(g_source);
#endif

        // try looking in the current directory
        safe_strncpy(strSystemEntitiesVoidFile, ENTITIES_VOID, _MAX_PATH);
        if (!q_exists(strSystemEntitiesVoidFile))
        {
            char tmp[_MAX_PATH];
            // try looking in the directory we were run from
#ifdef SYSTEM_WIN32
            GetModuleFileName(NULL, tmp, _MAX_PATH);
#else
            safe_strncpy(tmp, argv[0], _MAX_PATH);
#endif
            ExtractFilePath(tmp, strSystemEntitiesVoidFile);
            safe_strncat(strSystemEntitiesVoidFile, ENTITIES_VOID, _MAX_PATH);
        }

        // Set the optional level specific lights filename
#ifdef ZHLT_DEFAULTEXTENSION_FIX
		safe_snprintf(strMapEntitiesVoidFile, _MAX_PATH, "%s" ENTITIES_VOID_EXT, g_Mapname);
#else
        safe_strncpy(strMapEntitiesVoidFile, g_source, _MAX_PATH);
        DefaultExtension(strMapEntitiesVoidFile, ENTITIES_VOID_EXT);
#endif

        LoadAllowableOutsideList(strSystemEntitiesVoidFile);    // default entities.void
        if (*strMapEntitiesVoidFile)
        {
            LoadAllowableOutsideList(strMapEntitiesVoidFile);   // automatic mapname.void
        }
    }

    // BEGIN BSP
    start = I_FloatTime();

    ProcessFile(g_Mapname);

    end = I_FloatTime();
    LogTimeElapsed(end - start);
    // END BSP

    FreeAllowableOutsideList();

#ifdef ZHLT_PARAMFILE
		}
	}
#endif
    return 0;
}

