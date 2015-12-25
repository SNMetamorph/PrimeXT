#ifndef HLVIS_H__
#define HLVIS_H__

#if _MSC_VER >= 1000
#pragma once
#endif

#include "cmdlib.h"
#include "messages.h"
#include "win32fix.h"
#include "log.h"
#include "hlassert.h"
#include "mathlib.h"
#include "bspfile.h"
#include "threads.h"
#include "filelib.h"

#include "zones.h"
#ifdef ZHLT_PARAMFILE
#include "cmdlinecfg.h"
#endif

#ifdef HLVIS_MAXDIST    // AJM: MVD
#define DEFAULT_MAXDISTANCE_RANGE   0
#ifndef HLVIS_MAXDIST_NEW
//#define DEFAULT_POST_COMPILE      0
#define MAX_VISBLOCKERS 512
#endif
#endif

#ifdef ZHLT_PROGRESSFILE // AJM
#define DEFAULT_PROGRESSFILE NULL // progress file is only used if g_progressfile is non-null
#endif

#define DEFAULT_FULLVIS     false
#define DEFAULT_CHART       false
#define DEFAULT_INFO        true
#ifdef SYSTEM_WIN32
#define DEFAULT_ESTIMATE    false
#endif
#ifdef SYSTEM_POSIX
#define DEFAULT_ESTIMATE    true
#endif
#define DEFAULT_FASTVIS     false
#define DEFAULT_NETVIS_PORT 21212
#define DEFAULT_NETVIS_RATE 60

#define	MAX_PORTALS	32768

//#define USE_CHECK_STACK
#define RVIS_LEVEL_1
#define RVIS_LEVEL_2

#define PORTALFILE      "PRT1" // WTF?

#define	MAX_POINTS_ON_FIXED_WINDING	32

typedef struct
{
    bool            original;                              // don't free, it's part of the portal
    int             numpoints;
    vec3_t          points[MAX_POINTS_ON_FIXED_WINDING];
} winding_t;

typedef struct
{
    vec3_t          normal;
    float           dist;
} plane_t;

typedef enum
{ 
    stat_none, 
    stat_working, 
    stat_done 
} vstatus_t;

typedef struct
{
    plane_t         plane;                                 // normal pointing into neighbor
    int             leaf;                                  // neighbor
    winding_t*      winding;
    vstatus_t       status;
    byte*           visbits;
    byte*           mightsee;
    unsigned        nummightsee;
    int             numcansee;
#ifdef ZHLT_NETVIS
    int             fromclient;                            // which client did this come from
#endif
    UINT32          zone;                                  // Which zone is this portal a member of
} portal_t;

typedef struct seperating_plane_s
{
    struct seperating_plane_s* next;
    plane_t         plane;                                 // from portal is on positive side
} sep_t;

typedef struct passage_s
{
    struct passage_s* next;
    int             from, to;                              // leaf numbers
    sep_t*          planes;
} passage_t;

#define	MAX_PORTALS_ON_LEAF		256
typedef struct leaf_s
{
    unsigned        numportals;
    passage_t*      passages;
    portal_t*       portals[MAX_PORTALS_ON_LEAF];
} leaf_t;

typedef struct pstack_s
{
    byte            mightsee[MAX_MAP_LEAFS / 8];           // bit string
#ifdef USE_CHECK_STACK
    struct pstack_s* next;
#endif
    struct pstack_s* head;

    leaf_t*         leaf;
    portal_t*       portal;                                // portal exiting
    winding_t*      source;
    winding_t*      pass;

    winding_t       windings[3];                           // source, pass, temp in any order
    char            freewindings[3];

    const plane_t*  portalplane;

#ifdef RVIS_LEVEL_2
    int             clipPlaneCount;
    plane_t*        clipPlane;
#endif
} pstack_t;

typedef struct
{
    byte*           leafvis;                               // bit string
    //      byte            fullportal[MAX_PORTALS/8];              // bit string
    portal_t*       base;
    pstack_t        pstack_head;
} threaddata_t;

#ifdef HLVIS_MAXDIST
#ifndef HLVIS_MAXDIST_NEW
// AJM: MVD
// Special VISBLOCKER entity structure
typedef struct
{
	char			name[64];
	int				numplanes;
	plane_t			planes[MAX_PORTALS_ON_LEAF];
	int				numnames;
	int				numleafs;
	int				blockleafs[MAX_PORTALS];
	char			blocknames[MAX_VISBLOCKERS][64];
} visblocker_t;
#endif
#endif

extern bool     g_fastvis;
extern bool     g_fullvis;

extern int      g_numportals;
extern unsigned g_portalleafs;

#ifdef HLVIS_MAXDIST // AJM: MVD
extern unsigned int g_maxdistance;
//extern bool		g_postcompile;
#endif
#ifdef HLVIS_OVERVIEW
typedef struct
{
	vec3_t origin;
	int visleafnum;
#ifdef HLVIS_SKYBOXMODEL
	int reverse;
#endif
}
overview_t;
extern const int g_overview_max;
extern overview_t g_overview[];
extern int g_overview_count;

typedef struct
{
	bool isoverviewpoint;
#ifdef HLVIS_OVERVIEW
	bool isskyboxpoint;
#endif
}
leafinfo_t;
extern leafinfo_t *g_leafinfos;
#endif

extern portal_t*g_portals;
extern leaf_t*  g_leafs;

#ifdef HLVIS_MAXDIST // AJM: MVD
#ifndef HLVIS_MAXDIST_NEW
// Visblockers
extern visblocker_t g_visblockers[MAX_VISBLOCKERS];
extern int		g_numvisblockers;
extern byte*	g_mightsee; 
#endif
#endif

extern byte*    g_uncompressed;
extern unsigned g_bitbytes;
extern unsigned g_bitlongs;

extern volatile int g_vislocalpercent;

extern Zones*          g_Zones;

extern void     BasePortalVis(int threadnum);


#ifdef HLVIS_MAXDIST // AJM: MVD
#ifndef HLVIS_MAXDIST_NEW
extern visblocker_t *GetVisBlock(char *name);
extern void		BlockVis(int unused);
#endif
extern void		MaxDistVis(int threadnum);
//extern void		PostMaxDistVis(int threadnum);
#endif

extern void     PortalFlow(portal_t* p);
extern void     CalcAmbientSounds();

#ifdef ZHLT_NETVIS
#include "packet.h"
#include "c2cpp.h"
#include "NetvisSession.h"
#endif

#endif //      byte            fullportal[MAX_PORTALS/8];              // bit string  HLVIS_H__

