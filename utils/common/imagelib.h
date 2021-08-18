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

#define GAMMA		( 2.2f )		// Valve Software gamma
#define INVGAMMA		( 1.0f / 2.2f )	// back to 1.0

/*
========================================================================

.BMP image format

========================================================================
*/
#pragma pack( 1 )
typedef struct
{
	char	id[2];		// bmfh.bfType
	dword	fileSize;		// bmfh.bfSize
	dword	reserved0;	// bmfh.bfReserved1 + bmfh.bfReserved2
	dword	bitmapDataOffset;	// bmfh.bfOffBits
	dword	bitmapHeaderSize;	// bmih.biSize
	int	width;		// bmih.biWidth
	int	height;		// bmih.biHeight
	word	planes;		// bmih.biPlanes
	word	bitsPerPixel;	// bmih.biBitCount
	dword	compression;	// bmih.biCompression
	dword	bitmapDataSize;	// bmih.biSizeImage
	dword	hRes;		// bmih.biXPelsPerMeter
	dword	vRes;		// bmih.biYPelsPerMeter
	dword	colors;		// bmih.biClrUsed
	dword	importantColors;	// bmih.biClrImportant
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
	byte	id_length;
	byte	colormap_type;
	byte	image_type;
	word	colormap_index;
	word	colormap_length;
	byte	colormap_size;
	word	x_origin;
	word	y_origin;
	word	width;
	word	height;
	byte	pixel_size;
	byte	attributes;
} tga_t;
#pragma pack( )

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
typedef enum
{
	IMAGE_QUANTIZED		= BIT( 0 ),	// this image already quantized
	IMAGE_HAS_COLOR		= BIT( 1 ),	// image contain RGB-channel
	IMAGE_HAS_1BIT_ALPHA	= BIT( 2 ),	// textures with '{'
	IMAGE_HAS_8BIT_ALPHA	= BIT( 3 ),	// image contain full-range alpha-channel
	IMAGE_HAS_SDF_ALPHA		= BIT( 4 ),	// SIgned distance field alpha
	IMAGE_CUBEMAP		= BIT( 5 ),	// it's 6-sides cubemap buffer
	IMAGE_SKYBOX		= BIT( 6 ),	// it's 6-sides skybox buffer
	IMAGE_DXT_FORMAT		= BIT( 7 ),	// this image have a DXT compression
	IMAGE_QUAKE1_PAL		= BIT( 8 ),	// image has Quake1 palette
	IMAGE_NOMIPS		= BIT( 9 ),	// don't build mips before DXT compression

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
	word	width;		// image width
	word	height;		// image height
	word	flags;		// misc image flags
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
	size_t	size;		// for bounds checking
	float	reflectivity[3];	// sum color of all pixels
} rgbdata_t;

typedef struct imgtype_s
{
	char	*ext;
	char	type;
} imgtype_t;

typedef struct loadimage_s
{
	const char	*formatstring;
	const char	*ext;
	rgbdata_t		*(*loadfunc)( const char *name, const byte *buffer, size_t filesize );
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

// image storing
bool Image_SaveTGA( const char *name, rgbdata_t *pix );
bool Image_SaveBMP( const char *name, rgbdata_t *pix );
bool Image_SaveDDS( const char *name, rgbdata_t *pix );

// common functions
rgbdata_t *Image_Alloc( int width, int height, bool paletted = false );
rgbdata_t *Image_AllocCubemap( int width, int height );
rgbdata_t *Image_AllocSkybox( int width, int height );
rgbdata_t *Image_Copy( rgbdata_t *src );
void Image_PackRGB( float flColor[3], dword &icolor );
void Image_UnpackRGB( dword icolor, float flColor[3] );
char Image_HintFromSuf( const char *lumpname );
rgbdata_t *COM_LoadImage( const char *filename, bool quiet = false );
rgbdata_t *COM_LoadImageMemory( const char *filename, const byte *buf, size_t fileSize );
const imgtype_t *Image_ImageTypeFromHint( char value );
bool COM_SaveImage( const char *filename, rgbdata_t *pix );
bool Image_ValidSize( const char *name, int width, int height );
void Image_BuildMipMap( byte *in, int width, int height, bool isNormalMap );
rgbdata_t *Image_Resample( rgbdata_t *pic, int new_width, int new_height );
rgbdata_t *Image_MergeColorAlpha( rgbdata_t *color, rgbdata_t *alpha );
rgbdata_t *Image_CreateCubemap( rgbdata_t *images[6], bool skybox = false, bool nomips = false );
void Image_ConvertBumpStalker( rgbdata_t *bump, rgbdata_t *gloss );
void Image_MakeSignedDistanceField( rgbdata_t *pic );
rgbdata_t *Image_ExtractAlphaMask( rgbdata_t *pic );
void Image_MakeOneBitAlpha( rgbdata_t *pic );
rgbdata_t *Image_Quantize( rgbdata_t *pic );
void Image_ApplyGamma( rgbdata_t *pic );
rgbdata_t *Image_Flip( rgbdata_t *src );

extern float	g_gamma;

#endif//IMAGELIB_H