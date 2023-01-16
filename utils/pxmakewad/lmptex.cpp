/*
lmptex.cpp - prepare and store lmptex into the wad
Copyright (C) 2018 Uncle Mike

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
#include "cmdlib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "imagelib.h"
#include "makewad.h"
#include "miptex.h"
#include "port.h"
#include <math.h>

bool LMP_WriteLmptex( const char *lumpname, rgbdata_t *pix, bool todisk )
{
	bool	result;
	lmp_t	*lmp;

	// check for all the possible problems
	if (!pix || !FBitSet(pix->flags, IMAGE_QUANTIZED)) 
	{
		Msg(S_ERROR "image not quantized or buffer invalid\n");
		return false;
	}

	// lmp may have any dimensions
	if (pix->width < IMAGE_MINWIDTH || pix->width > IMAGE_MAXWIDTH || pix->height < IMAGE_MINHEIGHT || pix->height > IMAGE_MAXHEIGHT)
	{
		Msg(S_ERROR "image too small or too large\n");
		return false; // to small or too large
	}

	// calculate gamma corrected linear palette
	for( int i = 0; i < 256; i++ )
	{
		// setup palette
		lbmpalette[i*3+0] = pix->palette[i*4+0];
		lbmpalette[i*3+1] = pix->palette[i*4+1];
		lbmpalette[i*3+2] = pix->palette[i*4+2];
	}

	size_t pixels = pix->width * pix->height;
	size_t lumpsize = sizeof( lmp_t ) + pixels + sizeof( short ) + 768;
	byte *lumpbuffer, *lump_p;

	// all the lumps must be aligned by 4
	// or Wally will stop working properly
	lumpsize = (lumpsize + 3) & ~3;

	lumpbuffer = lump_p = (byte *)Mem_Alloc( lumpsize );
	lmp = (lmp_t *)lumpbuffer;

	// fill the header
	lmp->width = pix->width;
	lmp->height = pix->height;
	lump_p += sizeof( lmp_t );

	// write lmptex
	memcpy( lump_p, pix->buffer, pixels );
	lump_p += pixels;

	// write out palette
	*(unsigned short *)lump_p = 256; // palette size
	lump_p += sizeof( short );

	memcpy( lump_p, lbmpalette, 768 );
	lump_p += 768;

	size_t disksize = (( lump_p - lumpbuffer ) + 3) & ~3;

	if (lumpsize != disksize) {
		MsgDev(D_ERROR, "%s is corrupted (buffer is %s bytes, written %s)\n", lumpname, Q_memprint(lumpsize), Q_memprint(disksize));
	}

	if( todisk ) result = COM_SaveFile( lumpname, lumpbuffer, lumpsize );
	else result = W_SaveLump( output_wad, lumpname, lumpbuffer, lumpsize, TYP_GFXPIC, ATTR_NONE ) >= 0;

	Mem_Free( lumpbuffer );

	return result;
}

bool LMP_CheckForReplace( dlumpinfo_t *find, rgbdata_t *image, int &width, int &height )
{
	// NOTE: we can replace this lump but this is unsafe
	if( find != NULL )
	{
		size_t lumpsize = ((sizeof( lmp_t ) + ( width * height ) + sizeof( short ) + 768) + 3) & ~3;

		switch( GetReplaceLevel( ))
		{
		case REP_IGNORE:
			Msg(S_ERROR "%s already exists\n", find->name); 
			Image_Free(image);
			return false;
		case REP_NORMAL:
			if( FBitSet( find->attribs, ATTR_READONLY ))
			{
				// g-cont. i left this limitation as a protect of the replacement of compressed lumps
				Msg(S_ERROR "%s is read-only\n", find->name);
				Image_Free( image );
				return false;
			}
			if( lumpsize != find->size )
			{
				Msg(S_ERROR "%s.lmp [%s] should be [%s]\n",
				find->name, Q_memprint( lumpsize ), Q_memprint( find->size )); 
				Image_Free( image );
				return false;
			}
			break;
		case REP_FORCE:
			if( lumpsize != find->size )
			{
				size_t oldpos;
				lmp_t test;

				oldpos = tell( W_GetHandle( output_wad ) ); // don't forget restore original position

				if( lseek( W_GetHandle( output_wad ), find->filepos, SEEK_SET ) == -1 )
				{
					Msg(S_ERROR "%s is corrupted\n", find->name);
					lseek( W_GetHandle( output_wad ), oldpos, SEEK_SET );
					Image_Free( image );
					return false;
				}

				if( read( W_GetHandle( output_wad ), &test, sizeof( test )) != sizeof( test ))
				{
					Msg(S_ERROR "%s is corrupted\n", find->name);
					lseek( W_GetHandle( output_wad ), oldpos, SEEK_SET );
					Image_Free( image );
					return false;
				}

				// get new sizes from the saved lump to resample current
				lseek( W_GetHandle( output_wad ), oldpos, SEEK_SET );
				height = test.height;
				width = test.width;
			}
			break;
		}
	}

	return true;
}
