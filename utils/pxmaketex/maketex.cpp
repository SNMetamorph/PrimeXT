/*
maketex.cpp - convert textures into DDS images with custom encode
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "conprint.h"
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <stdio.h>
#include <io.h>
#include "cmdlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "imagelib.h"

int processed_files = 0;
int processed_errors = 0;

bool make_sdf = false;
bool no_mips = false;

int CheckSubeSides( const char *filename, char hint )
{
	const char	*ext = COM_FileExtension( filename );
	char		starthint = 0;
	char		testpath[256];
	char		cubepath[256];
	const imgtype_t	*imgtype;
	int		numSides;

	// disable skybox->cubemap conversion
	if( hint >= IMG_SKYBOX_FT && hint <= IMG_SKYBOX_LF )
		return 0;//starthint = IMG_SKYBOX_FT;
	else if( hint >= IMG_CUBEMAP_PX && hint <= IMG_CUBEMAP_NZ )
		starthint = IMG_CUBEMAP_PX;
	else return 0; // not a skybox or cubemap

	Q_strncpy( testpath, filename, sizeof( testpath ));
	COM_StripExtension( testpath );

	imgtype = Image_ImageTypeFromHint( starthint );

	// NOTE: this safety operation because we just the cutoff end of string
	if( imgtype ) Q_strncpy( testpath, testpath, Q_strlen( testpath ) - Q_strlen( imgtype->ext ) + 1 );

	for( numSides = 0; numSides < 6; numSides++, imgtype++ )
	{
		Q_snprintf( cubepath, sizeof( cubepath ), "%s%s.%s", testpath, imgtype->ext, ext );
		if( !COM_FileExists( cubepath )) break;
	}

	return numSides;
}

int ConvertCubeImageToEXT( const char *filename, char hint, const char *outext )
{
	const char	*ext = COM_FileExtension( filename );
	char		cubepath[256];
	char		outpath[256];
	const imgtype_t	*imgtype;
	rgbdata_t		*images[6];
	bool		skybox = false;
	rgbdata_t		*cube = NULL;
	int		i;

	memset( images, 0, sizeof( images ));

	if(( hint != IMG_SKYBOX_FT ) && ( hint != IMG_CUBEMAP_PX ))
	{
		// make sure what we have all the sides of cubemap
		int sides = CheckSubeSides( filename, hint );
		return (sides == 6) ? -1 : 2; // don't process these files
	}
	else if( CheckSubeSides( filename, hint ) != 6 )
		return 2;

	if( hint == IMG_SKYBOX_FT )
		skybox = true;

	Q_strncpy( outpath, filename, sizeof( outpath ));
	COM_StripExtension( outpath );

	imgtype = Image_ImageTypeFromHint( hint );

	// NOTE: this safety operation because we just the cutoff end of string
	if( imgtype ) Q_strncpy( outpath, outpath, Q_strlen( outpath ) - Q_strlen( imgtype->ext ) + 1 );

	if( COM_FileExists( va( "%s.%s", outpath, outext )))
		return -1; // already exist

	Msg( "%s found: %s\n", skybox ? "skybox" : "cubemap", outpath );

	for( i = 0; i < 6; i++, imgtype++ )
	{
		Q_snprintf( cubepath, sizeof( cubepath ), "%s%s.%s", outpath, imgtype->ext, ext );
		images[i] = COM_LoadImage( cubepath );
		if( !images[i] ) break;

		if( skybox ) MsgDev( D_INFO, "load skyside: %s[%i]\n", cubepath, i );
		else MsgDev( D_INFO, "load cubeside: %s[%i]\n", cubepath, i );
	}

	if(( i == 6 ) && ( cube = Image_CreateCubemap( images, skybox, no_mips )) != NULL )
	{
		int result;

		if( COM_SaveImage( va( "%s.%s", outpath, outext ), cube ))
		{
			MsgDev( D_INFO, "write %s.%s\n", outpath, outext );
			result = 1;
		}
		else
		{
			MsgDev( D_ERROR, "failed to save %s.%s\n", outpath, outext );
			result = 0;
		}

		Mem_Free( cube );				
		return result;
	}
	else
	{
		for( ; i >= 0; i-- )
			Mem_Free( images[i] );
		// not a cubemap just diffuse
		return 2;
	}
}

int ConvertImageToEXT( const char *filename, const char *outext )
{
	const char *ext = COM_FileExtension( filename );
	char outpath[256], lumpname[64];
	rgbdata_t	*pic, *alpha = NULL;
	char maskpath[256];

	// store name for detect suffixes
	COM_FileBase( filename, lumpname );

	char hint = Image_HintFromSuf( lumpname );

	if( hint == IMG_ALPHAMASK )
		return -1; // ignore to process
	else if( hint >= IMG_SKYBOX_FT && hint <= IMG_CUBEMAP_NZ )
	{
		int	result;

		result = ConvertCubeImageToEXT( filename, hint, outext );
		if( result != 2 ) return result;

		// continue as normal image
		hint = IMG_DIFFUSE;
	}

	Q_strncpy( outpath, filename, sizeof( outpath ));
	COM_StripExtension( outpath );

	if( COM_FileExists( va( "%s.%s", outpath, outext )))
		return -1; // already exist

	pic = COM_LoadImage( filename );
	if( !pic )
	{
		MsgDev( D_ERROR, "couldn't load (%s)\n", filename );
		return 0;
	}

	MsgDev( D_INFO, "processing %s\n", filename );

	// align by 4
	pic = Image_Resample( pic, ( pic->width + 15 ) & ~15, ( pic->height + 15 ) & ~15 );

	if( hint == IMG_DIFFUSE )
	{
		Q_snprintf( maskpath, sizeof( maskpath ), "%s_mask.%s", outpath, ext );

		if( COM_FileExists( maskpath ))
			alpha = COM_LoadImage( maskpath );

		if( alpha )
		{
			// get sizes from the colormap
			alpha = Image_Resample( alpha, pic->width, pic->height );

			MsgDev( D_INFO, "load mask (%s)\n", maskpath );
			pic = Image_MergeColorAlpha( pic, alpha );
			Mem_Free( alpha );

//			COM_SaveImage( va( "%s_merged.tga", outpath ), pic );

			if( pic->flags & IMAGE_HAS_8BIT_ALPHA )
				MsgDev( D_REPORT, "8-bit alpha detected\n" );
			if( pic->flags & IMAGE_HAS_1BIT_ALPHA )
				MsgDev( D_REPORT, "1-bit alpha detected\n" );

			if( make_sdf ) Image_MakeSignedDistanceField( pic );
		}
		else if( pic->flags & IMAGE_HAS_1BIT_ALPHA )
		{
			MsgDev( D_REPORT, "1-bit alpha detected\n" );

			if( make_sdf ) Image_MakeSignedDistanceField( pic );
		}
	}

	int result;

	if( COM_SaveImage( va( "%s.%s", outpath, outext ), pic ))
	{
		MsgDev( D_INFO, "write %s.dds\n", outpath );
		result = 1;
	}
	else
	{
		MsgDev( D_ERROR, "failed to save %s.%s\n", outpath, outext );
		result = 0;
	}
	Mem_Free( pic );

	return result;
}

int ProcessSearchResults( search_t *search, const char *outext )
{
	if( !search ) return 0;

	for( int i = 0; i < search->numfilenames; i++ )
	{
		int result = ConvertImageToEXT( search->filenames[i], outext );
		if( result > 0 ) processed_files++;
		else if( !result ) processed_errors++;
	}

	return 1;
}

void ProcessFiles( const char *srcpath, const char *outext )
{
	const char *ext = COM_FileExtension( srcpath );
	char folders[256], base[256], newpath[256];

	// now convert all the files in the root folder
	search_t *search = COM_Search( srcpath, true );
	ProcessSearchResults( search, outext );
	Mem_Free( search );

	COM_FileBase( srcpath, base );
	Q_strncpy( folders, srcpath, sizeof( folders ));
	COM_StripExtension( folders );
	search_t *foldsearch = COM_Search( folders, true );

	for( int i = 0; foldsearch != NULL && i < foldsearch->numfilenames; i++ )
	{
		if( COM_FolderExists( foldsearch->filenames[i] ))
		{
			Q_snprintf( newpath, sizeof( newpath ), "%s/%s.%s", foldsearch->filenames[i], base, ext ); 

			// now convert all the files in the root folder
			search_t *subsearch = COM_Search( newpath, true );
			ProcessSearchResults( subsearch, outext );
			Mem_Free( subsearch );
		}
	}
	Mem_Free( foldsearch );
}

int main( int argc, char **argv )
{
	char	srcpath[256];
	bool	srcset = false;
	double	start, end;
	char	str[64];
	int	i;

	atexit( Sys_CloseLog );
	Sys_InitLog( "maketex.log" );
	COM_InitCmdlib( argv, argc );

	Msg( "		P2:Savior DXT Convertor\n" );
	Msg( "		 XashXT Group 2017(^1c^7)\n\n\n" );

	for( i = 1; i < argc; i++ )
	{
		if( !Q_stricmp( argv[i], "-dev" ))
		{
			SetDeveloperLevel( atoi( argv[i+1] ));
			i++;
		}
		else if( !Q_stricmp( argv[i], "-sdf" ))
		{
			make_sdf = true;
		}
		else if( !Q_stricmp( argv[i], "-nomips" ))
		{
			no_mips = true;
		}
		else if( !srcset )
		{
			Q_strncpy( srcpath, argv[i], sizeof( srcpath ));
			srcset = true;
		}
		else
		{
			Msg( "maketex: unknown option %s\n", argv[i] );
			break;
		}
	}

	// starting default from the 'textures' folder
	if( !srcset ) Q_strncpy( srcpath, "*.tga", sizeof( srcpath ));

	if( i != argc )
	{
		Msg( "Usage: maketex.exe <path> <options>\n"
		"\nlist options:\n"
		"^2-dev^7 - shows developer messages\n"
		"^2-sdf^7 - create signed distance field from alpha-channel\n"
		"^2-nomips^7 - don't build mip-levels for cubemaps\n"
		"\t\tPress any key to exit" );

		system( "pause>nul" );
		return 1;
	}
	else
	{
		BuildGammaTable();	// init gamma conversion helper

		start = I_FloatTime();
		ProcessFiles( srcpath, "dds" );
		end = I_FloatTime();

		MsgDev( D_INFO, "%3i files processed, %3i errors\n", processed_files, processed_errors );

		Q_timestring((int)(end - start), str );
		MsgDev( D_INFO, "%s elapsed\n", str );

		SetDeveloperLevel( D_REPORT );
		Mem_Check(); // report leaks

		if( !srcset )
		{
			MsgDev( D_INFO, "press any key to exit\n" );
			system( "pause>nul" );
		}
	}

	return 0;
}