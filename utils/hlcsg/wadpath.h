// AJM: added file in
#ifndef WADPATH_H__
#define WADPATH_H__
#include "cmdlib.h" //--vluzacn

#define MAX_WADPATHS 128    // arbitrary

typedef struct    
{
    char            path[_MAX_PATH];
    bool            usedbymap;        // does this map requrie this wad to be included in the bsp?
    int             usedtextures;         // number of textures in this wad the map actually uses
#ifdef HLCSG_AUTOWAD_NEW
	int             totaltextures;
#endif
} wadpath_t;//!!! the above two are VERY DIFFERENT. ie (usedtextures == 0) != (usedbymap == false)

extern wadpath_t*  g_pWadPaths[MAX_WADPATHS];
extern int         g_iNumWadPaths;    


extern void        PushWadPath(const char* const path, bool inuse);
#ifndef HLCSG_AUTOWAD_NEW
extern bool        IsUsedWadPath(const char* const path);
extern bool        IsListedWadPath(const char* const path);
#endif
extern void        FreeWadPaths();
extern void        GetUsedWads();

#endif // WADPATH_H__
