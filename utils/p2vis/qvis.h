/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// vis.h

#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "stringlib.h"
#include "filesystem.h"

#include <windows.h>

#define DEFAULT_FASTVIS	false
#define DEFAULT_TESTLEVEL	2
#define DEFAULT_NOSORT	false
#define DEFAULT_FARPLANE	0

#define MAX_PORTALS		MAX_MAP_PORTALS
#define VIS_EPSILON		ON_EPSILON

typedef struct
{
	vec3_t		normal;
	float		dist;
} plane_t;

#define MAX_POINTS_ON_WINDING		64
#define MAX_POINTS_ON_STACK_WINDING	24
#define MAX_PORTALS_ON_LEAF		256
#define MAX_SEPERATORS		MAX_POINTS_ON_WINDING

typedef struct
{
	int		numpoints;
	vec3_t		p[MAX_POINTS_ON_STACK_WINDING];	// variable sized
} winding_t;

typedef enum
{
	stat_none = 0,
	stat_working,
	stat_done
} vstatus_t;

typedef struct
{
	plane_t		plane;		// normal pointing into neighbor
	vec3_t		origin;
	vec_t		radius;
	int		leaf;		// neighbor
	winding_t		*winding;
	vstatus_t		status;
	byte		*visbits;
	byte		*mightsee;
	int		nummightsee;
	int		numcansee;
} portal_t;

typedef struct leaf_s
{
	int		numportals;
	portal_t		*portals[MAX_PORTALS_ON_LEAF];
} leaf_t;
	
typedef struct pstack_s
{
	byte		mightsee[MAX_MAP_LEAFS/8];	// bit string
	struct pstack_s	*head;
	struct pstack_s	*next;
	leaf_t		*leaf;
	portal_t		*portal;			// portal exiting
	winding_t		*source;
	winding_t		*pass;

	winding_t		windings[3];		// source, pass, temp in any order
	int		freewindings[3];

	plane_t		portalplane;
	plane_t		seperators[2][MAX_SEPERATORS];
	int		numseperators[2];
} pstack_t;

typedef struct
{
	byte		*leafvis;		// bit string
	portal_t		*base;
	pstack_t		pstack_head;
} threaddata_t;

extern int	g_numportals;
extern int	g_portalleafs;

extern portal_t	*g_sorted_portals[MAX_MAP_PORTALS*2];
extern portal_t	*g_portals;
extern leaf_t	*g_leafs;

extern int	c_portaltest, c_portalpass, c_portalcheck;
extern int	c_portalskip, c_leafskip;
extern int	c_vistest, c_mighttest;
extern int	c_mightseeupdate;
extern int	c_chains;

extern byte	*vismap, *vismap_p, *vismap_end;	// past visfile

extern byte	*g_uncompressed;
extern int	g_bitbytes;
extern int	g_bitlongs;
extern vec_t	g_farplane;

void LeafFlow( int leafnum );
void BasePortalVis( int threadnum );
void PortalFlow( int portalnum, int threadnum = -1 );
void PortalFlow( portal_t *p );
void CalcAmbientSounds( void );

//
// winding.c
//
winding_t	*AllocWinding( int points );
void FreeWinding( winding_t *w );
winding_t	*ChopWindingEpsilon( winding_t *in, pstack_t *stack, plane_t *split, vec_t epsilon );
void WindingPlane( winding_t *w, vec3_t normal, vec_t *dist );
