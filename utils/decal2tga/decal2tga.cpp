/*
decal2tga.cpp - convert Half-Life decals into TGA decals
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <basetypes.h>
#include <conprint.h>
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <stdio.h>
#include <io.h>
#include <filesystem.h>
#include <wfile.h>
#include <cmdlib.h>
#include <stringlib.h>

struct
{
	unsigned int	width;
	unsigned int	height;
	unsigned long	table[256];
	unsigned char	*pixels;
	unsigned char	*pixels32;	// expanded
	size_t		size;
} image;

typedef struct
{
	byte	r, g, b, a;
} color32;

wfile_t	*source_wad = NULL;	// input WAD3 file
int	g_numdecals = 0;

/*
=================
BoxFilter3x3

box filter 3x3
=================
*/
void BoxFilter3x3( byte *out, const byte *in, int w, int h, int x, int y )
{
	int		r = 0, g = 0, b = 0, a = 0;
	int		count = 0, acount = 0;
	int		i, j, u, v;
	const byte	*pixel;

	for( i = 0; i < 3; i++ )
	{
		u = ( i - 1 ) + x;

		for( j = 0; j < 3; j++ )
		{
			v = ( j - 1 ) + y;

			if( u >= 0 && u < w && v >= 0 && v < h )
			{
				pixel = &in[( u + v * w ) * 4];

				if( pixel[3] != 0 )
				{
					r += pixel[0];
					g += pixel[1];
					b += pixel[2];
					a += pixel[3];
					acount++;
				}
			}
		}
	}

	if(  acount == 0 )
		acount = 1;

	out[0] = r / acount;
	out[1] = g / acount;
	out[2] = b / acount;
//	out[3] = (int)( SimpleSpline( ( a / 12.0f ) / 255.0f ) * 255 );
}

/*
=============
WriteTGAFile

=============
*/
bool WriteTGAFile( const char *filename, bool write_alpha )
{
	size_t		filesize;
	const unsigned char	*bufend, *in;
	unsigned char	*buffer, *out;
	const char	*comment = "Written by decal2tga tool\0";

	if( write_alpha )
		filesize = image.width * image.height * 4 + 18 + strlen( comment );
	else filesize = image.width * image.height * 3 + 18 + strlen( comment );

	buffer = (unsigned char *)malloc( filesize );
	memset( buffer, 0, 18 );

	// prepare header
	buffer[0] = strlen( comment ); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (image.width >> 0) & 0xFF;
	buffer[13] = (image.width >> 8) & 0xFF;
	buffer[14] = (image.height >> 0) & 0xFF;
	buffer[15] = (image.height >> 8) & 0xFF;
	buffer[16] = write_alpha ? 32 : 24;
	buffer[17] = write_alpha ? 8 : 0; // 8 bits of alpha
	strncpy((char *)buffer + 18, comment, strlen( comment )); 
	out = buffer + 18 + strlen( comment );

	// swap rgba to bgra and flip upside down
	for( int y = image.height - 1; y >= 0; y-- )
	{
		in = image.pixels32 + y * image.width * 4;
		bufend = in + image.width * 4;
		for( ; in < bufend; in += 4 )
		{
			*out++ = in[2];
			*out++ = in[1];
			*out++ = in[0];
			if( write_alpha )
				*out++ = in[3];
		}
	}

	// make sure what path is existing
	for( char *ofs = (char *)(filename + 1); *ofs; ofs++ )
	{
		if( *ofs == '/' || *ofs == '\\' )
		{
			// create the directory
			char save = *ofs;
			*ofs = 0;
			_mkdir( filename );
			*ofs = save;
		}
	}

	long handle = open( filename, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666 );
	if( handle < 0 )
	{
		MsgDev( D_ERROR, "couldn't write %s\n", filename );
		free( buffer );
		return false;
          }

	size_t size = write( handle, buffer, filesize );
	close( handle );
	free( buffer );

	if( size < 0 )
	{
		MsgDev( D_ERROR, "couldn't write %s\n", filename );
		return false;
	}

	return true;
}

bool WriteBMPFile( const char *name, bool write_alpha )
{
	long		file;
	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmih;
	dword		cbBmpBits;
	byte		*pb, *pbBmpBits;
	dword		biTrueWidth;
	int		pixel_size;
	int		i, x, y;

	if( write_alpha )
		pixel_size = 4;
	else pixel_size = 3;

	file = open( name, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666 );
	if( file < 0 ) return false;

	// NOTE: align transparency column will sucessfully removed
	// after create sprite or lump image, it's just standard requiriments 
	biTrueWidth = ((image.width + 3) & ~3);
	cbBmpBits = biTrueWidth * image.height * pixel_size;

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
	bmih.biHeight = image.height;
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

	pb = image.pixels32;

	for( y = 0; y < bmih.biHeight; y++ )
	{
		i = (bmih.biHeight - 1 - y ) * (bmih.biWidth);

		for( x = 0; x < image.width; x++ )
		{
			// 24 bit
			pbBmpBits[i*pixel_size+0] = pb[x*4+2];
			pbBmpBits[i*pixel_size+1] = pb[x*4+1];
			pbBmpBits[i*pixel_size+2] = pb[x*4+0];

			if( pixel_size == 4 ) // write alpha channel
				pbBmpBits[i*pixel_size+3] = pb[x*4+3];
			i++;
		}

		pb += image.width * 4;
	}

	// write bitmap bits (remainder of file)
	write( file, pbBmpBits, cbBmpBits );
	close( file );
	Mem_Free( pbBmpBits );

	return true;
}

bool ConvertDecal( const dlumpinfo_t *lump )
{
	byte *buffer = W_LoadLump( source_wad, lump->name, NULL, lump->type );
	mip_t *decal = (mip_t *)buffer;

	if( lump->disksize < sizeof( mip_t ))
	{
		MsgDev( D_ERROR, "ConvertDecal: file (%s) have invalid size\n", lump->name );
		Mem_Free( buffer, C_FILESYSTEM );
		return false;
	}

	if( decal->width <= 0 || decal->width > 1024 || decal->height <= 0 || decal->height > 1024 )
	{
		MsgDev( D_ERROR, "ConvertDecal: %s has invalid sizes %i x %i\n", lump->name, decal->width, decal->height );
		Mem_Free( buffer, C_FILESYSTEM );
		return false;
	}

	image.width = decal->width;
	image.height = decal->height;
	image.size = image.width * image.height;

	image.pixels = (unsigned char *)(buffer + decal->offsets[0]);
	unsigned char *pal = (byte *)(buffer + decal->offsets[0] + (((image.width * image.height) * 85)>>6));

	short numcolors = *(short *)pal;
	if( numcolors != 256 )
	{
		MsgDev( D_ERROR, "ConvertDecal: %s has invalid palette size %i should be 256\n", lump->name, numcolors );
		Mem_Free( buffer, C_FILESYSTEM );
		return false;
	}

	pal += sizeof( short ); // skip colorsize 
	bool gradient = true;
	byte *in, pal32[4];
	char filename[64];
	byte base = 128;

	// clear blue color for 'transparent' decals
	if( pal[255*3+0] == 0 && pal[255*3+1] == 0 && pal[255*3+2] == 255 )
	{
		pal[255*3+0] = pal[255*3+1] = pal[255*3+2] = 0;
		gradient = false;
	}

	// convert 24-bit palette into 32-bit palette	
	for( int i = 0; i < 256; i++ )
	{
		if( gradient )
		{
			pal32[0] = pal[765];
			pal32[1] = pal[766];
			pal32[2] = pal[767];
			pal32[3] = i;
		}
		else
		{
			pal32[0] = pal[i*3+0];
			pal32[1] = pal[i*3+1];
			pal32[2] = pal[i*3+2];
			pal32[3] = 0xFF;
		}
		image.table[i] = *(unsigned long *)pal32;
	}

	if( !gradient ) image.table[255] = 0; // last color is alpha

	// expand image into 32-bit buffer
	image.pixels32 = in = (byte *)malloc( image.size * 4 );

	for( i = 0; i < image.size; i++ )
	{
		color32 *color = (color32 *)&image.table[image.pixels[i]];

		if( gradient )
		{
			float lerp = 1.0 - ((float)color->a / 255.0f);

			image.pixels32[i*4+0] = color->r + (base - color->r) * lerp;
			image.pixels32[i*4+1] = color->g + (base - color->g) * lerp;
			image.pixels32[i*4+2] = color->b + (base - color->b) * lerp;
			image.pixels32[i*4+3] = color->a;
		}
		else
		{
			image.pixels32[i*4+0] = color->r;
			image.pixels32[i*4+1] = color->g;
			image.pixels32[i*4+2] = color->b;
			image.pixels32[i*4+3] = color->a;
		}
	}
#if 1
	// apply boxfilter to 1-bit alpha
	for( i = 0; !gradient && i < image.size; i++, in += 4 )
	{
		if( in[0] == 0 && in[1] == 0 && in[2] == 0 && in[3] == 0 )
			BoxFilter3x3( in, image.pixels32, image.width, image.height, i % image.width, i / image.width );
	}
#endif
	Q_snprintf( filename, sizeof( filename ), "decals/%s.tga", lump->name );
	bool bResult = WriteTGAFile( filename, true );
	Mem_Free( buffer, C_FILESYSTEM );
	free( image.pixels32 );

	return bResult;
}

bool ConvertDecals( const char *wadname )
{
	if(( source_wad = W_Open( wadname, "rb" )) == NULL )
		return false;

	dlumpinfo_t *lump_p;
	int i;

	for( i = 0, lump_p = source_wad->lumps; i < source_wad->numlumps; i++, lump_p++ )
	{
		if( lump_p->type != TYP_MIPTEX || lump_p->name[0] != '{' )
			continue;	// not a decal

		if( !ConvertDecal( lump_p ))
		{
			MsgDev( D_WARN, "can't convert decal %s\n", lump_p->name );
			continue;
		}

		// decal sucessfully converted
		g_numdecals++;
	}

	W_Close( source_wad );
	return true;
}

int main( int argc, char **argv )
{
	if( !ConvertDecals( "decals.wad" ))
	{
		// let the user read error
		system( "pause>nul" );
		return 1;
          }

	Msg( "%i decals converted\n", g_numdecals );
	return 0;
}