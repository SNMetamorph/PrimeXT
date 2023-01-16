/*
image_dds.cpp - image dds encoder. Based on original code from Doom3: BFG Edition
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#define STB_DXT_IMPLEMENTATION

#include <math.h>
#include <assert.h>
#include "conprint.h"
#include "port.h"
#include "cmdlib.h"
#include "stringlib.h"
#include "imagelib.h"
#include "stringlib.h"
#include "filesystem.h"
#include "ddstex.h"
#include "squish.h"
#include "mathlib.h"

#define BLOCK_SIZE			( 4 * 4 )	// DXT block size quad 4x4 pixels
#define RGB_TO_YCOCG_Y( r, g, b )	(((  r +   (g<<1) +  b     ) + 2 ) >> 2 )
#define RGB_TO_YCOCG_CO( r, g, b )	((( (r<<1)-(b<<1) ) + 2 ) >> 2 )
#define RGB_TO_YCOCG_CG( r, g, b )	((( -r +   (g<<1) -  b     ) + 2 ) >> 2 )

typedef enum
{
	PF_DXT1 = 0,	// without alpha
	PF_DXT3,		// DXT3
	PF_DXT5,		// DXT5 as default
	PF_DXT5_ALPHA,	// DXT5 with alpha
	PF_DXT5_SDF_ALPHA,	// DXT5 with SDF alpha
	PF_DXT5_YCoCg,
	PF_DXT5_NORM_BASE,	// same as original
	PF_ATI2_NORM_PARABOLOID,
} saveformat_t;

/*
========================
ExtractBlock

params: inPtr	- input image, 4 bytes per pixel
paramO: colorBlock	- 4*4 output tile, 4 bytes per pixel
========================
*/
inline static void ExtractBlock( const byte *inPtr, int width, byte *colorBlock )
{
	for( int i = 0; i < 4; i++ )
	{
		memcpy( &colorBlock[i*BLOCK_SIZE], inPtr, BLOCK_SIZE );
		inPtr += width * 4;
	}
}

static size_t Image_DXTGetBlockSize( int format )
{
	if( format == PF_DXT1 )
		return 8;
	return 16;
}

/*
========================
ScaleYCoCg

params: colorBlock	- 16 pixel block for which find color indexes
========================
*/
inline static void ScaleYCoCg( byte *colorBlock )
{
	for( int i = 0; i < 16; i++ )
	{
		int r = colorBlock[i*4+0];
		int g = colorBlock[i*4+1];
		int b = colorBlock[i*4+2];
		int a = colorBlock[i*4+3];

		const int Co = r - b;
		const int t  = b + Co / 2;
		const int Cg = g - t;
		const int Y  = t + Cg / 2;
				
		// Just saturate the chroma here (we loose out of one bit in each channel)
		// this just means that we won't have as high dynamic range. Perhaps a better option
		// is to loose the least significant bit instead?
		colorBlock[i*4+0] = bound( 0, Co + 128, 255 );
		colorBlock[i*4+1] = bound( 0, Cg + 128, 255 );
		colorBlock[i*4+2] = 0; // trying to use alpha ?
		colorBlock[i*4+3] = Y;
	}
}

/*
========================
NormalizeBlock

params: colorBlock	- 16 pixel block for which find color indexes
========================
*/
inline static void NormalizeBlock( byte *colorBlock, int format )
{
	float	pX, pY, a, t;
	float	discriminant;
	vec3_t	normal;

	for( int i = 0; i < 16; i++ )
	{
		int x = colorBlock[i*4+0];
		int y = colorBlock[i*4+1];
		int z = colorBlock[i*4+2];

		normal[0] = (x * (1.0f/127.0f) - 1.0f);
		normal[1] = (y * (1.0f/127.0f) - 1.0f);
		normal[2] = (z * (1.0f/127.0f) - 1.0f);

		if( VectorNormalize( normal ) == 0.0f )
			VectorSet( normal, 0.5f, 0.5f, 1.0f );

		switch( format )
		{
		case PF_ATI2_NORM_PARABOLOID:
			pX = normal[0];
			pY = normal[1];
			a = (pX * pX) + (pY * pY);
			discriminant = normal[2] * normal[2] - 4.0f * a * -1.0f;
			t = ( -normal[2] + sqrtf( discriminant )) / ( 2.0f * a );
			pX *= t;
			pY *= t;
			// store normal as two channels (RG)
			colorBlock[i*4+0] = 0;
			colorBlock[i*4+1] = (byte)(128 + 127 * pY);
			colorBlock[i*4+2] = 0;
			colorBlock[i*4+3] = (byte)(128 + 127 * pX);
			break;
		default:
			// store normal as normal image (XYZ)
			colorBlock[i*4+0] = (byte)(128 + 127 * normal[0]);
			colorBlock[i*4+1] = (byte)(128 + 127 * normal[1]);
			colorBlock[i*4+2] = (byte)(128 + 127 * normal[2]);
			colorBlock[i*4+3] = 0xFF;
			break;
		}
	}
}

/*
========================
CompressYCoCgDXT5HQ

params:	inBuf		- image to compress
paramO:	outBuf		- result of compression
params:	width		- width of image
params:	height		- height of image
========================
*/
inline static void CompressRGBABufferToDXT( const byte *inBuf, vfile_t *f, int width, int height, int format, bool estimate )
{
	ALIGN16 byte	block[64];
	ALIGN16 byte	outBlock[16];
	float		metricsRGB[3] = { 0.2126f, 0.7152f, 0.0722f };
	const char	*typeString = NULL;
	float		*metrics = NULL;
	double		start, end;
	int		flags = 0;
	char		str[64];

	start = I_FloatTime();

	for( int j = 0; j < height; j += 4, inBuf += width * BLOCK_SIZE )
	{
		for( int i = 0; i < width; i += 4 )
		{
			ExtractBlock( inBuf + i * 4, width, block );

			if( format == PF_DXT5_YCoCg )
			{
				SetBits( flags, squish::kDxt5 | squish::kColourIterativeClusterFit );
				typeString = "DXT5 YCoCg";
				ScaleYCoCg( block );
			}
			else if( format >= PF_DXT5_NORM_BASE && format <= PF_ATI2_NORM_PARABOLOID )
			{
				switch( format )
				{
				case PF_DXT5_NORM_BASE:
					typeString = "DXT5 NormXYZ Base";
					SetBits( flags, squish::kDxt5 | squish::kColourIterativeClusterFit );
					break;
				case PF_ATI2_NORM_PARABOLOID:
					typeString = "ATI2 NormAG Paraboloid";
					SetBits( flags, squish::kAti2 );
					break;
				}

				NormalizeBlock( block, format );
			}
			else if( format == PF_DXT5 )
			{
				SetBits( flags, squish::kDxt5 | squish::kColourIterativeClusterFit );
				typeString = "DXT5 RGB";
			}
			else if( format == PF_DXT5_ALPHA || format == PF_DXT5_SDF_ALPHA )
			{
				SetBits( flags, squish::kDxt5 | squish::kColourIterativeClusterFit | squish::kWeightColourByAlpha );
				typeString = "DXT5 RGBA";
			}
			else if( format == PF_DXT1 )
			{
				SetBits( flags, squish::kDxt1 | squish::kColourIterativeClusterFit );
				typeString = "DXT1 RGB";
			}

			squish::Compress( block, outBlock, flags, metrics );
			VFS_Write( f, outBlock, Image_DXTGetBlockSize( format ));

			if( estimate )
			{
				Sys_IgnoreLog( true );
				Msg( "\rcompress %s: %2d%%", typeString, ( j * width + i ) * 100 / ( width * height ) );
				Sys_IgnoreLog( false );
			}
		}
	}

	end = I_FloatTime();
	Q_timestring((int)(end - start), str );

	if( estimate )
	{
		Msg( "\r" );
		Msg( "compress %s: 100%%. %s elapsed\n", typeString, str );
	}
}

/*
========================================================================

.DDS image format

========================================================================
*/
#define DDSHEADER	((' '<<24)+('S'<<16)+('D'<<8)+'D') // little-endian "DDS "

// various four-cc types
#define TYPE_DXT1	(('1'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT1"
#define TYPE_DXT3	(('3'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT3"
#define TYPE_DXT5	(('5'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT5"
#define TYPE_ATI2	(('2'<<24)+('I'<<16)+('T'<<8)+'A') // little-endian "ATI2"

// dwFlags1
#define DDS_CAPS				0x00000001L
#define DDS_HEIGHT				0x00000002L
#define DDS_WIDTH				0x00000004L
#define DDS_PITCH				0x00000008L
#define DDS_PIXELFORMAT			0x00001000L
#define DDS_MIPMAPCOUNT			0x00020000L
#define DDS_LINEARSIZE			0x00080000L
#define DDS_DEPTH				0x00800000L

// dwFlags2
#define DDS_ALPHAPIXELS			0x00000001L
#define DDS_ALPHA				0x00000002L
#define DDS_FOURCC				0x00000004L
#define DDS_RGB				0x00000040L
#define DDS_RGBA				0x00000041L	// (DDS_RGB|DDS_ALPHAPIXELS)
#define DDS_LUMINANCE			0x00020000L
#define DDS_DUDV				0x00080000L

// dwCaps1
#define DDS_COMPLEX				0x00000008L
#define DDS_TEXTURE				0x00001000L
#define DDS_MIPMAP				0x00400000L

// dwCaps2
#define DDS_CUBEMAP				0x00000200L
#define DDS_CUBEMAP_POSITIVEX			0x00000400L
#define DDS_CUBEMAP_NEGATIVEX			0x00000800L
#define DDS_CUBEMAP_POSITIVEY			0x00001000L
#define DDS_CUBEMAP_NEGATIVEY			0x00002000L
#define DDS_CUBEMAP_POSITIVEZ			0x00004000L
#define DDS_CUBEMAP_NEGATIVEZ			0x00008000L
#define DDS_CUBEMAP_ALL_SIDES			0x0000FC00L
#define DDS_VOLUME				0x00200000L

typedef struct dds_pf_s
{
	uint		dwSize;
	uint		dwFlags;
	uint		dwFourCC;
	uint		dwRGBBitCount;
	uint		dwRBitMask;
	uint		dwGBitMask;
	uint		dwBBitMask;
	uint		dwABitMask;
} dds_pixf_t;

//  DDCAPS2
typedef struct dds_caps_s
{
	uint		dwCaps1;
	uint		dwCaps2;
	uint		dwCaps3;		// currently unused
	uint		dwCaps4;		// currently unused
} dds_caps_t;

typedef struct dds_s
{
	uint		dwIdent;		// must matched with DDSHEADER
	uint		dwSize;
	uint		dwFlags;		// determines what fields are valid
	uint		dwHeight;
	uint		dwWidth;
	uint		dwLinearSize;	// Formless late-allocated optimized surface size
	uint		dwDepth;		// depth if a volume texture
	uint		dwMipMapCount;	// number of mip-map levels requested
	uint		dwAlphaBitDepth;	// depth of alpha buffer requested
	uint		dwReserved1[10];	// reserved for future expansions
	dds_pixf_t	dsPixelFormat;
	dds_caps_t	dsCaps;
	uint		dwTextureStage;
} dds_t;

static bool Image_DXTGetPixelFormat( dds_t *hdr, uint &flags, uint &sqFlags )
{
	uint bits = hdr->dsPixelFormat.dwRGBBitCount;

	if( FBitSet( hdr->dsCaps.dwCaps2, DDS_VOLUME ))
		return false; // 3D textures doesn't support

	if( FBitSet( hdr->dsPixelFormat.dwFlags, DDS_FOURCC ))
	{
		switch( hdr->dsPixelFormat.dwFourCC )
		{
		case TYPE_DXT1:
			SetBits( flags, IMAGE_DXT_FORMAT );
			SetBits( sqFlags, squish::kDxt1 );
			break;
		case TYPE_DXT3:
			SetBits( flags, IMAGE_DXT_FORMAT );
			SetBits( sqFlags, squish::kDxt3 );
			break;
		case TYPE_DXT5:
			SetBits( flags, IMAGE_DXT_FORMAT );
			SetBits( sqFlags, squish::kDxt5 );
			break;
		default: return false;
		}
	}
	else
	{
		// this dds texture isn't compressed so write out ARGB or luminance format
		if( FBitSet( hdr->dsPixelFormat.dwFlags, DDS_DUDV ))
		{
			return false;
		}
		else if( FBitSet( hdr->dsPixelFormat.dwFlags, DDS_LUMINANCE ))
		{
			return false;
		}
		else 
		{
			if( bits == 32 )
				SetBits( flags, IMAGE_HAS_8BIT_ALPHA );
			else if( bits != 24 )
				return false;
		}
		// no deals with other DXT types anyway
	}

	SetBits( flags, IMAGE_HAS_COLOR ); // predict state

	if( FBitSet( hdr->dsPixelFormat.dwFlags, DDS_ALPHA ))
		SetBits( flags, IMAGE_HAS_8BIT_ALPHA );

	if( FBitSet( hdr->dsPixelFormat.dwFlags, DDS_ALPHAPIXELS ))
		SetBits( flags, IMAGE_HAS_8BIT_ALPHA );

	if( !FBitSet( hdr->dwFlags, DDS_MIPMAPCOUNT ))
		hdr->dwMipMapCount = 1;

	// setup additional flags
	if( FBitSet( hdr->dsCaps.dwCaps1, DDS_COMPLEX ) && FBitSet( hdr->dsCaps.dwCaps2, DDS_CUBEMAP ))
	{
		if( hdr->dwMipMapCount <= 1 )
			SetBits( flags, IMAGE_SKYBOX );
		else SetBits( flags, IMAGE_CUBEMAP );
	}

	return true;
}

static size_t Image_DXTGetLinearSize( size_t block, int width, int height )
{
	return ((width + 3) / 4) * ((height + 3) / 4) * block;
}

static size_t Image_DXTGetBlockSize( dds_t *hdr )
{
	if( FBitSet( hdr->dsPixelFormat.dwFlags, DDS_FOURCC ))
	{
		switch( hdr->dsPixelFormat.dwFourCC )
		{
		case TYPE_DXT1: return 8;
		case TYPE_DXT3: return 16;
		case TYPE_DXT5: return 16;
		case TYPE_ATI2: return 16;
		default: return 0;
		}
	}

	// no deals with other DXT types anyway
	return 0;
}

static size_t Image_DXTCalcMipmapSize( dds_t *hdr ) 
{
	size_t	buffsize = 0;
	int	width, height;
		
	// now correct buffer size
	for( int i = 0; i < hdr->dwMipMapCount; i++ )
	{
		width = Q_max( 1, ( hdr->dwWidth >> i ));
		height = Q_max( 1, ( hdr->dwHeight >> i ));
		buffsize += Image_DXTGetLinearSize( Image_DXTGetBlockSize( hdr ), width, height );
	}

	return buffsize;
}

static size_t Image_DXTCalcSize( const char *name, dds_t *hdr, size_t filesize ) 
{
	size_t buffsize = Image_DXTCalcMipmapSize( hdr );

	if (FBitSet(hdr->dsCaps.dwCaps2, DDS_CUBEMAP)) {
		// obviously, because cube has 6 sides
		buffsize *= 6;
	}

	if (filesize != buffsize) // main check
	{
		MsgDev(D_WARN, "Image_DXTCalcSize: probably corrupted (%i should be %i) (%s) \n", buffsize, filesize, name);
		if (filesize < buffsize) {
			return 0;
		}
	}
	return buffsize;
}

static int Image_DXTCalcMipmapCount( rgbdata_t *pix ) 
{
	int mipcount;
	int	width, height;

	if( FBitSet( pix->flags, IMAGE_SKYBOX|IMAGE_NOMIPS ))
		return 1;

	// mip-maps can't exceeds 16
	for( mipcount = 0; mipcount < 16; mipcount++ )
	{
		width = Q_max( 1, ( pix->width >> mipcount ));
		height = Q_max( 1, ( pix->height >> mipcount ));
		if( width == 1 && height == 1 )
			break;
	}

	MsgDev( D_REPORT, "image[ %i x %i ] has %i mips\n", pix->width, pix->height, mipcount + 1 );

	return mipcount + 1;
}

static bool Image_CheckDXT3Alpha( dds_t *hdr, byte *fin )
{
	word	sAlpha;
	byte	*alpha; 
	int	x, y, i, j; 

	for( y = 0; y < hdr->dwHeight; y += 4 )
	{
		for( x = 0; x < hdr->dwWidth; x += 4 )
		{
			alpha = fin + 8;
			fin += 16;

			for( j = 0; j < 4; j++ )
			{
				sAlpha = alpha[2*j+0] + 256 * alpha[2*j+1];

				for( i = 0; i < 4; i++ )
				{
					if((( x + i ) < hdr->dwWidth ) && (( y + j ) < hdr->dwHeight ))
					{
						if( sAlpha == 0 )
							return true;
					}
					sAlpha >>= 4;
				}
			}
		}
	}

	return false;
}

static bool Image_CheckDXT5Alpha( dds_t *hdr, byte *fin )
{
	uint	bits, bitmask;
	byte	*alphamask; 
	int	x, y, i, j; 

	for( y = 0; y < hdr->dwHeight; y += 4 )
	{
		for( x = 0; x < hdr->dwWidth; x += 4 )
		{
			if( y >= hdr->dwHeight || x >= hdr->dwWidth )
				break;

			alphamask = fin + 2;
			fin += 8;

			bitmask = ((uint *)fin)[1];
			fin += 8;

			// last three bytes
			bits = (alphamask[3]) | (alphamask[4] << 8) | (alphamask[5] << 16);

			for( j = 2; j < 4; j++ )
			{
				for( i = 0; i < 4; i++ )
				{
					// only put pixels out < width or height
					if((( x + i ) < hdr->dwWidth ) && (( y + j ) < hdr->dwHeight ))
					{
						if( bits & 0x07 )
							return true;
					}
					bits >>= 3;
				}
			}
		}
	}

	return false;
}

/*
=============
DDSToBuffer
=============
*/
rgbdata_t *DDSToBuffer( const char *name, const byte *buffer, size_t filesize )
{
	uint	flags = 0;
	uint	dummy = 0;
	dds_t	header;
	rgbdata_t	*pic;

	if( filesize < sizeof( dds_t ))
	{
		MsgDev( D_ERROR, "Image_LoadDDS: file (%s) have invalid size\n", name );
		return NULL;
	}

	memcpy( &header, buffer, sizeof( dds_t ));

	if( header.dwIdent != DDSHEADER )
		return NULL; // it's not a dds file, just skip it

	if( header.dwSize != sizeof( dds_t ) - sizeof( uint )) // size of the structure (minus MagicNum)
	{
		MsgDev( D_ERROR, "LoadDDS: (%s) have corrupted header\n", name );
		return NULL;
	}

	if( header.dsPixelFormat.dwSize != sizeof( dds_pixf_t )) // size of the structure
	{
		MsgDev( D_ERROR, "LoadDDS: (%s) have corrupt pixelformat header\n", name );
		return NULL;
	}

	if( !Image_ValidSize( name, header.dwWidth, header.dwHeight ))
		return NULL;

	if( !Image_DXTGetPixelFormat( &header, flags, dummy ))
	{
		MsgDev( D_ERROR, "LoadDDS: (%s) unsupported DXT format (only DXT1, DXT3 and DXT5 is supported)\n", name );
		return NULL;
	}

	if( !Image_DXTCalcSize( name, &header, filesize - sizeof( dds_t ))) 
		return NULL;

	// now all the checks are passed, store image as rgbdata
	pic = (rgbdata_t *)Mem_Alloc( sizeof( rgbdata_t ) + filesize );
	pic->buffer = ((byte *)pic) + sizeof( rgbdata_t ); 
	memcpy( pic->buffer, buffer, filesize );

	pic->width = header.dwWidth;
	pic->height = header.dwHeight;
	pic->size = filesize;
	pic->flags = flags;

	return pic;
}

static bool Image_DXTWriteHeader( vfile_t *f, rgbdata_t *pix, int format, int numMipMaps )
{
	uint32_t	dwFourCC = 0, dwFlags1 = 0, dwFlags2 = 0, dwCaps1 = 0, dwCaps2 = 0;
	uint32_t	dwLinearSize, dwSize = 124, dwSize2 = sizeof( dds_pixf_t );
	uint32_t	dwWidth, dwHeight, dwMipCount, dwCustomEncode = DXT_ENCODE_DEFAULT;
	uint32_t	dwReflectivity = 0;
	uint32_t	dwIdent = DDSHEADER;

	if( !pix || !pix->buffer )
		return false;

	// setup flags
	SetBits( dwFlags1, DDS_LINEARSIZE|DDS_WIDTH|DDS_HEIGHT|DDS_CAPS|DDS_PIXELFORMAT|DDS_PITCH );
	SetBits( dwFlags2, DDS_FOURCC );
	dwMipCount = numMipMaps;
	if( dwMipCount > 1 ) SetBits( dwFlags1, DDS_MIPMAPCOUNT );

	switch( format )
	{
	case PF_DXT1:
		dwFourCC = TYPE_DXT1;
		break;
	case PF_DXT3:
		dwFourCC = TYPE_DXT3;
		break;
	case PF_DXT5_YCoCg:
		dwCustomEncode = DXT_ENCODE_COLOR_YCoCg;
		dwFourCC = TYPE_DXT5;
		break;
	case PF_ATI2_NORM_PARABOLOID:
		dwCustomEncode = DXT_ENCODE_NORMAL_AG_PARABOLOID;
		dwFourCC = TYPE_ATI2;
		break;
	case PF_DXT5:
	case PF_DXT5_ALPHA:
	case PF_DXT5_NORM_BASE:
		dwFourCC = TYPE_DXT5;
		break;
	case PF_DXT5_SDF_ALPHA:
		dwCustomEncode = DXT_ENCODE_ALPHA_SDF;
		dwFourCC = TYPE_DXT5;
		break;
	default:
		MsgDev( D_ERROR, "Image_DXTWriteHeader: unknown format\n" );	
		return false;
	}

	Image_PackRGB( pix->reflectivity, dwReflectivity );

	dwWidth = pix->width;
	dwHeight = pix->height;

	VFS_Write( f, &dwIdent, sizeof(uint32_t));
	VFS_Write( f, &dwSize, sizeof(uint32_t));
	VFS_Write( f, &dwFlags1, sizeof(uint32_t));
	VFS_Write( f, &dwHeight, sizeof(uint32_t));
	VFS_Write( f, &dwWidth, sizeof(uint32_t));

	dwLinearSize = static_cast<uint32_t>(Image_DXTGetLinearSize(Image_DXTGetBlockSize(format), pix->width, pix->height));
	VFS_Write( f, &dwLinearSize, sizeof(uint32_t));

	VFS_Write( f, 0, sizeof(uint32_t));
	VFS_Write( f, &dwMipCount, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t));
	VFS_Write( f, &dwCustomEncode, sizeof(uint32_t));	// was dwReserved[0]
	VFS_Write( f, &dwReflectivity, sizeof(uint32_t));	// was dwReserved[1]
	VFS_Write( f, NULL, sizeof(uint32_t) * 8 );	// reserved fields

	VFS_Write( f, &dwSize2, sizeof(uint32_t));
	VFS_Write( f, &dwFlags2, sizeof(uint32_t));
	VFS_Write( f, &dwFourCC, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t));

	dwCaps1 |= DDS_TEXTURE;

	if( dwMipCount > 1 )
		SetBits( dwCaps1, DDS_MIPMAP|DDS_COMPLEX );

	if( pix->flags & ( IMAGE_CUBEMAP|IMAGE_SKYBOX ))
	{
		SetBits( dwCaps1, DDS_COMPLEX );
		SetBits( dwCaps2, DDS_CUBEMAP|DDS_CUBEMAP_ALL_SIDES );
	}

	VFS_Write( f, &dwCaps1, sizeof(uint32_t));
	VFS_Write( f, &dwCaps2, sizeof(uint32_t));
	VFS_Write( f, NULL, sizeof(uint32_t) * 3 ); // other caps and TextureStage

	return true;
}

static size_t CompressDXT( vfile_t *f, const byte *buffer, int width, int height, int format, bool estimate )
{
	size_t	dst_size;
	size_t	cur_size;
	size_t	curpos;

	if( !buffer ) return 0;

	dst_size = Image_DXTGetLinearSize( Image_DXTGetBlockSize( format ), width, height );
	curpos = VFS_Tell( f );

	CompressRGBABufferToDXT( buffer, f, width, height, format, estimate );

	cur_size = VFS_Tell( f ) - curpos;

	if( cur_size != dst_size )
      	{
		MsgDev( D_ERROR, "CompressDXT: corrupt mem (buffer is %s bytes, written %s)\n", Q_memprint( dst_size ), Q_memprint( cur_size ));
		return false;
	}		

	return true;
}

rgbdata_t *BufferToDDS( rgbdata_t *pix, int saveformat )
{
	vfile_t	*file;	// virtual file
	rgbdata_t	*out = NULL;
	rgbdata_t	*mip = NULL;
	bool	normalMap = (saveformat >= PF_DXT5_NORM_BASE && saveformat <= PF_ATI2_NORM_PARABOLOID) ? true : false;
	int	width, height;
	int	nummips = 1;
	int	numSides = 1;

	// check for all the possible problems
	if( !pix ) return NULL;

	if( FBitSet( pix->flags, IMAGE_DXT_FORMAT ))
		return NULL; // already compressed

	if(( pix->width & 15 ) || ( pix->height & 15 ))
		return NULL; // not aligned by 16

	file = VFS_Create( NULL, 0 );
	if( !file ) return NULL;

	nummips = Image_DXTCalcMipmapCount( pix );

	if( !Image_DXTWriteHeader( file, pix, saveformat, nummips ))
	{
		MsgDev( D_ERROR, "BufferToDDS: unsupported format\n" );
		VFS_Close( file );
		return NULL;
	}

	mip = Image_Copy( pix );

	if( FBitSet( pix->flags, IMAGE_CUBEMAP|IMAGE_SKYBOX ))
		numSides = 6;

	for( int i = 0; i < numSides; i++ )
	{
		byte	*buffer = mip->buffer + ((pix->width * pix->height * 4) * i);

		width = pix->width;
		height = pix->height;

		for( int j = 0; j < nummips; j++ )
		{
			if( j ) Image_BuildMipMap( buffer, width, height, normalMap );

			width = Q_max( 1, ( pix->width >> j ));
			height = Q_max( 1, ( pix->height >> j ));

			if( !CompressDXT( file, buffer, width, height, saveformat, ( j == 0 ) ))
			{
				MsgDev( D_ERROR, "BufferToDDS: can't compress image\n" );
				VFS_Close( file );
				Image_Free( mip );
				return NULL;
			}
		}
	}

	Image_Free( mip );

	// create a new pic
	out = (rgbdata_t *)Mem_Alloc( sizeof( rgbdata_t ) + VFS_Tell( file ));
	out->buffer = ((byte *)out) + sizeof( rgbdata_t ); 
	memcpy( out->buffer, VFS_GetBuffer( file ), VFS_Tell( file ));

	out->width = pix->width;
	out->height = pix->height;
	out->size = VFS_Tell( file );

	SetBits( out->flags, IMAGE_HAS_COLOR );
	SetBits( out->flags, IMAGE_DXT_FORMAT );

	if( FBitSet( pix->flags, IMAGE_CUBEMAP ))
		SetBits( out->flags, IMAGE_CUBEMAP );

	if( FBitSet( pix->flags, IMAGE_SKYBOX ))
		SetBits( out->flags, IMAGE_SKYBOX );

	// release virtual file
	VFS_Close( file );

	return out;
}

rgbdata_t *DDSToRGBA( const char *name, const byte *buffer, size_t filesize )
{
	uint flags = 0;
	uint sqFlags = 0;
	dds_t header;
	rgbdata_t *pic;
	byte *fin;

	if (filesize < sizeof(dds_t))
	{
		MsgDev(D_ERROR, "Image_LoadDDS: file have invalid size (%s)\n", name);
		return nullptr;
	}

	memcpy(&header, buffer, sizeof(dds_t));
	if (header.dwIdent != DDSHEADER)
	{
		MsgDev(D_ERROR, "Image_LoadDDS: invalid magic value (%s)\n", name);
		return nullptr; // it's not a dds file, just skip it
	}

	if (header.dwSize != sizeof(dds_t) - sizeof(uint)) // size of the structure (minus MagicNum)
	{
		MsgDev(D_ERROR, "Image_LoadDDS: have corrupted header (%s)\n", name);
		return nullptr;
	}

	if (header.dsPixelFormat.dwSize != sizeof(dds_pixf_t)) // size of the structure
	{
		MsgDev(D_ERROR, "Image_LoadDDS: have mismatched pixelformat header size (%s)\n", name);
		return nullptr;
	}

	if (!Image_ValidSize(name, header.dwWidth, header.dwHeight)) {
		return nullptr;
	}

	if (!Image_DXTGetPixelFormat(&header, flags, sqFlags))
	{
		MsgDev(D_ERROR, "Image_LoadDDS: unsupported DXT format (only DXT1, DXT3 and DXT5 is supported now) (%s)\n", name);
		return nullptr;
	}

	if (FBitSet(flags, IMAGE_DXT_FORMAT))
	{
		if (!Image_DXTCalcSize(name, &header, filesize - sizeof(dds_t))) {
			return nullptr;
		}
	}

	fin = (byte *)(buffer + sizeof(dds_t)); // pixels are immediately follows after header

	if (FBitSet(sqFlags, squish::kDxt3) && Image_CheckDXT3Alpha(&header, fin)) {
		SetBits(flags, IMAGE_HAS_8BIT_ALPHA);
	}
	else if (FBitSet(sqFlags, squish::kDxt5) && Image_CheckDXT5Alpha(&header, fin)) {
		SetBits(flags, IMAGE_HAS_8BIT_ALPHA);
	}

	pic = Image_Alloc(header.dwWidth, header.dwHeight);
	if (!pic) {
		return nullptr;
	}
	
	// now all the checks are passed, store image as rgbdata
	if (FBitSet(flags, IMAGE_DXT_FORMAT)) {
		squish::DecompressImage(pic->buffer, header.dwWidth, header.dwHeight, fin, sqFlags);
	}
	else {
		memcpy(pic->buffer, fin, header.dwWidth * header.dwHeight * FBitSet(flags, IMAGE_HAS_ALPHA) ? 4 : 3);
	}

	pic->flags = flags;
	return pic;
}

int DDS_GetSaveFormatForHint( int hint, rgbdata_t *pix )
{
	if( hint == IMG_NORMALMAP )
	{
		return PF_ATI2_NORM_PARABOLOID;
	}
	else
	{
		if( FBitSet( pix->flags, IMAGE_HAS_ALPHA ))
			return PF_DXT5_ALPHA;
		return PF_DXT1;
	}
}
