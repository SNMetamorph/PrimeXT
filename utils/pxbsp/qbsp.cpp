/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// qbsp.c

#include "bsp5.h"
#include "crashhandler.h"
#include "app_info.h"
#include "build_info.h"

//
// command line flags
//
bool	g_nofill = DEFAULT_NOFILL;
bool	g_notjunc = DEFAULT_NOTJUNC;
bool	g_noclip = DEFAULT_NOCLIP;
bool	g_forcevis = DEFAULT_FORCEVIS;

int	g_maxnode_size = DEFAULT_MAXNODE_SIZE;
int	g_merge_level = DEFAULT_MERGE_LEVEL;
vec_t	g_prtepsilon = PRTCHOP_EPSILON;

char	g_pointfilename[1024];
char	g_linefilename[1024];
char	g_portfilename[1024];

//===========================================================================
/*
============
ReadMapPlanes
============
*/
void ReadMapPlanes( const char *source )
{
	char	path[1024];
	size_t	filelen;
	FILE	*f;

	Q_snprintf( path, sizeof( path ), "%s.pln", source );
	f = fopen( path, "rb" );
	if (!f) {
		COM_FatalError("couldn't open %s\n", path);
		return;
	}

	fseek( f, 0, SEEK_END );
	filelen = ftell( f );
	fseek( f, 0, SEEK_SET );

	if( filelen > sizeof( g_mapplanes ))
		COM_FatalError( "insufficient MAX_INTERNAL_MAP_PLANES size\n" );		

	if( filelen % sizeof( plane_t ))
		COM_FatalError( "msimatch plane_t struct between csg and bsp\n" );

	if( fread( g_mapplanes, 1, filelen, f ) != filelen )
		COM_FatalError( "failed to read mapplanes\n" );

	g_nummapplanes = static_cast<int>(filelen / sizeof(plane_t));

	fclose( f );

	// clear out hash_chain, now is output planenum!
	for( int i = 0; i < g_nummapplanes; i++ )
		g_mapplanes[i].outplanenum = -1;
}

/*
============
ReadHullSizes
============
*/
void ReadHullSizes( const char *source )
{
	float	x1, y1, z1;
	float	x2, y2, z2;
	char	path[1024];
	FILE	*f;

	Q_snprintf( path, sizeof( path ), "%s.hsz", source );
	f = fopen( path, "rb" );
	if( !f ) return;	// just use predefined sizes

	for( int i = 0; i < MAX_MAP_HULLS; i++ )
	{
		int count = fscanf( f, "%f %f %f %f %f %f\n", &x1, &y1, &z1, &x2, &y2, &z2 );
		if( count != 6 ) COM_FatalError( "Load hull size (line %i): scanf failure", i + 1 );

		g_hull_size[i][0][0] = x1;
		g_hull_size[i][0][1] = y1;
		g_hull_size[i][0][2] = z1;
		g_hull_size[i][1][0] = x2;
		g_hull_size[i][1][1] = y2;
		g_hull_size[i][1][2] = z2;
	}

	fclose( f );
}

//===========================================================================
/*
==================
NewFaceFromFace

Duplicates the non point information of a face, used by SplitFace and
MergeFace.
==================
*/
face_t *NewFaceFromFace( face_t *in )
{
	face_t	*newf;
	
	newf = AllocFace ();

	newf->planenum = in->planenum;
	newf->texturenum = in->texturenum;	
	newf->original = in->original;
	newf->contents = in->contents;
	newf->detaillevel = in->detaillevel;
	newf->facestyle = in->facestyle;
	
	return newf;
}

/*
==================
SplitFace

==================
*/
void SplitFaceEpsilon( face_t *in, plane_t *split, face_t **front, face_t **back, vec_t epsilon, bool keepsource )
{
	vec_t	*facenormal = NULL;
	winding_t	*frontw, *backw;

	*front = *back = NULL;

	if( in->detaillevel )
		facenormal = g_mapplanes[in->planenum].normal;

	DivideWindingEpsilon( in->w, split->normal, split->dist, epsilon, &frontw, &backw, facenormal );

	if( frontw && backw )
	{
		// face is divided
		*front = NewFaceFromFace( in );
		*back = NewFaceFromFace( in );
		(*front)->w = frontw;
		(*back)->w = backw;

		if( !keepsource )
		{
			// free original
			FreeFace( in );
		}
	}
	else if( frontw != NULL )
	{
		*front = in;
	}
	else if( backw != NULL )
	{
		*back = in;
	}
}

void SplitFace( face_t *in, plane_t *split, face_t **front, face_t **back, bool keepsource )
{
	SplitFaceEpsilon( in, split, front, back, ON_EPSILON, keepsource );
}

//===========================================================================

int	c_activefaces, c_peakfaces;
int	c_activesurfaces, c_peaksurfaces;
int	c_activeportals, c_peakportals;

void PrintMemory( void )
{
	Msg( "faces   : %6i (%6i)\n", c_activefaces, c_peakfaces );
	Msg( "surfaces: %6i (%6i)\n", c_activesurfaces, c_peaksurfaces );
	Msg( "portals : %6i (%6i)\n", c_activeportals, c_peakportals );
}

/*
===========
AllocFace
===========
*/
face_t *AllocFace( void )
{
	face_t	*f;
	
	c_activefaces++;

	if( c_activefaces > c_peakfaces )
		c_peakfaces = c_activefaces;
		
	f = (face_t *)Mem_Alloc( sizeof( face_t ), C_SURFACE );
	f->planenum = -1;

	return f;
}

/*
===========
AddFaceToBounds
===========
*/
void AddFaceToBounds( face_t *f, vec3_t mins, vec3_t maxs )
{
	winding_t	*w = f->w;

	for ( int i = 0; i < w->numpoints; i++ )
		AddPointToBounds( w->p[i], mins, maxs );
}

/*
===========
FreeFace
===========
*/
void FreeFace( face_t *f )
{
	if( !f ) return;

	if( f->w ) FreeWinding( f->w );
	Mem_Free( f, C_SURFACE );

	c_activefaces--;
}

/*
==================
UnlinkFace

release specified face or purge all chain
==================
*/
void UnlinkFace( face_t **head, face_t *face )
{
	face_t	**prev = head;
	face_t	*cur;

	while( 1 )
	{
		cur = *prev;
		if( !cur ) break;

		if( face != NULL && face != cur )
		{
			prev = &cur->next;
			continue;
		}

		*prev = cur->next;
	}
}

/*
==================
CountListFaces

return count of valid faces
==================
*/
int CountListFaces( face_t *list )
{
	int	count = 0;

	for( face_t *f1 = list; f1 != NULL; f1 = f1->next )
		if( f1->w ) count++;

	return count;
}

/*
===========
AllocSurface
===========
*/
surface_t *AllocSurface( void )
{
	surface_t	*s;

	c_activesurfaces++;

	if( c_activesurfaces > c_peaksurfaces )
		c_peaksurfaces = c_activesurfaces;
	
	s = (surface_t *)Mem_Alloc( sizeof( surface_t ), C_SURFACE );
	ClearBounds( s->mins, s->maxs );
	s->planenum = -1;

	return s;
}

/*
===========
FreeSurface
===========
*/
void FreeSurface( surface_t *s )
{
	Mem_Free( s, C_SURFACE );
	c_activesurfaces--;
}

/*
===========
AllocPortal
===========
*/
portal_t *AllocPortal( void )
{
	c_activeportals++;
	if( c_activeportals > c_peakportals )
		c_peakportals = c_activeportals;
	
	return (portal_t *)Mem_Alloc( sizeof( portal_t ), C_PORTAL );
}

/*
===========
FreePortal
===========
*/
void FreePortal( portal_t *p )
{
	c_activeportals--;
	Mem_Free( p, C_PORTAL );
}

/*
===========
AllocNode
===========
*/
node_t *AllocNode( void )
{
	return (node_t *)Mem_Alloc( sizeof( node_t ), C_LEAFNODE );
}

/*
===========
FreeNode
===========
*/
void FreeNode( node_t *n )
{
	Mem_Free( n, C_LEAFNODE );
}

/*
===========
FreeLeaf
===========
*/
void FreeLeaf( node_t *n )
{
	if( n->markfaces )
		Mem_Free( n->markfaces );
	Mem_Free( n, C_LEAFNODE );
}

//===========================================================================
/*
=================
CreateSingleHull
=================
*/
void CreateSingleHull( const char *source, int hullnum )
{
	int	modnum = 0;
	FILE	*brushfile;
	FILE	*polyfile;
	char	name[1024];
	tree_t	*tree;

	Msg( "CreateHull: %i\n", hullnum );

	Q_snprintf( name, sizeof( name ), "%s.p%i", source, hullnum );
	polyfile = fopen( name, "r" );
	if (!polyfile) {
		COM_FatalError("Can't open %s", name);
		return;
	}

	Q_snprintf( name, sizeof( name ), "%s.b%i", source, hullnum );
	brushfile = fopen( name, "r" );
	if (!brushfile) {
		COM_FatalError("Can't open %s", name);
		return;
	}

	while(( tree = MakeTreeFromHullFaces( polyfile, brushfile )) != NULL )
	{
		tree = TreeProcessModel( tree, modnum, hullnum );
	
		if( hullnum == 0 ) EmitDrawNodes( tree );
		else EmitClipNodes( tree, modnum, hullnum );

		FreeTree( tree );
		modnum++;
	}

	Q_snprintf( name, sizeof( name ), "%s.p%i", source, hullnum );
	fclose( polyfile );
	unlink( name );

	Q_snprintf( name, sizeof( name ), "%s.b%i", source, hullnum );
	fclose( brushfile );
	unlink( name );
}

/*
=================
ProcessFile

=================
*/
void ProcessFile( const char *source )
{
	char	bspfilename[1024];
	char	name[1024];
	int	i;

	// create filenames
	Q_snprintf( g_portfilename, sizeof( g_portfilename ), "%s.prt", source );
	remove( g_portfilename );

	Q_snprintf( g_pointfilename, sizeof( g_pointfilename ), "%s.pts", source );
	remove( g_pointfilename );

	Q_snprintf( g_linefilename, sizeof( g_linefilename ), "%s.lin", source );
	remove( g_linefilename );

	// reading hull sizes from text-file
	ReadHullSizes( source );

	// reading planes from binary dump
	ReadMapPlanes( source );

	// load the output of qcsg
	Q_snprintf( bspfilename, sizeof( bspfilename ), "%s.bsp", source );
	LoadBSPFile( bspfilename );

	ParseEntities ();

	// init the tables to be shared by all models
	BeginBSPFile ();

	for( i = 0; i < MAX_MAP_HULLS; i++ )
	{
		CreateSingleHull( source, i );
	}

	// write the updated bsp file out
	FinishBSPFile( bspfilename );

	Q_snprintf( name, sizeof( name ), "%s.pln", source );
	unlink( name );

	Q_snprintf( name, sizeof( name ), "%s.hsz", source );
	unlink( name );
}

//=========================================
/*
============
GetNodeSize

print node size
============
*/
const char *GetNodeSize( int nodesize )
{
	if( nodesize == DEFAULT_MAXNODE_SIZE )
		return va( "%s", "Auto" );
	return va( "%d", nodesize );
}

/*
============
PrintBspSettings

show compiler settings like ZHLT
============
*/
static void PrintBspSettings( void )
{
	Msg( "Current %s settings\n", APP_ABBREVIATION );
	Msg( "Name                 |  Setting  |  Default\n" );
	Msg( "---------------------|-----------|-------------------------\n" );
	Msg( "developer             [ %7d ] [ %7d ]\n", GetDeveloperLevel(), DEFAULT_DEVELOPER );
	Msg( "max node size         [ %7s ] [ %7s ]\n", GetNodeSize( g_maxnode_size ), GetNodeSize( DEFAULT_MAXNODE_SIZE ));
	Msg( "merge faces depth     [ %7d ] [ %7d ]\n", g_merge_level, DEFAULT_MERGE_LEVEL );
	Msg( "notjunc               [ %7s ] [ %7s ]\n", g_notjunc ? "on" : "off", DEFAULT_NOTJUNC ? "on" : "off" );
	Msg( "noclip                [ %7s ] [ %7s ]\n", g_noclip ? "on" : "off", DEFAULT_NOCLIP ? "on" : "off" );
	Msg( "nofill                [ %7s ] [ %7s ]\n", g_nofill ? "on" : "off", DEFAULT_NOFILL ? "on" : "off" );
	Msg( "portal chop epsilon   [ %.6f] [ %.6f]\n", g_prtepsilon, PRTCHOP_EPSILON );
	Msg( "force vis             [ %7s ] [ %7s ]\n", g_forcevis ? "on" : "off", DEFAULT_FORCEVIS ? "on" : "off" );
	Msg( "\n" );
}

/*
============
PrintBspUsage

show compiler usage like ZHLT
============
*/
static void PrintBspUsage( void )
{
	Msg( "\n-= %s Options =-\n\n", APP_ABBREVIATION );
	Msg( "    -dev #           : compile with developer message (1 - 4). default is %d\n", DEFAULT_DEVELOPER );
	Msg( "    -threads #       : manually specify the number of threads to run\n" );
	Msg( "    -noclip          : don't create clipping hulls\n" );
	Msg( "    -notjunc         : don't break edges on t-junctions (not for final runs)\n" );
 	Msg( "    -nofill          : don't fill outside (used for brush models, not levels)\n" );
 	Msg( "    -noforcevis      : don't make a .prt if the map leaks\n" );
	Msg( "    -merge           : merge together chopped faces on nodes (merging depth level)\n" );
	Msg( "    -maxnodesize val : sets the maximum portal node size\n" );
	Msg( "    -epsilon         : portal chop precision epsilon\n" );
	Msg( "    mapfile          : the mapfile to compile\n\n" );
}

/*
==================
main

==================
*/
int main( int argc, char **argv )
{
	int		i;
	double	start, end;
	char	source[1024];
	char	str[64];

	atexit( Sys_CloseLog );
	Sys_SetupCrashHandler();
	source[0] = '\0';

	for( i = 1; i < argc; i++ )
	{
		if( !Q_strcmp( argv[i], "-dev" ))
		{
			SetDeveloperLevel( atoi( argv[i+1] ));
			i++;
		}
		else if( !Q_strcmp( argv[i], "-threads" ))
		{
			g_numthreads = atoi( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-noclip" ))
		{
			g_noclip = true;
		}
		else if( !Q_strcmp( argv[i], "-notjunc" ))
		{
			g_notjunc = true;
		}
		else if( !Q_strcmp( argv[i], "-nofill" ))
		{
			g_nofill = true;
		}
		else if( !Q_strcmp( argv[i], "-noforcevis" ))
		{
			g_forcevis = false;
		}
		else if( !Q_strcmp( argv[i], "-merge" ))
		{
			g_merge_level = atoi( argv[i+1] );
			g_merge_level = bound( 0, g_merge_level, 4 );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-maxnodesize" ))
		{
			g_maxnode_size = atoi( argv[i+1] );
			g_maxnode_size = bound( 256, g_maxnode_size, 65536 );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-epsilon" ))
		{
			g_prtepsilon = atof( argv[i+1] );
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

		PrintBspUsage();
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

	PrintBspSettings();
	ThreadSetDefault ();
	ProcessFile( source );
	FreeEntities();
	Sys_RestoreCrashHandler();

	// now check for leaks
	SetDeveloperLevel( D_REPORT );
	Mem_Check();

	end = I_FloatTime ();
	Q_timestring((int)( end - start ), str );
	Msg( "%s elapsed\n", str );

	return 0;
}
