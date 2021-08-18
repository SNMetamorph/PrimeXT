/*
imagelib.cpp - simple loader\serializer for TGA & BMP
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
#include "imagelib.h"
#include "filesystem.h"
#include "makewad.h"
#include "mathlib.h"

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

// suffix converts to img_type and back
const imgtype_t img_hints[] =
{
{ "_mask",   IMG_ALPHAMASK	}, // alpha-channel stored to another lump
{ "_norm",   IMG_NORMALMAP	}, // indexed normalmap
{ "_n",      IMG_NORMALMAP	}, // indexed normalmap
{ "_nrm",    IMG_NORMALMAP	}, // indexed normalmap
{ "_local",  IMG_NORMALMAP	}, // indexed normalmap
{ "_ddn",    IMG_NORMALMAP	}, // indexed normalmap
{ "_spec",   IMG_GLOSSMAP	}, // grayscale\color specular
{ "_gloss",  IMG_GLOSSMAP	}, // grayscale\color specular
{ "_hmap",   IMG_HEIGHTMAP	}, // heightmap (can be converted to normalmap)
{ "_height", IMG_HEIGHTMAP	}, // heightmap (can be converted to normalmap)
{ "_luma",   IMG_LUMA	}, // self-illuminate parts on the diffuse
{ "_add",    IMG_LUMA	}, // self-illuminate parts on the diffuse
{ "_illum",  IMG_LUMA	}, // self-illuminate parts on the diffuse
{ NULL,    0		}  // terminator
};

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
	size_t pic_size = sizeof( rgbdata_t ) + (width * height * (paletted ? 1 : 4)) + (paletted ? 1024 : 0);
	rgbdata_t	*pic = (rgbdata_t *)Mem_Alloc( pic_size );

	if( paletted )
	{
		pic->buffer = ((byte *)pic) + sizeof( rgbdata_t ); 
		pic->palette = ((byte *)pic) + sizeof( rgbdata_t ) + width * height;
		pic->flags |= IMAGE_QUANTIZED;
	}
	else
	{
		pic->buffer = ((byte *)pic) + sizeof( rgbdata_t ); 
		pic->palette = NULL; // not present
	}

	pic->size = (width * height * (paletted ? 1 : 4));
	pic->width = width;
	pic->height = height;

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
	rgbdata_t *dst = Image_Alloc( src->width, src->height, FBitSet( src->flags, IMAGE_QUANTIZED ));

	if( FBitSet( src->flags, IMAGE_QUANTIZED ))
		memcpy( dst->palette, src->palette, 1024 );
	memcpy( dst->buffer, src->buffer, src->size );

	dst->size = src->size;
	dst->width = src->width;
	dst->height = src->height;
	dst->flags = src->flags;

	return dst;
}

/*
===========
Image_HintFromSuf

Convert name suffix into image type
===========
*/
char Image_HintFromSuf( const char *lumpname )
{
	char		barename[64];
	char		suffix[16];
	const imgtype_t	*hint;

	// trying to extract hint from the name
	Q_strncpy( barename, lumpname, sizeof( barename ));

	// we not known about filetype, so match only by filename
	for( hint = img_hints; hint->ext; hint++ )
	{
		if( Q_strlen( barename ) <= Q_strlen( hint->ext ))
			continue;	// name too short

		Q_strncpy( suffix, barename + Q_strlen( barename ) - Q_strlen( hint->ext ), sizeof( suffix ));
		if( !Q_stricmp( suffix, hint->ext ))
			return hint->type;
	}

	// special additional check for "_normal"
	if( Q_stristr( lumpname, "_normal" ))
		return IMG_NORMALMAP;

	// no any special type was found
	return IMG_DIFFUSE;
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
	bool	paletted;
	rgbdata_t *pic;

	if( filesize < sizeof( tga_t ))
		return NULL;

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
	if( !Image_ValidSize( name, targa_header.width, targa_header.height ))
		return NULL;

	if( targa_header.image_type == 1 || targa_header.image_type == 9 )
	{
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 8 bit images supported for type 1 and 9\n", name );
			return NULL;
		}

		if( targa_header.colormap_length != 256 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 8 bit colormaps are supported for type 1 and 9\n", name );
			return NULL;
		}

		if( targa_header.colormap_index )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) colormap_index is not supported for type 1 and 9\n", name );
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
			MsgDev( D_WARN, "Image_LoadTGA: (%s) only 24 and 32 bit colormaps are supported for type 1 and 9\n", name );
			return NULL;
		}
	}
	else if( targa_header.image_type == 2 || targa_header.image_type == 10 )
	{
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 32 or 24 bit images supported for type 2 and 10\n", name );
			return NULL;
		}
	}
	else if( targa_header.image_type == 3 || targa_header.image_type == 11 )
	{
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 8 bit images supported for type 3 and 11\n", name );
			return NULL;
		}
	}

	paletted = ( targa_header.image_type == 1 || targa_header.image_type == 9 );

	pic = Image_Alloc( targa_header.width, targa_header.height, paletted );
	if( paletted ) memcpy( pic->palette, palette, sizeof( palette ));

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
		if( FBitSet( pic->flags, IMAGE_QUANTIZED ))
		{
			pixbuf = targa_rgba + ( rows - 1 ) * columns;
			row_inc = -columns * 2;
		}
		else
		{
			pixbuf = targa_rgba + ( rows - 1 ) * columns * 4;
			row_inc = -columns * 4 * 2;
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

			if( FBitSet( pic->flags, IMAGE_QUANTIZED ))
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
	int	i, columns, column, rows, row, bpp = 1;
	int	cbPalBytes = 0, padSize = 0, bps = 0;
	rgbdata_t *pic;
	bmp_t	bhdr;

	if( filesize < sizeof( bhdr ))
		return NULL; 

	buf_p = (byte *)buffer;
	bhdr.id[0] = *buf_p++;
	bhdr.id[1] = *buf_p++;		// move pointer
	bhdr.fileSize = *(long *)buf_p;	buf_p += 4;
	bhdr.reserved0 = *(long *)buf_p;	buf_p += 4;
	bhdr.bitmapDataOffset = *(long *)buf_p;	buf_p += 4;
	bhdr.bitmapHeaderSize = *(long *)buf_p;	buf_p += 4;
	bhdr.width = *(long *)buf_p;		buf_p += 4;
	bhdr.height = *(long *)buf_p;		buf_p += 4;
	bhdr.planes = *(short *)buf_p;	buf_p += 2;
	bhdr.bitsPerPixel = *(short *)buf_p;	buf_p += 2;
	bhdr.compression = *(long *)buf_p;	buf_p += 4;
	bhdr.bitmapDataSize = *(long *)buf_p;	buf_p += 4;
	bhdr.hRes = *(long *)buf_p;		buf_p += 4;
	bhdr.vRes = *(long *)buf_p;		buf_p += 4;
	bhdr.colors = *(long *)buf_p;		buf_p += 4;
	bhdr.importantColors = *(long *)buf_p;	buf_p += 4;

	// bogus file header check
	if( bhdr.reserved0 != 0 ) return NULL;
	if( bhdr.planes != 1 ) return NULL;

	if( memcmp( bhdr.id, "BM", 2 ))
	{
		MsgDev( D_ERROR, "Image_LoadBMP: only Windows-style BMP files supported (%s)\n", name );
		return NULL;
	} 

	if( bhdr.bitmapHeaderSize != 0x28 )
	{
		MsgDev( D_ERROR, "Image_LoadBMP: invalid header size %i\n", bhdr.bitmapHeaderSize );
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
		return false;
	}

	columns = bhdr.width;
	rows = abs( bhdr.height );

	if( !Image_ValidSize( name, columns, rows ))
		return false;          

	pic = Image_Alloc( columns, rows, ( bhdr.bitsPerPixel == 8 ));

	if( bhdr.bitsPerPixel <= 8 )
	{
		// figure out how many entries are actually in the table
		if( bhdr.colors == 0 )
		{
			bhdr.colors = 256;
			cbPalBytes = (1 << bhdr.bitsPerPixel) * sizeof( RGBQUAD );
		}
		else cbPalBytes = bhdr.colors * sizeof( RGBQUAD );
	}

	memcpy( palette, buf_p, cbPalBytes );

	if( bhdr.bitsPerPixel == 8 )
	{
		pixbuf = pic->palette;

		// bmp have a reversed palette colors
		for( i = 0; i < bhdr.colors; i++ )
		{
			*pixbuf++ = palette[i][2];
			*pixbuf++ = palette[i][1];
			*pixbuf++ = palette[i][0];
			*pixbuf++ = palette[i][3];

			if( palette[i][0] != palette[i][1] || palette[i][1] != palette[i][2] )
				pic->flags |= IMAGE_HAS_COLOR;
		}
	}
	else bpp = 4;

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
				*pixbuf++ = red = palette[palIndex][2];
				*pixbuf++ = green = palette[palIndex][1];
				*pixbuf++ = blue = palette[palIndex][0];
				*pixbuf++ = palette[palIndex][3];
				if( ++column == columns ) break;
				palIndex = alpha & 0x0F;
				*pixbuf++ = red = palette[palIndex][2];
				*pixbuf++ = green = palette[palIndex][1];
				*pixbuf++ = blue = palette[palIndex][0];
				*pixbuf++ = palette[palIndex][3];
				break;
			case 8:
				palIndex = *buf_p++;
				red = palette[palIndex][2];
				green = palette[palIndex][1];
				blue = palette[palIndex][0];
				alpha = palette[palIndex][3];

				if( FBitSet( pic->flags, IMAGE_QUANTIZED ))
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
				Mem_Free( pic );
				return NULL;
			}

			if( !FBitSet( pic->flags, IMAGE_QUANTIZED ) && ( red != green || green != blue ))
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

	return pic;
}

/*
=============
Image_LoadMIP

just read header
=============
*/
rgbdata_t *Image_LoadMIP( const char *name, const byte *buffer, size_t filesize )
{
	char	tmpname[64];

	if( filesize < sizeof( mip_t ))
		return NULL;

	mip_t *mip = (mip_t *)buffer;

	if( !Image_ValidSize( name, mip->width, mip->height ))
		return NULL;    

	int pixels = mip->width * mip->height;

	if( filesize < ( sizeof( mip_t ) + (( pixels * 85 ) >> 6 ) + sizeof( short ) + 768 ))
	{
		MsgDev( D_ERROR, "Image_LoadMIP: %s probably corrupted\n", name );
		return NULL;
	}

	rgbdata_t	*pic = Image_Alloc( mip->width, mip->height, true );
	memcpy( pic->buffer, buffer + mip->offsets[0], pixels );

	byte *pal = (byte *)buffer + mip->offsets[0] + (((mip->width * mip->height) * 85)>>6);

	int numcolors = *(short *)pal;
	if( numcolors != 256 ) MsgDev( D_WARN, "Image_LoadMIP: %s invalid palette num colors %i\n", name, numcolors );
	pal += sizeof( short ); // skip colorsize 

	// expand palette
	for( int i = 0; i < 256; i++ )
	{
		pic->palette[i*4+0] = *pal++;
		pic->palette[i*4+1] = *pal++;
		pic->palette[i*4+2] = *pal++;
		pic->palette[i*4+3] = 255;

		if( pic->palette[i*4+0] != pic->palette[i*4+1] || pic->palette[i*4+1] != pic->palette[i*4+2] )
			pic->flags |= IMAGE_HAS_COLOR;
	}

	// check for one-bit alpha
	COM_FileBase( name, tmpname );

	if( tmpname[0] == '{' && pic->palette[255*3+0] == 0 && pic->palette[255*3+1] == 0 && pic->palette[255*3+2] == 255 )
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
	if( filesize < sizeof( lmp_t ))
		return NULL;

	lmp_t *lmp = (lmp_t *)buffer;

	if( !Image_ValidSize( name, lmp->width, lmp->height ))
		return NULL;    

	int pixels = lmp->width * lmp->height;

	if( filesize < ( sizeof( lmp_t ) + pixels + sizeof( short ) + 768 ))
	{
		MsgDev( D_ERROR, "Image_LoadLMP: %s probably corrupted\n", name );
		return NULL;
	}

	rgbdata_t	*pic = Image_Alloc( lmp->width, lmp->height, true );
	memcpy( pic->buffer, buffer + sizeof( lmp_t ), pixels );

	byte *pal = (byte *)buffer + sizeof( lmp_t ) + pixels;

	int numcolors = *(short *)pal;
	if( numcolors != 256 ) MsgDev( D_WARN, "Image_LoadLMP: %s invalid palette num colors %i\n", name, numcolors );
	pal += sizeof( short ); // skip colorsize 

	// expand palette
	for( int i = 0; i < 256; i++ )
	{
		pic->palette[i*4+0] = *pal++;
		pic->palette[i*4+1] = *pal++;
		pic->palette[i*4+2] = *pal++;
		pic->palette[i*4+3] = 255;

		if( pic->palette[i*4+0] != pic->palette[i*4+1] || pic->palette[i*4+1] != pic->palette[i*4+2] )
			pic->flags |= IMAGE_HAS_COLOR;
	}

	// always has the alpha
	pic->flags |= IMAGE_HAS_1BIT_ALPHA;

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
	size_t fileSize;	
	byte *buf = (byte *)COM_LoadFile( filename, &fileSize, false );
	const char *ext = COM_FileExtension( filename );
	rgbdata_t	*pic = NULL;
	char barename[64];

	if( !buf && source_wad != NULL )
	{
		COM_FileBase( filename, barename );
		buf = W_LoadLump( source_wad, barename, &fileSize, W_TypeFromExt( filename ));
	}

	if( !buf )
	{
		if( !quiet )
			MsgDev( D_ERROR, "COM_LoadImage: unable to load (%s)\n", filename );
		return NULL;
	}

	if( !Q_stricmp( ext, "tga" ))
		pic = Image_LoadTGA( filename, buf, fileSize );
	else if( !Q_stricmp( ext, "bmp" ))
		pic = Image_LoadBMP( filename, buf, fileSize );
	else if( !Q_stricmp( ext, "mip" ))
		pic = Image_LoadMIP( filename, buf, fileSize );
	else if( !Q_stricmp( ext, "lmp" ))
		pic = Image_LoadLMP( filename, buf, fileSize );
	else if( !quiet )
		MsgDev( D_ERROR, "COM_LoadImage: unsupported format (%s)\n", ext );

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
	const char	*comment = "Generated by XashNT MakeWad tool.\0";
	int		i, y, outsize, pixel_size = 4;
	const byte	*bufend, *in;
	byte		*buffer, *out;

	if( COM_FileExists( name ))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	if( FBitSet( pix->flags, IMAGE_QUANTIZED ))
	{
		outsize = pix->width * pix->height + 18 + Q_strlen( comment );
	}
	else
	{
		if( pix->flags & IMAGE_HAS_ALPHA )
			outsize = pix->width * pix->height * 4 + 18 + Q_strlen( comment );
		else outsize = pix->width * pix->height * 3 + 18 + Q_strlen( comment );
	}

	if( FBitSet( pix->flags, IMAGE_QUANTIZED ))
		outsize += 768;	// palette

	buffer = (byte *)Mem_Alloc( outsize );
	memset( buffer, 0, 18 );

	// prepare header
	buffer[0] = Q_strlen( comment ); // tga comment length

	if( FBitSet( pix->flags, IMAGE_QUANTIZED ))
	{
		buffer[1] = 1;	// color map type
		buffer[2] = 1;	// uncompressed color mapped image
		buffer[5] = 0;	// number of palette entries (lo)
		buffer[6] = 1;	// number of palette entries (hi)
		buffer[7] = 24;	// palette bpp
	}
	else buffer[2] = 2;		// uncompressed type

	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;

	if( !FBitSet( pix->flags, IMAGE_QUANTIZED ))
	{
		buffer[16] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 32 : 24; // RGB pixel size
		buffer[17] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 8 : 0; // 8 bits of alpha
	}
	else buffer[16] = 8; // pixel size
	Q_strncpy( (char *)(buffer + 18), comment, Q_strlen( comment )); 
	out = buffer + 18 + Q_strlen( comment );

	// store palette, swapping rgb to bgr
	if( FBitSet( pix->flags, IMAGE_QUANTIZED ))
	{
		for( i = 0; i < 256; i++ )
		{
			*out++ = pix->palette[i*4+2];	// blue
			*out++ = pix->palette[i*4+1];	// green
			*out++ = pix->palette[i*4+0];	// red
		}

		// store the image data (and flip upside down)
		for( y = pix->height - 1; y >= 0; y-- )
		{
			in = pix->buffer + y * pix->width;
			bufend = in + pix->width;
			for( ; in < bufend; in++ )
				*out++ = *in;
		}
	}
	else
	{
		// swap rgba to bgra and flip upside down
		for( y = pix->height - 1; y >= 0; y-- )
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for( ; in < bufend; in += pixel_size )
			{
				*out++ = in[2];
				*out++ = in[1];
				*out++ = in[0];
				if( pix->flags & IMAGE_HAS_ALPHA )
					*out++ = in[3];
			}
		}
	}

	COM_SaveFile( name, buffer, outsize );
	Mem_Free( buffer );

	return true;
}

bool Image_SaveBMP( const char *name, rgbdata_t *pix )
{
	long		file;
	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmih;
	RGBQUAD		rgrgbPalette[256];
	dword		cbBmpBits;
	byte		*pb, *pbBmpBits;
	dword		cbPalBytes = 0;
	dword		biTrueWidth;
	int		pixel_size;
	int		i, x, y;

	if( COM_FileExists( name ))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	if( FBitSet( pix->flags, IMAGE_QUANTIZED ))
		pixel_size = 1;
	else pixel_size = 4;

	COM_CreatePath( (char *)name );

	file = open( name, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666 );
	if( file < 0 ) return false;

	// NOTE: align transparency column will sucessfully removed
	// after create sprite or lump image, it's just standard requiriments 
	biTrueWidth = ((pix->width + 3) & ~3);
	cbBmpBits = biTrueWidth * pix->height * pixel_size;
	if( pixel_size == 1 ) cbPalBytes = 256 * sizeof( RGBQUAD );

	// Bogus file header check
	bmfh.bfType = MAKEWORD( 'B', 'M' );
	bmfh.bfSize = sizeof( bmfh ) + sizeof( bmih ) + cbBmpBits + cbPalBytes;
	bmfh.bfOffBits = sizeof( bmfh ) + sizeof( bmih ) + cbPalBytes;
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;

	// write header
	write( file, &bmfh, sizeof( bmfh ));

	// size of structure
	bmih.biSize = sizeof( bmih );
	bmih.biWidth = biTrueWidth;
	bmih.biHeight = pix->height;
	bmih.biPlanes = 1;
	bmih.biBitCount = pixel_size * 8;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = cbBmpBits;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = ( pixel_size == 1 ) ? 256 : 0;
	bmih.biClrImportant = 0;

	// write info header
	write( file, &bmih, sizeof( bmih ));

	pbBmpBits = (byte *)Mem_Alloc( cbBmpBits );

	if( pixel_size == 1 )
	{
		pb = pix->palette;

		// copy over used entries
		for( i = 0; i < (int)bmih.biClrUsed; i++ )
		{
			rgrgbPalette[i].rgbRed = *pb++;
			rgrgbPalette[i].rgbGreen = *pb++;
			rgrgbPalette[i].rgbBlue = *pb++;
			rgrgbPalette[i].rgbReserved = *pb++;
		}

		// write palette
		write( file, rgrgbPalette, cbPalBytes );
	}

	pb = pix->buffer;

	for( y = 0; y < bmih.biHeight; y++ )
	{
		i = (bmih.biHeight - 1 - y ) * (bmih.biWidth);

		for( x = 0; x < pix->width; x++ )
		{
			if( pixel_size == 1 )
			{
				// 8-bit
				pbBmpBits[i] = pb[x];
			}
			else
			{
				// 24 bit
				pbBmpBits[i*pixel_size+0] = pb[x*pixel_size+2];
				pbBmpBits[i*pixel_size+1] = pb[x*pixel_size+1];
				pbBmpBits[i*pixel_size+2] = pb[x*pixel_size+0];
			}

			if( pixel_size == 4 ) // write alpha channel
				pbBmpBits[i*pixel_size+3] = pb[x*pixel_size+3];
			i++;
		}

		pb += pix->width * pixel_size;
	}

	// write bitmap bits (remainder of file)
	write( file, pbBmpBits, cbBmpBits );
	close( file );
	Mem_Free( pbBmpBits );

	return true;
}

/*
================
COM_SaveImage

handle bmp & tga
================
*/
bool COM_SaveImage( const char *filename, rgbdata_t *pix )
{
	const char *ext = COM_FileExtension( filename );

	if( !pix )
	{
		MsgDev( D_ERROR, "COM_SaveImage: pix == NULL\n" );
		return false;
	}

	if( !Q_stricmp( ext, "tga" ))
		return Image_SaveTGA( filename, pix );
	else if( !Q_stricmp( ext, "bmp" ))
		return Image_SaveBMP( filename, pix );
	else if( !Q_stricmp( ext, "lmp" ))
		return LMP_WriteLmptex( filename, pix, true );
	else
	{
		MsgDev( D_ERROR, "COM_SaveImage: unsupported format (%s)\n", ext );
		return false;
	}
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

void Image_Resample32Lerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
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

void Image_Resample8Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
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
	if( !pic ) return NULL;

	// nothing to resample ?
	if( pic->width == new_width && pic->height == new_height )
		return pic;

	MsgDev( D_REPORT, "Image_Resample: from %ix%i to %ix%i\n", pic->width, pic->height, new_width, new_height );

	rgbdata_t *out = Image_Alloc( new_width, new_height, FBitSet( pic->flags, IMAGE_QUANTIZED ));

	if( FBitSet( pic->flags, IMAGE_QUANTIZED ))
		Image_Resample8Nolerp( pic->buffer, pic->width, pic->height, out->buffer, out->width, out->height );
	else Image_Resample32Lerp( pic->buffer, pic->width, pic->height, out->buffer, out->width, out->height );

	// copy remaining data from source
	if( FBitSet( pic->flags, IMAGE_QUANTIZED ))
		memcpy( out->palette, pic->palette, 1024 );
	out->flags = pic->flags;

	// release old image
	Mem_Free( pic );

	return out;
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
	rgbdata_t *out;

	if( !pic ) return NULL;

	if( FBitSet( pic->flags, IMAGE_QUANTIZED ))
		return NULL; // can extract only from RGBA buffer

	if( !FBitSet( pic->flags, IMAGE_HAS_ALPHA ))
		return NULL; // no alpha-channel stored

	out = Image_Copy( pic ); // duplicate the image

	for( int i = 0; i < pic->width * pic->height; i++ )
	{
		// copy the alpha into color buffer
		out->buffer[i*4+0] = pic->buffer[i*4+3];
		out->buffer[i*4+1] = pic->buffer[i*4+3];
		out->buffer[i*4+2] = pic->buffer[i*4+3];
		out->buffer[i*4+3] = 0xFF; // clear the alpha
	}

	ClearBits( out->flags, IMAGE_HAS_COLOR );
	ClearBits( out->flags, IMAGE_HAS_ALPHA );

	return out;
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

	if( !color ) return NULL;
	if( !alpha ) return color;

	if( FBitSet( color->flags|alpha->flags, IMAGE_QUANTIZED ))
		return color; // can't merge compressed formats

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

	Mem_Free( int_alpha );

	return color;
}

/*
==============
Image_MakeOneBitAlpha

remap all pixels of color 0, 0, 255 to index 255
and remap index 255 to something else
==============
*/
void Image_MakeOneBitAlpha( rgbdata_t *pic )
{
	byte	transtable[256], *buf;
	int	i, j, firsttrans = -1;

	if( !pic ) return;

	if( !FBitSet( pic->flags, IMAGE_QUANTIZED ))
		return; // only for quantized images

	// don't move colors in quake palette!
	if( FBitSet( pic->flags, IMAGE_QUAKE1_PAL ))
	{
		// needs for software mip generator
		SetBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
		return;
	}

	for( i = 0; i < 256; i++ )
	{
		if( IS_TRANSPARENT(( pic->palette + ( i * 4 ))))
		{
			transtable[i] = 255;
			if( firsttrans < 0 )
				firsttrans = i;
		}
		else transtable[i] = i;
	}

	// if there is some transparency, translate it
	if( firsttrans >= 0 )
	{
		if( !IS_TRANSPARENT(( pic->palette + ( 255 * 4 ))))
			transtable[255] = firsttrans;
		buf = pic->buffer;

		for( j = 0; j < pic->height; j++ )
		{
			for( i = 0; i < pic->width; i++ )
			{
				*buf = transtable[*buf];
				buf++;
			}
		}

		// move palette entry for pixels previously mapped to entry 255
		pic->palette[firsttrans*4+0] = pic->palette[255*4+0];
		pic->palette[firsttrans*4+1] = pic->palette[255*4+1];
		pic->palette[firsttrans*4+2] = pic->palette[255*4+2];
		pic->palette[firsttrans*4+3] = pic->palette[255*4+3];
		pic->palette[255*4+0] = TRANSPARENT_R;
		pic->palette[255*4+1] = TRANSPARENT_G;
		pic->palette[255*4+2] = TRANSPARENT_B;
		pic->palette[255*4+3] = 0xFF;
	}

	// needs for software mip generator
	SetBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
}