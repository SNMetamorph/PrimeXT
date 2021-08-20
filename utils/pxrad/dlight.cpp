/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// doom per-sector lighting

#include "dlight.h"

char		source[MAX_PATH] = "";
vec3_t		g_reflectivity[MAX_MAP_TEXTURES];
bool		g_texture_init[MAX_MAP_TEXTURES];
vec_t		g_direct_scale = DEFAULT_DLIGHT_SCALE;
float		g_maxlight = DEFAULT_LIGHTCLIP;	// originally this was 196
vec_t		g_gamma = DEFAULT_GAMMA;
vec3_t		g_ambient = { 0, 0, 0 };
dplane_t		g_backplanes[MAX_MAP_PLANES];		// equal to MAX_MAP_FACES, there is no errors
faceinfo_t	g_faceinfo[MAX_MAP_FACES];
facelight_t	g_facelight[MAX_MAP_FACES];

static char	global_lights[MAX_PATH] = "";
static char	level_lights[MAX_PATH] = "";

/*
===================================================================

MISC

===================================================================
*/
const dplane_t *GetPlaneFromFace( const dface_t *face )
{
	ASSERT( face != NULL );

	if( face->side )
		return &g_backplanes[face->planenum];
	return &g_dplanes[face->planenum];
}

const dplane_t *GetPlaneFromFace( const uint facenum )
{
	ASSERT( facenum < MAX_MAP_FACES );

	dface_t	*face = &g_dfaces[facenum];

	if( face->side )
		return &g_backplanes[face->planenum];
	return &g_dplanes[face->planenum];
}

dleaf_t *PointInLeaf( const vec3_t point )
{
	int	nodenum = 0;

	while( nodenum >= 0 )
	{
		dnode_t	*node = &g_dnodes[nodenum];

		if( PlaneDiff( point, &g_dplanes[node->planenum] ) > 0 )
			nodenum = node->children[0];
		else nodenum = node->children[1];
	}

	return &g_dleafs[-nodenum - 1];
}

/*
=============
MakeBackplanes
=============
*/
void MakeBackplanes( void )
{
	for( int i = 0; i < g_numplanes; i++ )
	{
		VectorNegate( g_dplanes[i].normal, g_backplanes[i].normal );
		g_backplanes[i].dist = -g_dplanes[i].dist;
	}
}

/*
===================================================================

  TEXTURE LIGHT VALUES

===================================================================
*/
static texlight_t	g_texlights[MAX_TEXLIGHTS];
static int	g_num_texlights;

/*
============
ReadLightFile
============
*/
void ReadLightFile( const char *filename, bool use_direct_path )
{
	int	file_texlights = 0;
	char	scan[128];
	int	j, argCnt;
	file_t	*f;

	FS_AllowDirectPaths( use_direct_path );	
	f = FS_Open( filename, "r", false );
	FS_AllowDirectPaths( false );	
	if( !f ) return;
	else MsgDev( D_INFO, "[Reading texlights from '%s']\n", filename );

	while( FS_Gets( f, (byte *)&scan, sizeof( scan )) != EOF )
	{
		char	szTexlight[256];
		vec_t	r, g, b, i = 1;
		char	*comment = scan;

		// skip the comments
		if( comment[0] == '/' && comment[1] == '/' )
			continue;

		argCnt = sscanf( scan, "%s %f %f %f %f", szTexlight, &r, &g, &b, &i );

		if( argCnt == 2 )
		{
			// eith 1+1 args, the R,G,B values are all equal to the first value
			g = b = r;
		}
		else if( argCnt == 5 )
		{
			// With 1 + 4 args, the R,G,B values are "scaled" by the fourth numeric value i;
			r *= i / 255.0;
			g *= i / 255.0;
			b *= i / 255.0;
		}
		else if( argCnt != 4 )
		{
			if( Q_strlen( scan ) > 4 )
				MsgDev( D_WARN, "ignoring bad texlight '%s' in %s", scan, filename );
			continue;
		}

		for( j = 0; j < g_num_texlights; j++ )
		{
			texlight_t	*tl = &g_texlights[j];

			if( !Q_strcmp( tl->name, szTexlight ))
			{
				if( !Q_strcmp( tl->filename, filename ))
				{
					MsgDev( D_REPORT, "duplication of '%s' in file '%s'!\n", tl->name, tl->filename );
				} 
				else if( tl->value[0] != r || tl->value[1] != g || tl->value[2] != b )
				{
					MsgDev( D_REPORT, "overriding '%s' from '%s' with '%s'!\n", tl->name, tl->filename, filename );
				}
				else
				{
					MsgDev( D_WARN, "redundant '%s' def in '%s' AND '%s'!\n", tl->name, tl->filename, filename );
				}
				break;
			}
		}

		Q_strncpy( g_texlights[j].name, szTexlight, sizeof( g_texlights[0].name ));
		VectorSet( g_texlights[j].value, r, g, b );
		g_texlights[j].filename = filename;
		file_texlights++;

		g_num_texlights = Q_max( g_num_texlights, j + 1 );

		if( g_num_texlights == MAX_TEXLIGHTS )
			COM_FatalError( "MAX_TEXLIGHTS limit exceeded\n" );
	}		

	MsgDev( D_REPORT, "[%i texlights parsed from '%s']\n\n", file_texlights, filename );
	FS_Close( f );
}

/*
============
LightForTexture
============
*/
void LightForTexture( const char *name, vec3_t result )
{
	VectorClear( result );

	for( int i = 0; i < g_num_texlights; i++ )
	{
		if( !Q_stricmp( name, g_texlights[i].name ))
		{
			VectorCopy( g_texlights[i].value, result );
			MsgDev( D_REPORT, "Texture '%s': baselight is (%f,%f,%f).\n", name, result[0], result[1], result[2] );
			return;
		}
	}
}

/*
=============
BaseLightForFace
=============
*/
void BaseLightForFace( dface_t *f, vec3_t light, vec3_t reflectivity )
{
	int	miptex = g_texinfo[f->texinfo].miptex;
	miptex_t	*mt;

	// check for light emited by texture
	mt = GetTextureByMiptex( miptex );
	if( !mt ) return;

	LightForTexture( mt->name, light );
	VectorClear( reflectivity );

	int	samples = mt->width * mt->height;
	byte	*pal = ((byte *)mt) + mt->offsets[0] + (((mt->width * mt->height) * 85) >> 6);
	byte	*buf = ((byte *)mt) + mt->offsets[0];
	vec3_t	total;

	// check for cache
	if( g_texture_init[miptex] )
	{
		VectorCopy( g_reflectivity[miptex], reflectivity );
		return;
	}

	pal += sizeof( short ); // skip colorsize 
	VectorClear( total );

	for( int i = 0; i < samples; i++ )
	{
		vec3_t	reflectivity;

		if( mt->name[0] == '{' && buf[i] == 0xFF )
		{
			VectorClear( reflectivity );
		}
		else
		{
			int	texel = buf[i];
			reflectivity[0] = pow( pal[texel*3+0] * (1.0f / 255.0f), DEFAULT_TEXREFLECTGAMMA );
			reflectivity[1] = pow( pal[texel*3+1] * (1.0f / 255.0f), DEFAULT_TEXREFLECTGAMMA );
			reflectivity[2] = pow( pal[texel*3+2] * (1.0f / 255.0f), DEFAULT_TEXREFLECTGAMMA );
			VectorScale( reflectivity, DEFAULT_TEXREFLECTSCALE, reflectivity );
		}
		VectorAdd( total, reflectivity, total );
	}

	VectorScale( total, 1.0 / (double)(mt->width * mt->height), g_reflectivity[miptex] );
	VectorCopy( g_reflectivity[miptex], reflectivity );
	MsgDev( D_REPORT, "Texture '%s': reflectivity is (%f,%f,%f).\n", mt->name, reflectivity[0], reflectivity[1], reflectivity[2] );
	g_texture_init[miptex] = true;
}

/*
============
BuildFaceInfos

calc lightmap sizes
============
*/
void BuildFaceInfos( void )
{
	int		m, j, e;
	int		facenum1;
	vec_t		val, lmmins[2], lmmaxs[2];
	float		lmvecs[2][4];
	dtexinfo_t	*tex;
	const dplane_t	*dp1;
	const dface_t	*f1;
	faceinfo_t	*fn;
	dvertex_t		*v;

	// store a list of every face that uses a particular vertex
	for( facenum1 = 0; facenum1 < g_numfaces; facenum1++ )
	{
		fn = &g_faceinfo[facenum1];

		f1 = &g_dfaces[facenum1];
		dp1 = GetPlaneFromFace( f1 );
		tex = &g_texinfo[f1->texinfo];
		VectorCopy( dp1->normal, fn->facenormal );
		lmmins[0] = lmmins[1] = 999999;
		lmmaxs[0] = lmmaxs[1] =-999999;

		int max_surface_extent = GetSurfaceExtent( tex );
		int texture_step = GetTextureStep( tex );

		LightMatrixFromTexMatrix( tex, lmvecs );

		for( j = 0; j < f1->numedges; j++ )
		{
			e = g_dsurfedges[f1->firstedge + j];

			if( e >= 0 ) v = &g_dvertexes[g_dedges[e].v[0]];
			else v = &g_dvertexes[g_dedges[-e].v[1]];

			for( m = 0; m < 2; m++ )
			{
				/* The following calculation is sensitive to floating-point
			 	* precision.  It needs to produce the same result that the
			 	* light compiler does, because R_BuildLightMap uses surf->
			 	* extents to know the width/height of a surface's lightmap,
			 	* and incorrect rounding here manifests itself as patches
			 	* of "corrupted" looking lightmaps.
			 	* Most light compilers are win32 executables, so they use
			 	* x87 floating point.  This means the multiplies and adds
			 	* are done at 80-bit precision, and the result is rounded
			 	* down to 32-bits and stored in val.
			 	* Adding the casts to double seems to be good enough to fix
			 	* lighting glitches when Quakespasm is compiled as x86_64
			 	* and using SSE2 floating-point.  A potential trouble spot
			 	* is the hallway at the beginning of mfxsp17.  -- ericw
			 	*/
				val =	((double)v->point[0] * (double)lmvecs[m][0]) +
					((double)v->point[1] * (double)lmvecs[m][1]) +
					((double)v->point[2] * (double)lmvecs[m][2]) +
					(double)lmvecs[m][3];
				lmmins[m] = Q_min( val, lmmins[m] );
				lmmaxs[m] = Q_max( val, lmmaxs[m] );
			}
		}

		// calc face extents for traceline and lightmap extents for LightForPoint
		for( j = 0; j < 2; j++ )
		{
			lmmins[j] = floor( lmmins[j] / texture_step );
			lmmaxs[j] = ceil( lmmaxs[j] / texture_step );

			fn->texmins[j] = lmmins[j];
			fn->texsize[j] = (lmmaxs[j] - lmmins[j]);
		}

		if( !FBitSet( tex->flags, TEX_SPECIAL ))
		{
			if( fn->texsize[0] * fn->texsize[1] > ( MAX_SINGLEMAP / 3 ))
				COM_FatalError( "surface to large to map\n" );

			if( fn->texsize[0] > max_surface_extent )
				MsgDev( D_ERROR, "bad surface extents %d > %d\n", fn->texsize[0], max_surface_extent );

			if( fn->texsize[1] > max_surface_extent )
				MsgDev( D_ERROR, "bad surface extents %d > %d\n", fn->texsize[1], max_surface_extent );

			if( fn->texsize[0] < 0 || fn->texsize[1] < 0 )
				COM_FatalError( "negative extents\n" );
		}
	}
}

/*
=============
BuildSectorLights
=============
*/
void BuildSectorLights( int facenum, int thread )
{
	facelight_t	*fl = &g_facelight[facenum];
	int		lmwidth, lmheight;
	word		sectorlight;
	vec_t		lightvalue;
	int		i, j;
	faceinfo_t	*fn;
	dface_t		*f;
	sample_t		*s;

	f = &g_dfaces[facenum];

	// some surfaces don't need lightmaps
	f->lightofs = -1;
	for( j = 0; j < MAXLIGHTMAPS; j++ )
		f->styles[j] = 255;

	if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		return; // non-lit texture

	// decode per-sector lighting values
	sectorlight = GetSurfaceGroupId( f );

	f->styles[0] = ((sectorlight & 0xFF00) >> 8);
	lightvalue = (vec_t)(sectorlight & 0x00FF);

	fn = &g_faceinfo[facenum];
	lmwidth = fn->texsize[0] + 1;
	lmheight = fn->texsize[1] + 1;
	fl->numsamples = lmwidth * lmheight;

	// alloc lightmap for this face
	fl->samples = (sample_t *)Mem_Alloc( fl->numsamples * sizeof( sample_t ));

	// doom lighting it's easy. just fill face lightmap with single color from sector
	// TODO: add lighting from self-illuminated textures and reflecitivity?
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			VectorFill( fl->samples[i].light[j], lightvalue );
		}
	}

	// add an ambient term if desired
	if( g_ambient[0] || g_ambient[1] || g_ambient[2] )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] == 255; j++ );
		if( j == MAXLIGHTMAPS ) f->styles[0] = 0; // adding style

		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			if( f->styles[j] == 0 )
			{
				s = fl->samples;
				for( i = 0; i < fl->numsamples; i++, s++ )
					VectorAdd( s->light[j], g_ambient, s->light[j] );
				break;
			}
		}

	}
}

/*
============
ScaleDirectLights
============
*/
void ScaleDirectLights( void )
{
	sample_t		*samp;
	facelight_t	*fl;
	dface_t		*f;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		f = &g_dfaces[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue;

		fl = &g_facelight[facenum];

		for( int k = 0; k < MAXLIGHTMAPS && f->styles[k] != 255; k++ )
		{
			for( int i = 0; i < fl->numsamples; i++ )
			{
				samp = &fl->samples[i];
				VectorScale( samp->light[k], g_direct_scale, samp->light[k] );
			}
		}
	}
}

void PrecompLightmapOffsets( void )
{
	int		lightstyles;
	facelight_t	*fl;
	dface_t		*f;

	g_shadowdatasize = 0;
	g_normaldatasize = 0;
	g_lightdatasize = 0;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		f = &g_dfaces[facenum];
		fl = &g_facelight[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue; // non-lit texture

		for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
		{
			if( f->styles[lightstyles] == 255 )
				break; // end if styles
		}

		if( !lightstyles ) continue;

		f->lightofs = g_lightdatasize;
		g_lightdatasize += fl->numsamples * 3 * lightstyles;
	}

	g_dlightdata = (byte *)Mem_Realloc( g_dlightdata, g_lightdatasize );
}

void FinalLightFace( int facenum, int threadnum )
{
	float		minlight = 0.0f;	// TODO: allow minlight?
	int		lightstyles;
	int		i, j, k;
	sample_t		*samp;
	dtexinfo_t	*tx;
	facelight_t	*fl;
	dface_t		*f;
	vec3_t		lb;

	f = &g_dfaces[facenum];
	fl = &g_facelight[facenum];
	tx = &g_texinfo[f->texinfo];

	if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		return; // non-lit texture

	for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
	{
		if( f->styles[lightstyles] == 255 )
			break;
	}

	if( !lightstyles ) return;

	for( k = 0; k < lightstyles; k++ )
	{
		samp = fl->samples;

		for( j = 0; j < fl->numsamples; j++, samp++ )
		{
			VectorCopy( samp->light[k], lb ); 

			// clip from the bottom first
			lb[0] = Q_max( lb[0], minlight );
			lb[1] = Q_max( lb[1], minlight );
			lb[2] = Q_max( lb[2], minlight );

			// clip from the top
			if( lb[0] > g_maxlight || lb[1] > g_maxlight || lb[2] > g_maxlight )
			{
				// find max value and scale the whole color down;
				float	max = VectorMax( lb );

				for( i = 0; i < 3; i++ )
					lb[i] = ( lb[i] * g_maxlight ) / max;
			}

			// do gamma adjust
			lb[0] = (float)pow( lb[0] / 256.0f, g_gamma ) * 256.0f;
			lb[1] = (float)pow( lb[1] / 256.0f, g_gamma ) * 256.0f;
			lb[2] = (float)pow( lb[2] / 256.0f, g_gamma ) * 256.0f;

			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = Q_rint( lb[0] );
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = Q_rint( lb[1] );
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = Q_rint( lb[2] );
		}
	}
}

void FreeFaceLights( void )
{
	for( int i = 0; i < g_numfaces; i++ )
	{
		Mem_Free( g_facelight[i].samples );
	}
}

//==============================================================
/*
=============
DoomLightWorld
=============
*/
void DoomLightWorld( void )
{
	MakeBackplanes();
	BuildFaceInfos();

	// build initial facelights
	RunThreadsOnIndividual( g_numfaces, true, BuildSectorLights );

	ScaleDirectLights();

	PrecompLightmapOffsets();

	RunThreadsOnIndividual( g_numfaces, true, FinalLightFace );

	FreeFaceLights();
}

/*
============
PrintDlightSettings

show compiler settings like ZHLT
============
*/
static void PrintDlightSettings( void )
{
	char	buf1[1024];
	char	buf2[1024];

	Msg( "\nCurrent dlight settings\n" );
	Msg( "Name                 |  Setting  |  Default\n" );
	Msg( "---------------------|-----------|-------------------------\n" );
	Msg( "developer             [ %7d ] [ %7d ]\n", GetDeveloperLevel(), DEFAULT_DEVELOPER );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_direct_scale );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_DLIGHT_SCALE );
	Msg( "direct light scale    [ %7s ] [ %7s ]\n", buf1, buf2 );
	Q_snprintf( buf1, sizeof( buf1 ), "%3.3f", g_gamma );
	Q_snprintf( buf2, sizeof( buf2 ), "%3.3f", DEFAULT_GAMMA );
	Msg( "gamma factor          [ %7s ] [ %7s ]\n", buf1, buf2 );
	Msg( "\n" );
}

/*
============
PrintDlightUsage

show compiler usage like ZHLT
============
*/
static void PrintDlightUsage( void )
{
	Msg( "\n-= dlight Options =-\n\n" );
	Msg( "    -dev #         : compile with developer message (1 - 4). default is %d\n", DEFAULT_DEVELOPER );
	Msg( "    -threads #     : manually specify the number of threads to run\n" );
	Msg( "    -ambient r g b : set ambient world light (0.0 to 1.0, r g b)\n" );
	Msg( "    -dscale        : direct light scaling factor\n" );
	Msg( "    -gamma         : set global gamma value\n" );
	Msg( "    bspfile        : The bspfile to compile\n\n" );

	exit( 1 );
}

/*
========
main

light modelfile
========
*/
int main( int argc, char **argv )
{
	double	start, end;
	char	str[64];
	int	i;

	atexit( Sys_CloseLog );
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
		else if( !Q_strcmp( argv[i], "-ambient" ))
		{
			if( argc > ( i + 3 ))
			{
 				g_ambient[0] = (float)atof( argv[i+1] ) * 0.5f;
 				g_ambient[1] = (float)atof( argv[i+2] ) * 0.5f;
 				g_ambient[2] = (float)atof( argv[i+3] ) * 0.5f;
				i += 3;
			}
			else
			{
				break;
			}
		}
		else if( !Q_strcmp( argv[i], "-dscale" ))
		{
			g_direct_scale = (float)atof( argv[i+1] );
			i++;
		}
		else if( !Q_strcmp( argv[i], "-gamma" ))
		{
			g_gamma = (float)atof( argv[i+1] );
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

	if( i != argc || !source[0] )
	{
		if( !source[0] )
			Msg( "no mapfile specified\n" );
		PrintDlightUsage();
	}

	start = I_FloatTime ();

	Sys_InitLogAppend( va( "%s.log", source ));

	Msg( "\n%s %s (%s)\n", TOOLNAME, VERSIONSTRING, __DATE__ );

	PrintDlightSettings();

	ThreadSetDefault ();

	// starting base filesystem
	FS_Init( source );

	// Set the required global lights filename
	// try looking in the directory we were run from
	GetModuleFileName( NULL, global_lights, sizeof( global_lights ));
	COM_ExtractFilePath( global_lights, global_lights );
	Q_strncat( global_lights, "\\lights.rad", sizeof( global_lights ));

	// Set the optional level specific lights filename
	COM_FileBase( source, str );
	Q_snprintf( level_lights, sizeof( level_lights ), "maps/%s.rad", str );
	if( !FS_FileExists( level_lights, false )) level_lights[0] = '\0';	

	ReadLightFile( global_lights, true );			// Required
	if( *level_lights )	ReadLightFile( level_lights, false );	// Optional & implied

	COM_DefaultExtension( source, ".bsp" );
	MsgDev( D_INFO, "\n" );

	LoadBSPFile( source );

	if( g_nummodels <= 0 )
		COM_FatalError( "map %s without any models\n", source );

	ParseEntities();
	TEX_LoadTextures();

	// keep it in acceptable range
	g_gamma = bound( 0.5, g_gamma, 2.0 );

	DoomLightWorld ();

	WriteBSPFile( source );
	TEX_FreeTextures ();
	FreeEntities ();
	FS_Shutdown();

	SetDeveloperLevel( D_REPORT );
	Mem_Check();

	end = I_FloatTime ();
	Q_timestring((int)( end - start ), str );
	Msg( "%s elapsed\n", str );
	
	return 0;
}