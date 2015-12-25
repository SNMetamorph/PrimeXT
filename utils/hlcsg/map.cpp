//#pragma warning(disable: 4018) // '<' : signed/unsigned mismatch

#include "csg.h"

int             g_nummapbrushes;
brush_t         g_mapbrushes[MAX_MAP_BRUSHES];

int             g_numbrushsides;
side_t          g_brushsides[MAX_MAP_SIDES];

int             g_nMapFileVersion;

static const vec3_t   s_baseaxis[18] = {
    {0, 0, 1}, {1, 0, 0}, {0, -1, 0},                      // floor
    {0, 0, -1}, {1, 0, 0}, {0, -1, 0},                     // ceiling
    {1, 0, 0}, {0, 1, 0}, {0, 0, -1},                      // west wall
    {-1, 0, 0}, {0, 1, 0}, {0, 0, -1},                     // east wall
    {0, 1, 0}, {1, 0, 0}, {0, 0, -1},                      // south wall
    {0, -1, 0}, {1, 0, 0}, {0, 0, -1},                     // north wall
};

#ifdef HLCSG_COUNT_NEW
int				g_numparsedentities;
int				g_numparsedbrushes;
#endif

#ifdef HLCSG_COPYBRUSH
brush_t *CopyCurrentBrush (entity_t *entity, const brush_t *brush)
{
	if (entity->firstbrush + entity->numbrushes != g_nummapbrushes)
	{
		Error ("CopyCurrentBrush: internal error.");
	}
	brush_t *newb = &g_mapbrushes[g_nummapbrushes];
	g_nummapbrushes++;
	hlassume (g_nummapbrushes <= MAX_MAP_BRUSHES, assume_MAX_MAP_BRUSHES);
	memcpy (newb, brush, sizeof (brush_t));
	newb->firstside = g_numbrushsides;
	g_numbrushsides += brush->numsides;
	hlassume (g_numbrushsides <= MAX_MAP_SIDES, assume_MAX_MAP_SIDES);
	memcpy (&g_brushsides[newb->firstside], &g_brushsides[brush->firstside], brush->numsides * sizeof (side_t));
	newb->entitynum = entity - g_entities;
	newb->brushnum = entity->numbrushes;
	entity->numbrushes++;
#ifdef HLCSG_HULLBRUSH
	for (int h = 0; h < NUM_HULLS; h++)
	{
		if (brush->hullshapes[h] != NULL)
		{
			newb->hullshapes[h] = _strdup (brush->hullshapes[h]);
		}
		else
		{
			newb->hullshapes[h] = NULL;
		}
	}
#endif
	return newb;
}
#endif
#ifdef HLCSG_HULLBRUSH
void DeleteCurrentEntity (entity_t *entity)
{
	if (entity != &g_entities[g_numentities - 1])
	{
		Error ("DeleteCurrentEntity: internal error.");
	}
	if (entity->firstbrush + entity->numbrushes != g_nummapbrushes)
	{
		Error ("DeleteCurrentEntity: internal error.");
	}
	for (int i = entity->numbrushes - 1; i >= 0; i--)
	{
		brush_t *b = &g_mapbrushes[entity->firstbrush + i];
		if (b->firstside + b->numsides != g_numbrushsides)
		{
			Error ("DeleteCurrentEntity: internal error. (Entity %i, Brush %i)",
#ifdef HLCSG_COUNT_NEW
				b->originalentitynum, b->originalbrushnum
#else
				b->entitynum, b->brushnum
#endif
				);
		}
		memset (&g_brushsides[b->firstside], 0, b->numsides * sizeof (side_t));
		g_numbrushsides -= b->numsides;
		for (int h = 0; h < NUM_HULLS; h++)
		{
			if (b->hullshapes[h])
			{
				free (b->hullshapes[h]);
			}
		}
	}
	memset (&g_mapbrushes[entity->firstbrush], 0, entity->numbrushes * sizeof (brush_t));
	g_nummapbrushes -= entity->numbrushes;
	while (entity->epairs)
	{
		DeleteKey (entity, entity->epairs->key);
	}
	memset (entity, 0, sizeof(entity_t));
	g_numentities--;
}
#endif
// =====================================================================================
//  TextureAxisFromPlane
// =====================================================================================
void            TextureAxisFromPlane(const plane_t* const pln, vec3_t xv, vec3_t yv)
{
    int             bestaxis;
    vec_t           dot, best;
    int             i;

    best = 0;
    bestaxis = 0;

    for (i = 0; i < 6; i++)
    {
        dot = DotProduct(pln->normal, s_baseaxis[i * 3]);
        if (dot > best)
        {
            best = dot;
            bestaxis = i;
        }
    }

    VectorCopy(s_baseaxis[bestaxis * 3 + 1], xv);
    VectorCopy(s_baseaxis[bestaxis * 3 + 2], yv);
}

#define ScaleCorrection	(1.0/128.0)

#ifndef HLCSG_CUSTOMHULL
// =====================================================================================
//  CopySKYtoCLIP
//      clips a particluar sky brush
// =====================================================================================
static void     CopySKYtoCLIP(const brush_t* const b)
{
    int             i;
    entity_t*       mapent;
    brush_t*        newbrush;

    if (b->contents != CONTENTS_SKY)
		Error("[MOD] CopySKYtoCLIP: Got a NON-SKY for passed brush! (%s)",b->contents ); //Error("[MOD] CopySKYtoCLIP: Got a NON-SKY for passed brush! (%d)",b->contents ); //--vluzacn

    hlassert(b->contents == CONTENTS_SKY);                 // Only SKY brushes should be passed down to this function(sanity check)
    hlassert(b->entitynum == 0);                           // SKY must be in worldspawn entity

    mapent = &g_entities[b->entitynum];
    mapent->numbrushes++;

    newbrush = &g_mapbrushes[g_nummapbrushes];
#ifdef HLCSG_COUNT_NEW
	newbrush->originalentitynum = b->originalentitynum;
	newbrush->originalbrushnum = b->originalbrushnum;
#endif
    newbrush->entitynum = b->entitynum;
    newbrush->brushnum = g_nummapbrushes - mapent->firstbrush;
    newbrush->firstside = g_numbrushsides;
    newbrush->numsides = b->numsides;
    newbrush->contents = CONTENTS_CLIP;
#ifdef HLCSG_CLIPECONOMY
    newbrush->noclip = 0;
#endif
#ifdef ZHLT_DETAILBRUSH
	newbrush->detaillevel = b->detaillevel;
	newbrush->chopdown = b->chopdown;
	newbrush->chopup = b->chopup;
#ifdef ZHLT_CLIPNODEDETAILLEVEL
	newbrush->clipnodedetaillevel = b->clipnodedetaillevel;
#endif
#ifdef HLCSG_COPLANARPRIORITY
	newbrush->coplanarpriority = b->coplanarpriority;
#endif
#endif

    for (i = 0; i < b->numsides; i++)
    {
        int             j;

        side_t*         side = &g_brushsides[g_numbrushsides];

        *side = g_brushsides[b->firstside + i];
#ifdef HLCSG_CUSTOMHULL
        safe_strncpy(side->td.name, "NULL", sizeof(side->td.name));
#else
        safe_strncpy(side->td.name, "CLIP", sizeof(side->td.name));
#endif

        for (j = 0; j < NUM_HULLS; j++)
        {
            newbrush->hulls[j].faces = NULL;
            newbrush->hulls[j].bounds = b->hulls[j].bounds;
        }

        g_numbrushsides++;
        hlassume(g_numbrushsides < MAX_MAP_SIDES, assume_MAX_MAP_SIDES);
    }

    g_nummapbrushes++;
    hlassume(g_nummapbrushes < MAX_MAP_BRUSHES, assume_MAX_MAP_BRUSHES);
}

// =====================================================================================
//  HandleSKYCLIP
//      clips the whole sky, unconditional of g_skyclip
// =====================================================================================
static void     HandleSKYCLIP()
{
    int             i;
    int             last;
    entity_t*       e = &g_entities[0];

    for (i = e->firstbrush, last = e->firstbrush + e->numbrushes; i < last; i++)
    {
        if (g_mapbrushes[i].contents == CONTENTS_SKY
#ifdef HLCSG_CUSTOMHULL
			&& g_mapbrushes[i].noclip == false
#endif
			)
        {
            CopySKYtoCLIP(&g_mapbrushes[i]);
        }
    }
}
#endif

// =====================================================================================
//  CheckForInvisible
//      see if a brush is part of an invisible entity (KGP)
// =====================================================================================
#ifdef HLCSG_NULLIFY_INVISIBLE
static bool CheckForInvisible(entity_t* mapent)
{
	using namespace std;

	string keyval(ValueForKey(mapent,"classname"));
	if(g_invisible_items.count(keyval))
	{ return true; }

	keyval.assign(ValueForKey(mapent,"targetname"));
	if(g_invisible_items.count(keyval))
	{ return true; }

	keyval.assign(ValueForKey(mapent,"zhlt_invisible"));
	if(!keyval.empty() && strcmp(keyval.c_str(),"0"))
	{ return true; }

	return false;
}
#endif
// =====================================================================================
//  ParseBrush
//      parse a brush from script
// =====================================================================================
#ifdef HLCSG_COPYBRUSH
static void ParseBrush(entity_t* mapent)
#else
static contents_t ParseBrush(entity_t* mapent)
#endif
{
    brush_t*        b;
    int             i, j;
    side_t*         side;
    contents_t      contents;
    bool            ok;
#ifdef HLCSG_NULLIFY_INVISIBLE // KGP
	bool nullify = CheckForInvisible(mapent);
#endif
    hlassume(g_nummapbrushes < MAX_MAP_BRUSHES, assume_MAX_MAP_BRUSHES);

    b = &g_mapbrushes[g_nummapbrushes];
    g_nummapbrushes++;
    b->firstside = g_numbrushsides;
#ifdef HLCSG_COUNT_NEW
	b->originalentitynum = g_numparsedentities;
	b->originalbrushnum = g_numparsedbrushes;
#endif
    b->entitynum = g_numentities - 1;
    b->brushnum = g_nummapbrushes - mapent->firstbrush - 1;

#ifdef HLCSG_CLIPECONOMY // AJM
    b->noclip = 0;
#endif
#ifdef HLCSG_CUSTOMHULL
	if (IntForKey(mapent, "zhlt_noclip"))
	{
		b->noclip = 1;
	}
#endif
#ifdef HLCSG_CUSTOMHULL
	b->cliphull = 0;
	b->bevel = false;
#endif
#ifdef ZHLT_DETAILBRUSH
	{
		b->detaillevel = IntForKey (mapent, "zhlt_detaillevel");
		b->chopdown = IntForKey (mapent, "zhlt_chopdown");
		b->chopup = IntForKey (mapent, "zhlt_chopup");
#ifdef ZHLT_CLIPNODEDETAILLEVEL
		b->clipnodedetaillevel = IntForKey (mapent, "zhlt_clipnodedetaillevel");
#endif
#ifdef HLCSG_COPLANARPRIORITY
		b->coplanarpriority = IntForKey (mapent, "zhlt_coplanarpriority");
#endif
		bool wrong = false;
		if (b->detaillevel < 0)
		{
			wrong = true;
			b->detaillevel = 0;
		}
		if (b->chopdown < 0)
		{
			wrong = true;
			b->chopdown = 0;
		}
		if (b->chopup < 0)
		{
			wrong = true;
			b->chopup = 0;
		}
#ifdef ZHLT_CLIPNODEDETAILLEVEL
		if (b->clipnodedetaillevel < 0)
		{
			wrong = true;
			b->clipnodedetaillevel = 0;
		}
#endif
		if (wrong)
		{
			Warning ("Entity %i, Brush %i: incorrect settings for detail brush.",
#ifdef HLCSG_COUNT_NEW
					b->originalentitynum, b->originalbrushnum
#else
					b->entitynum, b->brushnum
#endif
					);
		}
	}
#endif
#ifdef HLCSG_HULLBRUSH
	for (int h = 0; h < NUM_HULLS; h++)
	{
		char key[16];
		const char *value;
		sprintf (key, "zhlt_hull%d", h);
		value = ValueForKey (mapent, key);
		if (*value)
		{
			b->hullshapes[h] = _strdup (value);
		}
		else
		{
			b->hullshapes[h] = NULL;
		}
	}
#endif

    mapent->numbrushes++;

	ok = GetToken(true);
    while (ok)
    {
        g_TXcommand = 0;
        if (!strcmp(g_token, "}"))
        {
            break;
        }

        hlassume(g_numbrushsides < MAX_MAP_SIDES, assume_MAX_MAP_SIDES);
        side = &g_brushsides[g_numbrushsides];
        g_numbrushsides++;

        b->numsides++;

#ifdef HLCSG_CUSTOMHULL
		side->bevel = false;
#endif
#ifdef ZHLT_HIDDENSOUNDTEXTURE
		side->shouldhide = false;
#endif
        // read the three point plane definition
        for (i = 0; i < 3; i++)
        {
            if (i != 0)
            {
                GetToken(true);
            }
            if (strcmp(g_token, "("))
            {
                Error("Parsing Entity %i, Brush %i, Side %i : Expecting '(' got '%s'",
#ifdef HLCSG_COUNT_NEW
					b->originalentitynum, b->originalbrushnum, 
#else
                      b->entitynum, b->brushnum, 
#endif
					  b->numsides, g_token);
            }

            for (j = 0; j < 3; j++)
            {
                GetToken(false);
                side->planepts[i][j] = atof(g_token);
            }

            GetToken(false);
            if (strcmp(g_token, ")"))
            {
                Error("Parsing	Entity %i, Brush %i, Side %i : Expecting ')' got '%s'",
#ifdef HLCSG_COUNT_NEW
					b->originalentitynum, b->originalbrushnum, 
#else
                      b->entitynum, b->brushnum, 
#endif
					  b->numsides, g_token);
            }
        }

        // read the     texturedef
        GetToken(false);
        _strupr(g_token);
#ifdef HLCSG_CUSTOMHULL
		{
			if (!strncasecmp (g_token, "NOCLIP", 6) || !strncasecmp (g_token, "NULLNOCLIP", 10))
			{
				strcpy (g_token, "NULL");
				b->noclip = true;
			}
			if (!strncasecmp (g_token, "BEVELBRUSH", 10))
			{
				strcpy (g_token, "NULL");
				b->bevel = true;
			}
			if (!strncasecmp (g_token, "BEVEL", 5))
			{
				strcpy (g_token, "NULL");
				side->bevel = true;
			}
			if (!strncasecmp (g_token, "CLIP", 4))
			{
				b->cliphull |= (1 << NUM_HULLS); // arbitrary nonexistent hull
				int h;
				if (!strncasecmp (g_token, "CLIPHULL", 8) && (h = g_token[8] - '0', 0 < h && h < NUM_HULLS))
				{
					b->cliphull |= (1 << h); // hull h
				}
				if (!strncasecmp (g_token, "CLIPBEVEL", 9))
				{
					side->bevel = true;
				}
				if (!strncasecmp (g_token, "CLIPBEVELBRUSH", 14))
				{
					b->bevel = true;
				}
#ifdef HLCSG_PASSBULLETSBRUSH
				strcpy (g_token, "SKIP");
#else
				strcpy (g_token, "NULL");
#endif
			}
		}
#endif
        safe_strncpy(side->td.name, g_token, sizeof(side->td.name));

        if (g_nMapFileVersion < 220)                       // Worldcraft 2.1-, Radiant
        {
            GetToken(false);
            side->td.vects.valve.shift[0] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.shift[1] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.rotate = atof(g_token);
            GetToken(false);
            side->td.vects.valve.scale[0] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.scale[1] = atof(g_token);
        }
        else                                               // Worldcraft 2.2+
        {
            // texture U axis
            GetToken(false);
            if (strcmp(g_token, "["))
            {
                hlassume(false, assume_MISSING_BRACKET_IN_TEXTUREDEF);
            }

            GetToken(false);
            side->td.vects.valve.UAxis[0] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.UAxis[1] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.UAxis[2] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.shift[0] = atof(g_token);

            GetToken(false);
            if (strcmp(g_token, "]"))
            {
                Error("missing ']' in texturedef (U)");
            }

            // texture V axis
            GetToken(false);
            if (strcmp(g_token, "["))
            {
                Error("missing '[' in texturedef (V)");
            }

            GetToken(false);
            side->td.vects.valve.VAxis[0] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.VAxis[1] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.VAxis[2] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.shift[1] = atof(g_token);

            GetToken(false);
            if (strcmp(g_token, "]"))
            {
                Error("missing ']' in texturedef (V)");
            }

            // Texture rotation is implicit in U/V axes.
            GetToken(false);
            side->td.vects.valve.rotate = 0;

            // texure scale
            GetToken(false);
            side->td.vects.valve.scale[0] = atof(g_token);
            GetToken(false);
            side->td.vects.valve.scale[1] = atof(g_token);
        }

        ok = GetToken(true);                               // Done with line, this reads the first item from the next line

        if ((g_TXcommand == '1' || g_TXcommand == '2'))
        {
            // We are QuArK mode and need to translate some numbers to align textures its way
            // from QuArK, the texture vectors are given directly from the three points
            vec3_t          TexPt[2];
            int             k;
            float           dot22, dot23, dot33, mdet, aa, bb, dd;

            k = g_TXcommand - '0';
            for (j = 0; j < 3; j++)
            {
                TexPt[1][j] = (side->planepts[k][j] - side->planepts[0][j]) * ScaleCorrection;
            }
            k = 3 - k;
            for (j = 0; j < 3; j++)
            {
                TexPt[0][j] = (side->planepts[k][j] - side->planepts[0][j]) * ScaleCorrection;
            }

            dot22 = DotProduct(TexPt[0], TexPt[0]);
            dot23 = DotProduct(TexPt[0], TexPt[1]);
            dot33 = DotProduct(TexPt[1], TexPt[1]);
            mdet = dot22 * dot33 - dot23 * dot23;
            if (mdet < 1E-6 && mdet > -1E-6)
            {
                aa = bb = dd = 0;
                Warning
                    ("Degenerate QuArK-style brush texture : Entity %i, Brush %i @ (%f,%f,%f) (%f,%f,%f)	(%f,%f,%f)",
#ifdef HLCSG_COUNT_NEW
					b->originalentitynum, b->originalbrushnum, 
#else
                     b->entitynum, b->brushnum, 
#endif
					 side->planepts[0][0], side->planepts[0][1], side->planepts[0][2],
                     side->planepts[1][0], side->planepts[1][1], side->planepts[1][2], side->planepts[2][0],
                     side->planepts[2][1], side->planepts[2][2]);
            }
            else
            {
                mdet = 1.0 / mdet;
                aa = dot33 * mdet;
                bb = -dot23 * mdet;
                //cc = -dot23*mdet;             // cc = bb
                dd = dot22 * mdet;
            }

            for (j = 0; j < 3; j++)
            {
                side->td.vects.quark.vects[0][j] = aa * TexPt[0][j] + bb * TexPt[1][j];
                side->td.vects.quark.vects[1][j] = -( /*cc */ bb * TexPt[0][j] + dd * TexPt[1][j]);
            }

            side->td.vects.quark.vects[0][3] = -DotProduct(side->td.vects.quark.vects[0], side->planepts[0]);
            side->td.vects.quark.vects[1][3] = -DotProduct(side->td.vects.quark.vects[1], side->planepts[0]);
        }

        side->td.txcommand = g_TXcommand;                  // Quark stuff, but needs setting always
    };
#ifdef HLCSG_CUSTOMHULL
	if (b->cliphull != 0) // has CLIP* texture
	{
		unsigned int mask_anyhull = 0;
		for (int h = 1; h < NUM_HULLS; h++)
		{
			mask_anyhull |= (1 << h);
		}
		if ((b->cliphull & mask_anyhull) == 0) // no CLIPHULL1 or CLIPHULL2 or CLIPHULL3 texture
		{
			b->cliphull |= mask_anyhull; // CLIP all hulls
		}
	}
#endif

    b->contents = contents = CheckBrushContents(b);
#ifdef HLCSG_NULLIFY_INVISIBLE //this part has been moved down from the next line after '_strupr(g_token);'. --vluzacn
	for (j = 0; j < b->numsides; j++)
	{
		side = &g_brushsides[b->firstside + j];
		if(nullify && strncasecmp(side->td.name,"BEVEL",5) && strncasecmp(side->td.name,"ORIGIN",6)
#ifdef HLCSG_ALLOWHINTINENTITY
			&& strncasecmp(side->td.name,"HINT",4) && strncasecmp(side->td.name,"SKIP",4)
#endif
#ifdef HLCSG_HLBSP_SOLIDHINT
			&& strncasecmp(side->td.name,"SOLIDHINT",9)
#endif
#ifdef HLCSG_NOSPLITBYHINT
			&& strncasecmp(side->td.name,"SPLITFACE",9)
#endif
#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
			&& strncasecmp(side->td.name,"BOUNDINGBOX",11)
#endif
#ifdef HLCSG_CUSTOMCONTENT
			&& strncasecmp(side->td.name,"CONTENT",7) && strncasecmp(side->td.name,"SKY",3)
#endif
			)
		{
			safe_strncpy(side->td.name,"NULL",sizeof(side->td.name));
		}
	}
#endif
#ifdef HLCSG_NOSPLITBYHINT
	for (j = 0; j < b->numsides; j++)
	{
		// change to SKIP now that we have set brush content.
		side = &g_brushsides[b->firstside + j];
		if (!strncasecmp (side->td.name, "SPLITFACE", 9))
		{
			strcpy (side->td.name, "SKIP");
		}
	}
#endif
#ifdef HLCSG_CUSTOMCONTENT
	for (j = 0; j < b->numsides; j++)
	{
		side = &g_brushsides[b->firstside + j];
		if (!strncasecmp (side->td.name, "CONTENT", 7))
		{
			strcpy (side->td.name, "NULL");
		}
	}
#endif
#ifdef HLCSG_NULLIFYAAATRIGGER
	if (g_nullifytrigger)
	{
		for (j = 0; j < b->numsides; j++)
		{
			side = &g_brushsides[b->firstside + j];
			if (!strncasecmp (side->td.name, "AAATRIGGER", 10))
			{
				strcpy (side->td.name, "NULL");
			}
		}
	}
#endif

    //
    // origin brushes are removed, but they set
    // the rotation origin for the rest of the brushes
    // in the entity
    //

    if (contents == CONTENTS_ORIGIN)
    {
#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
		if (*ValueForKey (mapent, "origin"))
		{
			Error ("Entity %i, Brush %i: Only one ORIGIN brush allowed.",
	#ifdef HLCSG_COUNT_NEW
					b->originalentitynum, b->originalbrushnum
	#else
					b->entitynum, b->brushnum
	#endif
					);
		}
#endif
        char            string[MAXTOKEN];
        vec3_t          origin;

        b->contents = CONTENTS_SOLID;
        CreateBrush(mapent->firstbrush + b->brushnum);     // to get sizes
        b->contents = contents;

        for (i = 0; i < NUM_HULLS; i++)
        {
            b->hulls[i].faces = NULL;
        }

        if (b->entitynum != 0)  // Ignore for WORLD (code elsewhere enforces no ORIGIN in world message)
        {
            VectorAdd(b->hulls[0].bounds.m_Mins, b->hulls[0].bounds.m_Maxs, origin);
            VectorScale(origin, 0.5, origin);
    
            safe_snprintf(string, MAXTOKEN, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
            SetKeyValue(&g_entities[b->entitynum], "origin", string);
        }
    }
#ifdef HLCSG_COPYMODELKEYVALUE
	if (*ValueForKey (&g_entities[b->entitynum], "zhlt_usemodel"))
	{
		memset (&g_brushsides[b->firstside], 0, b->numsides * sizeof (side_t));
		g_numbrushsides -= b->numsides;
#ifdef HLCSG_HULLBRUSH
		for (int h = 0; h < NUM_HULLS; h++)
		{
			if (b->hullshapes[h])
			{
				free (b->hullshapes[h]);
			}
		}
#endif
		memset (b, 0, sizeof (brush_t));
		g_nummapbrushes--;
		mapent->numbrushes--;
		return;
	}
#endif
#ifdef HLCSG_HULLBRUSH
	if (!strcmp (ValueForKey (&g_entities[b->entitynum], "classname"), "info_hullshape"))
	{
		// all brushes should be erased, but not now.
		return;
	}
#endif
#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
    if (contents == CONTENTS_BOUNDINGBOX)
    {
		if (*ValueForKey (mapent, "zhlt_minsmaxs"))
		{
			Error ("Entity %i, Brush %i: Only one BoundingBox brush allowed.",
	#ifdef HLCSG_COUNT_NEW
					b->originalentitynum, b->originalbrushnum
	#else
					b->entitynum, b->brushnum
	#endif
					);
		}
        char            string[MAXTOKEN];
        vec3_t          mins, maxs;
		char			*origin = NULL;
		if (*ValueForKey (mapent, "origin"))
		{
			origin = strdup (ValueForKey (mapent, "origin"));
			SetKeyValue (mapent, "origin", "");
		}

        b->contents = CONTENTS_SOLID;
        CreateBrush(mapent->firstbrush + b->brushnum);     // to get sizes
        b->contents = contents;

        for (i = 0; i < NUM_HULLS; i++)
        {
            b->hulls[i].faces = NULL;
        }

        if (b->entitynum != 0)  // Ignore for WORLD (code elsewhere enforces no ORIGIN in world message)
        {
            VectorCopy(b->hulls[0].bounds.m_Mins, mins);
            VectorCopy(b->hulls[0].bounds.m_Maxs, maxs);
    
            safe_snprintf(string, MAXTOKEN, "%.0f %.0f %.0f %.0f %.0f %.0f", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]);
            SetKeyValue(&g_entities[b->entitynum], "zhlt_minsmaxs", string);
        }

		if (origin)
		{
			SetKeyValue (mapent, "origin", origin);
			free (origin);
		}
    }
#endif
#ifdef HLCSG_CUSTOMHULL
	if (g_skyclip && b->contents == CONTENTS_SKY && !b->noclip)
	{
		brush_t *newb = CopyCurrentBrush (mapent, b);
		newb->contents = CONTENTS_SOLID;
		newb->cliphull = ~0;
		for (j = 0; j < newb->numsides; j++)
		{
			side = &g_brushsides[newb->firstside + j];
			strcpy (side->td.name, "NULL");
		}
	}
#endif
#ifdef HLCSG_PASSBULLETSBRUSH
	if (b->cliphull != 0 && b->contents == CONTENTS_TOEMPTY)
	{
		// check for mix of CLIP and normal texture
		bool mixed = false;
		for (j = 0; j < b->numsides; j++)
		{
			side = &g_brushsides[b->firstside + j];
			if (!strncasecmp (side->td.name, "NULL", 4))
			{ // this is not supposed to be a HINT brush, so remove all invisible faces from hull 0.
				strcpy (side->td.name, "SKIP");
			}
			if (strncasecmp (side->td.name, "SKIP", 4))
				mixed = true;
		}
		if (mixed)
		{
			brush_t *newb = CopyCurrentBrush (mapent, b);
			newb->cliphull = 0;
		}
		b->contents = CONTENTS_SOLID;
		for (j = 0; j < b->numsides; j++)
		{
			side = &g_brushsides[b->firstside + j];
			strcpy (side->td.name, "NULL");
		}
	}
#endif

#ifndef HLCSG_COPYBRUSH
    return contents;
#endif
}


// =====================================================================================
//  ParseMapEntity
//      parse an entity from script
// =====================================================================================
bool            ParseMapEntity()
{
    bool            all_clip = true;
    int             this_entity;
    entity_t*       mapent;
    epair_t*        e;

#ifdef HLCSG_COUNT_NEW
	g_numparsedbrushes = 0;
#endif
    if (!GetToken(true))
    {
        return false;
    }

    this_entity = g_numentities;

    if (strcmp(g_token, "{"))
    {
        Error("Parsing Entity %i, expected '{' got '%s'", 
#ifdef HLCSG_COUNT_NEW
			g_numparsedentities, 
#else
			this_entity, 
#endif
			g_token);
    }

    hlassume(g_numentities < MAX_MAP_ENTITIES, assume_MAX_MAP_ENTITIES);
    g_numentities++;

    mapent = &g_entities[this_entity];
    mapent->firstbrush = g_nummapbrushes;
    mapent->numbrushes = 0;

    while (1)
    {
        if (!GetToken(true))
            Error("ParseEntity: EOF without closing brace");

        if (!strcmp(g_token, "}"))  // end of our context
            break;

        if (!strcmp(g_token, "{"))  // must be a brush
        {
#ifdef HLCSG_COPYBRUSH
			ParseBrush (mapent);
#else
            contents_t contents = ParseBrush(mapent);
#endif
#ifdef HLCSG_COUNT_NEW
			g_numparsedbrushes++;
#endif

#ifndef HLCSG_COPYBRUSH
            if ((contents != CONTENTS_CLIP)
				&& (contents != CONTENTS_ORIGIN)
#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
				&& contents != CONTENTS_BOUNDINGBOX
#endif
				)
                all_clip = false;
#endif
        }
        else                        // else assume an epair
        {
            e = ParseEpair();
			if (mapent->numbrushes > 0) Warning ("Error: ParseEntity: Keyvalue comes after brushes."); //--vluzacn

            if (!strcmp(e->key, "mapversion"))
            {
                g_nMapFileVersion = atoi(e->value);
            }

#ifdef HLCSG_NOREDUNDANTKEY
			SetKeyValue (mapent, e->key, e->value);
			Free (e->key);
			Free (e->value);
			Free (e);
#else
            e->next = mapent->epairs;
            mapent->epairs = e;
#endif
        }
    }
#ifdef HLCSG_COPYBRUSH
	{
		int i;
		for (i = 0; i < mapent->numbrushes; i++)
		{
			brush_t *brush = &g_mapbrushes[mapent->firstbrush + i];
			if (
	#ifdef HLCSG_CUSTOMHULL
				brush->cliphull == 0
	#else
				brush->contents != CONTENTS_CLIP
	#endif
				&& brush->contents != CONTENTS_ORIGIN
	#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
				&& brush->contents != CONTENTS_BOUNDINGBOX
	#endif
				)
			{
				all_clip = false;
			}
		}
	}
#endif
#ifdef HLCSG_COPYMODELKEYVALUE
	if (*ValueForKey (mapent, "zhlt_usemodel"))
	{
		if (!*ValueForKey (mapent, "origin"))
			Warning ("Entity %i: 'zhlt_usemodel' requires the entity to have an origin brush.", 
#ifdef HLCSG_COUNT_NEW
				g_numparsedentities
#else
				this_entity
#endif
				);
		mapent->numbrushes = 0;
	}
#endif
#ifdef HLCSG_SCALESIZE
#ifdef HLCSG_HULLBRUSH
	if (strcmp (ValueForKey (mapent, "classname"), "info_hullshape")) // info_hullshape is not affected by '-scale'
#endif
	{
		bool ent_move_b = false, ent_scale_b = false, ent_gscale_b = false;
		vec3_t ent_move = {0,0,0}, ent_scale_origin = {0,0,0};
		vec_t ent_scale = 1, ent_gscale = 1;

		if (g_scalesize > 0)
		{
			ent_gscale_b = true;
			ent_gscale = g_scalesize;
		}
		double v[4] = {0,0,0,0};
		if (*ValueForKey (mapent, "zhlt_transform"))
		{
			switch
				(sscanf(ValueForKey (mapent, "zhlt_transform"), "%lf %lf %lf %lf", v, v+1, v+2, v+3))
			{
			case 1:
				ent_scale_b = true;
				ent_scale = v[0];
				break;
			case 3:
				ent_move_b = true;
				VectorCopy (v, ent_move);
				break;
			case 4:
				ent_scale_b = true;
				ent_scale = v[0];
				ent_move_b = true;
				VectorCopy (v+1, ent_move);
				break;
			default:
				Warning ("bad value '%s' for key 'zhlt_transform'", ValueForKey (mapent, "zhlt_transform"));
			}
			DeleteKey (mapent, "zhlt_transform");
		}
		GetVectorForKey (mapent, "origin", ent_scale_origin);

		if (ent_move_b || ent_scale_b || ent_gscale_b)
		{
			if (g_nMapFileVersion < 220 || g_brushsides[0].td.txcommand != 0)
			{
				Warning ("hlcsg scaling hack is not supported in Worldcraft 2.1- or QuArK mode");
			}
			else
			{
				int ibrush, iside, ipoint;
				brush_t *brush;
				side_t *side;
				vec_t *point;
				for (ibrush = 0, brush = g_mapbrushes + mapent->firstbrush; ibrush < mapent->numbrushes; ++ibrush, ++brush)
				{
					for (iside = 0, side = g_brushsides + brush->firstside; iside < brush->numsides; ++iside, ++side)
					{
						for (ipoint = 0; ipoint < 3; ++ipoint)
						{
							point = side->planepts[ipoint];
							if (ent_scale_b)
							{
								VectorSubtract (point, ent_scale_origin, point);
								VectorScale (point, ent_scale, point);
								VectorAdd (point, ent_scale_origin, point);
							}
							if (ent_move_b)
							{
								VectorAdd (point, ent_move, point);

							}
							if (ent_gscale_b)
							{
								VectorScale (point, ent_gscale, point);
							}
						}
#ifdef ZHLT_FREETEXTUREAXIS
						// note that  tex->vecs = td.vects.valve.Axis / td.vects.valve.scale
						//            tex->vecs[3] = vects.valve.shift + Dot(origin, tex->vecs)
						//      and   texcoordinate = Dot(worldposition, tex->vecs) + tex->vecs[3]
						bool zeroscale = false;
						if (!side->td.vects.valve.scale[0])
						{
							side->td.vects.valve.scale[0] = 1;
						}
						if (!side->td.vects.valve.scale[1])
						{
							side->td.vects.valve.scale[1] = 1;
						}
						if (ent_scale_b)
						{
							vec_t coord[2];
							if (fabs (side->td.vects.valve.scale[0]) > NORMAL_EPSILON)
							{
								coord[0] = DotProduct (ent_scale_origin, side->td.vects.valve.UAxis) / side->td.vects.valve.scale[0] + side->td.vects.valve.shift[0];
								side->td.vects.valve.scale[0] *= ent_scale;
								if (fabs (side->td.vects.valve.scale[0]) > NORMAL_EPSILON)
								{
									side->td.vects.valve.shift[0] = coord[0] - DotProduct (ent_scale_origin, side->td.vects.valve.UAxis) / side->td.vects.valve.scale[0];
								}
								else
								{
									zeroscale = true;
								}
							}
							else
							{
								zeroscale = true;
							}
							if (fabs (side->td.vects.valve.scale[1]) > NORMAL_EPSILON)
							{
								coord[1] = DotProduct (ent_scale_origin, side->td.vects.valve.VAxis) / side->td.vects.valve.scale[1] + side->td.vects.valve.shift[1];
								side->td.vects.valve.scale[1] *= ent_scale;
								if (fabs (side->td.vects.valve.scale[1]) > NORMAL_EPSILON)
								{
									side->td.vects.valve.shift[1] = coord[1] - DotProduct (ent_scale_origin, side->td.vects.valve.VAxis) / side->td.vects.valve.scale[1];
								}
								else
								{
									zeroscale = true;
								}
							}
							else
							{
								zeroscale = true;
							}
						}
						if (ent_move_b)
						{
							if (fabs (side->td.vects.valve.scale[0]) > NORMAL_EPSILON)
							{
								side->td.vects.valve.shift[0] -= DotProduct (ent_move, side->td.vects.valve.UAxis) / side->td.vects.valve.scale[0];
							}
							else
							{
								zeroscale = true;
							}
							if (fabs (side->td.vects.valve.scale[1]) > NORMAL_EPSILON)
							{
								side->td.vects.valve.shift[1] -= DotProduct (ent_move, side->td.vects.valve.VAxis) / side->td.vects.valve.scale[1];
							}
							else
							{
								zeroscale = true;
							}
						}
						if (ent_gscale_b)
						{
							side->td.vects.valve.scale[0] *= ent_gscale;
							side->td.vects.valve.scale[1] *= ent_gscale;
						}
						if (zeroscale)
						{
							Error ("Entity %i, Brush %i: invalid texture scale.\n", 
	#ifdef HLCSG_COUNT_NEW
								brush->originalentitynum, brush->originalbrushnum
	#else
								this_entity, ibrush
	#endif
								);
						}
#else
						vec3_t U, V, position;
						// assume: UAxis and VAxis are perpendicular normalized vectors.
						VectorScale (side->td.vects.valve.UAxis, - side->td.vects.valve.shift[0] * side->td.vects.valve.scale[0], U);
						VectorScale (side->td.vects.valve.VAxis, - side->td.vects.valve.shift[1] * side->td.vects.valve.scale[1], V);
						VectorAdd (U, V, position);
						if (ent_scale_b)
						{
							VectorSubtract (position, ent_scale_origin, position);
							VectorScale (position, ent_scale, position);
							VectorAdd (position, ent_scale_origin, position);
							side->td.vects.valve.scale[0] *= ent_scale;
							side->td.vects.valve.scale[1] *= ent_scale;
						}
						if (ent_move_b)
						{
							VectorAdd (position, ent_move, position);
						}
						if (ent_gscale_b)
						{
							VectorScale (position, ent_gscale, position);
							side->td.vects.valve.scale[0] *= ent_gscale;
							side->td.vects.valve.scale[1] *= ent_gscale;
						}
						side->td.vects.valve.shift[0] = DotProduct (position, side->td.vects.valve.UAxis) / - side->td.vects.valve.scale[0];
						side->td.vects.valve.shift[1] = DotProduct (position, side->td.vects.valve.VAxis) / - side->td.vects.valve.scale[1];
#endif
					}
				}
				if (ent_gscale_b)
				{
					if (*ValueForKey (mapent, "origin"))
					{
						double v[3];
						int origin[3];
						char string[MAXTOKEN];
						int i;
						GetVectorForKey (mapent, "origin", v);
						VectorScale (v, ent_gscale, v);
						for (i=0; i<3; ++i)
							origin[i] = (int)(v[i]>=0? v[i]+0.5: v[i]-0.5);
						safe_snprintf(string, MAXTOKEN, "%d %d %d", origin[0], origin[1], origin[2]);
						SetKeyValue (mapent, "origin", string);
					}
				}
#ifdef HLCSG_HLBSP_CUSTOMBOUNDINGBOX
				{
					double b[2][3];
					if (sscanf (ValueForKey (mapent, "zhlt_minsmaxs"), "%lf %lf %lf %lf %lf %lf", &b[0][0], &b[0][1], &b[0][2], &b[1][0], &b[1][1], &b[1][2]) == 6)
					{
						for (int i = 0; i < 2; i++)
						{
							vec_t *point = b[i];
							if (ent_scale_b)
							{
								VectorSubtract (point, ent_scale_origin, point);
								VectorScale (point, ent_scale, point);
								VectorAdd (point, ent_scale_origin, point);
							}
							if (ent_move_b)
							{
								VectorAdd (point, ent_move, point);

							}
							if (ent_gscale_b)
							{
								VectorScale (point, ent_gscale, point);
							}
						}
						char string[MAXTOKEN];
						safe_snprintf(string, MAXTOKEN, "%.0f %.0f %.0f %.0f %.0f %.0f", b[0][0], b[0][1], b[0][2], b[1][0], b[1][1], b[1][2]);
						SetKeyValue (mapent, "zhlt_minsmaxs", string);
					}
				}
#endif
			}
		}
	}
#endif


#ifndef HLCSG_HLBSP_ALLOWEMPTYENTITY
    if (mapent->numbrushes && all_clip)
        Fatal(assume_NO_VISIBILE_BRUSHES, "Entity %i has no visible brushes\n", 
#ifdef HLCSG_COUNT_NEW
			g_numparsedentities
#else
			this_entity
#endif
			);
#endif

    CheckFatal();
#ifdef HLCSG_LOGVERSION
	if (this_entity == 0)
	{
		// Let the map tell which version of the compiler it comes from, to help tracing compiler bugs.
		char versionstring [128];
		sprintf (versionstring, "ZHLT " ZHLT_VERSIONSTRING " " HACK_VERSIONSTRING " (%s)", __DATE__);
		SetKeyValue (mapent, "compiler", versionstring);
	}
#endif
    
#ifdef ZHLT_DETAIL // AJM
    if (!strcmp(ValueForKey(mapent, "classname"), "info_detail") && g_bDetailBrushes && this_entity != 0)
    {
        // mark all of the brushes in this entity as contents_detail
        for (int i = mapent->firstbrush; i < mapent->firstbrush + mapent->numbrushes; i++)
        {
            g_mapbrushes[i].contents = CONTENTS_DETAIL;
        }

        // move these brushes to worldspawn
        {
            brush_t*        temp;
            int             newbrushes;
            int             worldbrushes;
            int             i;

            newbrushes = mapent->numbrushes;
            worldbrushes = g_entities[0].numbrushes;

            temp = (brush_t*)Alloc(newbrushes * sizeof(brush_t));
            memcpy(temp, g_mapbrushes + mapent->firstbrush, newbrushes * sizeof(brush_t));

            for (i = 0; i < newbrushes; i++)
            {
                temp[i].entitynum = 0;
            }

            // make space to move the brushes (overlapped copy)
            memmove(g_mapbrushes + worldbrushes + newbrushes,
                    g_mapbrushes + worldbrushes, sizeof(brush_t) * (g_nummapbrushes - worldbrushes - newbrushes));

            // copy the new brushes down
            memcpy(g_mapbrushes + worldbrushes, temp, sizeof(brush_t) * newbrushes);

            // fix up indexes
            g_numentities--;
            g_entities[0].numbrushes += newbrushes;
            for (i = 1; i < g_numentities; i++)
            {
                g_entities[i].firstbrush += newbrushes;
            }
            memset(mapent, 0, sizeof(*mapent));
            Free(temp);
        }

        // delete this entity
        g_numentities--;
        return true;
    }
#endif


#ifdef ZHLT_INFO_COMPILE_PARAMETERS // AJM
    if (!strcmp(ValueForKey(mapent, "classname"), "info_compile_parameters"))
    {
        GetParamsFromEnt(mapent);
    }
#endif

#ifndef HLCSG_CUSTOMHULL
    // if its the worldspawn entity and we need to skyclip, then do it
    if ((this_entity == 0) && g_skyclip)                  // first entitiy
    {
        HandleSKYCLIP();
    }
#endif

#ifndef HLCSG_HLBSP_ALLOWEMPTYENTITY
    // if the given entity only has one brush and its an origin brush
    if ((mapent->numbrushes == 1) && (g_mapbrushes[mapent->firstbrush].contents == CONTENTS_ORIGIN))
    {
        brushhull_t*    hull = g_mapbrushes[mapent->firstbrush].hulls;

        Error("Entity %i, contains ONLY an origin brush near (%.0f,%.0f,%.0f)\n",
#ifdef HLCSG_COUNT_NEW
			g_numparsedentities, 
#else
              this_entity, 
#endif
			  hull->bounds.m_Mins[0], hull->bounds.m_Mins[1], hull->bounds.m_Mins[2]);
    }
#endif

    GetVectorForKey(mapent, "origin", mapent->origin);

#ifdef HLCSG_FUNCGROUP_FIX
	if (!strcmp("func_group", ValueForKey(mapent, "classname"))
#ifdef ZHLT_DETAILBRUSH
		|| !strcmp("func_detail", ValueForKey (mapent, "classname"))
#endif
		)
#else
    // group entities are just for editor convenience
    // toss all brushes into the world entity
    if (!g_onlyents && !strcmp("func_group", ValueForKey(mapent, "classname")))
#endif
    {
#ifdef ZHLT_HIDDENSOUNDTEXTURE
		if (IntForKey (mapent,"zhlt_hidden"))
		{
			for (int i = 0; i < mapent->numbrushes; i++)
			{
				brush_t *b = &g_mapbrushes[mapent->firstbrush + i];
				for (int j = 0; j < b->numsides; j++)
				{
					side_t *s = &g_brushsides[b->firstside + j];
					s->shouldhide = true;
				}
			}
		}
#endif
        // this is pretty gross, because the brushes are expected to be
        // in linear order for each entity
        brush_t*        temp;
        int             newbrushes;
        int             worldbrushes;
        int             i;

        newbrushes = mapent->numbrushes;
        worldbrushes = g_entities[0].numbrushes;

        temp = (brush_t*)Alloc(newbrushes * sizeof(brush_t));
        memcpy(temp, g_mapbrushes + mapent->firstbrush, newbrushes * sizeof(brush_t));

        for (i = 0; i < newbrushes; i++)
        {
            temp[i].entitynum = 0;
#ifdef HLCSG_FUNCGROUP_FIX
			temp[i].brushnum += worldbrushes;
#endif
        }

        // make space to move the brushes (overlapped copy)
        memmove(g_mapbrushes + worldbrushes + newbrushes,
                g_mapbrushes + worldbrushes, sizeof(brush_t) * (g_nummapbrushes - worldbrushes - newbrushes));

        // copy the new brushes down
        memcpy(g_mapbrushes + worldbrushes, temp, sizeof(brush_t) * newbrushes);

        // fix up indexes
        g_numentities--;
        g_entities[0].numbrushes += newbrushes;
        for (i = 1; i < g_numentities; i++)
        {
            g_entities[i].firstbrush += newbrushes;
        }
        memset(mapent, 0, sizeof(*mapent));
        Free(temp);
#ifdef HLCSG_FUNCGROUP_FIX
		return true;
#endif
    }

#ifdef HLCSG_HULLBRUSH
	if (!strcmp (ValueForKey (mapent, "classname"), "info_hullshape"))
	{
		bool disabled;
		const char *id;
		int defaulthulls;
		disabled = IntForKey (mapent, "disabled");
		id = ValueForKey (mapent, "targetname");
		defaulthulls = IntForKey (mapent, "defaulthulls");
		CreateHullShape (this_entity, disabled, id, defaulthulls);
		DeleteCurrentEntity (mapent);
		return true;
	}
#endif
#ifdef ZHLT_LARGERANGE
	if (fabs (mapent->origin[0]) > ENGINE_ENTITY_RANGE + ON_EPSILON ||
		fabs (mapent->origin[1]) > ENGINE_ENTITY_RANGE + ON_EPSILON ||
		fabs (mapent->origin[2]) > ENGINE_ENTITY_RANGE + ON_EPSILON )
	{
		const char *classname = ValueForKey (mapent, "classname");
		if (strncmp (classname, "light", 5))
		{
			Warning ("Entity %i (classname \"%s\"): origin outside +/-%.0f: (%.0f,%.0f,%.0f)", 
#ifdef HLCSG_COUNT_NEW
				g_numparsedentities, 
#else
				this_entity, 
#endif
				classname, (double)ENGINE_ENTITY_RANGE, mapent->origin[0], mapent->origin[1], mapent->origin[2]);
		}
	}
#endif
    return true;
}

// =====================================================================================
//  CountEngineEntities
// =====================================================================================
unsigned int    CountEngineEntities()
{
    unsigned int x;
    unsigned num_engine_entities = 0;
    entity_t*       mapent = g_entities;

    // for each entity in the map
    for (x=0; x<g_numentities; x++, mapent++)
    {
        const char* classname = ValueForKey(mapent, "classname");

        // if its a light_spot or light_env, dont include it as an engine entity!
        if (classname)
        {
            if (   !strncasecmp(classname, "light", 5) 
                || !strncasecmp(classname, "light_spot", 10) 
                || !strncasecmp(classname, "light_environment", 17)
               )
            {
                const char* style = ValueForKey(mapent, "style");
                const char* targetname = ValueForKey(mapent, "targetname");

                // lightspots and lightenviroments dont have a targetname or style
                if (!strlen(targetname) && !atoi(style))
                {
                    continue;
                }
            }
        }

        num_engine_entities++;
    }

    return num_engine_entities;
}

// =====================================================================================
//  LoadMapFile
//      wrapper for LoadScriptFile
//      parse in script entities
// =====================================================================================
const char*     ContentsToString(const contents_t type);

void            LoadMapFile(const char* const filename)
{
    unsigned num_engine_entities;

    LoadScriptFile(filename);

    g_numentities = 0;

#ifdef HLCSG_COUNT_NEW
	g_numparsedentities = 0;
#endif
    while (ParseMapEntity())
    {
#ifdef HLCSG_COUNT_NEW
		g_numparsedentities++;
#endif
    }

    // AJM debug
    /*
    for (int i = 0; i < g_numentities; i++)
    {
        Log("entity: %i - %i brushes - %s\n", i, g_entities[i].numbrushes, ValueForKey(&g_entities[i], "classname"));
    }
    Log("total entities: %i\ntotal brushes: %i\n\n", g_numentities, g_nummapbrushes);

    for (i = g_entities[0].firstbrush; i < g_entities[0].firstbrush + g_entities[0].numbrushes; i++)
    {
        Log("worldspawn brush %i: contents %s\n", i, ContentsToString((contents_t)g_mapbrushes[i].contents)); 
    }
    */

    num_engine_entities = CountEngineEntities();

    hlassume(num_engine_entities < MAX_ENGINE_ENTITIES, assume_MAX_ENGINE_ENTITIES);

    CheckFatal();

    Verbose("Load map:%s\n", filename);
    Verbose("%5i brushes\n", g_nummapbrushes);
    Verbose("%5i map entities \n", g_numentities - num_engine_entities);
    Verbose("%5i engine entities\n", num_engine_entities);

    // AJM: added in 
#ifdef HLCSG_AUTOWAD
#ifndef HLCSG_AUTOWAD_TEXTURELIST_FIX
#ifdef HLCSG_ONLYENTS_NOWADCHANGE
	if (!g_onlyents)
	{
#endif
    GetUsedTextures();
#ifdef HLCSG_ONLYENTS_NOWADCHANGE
	}
#endif
#endif
#endif
}

