/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "csg.h"
#include "crashhandler.h"
#include "app_info.h"
#include "build_info.h"

// default compiler settings
#define DEFAULT_ONLYENTS		false
#define DEFAULT_NULLIFYTRIGGER	true
#define DEFAULT_WADTEXTURES		true
#define DEFAULT_NOCLIP			false
#define DEFAULT_COMPAT_GOLDSRC	false
//#define ENABLE_TESTMAP

// actual compiler settings
bool		g_onlyents = DEFAULT_ONLYENTS;
bool		g_wadtextures = DEFAULT_WADTEXTURES;
bool		g_nullifytrigger = DEFAULT_NULLIFYTRIGGER;
bool		g_noclip = DEFAULT_NOCLIP;
bool		g_compatibility_goldsrc = DEFAULT_COMPAT_GOLDSRC;
vec_t		g_csgepsilon = CSGCHOP_EPSILON;

static FILE	*out_surfaces[MAX_MAP_HULLS];
static FILE	*out_detbrush[MAX_MAP_HULLS];
vec3_t		world_mins, world_maxs, world_size;
static FILE	*test_mapfile = NULL;

//======================================================================
/*
===========
EmitFace
===========
*/
void EmitFace( int hull, const bface_t *f, int detaillevel )
{
	// don't write out the discardable faces
	if( FBitSet( f->flags, FSIDE_SKIP ))
		return;

	ThreadLock();

	fprintf( out_surfaces[hull], "%i %i %i %i %i\n", detaillevel, f->planenum, f->texinfo, f->contents[0], f->w->numpoints );

	for( int i = 0; i < f->w->numpoints; i++ )
	{
		fprintf( out_surfaces[hull], "%5.8f %5.8f %5.8f\n", f->w->p[i][0], f->w->p[i][1], f->w->p[i][2] );
	}

	// put in an extra line break
	fprintf( out_surfaces[hull], "\n" );

	ThreadUnlock ();
}

/*
===========
EmitDetailBrush
===========
*/
void EmitDetailBrush( int hull, const bface_t *faces )
{
	ThreadLock();

	fprintf( out_detbrush[hull], "0\n" );

	for( const bface_t *f = faces; f != NULL; f = f->next )
	{
		fprintf( out_detbrush[hull], "%i %u\n", f->planenum, f->w->numpoints );

		for( int i = 0; i < f->w->numpoints; i++ )
		{
			fprintf( out_detbrush[hull], "%5.8f %5.8f %5.8f\n", f->w->p[i][0], f->w->p[i][1], f->w->p[i][2] );
		}
	}

	// write end marker
	fprintf( out_detbrush[hull], "-1 -1\n" );

	ThreadUnlock();
}

/*
============
EmitPlanes
============
*/
void EmitPlanes( void )
{
	dplane_t	*dp;
	plane_t	*mp;

	g_numplanes = g_nummapplanes;
	mp = g_mapplanes;
	dp = g_dplanes;

	for( int i = 0; i < g_nummapplanes; i++, mp++, dp++ )
	{
		VectorCopy( mp->normal, dp->normal );
		dp->dist = mp->dist;
		dp->type = mp->type;
	}
}

/*
==================
WriteMapKeyValues

test.map key-values
==================
*/
void WriteMapKeyValues( mapent_t *ent )
{
	char	key[1024], value[1024];
	epair_t	*ep;

	if( !test_mapfile ) return;

	for( ep = ent->epairs; ep; ep = ep->next )
	{
		Q_strncpy( key, ep->key, sizeof( key ));
		StripTrailing( key );
		Q_strncpy( value, ep->value, sizeof( value ));
		StripTrailing( value );
		fprintf( test_mapfile, "\"%s\" \"%s\"\n", key, value );
	}
}

void WriteMapFace( bface_t *f )
{
	dtexinfo_t	*tx = NULL;
	char		texname[64];
	texvecs_t	valve;

	if( !test_mapfile ) return;

	fprintf( test_mapfile, "  " );

	// write three plane points
	fprintf( test_mapfile, "( %g %g %g ) ", f->w->p[0][0], f->w->p[0][1], f->w->p[0][2] );
	fprintf( test_mapfile, "( %g %g %g ) ", f->w->p[1][0], f->w->p[1][1], f->w->p[1][2] );
	fprintf( test_mapfile, "( %g %g %g ) ", f->w->p[2][0], f->w->p[2][1], f->w->p[2][2] );

	if( f->texinfo != -1 )
	{
		vec_t	length;
		vec3_t	axis;

		tx = &g_texinfo[f->texinfo];

		for( int i = 0; i < 2; i++ )
		{
			for( int j = 0; j < 3; j++ )
				axis[j] = tx->vecs[i][j];
			length = VectorNormalize( axis );

			// avoid division by 0
			if( length != 0.0 )
				valve.scale[i] = 1.0 / length;
			else valve.scale[i] = 0.0;

			valve.shift[i] = tx->vecs[i][3];
			if( !i ) VectorCopy( axis, valve.UAxis );
			else VectorCopy( axis, valve.VAxis );
		}

		Q_strncpy(texname, TEX_GetMiptexNameByHash(tx->miptex), sizeof(texname));
	}
	else
	{
		TextureAxisFromNormal( f->plane->normal, valve.UAxis, valve.VAxis, false );
		valve.shift[0] = valve.shift[1] = 0.0f;
		valve.scale[0] = valve.scale[1] = 1.0f;

		if( FBitSet( f->flags, FSIDE_SKIP ))
			Q_strcpy( texname, "SKIP" );
		else if( FBitSet( f->flags, FSIDE_SOLIDHINT ))
			Q_strcpy( texname, "SOLIDHINT" );
		else if( FBitSet( f->flags, FSIDE_HINT ))
			Q_strcpy( texname, "HINT" );
		else Q_strcpy( texname, "NULL" ); // fallback
	}

	fprintf( test_mapfile, "%s [ ", texname );
	fprintf( test_mapfile, "%g ", valve.UAxis[0] );
	fprintf( test_mapfile, "%g ", valve.UAxis[1] );
	fprintf( test_mapfile, "%g ", valve.UAxis[2] );
	fprintf( test_mapfile, "%g ", valve.shift[0] );
	fprintf( test_mapfile, "] [ " );
	fprintf( test_mapfile, "%g ", valve.VAxis[0] );
	fprintf( test_mapfile, "%g ", valve.VAxis[1] );
	fprintf( test_mapfile, "%g ", valve.VAxis[2] );
	fprintf( test_mapfile, "%g ", valve.shift[1] );
	fprintf( test_mapfile, "] 0 " ); // rotate (unused)
	fprintf( test_mapfile, "%g ", valve.scale[0] );
	fprintf( test_mapfile, "%g ", valve.scale[1] );
	fprintf( test_mapfile, "\n" );
}

/*
==================
WriteMapBrushes

test.map brushes
==================
*/
void WriteMapBrushes( brush_t *b, bface_t *outside )
{
//	outside = b->hull[0].faces;

	if( !outside || !test_mapfile )
		return; // no faces?

	ThreadLock();

	fprintf( test_mapfile, " {\n" );

	for( bface_t *f = outside; f != NULL; f = f->next )
	{
		if( WindingArea( f->w ) <= 0.0 )
			continue;

		// write face
		WriteMapFace( f );
	}

	fprintf( test_mapfile, " }\n" );

	ThreadUnlock();
}

//======================================================================
/*
============
ProcessModels
============
*/
void ProcessModels( const char *source )
{
	char	name[1024];
	int	i;

	// open surface and detail files
	for( i = 0; i < MAX_MAP_HULLS; i++ )
	{
		Q_snprintf( name, sizeof( name ), "%s.p%i", source, i );
		out_surfaces[i] = fopen( name, "w" );
		if( !out_surfaces[i] ) COM_FatalError( "couldn't open %s\n", name );

		Q_snprintf( name, sizeof( name ), "%s.b%i", source, i );
		out_detbrush[i] = fopen( name, "w" );
		if( !out_detbrush[i] ) COM_FatalError( "couldn't open %s\n", name );
	}

	// DEBUG: write test map
	Q_snprintf( name, sizeof( name ), "%s_csg.map", source );
#ifdef ENABLE_TESTMAP
	test_mapfile = fopen( name, "w" );
#endif

	for( i = 0; i < g_mapentities.Count(); i++ )
	{
		mapent_t *ent = &g_mapentities[i];

		if( !ent->epairs ) continue; // ent got removed

		if( test_mapfile )
		{
			fprintf( test_mapfile, "{\n" );
			WriteMapKeyValues( ent );
		}

		if( ent->numbrushes )
		{
			ChopEntityBrushes( ent );

			// write end of model marker
			for( int j = 0; j < MAX_MAP_HULLS; j++ )
			{
				if( j != 0 && VectorIsNull( g_hull_size[j][0] ) && VectorIsNull( g_hull_size[j][1] ))
					continue;

				fprintf( out_surfaces[j], "-1 -1 -1 -1 -1\n" );
				fprintf( out_detbrush[j], "-1\n" );
			}
		}

		if( test_mapfile )
			fprintf( test_mapfile, "}\n" );
	}

	// close surface and detail files
	for( i = 0; i < MAX_MAP_HULLS; i++ )
	{
		fclose( out_surfaces[i] );
		fclose( out_detbrush[i] );
	}

	if( test_mapfile )
		fclose( test_mapfile );
	EmitPlanes(); // VHLT compatible (P2 compilers will be ignore it)

	Msg( "\n" );
}

/*
============
WriteHullSizes
============
*/
void WriteHullSizes( const char *source )
{
	float	x1, y1, z1;
	float	x2, y2, z2;
	char	path[1024];
	FILE	*f;

	Q_snprintf( path, sizeof( path ), "%s.hsz", source );
	f = fopen( path, "w" );
	if( !f ) COM_FatalError( "couldn't open %s\n", path );

	// g-cont. may be better store these sizes as keyvalues in worldspawn so engine can read them too
	for( int i = 0; i < MAX_MAP_HULLS; i++ )
	{
		x1 = g_hull_size[i][0][0];
		y1 = g_hull_size[i][0][1];
		z1 = g_hull_size[i][0][2];
		x2 = g_hull_size[i][1][0];
		y2 = g_hull_size[i][1][1];
		z2 = g_hull_size[i][1][2];
		fprintf( f, "%g %g %g %g %g %g\n", x1, y1, z1, x2, y2, z2 );
	}

	fclose( f );
}

/*
============
WriteMapPlanes
============
*/
void WriteMapPlanes( const char *source )
{
	char	path[1024];
	size_t	len;
	FILE	*f;

	Q_snprintf( path, sizeof( path ), "%s.pln", source );
	f = fopen( path, "wb" );
	if( !f ) COM_FatalError( "couldn't open %s\n", path );

	len = g_nummapplanes * sizeof( plane_t );

	if( fwrite( g_mapplanes, 1, len, f ) != len )
		COM_FatalError( "failed to store mapplanes\n" );

	fclose( f );
}

//======================================================================
/*
============
SetModelNumbers
============
*/
void SetModelNumbers( void )
{
	char	value[10];

	for( int mod = 1, i = 1; i < g_mapentities.Count(); i++ )
	{
		if( g_mapentities[i].numbrushes <= 0 )
			continue;

		Q_snprintf( value, sizeof( value ), "*%i", mod++ );
		SetKeyValue( (entity_t *)&g_mapentities[i], "model", value );
	}
}

/*
============
SetLightStyles
============
*/
void SetLightStyles( void )
{
	char	value[10];
	char	lighttargets[MAX_SWITCHED_LIGHTS][64];
	bool	newtexlight = false;
	int		i, j, stylenum = 0;

	// any light that is controlled (has a targetname)
	// must have a unique style number generated for it
	for( i = 1; i < g_mapentities.Count(); i++ )
	{
		mapent_t	*e = &g_mapentities[i];
		const char	*classname = ValueForKey( (entity_t *)e, "classname" );
		const char	*t = NULL;

		if( Q_strncmp( classname, "light", 5 ))
		{
			// if it's not a normal light entity, allocate it a new style if necessary.
			// xash func_light (a simple prefab for switchable texlight)
			if( !Q_strncmp( classname, "func_light", 10 ))
			{
				// func_light always has style -1
				t = "-1";
			}
			else
			{
				// if it's not a normal light entity, allocate it a new style if necessary.
				t = ValueForKey( (entity_t *)e, "style" );
			}

			int style = atoi( t );

			// ignore std styles (may be some settings in quake)
			if( style >= 0 && style <= 20 )
				continue;

			switch( style )
			{
			case -1:	// normal switchable texlight
				Q_snprintf( value, sizeof( value ), "%i",  (32 + stylenum));
				SetKeyValue( (entity_t *)e, "style", value );
				stylenum++;
				continue;
			case -2:	// backwards switchable texlight
				Q_snprintf( value, sizeof( value ), "%i", -(32 + stylenum));
				SetKeyValue( (entity_t *)e, "style", value );
				stylenum++;
				continue;
			case -3:	// (HACK) a piggyback texlight: switched on and off by triggering a real light that has the same name
				SetKeyValue( (entity_t *)e, "style", "0" ); // just in case the level designer didn't give it a name
				newtexlight = true;
				// don't 'continue', fall out
			}
		}

		t = ValueForKey( (entity_t *)e, "targetname" );

		if( CheckKey( (entity_t *)e, "zhlt_usestyle" ))
		{
			t = ValueForKey( (entity_t *)e, "zhlt_usestyle" );

			if( !Q_stricmp( t, "NULL" ))
				t = "";
		}

		// if no custom style specified
		if( !t[0] )
		{
#ifdef HLCSG_SKYFIXEDSTYLE
			if( BoolForKey( (entity_t *)e, "_sky" ) || !Q_strcmp( classname, "light_environment" ))
			{
				Q_snprintf( value, sizeof( value ), "%i", LS_SKY );
				SetKeyValue( (entity_t *)e, "style", value );
			}
#endif
			continue;
		}

		// find this targetname
		for( j = 0; j < stylenum; j++ )
		{
			if( !Q_strcmp( lighttargets[j], t ))
				break;
		}

		if( j == stylenum )
		{
			if( stylenum == MAX_SWITCHED_LIGHTS )
				COM_FatalError( "MAX_SWITCHED_LIGHTS limit exceeded\n" );
			Q_strncpy( lighttargets[j], t, sizeof( lighttargets[0] ));
			stylenum++;
		}

		Q_snprintf( value, sizeof( value ), "%i", 32 + j );
		SetKeyValue( (entity_t *)e, "style", value );
	}
}

/*
============
LoadWadValue

don't change "wad" string in 'onlyents' mode
============
*/
void LoadWadValue( void )
{
	mapent_t	mapent[2];
	char	*wadvalue;
	epair_t	*e;

	ParseFromMemory( g_dentdata, g_entdatasize );
	memset( mapent, 0, sizeof( entity_t ));

	if( !GetToken( true ))
	{
		wadvalue = copystring( "" );
	}
	else
	{
		if( Q_strcmp( token, "{" ))
		{
			COM_FatalError( "ParseEntity: { not found\n" );
		}

		while( 1 )
		{
			if( !GetToken( true ))
				COM_FatalError( "ParseEntity: EOF without closing brace\n" );

			if( !Q_strcmp( token, "}" ))
				break;

			e = ParseEpair ();
			e->next = mapent->epairs;
			mapent->epairs = e;
		}

		wadvalue = copystring( ValueForKey( (entity_t *)mapent, "wad" ));
		FreeMapEntity( mapent ); // throw memory
	}

	if( *wadvalue ) MsgDev( D_REPORT, "Wad files required to run the map: \"%s\"\n", wadvalue );
	else MsgDev( D_REPORT, "Wad files required to run the map: (None)\n" );
	SetKeyValue( (entity_t *)&g_mapentities[0], "wad", wadvalue );
	freestring( wadvalue );
}

/*
============
BoundWorld

tell designer about world bounds
============
*/
void BoundWorld( void )
{
	const char	*axis[3] = { "X", "Y", "Z" };
	int	i, j, negative_world_bounds = 0;
	ClearBounds( world_mins, world_maxs );

	for( i = 0; i < g_nummapbrushes; i++ )
	{
		brushhull_t *h = &g_mapbrushes[i].hull[0];

		if( !h->faces ) continue;

		for( j = 0; j < 3; j++ )
		{
			if( h->mins[j] > WORLD_MAXS || h->maxs[j] < WORLD_MINS )
				break;
		}

		if( j != 3 ) continue; // no valid points

		AddPointToBounds( h->mins, world_mins, world_maxs );
		AddPointToBounds( h->maxs, world_mins, world_maxs );
	}

	VectorSubtract( world_maxs, world_mins, world_size );

	for( i = 0; i < 3; i++ )
	{
		if( world_size[i] > WORLD_SIZE )
			MsgDev( D_ERROR, "world is not fit in allowed bounds by %s-axis (%g > %g)\n", axis[i], world_size[i], WORLD_SIZE );
		if( world_size[i] < 0 ) negative_world_bounds++;
	}

	MsgDev( D_INFO, "\n^3World Size:^7 (%.f %.f %.f)\n\n", world_size[0], world_size[1], world_size[2] );
	if( negative_world_bounds ) COM_FatalError( "Negative world bounds\n" );
}

//======================================================================
/*
============
WriteBSP
============
*/
void WriteBSP( const char *source )
{
	char	path[1024];

	Q_snprintf( path, sizeof( path ), "%s.bsp", source );

	SetModelNumbers ();
	SetLightStyles ();

	if( g_onlyents )
	{
		// restore origins that was created by auto-origin system
		RestoreModelOrigins ();
		LoadWadValue ();
	}
	else WriteMiptex ();

	UnparseMapEntities ();
	WriteBSPFile( path );
}

//=========================================
/*
============
PrintCsgSettings

show compiler settings like ZHLT
============
*/
static void PrintCsgSettings( void )
{
	Msg( "Current %s settings\n", APP_ABBREVIATION );
	Msg( "Name                 |  Setting  |  Default\n" );
	Msg( "---------------------|-----------|-------------------------\n" );
	Msg( "developer             [ %7d ] [ %7d ]\n", GetDeveloperLevel(), DEFAULT_DEVELOPER );
	Msg( "wadtextures           [ %7s ] [ %7s ]\n", g_wadtextures ? "on" : "off", DEFAULT_WADTEXTURES ? "on" : "off" );
	Msg( "nullify trigger       [ %7s ] [ %7s ]\n", g_nullifytrigger ? "on" : "off", DEFAULT_NULLIFYTRIGGER ? "on" : "off" );
	Msg( "noclip                [ %7s ] [ %7s ]\n", g_noclip ? "on" : "off", DEFAULT_NOCLIP ? "on" : "off" );
	Msg( "onlyents              [ %7s ] [ %7s ]\n", g_onlyents ? "on" : "off", DEFAULT_ONLYENTS ? "on" : "off" );
	Msg( "CSG chop epsilon      [ %.6f] [ %.6f]\n", g_csgepsilon, CSGCHOP_EPSILON );
	Msg( "GoldSrc compatible    [ %7s ] [ %7s ]\n", g_compatibility_goldsrc ? "on" : "off", DEFAULT_COMPAT_GOLDSRC ? "on" : "off" );
	Msg( "\n" );
}

/*
============
PrintCsgUsage

show compiler usage like ZHLT
============
*/
static void PrintCsgUsage( void )
{
	Msg( "\n-= %s Options =-\n\n", APP_ABBREVIATION );
	Msg( "    -dev #           : compile with developer message (1 - 4). default is %d\n", DEFAULT_DEVELOPER );
	Msg( "    -threads #       : manually specify the number of threads to run\n" );
	Msg( "    -noclip          : don't create clipping hulls\n" );
	Msg( "    -onlyents        : do an entity update from .map to .bsp\n" );
 	Msg( "    -nowadtextures   : include all used textures into bsp\n" );
	Msg( "    -wadinclude file : place textures used from wad specified into bsp\n" );
	Msg( "    -nonullifytrigger: remove 'aaatrigger' visible faces\n" );
	Msg( "    -compat <type>   : enable compatibility mode (goldsrc/xashxt)\n" );
	Msg( "    -epsilon         : CSG chop precision epsilon\n" );
	Msg( "    mapfile          : the mapfile to compile\n\n" );
}

/*
============
CheckDeprecatedParameter

checks should be parameter ignored or not
============
*/
static bool CheckDeprecatedParameter(const char *name)
{
	if (!Q_strcmp(name, "-wadautodetect"))
		return true;
	else
		return false;
}

/*
============
main
============
*/
int main( int argc, char **argv )
{
	char	source[1024];
	char	mapname[1024];
	double	start, end;
	char	str[64];
	int		i;

	atexit( Sys_CloseLog );
	Sys_SetupCrashHandler();
	source[0] = '\0';

	for( i = 1; i < argc; i++ )
	{
		if (CheckDeprecatedParameter(argv[i]))
		{
			// compatibility issues, does nothing
		}
		else if (!Q_strcmp(argv[i], "-compat"))
		{
			i++;
			if (!Q_strcmp(argv[i], "goldsrc")) {
				g_compatibility_goldsrc = true;
			}
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
		else if( !Q_strcmp( argv[i], "-noclip" ))
		{
			g_noclip = true;
		}
		else if( !Q_strcmp( argv[i], "-onlyents" ))
		{
			g_onlyents = true;
		}
		else if( !Q_strcmp( argv[i], "-nowadtextures" ))
		{
			g_wadtextures = false;
		}
		else if( !Q_strcmp( argv[i], "-wadinclude" ))
		{
			COM_FileBase( argv[i + 1], g_pszWadInclude[g_nWadInclude] );
			g_nWadInclude++;
			i++;
		}
		else if( !Q_strcmp(argv[i], "-nonullifytrigger" ))
		{
			g_nullifytrigger = false;
		}
		else if( !Q_strcmp( argv[i], "-epsilon" ))
		{
			g_csgepsilon = atof( argv[i+1] );
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

		PrintCsgUsage();
		exit(1);
	}

	start = I_FloatTime ();

	Sys_InitLog( va( "%s.log", source ));

	Q_snprintf( mapname, sizeof( mapname ), "%s.map", source );

	// trying to extract wadpath from the mapname
	if( Q_stristr( mapname, "maps" ))
	{
		char	temp[1024];

		COM_ExtractFilePath( mapname, temp );
		COM_ExtractFilePath( temp, g_wadpath );
	}

	Msg( "\n%s %s (%s, commit %s, arch %s, platform %s)\n\n", TOOLNAME, VERSIONSTRING, 
		BuildInfo::GetDate(), 
		BuildInfo::GetCommitHash(), 
		BuildInfo::GetArchitecture(), 
		BuildInfo::GetPlatform()
	);

	PrintCsgSettings();

	ThreadSetDefault ();

	// starting base filesystem
	FS_Init( source );
	LoadShaderInfo();

	if( g_onlyents )
	{
		char	bspname[1024];

		// if onlyents, just grab the entites and resave
		Q_snprintf( bspname, sizeof( bspname ), "%s.bsp", source );
		LoadBSPFile( bspname );

		// get the new entity data from the map file
		LoadMapFile( mapname );
	}
	else
	{
		// start from scratch
		LoadMapFile( mapname );

		// create brushes from map planes
		RunThreadsOnIndividual( g_nummapbrushes, true, CreateBrush );
		MsgDev( D_REPORT, "%5i map planes\n", g_nummapplanes );

		ProcessAutoOrigins();

		DumpBrushPlanes();

		BoundWorld ();

		ProcessModels ( source );

		WriteHullSizes( source );

		WriteMapPlanes( source );

		MsgDev( D_INFO, "%5i used faces\n", c_outfaces );
	}

	// write it all back out again.
	WriteBSP( source );

	// release dynamically allocated data
	TEX_FreeTextures();
	FreeHullFaces();
	FreeMapEntities();
	FreeShaderInfo();
	FS_Shutdown();
	Sys_RestoreCrashHandler();

	// now check for leaks
	SetDeveloperLevel( D_REPORT );
	Mem_Check();

	end = I_FloatTime ();
	Q_timestring((int)( end - start ), str );
	Msg( "%s elapsed\n", str );

	return 0;
}