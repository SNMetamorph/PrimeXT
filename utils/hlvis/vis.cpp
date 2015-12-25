/*
 
    VISIBLE INFORMATION SET    -aka-    V I S

    Code based on original code from Valve Software, 
    Modified by Sean "Zoner" Cavanaugh (seanc@gearboxsoftware.com) with permission.
    Modified by Tony "Merl" Moore (merlinis@bigpond.net.au)
    Contains code by Skyler "Zipster" York (zipster89134@hotmail.com) - Included with permission.
    
*/

#include "vis.h"
#ifdef ZHLT_LANGFILE
#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#ifdef ZHLT_NETVIS
#include "zlib.h"
#endif

/*

 NOTES

*/

int             g_numportals = 0;
unsigned        g_portalleafs = 0;

portal_t*       g_portals;

leaf_t*         g_leafs;
#ifdef ZHLT_DETAILBRUSH
int				*g_leafstarts;
int				*g_leafcounts;
int				g_leafcount_all;
#endif

// AJM: MVD
#ifdef HLVIS_MAXDIST
#ifndef HLVIS_MAXDIST_NEW
byte*			g_mightsee;
visblocker_t    g_visblockers[MAX_VISBLOCKERS];
int		        g_numvisblockers = 0;
#endif
#endif
//

static byte*    vismap;
static byte*    vismap_p;
static byte*    vismap_end;                                // past visfile
static int      originalvismapsize;

byte*           g_uncompressed;                            // [bitbytes*portalleafs]

unsigned        g_bitbytes;                                // (portalleafs+63)>>3
unsigned        g_bitlongs;

bool            g_fastvis = DEFAULT_FASTVIS;
bool            g_fullvis = DEFAULT_FULLVIS;
bool            g_estimate = DEFAULT_ESTIMATE;
bool            g_chart = DEFAULT_CHART;
bool            g_info = DEFAULT_INFO;

#ifdef HLVIS_MAXDIST
// AJM: MVD
unsigned int	g_maxdistance = DEFAULT_MAXDISTANCE_RANGE;
//bool			g_postcompile = DEFAULT_POST_COMPILE;
//
#endif
#ifdef HLVIS_OVERVIEW
const int		g_overview_max = MAX_MAP_ENTITIES;
overview_t		g_overview[g_overview_max];
int				g_overview_count = 0;
leafinfo_t*		g_leafinfos = NULL;
#endif

#ifdef ZHLT_PROGRESSFILE // AJM
char*           g_progressfile = DEFAULT_PROGRESSFILE; // "-progressfile path"
#endif

static int      totalvis = 0;

#if ZHLT_ZONES
Zones*          g_Zones;
#endif

#ifdef ZHLT_NETVIS
// -- these are definitions and initializations of C/CPP common variables
volatile int    g_visportalindex = UNINITIALIZED_PORTAL_INDEX;  // a client's portal index : current portalindex being worked on


volatile int    g_visportals = 0;                          // the total portals in the map
volatile int    g_visleafs = 0;                            // the total portal leafs in the map
volatile int    g_vislocalportal = 0;                      // number of portals solved locally
volatile enum vis_states g_visstate = VIS_STARTUP;         // current step of execution
volatile enum vis_modes g_vismode = VIS_MODE_NULL;         // style of execution (client or server)
volatile int    g_visleafthread = 0;                       // control flag (are we ready to leafthread)
unsigned int    g_rate = DEFAULT_NETVIS_RATE;
volatile double g_starttime = 0;                           // Start time (from I_FloatTime())
volatile unsigned long g_idletime = 0;                     // Accumulated idle time in milliseconds (rolls over after 46.7 days, hopefully a vis client wont run that long)
volatile unsigned long g_serverindex = 0;                  // client only variable, server index for calculating percentage indicators on the client
short           g_port = DEFAULT_NETVIS_PORT;
const char*     g_server_addr = NULL;


volatile bool   g_bsp_downloaded = false;       // Client variable
volatile bool   g_prt_downloaded = false;       // Client variable
volatile bool   g_mightsee_downloaded = false;  // Client variable

char*           g_bsp_image = NULL;         // Client/Server variable : Server uses it for cache for connecting clients, clients download it to memory to not require filesystem usage 
char*           g_prt_image = NULL;         // Client/Server variable : Server uses it for cache for connecting clients, clients download it to memory to not require filesystem usage 
unsigned long   g_bsp_compressed_size = 0;  // Server variable
unsigned long   g_prt_compressed_size = 0;  // Server variable
unsigned long   g_bsp_size = 0;             // Server variable
unsigned long   g_prt_size = 0;             // Server variable
#endif

#ifdef ZHLT_INFO_COMPILE_PARAMETERS
// AJM: addded in
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
    hlvis(choices) : "HLVIS" : 2 = 
    [ 
        0 : "Off"
        1 : "Fast"
        2 : "Normal" 
        3 : "Full"
    ]
    */
    iTmp = IntForKey(mapent, "hlvis");
    if (iTmp == 0)
    {
        Fatal(assume_TOOL_CANCEL, 
            "%s flag was not checked in info_compile_parameters entity, execution of %s cancelled", g_Program, g_Program);
        CheckFatal();   
    }
    else if (iTmp == 1)
    {
        g_fastvis = true;
        g_fullvis = false;
    }
    else if (iTmp == 2)
    {
        g_fastvis = false;
        g_fullvis = false;
    }
    else if (iTmp == 3)
    {
        g_fullvis = true;
        g_fastvis = false;
    }
    Log("%30s [ %-9s ]\n", "Fast VIS", g_fastvis ? "on" : "off"); 
    Log("%30s [ %-9s ]\n", "Full VIS", g_fullvis ? "on" : "off" );

    ///////////////////
    Log("\n");
}
#endif

// =====================================================================================
//  PlaneFromWinding
// =====================================================================================
static void     PlaneFromWinding(winding_t* w, plane_t* plane)
{
    vec3_t          v1;
    vec3_t          v2;

    // calc plane
    VectorSubtract(w->points[2], w->points[1], v1);
    VectorSubtract(w->points[0], w->points[1], v2);
    CrossProduct(v2, v1, plane->normal);
    VectorNormalize(plane->normal);
    plane->dist = DotProduct(w->points[0], plane->normal);
}

// =====================================================================================
//  NewWinding
// =====================================================================================
static winding_t* NewWinding(const int points)
{
    winding_t*      w;
    int             size;

    if (points > MAX_POINTS_ON_WINDING)
    {
        Error("NewWinding: %i points > MAX_POINTS_ON_WINDING", points);
    }

#ifdef ZHLT_64BIT_FIX
    size = (int)(int)((winding_t*)0)->points[points];
#else
    size = (int)((winding_t*)0)->points[points];
#endif
    w = (winding_t*)calloc(1, size);

    return w;
}

//=============================================================================

/////////
// NETVIS
#ifdef ZHLT_NETVIS

// =====================================================================================
//  GetPortalPtr
//      converts a portal index to a pointer
// =====================================================================================
portal_t*       GetPortalPtr(const long index)
{
    if (index < (g_numportals * 2))
    {
        return g_portals + index;
    }
    else
    {
        return (NULL);
    }
}


// =====================================================================================
//  GetNextPortalIndex
//      This is called by ClientSockets
// =====================================================================================
int             GetNextPortalIndex()
{
    int             j;
    int             best = NO_PORTAL_INDEX;
    portal_t*       p;
    portal_t*       tp;
    int             min;

    ThreadLock();

    min = 99999;
    p = NULL;

    for (j = 0, tp = g_portals; j < g_numportals * 2; j++, tp++)
    {
        if (tp->nummightsee < min && tp->status == stat_none)
        {
            min = tp->nummightsee;
            p = tp;
            best = j;
        }
    }

    if (p)
    {
        p->status = stat_working;
    }
    else
    {
        best = NO_PORTAL_INDEX;                            // hack to return NO_PORTAL_INDEX to the queue'ing code
    }

    ThreadUnlock();

    return best;
}

// =====================================================================================
//  AllPortalsDone
//      returns true if all portals are done...
// =====================================================================================
static int      AllPortalsDone()
{
    const unsigned  numportals = g_numportals * 2;
    portal_t*       tp;

    unsigned        j;
    for (j = 0, tp = g_portals; j < numportals; j++, tp++)
    {
        if (tp->status != stat_done)
        {
            return 0;
        }
    }

    return 1;
}

#endif
// NETVIS
///////////

// =====================================================================================
//  GetNextPortal
//      Returns the next portal for a thread to work on
//      Returns the portals from the least complex, so the later ones can reuse the earlier information.
// =====================================================================================
static portal_t* GetNextPortal()
{
    int             j;
    portal_t*       p;
    portal_t*       tp;
    int             min;

#ifdef ZHLT_NETVIS
    if (g_vismode == VIS_MODE_SERVER)
    {
#else
    {
        if (GetThreadWork() == -1)
        {
            return NULL;
        }
#endif
        ThreadLock();

        min = 99999;
        p = NULL;

        for (j = 0, tp = g_portals; j < g_numportals * 2; j++, tp++)
        {
            if (tp->nummightsee < min && tp->status == stat_none)
            {
                min = tp->nummightsee;
                p = tp;
#ifdef ZHLT_NETVIS
                g_visportalindex = j;
#endif
            }
        }

        if (p)
        {
            p->status = stat_working;
        }

        ThreadUnlock();

        return p;
    }
#ifdef ZHLT_NETVIS
    else                                                   // AS CLIENT
    {
        while (getWorkFromClientQueue() == WAITING_FOR_PORTAL_INDEX)
        {
            unsigned        delay = 100;

            g_idletime += delay;                           // This is the only point where the portal work goes idle, so its easy to add up just how idle it is.
            if (!isConnectedToServer())
            {
                Error("Unexepected disconnect from server(1)\n");
            }
            NetvisSleep(delay);
        }

        if (g_visportalindex == NO_PORTAL_INDEX)
        {
            g_visstate = VIS_CLIENT_DONE;
            Send_VIS_GOING_DOWN(g_ClientSession);
            return NULL;
        }

        // convert index to pointer
        tp = GetPortalPtr(g_visportalindex);

        if (tp)
        {
            tp->status = stat_working;
        }
        return (tp);
    }
#endif
}

#ifdef HLVIS_MAXDIST

#ifndef ZHLT_DETAILBRUSH
// AJM: MVD
// =====================================================================================
//  DecompressAll
// =====================================================================================
void			DecompressAll(void)
{
	int i;
	byte *dest;

	for(i = 0; i < g_portalleafs; i++)
	{
		dest = g_uncompressed + i * g_bitbytes;

		DecompressVis((const unsigned char*)(g_dvisdata + (byte)g_dleafs[i + 1].visofs), dest, g_bitbytes);
	}
}

// AJM: MVD
// =====================================================================================
//  CompressAll
// =====================================================================================
void			CompressAll(void)
{
	int i, x = 0;
	byte *dest;
	byte *src;
	byte	compressed[MAX_MAP_LEAFS / 8];

	vismap_p = vismap;
	
	for(i = 0; i < g_portalleafs; i++)
	{
		memset(&compressed, 0, sizeof(compressed));

		src = g_uncompressed + i * g_bitbytes;
		
		// Compress all leafs into global compression buffer
		x = CompressVis(src, g_bitbytes, compressed, sizeof(compressed));

		dest = vismap_p;
		vismap_p += x;

		if (vismap_p > vismap_end)
		{
	        Error("Vismap expansion overflow");
	    }
	
	    g_dleafs[i + 1].visofs = dest - vismap;            // leaf 0 is a common solid

	    memcpy(dest, compressed, x);
	}
}
#endif

#endif // HLVIS_MAXDIST

// =====================================================================================
//  LeafThread
// =====================================================================================
#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable: 4100)                             // unreferenced formal parameter
#endif

#ifndef ZHLT_NETVIS
static void     LeafThread(int unused)
{
    portal_t*       p;

    while (1)
    {
        if (!(p = GetNextPortal()))
        {
            return;
        }

        PortalFlow(p);

        Verbose("portal:%4i  mightsee:%4i  cansee:%4i\n", (int)(p - g_portals), p->nummightsee, p->numcansee);
    }
}
#endif //!ZHLT_NETVIS

#ifdef ZHLT_NETVIS

static void     LeafThread(int unused)
{
    if (g_vismode == VIS_MODE_CLIENT)
    {
        portal_t*       p;

        g_visstate = VIS_BASE_PORTAL_VIS_SERVER_WAIT;
        Send_VIS_LEAFTHREAD(g_visleafs, g_visportals, g_bitbytes);
        while (!g_visleafthread)
        {
            if (!isConnectedToServer())
            {
                Error("Unexepected disconnect from server(2)\n");
            }
            NetvisSleep(100);
        }
        g_visstate = VIS_PORTAL_FLOW;
        Send_VIS_WANT_FULL_SYNC();

        while (!g_NetvisAbort)
        {
            if (!(p = GetNextPortal()))
            {
                return;
            }

            PortalFlow(p);
            Send_VIS_DONE_PORTAL(g_visportalindex, p);
            g_vislocalportal++;
        }
    }
    else if (g_vismode == VIS_MODE_SERVER)
    {
#if 0
        // Server does zero work in ZHLT netvis
        g_visstate = VIS_WAIT_CLIENTS;
        while (!g_NetvisAbort)
        {
            NetvisSleep(1000);
            if (AllPortalsDone())
            {
                g_visstate = VIS_POST;
                return;
            }
        }
#else
        portal_t*       p;

        g_visstate = VIS_WAIT_CLIENTS;
        while (!g_NetvisAbort)
        {
            if (!(p = GetNextPortal()))
            {
                if (AllPortalsDone())
                {
                    g_visstate = VIS_POST;
                    return;
                }
                NetvisSleep(1000);                         // No need to churn while waiting on slow clients
                continue;
            }
            PortalFlow(p);
            g_vislocalportal++;
        }
#endif
    }
    else
    {
        hlassume(false, assume_VALID_NETVIS_STATE);
    }
}
#endif

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

// =====================================================================================
//  LeafFlow
//      Builds the entire visibility list for a leaf
// =====================================================================================
static void     LeafFlow(const int leafnum)
{
    leaf_t*         leaf;
    byte*           outbuffer;
    byte            compressed[MAX_MAP_LEAFS / 8];
    unsigned        i;
    unsigned        j;
    int             k;
    int             tmp;
    int             numvis;
    byte*           dest;
    portal_t*       p;

    //
    // flow through all portals, collecting visible bits
    //
    memset(compressed, 0, sizeof(compressed));
    outbuffer = g_uncompressed + leafnum * g_bitbytes;
    leaf = &g_leafs[leafnum];
    tmp = 0;

    const unsigned offset = leafnum >> 3;
    const unsigned bit = (1 << (leafnum & 7));

    for (i = 0; i < leaf->numportals; i++)
    {
        p = leaf->portals[i];
        if (p->status != stat_done)
        {
            Error("portal not done (leaf %d)", leafnum);
        }

        {
            byte* dst = outbuffer;
            byte* src = p->visbits;
            for (j=0; j<g_bitbytes; j++, dst++, src++)
            {
                *dst |= *src;
            }
        }

        if ((tmp == 0) && (outbuffer[offset] & bit))
        {
            tmp = 1;
            Warning("Leaf portals saw into leaf");
            Log("    Problem at portal between leaves %i and %i:\n   ", leafnum, p->leaf);
            for (k = 0; k < p->winding->numpoints; k++)
            {
                Log("    (%4.3f %4.3f %4.3f)\n", p->winding->points[k][0], p->winding->points[k][1], p->winding->points[k][2]);
            }
            Log("\n");
        }
    }

    outbuffer[offset] |= bit;

#ifdef HLVIS_OVERVIEW
	if (g_leafinfos[leafnum].isoverviewpoint)
	{
		for (i = 0; i < g_portalleafs; i++)
		{
			outbuffer[i >> 3] |= (1 << (i & 7));
		}
	}
#ifdef HLVIS_SKYBOXMODEL
	for (i = 0; i < g_portalleafs; i++)
	{
		if (g_leafinfos[i].isskyboxpoint)
		{
			outbuffer[i >> 3] |= (1 << (i & 7));
		}
	}
#endif
#endif
    numvis = 0;
    for (i = 0; i < g_portalleafs; i++)
    {
        if (outbuffer[i >> 3] & (1 << (i & 7)))
        {
            numvis++;
        }
    }

    //
    // compress the bit string
    //
    Verbose("leaf %4i : %4i visible\n", leafnum, numvis);
    totalvis += numvis;

#ifdef ZHLT_DETAILBRUSH
	byte buffer2[MAX_MAP_LEAFS / 8];
	int diskbytes = (g_leafcount_all + 7) >> 3;
	memset (buffer2, 0, diskbytes);
	for (i = 0; i < g_portalleafs; i++)
	{
		for (j = 0; j < g_leafcounts[i]; j++)
		{
			int srcofs = i >> 3;
			int srcbit = 1 << (i & 7);
			int dstofs = (g_leafstarts[i] + j) >> 3;
			int dstbit = 1 << ((g_leafstarts[i] + j) & 7);
			if (outbuffer[srcofs] & srcbit)
			{
				buffer2[dstofs] |= dstbit;
			}
		}
	}
	i = CompressVis (buffer2, diskbytes, compressed, sizeof (compressed));
#else
    i = CompressVis(outbuffer, g_bitbytes, compressed, sizeof(compressed));
#endif

    dest = vismap_p;
    vismap_p += i;

    if (vismap_p > vismap_end)
    {
        Error("Vismap expansion overflow");
    }

#ifdef ZHLT_DETAILBRUSH
	for (j = 0; j < g_leafcounts[leafnum]; j++)
	{
		g_dleafs[g_leafstarts[leafnum] + j + 1].visofs = dest - vismap;
	}
#else
    g_dleafs[leafnum + 1].visofs = dest - vismap;            // leaf 0 is a common solid
#endif

    memcpy(dest, compressed, i);
}

// =====================================================================================
//  CalcPortalVis
// =====================================================================================
static void     CalcPortalVis()
{
#ifndef ZHLT_NETVIS
    // g_fastvis just uses mightsee for a very loose bound
    if (g_fastvis)
    {
        int             i;

        for (i = 0; i < g_numportals * 2; i++)
        {
            g_portals[i].visbits = g_portals[i].mightsee;
            g_portals[i].status = stat_done;
        }
        return;
    }
#endif

#ifdef ZHLT_NETVIS
    LeafThread(0);
#else
    NamedRunThreadsOn(g_numportals * 2, g_estimate, LeafThread);
#endif
}

//////////////
// ZHLT_NETVIS

#ifdef ZHLT_NETVIS
// =====================================================================================
//  CalcVis
// =====================================================================================
static void     CalcVis()
{
    unsigned        lastpercent = 0;
    int             x, size;

    if (g_vismode == VIS_MODE_SERVER)
    {
        g_visstate = VIS_BASE_PORTAL_VIS;
        Log("BasePortalVis: \n");
        
        for (x = 0, size = g_numportals * 2; x < size; x++)
        {
            unsigned        percent = (x * 100 / size);
            
            if (percent && (percent != lastpercent) && ((percent % 10) == 0))
            {
                lastpercent = percent;
#ifdef ZHLT_CONSOLE
				PrintConsole
#else
                printf
#endif
					("%d%%....", percent);
            }
            BasePortalVis(x);
        }
#ifdef ZHLT_CONSOLE
		PrintConsole
#else
        printf
#endif
			("\n");
    }
    else
    {
        Send_VIS_WANT_MIGHTSEE_DATA();
        while(!g_mightsee_downloaded)
        {
            if (!isConnectedToServer())
            {
                Error("Unexepected disconnect from server(3)\n");
            }
            NetvisSleep(100);
        }
    }

    g_visportals = g_numportals;
    g_visleafs = g_portalleafs;

    g_starttime = I_FloatTime();
    StartStatusDisplayThread(g_rate);
    CalcPortalVis();

    if (g_vismode == VIS_MODE_SERVER)
    {
        unsigned int    i;

        for (i = 0; i < g_portalleafs; i++)
        {
            LeafFlow(i);
        }

        Log("average leafs visible: %i\n", totalvis / g_portalleafs);
    }
}
#endif

#ifndef ZHLT_NETVIS

#ifdef HLVIS_MAXDIST
#ifndef HLVIS_MAXDIST_NEW
// AJM: MVD
// =====================================================================================
//  GetVisBlock
// =====================================================================================
visblocker_t *GetVisBlock(char *name)
{
	int i;
	visblocker_t *v;

	for(i = 0, v = &g_visblockers[0]; i < g_numvisblockers; i++, v++)
	{
		if(!strcmp(name, v->name))
			return v;
	}

	return NULL;
}

// AJM: MVD
// =====================================================================================
//  InitVisBlock
// =====================================================================================
static void InitVisBlock(void)
{
	char visfile[_MAX_PATH];
	int i;
	int x = 0;
	int num_blocks;
	int num_sides;

#ifdef ZHLT_DEFAULTEXTENSION_FIX
	safe_snprintf(visfile, _MAX_PATH, "%s.vis", g_Mapname);
#else
	strcpy(visfile, g_Mapname);
	DefaultExtension(visfile, ".vis");
#endif

	if(!q_exists(visfile))
		return;

	FILE *fp = fopen(visfile, "r");

	if(!fp)
		return;

	while(!feof(fp))
	{
		fscanf(fp, "%s\n", g_visblockers[x].name);

		fscanf(fp, "%d\n", &num_blocks);
		
		for(i = 0; i < num_blocks; i++)
		{
			fscanf(fp, "%s\n", g_visblockers[x].blocknames[i]);
		}

		g_visblockers[x].numnames = num_blocks;

		fscanf(fp, "%d\n", &num_sides);

		for(i = 0; i < num_sides; i++)
		{
			fscanf(fp, "%f %f %f %f\n", &g_visblockers[x].planes[i].normal[0],
										&g_visblockers[x].planes[i].normal[1],
										&g_visblockers[x].planes[i].normal[2],
										&g_visblockers[x].planes[i].dist);
		}

		g_visblockers[x].numplanes = num_sides;
		g_visblockers[x].numleafs = 0;

		x++;
	}

	g_numvisblockers = x;
}

// AJM: MVD
// =====================================================================================
//  SetupVisBlockLeafs
//      Set up the leafs for the visblocker
// =====================================================================================
static void		SetupVisBlockLeafs(void)
{
	int i, j, k, l, q;
	visblocker_t *v;
	leaf_t *leaf;
	portal_t *p;
	plane_t *plane;
	float dist;

	for(i = 0, v = &g_visblockers[0]; i < g_numvisblockers; i++, v++)
	{
		for(j = 0, leaf = &g_leafs[0]; j < g_portalleafs; j++, leaf++)
		{
			for(q = 0, p = leaf->portals[0]; q < leaf->numportals; q++, p++)
			{
				for(k = 0; k < p->winding->numpoints; k++)
				{
					for(l = 0, plane = &v->planes[0]; l < v->numplanes; l++, plane++)
					{
						dist = DotProduct(p->winding->points[k], plane->normal) - plane->dist;
	
						if(dist > ON_EPSILON)
							goto PostLoop;
					}
				}
			}

PostLoop:
			if(q != leaf->numportals)
				continue;

			// If we reach this point, then the portal is completely inside the visblocker
			v->blockleafs[v->numleafs++] = j;
		}
	}
}
#endif

// AJM: MVD
// =====================================================================================
//  SaveVisData
// =====================================================================================
void		SaveVisData(const char *filename)
{
	int i;
	FILE *fp = fopen(filename, "wb");

	if(!fp)
		return;

	SafeWrite(fp, g_dvisdata, (vismap_p - g_dvisdata));

	// BUG BUG BUG!
	// Leaf offsets need to be saved too!!!!
	for(i = 0; i < g_numleafs; i++)
	{
		SafeWrite(fp, &g_dleafs[i].visofs, sizeof(int));
	}

	fclose(fp);
}

#ifndef HLVIS_MAXDIST_NEW
// AJM: MVD
// =====================================================================================
//  ResetPortalStatus
//      FIX: Used to reset p->status to stat_none; now it justs frees p->visbits
// =====================================================================================
void		ResetPortalStatus(void)
{
	int i;
	portal_t* p = g_portals;

	for(i = 0; i < g_numportals * 2; i++, p++)
	{
		//p->status = stat_none;
		free(p->visbits);
	}
}
#endif

#endif // HLVIS_MAXDIST


// AJM UNDONE HLVIS_MAXDIST THIS!!!!!!!!!!!!!

// AJM: MVD modified
// =====================================================================================
//  CalcVis
// =====================================================================================
static void     CalcVis()
{
    unsigned        i;
	char visdatafile[_MAX_PATH];

#ifdef ZHLT_DEFAULTEXTENSION_FIX
	safe_snprintf(visdatafile, _MAX_PATH, "%s.vdt", g_Mapname);
#else
	strcpy(visdatafile, g_Mapname);
	DefaultExtension(visdatafile, ".vdt");
#endif

	// Remove this file
	unlink(visdatafile);

/*    if(g_postcompile)
	{
		if(!g_maxdistance)
		{
			Error("Must use -maxdistance parameter with -postcompile");
		}

		// Decompress everything so we can edit it
		DecompressAll();
		
		NamedRunThreadsOn(g_portalleafs, g_estimate, PostMaxDistVis);

		// Recompress it
		CompressAll();
	}
	else
	{*/
//		InitVisBlock();
//		SetupVisBlockLeafs();

		NamedRunThreadsOn(g_numportals * 2, g_estimate, BasePortalVis);

//		if(g_numvisblockers)
//			NamedRunThreadsOn(g_numvisblockers, g_estimate, BlockVis);

		// First do a normal VIS, save to file, then redo MaxDistVis

		CalcPortalVis();

		//
		// assemble the leaf vis lists by oring and compressing the portal lists
		//
		for (i = 0; i < g_portalleafs; i++)
		{
	        LeafFlow(i);
	    }

		Log("average leafs visible: %i\n", totalvis / g_portalleafs);

		if(g_maxdistance)
		{
			totalvis = 0;
			
			Log("saving visdata to %s...\n", visdatafile);
			SaveVisData(visdatafile);

			// We need to reset the uncompressed variable and portal visbits
			free(g_uncompressed);
			g_uncompressed = (byte*)calloc(g_portalleafs, g_bitbytes);

			vismap_p = g_dvisdata;

			// We don't need to run BasePortalVis again			
			NamedRunThreadsOn(g_portalleafs, g_estimate, MaxDistVis);

			// No need to run this - MaxDistVis now writes directly to visbits after the initial VIS
			//CalcPortalVis();
		
			for (i = 0; i < g_portalleafs; i++)
			{
			    LeafFlow(i);
			}

#ifndef HLVIS_MAXDIST_NEW
			// FIX: Used to reset p->status to stat_none; now it justs frees p->visbits
			ResetPortalStatus();
#endif

			Log("average maxdistance leafs visible: %i\n", totalvis / g_portalleafs);
		}
//	}
}
#endif //!ZHLT_NETVIS

// ZHLT_NETVIS
//////////////

// =====================================================================================
//  CheckNullToken
// =====================================================================================
static INLINE void FASTCALL CheckNullToken(const char*const token)
{
    if (token == NULL) 
    {
        Error("LoadPortals: Damaged or invalid .prt file\n");
    }
}

// =====================================================================================
//  LoadPortals
// =====================================================================================
static void     LoadPortals(char* portal_image)
{
    int             i, j;
    portal_t*       p;
    leaf_t*         l;
    int             numpoints;
    winding_t*      w;
    int             leafnums[2];
    plane_t         plane;
    const char* const seperators = " ()\r\n\t";
    char*           token;

    token = strtok(portal_image, seperators);
    CheckNullToken(token);
    if (!sscanf(token, "%u", &g_portalleafs))
    {
        Error("LoadPortals: failed to read header: number of leafs");
    }

    token = strtok(NULL, seperators);
    CheckNullToken(token);
    if (!sscanf(token, "%i", &g_numportals))
    {
        Error("LoadPortals: failed to read header: number of portals");
    }
    
    Log("%4i portalleafs\n", g_portalleafs);
    Log("%4i numportals\n", g_numportals);

    g_bitbytes = ((g_portalleafs + 63) & ~63) >> 3;
    g_bitlongs = g_bitbytes / sizeof(long);

    // each file portal is split into two memory portals
    g_portals = (portal_t*)calloc(2 * g_numportals, sizeof(portal_t));
    g_leafs = (leaf_t*)calloc(g_portalleafs, sizeof(leaf_t));
#ifdef HLVIS_OVERVIEW
	g_leafinfos = (leafinfo_t*)calloc(g_portalleafs, sizeof(leafinfo_t));
#endif
#ifdef ZHLT_DETAILBRUSH
	g_leafcounts = (int*)calloc(g_portalleafs, sizeof(int));
	g_leafstarts = (int*)calloc(g_portalleafs, sizeof(int));
#endif

    originalvismapsize = g_portalleafs * ((g_portalleafs + 7) / 8);

    vismap = vismap_p = g_dvisdata;
    vismap_end = vismap + MAX_MAP_VISIBILITY;

#ifdef ZHLT_DETAILBRUSH
	if (g_portalleafs > MAX_MAP_LEAFS)
	{ // this may cause hlvis to overflow, because numportalleafs can be larger than g_numleafs in some special cases
		Error ("Too many portalleafs (g_portalleafs(%d) > MAX_MAP_LEAFS(%d)).", g_portalleafs, MAX_MAP_LEAFS);
	}
	g_leafcount_all = 0;
	for (i = 0; i < g_portalleafs; i++)
	{
		unsigned rval = 0;
		token = strtok(NULL, seperators);
		CheckNullToken(token);
		rval += sscanf(token, "%i", &g_leafcounts[i]);
		if (rval != 1)
		{
			Error("LoadPortals: read leaf %i failed", i);
		}
		g_leafstarts[i] = g_leafcount_all;
		g_leafcount_all += g_leafcounts[i];
	}
	if (g_leafcount_all != g_dmodels[0].visleafs)
	{ // internal error (this should never happen)
		Error ("Corrupted leaf mapping (g_leafcount_all(%d) != g_dmodels[0].visleafs(%d)).", g_leafcount_all, g_dmodels[0].visleafs);
	}
#endif
#ifdef HLVIS_OVERVIEW
	for (i = 0; i < g_portalleafs; i++)
	{
		for (j = 0; j < g_overview_count; j++)
		{
#ifdef ZHLT_DETAILBRUSH
			int d = g_overview[j].visleafnum - g_leafstarts[i];
			if (0 <= d && d < g_leafcounts[i])
#else
			if (g_overview[j].visleafnum == i)
#endif
			{
#ifdef HLVIS_SKYBOXMODEL
				if (g_overview[j].reverse)
				{
					g_leafinfos[i].isskyboxpoint = true;
				}
				else
				{
					g_leafinfos[i].isoverviewpoint = true;
				}
#else
				g_leafinfos[i].isoverviewpoint = true;
#endif
			}
		}
	}
#endif
    for (i = 0, p = g_portals; i < g_numportals; i++)
    {
        unsigned rval = 0;

        token = strtok(NULL, seperators);
        CheckNullToken(token);
        rval += sscanf(token, "%i", &numpoints);
        token = strtok(NULL, seperators);
        CheckNullToken(token);
        rval += sscanf(token, "%i", &leafnums[0]);
        token = strtok(NULL, seperators);
        CheckNullToken(token);
        rval += sscanf(token, "%i", &leafnums[1]);

        if (rval != 3)
        {
            Error("LoadPortals: reading portal %i", i);
        }
        if (numpoints > MAX_POINTS_ON_WINDING)
        {
            Error("LoadPortals: portal %i has too many points", i);
        }
        if (((unsigned)leafnums[0] > g_portalleafs) || ((unsigned)leafnums[1] > g_portalleafs))
        {
            Error("LoadPortals: reading portal %i", i);
        }

        w = p->winding = NewWinding(numpoints);
        w->original = true;
        w->numpoints = numpoints;

        for (j = 0; j < numpoints; j++)
        {
            int             k;
            double          v[3];
            unsigned        rval = 0;

            token = strtok(NULL, seperators);
            CheckNullToken(token);
            rval += sscanf(token, "%lf", &v[0]);
            token = strtok(NULL, seperators);
            CheckNullToken(token);
            rval += sscanf(token, "%lf", &v[1]);
            token = strtok(NULL, seperators);
            CheckNullToken(token);
            rval += sscanf(token, "%lf", &v[2]);

            // scanf into double, then assign to vec_t
            if (rval != 3)
            {
                Error("LoadPortals: reading portal %i", i);
            }
            for (k = 0; k < 3; k++)
            {
                w->points[j][k] = v[k];
            }
        }

        // calc plane
        PlaneFromWinding(w, &plane);

        // create forward portal
        l = &g_leafs[leafnums[0]];
        hlassume(l->numportals < MAX_PORTALS_ON_LEAF, assume_MAX_PORTALS_ON_LEAF);
        l->portals[l->numportals] = p;
        l->numportals++;

        p->winding = w;
        VectorSubtract(vec3_origin, plane.normal, p->plane.normal);
        p->plane.dist = -plane.dist;
        p->leaf = leafnums[1];
        p++;

        // create backwards portal
        l = &g_leafs[leafnums[1]];
        hlassume(l->numportals < MAX_PORTALS_ON_LEAF, assume_MAX_PORTALS_ON_LEAF);
        l->portals[l->numportals] = p;
        l->numportals++;

        p->winding = NewWinding(w->numpoints);
        p->winding->numpoints = w->numpoints;
        for (j = 0; j < w->numpoints; j++)
        {
            VectorCopy(w->points[w->numpoints - 1 - j], p->winding->points[j]);
        }

        p->plane = plane;
        p->leaf = leafnums[0];
        p++;

    }
}

// =====================================================================================
//  LoadPortalsByFilename
// =====================================================================================
static void     LoadPortalsByFilename(const char* const filename)
{
    char* file_image;

    if (!q_exists(filename))
    {
        Error("Portal file '%s' does not exist, cannot vis the map\n", filename);
    }
    LoadFile(filename, &file_image);
    LoadPortals(file_image);
    free(file_image);
}


#if ZHLT_ZONES
// =====================================================================================
//  AssignPortalsToZones
// =====================================================================================
static void     AssignPortalsToZones()
{
    hlassert(g_Zones != NULL);

    UINT32 count = 0;

    portal_t* p;
    UINT32 x;

    UINT32 tmp[20];
    memset(tmp, 0, sizeof(tmp));

    UINT32 numportals = g_numportals * 2;
    for (x=0, p=g_portals; x<numportals; x++, p++)
    {
        BoundingBox bounds;
        winding_t* w = p->winding;
        UINT32 numpoints = w->numpoints;

        UINT32 y;

        for (y=0; y<numpoints; y++)
        {
            bounds.add(w->points[y]);
        }

        p->zone = g_Zones->getZoneFromBounds(bounds);
        tmp[p->zone]++;
        if (p->zone)
        {
            count++;
        }
    }

    for (x=0; x<15; x++)
    {
        Log("Zone %2u : %u\n", x, tmp[x]);
    }
    Log("%u of %u portals were contained in func_vis zones\n", count, numportals);
}
#endif

// =====================================================================================
//  Usage
// =====================================================================================
static void     Usage()
{
    Banner();

    Log("\n-= %s Options =-\n\n", g_Program);
#ifdef ZHLT_CONSOLE
	Log("    -console #      : Set to 0 to turn off the pop-up console (default is 1)\n");
#endif
#ifdef ZHLT_LANGFILE
	Log("    -lang file      : localization file\n");
#endif
    Log("    -full           : Full vis\n");
    Log("    -fast           : Fast vis\n\n");
#ifdef ZHLT_NETVIS
    Log("    -connect address : Connect to netvis server at address as a client\n");
    Log("    -server          : Run as the netvis server\n");
    Log("    -port #          : Use a non-standard port for netvis\n");
    Log("    -rate #          : Alter the display update rate\n\n");
#endif
    Log("    -texdata #      : Alter maximum texture memory limit (in kb)\n");
    Log("    -lightdata #      : Alter maximum lighting memory limit (in kb)\n"); //lightdata //--vluzacn
    Log("    -chart          : display bsp statitics\n");
    Log("    -low | -high    : run program an altered priority level\n");
    Log("    -nolog          : don't generate the compile logfiles\n");
    Log("    -threads #      : manually specify the number of threads to run\n");
#ifdef SYSTEM_WIN32
    Log("    -estimate       : display estimated time during compile\n");
#endif
#ifdef ZHLT_PROGRESSFILE // AJM
    Log("    -progressfile path  : specify the path to a file for progress estimate output\n");
#endif
#ifdef SYSTEM_POSIX
    Log("    -noestimate     : do not display continuous compile time estimates\n");
#endif
#ifdef HLVIS_MAXDIST // AJM: MVD
	Log("    -maxdistance #  : Alter the maximum distance for visibility\n");
#endif
    Log("    -verbose        : compile with verbose messages\n");
    Log("    -noinfo         : Do not show tool configuration information\n");
    Log("    -dev #          : compile with developer message\n\n");
    Log("    mapfile         : The mapfile to compile\n\n");

#ifdef ZHLT_NETVIS
    Log("\n"
        "In netvis one computer must be the server and all the rest are the clients.\n"
        "The server should be started with      : netvis -server mapname\n"
        "And the clients should be started with : netvis -connect servername\n"
        "\n"
        "The default socket it uses is 21212 and can be changed with -port\n"
        "The default update rate is 60 seconds and can be changed with -rate\n");

#endif

    exit(1);
}

// =====================================================================================
//  Settings
// =====================================================================================
static void     Settings()
{
    char*           tmp;

    if (!g_info)
    {
        return; 
    }

    Log("\n-= Current %s Settings =-\n", g_Program);
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

#ifdef HLVIS_MAXDIST // AJM: MVD
    Log("max vis distance    [ %7d ] [ %7d ]\n", g_maxdistance, DEFAULT_MAXDISTANCE_RANGE);
	//Log("max dist only       [ %7s ] [ %7s ]\n", g_postcompile ? "on" : "off", DEFAULT_POST_COMPILE ? "on" : "off");
#endif

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

    // HLVIS Specific Settings
    Log("fast vis            [ %7s ] [ %7s ]\n", g_fastvis ? "on" : "off", DEFAULT_FASTVIS ? "on" : "off");
    Log("full vis            [ %7s ] [ %7s ]\n", g_fullvis ? "on" : "off", DEFAULT_FULLVIS ? "on" : "off");

#ifdef ZHLT_NETVIS
    if (g_vismode == VIS_MODE_SERVER)
    {
        Log("netvis mode         [  Server ]\n");
    }
    else if (g_vismode == VIS_MODE_CLIENT)
    {
        Log("netvis mode         [  Client, connected to %s ]\n", g_server_addr);
    }
    Log("netvis port         [ %7d ] [ %7d ]\n", g_port, DEFAULT_NETVIS_PORT);
    Log("netvis display rate [ %7d ] [ %7d ]\n", g_rate, DEFAULT_NETVIS_RATE);
#endif

    Log("\n\n");
}

#ifdef HLVIS_OVERVIEW
int        VisLeafnumForPoint(const vec3_t point)
{
    int             nodenum;
    vec_t           dist;
    dnode_t*        node;
    dplane_t*       plane;

    nodenum = 0;
    while (nodenum >= 0)
    {
        node = &g_dnodes[nodenum];
        plane = &g_dplanes[node->planenum];
        dist = DotProduct(point, plane->normal) - plane->dist;
        if (dist >= 0.0)
        {
            nodenum = node->children[0];
        }
        else
        {
            nodenum = node->children[1];
        }
    }

    return -nodenum - 2;
}
#endif
// =====================================================================================
//  main
// =====================================================================================
int             main(const int argc, char** argv)
{
    char            portalfile[_MAX_PATH];
    char            source[_MAX_PATH];
    int             i;
    double          start, end;
    const char*     mapname_from_arg = NULL;

#ifdef ZHLT_NETVIS
    g_Program = "netvis";
#else
    g_Program = "hlvis";
#endif

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
    if (argc == 1)
    {
        Usage();
    }

    for (i = 1; i < argc; i++)
    {
        if (!strcasecmp(argv[i], "-threads"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_numthreads = atoi(argv[++i]);
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
        else if (!strcasecmp(argv[i], "-server"))
        {
            g_vismode = VIS_MODE_SERVER;
        }
        else if (!strcasecmp(argv[i], "-connect"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_vismode = VIS_MODE_CLIENT;
                g_server_addr = argv[++i];
            }
            else
            {
                Usage();
            }
        }
        else if (!strcasecmp(argv[i], "-port"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_port = atoi(argv[++i]);
            }
            else
            {
                Usage();
            }
        }
        else if (!strcasecmp(argv[i], "-rate"))
        {
            if (i + 1 < argc)	//added "1" .--vluzacn
            {
                g_rate = atoi(argv[++i]);
            }
            else
            {
                Usage();
            }
            if (g_rate < 5)
            {
                Log("Minimum -rate is 5, setting to 5 seconds\n");
                g_rate = 5;
            }
            if (g_rate > 900)
            {
                Log("Maximum -rate is 900, setting to 900 seconds\n");
                g_rate = 900;
            }
        }
#endif
#ifndef ZHLT_NETVIS
        else if (!strcasecmp(argv[i], "-fast"))
        {
            Log("g_fastvis = true\n");
            g_fastvis = true;
        }
#endif
        else if (!strcasecmp(argv[i], "-full"))
        {
            g_fullvis = true;
        }
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
        else if (!strcasecmp(argv[i], "-lightdata")) //lightdata
        {
            if (i + 1 < argc) //--vluzacn
            {
                int             x = atoi(argv[++i]) * 1024;

                //if (x > g_max_map_lightdata) //--vluzacn
                {
                    g_max_map_lightdata = x; //--vluzacn
                }
            }
            else
            {
                Usage();
            }
        }

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
        
#ifdef HLVIS_MAXDIST
        // AJM: MVD
		else if(!strcasecmp(argv[i], "-maxdistance"))
		{
			if(i + 1 < argc)	//added "1" .--vluzacn
			{
				g_maxdistance = abs(atoi(argv[++i]));
			}
			else
			{
				Usage();
			}
		}
/*		else if(!strcasecmp(argv[i], "-postcompile"))
		{
			g_postcompile = true;
		}*/
#endif
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
            Log("Unknown option \"%s\"", argv[i]);
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

#ifdef ZHLT_NETVIS
    threads_InitCrit();

    if (g_vismode == VIS_MODE_CLIENT)
    {
        ConnectToServer(g_server_addr, g_port);

        while (!isConnectedToServer())
        {
            NetvisSleep(100);
        }
        Send_VIS_LOGIN();
        while (!g_clientid)
        {
            if (!isConnectedToServer())
            {
                Error("Unexepected disconnect from server(4)\n");
            }
            NetvisSleep(100);
        }

        mapname_from_arg = "proxy";
    }
    else if (g_vismode == VIS_MODE_SERVER)
    {
        StartNetvisSocketServer(g_port);

        if (!mapname_from_arg)
        {
            Log("No mapfile specified\n");
            Usage();
        }
    }
    else
    {
        Log("Netvis must be run either as a server (-server)\n" "or as a client (-connect servername)\n\n");
        Usage();
    }

#else

    if (!mapname_from_arg)
    {
        Log("No mapfile specified\n");
        Usage();
    }
#endif

#ifdef ZHLT_NETVIS
    if (g_vismode == VIS_MODE_CLIENT)
    {
        g_log = false;
    }
#endif

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

#ifdef ZHLT_NETVIS
    if (g_vismode == VIS_MODE_CLIENT)
    {
        Log("ZHLT NETVIS Client #%d\n", g_clientid);
        g_log = false;
    }
    else
    {
        Log("ZHLT NETVIS Server\n");
    }
#endif

    CheckForErrorLog();
	
#ifdef ZHLT_64BIT_FIX
#ifdef PLATFORM_CAN_CALC_EXTENT
	hlassume (CalcFaceExtents_test (), assume_first);
#endif
#endif
    dtexdata_init();
    atexit(dtexdata_free);
    // END INIT

    // BEGIN VIS
    start = I_FloatTime();

    safe_strncpy(source, g_Mapname, _MAX_PATH);
    safe_strncat(source, ".bsp", _MAX_PATH);
    safe_strncpy(portalfile, g_Mapname, _MAX_PATH);
    safe_strncat(portalfile, ".prt", _MAX_PATH);

#ifdef ZHLT_NETVIS

    if (g_vismode == VIS_MODE_SERVER)
    {
        LoadBSPFile(source);
        LoadPortalsByFilename(portalfile);

        char* bsp_image;
        char* prt_image;

        g_bsp_size = LoadFile(source, &bsp_image);
        g_prt_size = LoadFile(portalfile, &prt_image);

        g_bsp_compressed_size = (g_bsp_size * 1.01) + 12; // Magic numbers come from zlib documentation
        g_prt_compressed_size = (g_prt_size * 1.01) + 12; // Magic numbers come from zlib documentation

        char* bsp_compressed_image = (char*)malloc(g_bsp_compressed_size);
        char* prt_compressed_image = (char*)malloc(g_prt_compressed_size);

        int rval;

        rval = compress2((byte*)bsp_compressed_image, &g_bsp_compressed_size, (byte*)bsp_image, g_bsp_size, 5);
        if (rval != Z_OK)
        {
            Error("zlib Compression error with bsp image\n");
        }

        rval =  compress2((byte*)prt_compressed_image, &g_prt_compressed_size, (byte*)prt_image, g_prt_size, 7);
        if (rval != Z_OK)
        {
            Error("zlib Compression error with prt image\n");
        }

        free(bsp_image);
        free(prt_image);

        g_bsp_image = bsp_compressed_image;
        g_prt_image = prt_compressed_image;
    }
    else if (g_vismode == VIS_MODE_CLIENT)
    {
        Send_VIS_WANT_BSP_DATA();
        while (!g_bsp_downloaded)
        {
            if (!isConnectedToServer())
            {
                Error("Unexepected disconnect from server(5)\n");
            }
            NetvisSleep(100);
        }
        Send_VIS_WANT_PRT_DATA();
        while (!g_prt_downloaded)
        {
            if (!isConnectedToServer())
            {
                Error("Unexepected disconnect from server(6)\n");
            }
            NetvisSleep(100);
        }
        LoadPortals(g_prt_image);
        free(g_prt_image);
    }

#else // NOT ZHLT_NETVIS

    LoadBSPFile(source);
    ParseEntities();
#ifdef HLVIS_OVERVIEW
	{
		int i;
		for (i = 0; i < g_numentities; i++)
		{
			if (!strcmp (ValueForKey (&g_entities[i], "classname"), "info_overview_point"))
			{
				if (g_overview_count < g_overview_max)
				{
					vec3_t p;
					GetVectorForKey (&g_entities[i], "origin", p);
					VectorCopy (p, g_overview[g_overview_count].origin);
					g_overview[g_overview_count].visleafnum = VisLeafnumForPoint (p);
#ifdef HLVIS_SKYBOXMODEL
					g_overview[g_overview_count].reverse = IntForKey (&g_entities[i], "reverse");
#endif
					g_overview_count++;
				}
			}
		}
	}
#endif
    LoadPortalsByFilename(portalfile);

#   if ZHLT_ZONES
        g_Zones = MakeZones();
        AssignPortalsToZones();
#   endif

#endif

    Settings();
    g_uncompressed = (byte*)calloc(g_portalleafs, g_bitbytes);

    CalcVis();

#ifdef ZHLT_NETVIS

    if (g_vismode == VIS_MODE_SERVER)
    {
        g_visdatasize = vismap_p - g_dvisdata;
        Log("g_visdatasize:%i  compressed from %i\n", g_visdatasize, originalvismapsize);

        if (g_chart)
        {
            PrintBSPFileSizes();
        }

        WriteBSPFile(source);

        end = I_FloatTime();
        LogTimeElapsed(end - start);

        free(g_uncompressed);
        // END VIS

#ifndef SYSTEM_WIN32
        // Talk about cheese . . .
        StopNetvisSocketServer();
#endif

    }
    else if (g_vismode == VIS_MODE_CLIENT)
    {

#ifndef SYSTEM_WIN32
        // Dont ask  . . 
        DisconnectFromServer();
#endif

        end = I_FloatTime();
        LogTimeElapsed(end - start);

        free(g_uncompressed);
        // END VIS
    }
    threads_UninitCrit();

#else // NOT ZHLT_NETVIS

    g_visdatasize = vismap_p - g_dvisdata;
    Log("g_visdatasize:%i  compressed from %i\n", g_visdatasize, originalvismapsize);

    if (g_chart)
    {
        PrintBSPFileSizes();
    }

    WriteBSPFile(source);

    end = I_FloatTime();
    LogTimeElapsed(end - start);

    free(g_uncompressed);
    // END VIS

#endif // ZHLT_NETVIS
#ifdef ZHLT_PARAMFILE
		}
	}
#endif

    return 0;
}

