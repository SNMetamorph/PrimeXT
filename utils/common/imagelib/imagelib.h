/*
imagelib.h - image processing library for PrimeXT utilities
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

#ifndef IMAGELIB_H
#define IMAGELIB_H

#include <stdint.h>

#define GAMMA		( 2.2f )		// Valve Software gamma
#define INVGAMMA	( 1.0f / 2.2f )	// back to 1.0

/*
========================================================================
.BMP image format
========================================================================
*/
#pragma pack( 1 )
typedef struct
{
	int8_t		id[2];		// bmfh.bfType
	uint32_t	fileSize;	// bmfh.bfSize
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
#pragma pack(1)
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
#pragma pack()

/*
========================================================================
.PNG image format	(Portable Network Graphics)
========================================================================
*/
enum png_colortype
{
	PNG_CT_GREY,
	PNG_CT_RGB = BIT(1),
	PNG_CT_PALLETE = (PNG_CT_RGB|BIT(0)),
	PNG_CT_ALPHA = BIT(2),
	PNG_CT_RGBA = (PNG_CT_RGB|PNG_CT_ALPHA)
};

enum png_filter
{
	PNG_F_NONE,
	PNG_F_SUB,
	PNG_F_UP,
	PNG_F_AVERAGE,
	PNG_F_PAETH
};

#pragma pack( push, 1 )
typedef struct png_ihdr_s
{
	uint32_t    width;
	uint32_t    height;
	uint8_t     bitdepth;
	uint8_t     colortype;
	uint8_t     compression;
	uint8_t     filter;
	uint8_t     interlace;
} png_ihdr_t;

typedef struct png_s
{
	uint8_t     sign[8];
	uint32_t    ihdr_len;
	uint8_t     ihdr_sign[4];
	png_ihdr_t  ihdr_chunk;
	uint32_t    ihdr_crc32;
} png_t;

typedef struct png_footer_s
{
	uint32_t    idat_crc32;
	uint32_t    iend_len;
	uint8_t     iend_sign[4];
	uint32_t    iend_crc32;
} png_footer_t;
#pragma pack( pop )

#define IMAGE_MINWIDTH	1			// last mip-level is 1x1
#define IMAGE_MINHEIGHT	1
#define IMAGE_MAXWIDTH	4096
#define IMAGE_MAXHEIGHT	4096
#define MIP_MAXWIDTH	1024			// large sizes it's too complicated for quantizer
#define MIP_MAXHEIGHT	1024			// and provoked color degradation

#define IMAGE_HAS_ALPHA	(IMAGE_HAS_1BIT_ALPHA|IMAGE_HAS_8BIT_ALPHA|IMAGE_HAS_SDF_ALPHA)

#define IMG_DIFFUSE		0	// same as default pad1 always equal 0
#define IMG_ALPHAMASK	1	// alpha-channel that stored separate as luminance texture
#define IMG_NORMALMAP	2	// indexed normalmap
#define IMG_GLOSSMAP	3	// luminance or color specularity map
#define IMG_GLOSSPOWER	4	// gloss power map (each value is a specular pow)
#define IMG_HEIGHTMAP	5	// heightmap (for parallax occlusion mapping or source of normalmap)
#define IMG_LUMA		6	// luma or glow texture with self-illuminated parts
#define IMG_STALKER_BUMP	7	// stalker two-component bump
#define IMG_STALKER_GLOSS	8	// stalker two-component bump

// NOTE: ordering is important!
#define IMG_SKYBOX_FT	9
#define IMG_SKYBOX_BK	10
#define IMG_SKYBOX_UP	11
#define IMG_SKYBOX_DN	12
#define IMG_SKYBOX_RT	13
#define IMG_SKYBOX_LF	14

#define IMG_CUBEMAP_PX	15
#define IMG_CUBEMAP_NX	16
#define IMG_CUBEMAP_PY	17
#define IMG_CUBEMAP_NY	18
#define IMG_CUBEMAP_PZ	19
#define IMG_CUBEMAP_NZ	20

// rgbdata->flags
typedef enum : uint64_t
{
	IMAGE_QUANTIZED			= BIT( 0 ),	// this image already quantized
	IMAGE_HAS_COLOR			= BIT( 1 ),	// image contain RGB-channel
	IMAGE_HAS_1BIT_ALPHA	= BIT( 2 ),	// textures with '{'
	IMAGE_HAS_8BIT_ALPHA	= BIT( 3 ),	// image contain full-range alpha-channel
	IMAGE_HAS_SDF_ALPHA		= BIT( 4 ),	// SIgned distance field alpha
	IMAGE_CUBEMAP			= BIT( 5 ),	// it's 6-sides cubemap buffer
	IMAGE_SKYBOX			= BIT( 6 ),	// it's 6-sides skybox buffer
	IMAGE_DXT_FORMAT		= BIT( 7 ),	// this image have a DXT compression
	IMAGE_QUAKE1_PAL		= BIT( 8 ),	// image has Quake1 palette
	IMAGE_NOMIPS			= BIT( 9 ),	// don't build mips before DXT compression

	// Image_Process manipulation flags
	IMAGE_FLIP_X		= BIT( 10 ),	// flip the image by width
	IMAGE_FLIP_Y		= BIT( 11 ),	// flip the image by height
	IMAGE_ROT_90		= BIT( 12 ),	// flip from upper left corner to down right corner
	IMAGE_ROT180		= IMAGE_FLIP_X|IMAGE_FLIP_Y,
	IMAGE_ROT270		= IMAGE_FLIP_X|IMAGE_FLIP_Y|IMAGE_ROT_90,
} imgFlags_t;

// loaded image
typedef struct rgbdata_s
{
	size_t		width;		// image width
	size_t		height;		// image height
	uint64_t	flags;		// misc image flags
	byte		*palette;	// palette if present
	byte		*buffer;	// image buffer
	size_t		size;		// for bounds checking
	float		reflectivity[3]; // sum color of all pixels
} rgbdata_t;

typedef struct imgtype_s
{
	const char *ext;
	int type;
	bool permissive;
} imgtype_t;

typedef struct loadimage_s
{
	const char	*formatstring;
	const char	*ext;
	rgbdata_t	*(*loadfunc)( const char *name, const byte *buffer, size_t filesize );
} loadimage_t;

typedef struct saveimage_s
{
	const char	*formatstring;
	const char	*ext;
	bool		(*savefunc)( const char *name, rgbdata_t *pix );
} saveimage_t;

// image loading
rgbdata_t *Image_LoadTGA( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *Image_LoadBMP( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *Image_LoadDDS( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *Image_LoadPNG( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *Image_LoadMIP( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *Image_LoadLMP( const char *name, const byte *buffer, size_t filesize );

// image storing
bool Image_SaveTGA( const char *name, rgbdata_t *pix );
bool Image_SaveBMP( const char *name, rgbdata_t *pix );
bool Image_SaveDDS( const char *name, rgbdata_t *pix );
bool Image_SavePNG( const char *name, rgbdata_t *pix );

// common functions
rgbdata_t *Image_Alloc( int width, int height, bool paletted = false );
void Image_Free( rgbdata_t *image );
rgbdata_t *Image_AllocCubemap( int width, int height );
rgbdata_t *Image_AllocSkybox( int width, int height );
rgbdata_t *Image_Copy( rgbdata_t *src );
void Image_PackRGB( float flColor[3], uint32_t &icolor );
void Image_UnpackRGB( uint32_t icolor, float flColor[3] );
int Image_HintFromSuf( const char *lumpname, bool permissive = false );
rgbdata_t *COM_LoadImage( const char *filename, bool quiet = false );
const imgtype_t *Image_ImageTypeFromHint( char value );
bool COM_SaveImage( const char *filename, rgbdata_t *pix );
bool Image_ValidSize( const char *name, int width, int height );
void Image_BuildMipMap( byte *in, int width, int height, bool isNormalMap );
rgbdata_t *Image_Resample( rgbdata_t *pic, int new_width, int new_height );
rgbdata_t *Image_MergeColorAlpha( rgbdata_t *color, rgbdata_t *alpha );
rgbdata_t *Image_CreateCubemap( rgbdata_t *images[6], bool skybox = false, bool nomips = false );
void Image_ConvertBumpStalker( rgbdata_t *bump, rgbdata_t *gloss );
rgbdata_t *Image_ExtractAlphaMask( rgbdata_t *pic );
bool Image_ApplyAlphaMask( rgbdata_t *pic, rgbdata_t *alphaMask, float alphaThreshold );
rgbdata_t *Image_Quantize( rgbdata_t *pic );
void Image_ApplyGamma( rgbdata_t *pic );

extern float	g_gamma;

#endif // IMAGELIB_H
