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

#include <windows.h>
#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "scriplib.h"
#include "filesystem.h"
#include "imagelib.h"

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
	size_t pic_size = sizeof( rgbdata_t ) + src->size + (FBitSet( src->flags, IMAGE_QUANTIZED ) ? 1024 : 0 );
	rgbdata_t	*dst = (rgbdata_t *)Mem_Alloc( pic_size );
	dst->buffer = ((byte *)dst) + sizeof( rgbdata_t );

	if( FBitSet( src->flags, IMAGE_QUANTIZED ))
	{
		dst->palette = dst->buffer + src->size;
		memcpy( dst->palette, src->palette, 1024 );
	}

	memcpy( dst->buffer, src->buffer, src->size );

	dst->size = src->size;
	dst->width = src->width;
	dst->height = src->height;
	dst->flags = src->flags;

	return dst;
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

	pic = Image_Alloc( columns, rows, ( bhdr.bitsPerPixel == 4 || bhdr.bitsPerPixel == 8 ));

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

	if( bhdr.bitsPerPixel == 4 || bhdr.bitsPerPixel == 8 )
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

				if( ++column == columns )
					break;

				palIndex = alpha & 0x0F;
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
================
COM_LoadImage

handle bmp & tga
================
*/
rgbdata_t *COM_LoadImage( const char *filename )
{
	size_t fileSize;	
	byte *buf = (byte *)COM_LoadFile( filename, &fileSize, false );
	const char *ext = COM_FileExtension( filename );
	rgbdata_t	*pic = NULL;

	if( !buf ) return NULL;

	if( !Q_stricmp( ext, "tga" ))
		pic = Image_LoadTGA( filename, buf, fileSize );
	else if( !Q_stricmp( ext, "bmp" ))
		pic = Image_LoadBMP( filename, buf, fileSize );
	else MsgDev( D_ERROR, "COM_LoadImage: unsupported format (%s)\n", ext );

	Mem_Free( buf ); // release file

	return pic; // may be NULL
}

/*
================
COM_LoadImage

handle bmp & tga
================
*/
rgbdata_t *COM_LoadImageMemory( const char *filename, const byte *buf, size_t fileSize )
{
	const char *ext = COM_FileExtension( filename );
	rgbdata_t	*pic = NULL;

	if( !buf )
	{
		MsgDev( D_ERROR, "COM_LoadImageMemory: unable to load (%s)\n", filename );
		return NULL;
	}

	if( !Q_stricmp( ext, "tga" ))
		pic = Image_LoadTGA( filename, buf, fileSize );
	else if( !Q_stricmp( ext, "bmp" ))
		pic = Image_LoadBMP( filename, buf, fileSize );
	else MsgDev( D_ERROR, "COM_LoadImage: unsupported format (%s)\n", ext );

	return pic; // may be NULL
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

	if( !pic || !FBitSet( pic->flags, IMAGE_QUANTIZED ))
		return; // only for quantized images

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
	if( 0 )//firsttrans >= 0 )
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
	else
	{
		// just turn last color to blue
		pic->palette[255*4+0] = TRANSPARENT_R;
		pic->palette[255*4+1] = TRANSPARENT_G;
		pic->palette[255*4+2] = TRANSPARENT_B;
		pic->palette[255*4+3] = 0xFF;
	}

	// needs for software mip generator
	SetBits( pic->flags, IMAGE_HAS_1BIT_ALPHA );
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