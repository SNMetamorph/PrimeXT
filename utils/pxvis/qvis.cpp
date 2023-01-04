/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// qvis.c

#include "qvis.h"
#include "threads.h"
#include "crashhandler.h"
#include "app_info.h"
#include "build_info.h"

int	g_numportals;
int	g_portalleafs;
portal_t	*g_portals;
leaf_t	*g_leafs;
int	*g_leafstarts;
int	*g_leafcounts;
int	g_leafcount_all;

int	c_portaltest, c_portalpass, c_portalcheck;
int	c_totalvis, c_saw_into_leaf, c_optimized;
portal_t	*g_sorted_portals[MAX_MAP_PORTALS*2];

byte	*vismap, *vismap_p, *vismap_end;	// past visfile
int	originalvismapsize;
byte	*g_uncompressed;			// [bitbytes*portalleafs]
int	c_reused;

int	g_bitbytes;			// (portalleafs+63)>>3
int	g_bitlongs;

bool	g_fastvis = DEFAULT_FASTVIS;
bool	g_nosort = DEFAULT_NOSORT;
int	g_testlevel = DEFAULT_TESTLEVEL;
vec_t	g_farplane = DEFAULT_FARPLANE;

//=============================================================================
void prl( leaf_t *l )
{
	portal_t	*p;
	plane_t	pl;
	
	for( int i = 0; i < l->numportals; i++ )
	{
		p = l->portals[i];
		pl = p->plane;
		Msg( "portal %4i to leaf %4i : %7.1f : (%4.1f, %4.1f, %4.1f)\n",
		(int)(p - g_portals), p->leaf, pl.dist, pl.normal[0], pl.normal[1], pl.normal[2] );
	}
}

/*
=============
GetNextPortal

Returns the next portal for a thread to work on
Returns the portals from the least complex, so the later ones can reuse
the earlier information.
=============
*/
portal_t *GetNextPortal( void )
{
	int	i, j, min;
	portal_t	*p, *tp;

	i = GetThreadWork(); // bump the pacifier
	if( i == -1 ) return NULL;

	ThreadLock();

	min = 99999;
	p = NULL;
	
	for( j = 0, tp = g_portals; j < g_numportals * 2; j++, tp++ )
	{
		if( tp->nummightsee < min && tp->status == stat_none )
		{
			min = tp->nummightsee;
			p = tp;
		}
	}

	if( p ) p->status = stat_working;

	ThreadUnlock();

	return p;
}

/*
==============
LeafThread
==============
*/
static void LeafThread( int thread )
{
	portal_t	*p;
		
	while( 1 )
	{
		if(( p = GetNextPortal()) == NULL )
			break;
		PortalFlow( p );
	};
}

//=============================================================================
/*
=============
SortPortals

Sorts the portals from the least complex, so the later ones can reuse
the earlier information.
=============
*/
static int PComp( const void *a, const void *b )
{
	if((*(portal_t **)a)->nummightsee == (*(portal_t **)b)->nummightsee )
		return 0;

	if((*(portal_t **)a)->nummightsee < (*(portal_t **)b)->nummightsee )
		return -1;

	return 1;
}

static void SortPortals( void )
{
	for( int i = 0; i < g_numportals * 2; i++ )
		g_sorted_portals[i] = &g_portals[i];

	if( g_nosort ) return;

	qsort( g_sorted_portals, g_numportals * 2, sizeof( g_sorted_portals[0] ), PComp );
}

//=============================================================================
/*
===============
LeafMerge

Merges the portal visibility for a leaf
===============
*/
void LeafMerge( int leafnum )
{
	leaf_t	*leaf;
	byte	*outbuffer;
	int	i, j, numvis = 0;
	portal_t	*p;
	
	// flow through all portals, collecting visible bits
	outbuffer = g_uncompressed + leafnum * g_bitbytes;
	leaf = &g_leafs[leafnum];

	for( i = 0; i < leaf->numportals; i++ )
	{
		p = leaf->portals[i];

		if( p->status != stat_done )
			COM_FatalError( "portal not done (leaf %d)\n", leafnum );

		for( j = 0; j < g_bitbytes; j++ )
			outbuffer[j] |= p->visbits[j];
	}

	if( CHECKVISBIT( outbuffer, leafnum ))
		c_saw_into_leaf++;

	SETVISBIT( outbuffer, leafnum );
	numvis++;	// count the leaf itself

	for( i = 0; i < g_portalleafs; i++ )
	{
		if( CHECKVISBIT( outbuffer, i ))
			numvis++;
	}

	// compress the bit string
	MsgDev( D_REPORT, "leaf %4i : %4i visible\n", leafnum, numvis );
	c_totalvis += numvis;
}

/*
===============
LeafFlow

Builds the entire visibility list for a leaf
===============
*/
void LeafFlow( int leafnum )
{
	byte	compressed[MAX_MAP_LEAFS/8];
	byte	outbuffer2[MAX_MAP_LEAFS/8];
	int	diskbytes = (g_leafcount_all + 7) >> 3;
	byte	*dest, *data = NULL;
	byte	*outbuffer;
	int	i, j;
	
	memset( compressed, 0, sizeof( compressed ));
	outbuffer = g_uncompressed + leafnum * g_bitbytes;

	// crosscheck the leafs
	for( i = 0; i < g_portalleafs; i++ )
	{
		if( i == leafnum ) continue;

		// sometimes leaf A is visible from leaf B
		// but leaf B is not visible from leaf A
		// fixup this issue - make leaf A invisible from B
		if( CHECKVISBIT( outbuffer, i ))
		{
			byte	*other = g_uncompressed + i * g_bitbytes;

			if( !CHECKVISBIT( other, leafnum ))
			{
				CLEARVISBIT( outbuffer, i );
				c_optimized++;
			}
		}
	}

	memset( outbuffer2, 0, diskbytes );

	for( i = 0; i < g_portalleafs; i++ )
	{
		for( j = 0; j < g_leafcounts[i]; j++ )
		{
			int	visbit = (g_leafstarts[i] + j);

			if( CHECKVISBIT( outbuffer, i ))
				SETVISBIT( outbuffer2, visbit );
		}
	}

	for( i = 0, data = g_uncompressed; i < leafnum; i++, data += g_bitbytes )
	{
		if( !memcmp( data, outbuffer2, g_bitbytes ))
		{
			g_dleafs[g_leafstarts[leafnum]+1].visofs = g_dleafs[i+1].visofs;
			c_reused++;
			return;
		}
	}

	// compress the buffer now
	i = CompressVis( outbuffer2, diskbytes, compressed, sizeof( compressed ));

	dest = vismap_p;
	vismap_p += i;
	
	if( vismap_p > vismap_end )
		COM_FatalError( "Vismap expansion overflow\n" );

	for( j = 0; j < g_leafcounts[leafnum]; j++ )
	{
		g_dleafs[g_leafstarts[leafnum]+j+1].visofs = dest - vismap;
	}

	memcpy( dest, compressed, i );	
}

/*
==================
CalcFastVis

fastvis just uses mightsee for a very loose bound
==================
*/
static void CalcFastVis( void )
{
	for( int i = 0; i < g_numportals * 2; i++ )
	{
		Mem_Free( g_portals[i].visbits );
		g_portals[i].visbits = g_portals[i].mightsee;
		g_portals[i].status = stat_done;
		g_portals[i].mightsee = NULL;
	}
}

/*
==================
CalcPortalVis
==================
*/
static void CalcPortalVis( void )
{
#ifdef HLVIS_SORT_PORTALS
	RunThreadsOnIndividual( g_numportals * 2, true, PortalFlow );
#else
	RunThreadsOn( g_numportals * 2, true, LeafThread );
#endif
	MsgDev( D_REPORT, "portalcheck: %i  portaltest: %i  portalpass: %i\n", c_portalcheck, c_portaltest, c_portalpass );
	MsgDev( D_REPORT, "c_vistest: %i  c_mighttest: %i, c_merged %i\n", c_vistest, c_mighttest, c_mightseeupdate );
}

/*
==================
CalcVis
==================
*/
static void CalcVis( void )
{
	int	i;

	RunThreadsOn( g_numportals * 2, true, BasePortalVis );
#ifdef HLVIS_SORT_PORTALS
	SortPortals ();
#endif
	if( g_fastvis )
	{
		CalcFastVis ();
	}	
	else
	{
		CalcPortalVis ();
	}

	// assemble the leaf vis lists by oring and compressing the portal lists
	for( i = 0; i < g_portalleafs; i++ )
		LeafMerge( i );

	if( c_saw_into_leaf )
		MsgDev( D_WARN, "%i leaf portals saw into leaf\n", c_saw_into_leaf );

	// now crosscheck each leaf's vis and compress
	for( i = 0; i < g_portalleafs; i++ )
		LeafFlow( i );

	MsgDev( D_INFO, "optimized: %d visible leafs %d (%.2f%%)\n", c_optimized, c_totalvis, c_optimized * 100 / (float)c_totalvis );
	MsgDev( D_INFO, "average leafs visible: %i\n", c_totalvis / g_portalleafs );
	MsgDev( D_REPORT, "total leafs visible: %i, reused vis data %i\n", c_totalvis, c_reused );
}

/*
==================
SetPortalSphere
==================
*/
void SetPortalSphere( portal_t *p )
{
	vec3_t	center, edge;
	winding_t	*w;
	int	i;

	VectorClear( center );
	w = p->winding;

	for( i = 0; i < w->numpoints; i++ )
		VectorAdd( center, w->p[i], center );
	VectorScale( center, ( 1.0 / w->numpoints ), center );

	for( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( center, w->p[i], edge );
		float r = VectorLength( edge );
		p->radius = Q_max( r, p->radius );
	}

	VectorCopy( center, p->origin );
}

/*
============
LoadPortals
============
*/
static void LoadPortals( const char *name )
{
	int	i, j, leafnums[2];
	int	maxportals = 0;
	int	numpoints;
	plane_t	plane;
	portal_t	*p;
	leaf_t	*l;
	FILE	*f;
	winding_t	*w;

	f = fopen( name, "r" );
	if( !f ) COM_FatalError( "couldn't read %s\nno vis performed\n", name );

	if( fscanf( f, "%i\n%i\n", &g_portalleafs, &g_numportals ) != 2 )
		COM_FatalError( "LoadPortals: failed to read header\n" );

	Msg( "%4i portalleafs\n", g_portalleafs );
	Msg( "%4i numportals\n", g_numportals );

	if( g_numportals > MAX_MAP_PORTALS )
		COM_FatalError( "MAX_MAP_PORTALS limit exceeded\n" );

	// these counts should take advantage of 64 bit systems automatically
	g_bitbytes = ((g_portalleafs + 63) & ~63) >> 3;
	g_bitlongs = g_bitbytes / sizeof(int);
	
	// each file portal is split into two memory portals
	g_portals = (portal_t *)Mem_Alloc( 2 * g_numportals * sizeof( portal_t ));
	g_leafs = (leaf_t *)Mem_Alloc( g_portalleafs * sizeof( leaf_t ));
	g_leafcounts = (int *)Mem_Alloc( g_portalleafs * sizeof( int ));
	g_leafstarts = (int *)Mem_Alloc( g_portalleafs * sizeof( int ));

	originalvismapsize = g_portalleafs * ((g_portalleafs + 7) / 8 );
	vismap = vismap_p = g_dvisdata;
	vismap_end = vismap + MAX_MAP_VISIBILITY;

	if( g_portalleafs > MAX_MAP_LEAFS )
	{ 
		// this may cause hlvis to overflow, because numportalleafs can be larger than g_numleafs in some special cases
		COM_FatalError( "Too many portalleafs( g_portalleafs( %d ) > MAX_MAP_LEAFS( %d ))\n", g_portalleafs, MAX_MAP_LEAFS );
	}

	g_leafcount_all = 0;

	for( i = 0; i < g_portalleafs; i++ )
	{
		if( fscanf( f, "%i\n", &g_leafcounts[i] ) != 1 )
			COM_FatalError("LoadPortals: read leaf %i failed\n", i );
		g_leafstarts[i] = g_leafcount_all;
		g_leafcount_all += g_leafcounts[i];
	}

	if( g_leafcount_all != g_numvisleafs )
	{
		// internal error (this should never happen)
		COM_FatalError( "LoadPortals: portalleafs %d mismatch bsp visleafs %d\n", g_leafcount_all, g_numvisleafs );
	}
		
	for( i = 0, p = g_portals; i < g_numportals; i++ )
	{
		if( fscanf( f, "%i %i %i ", &numpoints, &leafnums[0], &leafnums[1] ) != 3 )
			COM_FatalError( "LoadPortals: reading portal %i\n", i );

		if( numpoints > MAX_POINTS_ON_WINDING )
			COM_FatalError( "LoadPortals: portal %i has too many points\n", i );

		if((uint)leafnums[0] > g_portalleafs || (uint)leafnums[1] > g_portalleafs )
			COM_FatalError( "LoadPortals: reading portal %i\n", i );
		
		w = p->winding = AllocWinding( numpoints );
		w->numpoints = numpoints;
		
		for( j = 0; j < numpoints; j++ )
		{
			double	v[3];

			// scanf into double, then assign to vec_t
			if( fscanf( f, "(%lf %lf %lf ) ", &v[0], &v[1], &v[2]) != 3 )
				COM_FatalError( "LoadPortals: reading portal %i\n", i );
			VectorCopy( v, w->p[j] );
		}
		fscanf( f, "\n" );
		
		// calc plane
		WindingPlane( w, plane.normal, &plane.dist );

		// create forward portal
		l = &g_leafs[leafnums[0]];
		if( l->numportals == MAX_PORTALS_ON_LEAF )
			COM_FatalError( "Leaf with too many portals\n" );
		l->portals[l->numportals] = p;
		l->numportals++;

		maxportals = Q_max( maxportals, l->numportals );
		
		p->winding = w;
		VectorNegate( plane.normal, p->plane.normal );
		p->plane.dist = -plane.dist;
		p->leaf = leafnums[1];
		SetPortalSphere( p );
		p++;
		
		// create backwards portal
		l = &g_leafs[leafnums[1]];
		if( l->numportals == MAX_PORTALS_ON_LEAF )
			COM_FatalError( "Leaf with too many portals\n" );
		l->portals[l->numportals] = p;
		l->numportals++;

		maxportals = Q_max( maxportals, l->numportals );
		
		p->winding = AllocWinding( w->numpoints );
		p->winding->numpoints = w->numpoints;
		p->plane = plane;
		p->leaf = leafnums[0];

		// reverse winding for backward portal
		for( j = 0; j < w->numpoints; j++ )
			VectorCopy( w->p[w->numpoints-1-j], p->winding->p[j] );
		SetPortalSphere( p );
		p++;

	}
	
	fclose( f );

	if( maxportals > 60 )
		Msg( "^1%d peak portals on a leaf^7\n", maxportals );
	else if( maxportals > 40 )
		Msg( "^3%d peak portals on a leaf^7\n", maxportals );
	else Msg( "^2%d peak portals on a leaf^7\n", maxportals );
}

/*
============
FreePortals
============
*/
static void FreePortals( void )
{
	for( int i = 0; i < g_numportals * 2; i++ )
	{
		portal_t	*p = &g_portals[i];

		if( p->mightsee ) Mem_Free( p->mightsee );
		if( p->visbits ) Mem_Free( p->visbits );
		if( p->winding ) FreeWinding( p->winding );
	}

	Mem_Free( g_leafcounts );
	Mem_Free( g_leafstarts );
	Mem_Free( g_portals );
	Mem_Free( g_leafs );
}

/*
============
PrintVisSettings

show compiler settings like ZHLT
============
*/
static void PrintVisSettings( void )
{
	Msg( "Current %s settings\n", APP_ABBREVIATION );
	Msg( "Name                 |  Setting  |  Default\n" );
	Msg( "---------------------|-----------|-------------------------\n" );
	Msg( "developer             [ %7d ] [ %7d ]\n", GetDeveloperLevel(), DEFAULT_DEVELOPER );
	Msg( "fast vis              [ %7s ] [ %7s ]\n", g_fastvis ? "on" : "off", DEFAULT_FASTVIS ? "on" : "off" );
	Msg( "no sort portals       [ %7s ] [ %7s ]\n", g_nosort ? "on" : "off", DEFAULT_NOSORT ? "on" : "off" );
	Msg( "maxdistance           [ %7d ] [ %7d ]\n", (int)g_farplane, (int)DEFAULT_FARPLANE );
	Msg( "\n" );
}

/*
============
PrintVisUsage

show compiler usage like ZHLT
============
*/
static void PrintVisUsage( void )
{
	Msg( "\n-= %s Options =-\n\n", APP_ABBREVIATION );
	Msg( "    -dev #         : compile with developer message (1 - 4). default is %d\n", DEFAULT_DEVELOPER );
	Msg( "    -threads #     : manually specify the number of threads to run\n" );
 	Msg( "    -fast          : only do first quick pass on vis calculations\n" );
	Msg( "    -nosort        : don't sort portals (disable optimization)\n" );
	Msg( "    -maxdistance   : limit visible distance (e.g. for fogged levels)\n" );
	Msg( "    bspfile        : The bspfile to compile\n\n" );
}

/*
============
CheckDeprecatedParameter

checks should be parameter ignored or not
============
*/
static bool CheckDeprecatedParameter(const char *name)
{
	if (!Q_strcmp(name, "-full"))
		return true;
	else
		return false;
}

/*
===========
main
===========
*/
int main( int argc, char **argv )
{
	char	portalfile[1024];
	char	source[1024];
	int		i;
	double	start, end;
	char	str[64];

	atexit( Sys_CloseLog );
	Sys_SetupCrashHandler();
	source[0] = '\0';

	for( i = 1; i < argc; i++ )
	{
		if (CheckDeprecatedParameter(argv[i]))
		{
			// compatibility issues, does nothing
		}
		else if( !Q_strcmp( argv[i], "-dev" ))
		{
			SetDeveloperLevel( atoi( argv[i+1] ));
			i++;
		}
		else if( !Q_strcmp( argv[i], "-threads" ))
		{
			g_numthreads = atoi( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-fast" ))
		{
			g_fastvis = true;
		}
		else if( !Q_strcmp( argv[i], "-nosort" ))
		{
			g_nosort = true;
		}
		else if( !Q_strcmp( argv[i], "-maxdistance" ))
		{
			g_farplane = atof( argv[i+1] );
			g_farplane = bound( 64.0, g_farplane, 65536.0 * 1.73 );
			i++;
		}
		else if( argv[i][0] == '-' )
		{
			MsgDev( D_ERROR, "\nUnknown option \"%s\"\n", argv[i] );
			break;
		}
		else if( !source[0] )
		{
			Q_strncpy( source, COM_ExpandArg( argv[i] ), sizeof( source ));
			COM_StripExtension( source );
		}
		else
		{
			MsgDev( D_ERROR, "\nUnknown option \"%s\"\n", argv[i] );
			break;
		}
	}

	if (i != argc || !source[0])
	{
		if (!source[0]) {
			Msg("no mapfile specified\n");
		}

		PrintVisUsage();
		exit(1);
	}

	start = I_FloatTime ();

	Sys_InitLogAppend( va( "%s.log", source ));

	Msg( "\n%s %s (%s, commit %s, arch %s, platform %s)\n\n", TOOLNAME, VERSIONSTRING, 
		BuildInfo::GetDate(), 
		BuildInfo::GetCommitHash(), 
		BuildInfo::GetArchitecture(), 
		BuildInfo::GetPlatform()
	);

	PrintVisSettings();

	ThreadSetDefault ();

	Q_strncpy( portalfile, source, sizeof( portalfile ));
	COM_DefaultExtension( portalfile, ".prt" );
	COM_DefaultExtension( source, ".bsp" );

	LoadBSPFile( source );
	LoadPortals( portalfile );
	
	g_uncompressed = (byte *)Mem_Alloc( g_bitbytes * g_portalleafs );

	CalcVis ();

	MsgDev( D_REPORT, "c_chains: %i\n", c_chains );
	g_visdatasize = vismap_p - g_dvisdata;	

	if( originalvismapsize < g_visdatasize )
		MsgDev( D_INFO, "visdatasize: ^1%s expanded from %s^7\n", Q_memprint( g_visdatasize ), Q_memprint( originalvismapsize ));
	else MsgDev( D_INFO, "visdatasize: ^2%s compressed from %s^7\n", Q_memprint( g_visdatasize ), Q_memprint( originalvismapsize ));
	
	CalcAmbientSounds ();

	WriteBSPFile( source );	
	
	if( !g_fastvis )
		unlink( portalfile );
	Mem_Free( g_uncompressed );
	FreePortals();

	Sys_RestoreCrashHandler();
	SetDeveloperLevel( D_REPORT );
	Mem_Check();

	end = I_FloatTime ();
	Q_timestring((int)( end - start ), str );
	Msg( "%s elapsed\n", str );

	return 0;
}
