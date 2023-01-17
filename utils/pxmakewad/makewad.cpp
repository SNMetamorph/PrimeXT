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
#include "cmdlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "imagelib.h"
#include "makewad.h"
#include "status_code.h"
#include "crashhandler.h"
#include "build_info.h"
#include "app_info.h"
#include "port.h"
#include <string>
#include <iostream>

enum class ProgramWorkingMode
{
	Unknown,
	ExtractTextures,
	PackingTextures,
	CopyTexturesToWad
};

wfile_t *source_wad = NULL;	// input WAD3 file
wfile_t *output_wad = NULL;	// output WAD3 file
char output_path[256];
char output_ext[8];
float resize_percent = 0.0f;
float alpha_threshold = 0.5f;
bool ask_for_settings = true;
int processed_files = 0;
int graphics_wadfile = 0;
ProgramWorkingMode working_mode = ProgramWorkingMode::Unknown;

// using to detect quake1 textures and possible to convert luma
static const byte palette_q1[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,
171,171,187,187,187,203,203,203,219,219,219,235,235,235,15,11,7,23,15,11,31,23,11,39,27,15,47,35,19,55,43,23,63,
47,23,75,55,27,83,59,27,91,67,31,99,75,31,107,83,31,115,87,31,123,95,35,131,103,35,143,111,35,11,11,15,19,19,27,
27,27,39,39,39,51,47,47,63,55,55,75,63,63,87,71,71,103,79,79,115,91,91,127,99,99,139,107,107,151,115,115,163,123,
123,175,131,131,187,139,139,203,0,0,0,7,7,0,11,11,0,19,19,0,27,27,0,35,35,0,43,43,7,47,47,7,55,55,7,63,63,7,71,71,
7,75,75,11,83,83,11,91,91,11,99,99,11,107,107,15,7,0,0,15,0,0,23,0,0,31,0,0,39,0,0,47,0,0,55,0,0,63,0,0,71,0,0,79,
0,0,87,0,0,95,0,0,103,0,0,111,0,0,119,0,0,127,0,0,19,19,0,27,27,0,35,35,0,47,43,0,55,47,0,67,55,0,75,59,7,87,67,7,
95,71,7,107,75,11,119,83,15,131,87,19,139,91,19,151,95,27,163,99,31,175,103,35,35,19,7,47,23,11,59,31,15,75,35,19,
87,43,23,99,47,31,115,55,35,127,59,43,143,67,51,159,79,51,175,99,47,191,119,47,207,143,43,223,171,39,239,203,31,255,
243,27,11,7,0,27,19,0,43,35,15,55,43,19,71,51,27,83,55,35,99,63,43,111,71,51,127,83,63,139,95,71,155,107,83,167,123,
95,183,135,107,195,147,123,211,163,139,227,179,151,171,139,163,159,127,151,147,115,135,139,103,123,127,91,111,119,
83,99,107,75,87,95,63,75,87,55,67,75,47,55,67,39,47,55,31,35,43,23,27,35,19,19,23,11,11,15,7,7,187,115,159,175,107,
143,163,95,131,151,87,119,139,79,107,127,75,95,115,67,83,107,59,75,95,51,63,83,43,55,71,35,43,59,31,35,47,23,27,35,
19,19,23,11,11,15,7,7,219,195,187,203,179,167,191,163,155,175,151,139,163,135,123,151,123,111,135,111,95,123,99,83,
107,87,71,95,75,59,83,63,51,67,51,39,55,43,31,39,31,23,27,19,15,15,11,7,111,131,123,103,123,111,95,115,103,87,107,
95,79,99,87,71,91,79,63,83,71,55,75,63,47,67,55,43,59,47,35,51,39,31,43,31,23,35,23,15,27,19,11,19,11,7,11,7,255,
243,27,239,223,23,219,203,19,203,183,15,187,167,15,171,151,11,155,131,7,139,115,7,123,99,7,107,83,0,91,71,0,75,55,
0,59,43,0,43,31,0,27,15,0,11,7,0,0,0,255,11,11,239,19,19,223,27,27,207,35,35,191,43,43,175,47,47,159,47,47,143,47,
47,127,47,47,111,47,47,95,43,43,79,35,35,63,27,27,47,19,19,31,11,11,15,43,0,0,59,0,0,75,7,0,95,7,0,111,15,0,127,23,
7,147,31,7,163,39,11,183,51,15,195,75,27,207,99,43,219,127,59,227,151,79,231,171,95,239,191,119,247,211,139,167,123,
59,183,155,55,199,195,55,231,227,87,127,191,255,171,231,255,215,255,255,103,0,0,139,0,0,179,0,0,215,0,0,255,0,0,255,
243,147,255,247,199,255,255,255,159,91,83
};

/*
================
SaveImage

handle bmp & tga
================
*/
static bool Makewad_SaveImage( const char *filename, rgbdata_t *pix )
{
	const char *ext = COM_FileExtension(filename);

	if (!pix)
	{
		Msg(S_ERROR "pix == NULL\n");
		return false;
	}

	if (!Q_stricmp(ext, "tga"))
		return Image_SaveTGA(filename, pix);
	else if (!Q_stricmp(ext, "bmp"))
		return Image_SaveBMP(filename, pix);
	else if (!Q_stricmp(ext, "dds"))
		return Image_SaveDDS(filename, pix);
	else if (!Q_stricmp(ext, "png"))
		return Image_SavePNG(filename, pix);
	else if (!Q_stricmp(ext, "lmp"))
		return LMP_WriteLmptex(filename, pix, true);
	else
	{
		Msg(S_ERROR "unsupported format (%s)\n", ext);
		return false;
	}
}

/*
================
LoadImage

handle bmp & tga
================
*/
static rgbdata_t *Makewad_LoadImage( const char *filename, bool quiet = false )
{
	size_t fileSize;	
	byte *buf = (byte *)COM_LoadFile( filename, &fileSize, false );
	const char *ext = COM_FileExtension( filename );
	rgbdata_t *pic = NULL;
	char barename[64];

	if (!buf && source_wad != NULL)
	{
		COM_FileBase(filename, barename);
		buf = W_LoadLump(source_wad, barename, &fileSize, W_TypeFromExt(filename));
	}

	if (!buf)
	{
		if (!quiet) {
			Msg(S_ERROR "unable to load (%s)\n", filename);
		}
		return NULL;
	}

	if (!Q_stricmp(ext, "tga"))
		pic = Image_LoadTGA(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "bmp"))
		pic = Image_LoadBMP(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "mip"))
		pic = Image_LoadMIP(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "lmp"))
		pic = Image_LoadLMP(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "dds"))
		pic = Image_LoadDDS(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "png"))
		pic = Image_LoadPNG(filename, buf, fileSize);
	else if (!quiet) {
		Msg(S_ERROR "unsupported format (%s)\n", ext);
	}

	Mem_Free( buf ); // release file

	if( pic != NULL )
	{
		// check for quake1 palette
		if( FBitSet( pic->flags, IMAGE_QUANTIZED ) && pic->palette != NULL )
		{
			byte src[256*3];

			// first we need to turn palette into 768 bytes
			for( int i = 0; i < 256; i++ )
			{
				src[i*3+0] = pic->palette[i*4+0];
				src[i*3+1] = pic->palette[i*4+1];
				src[i*3+2] = pic->palette[i*4+2];
			}

			if( !memcmp( src, palette_q1, sizeof( src )))
				SetBits( pic->flags, IMAGE_QUAKE1_PAL );
		}
	}

	return pic; // may be NULL
}

// just for debug
void Test_ConvertImageTo8Bit( const char *filename )
{
	char outpath[256];
	rgbdata_t	*pic;

	MsgDev( D_INFO, "Quantize %s\n", filename );

	pic = Makewad_LoadImage( filename );
	if( !pic )
	{
		MsgDev( D_ERROR, "couldn't load (%s)\n", filename );
		return;
	}

	pic = Image_Quantize( pic );
	Q_strncpy( outpath, filename, sizeof( outpath ));
	COM_StripExtension( outpath );
	Q_strncat( outpath, "_8bit.bmp", sizeof( outpath ));

	if( Makewad_SaveImage( outpath, pic ))
		MsgDev( D_INFO, "Write %s\n", outpath );
	else MsgDev( D_INFO, "Failed to save %s\n", outpath );

	Image_Free( pic );
}

/*
=============
WAD_CreateTexture

put the texture in the wad or
extract her onto disk
=============
*/
FileStatusCode WAD_CreateTexture( const char *filename )
{
	const char *ext = COM_FileExtension( filename );
	int len = WAD3_NAMELEN;
	dlumpinfo_t *find;
	char lumpname[64];
	int mipwidth, mipheight;
	rgbdata_t *image = nullptr, *alphaMask = nullptr;

	// store name for detect suffixes
	COM_FileBase(filename, lumpname);
	int hint = Image_HintFromSuf(lumpname, true);

	if (hint != IMG_DIFFUSE)
	{
		return FileStatusCode::InvalidImageHint; // only diffuse textures can be passed
	}

	if (Q_strlen(lumpname) > len)
	{
		// NOTE: technically we can cutoff too long names but it just not needs
		return FileStatusCode::NameTooLong;
	}

	image = Makewad_LoadImage(filename);
	if (!image) {
		return FileStatusCode::ErrorSilent;
	}

	mipwidth = image->width;
	mipheight = image->height;
	bool imageHasAlpha = FBitSet(image->flags, IMAGE_HAS_8BIT_ALPHA);

	// wad-copy mode: wad->wad
	if (working_mode == ProgramWorkingMode::CopyTexturesToWad)
	{
		bool result;
		bool extensionMip = !Q_stricmp(ext, "mip");
		bool extensionLmp = !Q_stricmp(ext, "lmp");
		if (extensionMip || extensionLmp)
		{
			if (extensionMip)
				find = W_FindMiptex(source_wad, lumpname);
			else
				find = W_FindLmptex(source_wad, lumpname);

			if (!find)
			{
				Image_Free( image );
				return FileStatusCode::NoMatchedInputFiles;
			}

			if (extensionMip)
			{
				find = W_FindMiptex(output_wad, lumpname);
				if (!MIP_CheckForReplace(find, image, mipwidth, mipheight))
					return FileStatusCode::ErrorSilent; // NOTE: image already freed on failed
			}
			else
			{
				find = W_FindLmptex(output_wad, lumpname);
				if (!LMP_CheckForReplace(find, image, mipwidth, mipheight))
					return FileStatusCode::ErrorSilent; // NOTE: image already freed on failed
			}

			// fit to the replacement
			image = Image_Resample(image, mipwidth, mipheight);
			image = Image_Quantize(image); // just in case.

			if (extensionMip)
				result = MIP_WriteMiptex(lumpname, image);
			else
				result = LMP_WriteLmptex(lumpname, image);

			Image_Free( image );
			return result ? FileStatusCode::Success : FileStatusCode::ErrorSilent;
		}
		else
		{
			Image_Free( image );
			return FileStatusCode::UnknownLumpFormat;
		}
	}

	// extraction or conversion mode: wad->bmp, wad->tga
	if (working_mode == ProgramWorkingMode::ExtractTextures)
	{
		char real_ext[8];

		// set default extension
		Q_strcpy( real_ext, output_ext );

		// wad-extract mode
		const char *path = va( "%s/%s.%s", output_path, lumpname, real_ext );
		bool result = Makewad_SaveImage( path, image );
		Image_Free( image );

		return result ? FileStatusCode::Success : FileStatusCode::ErrorSilent;
	}

	// normal mode: bmp->wad, tga->wad
	// append '{' character in case source image has transparency
	if (imageHasAlpha && lumpname[0] != '{')
	{
		char changedLumpName[64];
		Q_snprintf(changedLumpName, sizeof(changedLumpName), "{%s", lumpname);
		Q_strncpy(lumpname, changedLumpName, sizeof(lumpname));
	}

	if (graphics_wadfile)
	{
		// check for minmax sizes
		mipwidth = bound(IMAGE_MINWIDTH, mipwidth, IMAGE_MAXWIDTH);
		mipheight = bound(IMAGE_MINHEIGHT, mipheight, IMAGE_MAXHEIGHT);

		find = W_FindLmptex(output_wad, lumpname);

		if (!LMP_CheckForReplace(find, image, mipwidth, mipheight)) {
			return FileStatusCode::ErrorSilent; // NOTE: image already freed on failed
		}

		image = Image_Resample(image, mipwidth, mipheight); // align by 16 or fit to the replacement
		if (!FBitSet(image->flags, IMAGE_QUANTIZED))
		{
			if (imageHasAlpha) {
				alphaMask = Image_ExtractAlphaMask(image);
			}
			image = Image_Quantize(image); // now quantize image
			if (imageHasAlpha) 
			{
				Image_ApplyAlphaMask(image, alphaMask, alpha_threshold);
				Image_Free(alphaMask);
			}
		}

		bool result = LMP_WriteLmptex(lumpname, image);
		Image_Free(image);
		return result ? FileStatusCode::Success : FileStatusCode::ErrorSilent;
	}
	else
	{
		// check for minmax sizes
		mipwidth = bound(IMAGE_MINWIDTH, mipwidth, IMAGE_MAXWIDTH);
		mipheight = bound(IMAGE_MINHEIGHT, mipheight, IMAGE_MAXHEIGHT);

		if (resize_percent > 0.0f)
		{
			mipwidth *= (resize_percent / 100.0f);
			mipheight *= (resize_percent / 100.0f);
		}

		// all the mips must be aligned by 16
		mipwidth = (mipwidth + 7) & ~7;
		mipheight = (mipheight + 7) & ~7;

		find = W_FindMiptex(output_wad, lumpname);
		if (!MIP_CheckForReplace(find, image, mipwidth, mipheight)) {
			return FileStatusCode::ErrorSilent; // NOTE: image already freed on failed
		}

		// align by 16 or fit to the replacement
		image = Image_Resample(image, mipwidth, mipheight);
		if (!FBitSet(image->flags, IMAGE_QUANTIZED)) 
		{
			if (imageHasAlpha) {
				alphaMask = Image_ExtractAlphaMask(image);
			}
			image = Image_Quantize(image); // now quantize image
			if (imageHasAlpha) 
			{
				Image_ApplyAlphaMask(image, alphaMask, alpha_threshold);
				Image_Free(alphaMask);
			}
		}

		bool result = MIP_WriteMiptex(lumpname, image);
		Image_Free(image);
		return result ? FileStatusCode::Success : FileStatusCode::ErrorSilent;
	}

	return FileStatusCode::UnknownError;
}

void __cdecl Shutdown_Makewad( void )
{
	W_Close( source_wad );
	W_Close( output_wad );
}

static const char *GetStatusCodeDescription(FileStatusCode statusCode)
{
	switch (statusCode)
	{
		case FileStatusCode::InvalidImageHint: return "wrong texture type";
		case FileStatusCode::NameTooLong: return "file name too long";
		case FileStatusCode::NoMatchedInputFiles: return "no matched files in source WAD";
		case FileStatusCode::UnknownError: return "unknown error";
		case FileStatusCode::UnknownLumpFormat: return "unknown lump format";
	}
	return "unknown";
}

static void PrintTitle()
{
	Msg("\n");
	Msg("  pxmakewad - utility for editing or creating WAD files from textures\n");
	Msg("  Version   : %s (^1%s ^7/ ^2%s ^7/ ^3%s ^7/ ^4%s^7)\n",
		APP_VERSION_STRING,
		BuildInfo::GetDate(),
		BuildInfo::GetCommitHash(),
		BuildInfo::GetArchitecture(),
		BuildInfo::GetPlatform()
	);
	Msg("  Website   : https://github.com/SNMetamorph/PrimeXT\n");
	Msg("  Usage     : <input path> <output path> <options>\n");
	Msg("  Example   : \"C:/img/*.bmp\" \"new.wad\"\n");
	Msg("\n");
}

static void PrintOptionsList()
{
	Msg("  Options list:\n"
		"     ^5-replace^7      : replace existing images if they matched by size\n"
		"     ^5-forcereplace^7 : replace existing images even if they doesn't matched by size\n"
		"     ^5-resize^7       : resize source image in percents (range 10%%-200%%)\n"
		"     ^5-alphathres^7   : alpha threshold for transparency (0.0 - 1.0, default is 0.5)\n"
		"     ^5-defaults^7     : don't ask user to set settings, use default if other not set\n"
		"     ^5-outputfmt^7    : image format to be used when extracting images (bmp/dds/png/tga, default is \"bmp\")\n"
		"     ^5-dev^7          : set message verbose level (1 - 5, default is 3)\n"
		"\n"
	);
}

static void AskPackingSettings()
{
	std::string userInput;
	while (true)
	{
		Sys_Print("Would you like to set settings? [y/n]: ");
		std::cin >> userInput;
		if (userInput.compare("y") == 0) {
			break;
		}
		else if (userInput.compare("n") == 0) {
			return;
		}
	}

	while (true)
	{
		Sys_Print("Alpha transparency threshold [0.0 - 1.0, \"d\" - for default]: ");
		std::cin >> userInput;
		if (userInput.compare("d") == 0) {
			break;
		}
		else if (!userInput.empty()) {
			alpha_threshold = bound(0.0f, atof(userInput.c_str()), 1.0f);
			break;
		}
	}

	while (true)
	{
		Sys_Print("Image resize percent [10% - 200%, \"n\" - to disable resize]: ");
		std::cin >> userInput;
		if (userInput.compare("n") == 0) {
			break;
		}
		else if (!userInput.empty()) {
			resize_percent = bound(10.0f, atof(userInput.c_str()), 200.0f);
			break;
		}
	}

	while (true)
	{
		Sys_Print("Replace existing images in WAD, if they matched by size? [y/n, \"d\" - for default]: ");
		std::cin >> userInput;
		if (userInput.compare("y") == 0) {
			SetReplaceLevel(REP_NORMAL);
			break;
		}
		else if (userInput.compare("n") == 0 || userInput.compare("d") == 0) {
			break;
		}
	}
	Sys_Print("\n");
}

static void WaitForKey()
{
	Msg("Press any key to exit...\n");
	Sys_WaitForKeyInput();
}

int main( int argc, char **argv )
{
	int		i;
	char	srcpath[256] = "", dstpath[256] = "";
	char	srcwad[256] = "", dstwad[256] = "";
	bool	srcset = false;
	bool	dstset = false;
	double	start, end;
	char	str[64];

	atexit( Shutdown_Makewad );
	Sys_SetupCrashHandler();
	PrintTitle();

	SetReplaceLevel( REP_IGNORE );
	output_path[0] = '\0';
	output_ext[0] = '\0';

	for (i = 1; i < argc; i++)
	{
		if (!Q_stricmp(argv[i], "-replace"))
		{
			ask_for_settings = false;
			SetReplaceLevel(REP_NORMAL); // replace only if image dimensions is equal
		}
		else if (!Q_stricmp(argv[i], "-forcereplace"))
		{
			ask_for_settings = false;
			SetReplaceLevel(REP_FORCE); // rescale image, replace in any cases
		}
		else if (!Q_stricmp(argv[i], "-resize"))
		{
			ask_for_settings = false;
			resize_percent = atof(argv[i + 1]); // resize source image (percent)
			resize_percent = bound(10.0f, resize_percent, 200.0f);
			i++;
		}
		else if (!Q_stricmp(argv[i], "-defaults"))
		{
			ask_for_settings = false;
		}
		else if (!Q_stricmp(argv[i], "-outputfmt"))
		{
			ask_for_settings = false;
			if (Q_stricmp(argv[i + 1], "bmp") != 0 &&
				Q_stricmp(argv[i + 1], "tga") != 0 &&
				Q_stricmp(argv[i + 1], "dds") != 0 &&
				Q_stricmp(argv[i + 1], "png") != 0)
			{
				Msg("Specified unknown output image file format\n");
				break;
			}
			Q_strncpy(output_ext, argv[i + 1], sizeof(output_ext));
			i++;
		}
		else if (!Q_stricmp(argv[i], "-alphathres"))
		{
			ask_for_settings = false;
			alpha_threshold = atof(argv[i + 1]);
			alpha_threshold = bound(0.0f, alpha_threshold, 1.0f);
			i++;
		}
		else if (!Q_stricmp(argv[i], "-dev"))
		{
			SetDeveloperLevel(atoi(argv[i + 1]));
			i++;
		}
		else if (i == 1)
		{
			Q_strncpy(srcpath, argv[i], sizeof(srcpath));
			COM_FixSlashes(srcpath);
			srcset = true;
		}
		else if (i == 2)
		{
			Q_strncpy(dstpath, argv[i], sizeof(dstpath));
			COM_FixSlashes(dstpath);
			dstset = true;
		}
		else
		{
			Msg("Unknown option %s\n", argv[i]);
			break;
		}
	}

	if (i != argc || !srcset)
	{
		PrintOptionsList();
		WaitForKey();
		return 1;
	}
	else
	{
		char testname[64];
		char *find = NULL;

		Q_strncpy( srcwad, srcpath, sizeof( srcwad ));
		find = Q_stristr( srcwad, ".wad" );

		if( find )
		{
			find += 4, *find = '\0';
			source_wad = W_Open( srcwad, "rb" );
		}

		// if pattern not set, that means we want to include ALL files in directory
		const char *a = COM_FindLastSlashEntry(srcpath);
		if (a)
		{
			const char *b = Q_strrchr(a, '*');
			if (!b) 
			{
				if (strlen(a) > 1)
					Q_strncat(srcpath, "/*.*", sizeof(srcpath));
				else
					Q_strncat(srcpath, "*.*", sizeof(srcpath));
			}
		}
		else {
			Q_strncat(srcpath, "/*.*", sizeof(srcpath));
		}

		search_t *search = COM_Search( srcpath, true, source_wad );
		if( !search ) 
			return 1; // nothing found

		if (dstset)
		{
			Q_strncpy(dstwad, dstpath, sizeof(dstwad));
		}
		else if (!source_wad)
		{
			const char *filename = COM_FileWithoutPath(srcwad);
			COM_ExtractFilePath(srcwad, output_path);
			COM_StripExtension(output_path);
			if (!output_path[0]) {
				Q_getwd(output_path, sizeof(output_path));
			}
			Q_snprintf(dstwad, sizeof(dstwad), "%s/%s.wad", output_path, filename);
		}

		find = Q_stristr(dstwad, ".wad");
		if( find )
		{
			find += 4, *find = '\0';
			output_wad = W_Open( dstwad, "a+b" );
			COM_FileBase( dstwad, testname );

			if( !Q_stricmp( testname, "gfx" ) || !Q_stricmp( testname, "cached" ))
			{
				Msg( "Graphics WAD detected\n" );
				graphics_wadfile = 1;
			}
		}
		else
		{
			// use .bmp format for extracting textures from WAD by default, if other not specified
			if (Q_strlen(output_ext) == 0) {
				Q_strncpy(output_ext, "bmp", sizeof(output_ext));
			}

			if (dstset) {
				Q_strncpy(output_path, dstpath, sizeof(output_path));
			}
			else
			{
				char filename[64];
				COM_ExtractFilePath(srcwad, output_path);
				COM_StripExtension(output_path);

				if (!output_path[0]) {
					Q_getwd(output_path, sizeof(output_path));
				}

				COM_FileBase(source_wad->filename, filename);
				Q_strncat(output_path, "/", sizeof(output_path));
				Q_strncat(output_path, filename, sizeof(output_path));
				Q_strncat(output_path, "_extracted", sizeof(output_path));
			}
		}

		if (source_wad && !output_wad && output_path[0] && output_ext[0])
		{
			MsgDev(D_INFO, "Extract textures from ^3%s\n", srcwad);
			working_mode = ProgramWorkingMode::ExtractTextures;
		}
		else if (source_wad && output_wad)
		{
			MsgDev(D_INFO, "Copying textures from ^3%s^7 to ^3%s\n", srcwad, dstwad);
			working_mode = ProgramWorkingMode::CopyTexturesToWad;
		}
		else
		{
			AskPackingSettings();
			MsgDev(D_INFO, "Packing textures into ^3%s\n", dstwad);
			working_mode = ProgramWorkingMode::PackingTextures;
		}

		start = I_FloatTime();
		for (i = 0; i < search->numfilenames; i++)
		{
			Msg("Processing %s... ", search->filenames[i]);

			FileStatusCode statusCode = WAD_CreateTexture(search->filenames[i]);
			if (statusCode == FileStatusCode::Success) {
				Msg("^2success\n");
				processed_files++;
			}
			else if (statusCode != FileStatusCode::ErrorSilent) {
				Msg("^1%s\n", GetStatusCodeDescription(statusCode));
			}
		}

		end = I_FloatTime();
		Q_timestring((int)(end - start), str);

		MsgDev(D_INFO, "\n");
		MsgDev(D_INFO, "%i/%i files processed\n", processed_files, search->numfilenames);
		MsgDev(D_INFO, "%s elapsed\n", str);
		MsgDev(D_INFO, "\n");

		W_Close(source_wad);
		source_wad = NULL;
		W_Close(output_wad);
		output_wad = NULL;

		Mem_Free(search);
		Mem_Check(); // report leaks
	}

	WaitForKey();
	Sys_RestoreCrashHandler();
	return 0;
}
