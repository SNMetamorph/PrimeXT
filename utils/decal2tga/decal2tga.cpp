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

#include "waddesc.h"
#include "conprint.h"
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <stdio.h>
#include <io.h>

#pragma warning(disable : 4244)	// MIPS

struct
{
	unsigned char	*base;
	dlumpinfo_t	*lumps;
	int		numlumps;
} wad;

struct
{
	unsigned int	width;
	unsigned int	height;
	unsigned long	table[256];
	unsigned char	*pixels;
	unsigned char	*pixels32;	// expanded
	size_t		size;
} image;

int	g_numdecals = 0;

/*
==================
CleanupName

Lowercases name and pads with spaces and a terminating 0 to the length of
lumpinfo_t->name.
Used so lumpname lookups can proceed rapidly by comparing 4 chars at a time
Space padding is so names can be printed nicely in tables.
Can safely be performed in place.
==================
*/
void CleanupName( const char *in, char *out )
{
	int i;

	for( i = 0; i < 16; i++ )
	{
		int c = in[i];
		if( !c ) break;
			
		if( c >= 'A' && c <= 'Z' )
			c += ( 'a' - 'A' );
		out[i] = c;
	}
	
	for( ; i < 16; i++ )
		out[i] = 0;
}

/*
============
ExpandIndexedImageToRGBA

============
*/
void ExpandIndexedImageToRGBA( const unsigned char *in, unsigned char *out, int pixels )
{
	unsigned long *iout = (unsigned long *)out;
	unsigned char *fin = (unsigned char *)in;

	while( pixels >= 8 )
	{
		iout[0] = image.table[in[0]];
		iout[1] = image.table[in[1]];
		iout[2] = image.table[in[2]];
		iout[3] = image.table[in[3]];
		iout[4] = image.table[in[4]];
		iout[5] = image.table[in[5]];
		iout[6] = image.table[in[6]];
		iout[7] = image.table[in[7]];

		in += 8;
		iout += 8;
		pixels -= 8;
	}

	if( pixels & 4 )
	{
		iout[0] = image.table[in[0]];
		iout[1] = image.table[in[1]];
		iout[2] = image.table[in[2]];
		iout[3] = image.table[in[3]];

		in += 4;
		iout += 4;
	}

	if( pixels & 2 )
	{
		iout[0] = image.table[in[0]];
		iout[1] = image.table[in[1]];

		in += 2;
		iout += 2;
	}

	if( pixels & 1 ) // last byte
		iout[0] = image.table[in[0]];
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
		Con_Printf( "^1Error:^7 couldn't write %s\n", filename );
		free( buffer );
		return false;
          }

	size_t size = write( handle, buffer, filesize );
	close( handle );
	free( buffer );

	if( size < 0 )
	{
		Con_Printf( "^1Error:^7 couldn't write %s\n", filename );
		return false;
	}

	return true;
}

bool LoadWadFile( const char *wadname )
{
	long handle = open( wadname, O_RDONLY|O_BINARY, 0x666 );

	if( handle < 0 )
	{
		Con_Printf( "^1Error:^7 couldn't open %s\n", wadname );
		return false;
	}

	size_t size = lseek( handle, 0, SEEK_END );
	lseek( handle, 0, SEEK_SET );

	wad.base = (unsigned char *)malloc( size + 1 );
	wad.base[size] = '\0';
	read( handle, (unsigned char *)wad.base, size );
	close( handle );

	dlumpinfo_t	*lump_p;
	dwadinfo_t	*header;

	header = (dwadinfo_t *)wad.base;

	if( header->ident != IDWAD3HEADER )
	{
		Con_Printf( "^1Error:^7 %s it's not a wad-file\n", wadname );
		free( wad.base );
		return false;
	}	

	wad.numlumps = header->numlumps;
	wad.lumps = (dlumpinfo_t *)(wad.base + header->infotableofs);
	int i;

	for( i = 0, lump_p = wad.lumps; i < wad.numlumps; i++, lump_p++ )
		CleanupName( lump_p->name, lump_p->name );

	return true;
}

void FreeWadFile( void )
{
	if( !wad.base ) return;
	free( wad.base );
	memset( &wad, 0, sizeof( wad ));
}

bool ConvertDecal( const dlumpinfo_t *lump )
{
	unsigned char *buffer = (unsigned char *)(wad.base + lump->filepos);
	mip_t *decal = (mip_t *)buffer;

	if( lump->size < sizeof( mip_t ))
	{
		Con_Printf( "^1Error:^7 ConvertDecal: file (%s) have invalid size\n", lump->name );
		return false;
	}

	if( decal->width <= 0 || decal->width > 1024 || decal->height <= 0 || decal->height > 1024 )
	{
		Con_Printf( "^1Error:^7 ConvertDecal: %s has invalid sizes %i x %i\n", lump->name, decal->width, decal->height );
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
		Con_Printf( "^1Error:^7 ConvertDecal: %s has invalid palette size %i should be 256\n", lump->name, numcolors );
		return false;
	}

	pal += sizeof( short ); // skip colorsize 
	unsigned char entry[4];
	float flentry1[4], flentry2[4];

	// clear blue color for 'transparent' decals
	if( pal[255*3+0] == 0 && pal[255*3+1] == 0 && pal[255*3+2] == 255 )
		pal[255*3+0] = pal[255*3+1] = pal[255*3+2] = 0;

	// convert 24-bit palette into 32-bit palette	
	for( int i = 0; i < 256; i++ )
	{
		flentry1[0] = ((float)i / 255.0f) + 0.5f;
		flentry1[1] = ((float)i / 255.0f) + 0.5f;
		flentry1[2] = ((float)i / 255.0f) + 0.5f;
		flentry1[3] = ((float)i / 255.0f);

		flentry2[0] = (flentry1[3] * ((float)pal[765] / 255.0f)) + (flentry1[0] * (1.0f - flentry1[3]));
		flentry2[1] = (flentry1[3] * ((float)pal[766] / 255.0f)) + (flentry1[1] * (1.0f - flentry1[3]));
		flentry2[2] = (flentry1[3] * ((float)pal[767] / 255.0f)) + (flentry1[2] * (1.0f - flentry1[3]));
		flentry2[3] = 0.5f;

		float max;

		if( flentry2[0] > flentry2[1] )
			max = flentry2[0];
		else max = flentry2[1];
		
		if( flentry2[2] > max )
			max = flentry2[1];

		if( max > 1.0f )
		{
			float t = 1.0F / max;
			flentry2[0] *= t;
			flentry2[1] *= t;
			flentry2[2] *= t;
		}

		entry[0] = flentry2[0] * 255;
		entry[1] = flentry2[1] * 255;
		entry[2] = flentry2[2] * 255;
		entry[3] = 0;
		image.table[i] = *(unsigned long *)entry;
	}

	// expand image into 32-bit buffer
	image.pixels32 = (unsigned char *)malloc( image.size * 4 );
	ExpandIndexedImageToRGBA( image.pixels, image.pixels32, image.size );
	char filename[64];

	sprintf( filename, "decals/%s.tga", lump->name );
	bool bResult = WriteTGAFile( filename, false );
	free( image.pixels32 );

	return bResult;
}

bool ConvertDecals( const char *wadname )
{
	if( !LoadWadFile( wadname ))
		return false;

	dlumpinfo_t *lump_p;
	int i;

	for( i = 0, lump_p = wad.lumps; i < wad.numlumps; i++, lump_p++ )
	{
		if( lump_p->type != TYP_MIPTEX || lump_p->name[0] != '{' )
			continue;	// not a decal

		if( lump_p->compression != CMP_NONE )
		{
			Con_Printf( "^3Warning:^7 lump %s has unsupported compression method. Ignored\n", lump_p->name );
			continue;
		}

		if( !ConvertDecal( lump_p ))
		{
			Con_Printf( "^3Warning:^7 can't convert decal %s\n", lump_p->name );
			continue;
		}

		// decal sucessfully converted
		g_numdecals++;
	}

	FreeWadFile();
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

	Con_Printf( "%i decals converted\n", g_numdecals );
	return 0;
}
