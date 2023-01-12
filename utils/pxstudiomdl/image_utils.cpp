/*
image_utils.cpp - utility routines for working with images
Copyright (C) 2015 Uncle Mike
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "scriplib.h"
#include "filesystem.h"
#include "imagelib.h"
#include "image_utils.h"

/*
================
COM_LoadImage

handle bmp & tga
================
*/
rgbdata_t *ImageUtils::LoadImageFile( const char *filename )
{
	size_t fileSize;
	byte *buf = (byte *)COM_LoadFile(filename, &fileSize, false);
	const char *ext = COM_FileExtension(filename);
	rgbdata_t *pic = nullptr;

	if (!buf)
	{
		MsgDev(D_ERROR, "LoadImageFile: unable to load (%s)\n", filename);
		return NULL;
	}

	if (!Q_stricmp(ext, "tga"))
		pic = Image_LoadTGA(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "bmp"))
		pic = Image_LoadBMP(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "dds"))
		pic = Image_LoadDDS(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "png"))
		pic = Image_LoadPNG(filename, buf, fileSize);
	else {
		MsgDev(D_ERROR, "LoadImageFile: unsupported format (%s)\n", ext);
	}

	Mem_Free(buf); // release file
	return pic;
}

/*
================
COM_LoadImage

handle bmp & tga
================
*/
rgbdata_t *ImageUtils::LoadImageMemory( const char *filename, const byte *buf, size_t fileSize )
{
	const char *ext = COM_FileExtension(filename);
	rgbdata_t *pic = nullptr;

	if (!buf)
	{
		MsgDev(D_ERROR, "LoadImageMemory: unable to load (%s)\n", filename);
		return NULL;
	}

	if (!Q_stricmp(ext, "tga"))
		pic = Image_LoadTGA(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "bmp"))
		pic = Image_LoadBMP(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "dds"))
		pic = Image_LoadDDS(filename, buf, fileSize);
	else if (!Q_stricmp(ext, "png"))
		pic = Image_LoadPNG(filename, buf, fileSize);
	else {
		MsgDev(D_ERROR, "LoadImageFile: unsupported format (%s)\n", ext);
	}

	return pic;
}

/*
================
Image_ApplyPaletteGamma

we can't store alpha-channel into 8-bit texture
but we can store it separate as another image
================
*/
void ImageUtils::ApplyPaletteGamma( rgbdata_t *pic )
{
	if( !pic || g_gamma == 1.8f )
		return;

	if( !FBitSet( pic->flags, IMAGE_QUANTIZED ))
		return; // only for quantized images

	float g = g_gamma / 1.8;

	// gamma correct the monster textures to a gamma of 1.8
	for( int i = 0; i < 256; i++ )
	{
		pic->palette[i*4+0] = pow( pic->palette[i*4+0] / 255.0f, g ) * 255;
		pic->palette[i*4+1] = pow( pic->palette[i*4+1] / 255.0f, g ) * 255;
		pic->palette[i*4+2] = pow( pic->palette[i*4+2] / 255.0f, g ) * 255;
	}
}
