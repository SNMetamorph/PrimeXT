/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

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

#define DEFAULT_FONT	"Arial"
#define QCHAR_WIDTH		16	// font width

bool	bItalic = false;
bool	bBold = false;
bool	bUnderline = false;
bool	bRussian = true;
bool	bMonotic = false;
char	fontname[256];
char	destfile[256];
int	pointsize[3] = { 9, 11, 15 };

byte char_table_ansi[96] =
{
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127
};

byte char_table_rus[164] =
{
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
127, 168, 169, 174, 184, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240,
241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
};

/*
=================
Draw_SetupConsolePalette

Set's the palette to full brightness ( 192 ) and 
set's up palette entry 0 -- black
=================
*/
void Draw_SetupConsolePalette( byte *pal )
{
	byte	*pPalette = pal;

	*(short *)pPalette = 256 * 3;
	pPalette += sizeof( short );

	// gradient palette
	for( int i = 0; i < 256; i++ )
	{
		pPalette[i*3+0] = i;
		pPalette[i*3+1] = i;
		pPalette[i*3+2] = i;
	}

	// set palette zero correctly
	pPalette[0] = 0;
	pPalette[1] = 0;
	pPalette[2] = 0;
}

/*
=================
CreateConsoleFont

Renders TT font into memory dc and creates appropriate qfont_t structure
=================
*/

// YWB:  Sigh, VC 6.0's global optimizer causes weird stack fixups in release builds.  Disable the globabl optimizer for this function.
// g-cont. At 2018 year this problem is actual!
#pragma optimize( "g", off )
qfont_t *CreateConsoleFont( const char *pszFont, int nPointSize, bool bItalic, bool bUnderline, bool bBold, bool bRussian, int *outsize )
{
	HDC		hdc, hmemDC;
	HBITMAP		hbm, oldbm;
	HFONT		fnt, oldfnt;
	int		startchar = 32;
	int		c, i, j, x, y;
	BITMAPINFO	tempbmi, *pbmi;
	BITMAPINFOHEADER	*pbmheader;
	byte		*pqdata, *pCur;
	int		x1, y1, nScans;
	byte		*pPalette, *bits;
	qfont_t		*pqf = NULL;
	int		symbols = (bRussian) ? sizeof( char_table_rus ) : sizeof( char_table_ansi );
	byte		*char_table = (bRussian) ? char_table_rus : char_table_ansi;
	int		fullsize;
	int		w = QCHAR_WIDTH;
	int		h = symbols / QCHAR_WIDTH; // !!!
	int		charheight = nPointSize + 5; // add some padding
	int		fontheight = h * charheight;
	int		charwidth = QCHAR_WIDTH;
	RECT		rc, rcChar;

	// Create the font
	fnt = CreateFont( -nPointSize, 0, 0, 0, bBold ? FW_HEAVY : FW_MEDIUM, bItalic, bUnderline, 0, RUSSIAN_CHARSET, OUT_TT_PRECIS,
	CLIP_DEFAULT_PRECIS, PROOF_QUALITY, VARIABLE_PITCH | FF_DONTCARE, pszFont );

	bits = NULL;

	// last sixty four bytes - what the hell ????
	fullsize = sizeof( qfont_t ) + ( fontheight * w * charwidth ) + sizeof( short ) + 768 + 64;

	// store off final size
	*outsize = fullsize;

	Msg( "%s ( 256 x %d )\n", pszFont, fontheight );

	pqf = ( qfont_t * )Mem_Alloc( fullsize );
	pqdata = (byte *)pqf + sizeof( qfont_t );
	pPalette = pqdata + ( fontheight * w * charwidth );

	// configure palette
	Draw_SetupConsolePalette( pPalette );

	hdc = GetDC( NULL );
	hmemDC = CreateCompatibleDC( hdc );
	rc.top = 0;
	rc.left = 0;
	rc.right = charwidth * w;
	rc.bottom	= charheight * h;

	hbm = CreateBitmap( charwidth * w, charheight * h, 1, 1, NULL ); 
	oldbm = (HBITMAP)SelectObject( hmemDC, hbm );
	oldfnt = (HFONT)SelectObject( hmemDC, fnt );
	SetTextColor( hmemDC, 0x00ffffff );
	SetBkMode( hmemDC, TRANSPARENT );

	// Paint black background
	FillRect( hmemDC, &rc, (HBRUSH)GetStockObject( BLACK_BRUSH ));
   
	// draw character set into memory DC
	for( j = 0; j < h; j++ )
	{
		for( i = 0; i < w; i++ )
		{
			x = i * charwidth;
			y = j * charheight;

			c = char_table[j * w + i];

			// Only draw printable characters, of course
			if( !Q_isspace( c ))
			{
				// draw it.
				rcChar.left = x + 1;
				rcChar.top = y + 1;
				rcChar.right = x + charwidth - 1;
				rcChar.bottom = y + charheight - 1;
				DrawText( hmemDC, (char *)&c, 1, &rcChar, DT_NOPREFIX|DT_LEFT );
			}
		}
	}

	// now turn the qfont into raw format
	memset( &tempbmi, 0, sizeof( BITMAPINFO ));
	pbmheader = ( BITMAPINFOHEADER * )&tempbmi;

	pbmheader->biSize = sizeof( BITMAPINFOHEADER );
	pbmheader->biWidth = w * charwidth; 
	pbmheader->biHeight	= -h * charheight; 
	pbmheader->biPlanes	= 1;
	pbmheader->biBitCount = 1;
	pbmheader->biCompression = BI_RGB;

	// find out how big the bitmap is
	nScans = GetDIBits( hmemDC, hbm, 0, h * charheight, NULL, &tempbmi, DIB_RGB_COLORS );

	// Allocate space for all bits
	pbmi = (BITMAPINFO *)Mem_Alloc( sizeof ( BITMAPINFOHEADER ) + 2 * sizeof( RGBQUAD ) + pbmheader->biSizeImage );
	memcpy( pbmi, &tempbmi, sizeof( BITMAPINFO ));
	bits = (byte *)pbmi + sizeof( BITMAPINFOHEADER ) + 2 * sizeof( RGBQUAD ); 

	// now read in bits
	nScans = GetDIBits( hmemDC, hbm, 0, h * charheight, bits, pbmi, DIB_RGB_COLORS );

	if( nScans > 0 )
	{
		// Now convert to proper raw format
		//
		// now get results from dib
		pqf->height = fontheight;
		pqf->width = charwidth;
		pqf->rowheight = charheight;
		pqf->rowcount = h;
		pCur = pqdata;
		
		// Set everything to index 255 ( 0xff ) == transparent
		memset( pCur, 0xFF, w * charwidth * pqf->height );

		for( j = 0; j < h; j++ )
		{
			for ( i = 0; i < w; i++ )
			{
				int edge = 1;
				int bestwidth;
				x = i * charwidth;
				y = j * charheight;

				c = (byte)char_table[j * w + i];
				int startoffset = y * w * charwidth + x;

				if( startoffset > 0xFFFF )
				{
					COM_FatalError( "\"%s\" with pointsize %d is greater than 64K ( %s )\n",
					pszFont, nPointSize, Q_memprint( startoffset ));
				}

				pqf->fontinfo[c].charwidth = charwidth;
				pqf->fontinfo[c].startoffset = startoffset;
				bestwidth = 0;

				// In this first pass, place the black drop shadow so characters draw ok in the engine against
				//  most backgrounds.
				// YWB:  FIXME, apply a box filter and enable blending?
				// Make it all transparent for starters
				for( y1 = 0; y1 < charheight; y1++ )
				{
					for( x1 = 0; x1 < charwidth; x1++ )
					{
						int offset = ( y + y1 ) * w * charwidth + x + x1;
						// dest
						pCur = pqdata + offset;
						// assume transparent
						pCur[0] = 255;
					}
				}

				// put black pixels below and to the right of each pixel
				for( y1 = edge; y1 < charheight - edge; y1++ )
				{
					for( x1 = 0; x1 < charwidth; x1++ )
					{
						int offset;
						int srcoffset;
						int xx0, yy0;

						offset = ( y + y1 ) * w * charwidth + x + x1;

						// Dest
						pCur = pqdata + offset;

						for( xx0 = -edge; xx0 <= edge; xx0++ )
						{
							for( yy0 = -edge; yy0 <= edge; yy0++ )
							{
								srcoffset = ( y + y1 + yy0 ) * w * charwidth + x + x1 + xx0;

								if( bits[ srcoffset >> 3 ] & ( 1 << ( 7 - srcoffset & 7 )))
								{
									// near black
									pCur[0] = 32;
								}
							}
						}
					}
				}

				// now copy in the actual font pixels
				for( y1 = 0; y1 < charheight; y1++ )
				{
					for( x1 = 0; x1 < charwidth; x1++ )
					{
						int offset = ( y + y1 ) * w * charwidth + x + x1;

						// dest
						pCur = pqdata + offset;

						if( bits[ offset >> 3 ] & ( 1 << ( 7 - offset & 7 )))
						{
							if( x1 > bestwidth )
							{
								bestwidth = x1;
							}

							// Full color
							// FIXME:  Enable true palette support in engine?
							pCur[0] = 192;
						}
					}
				}

				// Space character width
				if( c == 32 )
				{
					bestwidth = 8;
				}
				else
				{
					// small characters needs can be padded a bit so they don't run into each other
					if( bestwidth <= 14 )
						bestwidth += 2;
				}
				
				// Store off width
				if( !bMonotic ) pqf->fontinfo[c].charwidth = bestwidth;
				else pqf->fontinfo[c].charwidth = min( nPointSize - 2, charwidth );
			}
		}
	}

	// free memory bits
	Mem_Free( pbmi );
	SelectObject( hmemDC, oldfnt );
	DeleteObject( fnt );
	SelectObject( hmemDC, oldbm );
	DeleteObject( hbm );
	DeleteDC( hmemDC );
	ReleaseDC( NULL, hdc );

	return pqf;
}
#pragma optimize( "g", on )

/*
=================
main

=================
*/
int main( int argc, char* argv[] )
{
	bool	srcset = false;
	double	start, end;
	int	outsize[3];
	wfile_t	*fonts_wad;
	qfont_t	*fonts[3];
	char	str[64];
	int	i;

	Q_strncpy( fontname, DEFAULT_FONT, sizeof( fontname ));
	Q_strncpy( destfile, "fonts.wad", sizeof( destfile ));

	Msg( "		P2:Savior Font Generator\n" );
	Msg( "		 XashXT Group 2018(^1c^7)\n\n\n" );

	for( i = 1; i < argc; i++ )
	{
		if( !Q_strcmp( argv[i], "-font" ))
		{
			Q_strncpy( fontname, argv[i+1], sizeof( fontname ));
			i++;
		}
		else if( !Q_strcmp( argv[i], "-pointsizes" ))
		{
			if( i + 3 >= argc )
				COM_FatalError( "insufficient point sizes specified\n" );
			pointsize[0] = atoi( argv[i+1] );
			pointsize[1] = atoi( argv[i+2] );
			pointsize[2] = atoi( argv[i+3] );
			i += 3;
		}
		else if( !Q_strcmp( argv[i], "-italic" ))
		{
			bItalic = true;
		}
		else if( !Q_strcmp( argv[i], "-bold" ))
		{
			bBold = true;
		}
		else if( !Q_strcmp( argv[i], "-underline" ))
		{
			bUnderline = true;
		}
		else if( !Q_strcmp( argv[i], "-ansi" ))
		{
			bRussian = false;
		}
		else if( !Q_strcmp( argv[i], "-monotic" ))
		{
			bMonotic = true;
		}
		else if( !srcset )
		{
			Q_strncpy( destfile, argv[i], sizeof( destfile ));
			srcset = true;
		}
		else
		{
			Msg( "makefont: unknown option %s\n", argv[i] );
			break;
		}
	}

	if( i != argc )
	{
		Msg( "Usage: makefont.exe -font \"fontname\" <destfile> <options>\n"
		"\nlist options:\n"
		"^2-italic^7 - make italic style\n"
		"^2-bold^7 - make bold style\n"
		"^2-underline^7 - underline characters\n"
		"^2-monotic^7 - constant font widths\n"
		"^2-pointsizes^7 0 1 2 - char sizes for font0, font1 and font2\n"
		"^2-ansi^7 - disable russian charset\n"
		"\t\tPress any key to exit" );

		system( "pause>nul" );
		return 1;
	}

	Msg( "Creating %i, %i, and %i point %s fonts\n", pointsize[0], pointsize[1], pointsize[2], fontname );

	start = I_FloatTime();

	// create the fonts
	for( i = 0; i < 3; i++ )
	{
		fonts[i] = CreateConsoleFont( fontname, pointsize[i], bItalic, bUnderline, bBold, bRussian, &outsize[i] );
	}

	// Create wad file
	fonts_wad = W_Open( destfile, "wb" );
	if( !fonts_wad ) COM_FatalError( "couldn't create %s\n", destfile );

	// add fonts as lumps
	for( i = 0; i < 3; i++ )
	{
		Q_snprintf( str, sizeof( str ), "font%i", i );

		if( W_SaveLump( fonts_wad, str, fonts[i], outsize[i], TYP_QFONT, ATTR_NONE ) == -1 )
			COM_FatalError( "couldn't write '%s.fnt'\n", str );
	}

	// store results as a WAD3
	W_Close( fonts_wad );

	// clean up memory
	for( i = 0; i < 3; i++ )
		Mem_Free( fonts[i] );

	end = I_FloatTime();

	Q_timestring((int)(end - start), str );
	MsgDev( D_INFO, "%s elapsed\n", str );
	
	// display for a second since it might not be running from command prompt
	Sleep( 1000 );

	return 0;
}