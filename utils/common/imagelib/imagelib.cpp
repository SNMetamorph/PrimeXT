/*
imagelib.cpp - image processing library for PrimeXT utilities
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
#include "conprint.h"
#include "cmdlib.h"
#include "stringlib.h"
#include "imagelib.h"
#include "filesystem.h"
#include "ddstex.h"
#include "mathlib.h"
#include "crclib.h"
#include <fcntl.h>
#include <stdio.h>
#include <miniz.h>

#if XASH_WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <winsock.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <netinet/in.h>
#endif

#define BI_SIZE 40 // size of bitmap info header.

#if !XASH_WIN32

#define BI_RGB 0

#pragma pack(push, 1)
typedef struct tagRGBQUAD {
  uint8_t rgbBlue;
  uint8_t rgbGreen;
  uint8_t rgbRed;
  uint8_t rgbReserved;
} RGBQUAD;
#pragma pack(pop)

#endif

// suffix converts to img_type and back
const imgtype_t img_hints[] =
{
{ "_mask",   IMG_ALPHAMASK,		false }, // alpha-channel stored to another lump
{ "_norm",   IMG_NORMALMAP,		false }, // indexed normalmap
{ "_nrm",    IMG_NORMALMAP,		false }, // indexed normalmap
{ "_local",  IMG_NORMALMAP,		false }, // indexed normalmap
{ "_ddn",    IMG_NORMALMAP,		false }, // indexed normalmap
{ "_spec",   IMG_GLOSSMAP,		false }, // grayscale\color specular
{ "_gloss",  IMG_GLOSSMAP,		false }, // grayscale\color specular
{ "_hmap",   IMG_HEIGHTMAP,		false }, // heightmap (can be converted to normalmap)
{ "_height", IMG_HEIGHTMAP,		false }, // heightmap (can be converted to normalmap)
{ "_luma",   IMG_LUMA,			false }, // self-illuminate parts on the diffuse
{ "_add",    IMG_LUMA,			false }, // self-illuminate parts on the diffuse
{ "_illum",  IMG_LUMA,			false }, // self-illuminate parts on the diffuse
{ "_bump",   IMG_STALKER_BUMP,	false }, // stalker two-component bump
{ "_bump#",  IMG_STALKER_GLOSS, false }, // stalker two-component bump
{ "ft",      IMG_SKYBOX_FT,		true },
{ "bk",      IMG_SKYBOX_BK,		true },
{ "up",      IMG_SKYBOX_UP,		true },
{ "dn",      IMG_SKYBOX_DN,		true },
{ "rt",      IMG_SKYBOX_RT,		true },
{ "lf",      IMG_SKYBOX_LF,		true },
{ "px",      IMG_CUBEMAP_PX,	true },
{ "nx",      IMG_CUBEMAP_NX,	true },
{ "py",      IMG_CUBEMAP_PY,	true },
{ "ny",      IMG_CUBEMAP_NY,	true },
{ "pz",      IMG_CUBEMAP_PZ,	true },
{ "nz",      IMG_CUBEMAP_NZ,	true },
{ NULL,      0,					true }  // terminator
};

static const loadimage_t load_hint[] =
{
{ "%s%s.%s", "bmp", Image_LoadBMP },	// Windows Bitmap
{ "%s%s.%s", "tga", Image_LoadTGA },	// TrueVision Targa
{ "%s%s.%s", "dds", Image_LoadDDS },	// DirectDraw Surface
{ NULL, NULL, NULL }
};

// Xash3D normal instance
static const saveimage_t save_hint[] =
{
{ "%s%s.%s", "bmp", Image_SaveBMP },	// Windows Bitmap
{ "%s%s.%s", "tga", Image_SaveTGA },	// TrueVision Targa
{ "%s%s.%s", "dds", Image_SaveDDS },	// DirectDraw Surface
{ NULL, NULL, NULL }
};

static const int8_t png_sign[] = { (int8_t)0x89, 'P', 'N', 'G', '\r', '\n', (int8_t)0x1a, '\n'};
static const int8_t ihdr_sign[] = {'I', 'H', 'D', 'R'};
static const int8_t trns_sign[] = {'t', 'R', 'N', 'S'};
static const int8_t plte_sign[] = {'P', 'L', 'T', 'E'};
static const int8_t idat_sign[] = {'I', 'D', 'A', 'T'};
static const int8_t iend_sign[] = {'I', 'E', 'N', 'D'};
static const uint32_t iend_crc32 = 0xAE426082;

/*
=================
Image_ValidSize

check image for valid dimensions
=================
*/
bool Image_ValidSize( const char *name, int width, int height )
{
	if( width > IMAGE_MAXWIDTH || height > IMAGE_MAXHEIGHT || width < IMAGE_MINWIDTH || height < IMAGE_MINHEIGHT )
	{
		MsgDev( D_ERROR, "Image: %s has invalid sizes %i x %i\n", name, width, height );
		return false;
	}
	return true;
}

/*
=================
Image_Alloc

allocate image struct and partially fill it
=================
*/
rgbdata_t *Image_Alloc( int width, int height, bool paletted )
{
	size_t pixel_size = paletted ? 1 : 4;
	size_t palette_size = paletted ? 1024 : 0;
	size_t pic_size = sizeof(rgbdata_t) + (width * height * pixel_size) + palette_size;
	rgbdata_t *pic = (rgbdata_t *)Mem_Alloc(pic_size);
	
	if (!pic) 
	{
		MsgDev(D_ERROR, "Image_Alloc: failed to allocate image (%ix%i, %zu bytes)\n", width, height, pic_size);
		return nullptr;
	}

	pic->buffer = ((byte *)pic) + sizeof(rgbdata_t); 
	if (paletted)
	{
		pic->palette = ((byte *)pic) + sizeof(rgbdata_t) + width * height * pixel_size;
		pic->flags |= IMAGE_QUANTIZED;
	}
	else
	{
		pic->palette = nullptr;
	}

	pic->width = width;
	pic->height = height;
	pic->size = width * height * pixel_size;

	return pic;
}

/*
=================
Image_Free

dispose allocated image representation
=================
*/
void Image_Free( rgbdata_t *image )
{
	if (image) {
		Mem_Free(image);
	}
}

/*
=================
Image_AllocCubemap

allocate image struct and partially fill it
=================
*/
rgbdata_t *Image_AllocCubemap( int width, int height )
{
	size_t pic_size = sizeof( rgbdata_t ) + (width * height * 4 * 6);
	rgbdata_t	*pic = (rgbdata_t *)Mem_Alloc( pic_size );

	pic->buffer = ((byte *)pic) + sizeof( rgbdata_t ); 
	pic->size = (width * height * 4 * 6);
	pic->width = width;
	pic->height = height;
	SetBits( pic->flags, IMAGE_CUBEMAP );

	return pic;
}

/*
=================
Image_AllocSkybox

allocate image struct and partially fill it
=================
*/
rgbdata_t *Image_AllocSkybox( int width, int height )
{
	size_t pic_size = sizeof( rgbdata_t ) + (width * height * 4 * 6);
	rgbdata_t	*pic = (rgbdata_t *)Mem_Alloc( pic_size );

	pic->buffer = ((byte *)pic) + sizeof( rgbdata_t ); 
	pic->size = (width * height * 4 * 6);
	pic->width = width;
	pic->height = height;
	SetBits( pic->flags, IMAGE_SKYBOX );

	return pic;
}

/*
=================
Image_Copy

make an copy of image
=================
*/
rgbdata_t *Image_Copy( rgbdata_t *src )
{
	rgbdata_t *dst = Image_Alloc(src->width, src->height, FBitSet(src->flags, IMAGE_QUANTIZED));

	memcpy(dst->buffer, src->buffer, src->size);
	if (FBitSet(src->flags, IMAGE_QUANTIZED) && src->palette) {
		memcpy(dst->palette, src->palette, 1024); // copy palette if it's presented
	}
	
	dst->size = src->size;
	dst->width = src->width;
	dst->height = src->height;
	dst->flags = src->flags;
	VectorCopy(src->reflectivity, dst->reflectivity);

	return dst;
}

/*
===========
Image_HintFromSuf

Convert name suffix into image type
===========
*/
int Image_HintFromSuf( const char *lumpname, bool permissive )
{
	char barename[64];
	char suffix[16];
	const imgtype_t	*hint;

	// trying to extract hint from the name
	Q_strncpy( barename, lumpname, sizeof( barename ));

	// we not known about filetype, so match only by filename
	for( hint = img_hints; hint->ext; hint++ )
	{
		if( Q_strlen( barename ) <= Q_strlen( hint->ext ))
			continue;	// name too short

		Q_strncpy( suffix, barename + Q_strlen( barename ) - Q_strlen( hint->ext ), sizeof( suffix ));
		if (!Q_stricmp(suffix, hint->ext))
		{
			// in some cases we don't need such strict checks, so can ability to skip it
			if (permissive && hint->permissive) {
				return IMG_DIFFUSE;
			}
			return hint->type;
		}
	}

	// special additional check for "_normal"
	if( Q_stristr( lumpname, "_normal" ))
		return IMG_NORMALMAP;

	// no any special type was found
	return IMG_DIFFUSE;
}

const imgtype_t *Image_ImageTypeFromHint( char value )
{
	const imgtype_t	*hint;

	// we not known about filetype, so match only by filename
	for( hint = img_hints; hint->ext; hint++ )
	{
		if( hint->type == value )
			return hint;
	}

	return NULL;
}

void Image_PackRGB( float flColor[3], uint32_t &icolor )
{
	byte	rgba[4];
	
	rgba[0] = LinearToTexture( flColor[0] );
	rgba[1] = LinearToTexture( flColor[1] );
	rgba[2] = LinearToTexture( flColor[2] );

	icolor = (0xFF << 24) | (rgba[2] << 16) | (rgba[1] << 8) | rgba[0];
}

void Image_UnpackRGB( uint32_t icolor, float flColor[3] )
{
	flColor[0] = TextureToLinear((icolor & 0x000000FF) >> 0 );
	flColor[1] = TextureToLinear((icolor & 0x0000FF00) >> 8 );
	flColor[2] = TextureToLinear((icolor & 0x00FF0000) >> 16);
}

/*
=============================================================================

	IMAGE LOADING

=============================================================================
*/
/*
=============
Image_LoadTGA

expand any image to RGBA32 but keep 8-bit unchanged
=============
*/
rgbdata_t *Image_LoadTGA( const char *name, const byte *buffer, size_t filesize )
{
	int	i, columns, rows, row_inc, row, col;
	byte	*buf_p, *pixbuf, *targa_rgba;
	byte	palette[256][4], red = 0, green = 0, blue = 0, alpha = 0;
	int	readpixelcount, pixelcount, palIndex;
	tga_t	targa_header;
	bool	compressed;
	rgbdata_t *pic;

	if (filesize < sizeof(tga_t)) {
		MsgDev( D_ERROR, "Image_LoadTGA: file header validation failed (%s)\n", name );
		return NULL;
	}

	buf_p = (byte *)buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = *(short *)buf_p;	buf_p += 2;
	targa_header.colormap_length = *(short *)buf_p;	buf_p += 2;
	targa_header.colormap_size = *buf_p;		buf_p += 1;
	targa_header.x_origin = *(short *)buf_p;	buf_p += 2;
	targa_header.y_origin = *(short *)buf_p;	buf_p += 2;
	targa_header.width = *(short *)buf_p;		buf_p += 2;
	targa_header.height = *(short *)buf_p;		buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;
	if( targa_header.id_length != 0 )
		buf_p += targa_header.id_length; // skip TARGA image comment

	// check for tga file
	if (!Image_ValidSize(name, targa_header.width, targa_header.height)) {
		MsgDev( D_ERROR, "Image_LoadTGA: invalid file size (%s)\n", name );
		return NULL;
	}

	if( targa_header.image_type == 1 || targa_header.image_type == 9 )
	{
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only 8 bit images supported for type 1 and 9 (%s)\n", name );
			return NULL;
		}

		if( targa_header.colormap_length != 256 )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only 8 bit colormaps are supported for type 1 and 9 (%s)\n", name );
			return NULL;
		}

		if( targa_header.colormap_index )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: colormap_index is not supported for type 1 and 9 (%s)\n", name );
			return NULL;
		}

		if( targa_header.colormap_size == 24 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
		}
		else if( targa_header.colormap_size == 32 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		}
		else
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only 24 and 32 bit colormaps are supported for type 1 and 9 (%s)\n", name );
			return NULL;
		}
	}
	else if( targa_header.image_type == 2 || targa_header.image_type == 10 )
	{
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only 32 or 24 bit images supported for type 2 and 10 (%s)\n", name );
			return NULL;
		}
	}
	else if( targa_header.image_type == 3 || targa_header.image_type == 11 )
	{
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_ERROR, "Image_LoadTGA: only 8 bit images supported for type 3 and 11 (%s)\n", name );
			return NULL;
		}
	}

	bool paletted = (targa_header.image_type == 1 || targa_header.image_type == 9);
	pic = Image_Alloc(targa_header.width, targa_header.height, paletted);
	if (paletted) {
		memcpy(pic->palette, palette, sizeof(palette));
	}

	columns = targa_header.width;
	rows = targa_header.height;
	targa_rgba = pic->buffer;

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if( targa_header.attributes & 0x20 )
	{
		pixbuf = targa_rgba;
		row_inc = 0;
	}
	else
	{
		if (FBitSet(pic->flags, IMAGE_QUANTIZED))
		{
			pixbuf = targa_rgba + (rows - 1) * columns;
			row_inc = -columns * 2;
		}
		else
		{
			pixbuf = targa_rgba + (rows - 1) * columns * 4;
			row_inc = -columns * 2 * 4;
		}
	}

	compressed = ( targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11 );

	for( row = col = 0; row < rows; )
	{
		pixelcount = 0x10000;
		readpixelcount = 0x10000;

		if( compressed )
		{
			pixelcount = *buf_p++;
			if( pixelcount & 0x80 )  // run-length packet
				readpixelcount = 1;
			pixelcount = 1 + ( pixelcount & 0x7f );
		}

		while( pixelcount-- && ( row < rows ) )
		{
			if( readpixelcount-- > 0 )
			{
				switch( targa_header.image_type )
				{
				case 1:
				case 9:
					// colormapped image
					palIndex = *buf_p++;
					red = palette[palIndex][0];
					green = palette[palIndex][1];
					blue = palette[palIndex][2];
					alpha = palette[palIndex][3];
					break;
				case 2:
				case 10:
					// 24 or 32 bit image
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alpha = 255;
					if( targa_header.pixel_size == 32 )
						alpha = *buf_p++;
					break;
				case 3:
				case 11:
					// greyscale image
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				}
			}

			if( red != green || green != blue )
				pic->flags |= IMAGE_HAS_COLOR;

			if( alpha != 255 )
			{
				if( alpha != 0 )
				{
					SetBits( pic->flags, IMAGE_HAS_8BIT_ALPHA );
					ClearBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
				}
				else if( !FBitSet( pic->flags, IMAGE_HAS_8BIT_ALPHA ))
					SetBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
			}

			pic->reflectivity[0] += TextureToLinear( red );
			pic->reflectivity[1] += TextureToLinear( green );
			pic->reflectivity[2] += TextureToLinear( blue );

			if (FBitSet(pic->flags, IMAGE_QUANTIZED))
			{
				*pixbuf++ = palIndex;
			}
			else
			{
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
			}

			if( ++col == columns )
			{
				// run spans across rows
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}

	VectorDivide( pic->reflectivity, ( pic->width * pic->height ), pic->reflectivity );

	return pic;
}

/*
=============
Image_LoadBMP

expand any image to RGBA32 but keep 8-bit unchanged
=============
*/
rgbdata_t *Image_LoadBMP( const char *name, const byte *buffer, size_t filesize )
{
	byte	*buf_p, *pixbuf;
	byte	palette[256][4];
	int	columns, column, rows, row, bpp = 1;
	size_t	cbPalBytes = 0, padSize = 0, bps = 0;
	rgbdata_t *pic;
	bmp_t	bhdr;

	if (filesize < sizeof(bhdr)) {
		MsgDev( D_ERROR, "Image_LoadBMP: invalid file size (%s)\n", name );
		return NULL;
	}

	buf_p = (byte *)buffer;
	memcpy(&bhdr, buf_p, sizeof(bmp_t));
	buf_p += 14 + bhdr.bitmapHeaderSize;

	// bogus file header check
	if (bhdr.reserved0 != 0 || bhdr.planes != 1) {
		MsgDev( D_ERROR, "Image_LoadBMP: bogus file header (%s)\n", name );
		return NULL;
	}

	if( memcmp( bhdr.id, "BM", 2 ))
	{
		MsgDev( D_ERROR, "Image_LoadBMP: only Windows-style BMP files supported (%s)\n", name );
		return NULL;
	} 

	if (bhdr.bitmapHeaderSize != 40 && bhdr.bitmapHeaderSize != 108 && bhdr.bitmapHeaderSize != 124)
	{
		MsgDev(D_ERROR, "Image_LoadBMP: invalid header size (%i)\n", bhdr.bitmapHeaderSize);
		return NULL;
	}

	// bogus info header check
	if( bhdr.fileSize != filesize )
	{
		// Sweet Half-Life issues. splash.bmp have bogus filesize
		MsgDev( D_WARN, "Image_LoadBMP: %s have incorrect file size %i should be %i\n", name, filesize, bhdr.fileSize );
    }
          
	// bogus compression?  Only non-compressed supported.
	if( bhdr.compression != BI_RGB ) 
	{
		MsgDev( D_ERROR, "Image_LoadBMP: only uncompressed BMP files supported (%s)\n", name );
		return NULL;
	}

	columns = bhdr.width;
	rows = abs( bhdr.height );

	if( !Image_ValidSize( name, columns, rows ))
		return NULL;          

	pic = Image_Alloc( columns, rows, (bhdr.bitsPerPixel == 4 || bhdr.bitsPerPixel == 8));

	if (bhdr.bitsPerPixel <= 8)
	{
		// figure out how many entries are actually in the table
		if( bhdr.colors == 0 )
		{
			bhdr.colors = 256;
			cbPalBytes = static_cast<size_t>(powf(2, bhdr.bitsPerPixel)) * sizeof(RGBQUAD);
		}
		else cbPalBytes = bhdr.colors * sizeof( RGBQUAD );
	}

	memcpy( palette, buf_p, cbPalBytes );

	if (bhdr.bitsPerPixel == 4 || bhdr.bitsPerPixel == 8)
	{
		pixbuf = pic->palette;

		// bmp have a reversed palette colors
		for (int i = 0; i < bhdr.colors; i++)
		{
			*pixbuf++ = palette[i][2];
			*pixbuf++ = palette[i][1];
			*pixbuf++ = palette[i][0];
			*pixbuf++ = palette[i][3];

			if (palette[i][0] != palette[i][1] || palette[i][1] != palette[i][2])
				pic->flags |= IMAGE_HAS_COLOR;
		}
	}
	else {
		bpp = 4;
	}

	buf_p += cbPalBytes;
	bps = bhdr.width * (bhdr.bitsPerPixel >> 3);

	switch( bhdr.bitsPerPixel )
	{
	case 1:
		padSize = (( 32 - ( bhdr.width % 32 )) / 8 ) % 4;
		break;
	case 4:
		padSize = (( 8 - ( bhdr.width % 8 )) / 2 ) % 4;
		break;
	case 16:
		padSize = ( 4 - ( bhdr.width * 2 % 4 )) % 4;
		break;
	case 8:
	case 24:
		padSize = ( 4 - ( bps % 4 )) % 4;
		break;
	}

	for( row = rows - 1; row >= 0; row-- )
	{
		pixbuf = pic->buffer + (row * columns * bpp);

		for( column = 0; column < columns; column++ )
		{
			byte	red, green, blue, alpha;
			int	c, k, palIndex;
			word	shortPixel;

			switch( bhdr.bitsPerPixel )
			{
			case 1:
				alpha = *buf_p++;
				column--;	// ingnore main iterations
				for( c = 0, k = 128; c < 8; c++, k >>= 1 )
				{
					red = green = blue = (!!(alpha & k) == 1 ? 0xFF : 0x00);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 0x00;
					if( ++column == columns )
						break;
				}
				break;
			case 4:
				alpha = *buf_p++;
				palIndex = alpha >> 4;
				red = palette[palIndex][2];
				green = palette[palIndex][1];
				blue = palette[palIndex][0];
				alpha = palette[palIndex][3];

				if (FBitSet(pic->flags, IMAGE_QUANTIZED))
				{
					*pixbuf++ = palIndex;
				}
				else
				{
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alpha;
				}

				if (++column == columns)
					break;

				palIndex = alpha & 0x0F;
				red = palette[palIndex][2];
				green = palette[palIndex][1];
				blue = palette[palIndex][0];
				alpha = palette[palIndex][3];

				if (FBitSet(pic->flags, IMAGE_QUANTIZED))
				{
					*pixbuf++ = palIndex;
				}
				else
				{
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alpha;
				}
				break;
			case 8:
				palIndex = *buf_p++;
				red = palette[palIndex][2];
				green = palette[palIndex][1];
				blue = palette[palIndex][0];
				alpha = palette[palIndex][3];

				if (FBitSet(pic->flags, IMAGE_QUANTIZED))
				{
					*pixbuf++ = palIndex;
				}
				else
				{
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alpha;
				}
				break;
			case 16:
				shortPixel = *(word *)buf_p, buf_p += 2;
				*pixbuf++ = blue = (shortPixel & ( 31 << 10 )) >> 7;
				*pixbuf++ = green = (shortPixel & ( 31 << 5 )) >> 2;
				*pixbuf++ = red = (shortPixel & ( 31 )) << 3;
				*pixbuf++ = alpha = 0xff;
				break;
			case 24:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha = 0xFF;
				break;
			case 32:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				alpha = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
				break;
			default:
				MsgDev( D_ERROR, "Image_LoadBMP: illegal pixel_size (%s)\n", name );
				Image_Free( pic );
				return NULL;
			}

			pic->reflectivity[0] += TextureToLinear( red );
			pic->reflectivity[1] += TextureToLinear( green );
			pic->reflectivity[2] += TextureToLinear( blue );

			if (!FBitSet(pic->flags, IMAGE_QUANTIZED) && (red != green) || (green != blue))
				pic->flags |= IMAGE_HAS_COLOR;

			if( alpha != 255 )
			{
				if( alpha != 0 )
				{
					SetBits( pic->flags, IMAGE_HAS_8BIT_ALPHA );
					ClearBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
				}
				else if( !FBitSet( pic->flags, IMAGE_HAS_8BIT_ALPHA ))
					SetBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
			}
		}

		buf_p += padSize;	// probably actual only for 4-bit bmps
	}

	VectorDivide( pic->reflectivity, ( pic->width * pic->height ), pic->reflectivity );

	return pic;
}

/*
=============
Image_LoadDDS
=============
*/
rgbdata_t *Image_LoadDDS( const char *name, const byte *buffer, size_t filesize )
{
	return DDSToRGBA( name, buffer, filesize );
}

/*
=============
Image_LoadMIP

just read header
=============
*/
rgbdata_t *Image_LoadMIP( const char *name, const byte *buffer, size_t filesize )
{
	char tmpname[64];

	if (filesize < sizeof(mip_t))
		return NULL;

	mip_t *mip = (mip_t *)buffer;

	if (!Image_ValidSize(name, mip->width, mip->height))
		return NULL;

	int pixels = mip->width * mip->height;

	if (filesize < (sizeof(mip_t) + ((pixels * 85) >> 6) + sizeof(short) + 768))
	{
		MsgDev(D_ERROR, "Image_LoadMIP: %s probably corrupted\n", name);
		return NULL;
	}

	rgbdata_t *pic = Image_Alloc(mip->width, mip->height, true);
	memcpy(pic->buffer, buffer + mip->offsets[0], pixels);
	uint8_t *pal = (uint8_t *)buffer + mip->offsets[0] + (((mip->width * mip->height) * 85) >> 6);

	int numcolors = *(int16_t *)pal;
	if (numcolors != 256) {
		MsgDev(D_WARN, "Image_LoadMIP: %s invalid palette num colors %i\n", name, numcolors);
	}
	pal += sizeof(int16_t); // skip colorsize 

	// expand palette
	for (int i = 0; i < 256; i++)
	{
		pic->palette[i * 4 + 0] = *pal++;
		pic->palette[i * 4 + 1] = *pal++;
		pic->palette[i * 4 + 2] = *pal++;
		pic->palette[i * 4 + 3] = 255;

		if (pic->palette[i * 4 + 0] != pic->palette[i * 4 + 1] || pic->palette[i * 4 + 1] != pic->palette[i * 4 + 2])
			pic->flags |= IMAGE_HAS_COLOR;
	}

	// check for one-bit alpha
	COM_FileBase(name, tmpname);

	if (tmpname[0] == '{' && pic->palette[255 * 3 + 0] == 0 && pic->palette[255 * 3 + 1] == 0 && pic->palette[255 * 3 + 2] == 255)
		pic->flags |= IMAGE_HAS_1BIT_ALPHA;

	return pic;
}

/*
=============
Image_LoadLMP

just read header
=============
*/
rgbdata_t *Image_LoadLMP( const char *name, const byte *buffer, size_t filesize )
{
	if (filesize < sizeof(lmp_t))
		return NULL;

	lmp_t *lmp = (lmp_t *)buffer;

	if (!Image_ValidSize(name, lmp->width, lmp->height))
		return NULL;

	int pixels = lmp->width * lmp->height;

	if (filesize < (sizeof(lmp_t) + pixels + sizeof(short) + 768))
	{
		MsgDev(D_ERROR, "Image_LoadLMP: %s probably corrupted\n", name);
		return NULL;
	}

	rgbdata_t *pic = Image_Alloc(lmp->width, lmp->height, true);
	memcpy(pic->buffer, buffer + sizeof(lmp_t), pixels);
	uint8_t *pal = (uint8_t *)buffer + sizeof(lmp_t) + pixels;

	int numcolors = *(int16_t *)pal;
	if (numcolors != 256) {
		MsgDev(D_WARN, "Image_LoadLMP: %s invalid palette num colors %i\n", name, numcolors);
	}
	pal += sizeof(int16_t); // skip colorsize 

	// expand palette
	for (int i = 0; i < 256; i++)
	{
		pic->palette[i * 4 + 0] = *pal++;
		pic->palette[i * 4 + 1] = *pal++;
		pic->palette[i * 4 + 2] = *pal++;
		pic->palette[i * 4 + 3] = 255;

		if (pic->palette[i * 4 + 0] != pic->palette[i * 4 + 1] || pic->palette[i * 4 + 1] != pic->palette[i * 4 + 2])
			pic->flags |= IMAGE_HAS_COLOR;
	}

	// always has the alpha
	pic->flags |= IMAGE_HAS_1BIT_ALPHA;

	return pic;
}

/*
=============
Image_LoadPNG
=============
*/
rgbdata_t *Image_LoadPNG( const char *name, const byte *buffer, size_t filesize )
{
	int		ret;
	short		p, a, b, c, pa, pb, pc;
	byte		*buf_p, *pixbuf, *raw, *prior, *idat_buf = NULL, *uncompressed_buffer = NULL;
	byte		*pallete = NULL, *trns = NULL;
	uint	 	chunk_len, trns_len, plte_len, crc32, crc32_check, oldsize = 0, newsize = 0, rowsize;
	uint		uncompressed_size, pixel_size, pixel_count, i, y, filter_type, chunk_sign, r_alpha, g_alpha, b_alpha;
	qboolean 	has_iend_chunk = false;
	z_stream 	stream = {0};
	png_t		png_hdr;
	rgbdata_t	*pic;

	if (filesize < sizeof(png_hdr))
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Invalid file size (%s)\n", name );
		return nullptr;
	}

	buf_p = (byte *)buffer;

	// get png header
	memcpy( &png_hdr, buffer, sizeof( png_t ) );

	// check png signature
	if( memcmp( png_hdr.sign, png_sign, sizeof( png_sign ) ) )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Invalid PNG signature (%s)\n", name );
		return nullptr;
	}

	// convert IHDR chunk length to little endian
	png_hdr.ihdr_len = ntohl( png_hdr.ihdr_len );

	// check IHDR chunk length (valid value - 13)
	if( png_hdr.ihdr_len != sizeof( png_ihdr_t ) )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Invalid IHDR chunk size %u (%s)\n", png_hdr.ihdr_len, name );
		return nullptr;
	}

	// check IHDR chunk signature
	if( memcmp( png_hdr.ihdr_sign, ihdr_sign, sizeof( ihdr_sign ) ) )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: IHDR chunk corrupted (%s)\n", name );
		return nullptr;
	}

	// convert image width and height to little endian
	uint32_t height = png_hdr.ihdr_chunk.height = ntohl( png_hdr.ihdr_chunk.height );
	uint32_t width  = png_hdr.ihdr_chunk.width  = ntohl( png_hdr.ihdr_chunk.width );

	if( png_hdr.ihdr_chunk.height == 0 || png_hdr.ihdr_chunk.width == 0 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Invalid image size %ux%u (%s)\n", png_hdr.ihdr_chunk.width, png_hdr.ihdr_chunk.height, name );
		return nullptr;
	}

	if( !Image_ValidSize( name, width, height ))
		return nullptr;

	pic = Image_Alloc( width, height );
	if( png_hdr.ihdr_chunk.bitdepth != 8 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Only 8-bit images is supported (%s)\n", name );
		return nullptr;
	}

	if( !( png_hdr.ihdr_chunk.colortype == PNG_CT_RGB
	    || png_hdr.ihdr_chunk.colortype == PNG_CT_RGBA
	    || png_hdr.ihdr_chunk.colortype == PNG_CT_GREY
	    || png_hdr.ihdr_chunk.colortype == PNG_CT_ALPHA
	    || png_hdr.ihdr_chunk.colortype == PNG_CT_PALLETE ) )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Unknown color type %u (%s)\n", png_hdr.ihdr_chunk.colortype, name );
		return nullptr;
	}

	if( png_hdr.ihdr_chunk.compression > 0 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Unknown compression method %u (%s)\n", png_hdr.ihdr_chunk.compression, name );
		return nullptr;
	}

	if( png_hdr.ihdr_chunk.filter > 0 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Unknown filter type %u (%s)\n", png_hdr.ihdr_chunk.filter, name );
		return nullptr;
	}

	if( png_hdr.ihdr_chunk.interlace == 1 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Adam7 Interlacing not supported (%s)\n", name );
		return nullptr;
	}

	if( png_hdr.ihdr_chunk.interlace > 0 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Unknown interlacing type %u (%s)\n", png_hdr.ihdr_chunk.interlace, name );
		return nullptr;
	}

	// calculate IHDR chunk CRC
	CRC32_Init( &crc32_check );
	CRC32_ProcessBuffer( &crc32_check, buf_p + sizeof( png_hdr.sign ) + sizeof( png_hdr.ihdr_len ), png_hdr.ihdr_len + sizeof( png_hdr.ihdr_sign ) );
	crc32_check = CRC32_Final( crc32_check );

	// check IHDR chunk CRC
	if( ntohl( png_hdr.ihdr_crc32 ) != crc32_check )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: IHDR chunk has wrong CRC32 sum (%s)\n", name );
		return nullptr;
	}

	// move pointer
	buf_p += sizeof( png_hdr );

	// find all critical chunks
	while( !has_iend_chunk && ( buf_p - buffer ) < filesize )
	{
		// get chunk length
		memcpy( &chunk_len, buf_p, sizeof( chunk_len ) );

		// convert chunk length to little endian
		chunk_len = ntohl( chunk_len );

		if( chunk_len > INT_MAX )
		{
			MsgDev( D_ERROR, "Image_LoadPNG: Found chunk with wrong size (%s)\n", name );
			if( idat_buf ) Mem_Free( idat_buf );
			return nullptr;
		}

		if( chunk_len > filesize - ( buf_p - buffer ))
		{
			MsgDev( D_ERROR, "Image_LoadPNG: Found chunk with size past file size (%s)\n", name );
			if( idat_buf ) Mem_Free( idat_buf );
			return nullptr;
		}

		// move pointer
		buf_p += sizeof( chunk_len );

		// find transparency
		if( !memcmp( buf_p, trns_sign, sizeof( trns_sign ) ) )
		{
			trns = buf_p + sizeof( trns_sign );
			trns_len = chunk_len;
		}
		// find pallete for indexed image
		else if( !memcmp( buf_p, plte_sign, sizeof( plte_sign ) ) )
		{
			pallete = buf_p + sizeof( plte_sign );
			plte_len = chunk_len / 3;
		}
		// get all IDAT chunks data
		else if( !memcmp( buf_p, idat_sign, sizeof( idat_sign ) ) )
		{
			newsize = oldsize + chunk_len;
			idat_buf = (byte *)Mem_Realloc( idat_buf, newsize );
			memcpy( idat_buf + oldsize, buf_p + sizeof( idat_sign ), chunk_len );
			oldsize = newsize;
		}
		else if( !memcmp( buf_p, iend_sign, sizeof( iend_sign ) ) )
			has_iend_chunk = true;

		// calculate chunk CRC
		CRC32_Init( &crc32_check );
		CRC32_ProcessBuffer( &crc32_check, buf_p, chunk_len + sizeof( idat_sign ) );
		crc32_check = CRC32_Final( crc32_check );

		// move pointer
		buf_p += sizeof( chunk_sign );
		buf_p += chunk_len;

		// get real chunk CRC
		memcpy( &crc32, buf_p, sizeof( crc32 ) );

		// check chunk CRC
		if( ntohl( crc32 ) != crc32_check )
		{
			MsgDev( D_ERROR, "Image_LoadPNG: Found chunk with wrong CRC32 sum (%s)\n", name );
			if( idat_buf ) Mem_Free( idat_buf );
			return nullptr;
		}

		// move pointer
		buf_p += sizeof( crc32 );
	}

	if( oldsize == 0 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: Couldn't find IDAT chunks (%s)\n", name );
		return nullptr;
	}

	if( png_hdr.ihdr_chunk.colortype == PNG_CT_PALLETE && !pallete )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: PLTE chunk not found (%s)\n", name );
		Mem_Free( idat_buf );
		return nullptr;
	}

	if( !has_iend_chunk )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: IEND chunk not found (%s)\n", name );
		Mem_Free( idat_buf );
		return nullptr;
	}

	if( chunk_len != 0 )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: IEND chunk has wrong size %u (%s)\n", chunk_len, name );
		Mem_Free( idat_buf );
		return nullptr;
	}

	switch( png_hdr.ihdr_chunk.colortype )
	{
	case PNG_CT_GREY:
	case PNG_CT_PALLETE:
		pixel_size = 1;
		break;
	case PNG_CT_ALPHA:
		pixel_size = 2;
		break;
	case PNG_CT_RGB:
		pixel_size = 3;
		break;
	case PNG_CT_RGBA:
		pixel_size = 4;
		break;
	default:
		pixel_size = 0; // make compiler happy
		ASSERT( false );
		break;
	}

	pixel_count = pic->height * pic->width;
	pic->size = pixel_count * 4;

	if( png_hdr.ihdr_chunk.colortype & PNG_CT_RGB )
		pic->flags |= IMAGE_HAS_COLOR;

	if( trns || ( png_hdr.ihdr_chunk.colortype & PNG_CT_ALPHA ) )
		pic->flags |= IMAGE_HAS_ALPHA;

	rowsize = pixel_size * pic->width;
	uncompressed_size = pic->height * ( rowsize + 1 ); // +1 for filter
	uncompressed_buffer = reinterpret_cast<byte*>(Mem_Alloc(uncompressed_size));

	stream.next_in = idat_buf;
	stream.total_in = stream.avail_in = newsize;
	stream.next_out = uncompressed_buffer;
	stream.total_out = stream.avail_out = uncompressed_size;

	// uncompress image
	if( inflateInit2( &stream, MAX_WBITS ) != Z_OK )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: IDAT chunk decompression failed (%s)\n", name );
		Mem_Free( uncompressed_buffer );
		Mem_Free( idat_buf );
		return nullptr;
	}

	ret = inflate( &stream, Z_NO_FLUSH );
	inflateEnd( &stream );

	Mem_Free( idat_buf );

	if( ret != Z_OK && ret != Z_STREAM_END )
	{
		MsgDev( D_ERROR, "Image_LoadPNG: IDAT chunk decompression failed (%s)\n", name );
		Mem_Free( uncompressed_buffer );
		return nullptr;
	}

	prior = pixbuf = pic->buffer = reinterpret_cast<byte*>(Mem_Alloc(pic->size));
	i = 0;
	raw = uncompressed_buffer;

	if( png_hdr.ihdr_chunk.colortype != PNG_CT_RGBA )
		prior = pixbuf = raw;

	filter_type = *raw++;

	// decode adaptive filter
	switch( filter_type )
	{
	case PNG_F_NONE:
	case PNG_F_UP:
		for( ; i < rowsize; i++ )
			pixbuf[i] = raw[i];
		break;
	case PNG_F_SUB:
	case PNG_F_PAETH:
		for( ; i < pixel_size; i++ )
			pixbuf[i] = raw[i];

		for( ; i < rowsize; i++ )
			pixbuf[i] = raw[i] + pixbuf[i - pixel_size];
		break;
	case PNG_F_AVERAGE:
		for( ; i < pixel_size; i++ )
			pixbuf[i] = raw[i];

		for( ; i < rowsize; i++ )
			pixbuf[i] = raw[i] + ( pixbuf[i - pixel_size] >> 1 );
		break;
	default:
		MsgDev( D_ERROR, "Image_LoadPNG: Found unknown filter type (%s)\n", name );
		Mem_Free( uncompressed_buffer );
		Image_Free( pic );
		return nullptr;
	}

	for( y = 1; y < pic->height; y++ )
	{
		i = 0;

		pixbuf += rowsize;
		raw += rowsize;

		filter_type = *raw++;

		switch( filter_type )
		{
		case PNG_F_NONE:
			for( ; i < rowsize; i++ )
				pixbuf[i] = raw[i];
			break;
		case PNG_F_SUB:
			for( ; i < pixel_size; i++ )
				pixbuf[i] = raw[i];

			for( ; i < rowsize; i++ )
				pixbuf[i] = raw[i] + pixbuf[i - pixel_size];
			break;
		case PNG_F_UP:
			for( ; i < rowsize; i++ )
				pixbuf[i] = raw[i] + prior[i];
			break;
		case PNG_F_AVERAGE:
			for( ; i < pixel_size; i++ )
				pixbuf[i] = raw[i] + ( prior[i] >> 1 );

			for( ; i < rowsize; i++ )
				pixbuf[i] = raw[i] + ( ( pixbuf[i - pixel_size] + prior[i] ) >> 1 );
			break;
		case PNG_F_PAETH:
			for( ; i < pixel_size; i++ )
				pixbuf[i] = raw[i] + prior[i];

			for( ; i < rowsize; i++ )
			{
				a = pixbuf[i - pixel_size];
				b = prior[i];
				c = prior[i - pixel_size];
				p = a + b - c;
				pa = abs( p - a );
				pb = abs( p - b );
				pc = abs( p - c );

				pixbuf[i] = raw[i];

				if( pc < pa && pc < pb )
					pixbuf[i] += c;
				else if( pb < pa )
					pixbuf[i] += b;
				else
					pixbuf[i] += a;
			}
			break;
		default:
			MsgDev( D_ERROR, "Image_LoadPNG: Found unknown filter type (%s)\n", name );
			Mem_Free( uncompressed_buffer );
			Image_Free( pic );
			return nullptr;
		}

		prior = pixbuf;
	}

	pixbuf = pic->buffer;
	raw = uncompressed_buffer;

	switch( png_hdr.ihdr_chunk.colortype )
	{
	case PNG_CT_RGB:
		if( trns )
		{
			r_alpha = trns[0] << 8 | trns[1];
			g_alpha = trns[2] << 8 | trns[3];
			b_alpha = trns[4] << 8 | trns[5];
		}

		for( y = 0; y < pixel_count; y++, raw += pixel_size )
		{
			*pixbuf++ = raw[0];
			*pixbuf++ = raw[1];
			*pixbuf++ = raw[2];

			if( trns && r_alpha == raw[0]
			    && g_alpha == raw[1]
			    && b_alpha == raw[2] )
				*pixbuf++ = 0;
			else
				*pixbuf++ = 0xFF;
		}
		break;
	case PNG_CT_GREY:
		if( trns )
			r_alpha = trns[0] << 8 | trns[1];

		for( y = 0; y < pixel_count; y++, raw += pixel_size )
		{
			*pixbuf++ = raw[0];
			*pixbuf++ = raw[0];
			*pixbuf++ = raw[0];

			if( trns && r_alpha == raw[0] )
				*pixbuf++ = 0;
			else
				*pixbuf++ = 0xFF;
		}
		break;
	case PNG_CT_ALPHA:
		for( y = 0; y < pixel_count; y++, raw += pixel_size )
		{
			*pixbuf++ = raw[0];
			*pixbuf++ = raw[0];
			*pixbuf++ = raw[0];
			*pixbuf++ = raw[1];
		}
		break;
	case PNG_CT_PALLETE:
		for( y = 0; y < pixel_count; y++, raw += pixel_size )
		{
			if( raw[0] < plte_len )
			{
				*pixbuf++ = pallete[3 * raw[0] + 0];
				*pixbuf++ = pallete[3 * raw[0] + 1];
				*pixbuf++ = pallete[3 * raw[0] + 2];

				if( trns && raw[0] < trns_len )
					*pixbuf++ = trns[raw[0]];
				else
					*pixbuf++ = 0xFF;
			}
			else
			{
				*pixbuf++ = 0;
				*pixbuf++ = 0;
				*pixbuf++ = 0;
				*pixbuf++ = 0xFF;
			}
		}
		break;
	default:
		break;
	}

	Mem_Free( uncompressed_buffer );
	return pic;
}

/*
================
COM_LoadImage

handle bmp & tga
================
*/
rgbdata_t *COM_LoadImage( const char *filename, bool quiet )
{
	const char	*ext = COM_FileExtension( filename );
	char		path[128], loadname[128];
	bool		anyformat = true;
	size_t		filesize = 0;
	const loadimage_t	*format;
	rgbdata_t		*image;
	byte		*buf;

	Q_strncpy( loadname, filename, sizeof( loadname ));

	if( Q_stricmp( ext, "" ))
	{
		// we needs to compare file extension with list of supported formats
		// and be sure what is real extension, not a filename with dot
		for( format = load_hint; format && format->formatstring; format++ )
		{
			if( !Q_stricmp( format->ext, ext ))
			{
				COM_StripExtension( loadname );
				anyformat = false;
				break;
			}
		}
	}

	// now try all the formats in the selected list
	for( format = load_hint; format && format->formatstring; format++ )
	{
		if( anyformat || !Q_stricmp( ext, format->ext ))
		{
			Q_sprintf( path, format->formatstring, loadname, "", format->ext );
#ifdef ALLOW_WADS_IN_PACKS
			buf = FS_LoadFile( path, &filesize, false );
#else
			buf = COM_LoadFile( path, &filesize );
#endif
			if( buf && filesize > 0 )
			{
				image = format->loadfunc( path, buf, filesize );
				Mem_Free( buf ); // release buffer
				if( image ) return image; // loaded
			}
		}
	}

	if( !quiet )
		MsgDev( D_ERROR, "COM_LoadImage: couldn't load \"%s\"\n", loadname );
	return NULL;
}

/*
=============================================================================

	IMAGE SAVING

=============================================================================
*/
/*
=============
Image_SaveTGA
=============
*/
bool Image_SaveTGA( const char *name, rgbdata_t *pix )
{
	const char	*comment = "Generated by PrimeXT Image Library\0";
	int	y, outsize, pixel_size = 4;
	const byte *bufend, *in;
	byte *buffer, *out;

	if (COM_FileExists(name)) {
		MsgDev( D_ERROR, "Image_SaveTGA: file already exists (%s)\n", name );
		return false; // already existed
	}

	// bogus parameter check
	if (!pix->buffer) {
		MsgDev( D_ERROR, "Image_SaveTGA: invalid image buffer (%s)\n", name );
		return false;
	}

	if (FBitSet(pix->flags, IMAGE_QUANTIZED)) {
		outsize = pix->width * pix->height + 18 + Q_strlen(comment);
	}
	else
	{
		if (pix->flags & IMAGE_HAS_ALPHA)
			outsize = pix->width * pix->height * 4 + 18 + Q_strlen(comment);
		else
			outsize = pix->width * pix->height * 3 + 18 + Q_strlen(comment);
	}

	if (FBitSet(pix->flags, IMAGE_QUANTIZED)) {
		outsize += 768;	// reserve bytes for palette
	}

	buffer = (byte *)Mem_Alloc( outsize );
	memset(buffer, 0, sizeof(tga_t));
	tga_t *header = reinterpret_cast<tga_t*>(buffer);

	// prepare header
	buffer[0] = Q_strlen( comment ); // tga comment length

	if (FBitSet(pix->flags, IMAGE_QUANTIZED))
	{
		buffer[1] = 1;	// color map type
		buffer[2] = 1;	// uncompressed color mapped image
		buffer[5] = 0;	// number of palette entries (lo)
		buffer[6] = 1;	// number of palette entries (hi)
		buffer[7] = 24;	// palette bpp
	}
	else {
		buffer[2] = 2; // uncompressed type
	}

	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;

	if (!FBitSet(pix->flags, IMAGE_QUANTIZED))
	{
		buffer[16] = (pix->flags & IMAGE_HAS_ALPHA) ? 32 : 24; // RGB pixel size
		buffer[17] = (pix->flags & IMAGE_HAS_ALPHA) ? 8 : 0; // 8 bits of alpha
	}
	else {
		buffer[16] = 8; // pixel size
		buffer[17] = 0; // ???
	}

	Q_strncpy( (char *)(buffer + 18), comment, Q_strlen( comment )); 
	out = buffer + 18 + Q_strlen( comment );

	// store palette, swapping rgb to bgr
	if (FBitSet(pix->flags, IMAGE_QUANTIZED))
	{
		for (int i = 0; i < 256; i++)
		{
			*out++ = pix->palette[i * 4 + 2];	// blue
			*out++ = pix->palette[i * 4 + 1];	// green
			*out++ = pix->palette[i * 4 + 0];	// red
		}

		// store the image data (and flip upside down)
		for (y = pix->height - 1; y >= 0; y--)
		{
			in = pix->buffer + y * pix->width;
			bufend = in + pix->width;
			for (; in < bufend; in++)
				*out++ = *in;
		}
	}
	else
	{
		// swap rgba to bgra and flip upside down
		for (y = pix->height - 1; y >= 0; y--)
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for (; in < bufend; in += pixel_size)
			{
				*out++ = in[2];
				*out++ = in[1];
				*out++ = in[0];
				if (pix->flags & IMAGE_HAS_ALPHA)
					*out++ = in[3];
			}
		}
	}

	COM_SaveFile( name, buffer, outsize );
	Mem_Free( buffer );

	return true;
}

/*
=============
Image_SaveBMP
=============
*/
bool Image_SaveBMP( const char *name, rgbdata_t *pix )
{
	bmp_t		fileHeader;
	size_t		pixelSize;
	size_t		paletteBytes;
	RGBQUAD		rgrgbPalette[256];

	if (COM_FileExists(name)) {
		MsgDev( D_ERROR, "Image_SaveBMP: file already exists (%s)\n", name );
		return false; // already existed
	}

	// bogus parameter check
	if (!pix->buffer) {
		MsgDev( D_ERROR, "Image_SaveBMP: invalid image buffer (%s)\n", name );
		return false;
	}

	if (FBitSet(pix->flags, IMAGE_QUANTIZED)) {
		pixelSize = 1;
	}
	else 
	{
		if (FBitSet(pix->flags, IMAGE_HAS_ALPHA))
			pixelSize = 4;
		else 
			pixelSize = 3;
	}

	COM_CreatePath((char *)name);
	int file = open(name, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0666);
	if (file < 0) {
		MsgDev( D_ERROR, "Image_SaveBMP: failed to open file (%s)\n", name );
		return false; // failed to open file
	}

	// NOTE: align transparency column will successfully removed
	// after create sprite or lump image, it's just standard requirements 
	size_t biTrueWidth = ((pix->width + 3) & ~3);
	size_t cbBmpBits = biTrueWidth * pix->height * pixelSize;
	if (pixelSize == 1) {
		paletteBytes = 256 * sizeof(RGBQUAD);
	}
	else {
		paletteBytes = 0;
	}

	// write header to file
	fileHeader.id[0] = 'B';
	fileHeader.id[1] = 'M';
	fileHeader.fileSize = sizeof(fileHeader) + cbBmpBits + paletteBytes;
	fileHeader.reserved0 = 0;
	fileHeader.bitmapDataOffset = sizeof(fileHeader) + paletteBytes;
	fileHeader.bitmapHeaderSize = BI_SIZE;
	fileHeader.width = biTrueWidth;
	fileHeader.height = pix->height;
	fileHeader.planes = 1;
	fileHeader.bitsPerPixel = pixelSize * 8;
	fileHeader.compression = BI_RGB;
	fileHeader.bitmapDataSize = cbBmpBits;
	fileHeader.hRes = 0;
	fileHeader.vRes = 0;
	fileHeader.colors = (pixelSize == 1) ? 256 : 0;
	fileHeader.importantColors = 0;
	write(file, &fileHeader, sizeof(fileHeader));

	// write palette to file, in case of image is quantized
	if (pixelSize == 1)
	{
		uint8_t *pb = pix->palette;
		for (int i = 0; i < (int)fileHeader.colors; i++)
		{
			rgrgbPalette[i].rgbRed = *pb++;
			rgrgbPalette[i].rgbGreen = *pb++;
			rgrgbPalette[i].rgbBlue = *pb++;
			rgrgbPalette[i].rgbReserved = *pb++;
		}
		write(file, rgrgbPalette, paletteBytes);
	}

	uint8_t *pb = pix->buffer;
	uint8_t *pbBmpBits = (byte *)Mem_Alloc(cbBmpBits);
	for (int y = 0; y < fileHeader.height; y++)
	{
		int i = (fileHeader.height - 1 - y) * (fileHeader.width);
		for (int x = 0; x < pix->width; x++)
		{
			if (pixelSize == 1)
			{
				// 8-bit
				pbBmpBits[i] = pb[x];
			}
			else
			{
				// 24 bit
				pbBmpBits[i * pixelSize + 0] = pb[x * pixelSize + 2];
				pbBmpBits[i * pixelSize + 1] = pb[x * pixelSize + 1];
				pbBmpBits[i * pixelSize + 2] = pb[x * pixelSize + 0];
			}

			if (pixelSize == 4) {
				// write alpha channel
				pbBmpBits[i * pixelSize + 3] = pb[x * pixelSize + 3];
			}
			i++;
		}
		pb += pix->width * pixelSize;
	}

	// write bitmap bits (remainder of file)
	write(file, pbBmpBits, cbBmpBits);
	close(file);
	Mem_Free(pbBmpBits);

	return true;
}

/*
=============
Image_SaveDDS
=============
*/
bool Image_SaveDDS( const char *name, rgbdata_t *pix )
{
	int hint;
	char lumpname[64];
	rgbdata_t *dds_image = nullptr;

	if (COM_FileExists(name))
	{
		MsgDev(D_ERROR, "Image_SaveDDS: file already exists (%s)\n", name);
		return false; // already existed
	}

	if (!pix->buffer)
	{
		MsgDev(D_ERROR, "Image_SaveDDS: invalid image buffer (%s)\n", name);
		return false; // bogus parameter check
	}

	// check for easy out
	if (FBitSet(pix->flags, IMAGE_DXT_FORMAT))
	{
		bool status = COM_SaveFile(name, pix->buffer, pix->size);
		if (!status) {
			MsgDev(D_ERROR, "Image_SaveDDS: failed to save file (%s)\n", name);
		}
		return status;
	}

	COM_FileBase(name, lumpname);
	hint = Image_HintFromSuf(lumpname);
	dds_image = BufferToDDS(pix, DDS_GetSaveFormatForHint(hint, pix));

	if (!dds_image)
		return false;

	bool result = COM_SaveFile(name, dds_image->buffer, dds_image->size);
	Image_Free(dds_image);
	return result;
}

/*
=============
Image_SavePNG
=============
*/
bool Image_SavePNG( const char *name, rgbdata_t *pix )
{
	int			ret;
	uint		 y, outsize, pixel_size, filtered_size, idat_len;
	uint		 ihdr_len, crc32, rowsize, big_idat_len;
	byte		*in, *buffer, *out, *filtered_buffer, *rowend;
	z_stream 	 stream = {0};
	png_t		 png_hdr;
	png_footer_t	 png_ftr;

	if (COM_FileExists(name))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	pixel_size = 4;
	rowsize = pix->width * pixel_size;

	// get filtered image size
	filtered_size = ( rowsize + 1 ) * pix->height;
	out = filtered_buffer = reinterpret_cast<byte*>(Mem_Alloc(filtered_size));

	// apply adaptive filter to image
	for( y = 0; y < pix->height; y++ )
	{
		in = pix->buffer + y * pix->width * pixel_size;
		*out++ = PNG_F_NONE;
		rowend = in + rowsize;
		for( ; in < rowend; in += pixel_size )
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			if( pix->flags & IMAGE_HAS_ALPHA )
				*out++ = in[3];
		}
	}

	// get IHDR chunk length
	ihdr_len = sizeof( png_ihdr_t );

	// predict IDAT chunk length
	idat_len = deflateBound( NULL, filtered_size );

	// calculate PNG filesize
	outsize = sizeof( png_t );
	outsize += sizeof( idat_len );
	outsize += sizeof( idat_sign );
	outsize += idat_len;
	outsize += sizeof( png_footer_t );

	// write PNG header
	memcpy( png_hdr.sign, png_sign, sizeof( png_sign ) );

	// write IHDR chunk length
	png_hdr.ihdr_len = htonl( ihdr_len );

	// write IHDR chunk signature
	memcpy( png_hdr.ihdr_sign, ihdr_sign, sizeof( ihdr_sign ) );

	// write image width
	png_hdr.ihdr_chunk.width = htonl( pix->width );

	// write image height
	png_hdr.ihdr_chunk.height = htonl( pix->height );

	// write image bitdepth
	png_hdr.ihdr_chunk.bitdepth = 8;

	// write image colortype
	png_hdr.ihdr_chunk.colortype = ( pix->flags & IMAGE_HAS_ALPHA ) ? PNG_CT_RGBA : PNG_CT_RGB; // 8 bits of alpha

	// write image comression method
	png_hdr.ihdr_chunk.compression = 0;

	// write image filter type
	png_hdr.ihdr_chunk.filter = 0;

	// write image interlacing
	png_hdr.ihdr_chunk.interlace = 0;

	// get IHDR chunk CRC
	CRC32_Init( &crc32 );
	CRC32_ProcessBuffer( &crc32, &png_hdr.ihdr_sign, ihdr_len + sizeof( ihdr_sign ) );
	crc32 = CRC32_Final( crc32 );

	// write IHDR chunk CRC
	png_hdr.ihdr_crc32 = htonl( crc32 );

	out = buffer = (byte *)Mem_Alloc( outsize );

	stream.next_in = filtered_buffer;
	stream.avail_in = filtered_size;
	stream.next_out = buffer + sizeof( png_hdr ) + sizeof( idat_len ) + sizeof( idat_sign );
	stream.avail_out = idat_len;

	// compress image
	if( deflateInit( &stream, Z_BEST_COMPRESSION ) != Z_OK )
	{
		MsgDev( D_ERROR, "Image_SavePNG: deflateInit failed (%s)\n", name );
		Mem_Free( filtered_buffer );
		Mem_Free( buffer );
		return false;
	}

	ret = deflate( &stream, Z_FINISH );
	deflateEnd( &stream );

	Mem_Free( filtered_buffer );

	if( ret != Z_OK && ret != Z_STREAM_END )
	{
		MsgDev( D_ERROR, "Image_SavePNG: IDAT chunk compression failed (%s)\n", name );
		Mem_Free( buffer );
		return false;
	}

	// get final filesize
	outsize -= idat_len;
	idat_len = stream.total_out;
	outsize += idat_len;

	memcpy( out, &png_hdr, sizeof( png_t ) );

	out += sizeof( png_t );

	// convert IDAT chunk length to big endian
	big_idat_len = htonl( idat_len );

	// write IDAT chunk length
	memcpy( out, &big_idat_len, sizeof( idat_len ) );

	out += sizeof( idat_len );

	// write IDAT chunk signature
	memcpy( out, idat_sign, sizeof( idat_sign ) );

	// calculate IDAT chunk CRC
	CRC32_Init( &crc32 );
	CRC32_ProcessBuffer( &crc32, out, idat_len + sizeof( idat_sign ) );
	crc32 = CRC32_Final( crc32 );

	out += sizeof( idat_sign );
	out += idat_len;

	// write IDAT chunk CRC
	png_ftr.idat_crc32 = htonl( crc32 );

	// write IEND chunk length
	png_ftr.iend_len = 0;

	// write IEND chunk signature
	memcpy( png_ftr.iend_sign, iend_sign, sizeof( iend_sign ) );

	// write IEND chunk CRC
	png_ftr.iend_crc32 = htonl( iend_crc32 );

	// write PNG footer to buffer
	memcpy( out, &png_ftr, sizeof( png_ftr ) );

	bool result = COM_SaveFile( name, buffer, outsize );
	Mem_Free( buffer );
	return result;
}

/*
================
COM_SaveImage

handle bmp & tga
================
*/
bool COM_SaveImage( const char *filename, rgbdata_t *pix )
{
	const char	*ext = COM_FileExtension( filename );
	bool		anyformat = !Q_stricmp( ext, "" ) ? true : false;
	char		path[128], savename[128];
	const saveimage_t	*format;

	if( !pix || !pix->buffer || anyformat )
		return false;

	Q_strncpy( savename, filename, sizeof( savename ));
	COM_StripExtension( savename ); // remove extension if needed

	for( format = save_hint; format && format->formatstring; format++ )
	{
		if( !Q_stricmp( ext, format->ext ))
		{
			Q_sprintf( path, format->formatstring, savename, "", format->ext );
			if( format->savefunc( path, pix ))
				return true; // saved
		}
	}

	MsgDev( D_ERROR, "COM_SaveImage: unsupported format (%s)\n", ext );

	return false;
}

/*
=============================================================================

	IMAGE PROCESSING

=============================================================================
*/
#define TRANSPARENT_R	0x0
#define TRANSPARENT_G	0x0
#define TRANSPARENT_B	0xFF
#define IS_TRANSPARENT( p )	( p[0] == TRANSPARENT_R && p[1] == TRANSPARENT_G && p[2] == TRANSPARENT_B )
#define LERPBYTE( i )	r = resamplerow1[i]; out[i] = (byte)(((( resamplerow2[i] - r ) * lerp)>>16 ) + r )

static void Image_Resample32LerpLine( const byte *in, byte *out, int inwidth, int outwidth )
{
	int	j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int)(inwidth * 65536.0f / outwidth);
	endx = (inwidth-1);

	for( j = 0, f = 0; j < outwidth; j++, f += fstep )
	{
		xi = f>>16;
		if( xi != oldx )
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if( xi < endx )
		{
			lerp = f & 0xFFFF;
			*out++ = (byte)((((in[4] - in[0]) * lerp)>>16) + in[0]);
			*out++ = (byte)((((in[5] - in[1]) * lerp)>>16) + in[1]);
			*out++ = (byte)((((in[6] - in[2]) * lerp)>>16) + in[2]);
			*out++ = (byte)((((in[7] - in[3]) * lerp)>>16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

static void Image_Resample32Lerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	const byte *inrow;
	int	i, j, r, yi, oldy = 0, f, fstep, lerp, endy = (inheight - 1);
	int	inwidth4 = inwidth * 4;
	int	outwidth4 = outwidth * 4;
	byte	*out = (byte *)outdata;
	byte	*resamplerow1;
	byte	*resamplerow2;

	fstep = (int)(inheight * 65536.0f / outheight);

	resamplerow1 = (byte *)Mem_Alloc( outwidth * 4 * 2 );
	resamplerow2 = resamplerow1 + outwidth * 4;

	inrow = (const byte *)indata;

	Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
	Image_Resample32LerpLine( inrow + inwidth4, resamplerow2, inwidth, outwidth );

	for( i = 0, f = 0; i < outheight; i++, f += fstep )
	{
		yi = f >> 16;

		if( yi < endy )
		{
			lerp = f & 0xFFFF;

			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth4 * yi;
				if( yi == ( oldy + 1 )) memcpy( resamplerow1, resamplerow2, outwidth4 );
				else Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
				Image_Resample32LerpLine( inrow + inwidth4, resamplerow2, inwidth, outwidth );
				oldy = yi;
			}

			j = outwidth - 4;

			while( j >= 0 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}

			if( j & 2 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}

			if( j & 1 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}

			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		}
		else
		{
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth4 * yi;
				if( yi == ( oldy + 1 )) memcpy( resamplerow1, resamplerow2, outwidth4 );
				else Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
				oldy = yi;
			}

			memcpy( out, resamplerow1, outwidth4 );
		}
	}

	Mem_Free( resamplerow1 );
}

static void Image_Resample8Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j;
	byte	*in, *inrow;
	size_t	frac, fracstep;
	byte	*out = (byte *)outdata;

	in = (byte *)indata;
	fracstep = inwidth * 0x10000 / outwidth;

	for( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth * (i * inheight / outheight);
		frac = fracstep >> 1;

		for( j = 0; j < outwidth; j++ )
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
Image_Resample
================
*/
rgbdata_t *Image_Resample( rgbdata_t *pic, int new_width, int new_height )
{
	if (!pic) 
		return NULL;

	// nothing to resample ?
	if (pic->width == new_width && pic->height == new_height)
		return pic;

	MsgDev(D_REPORT, "Image_Resample: from %ix%i to %ix%i\n", pic->width, pic->height, new_width, new_height);
	rgbdata_t *out = Image_Alloc(new_width, new_height, FBitSet(pic->flags, IMAGE_QUANTIZED));

	if (FBitSet(pic->flags, IMAGE_QUANTIZED))
		Image_Resample8Nolerp(pic->buffer, pic->width, pic->height, out->buffer, out->width, out->height);
	else
		Image_Resample32Lerp(pic->buffer, pic->width, pic->height, out->buffer, out->width, out->height);

	// copy remaining data from source
	if (FBitSet(pic->flags, IMAGE_QUANTIZED))
		memcpy(out->palette, pic->palette, 1024);

	out->flags = pic->flags;

	// release old image
	Image_Free(pic);

	return out;
}

/*
==============
Image_ApplyAlphaMask

set transparent color index for all corresponding pixels from alpha mask
but only if alpha value is above alpha threshold
==============
*/
bool Image_ApplyAlphaMask(rgbdata_t *pic, rgbdata_t *alphaMask, float alphaThreshold)
{
	if (!pic || !alphaMask) {
		return false; // invalid buffer
	}

	if (!FBitSet(pic->flags, IMAGE_QUANTIZED)) {
		return false; 
	}

	if (pic->width != alphaMask->width || pic->height != alphaMask->height) {
		return false; // alpha mask and original image should be same size
	}

	int threshold = static_cast<int>(Q_round(alphaThreshold * 255.0f));
	size_t pixelCount = alphaMask->width * alphaMask->height;

	// release last index for transparency (replace with closest color)
	for (size_t i = 0; i < pixelCount; i++)
	{
		if (pic->buffer[i] == 255) {
			pic->buffer[i] = 254;
		}
	}

	for (size_t i = 0; i < pixelCount; i++)
	{
		int alpha = alphaMask->buffer[i];
		if (alpha < threshold) {
			pic->buffer[i] = 255;
		}
	}

	// fill last palette color with "transparency color"
	pic->palette[255 * 4 + 0] = TRANSPARENT_R;
	pic->palette[255 * 4 + 1] = TRANSPARENT_G;
	pic->palette[255 * 4 + 2] = TRANSPARENT_B;
	pic->palette[255 * 4 + 3] = 0xFF;

	SetBits(pic->flags, IMAGE_HAS_1BIT_ALPHA);
	return true;
}

/*
================
Image_ExtractAlphaMask

we can't store alpha-channel into 8-bit texture
but we can store it separate as another image
================
*/
rgbdata_t *Image_ExtractAlphaMask( rgbdata_t *pic )
{
	if (!pic) {
		return nullptr;
	}

	if (!FBitSet(pic->flags, IMAGE_HAS_ALPHA)) {
		return nullptr; // no alpha-channel stored
	}

	if (FBitSet(pic->flags, IMAGE_QUANTIZED)) {
		return nullptr; // can extract only from RGBA buffer
	}

	size_t pic_size = sizeof(rgbdata_t) + pic->width * pic->height;
	rgbdata_t *out = (rgbdata_t *)Mem_Alloc(pic_size);
	if (!out) {
		return nullptr;
	}

	out->buffer = reinterpret_cast<byte*>(out) + sizeof(rgbdata_t);
	out->size = pic->width * pic->height * sizeof(uint8_t);
	out->width = pic->width;
	out->height = pic->height;
	out->palette = nullptr;
	out->flags = 0;

	size_t pixel_count = pic->width * pic->height;
	for (size_t i = 0; i < pixel_count; i++) {
		out->buffer[i] = pic->buffer[i * 4 + 3];
	}

	return out;
}

/*
================
Image_ApplyGamma

we can't store alpha-channel into 8-bit texture
but we can store it separate as another image
================
*/
void Image_ApplyGamma( rgbdata_t *pic )
{
	if( !pic || FBitSet( pic->flags, IMAGE_DXT_FORMAT ))
		return; // can't process compressed image

	for( int i = 0; i < pic->width * pic->height; i++ )
	{
		// copy the alpha into color buffer
		pic->buffer[i*4+0] = TextureLightScale( pic->buffer[i*4+0] );
		pic->buffer[i*4+1] = TextureLightScale( pic->buffer[i*4+1] );
		pic->buffer[i*4+2] = TextureLightScale( pic->buffer[i*4+2] );
	}
}

/*
================
Image_MergeColorAlpha

we can't store alpha-channel into 8-bit texture
but we can store it separate as another image
================
*/
rgbdata_t *Image_MergeColorAlpha( rgbdata_t *color, rgbdata_t *alpha )
{
	rgbdata_t *int_alpha;
	byte	avalue;

	if (!color) return NULL;
	if (!alpha) return color;

	if (FBitSet(color->flags, IMAGE_DXT_FORMAT))
		return color; // can't merge compressed formats

	if (FBitSet(alpha->flags, IMAGE_DXT_FORMAT))
		return color; // can't merge compressed formats

	if (FBitSet(color->flags | alpha->flags, IMAGE_QUANTIZED))
		return color; // can't merge paletted images

	int_alpha = Image_Copy( alpha ); // duplicate the image

	if( color->width != alpha->width || color->height != alpha->height )
	{
		Image_Resample( int_alpha, color->width, color->height );
	}

	for( int i = 0; i < color->width * color->height; i++ )
	{
		// copy the alpha into color buffer (just use R instead?)
		avalue = (int_alpha->buffer[i*4+0] + int_alpha->buffer[i*4+1] + int_alpha->buffer[i*4+2]) / 3;

		if( avalue != 255 )
		{
			if( avalue != 0 )
			{
				SetBits( color->flags, IMAGE_HAS_8BIT_ALPHA );
				ClearBits( color->flags, IMAGE_HAS_1BIT_ALPHA );
			}
			else if( !FBitSet( color->flags, IMAGE_HAS_8BIT_ALPHA ))
				SetBits( color->flags, IMAGE_HAS_1BIT_ALPHA );
		}

		color->buffer[i*4+3] = avalue;
	}

	Image_Free( int_alpha );

	return color;
}

/*
================
Image_CreateCubemap

create cubemap from 6 images
images must be sorted in properly order
================
*/
rgbdata_t *Image_CreateCubemap( rgbdata_t *images[6], bool skybox, bool nomips )
{
	rgbdata_t *out;
	int	base_width;
	int	base_height;
	int	i;

	if( !images ) return NULL;

	base_width = (images[0]->width + 15) & ~15;
	base_height = (images[0]->height + 15) & ~15;

	for( i = 0; i < 6; i++ )
	{
		// validate the sides
		if( !images[i] || FBitSet( images[i]->flags, IMAGE_DXT_FORMAT|IMAGE_CUBEMAP|IMAGE_SKYBOX ))
			break;

		// rare case: cube sides with different dimensions
		images[i] = Image_Resample( images[i], base_width, base_height );
	}

	if( i != 6 ) return NULL;

	if( skybox )
		out = Image_AllocSkybox( base_width, base_height );
	else out = Image_AllocCubemap( base_width, base_height );

	if( nomips ) SetBits( out->flags, IMAGE_NOMIPS );

	// copy the sides
	for( i = 0; i < 6; i++ )
	{
		VectorAdd( out->reflectivity, images[i]->reflectivity, out->reflectivity );
		memcpy( out->buffer + (images[i]->size * i), images[i]->buffer, images[i]->size );
		Image_Free( images[i] ); // release original
	}

	// divide by sides count
	VectorDivide( out->reflectivity, 6.0f, out->reflectivity );

	return out;
}

/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void Image_BuildMipMapLinear( uint *in, int inWidth, int inHeight )
{
	int	i, j, k;
	byte	*outpix;
	int	inWidthMask, inHeightMask;
	int	total;
	int	outWidth, outHeight;
	uint	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = (uint *)Mem_Alloc( outWidth * outHeight * 4 );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for( i = 0; i < outHeight; i++ )
	{
		for( j = 0; j < outWidth; j++ )
		{
			outpix = (byte *)(temp + i * outWidth + j);
			for( k = 0; k < 4; k++ )
			{
				total =
					1 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +
					1 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k];
				outpix[k] = total / 36;
			}
		}
	}

	memcpy( in, temp, outWidth * outHeight * 4 );
	Mem_Free( temp );
}

/*
=================
Image_BuildMipMap

Operates in place, quartering the size of the texture
=================
*/
void Image_BuildMipMap( byte *in, int width, int height, bool isNormalMap )
{
	byte	*out = in;
	float	inv127 = (1.0f / 127.0f);
	vec3_t	normal;
	int	x, y;

	if( isNormalMap )
	{
		width <<= 2;
		height >>= 1;

		for( y = 0; y < height; y++, in += width )
		{
			for( x = 0; x < width; x += 8, in += 8, out += 4 )
			{
				normal[0] = (in[0] * inv127 - 1.0f) + (in[4] * inv127 - 1.0f) + (in[width+0] * inv127 - 1.0f) + (in[width+4] * inv127 - 1.0f);
				normal[1] = (in[1] * inv127 - 1.0f) + (in[5] * inv127 - 1.0f) + (in[width+1] * inv127 - 1.0f) + (in[width+5] * inv127 - 1.0f);
				normal[2] = (in[2] * inv127 - 1.0f) + (in[6] * inv127 - 1.0f) + (in[width+2] * inv127 - 1.0f) + (in[width+6] * inv127 - 1.0f);

				if( VectorNormalize( normal ) == 0.0f )
					VectorSet( normal, 0.5f, 0.5f, 1.0f );

				out[0] = (byte)(128 + 127 * normal[0]);
				out[1] = (byte)(128 + 127 * normal[1]);
				out[2] = (byte)(128 + 127 * normal[2]);
				out[3] = 255;
			}
		}
	}
	else
	{
#if 0
		Image_BuildMipMapLinear( (uint *)in, width, height );
#else
		width <<= 2;
		height >>= 1;

		for( y = 0; y < height; y++, in += width )
		{
			for( x = 0; x < width; x += 8, in += 8, out += 4 )
			{
				out[0] = (in[0] + in[4] + in[width+0] + in[width+4]) >> 2;
				out[1] = (in[1] + in[5] + in[width+1] + in[width+5]) >> 2;
				out[2] = (in[2] + in[6] + in[width+2] + in[width+6]) >> 2;
				out[3] = (in[3] + in[7] + in[width+3] + in[width+7]) >> 2;
			}
		}
#endif
	}
}

/*
================
Image_ConvertBumpStalker

convert stalker bump into normal textures
================
*/
void Image_ConvertBumpStalker( rgbdata_t *bump, rgbdata_t *spec )
{
	if( !bump || !spec || bump->width != spec->width || bump->height != spec->height )
		return; // we need both the textures to processing

	for( int i = 0; i < bump->width * bump->height; i++ )
	{
		byte	bump_rgba[4], spec_rgba[4];
		vec3_t	normal, error;
		byte	temp[4];

		memcpy( bump_rgba, bump->buffer + (i * 4), sizeof( bump_rgba ));
		memcpy( spec_rgba, spec->buffer + (i * 4), sizeof( bump_rgba ));

		normal[0] = (float)bump_rgba[3] * (1.0f / 255.0f);	// alpha is X
		normal[1] = (float)bump_rgba[2] * (1.0f / 255.0f);	// blue is Y
		normal[2] = (float)bump_rgba[1] * (1.0f / 255.0f);	// green is Z

		error[0] = (float)spec_rgba[0] * (1.0f / 255.0f);	// alpha is X
		error[1] = (float)spec_rgba[1] * (1.0f / 255.0f);	// blue is Y
		error[2] = (float)spec_rgba[2] * (1.0f / 255.0f);	// green is Z

		// compensate normal error
		VectorAdd( normal, error, normal );
		normal[0] -= 1.0f;
		normal[1] -= 1.0f;
		normal[2] -= 1.0f;		

		VectorNormalize( normal );		

		// store back to byte
		temp[0] = (byte)(normal[0] * 127.0f + 128.0f);
		temp[1] = (byte)(normal[1] * 127.0f + 128.0f);
		temp[2] = (byte)(normal[2] * 127.0f + 128.0f);
		temp[3] = 0;

		// put transformed pixel back to the buffer
		memcpy( bump->buffer + (i * 4), temp, sizeof( temp ));		

		// store glossiness
		temp[0] = bump_rgba[0];
		temp[1] = bump_rgba[0];
		temp[2] = bump_rgba[0];
		temp[3] = spec_rgba[3]; // store heightmap into alpha

		// put transformed pixel back to the buffer
		memcpy( spec->buffer + (i * 4), temp, sizeof( temp ));	
	}
}
