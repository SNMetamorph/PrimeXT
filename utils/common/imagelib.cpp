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
#include "ddstex.h"
#include "mathlib.h"

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
{ "_bump",   IMG_STALKER_BUMP	}, // stalker two-component bump
{ "_bump#",  IMG_STALKER_GLOSS}, // stalker two-component bump
{ "ft",	   IMG_SKYBOX_FT	},
{ "bk",      IMG_SKYBOX_BK	},
{ "up",      IMG_SKYBOX_UP	},
{ "dn",      IMG_SKYBOX_DN	},
{ "rt",      IMG_SKYBOX_RT	},
{ "lf",      IMG_SKYBOX_LF	},
{ "px",      IMG_CUBEMAP_PX	},
{ "nx",      IMG_CUBEMAP_NX	},
{ "py",      IMG_CUBEMAP_PY	},
{ "ny",      IMG_CUBEMAP_NY	},
{ "pz",      IMG_CUBEMAP_PZ	},
{ "nz",      IMG_CUBEMAP_NZ	},
{ NULL,    0		}  // terminator
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
	size_t pic_size = sizeof( rgbdata_t ) + (width * height * 4);
	rgbdata_t	*pic = (rgbdata_t *)Mem_Alloc( pic_size );

	pic->buffer = ((byte *)pic) + sizeof( rgbdata_t ); 
	pic->size = (width * height * 4);
	pic->width = width;
	pic->height = height;

	return pic;
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
	size_t pic_size = sizeof( rgbdata_t ) + src->size;
	rgbdata_t	*dst = (rgbdata_t *)Mem_Alloc( pic_size );
	dst->buffer = ((byte *)dst) + sizeof( rgbdata_t );
	memcpy( dst->buffer, src->buffer, src->size );

	dst->size = src->size;
	dst->width = src->width;
	dst->height = src->height;
	dst->flags = src->flags;
	VectorCopy( src->reflectivity, dst->reflectivity );

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

void Image_PackRGB( float flColor[3], dword &icolor )
{
	byte	rgba[4];
	
	rgba[0] = LinearToTexture( flColor[0] );
	rgba[1] = LinearToTexture( flColor[1] );
	rgba[2] = LinearToTexture( flColor[2] );

	icolor = (0xFF << 24) | (rgba[2] << 16) | (rgba[1] << 8) | rgba[0];
}

void Image_UnpackRGB( dword icolor, float flColor[3] )
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

	pic = Image_Alloc( targa_header.width, targa_header.height );

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
		pixbuf = targa_rgba + ( rows - 1 ) * columns * 4;
		row_inc = -columns * 4 * 2;
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

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;

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
	int	columns, column, rows, row, bpp = 4;
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

	pic = Image_Alloc( columns, rows );

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
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
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

			pic->reflectivity[0] += TextureToLinear( red );
			pic->reflectivity[1] += TextureToLinear( green );
			pic->reflectivity[2] += TextureToLinear( blue );

			if(( red != green ) || ( green != blue ))
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
	const char	*comment = "Generated by XashNT MakeTex tool.\0";
	int		y, outsize, pixel_size = 4;
	const byte	*bufend, *in;
	byte		*buffer, *out;

	if( COM_FileExists( name ))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	if( pix->flags & IMAGE_HAS_ALPHA )
		outsize = pix->width * pix->height * 4 + 18 + Q_strlen( comment );
	else outsize = pix->width * pix->height * 3 + 18 + Q_strlen( comment );

	buffer = (byte *)Mem_Alloc( outsize );
	memset( buffer, 0, 18 );

	// prepare header
	buffer[0] = Q_strlen( comment ); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;
	buffer[16] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 32 : 24; // RGB pixel size
	buffer[17] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 8 : 0; // 8 bits of alpha

	Q_strncpy( (char *)(buffer + 18), comment, Q_strlen( comment )); 
	out = buffer + 18 + Q_strlen( comment );

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

	COM_SaveFile( name, buffer, outsize );
	Mem_Free( buffer );

	return true;
}

bool Image_SaveBMP( const char *name, rgbdata_t *pix )
{
	long		file;
	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmih;
	dword		cbBmpBits;
	byte		*pb, *pbBmpBits;
	dword		biTrueWidth;
	int		pixel_size;
	int		i, x, y;

	if( COM_FileExists( name ))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	if( FBitSet( pix->flags, IMAGE_HAS_ALPHA ))
		pixel_size = 4;
	else pixel_size = 3;

	COM_CreatePath( (char *)name );

	file = open( name, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666 );
	if( file < 0 ) return false;

	// NOTE: align transparency column will sucessfully removed
	// after create sprite or lump image, it's just standard requiriments 
	biTrueWidth = ((pix->width + 3) & ~3);
	cbBmpBits = biTrueWidth * pix->height * pixel_size;

	// Bogus file header check
	bmfh.bfType = MAKEWORD( 'B', 'M' );
	bmfh.bfSize = sizeof( bmfh ) + sizeof( bmih ) + cbBmpBits;
	bmfh.bfOffBits = sizeof( bmfh ) + sizeof( bmih );
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
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;

	// write info header
	write( file, &bmih, sizeof( bmih ));

	pbBmpBits = (byte *)Mem_Alloc( cbBmpBits );

	pb = pix->buffer;

	for( y = 0; y < bmih.biHeight; y++ )
	{
		i = (bmih.biHeight - 1 - y ) * (bmih.biWidth);

		for( x = 0; x < pix->width; x++ )
		{
			// 24 bit
			pbBmpBits[i*pixel_size+0] = pb[x*4+2];
			pbBmpBits[i*pixel_size+1] = pb[x*4+1];
			pbBmpBits[i*pixel_size+2] = pb[x*4+0];

			if( pixel_size == 4 ) // write alpha channel
				pbBmpBits[i*pixel_size+3] = pb[x*4+3];
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
=============
Image_SaveDDS
=============
*/
bool Image_SaveDDS( const char *name, rgbdata_t *pix )
{
	char	lumpname[64];
	rgbdata_t	*dds_image = NULL;

	if( COM_FileExists( name ))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	// check for easy out
	if( FBitSet( pix->flags, IMAGE_DXT_FORMAT ))
		return COM_SaveFile( name, pix->buffer, pix->size );

	COM_FileBase( name, lumpname );

	char hint = Image_HintFromSuf( lumpname );

	dds_image = BufferToDDS( pix, DDS_GetSaveFormatForHint( hint, pix ));
	if( !dds_image ) return false;

	bool result = COM_SaveFile( name, dds_image->buffer, dds_image->size );

	Mem_Free( dds_image );

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

	rgbdata_t *out = Image_Alloc( new_width, new_height );

	Image_Resample32Lerp( pic->buffer, pic->width, pic->height, out->buffer, out->width, out->height );

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

	if( !color ) return NULL;
	if( !alpha ) return color;

	if( FBitSet( color->flags, IMAGE_DXT_FORMAT ))
		return color; // can't merge compressed formats

	if( FBitSet( alpha->flags, IMAGE_DXT_FORMAT ))
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
		Mem_Free( images[i] ); // release original
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