/*
makewad.cpp - convert textures into 8-bit indexed and pack into wad3 file
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
#include "makewad.h"

wfile_t *source_wad = NULL;	// input WAD3 file
wfile_t *output_wad = NULL;	// output WAD3 file
char output_path[256];
char output_ext[8];
float resize_percent = 0.0f;
int processed_files = 0;
int processed_errors = 0;
int graphics_wadfile = 0;

// just for debug
void Test_ConvertImageTo8Bit( const char *filename )
{
	char outpath[256];
	rgbdata_t	*pic;

	MsgDev( D_INFO, "Quantize %s\n", filename );

	pic = COM_LoadImage( filename );
	if( !pic )
	{
		MsgDev( D_ERROR, "couldn't load (%s)\n", filename );
		return;
	}

	pic = Image_Quantize( pic );
	Q_strncpy( outpath, filename, sizeof( outpath ));
	COM_StripExtension( outpath );
	Q_strncat( outpath, "_8bit.bmp", sizeof( outpath ));

	if( COM_SaveImage( outpath, pic ))
		MsgDev( D_INFO, "Write %s\n", outpath );
	else MsgDev( D_INFO, "Failed to save %s\n", outpath );

	Mem_Free( pic );
}

/*
=============
WAD_CreateTexture

put the texture in the wad or
extract her onto disk
=============
*/
bool WAD_CreateTexture( const char *filename )
{
	const char *ext = COM_FileExtension( filename );
	int width, height, len = WAD3_NAMELEN;
	dlumpinfo_t *find, *find2;
	char lumpname[64], hint;
	int mipwidth, mipheight;
	rgbdata_t	*image;

	// store name for detect suffixes
	COM_FileBase( filename, lumpname );
	hint = Image_HintFromSuf( lumpname );

	if( hint != IMG_DIFFUSE )
	{
		MsgDev( D_ERROR, "WAD_CreateTexture: non-diffuse texture %s rejected\n", lumpname ); 
		return false; // only diffuse textures can be passed
	}

	if( Q_strlen( lumpname ) > len )
	{
		// NOTE: technically we can cutoff too long names but it just not needs
		MsgDev( D_ERROR, "WAD_CreateTexture: %s more than %i symbols\n", lumpname, len ); 
		return false;
	}

	image = COM_LoadImage( filename );
	if( !image ) return false;

	width = mipwidth = image->width;
	height = mipheight = image->height;

	// wad-copy mode: wad->wad
	if( source_wad && output_wad )
	{
		if( !Q_stricmp( ext, "mip" ))
		{
			find = W_FindMiptex( source_wad, lumpname );

			if( !find )
			{
				Mem_Free( image );
				return false;
			}

			find2 = W_FindMiptex( output_wad, lumpname );

			if( !MIP_CheckForReplace( find2, image, mipwidth, mipheight ))
				return false; // NOTE: image already freed on failed

			// fit to the replacement
			image = Image_Resample( image, mipwidth, mipheight );
			image = Image_Quantize( image ); // just in case.

			bool result = MIP_WriteMiptex( lumpname, image );

			if( result ) MsgDev( D_INFO, "%s.mip\n", lumpname );
			else MsgDev( D_ERROR, "coudln't copy: %s.mip\n", lumpname );

			Mem_Free( image );

			return result;
		}
		else if( !Q_stricmp( ext, "lmp" ))
		{
			find = W_FindLmptex( source_wad, lumpname );

			if( !find )
			{
				Mem_Free( image );
				return false;
			}

			find2 = W_FindLmptex( output_wad, lumpname );

			if( !LMP_CheckForReplace( find2, image, mipwidth, mipheight ))
				return false; // NOTE: image already freed on failed

			// fit to the replacement
			image = Image_Resample( image, mipwidth, mipheight );
			image = Image_Quantize( image ); // just in case.

			bool result = LMP_WriteLmptex( lumpname, image );

			if( result ) MsgDev( D_INFO, "%s.lmp\n", lumpname );
			else MsgDev( D_ERROR, "coudln't copy: %s.lmp\n", lumpname );

			Mem_Free( image );

			return result;
		}

		// unknown format?
		Mem_Free( image );
		return false;
	}

	// extraction or conversion mode: wad->bmp, wad->tga
	if( !output_wad && output_path[0] && output_ext[0] )
	{
		char	real_ext[8];

		// set default extension
		Q_strcpy( real_ext, output_ext );

		// wad-extract mode
		const char *path = va( "%s\\%s.%s", output_path, lumpname, real_ext );
		bool result = COM_SaveImage( path, image );

		if( result ) MsgDev( D_INFO, "%s\n", path );
		else MsgDev( D_ERROR, "coudln't save: %s\n", path );

		Mem_Free( image );

		return result;
	}

	// normal mode: bmp->wad, tga->wad
	if( graphics_wadfile )
	{
		if( !FBitSet( image->flags, IMAGE_QUANTIZED ))
		{
			// check for minmax sizes
			mipwidth = bound( IMAGE_MINWIDTH, mipwidth, IMAGE_MAXWIDTH );
			mipheight = bound( IMAGE_MINHEIGHT, mipheight, IMAGE_MAXHEIGHT );

			find2 = W_FindLmptex( output_wad, lumpname );

			if( !LMP_CheckForReplace( find2, image, mipwidth, mipheight ))
				return false; // NOTE: image already freed on failed

			rgbdata_t *rgba_image = Image_Copy( image );

			// align by 16 or fit to the replacement
			image = Image_Resample( image, mipwidth, mipheight );

			// now quantize image
			image = Image_Quantize( image );

			bool result = LMP_WriteLmptex( lumpname, image );

			if( result ) MsgDev( D_INFO, "%s.lmp\n", lumpname );
			else MsgDev( D_ERROR, "coudln't write: %s.lmp\n", lumpname );

			Mem_Free( rgba_image );
			Mem_Free( image );	// no reason to keep it

			return result;
		}
		else // image already indexed
		{
			// check for minmax sizes
			mipwidth = bound( IMAGE_MINWIDTH, mipwidth, IMAGE_MAXWIDTH );
			mipheight = bound( IMAGE_MINHEIGHT, mipheight, IMAGE_MAXHEIGHT );

			find = W_FindLmptex( output_wad, lumpname );

			if( !LMP_CheckForReplace( find, image, mipwidth, mipheight ))
				return false; // NOTE: image already freed on failed

			// align by 16 or fit to the replacement
			image = Image_Resample( image, mipwidth, mipheight );

			bool result = LMP_WriteLmptex( lumpname, image );

			if( result ) MsgDev( D_INFO, "%s.lmp\n", lumpname );
			else MsgDev( D_ERROR, "coudln't write: %s.lmp\n", lumpname );
			if( image ) Mem_Free( image );

			return result;
		}
	}
	else
	{
		// all the mips must be aligned by 16
		width = (width + 7) & ~7;
		height = (height + 7) & ~7;

		if( !FBitSet( image->flags, IMAGE_QUANTIZED ))
		{
			// check for minmax sizes
			mipwidth = bound( IMAGE_MINWIDTH, mipwidth, IMAGE_MAXWIDTH );
			mipheight = bound( IMAGE_MINHEIGHT, mipheight, IMAGE_MAXHEIGHT );

			if( resize_percent != 0.0f )
			{
				mipwidth *= (resize_percent / 100.0f);
				mipheight *= (resize_percent / 100.0f);
			}

			// all the mips must be aligned by 16
			mipwidth = (mipwidth + 7) & ~7;
			mipheight = (mipheight + 7) & ~7;

			find2 = W_FindMiptex( output_wad, lumpname );

			if( !MIP_CheckForReplace( find2, image, mipwidth, mipheight ))
				return false; // NOTE: image already freed on failed

			rgbdata_t *rgba_image = Image_Copy( image );

			// align by 16 or fit to the replacement
			image = Image_Resample( image, mipwidth, mipheight );

			// now quantize image
			image = Image_Quantize( image );

			if( lumpname[0] == '{' )
				Image_MakeOneBitAlpha( image ); // make one-bit alpha from blue color

			bool result = MIP_WriteMiptex( lumpname, image );

			if( result ) MsgDev( D_INFO, "%s.mip\n", lumpname );
			else MsgDev( D_ERROR, "coudln't write: %s.mip\n", lumpname );

			Mem_Free( rgba_image );
			Mem_Free( image );	// no reason to keep it

			return result;
		}
		else // image already indexed
		{
			// check for minmax sizes
			mipwidth = bound( IMAGE_MINWIDTH, mipwidth, IMAGE_MAXWIDTH );
			mipheight = bound( IMAGE_MINHEIGHT, mipheight, IMAGE_MAXHEIGHT );

			if( resize_percent != 0.0f )
			{
				mipwidth *= (resize_percent / 100.0f);
				mipheight *= (resize_percent / 100.0f);
			}

			// all the mips must be aligned by 16
			mipwidth = (mipwidth + 7) & ~7;
			mipheight = (mipheight + 7) & ~7;

			find = W_FindMiptex( output_wad, lumpname );

			if( !MIP_CheckForReplace( find, image, mipwidth, mipheight ))
				return false; // NOTE: image already freed on failed

			// align by 16 or fit to the replacement
			image = Image_Resample( image, mipwidth, mipheight );

			if( lumpname[0] == '{' )
				Image_MakeOneBitAlpha( image ); // make one-bit alpha from blue color
			bool result = MIP_WriteMiptex( lumpname, image );

			if( result ) MsgDev( D_INFO, "%s.mip\n", lumpname );
			else MsgDev( D_ERROR, "coudln't write: %s.mip\n", lumpname );
			if( image ) Mem_Free( image );

			return result;
		}
	}

	return false;
}

void CDECL Shutdown_Makewad( void )
{
	W_Close( source_wad );
	W_Close( output_wad );
}

int main( int argc, char **argv )
{
	char	srcpath[256], dstpath[256];
	char	srcwad[256], dstwad[256];
	bool	srcset = false;
	bool	dstset = false;
	double	start, end;
	char	str[64];
	int	i;

	start = I_FloatTime();

	Msg( "		Image Quantizer & Wad3 creator\n" );
	Msg( "		   Xash XT Group 2018(^1c^7)\n\n\n" );

	atexit( Shutdown_Makewad );

	// initialize command vars
	SetReplaceLevel( REP_IGNORE );
	output_path[0] = '\0';
	output_ext[0] = '\0';

	for( i = 1; i < argc; i++ )
	{
		if( !Q_stricmp( argv[i], "-input" ))
		{
			Q_strncpy( srcpath, argv[i+1], sizeof( srcpath ));
			srcset = true;
			i++;
		}
		else if( !Q_stricmp( argv[i], "-output" ))
		{
			Q_strncpy( dstpath, argv[i+1], sizeof( dstpath ));
			dstset = true;
			i++;
		}
		else if( !Q_stricmp( argv[i], "-replace" ))
		{
			SetReplaceLevel( REP_NORMAL ); // replace only if image dimensions is equal
		}
		else if( !Q_stricmp( argv[i], "-forcereplace" ))
		{
			SetReplaceLevel( REP_FORCE ); // rescale image, replace in any cases
		}
		else if( !Q_stricmp( argv[i], "-resize" ))
		{
			resize_percent = atof( argv[i+1] ); // resize source image (percent)
			resize_percent = bound( 10.0f, resize_percent, 200.0f );
			i++;
		}
		else if( !Q_stricmp( argv[i], "-dev" ))
		{
			SetDeveloperLevel( atoi( argv[i+1] ));
			i++;
		}
		else
		{
			Msg( "makewad: unknown option %s\n", argv[i] );
			break;
		}
	}

	if( i != argc || !srcset || !dstset )
	{
		Msg( "Usage: -input <wad|tga|bmp|lmp> -output <wadname.wad|tga|bmp|lmp> <options>\n"
		"\nlist options:\n"
		"^2-replace^7 - replace existing images if they matched by size\n"
		"^2-forcereplace^7 - replace existing images even if they doesn't matched by size\n"
		"^2-resize^7 - resize source image in percents (range 10%%-200%%)\n\n"
		"\t\tPress any key to exit" );

		system( "pause>nul" );
		return 1;
	}
	else
	{
		char	testname[64];
		char	*find = NULL;

		Q_strncpy( srcwad, srcpath, sizeof( srcwad ));
		find = Q_stristr( srcwad, ".wad" );

		if( find )
		{
			find += 4, *find = '\0';
			source_wad = W_Open( srcwad, "rb" );
		}

		search_t *search = COM_Search( srcpath, true, source_wad );
		if( !search ) return 1; // nothing found

		Q_strncpy( dstwad, dstpath, sizeof( dstwad ));
		find = Q_stristr( dstwad, ".wad" );

		if( find )
		{
			find += 4, *find = '\0';
			output_wad = W_Open( dstwad, "a+b" );
			COM_FileBase( dstwad, testname );

			if( !Q_stricmp( testname, "gfx" ) || !Q_stricmp( testname, "cached" ))
			{
				Msg( "graphics wad detected\n" );
				graphics_wadfile = 1;
			}
		}
		else
		{
			const char *ext = COM_FileExtension( srcwad );

			if( !Q_stricmp( dstpath, "bmp" ) || !Q_stricmp( dstpath, "tga" ) || !Q_stricmp( dstpath, "lmp" ))
			{
				COM_ExtractFilePath( srcwad, output_path );
				COM_StripExtension( output_path );
				Q_strncpy( output_ext, dstpath, sizeof( output_ext ));
				if( !output_path[0] ) _getcwd( output_path, sizeof( output_path ));
			}
			else
			{
				Msg( "Usage: -input <wad|tga|bmp|lmp> -output <wadname.wad|tga|bmp|lmp> <options>\n"
				"\nlist options:\n"
				"^2-replace^7 - replace existing images if they matched by size\n"
				"^2-forcereplace^7 - replace existing images even if they doesn't matched by size\n"
				"^2-resize^7 - resize source image in percents (range 10%%-200%%)\n\n"
				"\t\tPress any key to exit" );

				Mem_Free( search );
				system( "pause>nul" );
				return 1;
			}
		}

		if( source_wad && !output_wad && output_path[0] && output_ext[0] )
		{
			MsgDev( D_INFO, "extract textures from %s\n", srcwad );
		}
		else if( source_wad && output_wad )
		{
			MsgDev( D_INFO, "copying textures from %s to %s\n", srcwad, dstwad );
		}
		else
		{
			MsgDev( D_INFO, "write textures into %s\n", dstwad );
		}

		for( i = 0; i < search->numfilenames; i++ )
		{
			if( WAD_CreateTexture( search->filenames[i] ))
				processed_files++;
			else processed_errors++;
		}

		end = I_FloatTime();

		MsgDev( D_INFO, "%3i files processed, %3i errors\n", processed_files, processed_errors );

		Q_timestring((int)(end - start), str );
		MsgDev( D_INFO, "%s elapsed\n", str );

		W_Close( source_wad );
		source_wad = NULL;

		W_Close( output_wad );
		output_wad = NULL;

		Mem_Free( search );
		Mem_Check(); // report leaks
	}

	return 0;
}