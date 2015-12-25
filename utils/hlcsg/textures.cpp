#include "csg.h"

#define MAXWADNAME 16
#define MAX_TEXFILES 128

//  FindMiptex
//  TEX_InitFromWad
//  FindTexture
//  LoadLump
//  AddAnimatingTextures


typedef struct
{
    char            identification[4];                     // should be WAD2/WAD3
    int             numlumps;
    int             infotableofs;
} wadinfo_t;

typedef struct
{
    int             filepos;
    int             disksize;
    int             size;                                  // uncompressed
    char            type;
    char            compression;
    char            pad1, pad2;
    char            name[MAXWADNAME];                      // must be null terminated // upper case

    int             iTexFile;                              // index of the wad this texture is located in

} lumpinfo_t;

std::deque< std::string > g_WadInclude;
#ifndef HLCSG_AUTOWAD_NEW
std::map< int, bool > s_WadIncludeMap;
#endif

static int      nummiptex = 0;
static lumpinfo_t miptex[MAX_MAP_TEXTURES];
static int      nTexLumps = 0;
static lumpinfo_t* lumpinfo = NULL;
static int      nTexFiles = 0;
static FILE*    texfiles[MAX_TEXFILES];
#ifdef HLCSG_AUTOWAD_NEW
static wadpath_t* texwadpathes[MAX_TEXFILES]; // maps index of the wad to its path
#endif

#ifdef HLCSG_TEXMAP64_FIX
// The old buggy code in effect limit the number of brush sides to MAX_MAP_BRUSHES
#ifdef HLCSG_HLBSP_REDUCETEXTURE
static char *texmap[MAX_INTERNAL_MAP_TEXINFO];
#else
static char *texmap[MAX_MAP_TEXINFO];
#endif
static int numtexmap = 0;

static int texmap_store (char *texname, bool shouldlock = true)
	// This function should never be called unless a new entry in g_texinfo is being allocated.
{
	int i;
	if (shouldlock)
	{
		ThreadLock ();
	}
#ifdef HLCSG_HLBSP_REDUCETEXTURE
	hlassume (numtexmap < MAX_INTERNAL_MAP_TEXINFO, assume_MAX_MAP_TEXINFO); // This error should never appear.
#else
	hlassume (numtexmap < MAX_MAP_TEXINFO, assume_MAX_MAP_TEXINFO); // This error should never appear.
#endif
	i = numtexmap;
	texmap[numtexmap] = strdup (texname);
	numtexmap++;
	if (shouldlock)
	{
		ThreadUnlock ();
	}
	return i;
}

static char *texmap_retrieve (int index)
{
	hlassume (0 <= index && index < numtexmap, assume_first);
	return texmap[index];
}

static void texmap_clear ()
{
	int i;
	ThreadLock ();
	for (i = 0; i < numtexmap; i++)
	{
		free (texmap[i]);
	}
	numtexmap = 0;
	ThreadUnlock ();
}
#else
// fix for 64 bit machines
#if /* 64 bit */
    static char* texmap64[MAX_MAP_BRUSHES];
    static int   tex_max64=0;

    static inline int texmap64_store(char *texname)
    {
        int curr_tex;
        ThreadLock();
        if (tex_max64 >= MAX_MAP_BRUSHES)   // no assert?
        {
#ifdef ZHLT_CONSOLE
			Error ("MAX_MAP_BRUSHES exceeded!");
#else
            printf("MAX_MAP_BRUSHES exceeded!\n");
            exit(-1);
#endif
        }
        curr_tex = tex_max64;
        texmap64[tex_max64] = texname;
        tex_max64++;
        ThreadUnlock();
        return curr_tex;
    }

    static inline char* texmap64_retrieve( int index)
    {
        if(index > tex_max64)
        {
#ifdef ZHLT_CONSOLE
			Error ("retrieving bogus texture index %d", index);
#else
            printf("retrieving bogus texture index %d\n", index);
            exit(-1);
#endif
        }
        return texmap64[index];
    }

#else
    #define texmap64_store( A ) ( (int) A)
    #define texmap64_retrieve( A ) ( (char*) A)
#endif
#endif

// =====================================================================================
//  CleanupName
// =====================================================================================
static void     CleanupName(const char* const in, char* out)
{
    int             i;

    for (i = 0; i < MAXWADNAME; i++)
    {
        if (!in[i])
        {
            break;
        }

        out[i] = toupper(in[i]);
    }

    for (; i < MAXWADNAME; i++)
    {
        out[i] = 0;
    }
}

// =====================================================================================
//  lump_sorters
// =====================================================================================

static int CDECL lump_sorter_by_wad_and_name(const void* lump1, const void* lump2)
{
    lumpinfo_t*     plump1 = (lumpinfo_t*)lump1;
    lumpinfo_t*     plump2 = (lumpinfo_t*)lump2;

    if (plump1->iTexFile == plump2->iTexFile)
    {
        return strcmp(plump1->name, plump2->name);
    }
    else
    {
        return plump1->iTexFile - plump2->iTexFile;
    }
}

static int CDECL lump_sorter_by_name(const void* lump1, const void* lump2)
{
    lumpinfo_t*     plump1 = (lumpinfo_t*)lump1;
    lumpinfo_t*     plump2 = (lumpinfo_t*)lump2;

    return strcmp(plump1->name, plump2->name);
}

// =====================================================================================
//  FindMiptex
//      Find and allocate a texture into the lump data
// =====================================================================================
static int      FindMiptex(const char* const name)
{
    int             i;
#ifdef HLCSG_TEXMAP64_FIX
	if (strlen (name) >= MAXWADNAME)
	{
		Error ("Texture name is too long (%s)\n", name);
	}
#endif

    ThreadLock();
    for (i = 0; i < nummiptex; i++)
    {
        if (!strcmp(name, miptex[i].name))
        {
            ThreadUnlock();
            return i;
        }
    }

    hlassume(nummiptex < MAX_MAP_TEXTURES, assume_MAX_MAP_TEXTURES);
    safe_strncpy(miptex[i].name, name, MAXWADNAME);
    nummiptex++;
    ThreadUnlock();
    return i;
}

// =====================================================================================
//  TEX_InitFromWad
// =====================================================================================
bool            TEX_InitFromWad()
{
    int             i, j;
    wadinfo_t       wadinfo;
#ifndef HLCSG_AUTOWAD_NEW
    char            szTmpWad[1024]; // arbitrary, but needs to be large.
#endif
    char*           pszWadFile;
    const char*     pszWadroot;
    wadpath_t*      currentwad;

    Log("\n"); // looks cleaner
#ifdef HLCSG_AUTOWAD_NEW
	// update wad inclusion
	for (i = 0; i < g_iNumWadPaths; i++)
	{
        currentwad = g_pWadPaths[i];
		if (!g_wadtextures) // '-nowadtextures'
		{
			currentwad->usedbymap = false; // include this wad
		}
		for (WadInclude_i it = g_WadInclude.begin (); it != g_WadInclude.end (); it++) // '-wadinclude xxx.wad'
		{
			if (stristr (currentwad->path, it->c_str ()))
			{
				currentwad->usedbymap = false; // include this wad
			}
		}
	}
#endif

#ifndef HLCSG_AUTOWAD_NEW
    szTmpWad[0] = 0;
#endif
    pszWadroot = getenv("WADROOT");

#ifndef HLCSG_AUTOWAD_NEW
#ifdef HLCSG_AUTOWAD
    autowad_UpdateUsedWads();
#endif
#endif

    // for eachwadpath
    for (i = 0; i < g_iNumWadPaths; i++)
    {
        FILE*           texfile;                           // temporary used in this loop
#ifndef HLCSG_AUTOWAD_NEW
        bool            bExcludeThisWad = false;
#endif

        currentwad = g_pWadPaths[i];
        pszWadFile = currentwad->path;


#ifndef HLCSG_AUTOWAD_NEW
#ifdef HLCSG_AUTOWAD
        #ifdef _DEBUG
        Log("[dbg] Attempting to parse wad: '%s'\n", pszWadFile);
        #endif

        if (g_bWadAutoDetect && !currentwad->usedtextures)
            continue;

        #ifdef _DEBUG
        Log("[dbg] Parsing wad\n");
        #endif
#endif
#endif

#ifdef HLCSG_AUTOWAD_NEW
		texwadpathes[nTexFiles] = currentwad;
#endif
        texfiles[nTexFiles] = fopen(pszWadFile, "rb");

        #ifdef SYSTEM_WIN32
        if (!texfiles[nTexFiles])
        {
            // cant find it, maybe this wad file has a hard code drive
            if (pszWadFile[1] == ':')
            {
                pszWadFile += 2;                           // skip past the drive
                texfiles[nTexFiles] = fopen(pszWadFile, "rb");
            }
        }
        #endif

        if (!texfiles[nTexFiles] && pszWadroot)
        {
            char            szTmp[_MAX_PATH];
            char            szFile[_MAX_PATH];
            char            szSubdir[_MAX_PATH];

            ExtractFile(pszWadFile, szFile);

            ExtractFilePath(pszWadFile, szTmp);
            ExtractFile(szTmp, szSubdir);

            // szSubdir will have a trailing separator
            safe_snprintf(szTmp, _MAX_PATH, "%s" SYSTEM_SLASH_STR "%s%s", pszWadroot, szSubdir, szFile);
            texfiles[nTexFiles] = fopen(szTmp, "rb");

            #ifdef SYSTEM_POSIX
            if (!texfiles[nTexFiles])
            {
                // if we cant find it, Convert to lower case and try again
                strlwr(szTmp);
                texfiles[nTexFiles] = fopen(szTmp, "rb");
            }
            #endif
        }

		#ifdef HLCSG_SEARCHWADPATH_VL
        #ifdef SYSTEM_WIN32
		if (!texfiles[nTexFiles] && pszWadFile[0] == '\\')
		{
			char tmp[_MAX_PATH];
			int l;
			for (l = 'C'; l <= 'Z'; ++l)
			{
				safe_snprintf (tmp, _MAX_PATH, "%c:%s", l, pszWadFile);
				texfiles[nTexFiles] = fopen (tmp, "rb");
				if (texfiles[nTexFiles])
				{
					Developer (DEVELOPER_LEVEL_MESSAGE, "wad file found in drive '%c:' : %s\n", l, pszWadFile);
					break;
				}
			}
		}
		#endif
		#endif

        if (!texfiles[nTexFiles])
        {
#ifdef HLCSG_SEARCHWADPATH_VL
			pszWadFile = currentwad->path; // correct it back
#endif
            // still cant find it, error out
            Fatal(assume_COULD_NOT_FIND_WAD, "Could not open wad file %s", pszWadFile);
            continue;
        }

#ifdef HLCSG_WADCFG_NEW
		pszWadFile = currentwad->path; // correct it back
#endif
#ifndef HLCSG_AUTOWAD_NEW
        // look and see if we're supposed to include the textures from this WAD in the bsp.
        WadInclude_i it;
        for (it = g_WadInclude.begin(); it != g_WadInclude.end(); it++)
        {
            if (stristr(pszWadFile, it->c_str()))
            {
                Log("Including Wadfile: %s\n", pszWadFile);
                bExcludeThisWad = true;             // wadincluding this one
                s_WadIncludeMap[nTexFiles] = true;
                break;
            }
        }
#ifdef HLCSG_ONLYENTS_NOWADCHANGE
		if (!bExcludeThisWad && !g_wadtextures) // -nowadtextures
		{
			Log("Including Wadfile: %s\n", pszWadFile);
			bExcludeThisWad = true;
		}
#endif

        if (!bExcludeThisWad)
        {
            Log("Using Wadfile: %s\n", pszWadFile);
#ifdef HLCSG_STRIPWADPATH
			char tmp[_MAX_PATH];
			ExtractFile (pszWadFile, tmp);
            safe_snprintf(szTmpWad, 1024, "%s%s;", szTmpWad, tmp);
#else
            safe_snprintf(szTmpWad, 1024, "%s%s;", szTmpWad, pszWadFile);
#endif
        }
#endif

        // temp assignment to make things cleaner:
        texfile = texfiles[nTexFiles];

        // read in this wadfiles information
        SafeRead(texfile, &wadinfo, sizeof(wadinfo));

        // make sure its a valid format
        if (strncmp(wadinfo.identification, "WAD2", 4) && strncmp(wadinfo.identification, "WAD3", 4))
        {
            Log(" - ");
            Error("%s isn't a Wadfile!", pszWadFile);
        }

        wadinfo.numlumps        = LittleLong(wadinfo.numlumps);
        wadinfo.infotableofs    = LittleLong(wadinfo.infotableofs);

        // read in lump
        if (fseek(texfile, wadinfo.infotableofs, SEEK_SET))
            Warning("fseek to %d in wadfile %s failed\n", wadinfo.infotableofs, pszWadFile);

        // memalloc for this lump
        lumpinfo = (lumpinfo_t*)realloc(lumpinfo, (nTexLumps + wadinfo.numlumps) * sizeof(lumpinfo_t));

        // for each texlump
        for (j = 0; j < wadinfo.numlumps; j++, nTexLumps++)
        {
            SafeRead(texfile, &lumpinfo[nTexLumps], (sizeof(lumpinfo_t) - sizeof(int)) );  // iTexFile is NOT read from file

            if (!TerminatedString(lumpinfo[nTexLumps].name, MAXWADNAME))
            {
                lumpinfo[nTexLumps].name[MAXWADNAME - 1] = 0;
                Log(" - ");
                Warning("Unterminated texture name : wad[%s] texture[%d] name[%s]\n", pszWadFile, nTexLumps, lumpinfo[nTexLumps].name);
            }

            CleanupName(lumpinfo[nTexLumps].name, lumpinfo[nTexLumps].name);

            lumpinfo[nTexLumps].filepos = LittleLong(lumpinfo[nTexLumps].filepos);
            lumpinfo[nTexLumps].disksize = LittleLong(lumpinfo[nTexLumps].disksize);
            lumpinfo[nTexLumps].iTexFile = nTexFiles;

            if (lumpinfo[nTexLumps].disksize > MAX_TEXTURE_SIZE)
            {
                Log(" - ");
                Warning("Larger than expected texture (%d bytes): '%s'",
                    lumpinfo[nTexLumps].disksize, lumpinfo[nTexLumps].name);
            }

        }

        // AJM: this feature is dependant on autowad. :(
        // CONSIDER: making it standard?
#ifdef HLCSG_AUTOWAD_NEW
		currentwad->totaltextures = wadinfo.numlumps;
#else
#ifdef HLCSG_AUTOWAD
        {
            double percused = ((float)(currentwad->usedtextures) / (float)(g_numUsedTextures)) * 100;
            Log(" - Contains %i used texture%s, %2.2f percent of map (%d textures in wad)\n",
                currentwad->usedtextures, currentwad->usedtextures == 1 ? "" : "s", percused, wadinfo.numlumps);
        }
#endif
#endif

        nTexFiles++;
        hlassume(nTexFiles < MAX_TEXFILES, assume_MAX_TEXFILES);
    }

    //Log("num of used textures: %i\n", g_numUsedTextures);

#ifndef HLCSG_STRIPWADPATH
	// This is absurd. --vluzacn
    // AJM: Tommy suggested i add this warning message in, and  it certianly doesnt
    //  hurt to be cautious. Especially one of the possible side effects he mentioned was svc_bad
    if (nTexFiles > 8)
    {
        Log("\n");
        Warning("More than 8 wadfiles are in use. (%i)\n"
                "This may be harmless, and if no strange side effects are occurring, then\n"
                "it can safely be ignored. However, if your map starts exhibiting strange\n"
                "or obscure errors, consider this as suspect.\n"
                , nTexFiles);
    }
#endif

    // sort texlumps in memory by name
    qsort((void*)lumpinfo, (size_t) nTexLumps, sizeof(lumpinfo[0]), lump_sorter_by_name);

#ifndef HLCSG_AUTOWAD_NEW
#ifdef HLCSG_CHART_FIX
    Log("\n");
#ifdef HLCSG_ONLYENTS_NOWADCHANGE
	if (*szTmpWad)
	{
		Log ("Wad files required to run the map: \"%s\"\n", szTmpWad);
	}
	else
	{
		Log ("Wad files required to run the map: (None)\n");
	}
#else
    Log("\"wad\" is \"%s\"\n", szTmpWad);
#endif
#endif
    SetKeyValue(&g_entities[0], "wad", szTmpWad);

    Log("\n");
#endif
    CheckFatal();
    return true;
}

// =====================================================================================
//  FindTexture
// =====================================================================================
lumpinfo_t*     FindTexture(const lumpinfo_t* const source)
{
    //Log("** PnFNFUNC: FindTexture\n");

    lumpinfo_t*     found = NULL;

    found = (lumpinfo_t*)bsearch(source, (void*)lumpinfo, (size_t) nTexLumps, sizeof(lumpinfo[0]), lump_sorter_by_name);
    if (!found)
    {
        Warning("::FindTexture() texture %s not found!", source->name);
        if (!strcmp(source->name, "NULL")
#ifdef HLCSG_PASSBULLETSBRUSH
			|| !strcmp (source->name, "SKIP")
#endif
			)
        {
            Log("Are you sure you included zhlt.wad in your wadpath list?\n");
        }
    }

#ifdef HLCSG_AUTOWAD_NEW
	if (found)
	{
		// get the first and last matching lump
		lumpinfo_t *first = found;
		lumpinfo_t *last = found;
		while (first - 1 >= lumpinfo && lump_sorter_by_name (first - 1, source) == 0)
		{
			first = first - 1;
		}
		while (last + 1 < lumpinfo + nTexLumps && lump_sorter_by_name (last + 1, source) == 0)
		{
			last = last + 1;
		}
		// find the best matching lump
		lumpinfo_t *best = NULL;
		for (found = first; found < last + 1; found++)
		{
			bool better = false;
			if (best == NULL)
			{
				better = true;
			}
			else if (found->iTexFile != best->iTexFile)
			{
				wadpath_t *found_wadpath = texwadpathes[found->iTexFile];
				wadpath_t *best_wadpath = texwadpathes[best->iTexFile];
				if (found_wadpath->usedbymap != best_wadpath->usedbymap)
				{
					better = !found_wadpath->usedbymap; // included wad is better
				}
				else
				{
					better = found->iTexFile < best->iTexFile; // upper in the wad list is better
				}
			}
			else if (found->filepos != best->filepos)
			{
				better = found->filepos < best->filepos; // when there are several lumps with the same name in one wad file
			}

			if (better)
			{
				best = found;
			}
		}
		found = best;
	}
#endif
    return found;
}

// =====================================================================================
//  LoadLump
// =====================================================================================
int             LoadLump(const lumpinfo_t* const source, byte* dest, int* texsize
#ifdef HLCSG_FILEREADFAILURE_FIX
						, int dest_maxsize
#endif
#ifdef ZHLT_NOWADDIR
						, byte *&writewad_data, int &writewad_datasize
#endif
						)
{
#ifdef ZHLT_NOWADDIR
	writewad_data = NULL;
	writewad_datasize = -1;
#endif
    //Log("** PnFNFUNC: LoadLump\n");

    *texsize = 0;
    if (source->filepos)
    {
        if (fseek(texfiles[source->iTexFile], source->filepos, SEEK_SET))
        {
            Warning("fseek to %d failed\n", source->filepos);
#ifdef HLCSG_FILEREADFAILURE_FIX
			Error ("File read failure");
#endif
        }
        *texsize = source->disksize;

#ifdef HLCSG_AUTOWAD_NEW
		if (texwadpathes[source->iTexFile]->usedbymap)
#else
        bool wadinclude = false;
        std::map< int, bool >::iterator it;
        it = s_WadIncludeMap.find(source->iTexFile);
        if (it != s_WadIncludeMap.end())
        {
            wadinclude = it->second;
        }

        // Should we just load the texture header w/o the palette & bitmap?
        if (g_wadtextures && !wadinclude)
#endif
        {
            // Just read the miptex header and zero out the data offsets.
            // We will load the entire texture from the WAD at engine runtime
            int             i;
            miptex_t*       miptex = (miptex_t*)dest;
#ifdef HLCSG_FILEREADFAILURE_FIX
			hlassume ((int)sizeof (miptex_t) <= dest_maxsize, assume_MAX_MAP_MIPTEX);
#endif
            SafeRead(texfiles[source->iTexFile], dest, sizeof(miptex_t));

            for (i = 0; i < MIPLEVELS; i++)
                miptex->offsets[i] = 0;
#ifdef ZHLT_NOWADDIR
			writewad_data = (byte *)malloc (source->disksize);
			hlassume (writewad_data != NULL, assume_NoMemory);
			if (fseek (texfiles[source->iTexFile], source->filepos, SEEK_SET))
				Error ("File read failure");
			SafeRead (texfiles[source->iTexFile], writewad_data, source->disksize);
			writewad_datasize = source->disksize;
#endif
            return sizeof(miptex_t);
        }
        else
        {
			Developer(DEVELOPER_LEVEL_MESSAGE,"Including texture %s\n",source->name);
            // Load the entire texture here so the BSP contains the texture
#ifdef HLCSG_FILEREADFAILURE_FIX
			hlassume (source->disksize <= dest_maxsize, assume_MAX_MAP_MIPTEX);
#endif
            SafeRead(texfiles[source->iTexFile], dest, source->disksize);
            return source->disksize;
        }
    }

#ifdef HLCSG_ERROR_MISSINGTEXTURE
	Error("::LoadLump() texture %s not found!", source->name);
#else
    Warning("::LoadLump() texture %s not found!", source->name);
#endif
    return 0;
}

// =====================================================================================
//  AddAnimatingTextures
// =====================================================================================
void            AddAnimatingTextures()
{
    int             base;
    int             i, j, k;
    char            name[MAXWADNAME];

    base = nummiptex;

    for (i = 0; i < base; i++)
    {
        if ((miptex[i].name[0] != '+') && (miptex[i].name[0] != '-'))
        {
            continue;
        }

        safe_strncpy(name, miptex[i].name, MAXWADNAME);

        for (j = 0; j < 20; j++)
        {
            if (j < 10)
            {
                name[1] = '0' + j;
            }
            else
            {
                name[1] = 'A' + j - 10;                    // alternate animation
            }

            // see if this name exists in the wadfile
            for (k = 0; k < nTexLumps; k++)
            {
                if (!strcmp(name, lumpinfo[k].name))
                {
                    FindMiptex(name);                      // add to the miptex list
                    break;
                }
            }
        }
    }

    if (nummiptex - base)
    {
        Log("added %i additional animating textures.\n", nummiptex - base);
    }
}

#ifndef HLCSG_AUTOWAD_NEW
// =====================================================================================
//  GetWadPath
// AJM: this sub is obsolete
// =====================================================================================
char*           GetWadPath()
{
    const char*     path = ValueForKey(&g_entities[0], "_wad");

    if (!path || !path[0])
    {
        path = ValueForKey(&g_entities[0], "wad");
        if (!path || !path[0])
        {
            Warning("no wadfile specified");
            return strdup("");
        }
    }

    return strdup(path);
}
#endif

// =====================================================================================
//  WriteMiptex
// =====================================================================================
void            WriteMiptex()
{
    int             len, texsize, totaltexsize = 0;
    byte*           data;
    dmiptexlump_t*  l;
    double          start, end;

    g_texdatasize = 0;

    start = I_FloatTime();
    {
        if (!TEX_InitFromWad())
            return;

        AddAnimatingTextures();
    }
    end = I_FloatTime();
    Verbose("TEX_InitFromWad & AddAnimatingTextures elapsed time = %ldms\n", (long)(end - start));

    start = I_FloatTime();
    {
        int             i;

        for (i = 0; i < nummiptex; i++)
        {
            lumpinfo_t*     found;

            found = FindTexture(miptex + i);
            if (found)
            {
                miptex[i] = *found;
#ifdef HLCSG_AUTOWAD_NEW
				texwadpathes[found->iTexFile]->usedtextures++;
#endif
            }
            else
            {
                miptex[i].iTexFile = miptex[i].filepos = miptex[i].disksize = 0;
            }
        }
    }
    end = I_FloatTime();
    Verbose("FindTextures elapsed time = %ldms\n", (long)(end - start));

#ifdef HLCSG_AUTOWAD_NEW
	// Now we have filled lumpinfo for each miptex and the number of used textures for each wad.
	{
		char szTmpWad[MAX_VAL];
		int i;

		szTmpWad[0] = 0;
		for (i = 0; i < nTexFiles; i++)
		{
			wadpath_t *currentwad = texwadpathes[i];
			if (!currentwad->usedbymap && (currentwad->usedtextures > 0 || !g_bWadAutoDetect))
			{
				Log ("Including Wadfile: %s\n", currentwad->path);
				double percused = (double)currentwad->usedtextures / (double)nummiptex * 100;
				Log (" - Contains %i used texture%s, %2.2f percent of map (%d textures in wad)\n",
					 currentwad->usedtextures, currentwad->usedtextures == 1? "": "s", percused, currentwad->totaltextures);
			}
		}
		for (i = 0; i < nTexFiles; i++)
		{
			wadpath_t *currentwad = texwadpathes[i];
			if (currentwad->usedbymap && (currentwad->usedtextures > 0 || !g_bWadAutoDetect))
			{
				Log ("Using Wadfile: %s\n", currentwad->path);
				double percused = (double)currentwad->usedtextures / (double)nummiptex * 100;
				Log (" - Contains %i used texture%s, %2.2f percent of map (%d textures in wad)\n",
					 currentwad->usedtextures, currentwad->usedtextures == 1? "": "s", percused, currentwad->totaltextures);
	#ifdef HLCSG_STRIPWADPATH
				char tmp[_MAX_PATH];
				ExtractFile (currentwad->path, tmp);
				safe_strncat (szTmpWad, tmp, MAX_VAL);
	#else
				safe_strncat (szTmpWad, currentwad->path, MAX_VAL);
	#endif
				safe_strncat (szTmpWad, ";", MAX_VAL);
			}
		}
		
	#ifdef HLCSG_CHART_FIX
		Log("\n");
		if (*szTmpWad)
		{
			Log ("Wad files required to run the map: \"%s\"\n", szTmpWad);
		}
		else
		{
			Log ("Wad files required to run the map: (None)\n");
		}
	#endif
		SetKeyValue(&g_entities[0], "wad", szTmpWad);
	}

#endif
    start = I_FloatTime();
    {
        int             i;
        texinfo_t*      tx = g_texinfo;

        // Sort them FIRST by wadfile and THEN by name for most efficient loading in the engine.
        qsort((void*)miptex, (size_t) nummiptex, sizeof(miptex[0]), lump_sorter_by_wad_and_name);

        // Sleazy Hack 104 Pt 2 - After sorting the miptex array, reset the texinfos to point to the right miptexs
        for (i = 0; i < g_numtexinfo; i++, tx++)
        {
#ifdef HLCSG_TEXMAP64_FIX
            char*          miptex_name = texmap_retrieve(tx->miptex);
#else
            char*          miptex_name = texmap64_retrieve(tx->miptex);
#endif

            tx->miptex = FindMiptex(miptex_name);

#ifndef HLCSG_TEXMAP64_FIX
            // Free up the originally strdup()'ed miptex_name
            free(miptex_name);
#endif
        }
#ifdef HLCSG_TEXMAP64_FIX
		texmap_clear ();
#endif
    }
    end = I_FloatTime();
    Verbose("qsort(miptex) elapsed time = %ldms\n", (long)(end - start));

    start = I_FloatTime();
    {
        int             i;

        // Now setup to get the miptex data (or just the headers if using -wadtextures) from the wadfile
        l = (dmiptexlump_t*)g_dtexdata;
        data = (byte*) & l->dataofs[nummiptex];
        l->nummiptex = nummiptex;
#ifdef ZHLT_NOWADDIR
		char writewad_name[_MAX_PATH];
		FILE *writewad_file;
		int writewad_maxlumpinfos;
		typedef struct
		{
			int             filepos;
			int             disksize;
			int             size;
			char            type;
			char            compression;
			char            pad1, pad2;
			char            name[MAXWADNAME];
		} dlumpinfo_t;
		dlumpinfo_t *writewad_lumpinfos;
		wadinfo_t writewad_header;
		safe_snprintf (writewad_name, _MAX_PATH, "%s.wa_", g_Mapname);
		writewad_file = SafeOpenWrite (writewad_name);
		writewad_maxlumpinfos = nummiptex;
		writewad_lumpinfos = (dlumpinfo_t *)malloc (writewad_maxlumpinfos * sizeof (dlumpinfo_t));
		hlassume (writewad_lumpinfos != NULL, assume_NoMemory);
		writewad_header.identification[0] = 'W';
		writewad_header.identification[1] = 'A';
		writewad_header.identification[2] = 'D';
		writewad_header.identification[3] = '3';
		writewad_header.numlumps = 0;
		if (fseek (writewad_file, sizeof (wadinfo_t), SEEK_SET))
			Error ("File write failure");
#endif
        for (i = 0; i < nummiptex; i++)
        {
            l->dataofs[i] = data - (byte*) l;
#ifdef ZHLT_NOWADDIR
			byte *writewad_data;
			int writewad_datasize;
			len = LoadLump (miptex + i, data, &texsize
#ifdef HLCSG_FILEREADFAILURE_FIX
							, &g_dtexdata[g_max_map_miptex] - data
#endif
							, writewad_data, writewad_datasize);
			if (writewad_data)
			{
				dlumpinfo_t *writewad_lumpinfo = &writewad_lumpinfos[writewad_header.numlumps];
				writewad_lumpinfo->filepos = ftell (writewad_file);
				writewad_lumpinfo->disksize = writewad_datasize;
				writewad_lumpinfo->size = miptex[i].size;
				writewad_lumpinfo->type = miptex[i].type;
				writewad_lumpinfo->compression = miptex[i].compression;
				writewad_lumpinfo->pad1 = miptex[i].pad1;
				writewad_lumpinfo->pad2 = miptex[i].pad2;
				memcpy (writewad_lumpinfo->name, miptex[i].name, MAXWADNAME);
				writewad_header.numlumps++;
				SafeWrite (writewad_file, writewad_data, writewad_datasize);
				free (writewad_data);
			}
#else
            len = LoadLump(miptex + i, data, &texsize
#ifdef HLCSG_FILEREADFAILURE_FIX
							, &g_dtexdata[g_max_map_miptex] - data
#endif
							);
#endif

            if (!len)
            {
                l->dataofs[i] = -1;                        // didn't find the texture
            }
            else
            {
                totaltexsize += texsize;

                hlassume(totaltexsize < g_max_map_miptex, assume_MAX_MAP_MIPTEX);
            }
            data += len;
        }
        g_texdatasize = data - g_dtexdata;
#ifdef ZHLT_NOWADDIR
		writewad_header.infotableofs = ftell (writewad_file);
		SafeWrite (writewad_file, writewad_lumpinfos, writewad_header.numlumps * sizeof (dlumpinfo_t));
		if (fseek (writewad_file, 0, SEEK_SET))
			Error ("File write failure");
		SafeWrite (writewad_file, &writewad_header, sizeof (wadinfo_t));
		if (fclose (writewad_file))
			Error ("File write failure");
#endif
    }
    end = I_FloatTime();
    Log("Texture usage is at %1.2f mb (of %1.2f mb MAX)\n", (float)totaltexsize / (1024 * 1024),
        (float)g_max_map_miptex / (1024 * 1024));
    Verbose("LoadLump() elapsed time = %ldms\n", (long)(end - start));
}

//==========================================================================

// =====================================================================================
//  TexinfoForBrushTexture
// =====================================================================================
int             TexinfoForBrushTexture(const plane_t* const plane, brush_texture_t* bt, const vec3_t origin
#ifdef ZHLT_HIDDENSOUNDTEXTURE
					, bool shouldhide
#endif
					)
{
    vec3_t          vecs[2];
    int             sv, tv;
    vec_t           ang, sinv, cosv;
    vec_t           ns, nt;
    texinfo_t       tx;
    texinfo_t*      tc;
    int             i, j, k;

#ifdef HLCSG_HLBSP_VOIDTEXINFO
	if (!strncasecmp(bt->name, "NULL", 4))
	{
		return -1;
	}
#endif
    memset(&tx, 0, sizeof(tx));
#ifndef HLCSG_CUSTOMHULL
#ifdef HLCSG_PRECISIONCLIP
	if(!strncmp(bt->name,"BEVEL",5))
	{
		tx.flags |= TEX_BEVEL;
		safe_strncpy(bt->name,"NULL",5);
	}
#endif
#endif
#ifndef HLCSG_AUTOWAD_NEW
#ifdef HLCSG_AUTOWAD_TEXTURELIST_FIX
	ThreadLock ();
	autowad_PushName (bt->name);
	ThreadUnlock ();
#endif
#endif
#ifdef HLCSG_TEXMAP64_FIX
	FindMiptex (bt->name);
#else
    tx.miptex = FindMiptex(bt->name);

    // Note: FindMiptex() still needs to be called here to add it to the global miptex array

    // Very Sleazy Hack 104 - since the tx.miptex index will be bogus after we sort the miptex array later
    // Put the string name of the miptex in this "index" until after we are done sorting it in WriteMiptex().
    tx.miptex = texmap64_store(strdup(bt->name));
#endif

    // set the special flag
    if (bt->name[0] == '*'
        || !strncasecmp(bt->name, "sky", 3)

// =====================================================================================
//Cpt_Andrew - Env_Sky Check
// =====================================================================================
        || !strncasecmp(bt->name, "env_sky", 5)
// =====================================================================================

#ifndef HLCSG_CUSTOMHULL
        || !strncasecmp(bt->name, "clip", 4)
#endif
        || !strncasecmp(bt->name, "origin", 6)
#ifdef ZHLT_NULLTEX // AJM
        || !strncasecmp(bt->name, "null", 4)
#endif
        || !strncasecmp(bt->name, "aaatrigger", 10)
       )
    {
		// actually only 'sky' and 'aaatrigger' needs this. --vluzacn
        tx.flags |= TEX_SPECIAL;
    }
#ifdef ZHLT_HIDDENSOUNDTEXTURE
	if (shouldhide)
	{
		tx.flags |= TEX_SHOULDHIDE;
	}
#endif

    if (bt->txcommand)
    {
        memcpy(tx.vecs, bt->vects.quark.vects, sizeof(tx.vecs));
        if (origin[0] || origin[1] || origin[2])
        {
            tx.vecs[0][3] += DotProduct(origin, tx.vecs[0]);
            tx.vecs[1][3] += DotProduct(origin, tx.vecs[1]);
        }
    }
    else
    {
        if (g_nMapFileVersion < 220)
        {
            TextureAxisFromPlane(plane, vecs[0], vecs[1]);
        }

        if (!bt->vects.valve.scale[0])
        {
            bt->vects.valve.scale[0] = 1;
        }
        if (!bt->vects.valve.scale[1])
        {
            bt->vects.valve.scale[1] = 1;
        }

        if (g_nMapFileVersion < 220)
        {
            // rotate axis
            if (bt->vects.valve.rotate == 0)
            {
                sinv = 0;
                cosv = 1;
            }
            else if (bt->vects.valve.rotate == 90)
            {
                sinv = 1;
                cosv = 0;
            }
            else if (bt->vects.valve.rotate == 180)
            {
                sinv = 0;
                cosv = -1;
            }
            else if (bt->vects.valve.rotate == 270)
            {
                sinv = -1;
                cosv = 0;
            }
            else
            {
                ang = bt->vects.valve.rotate / 180 * Q_PI;
                sinv = sin(ang);
                cosv = cos(ang);
            }

            if (vecs[0][0])
            {
                sv = 0;
            }
            else if (vecs[0][1])
            {
                sv = 1;
            }
            else
            {
                sv = 2;
            }

            if (vecs[1][0])
            {
                tv = 0;
            }
            else if (vecs[1][1])
            {
                tv = 1;
            }
            else
            {
                tv = 2;
            }

            for (i = 0; i < 2; i++)
            {
                ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
                nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
                vecs[i][sv] = ns;
                vecs[i][tv] = nt;
            }

            for (i = 0; i < 2; i++)
            {
                for (j = 0; j < 3; j++)
                {
                    tx.vecs[i][j] = vecs[i][j] / bt->vects.valve.scale[i];
                }
            }
        }
        else
        {
            vec_t           scale;

            scale = 1 / bt->vects.valve.scale[0];
            VectorScale(bt->vects.valve.UAxis, scale, tx.vecs[0]);

            scale = 1 / bt->vects.valve.scale[1];
            VectorScale(bt->vects.valve.VAxis, scale, tx.vecs[1]);
        }

        tx.vecs[0][3] = bt->vects.valve.shift[0] + DotProduct(origin, tx.vecs[0]);
        tx.vecs[1][3] = bt->vects.valve.shift[1] + DotProduct(origin, tx.vecs[1]);
    }

    //
    // find the g_texinfo
    //
    ThreadLock();
    tc = g_texinfo;
    for (i = 0; i < g_numtexinfo; i++, tc++)
    {
        // Sleazy hack 104, Pt 3 - Use strcmp on names to avoid dups
#ifdef HLCSG_TEXMAP64_FIX
		if (strcmp (texmap_retrieve (tc->miptex), bt->name) != 0)
#else
        if (strcmp(texmap64_retrieve((tc->miptex)), texmap64_retrieve((tx.miptex))) != 0)
#endif
        {
            continue;
        }
        if (tc->flags != tx.flags)
        {
            continue;
        }
        for (j = 0; j < 2; j++)
        {
            for (k = 0; k < 4; k++)
            {
                if (tc->vecs[j][k] != tx.vecs[j][k])
                {
                    goto skip;
                }
            }
        }
        ThreadUnlock();
        return i;
skip:;
    }

#ifdef HLCSG_HLBSP_REDUCETEXTURE
    hlassume(g_numtexinfo < MAX_INTERNAL_MAP_TEXINFO, assume_MAX_MAP_TEXINFO);
#else
    hlassume(g_numtexinfo < MAX_MAP_TEXINFO, assume_MAX_MAP_TEXINFO);
#endif

    *tc = tx;
#ifdef HLCSG_TEXMAP64_FIX
	tc->miptex = texmap_store (bt->name, false);
#endif
    g_numtexinfo++;
    ThreadUnlock();
    return i;
}

#ifdef HLCSG_HLBSP_VOIDTEXINFO
// Before WriteMiptex(), for each texinfo in g_texinfo, .miptex is a string rather than texture index, so this function should be used instead of GetTextureByNumber.
const char *GetTextureByNumber_CSG(int texturenumber)
{
	if (texturenumber == -1)
		return "";
#ifdef HLCSG_TEXMAP64_FIX
	return texmap_retrieve (g_texinfo[texturenumber].miptex);
#else
	return texmap64_retrieve (g_texinfo[texturenumber].miptex);
#endif
}
#endif

