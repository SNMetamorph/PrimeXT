// AJM: added this file in

#include "csg.h"

wadpath_t*  g_pWadPaths[MAX_WADPATHS];
int         g_iNumWadPaths = 0;    


// =====================================================================================
//  PushWadPath
//      adds a wadpath into the wadpaths list, without duplicates
// =====================================================================================
void        PushWadPath(const char* const path, bool inuse)
{
    wadpath_t*  current;

#ifdef HLCSG_AUTOWAD_NEW
	hlassume (g_iNumWadPaths < MAX_WADPATHS, assume_MAX_TEXFILES);
#else
    if (!strlen(path))
        return; // no path

    // check for pre-existing path
    for (int i = 0; i < g_iNumWadPaths; i++)
    {
        current = g_pWadPaths[i];

        if (!strcmp(current->path, path))
            return; 
    }
#endif

    current = (wadpath_t*)malloc(sizeof(wadpath_t));

    safe_strncpy(current->path, path, _MAX_PATH);
    current->usedbymap = inuse;
    current->usedtextures = 0;  // should be updated later in autowad procedures
#ifdef HLCSG_AUTOWAD_NEW
	current->totaltextures = 0;
#endif

    g_pWadPaths[g_iNumWadPaths++] = current;

#ifdef _DEBUG
    Log("[dbg] PushWadPath: %i[%s]\n", g_iNumWadPaths, path);
#endif
}

#ifndef HLCSG_AUTOWAD_NEW
// =====================================================================================
//  IsUsedWadPath
// =====================================================================================
bool        IsUsedWadPath(const char* const path)
{
    int         i;
    wadpath_t*  current;

    for (i = 0; i < g_iNumWadPaths; i++)
    {
        current = g_pWadPaths[i];
        if (!strcmp(current->path, path))
        {
            if (current->usedbymap)
                return true;

            return false;
        }
    }   

    return false;
}

// =====================================================================================
//  IsListedWadPath
// =====================================================================================
bool        IsListedWadPath(const char* const path)
{
    int         i;
    wadpath_t*  current;

    for (i = 0; i < g_iNumWadPaths; i++)
    {
        current = g_pWadPaths[i];
        if (!strcmp(current->path, path))
            return true;
    }

    return false;
}
#endif

// =====================================================================================
//  FreeWadPaths
// =====================================================================================
void        FreeWadPaths()
{
    int         i;
    wadpath_t*  current;

    for (i = 0; i < g_iNumWadPaths; i++)
    {
        current = g_pWadPaths[i];
        free(current);
    }
}

// =====================================================================================
//  GetUsedWads
//      parse the "wad" keyvalue into wadpath_t structs
// =====================================================================================
void        GetUsedWads()
{
    const char* pszWadPaths;
    char        szTmp[_MAX_PATH];
    int         i, j;

    pszWadPaths = ValueForKey(&g_entities[0], "wad");

#ifdef HLCSG_AUTOWAD_NEW
	for (i = 0; ; )
	{
		for (j = i; pszWadPaths[j] != '\0'; j++)
		{
			if (pszWadPaths[j] == ';')
			{
				break;
			}
		}
		if (j - i > 0)
		{
			int count = qmin (j - i, _MAX_PATH - 1);
			memcpy (szTmp, &pszWadPaths[i], count);
			szTmp[count] = '\0';

			if (g_iNumWadPaths >= MAX_WADPATHS)
			{
				Error ("Too many wad files");
			}
			PushWadPath (szTmp, true);
		}
		if (pszWadPaths[j] == '\0')
		{
			break;
		}
		else
		{
			i = j + 1;
		}
	}
#else
    for(i = 0; i < MAX_WADPATHS; i++)
    {
        memset(szTmp, 0, sizeof(szTmp));    // are you happy zipster?
        for (j = 0; j < _MAX_PATH; j++, pszWadPaths++)
        {
            if (pszWadPaths[0] == ';')
            {
                pszWadPaths++;
                PushWadPath(szTmp, true);
                break;
            }

            if (pszWadPaths[0] == 0)
            {
                PushWadPath(szTmp, true); // fix by AmericanRPG for last wadpath ignorance bug
                return;
            }

            szTmp[j] = pszWadPaths[0];
        }
    }
#endif
}
