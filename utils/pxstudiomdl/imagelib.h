/*
imagelib.h - simple loader\serializer for TGA & BMP
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

#ifndef IMAGELIB_H
#define IMAGELIB_H
#include <stdint.h>

/*
========================================================================

.BMP image format

========================================================================
*/
#pragma pack( 1 )
typedef struct
{
	int8_t		id[2];		// bmfh.bfType
	uint32_t	fileSize;		// bmfh.bfSize
	uint32_t	reserved0;	// bmfh.bfReserved1 + bmfh.bfReserved2
	uint32_t	bitmapDataOffset;	// bmfh.bfOffBits
	uint32_t	bitmapHeaderSize;	// bmih.biSize
	int32_t		width;		// bmih.biWidth
	int32_t		height;		// bmih.biHeight
	uint16_t	planes;		// bmih.biPlanes
	uint16_t	bitsPerPixel;	// bmih.biBitCount
	uint32_t	compression;	// bmih.biCompression
	uint32_t	bitmapDataSize;	// bmih.biSizeImage
	uint32_t	hRes;		// bmih.biXPelsPerMeter
	uint32_t	vRes;		// bmih.biYPelsPerMeter
	uint32_t	colors;		// bmih.biClrUsed
	uint32_t	importantColors;	// bmih.biClrImportant
} bmp_t;
#pragma pack( )

/*
========================================================================

.TGA image format	(Truevision Targa)

========================================================================
*/
#pragma pack( 1 )
typedef struct tga_s
{
	uint8_t		id_length;
	uint8_t		colormap_type;
	uint8_t		image_type;
	uint16_t	colormap_index;
	uint16_t	colormap_length;
	uint8_t		colormap_size;
	uint16_t	x_origin;
	uint16_t	y_origin;
	uint16_t	width;
	uint16_t	height;
	uint8_t		pixel_size;
	uint8_t		attributes;
} tga_t;
#pragma pack( )

#define IMAGE_MINWIDTH	1			// last mip-level is 1x1
#define IMAGE_MINHEIGHT	1
#define IMAGE_MAXWIDTH	4096
#define IMAGE_MAXHEIGHT	4096
#define MIP_MAXWIDTH	1024			// large sizes it's too complicated for quantizer
#define MIP_MAXHEIGHT	1024			// and provoked color degradation

#define IMAGE_HAS_ALPHA	(IMAGE_HAS_1BIT_ALPHA|IMAGE_HAS_8BIT_ALPHA)

// rgbdata->flags
typedef enum
{
	IMAGE_QUANTIZED		= BIT( 0 ),	// this image already quantized
	IMAGE_HAS_COLOR		= BIT( 1 ),	// image contain RGB-channel
	IMAGE_HAS_1BIT_ALPHA	= BIT( 2 ),	// textures with '{'
	IMAGE_HAS_8BIT_ALPHA	= BIT( 3 ),	// image contain full-range alpha-channel
} imgFlags_t;

// loaded image
typedef struct rgbdata_s
{
	uint16_t width;		// image width
	uint16_t height;	// image height
	uint16_t flags;		// misc image flags
	uint8_t	*palette;	// palette if present
	uint8_t *buffer;	// image buffer
	size_t	size;		// for bounds checking
} rgbdata_t;

// common functions
rgbdata_t *Image_Alloc( int width, int height, bool paletted = false );
rgbdata_t *Image_Copy( rgbdata_t *src );
rgbdata_t *COM_LoadImage( const char *filename );
rgbdata_t *COM_LoadImageMemory( const char *filename, const byte *buf, size_t fileSize );
bool Image_ValidSize( const char *name, int width, int height );
rgbdata_t *Image_Resample( rgbdata_t *pic, int new_width, int new_height );
rgbdata_t *Image_Quantize( rgbdata_t *pic );
void Image_MakeOneBitAlpha( rgbdata_t *pic );
void Image_ApplyGamma( rgbdata_t *pic );

extern float	g_gamma;

#endif//IMAGELIB_H