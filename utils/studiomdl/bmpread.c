#include <windows.h>
#include <STDIO.H>

int LoadBMP (const char* szFile, byte** ppbBits, byte** ppbPalette, int *width, int *height )
{
	int i, rc = 0;
	FILE *pfile = NULL;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	ULONG cbBmpBits;
	BYTE* pbBmpBits;
	byte  *pb, *pbPal = NULL;
	ULONG cbPalBytes;
	ULONG biTrueWidth;

	// Bogus parameter check
	if (!(ppbPalette != NULL && ppbBits != NULL))
		{ fprintf(stderr, "invalid BMP file\n"); rc = -1000; goto GetOut; }

	// File exists?
	if ((pfile = fopen(szFile, "rb")) == NULL)
		{ fprintf(stderr, "unable to open BMP file\n"); rc = -1; goto GetOut; }
	
	// Read file header
	if (fread(&bmfh, sizeof bmfh, 1/*count*/, pfile) != 1)
		{ rc = -2; goto GetOut; }

	// Bogus file header check
	if (!(bmfh.bfReserved1 == 0 && bmfh.bfReserved2 == 0))
		{ rc = -2000; goto GetOut; }

	// Read info header
	if (fread(&bmih, sizeof bmih, 1/*count*/, pfile) != 1)
		{ rc = -3; goto GetOut; }

	// Bogus info header check
	if (!(bmih.biSize == sizeof bmih && bmih.biPlanes == 1))
		{ fprintf(stderr, "invalid BMP file header\n");  rc = -3000; goto GetOut; }

	// Bogus bit depth?  Only 8-bit supported.
	if (bmih.biBitCount != 8)
		{ fprintf(stderr, "BMP file not 8 bit\n");  rc = -4; goto GetOut; }
	
	// Bogus compression?  Only non-compressed supported.
	if (bmih.biCompression != BI_RGB)
		{ fprintf(stderr, "invalid BMP compression type\n"); rc = -5; goto GetOut; }

	// Figure out how many entires are actually in the table
	if (bmih.biClrUsed == 0)
		{
		bmih.biClrUsed = 256;
		cbPalBytes = (1 << bmih.biBitCount) * sizeof( RGBQUAD );
		}
	else 
		{
		cbPalBytes = bmih.biClrUsed * sizeof( RGBQUAD );
		}

	// Read palette (bmih.biClrUsed entries)
	if (fread(rgrgbPalette, cbPalBytes, 1/*count*/, pfile) != 1)
		{ rc = -6; goto GetOut; }

	// convert to a packed 768 byte palette
	pbPal = malloc(768);
	if (pbPal == NULL)
		{ rc = -7; goto GetOut; }

	pb = pbPal;

	// Copy over used entries
	for (i = 0; i < (int)bmih.biClrUsed; i++)
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
	}

	// Fill in unused entires will 0,0,0
	for (i = bmih.biClrUsed; i < 256; i++) 
	{
		*pb++ = 0;
		*pb++ = 0;
		*pb++ = 0;
	}

	// Read bitmap bits (remainder of file)
	cbBmpBits = bmfh.bfSize - ftell(pfile);
	pb = malloc(cbBmpBits);
	if (fread(pb, cbBmpBits, 1/*count*/, pfile) != 1)
		{ rc = -7; goto GetOut; }

	pbBmpBits = malloc(cbBmpBits);

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = (bmih.biWidth + 3) & ~3;
	
	// reverse the order of the data.
	pb += (bmih.biHeight - 1) * biTrueWidth;
	for(i = 0; i < bmih.biHeight; i++)
	{
		memmove(&pbBmpBits[biTrueWidth * i], pb, biTrueWidth);
		pb -= biTrueWidth;
	}

	pb += biTrueWidth;
	free(pb);

	*width = (WORD)bmih.biWidth;
	*height = (WORD)bmih.biHeight;
	// Set output parameters
	*ppbPalette = pbPal;
	*ppbBits = pbBmpBits;

GetOut:
	if (pfile) 
		fclose(pfile);

	return rc;
}
