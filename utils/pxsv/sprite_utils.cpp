/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/
// updates:
// 1-4-99	fixed file texture load and file read bug

////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gl.h>
#include <GL/glu.h>
#include "SpriteModel.h"
#include "GlWindow.h"
#include "ViewerSettings.h"
#include "stringlib.h" 
#include "cmdlib.h"

#include <mx.h>
#include "sprviewer.h"

SpriteModel g_spriteModel;
extern bool bUseWeaponOrigin;
extern bool bUseWeaponLeftHand;

////////////////////////////////////////////////////////////////////////

// Quake1 always has same palette
byte palette_q1[768] =
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

////////////////////////////////////////////////////////////////////////
static int g_tex_base = TEXTURE_COUNT;

//Mugsy - upped the maximum texture size to 512. All changes are the replacement of '256'
//with this define, MAX_TEXTURE_DIMS
#define MAX_TEXTURE_DIMS 512

void SpriteModel :: UploadTexture( byte *data, int width, int height, byte *srcpal, int name, int size, bool has_alpha )
{
	int	row1[MAX_TEXTURE_DIMS], row2[MAX_TEXTURE_DIMS], col1[MAX_TEXTURE_DIMS], col2[MAX_TEXTURE_DIMS];
	byte	*pix1, *pix2, *pix3, *pix4;
	byte	*tex, *in, *out, pal[768];
	int	i, j;

	// convert texture to power of 2
	int outwidth;
	for (outwidth = 1; outwidth < width; outwidth <<= 1);

	if (outwidth > MAX_TEXTURE_DIMS)
		outwidth = MAX_TEXTURE_DIMS;

	int outheight;
	for (outheight = 1; outheight < height; outheight <<= 1);

	if (outheight > MAX_TEXTURE_DIMS)
		outheight = MAX_TEXTURE_DIMS;

	in = (byte *)Mem_Alloc(width * height * 4);
	if (!in) return;

	if( size == 1024 )
	{
		// expand pixels to rgba
		for (i=0 ; i < width * height; i++)
		{
			in[i*4+0] = srcpal[data[i]*4+0];
			in[i*4+1] = srcpal[data[i]*4+1];
			in[i*4+2] = srcpal[data[i]*4+2];
			in[i*4+3] = srcpal[data[i]*4+3];
		}
	}
	else
	{
		memcpy( pal, srcpal, 768 );

		// expand pixels to rgba
		for (i=0 ; i < width * height; i++)
		{
			if( data[i] == 255 )
			{
				has_alpha = true;
				in[i*4+0] = 0x00;
				in[i*4+1] = 0x00;
				in[i*4+2] = 0x00;
				in[i*4+3] = 0x00;
			}
			else
			{
				in[i*4+0] = pal[data[i]*3+0];
				in[i*4+1] = pal[data[i]*3+1];
				in[i*4+2] = pal[data[i]*3+2];
				in[i*4+3] = 0xFF;
			}
		}
	}

	tex = out = (byte *)Mem_Alloc(outwidth * outheight * 4);
	if (!out) return;

	for (i = 0; i < outwidth; i++)
	{
		col1[i] = (int) ((i + 0.25) * (width / (float)outwidth));
		col2[i] = (int) ((i + 0.75) * (width / (float)outwidth));
	}

	for (i = 0; i < outheight; i++)
	{
		row1[i] = (int) ((i + 0.25) * (height / (float)outheight)) * width;
		row2[i] = (int) ((i + 0.75) * (height / (float)outheight)) * width;
	}

	// scale down and convert to 32bit RGB
	for (i=0 ; i<outheight ; i++)
	{
		for (j=0 ; j<outwidth ; j++, out += 4)
		{
			pix1 = &in[(row1[i] + col1[j]) * 4];
			pix2 = &in[(row1[i] + col2[j]) * 4];
			pix3 = &in[(row2[i] + col1[j]) * 4];
			pix4 = &in[(row2[i] + col2[j]) * 4];

			out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			out[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}

	GLint outFormat = (has_alpha) ? GL_RGBA : GL_RGB;

	glBindTexture( GL_TEXTURE_2D, name );		
	glHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
	glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
	glTexImage2D( GL_TEXTURE_2D, 0, outFormat, outwidth, outheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex );
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	const char *extensions = (const char *)glGetString( GL_EXTENSIONS );

	// check for anisotropy support
	if( Q_strstr( extensions, "GL_EXT_texture_filter_anisotropic" ))
	{
		float	anisotropy = 1.0f;
		glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy );
	}

	Mem_Free( tex );
	Mem_Free( in );
}

/*
================
LoadSpriteFrame
================
*/
dframetype_t* SpriteModel :: LoadSpriteFrame( void *pin, mspriteframe_t **ppframe )
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	bool		has_alpha = false; 

	// check for alpha-channel
	if( m_pspritehdr->texFormat == SPR_ALPHTEST || m_pspritehdr->texFormat == SPR_INDEXALPHA )
		has_alpha = true;

	pinframe = (dspriteframe_t *)pin;

	UploadTexture( (byte *)(pinframe + 1), pinframe->width, pinframe->height, m_palette, g_tex_base + m_loadframe, 1024, has_alpha );

	// setup frame description
	pspriteframe = (mspriteframe_t *)Mem_Alloc(sizeof(mspriteframe_t));
	pspriteframe->width = pinframe->width;
	pspriteframe->height = pinframe->height;
	pspriteframe->up = pinframe->origin[1];
	pspriteframe->left = pinframe->origin[0];
	pspriteframe->down = pinframe->origin[1] - pinframe->height;
	pspriteframe->right = pinframe->width + pinframe->origin[0];
	pspriteframe->gl_texturenum = g_tex_base + m_loadframe;
	*ppframe = pspriteframe;
	m_loadframe++;

	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
}

/*
================
LoadSpriteGroup
================
*/
dframetype_t* SpriteModel :: LoadSpriteGroup( void *pin, mspriteframe_t **ppframe )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	int		i, groupsize, numframes;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = pingroup->numframes;

	groupsize = sizeof( mspritegroup_t ) + (numframes - 1) * sizeof( pspritegroup->frames[0] );
	pspritegroup = (mspritegroup_t *)Mem_Alloc(groupsize);
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = (float *)Mem_Alloc(numframes * sizeof(float));
	pspritegroup->intervals = poutintervals;

	for( i = 0; i < numframes; i++ )
	{
		*poutintervals = pin_intervals->interval;
		if( *poutintervals <= 0.0f )
			*poutintervals = 1.0f; // set error value
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = LoadSpriteFrame( ptemp, &pspritegroup->frames[i] );
	}

	return (dframetype_t *)ptemp;
}

msprite_t *SpriteModel :: LoadSprite( const char *spritename )
{
	FILE *fp;
	void *buffer;

	if (!spritename)
		return 0;

	// load the model
	if( (fp = fopen( spritename, "rb" )) == NULL)
		return 0;

	fseek( fp, 0, SEEK_END );
	m_iFileSize = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	buffer = Mem_Alloc(m_iFileSize);
	if (!buffer)
	{
		m_iFileSize = 0;
		fclose (fp);
		return 0;
	}

	fread( buffer, m_iFileSize, 1, fp );
	fclose( fp );

	dsprite_q1_t	*pinq1;
	dsprite_hl_t	*pinhl;
	dsprite_t		*pin;
	short		*numi = NULL;
	dframetype_t	*pframetype;
	msprite_t		*psprite;
	int		i, size;

	pin = (dsprite_t *)buffer;

	if (strncmp ((const char *) buffer, "IDSP", 4 ))
	{
		mxMessageBox( g_GlWindow, "Unknown file format.", g_appTitle, MX_MB_OK | MX_MB_ERROR );
		m_iFileSize = 0;
		Mem_Free(buffer);
		return 0;
	}

	if( pin->version != SPRITE_VERSION_Q1 && pin->version != SPRITE_VERSION_HL )
	{
		mxMessageBox( g_GlWindow, "Unsupported sprite version.", g_appTitle, MX_MB_OK | MX_MB_ERROR );
		m_iFileSize = 0;
		Mem_Free(buffer);
		return 0;
	}

	if( pin->version == SPRITE_VERSION_Q1 )
	{
		pinq1 = (dsprite_q1_t *)buffer;
		size = sizeof( msprite_t ) + ( pinq1->numframes - 1 ) * sizeof( psprite->frames );
		psprite = (msprite_t *)Mem_Alloc(size);
		m_pspritehdr = psprite;	// make link to extradata

		psprite->type = pinq1->type;
		psprite->texFormat = SPR_ALPHTEST;
		psprite->numframes = pinq1->numframes;
		psprite->facecull = SPR_CULL_FRONT;
		psprite->radius = pinq1->boundingradius;
		psprite->synctype = pinq1->synctype;

		m_spritemins[0] = m_spritemins[1] = -pinq1->bounds[0] * 0.5f;
		m_spritemaxs[0] = m_spritemaxs[1] = pinq1->bounds[0] * 0.5f;
		m_spritemins[2] = -pinq1->bounds[1] * 0.5f;
		m_spritemaxs[2] = pinq1->bounds[1] * 0.5f;
		numi = NULL;
	}
	else if( pin->version == SPRITE_VERSION_HL )
	{
		pinhl = (dsprite_hl_t *)buffer;
		size = sizeof( msprite_t ) + ( pinhl->numframes - 1 ) * sizeof( psprite->frames );
		psprite = (msprite_t *)Mem_Alloc(size);
		m_pspritehdr = psprite;	// make link to extradata

		psprite->type = pinhl->type;
		psprite->texFormat = pinhl->texFormat;
		psprite->numframes = pinhl->numframes;
		psprite->facecull = pinhl->facetype;
		psprite->radius = pinhl->boundingradius;
		psprite->synctype = pinhl->synctype;

		m_spritemins[0] = m_spritemins[1] = -pinhl->bounds[0] * 0.5f;
		m_spritemaxs[0] = m_spritemaxs[1] = pinhl->bounds[0] * 0.5f;
		m_spritemins[2] = -pinhl->bounds[1] * 0.5f;
		m_spritemaxs[2] = pinhl->bounds[1] * 0.5f;
		numi = (short *)(pinhl + 1);
	}

	// last color are transparent
	m_palette[255*4+0] = m_palette[255*4+1] = m_palette[255*4+2] = m_palette[255*4+3] = 0;
	m_loadframe = 0;

	if( numi == NULL )
	{
		for( i = 0; i < 255; i++ )
		{
			m_palette[i*4+0] = palette_q1[i*3+0];
			m_palette[i*4+1] = palette_q1[i*3+1];
			m_palette[i*4+2] = palette_q1[i*3+2];
			m_palette[i*4+3] = 0xFF;
		}
		pframetype = (dframetype_t *)(pinq1 + 1);
	}
	else if( *numi == 256 )
	{	
		byte	*pal = (byte *)(numi+1);

		// install palette
		switch( psprite->texFormat )
		{
                    case SPR_INDEXALPHA:
			for( i = 0; i < 256; i++ )
			{
				m_palette[i*4+0] = pal[765];
				m_palette[i*4+1] = pal[766];
				m_palette[i*4+2] = pal[767];
				m_palette[i*4+3] = i;
			}
			break;
		case SPR_ALPHTEST:		
			for( i = 0; i < 255; i++ )
			{
				m_palette[i*4+0] = pal[i*3+0];
				m_palette[i*4+1] = pal[i*3+1];
				m_palette[i*4+2] = pal[i*3+2];
				m_palette[i*4+3] = 0xFF;
			}
                              break;
		default:
			for( i = 0; i < 256; i++ )
			{
				m_palette[i*4+0] = pal[i*3+0];
				m_palette[i*4+1] = pal[i*3+1];
				m_palette[i*4+2] = pal[i*3+2];
				m_palette[i*4+3] = 0xFF;
			}
			break;
		}

		pframetype = (dframetype_t *)(pal + 768);
	}
	else 
	{
		mxMessageBox( g_GlWindow, "sprite has wrong number of palette colors.\n", g_appTitle, MX_MB_OK | MX_MB_ERROR );
		Mem_Free(m_pspritehdr);
		m_iFileSize = 0;
		Mem_Free(buffer);
		return 0;
	}

	if( m_pspritehdr->numframes > MAX_SPRITE_FRAMES )
	{
		mxMessageBox( g_GlWindow, "sprite has too many frames.", g_appTitle, MX_MB_OK | MX_MB_ERROR );
		Mem_Free(m_pspritehdr);
		m_iFileSize = 0;
		Mem_Free(buffer);
		return 0;
	}

	Q_strncpy (g_viewerSettings.spriteFile, spritename, sizeof( g_viewerSettings.spriteFile ));
	Q_strncpy( g_viewerSettings.spritePath, spritename, sizeof( g_viewerSettings.spritePath ));

	for( i = 0; i < m_pspritehdr->numframes; i++ )
	{
		frametype_t frametype = pframetype->type;
		psprite->frames[i].type = static_cast<spriteframetype_t>(frametype);

		switch( frametype )
		{
		case FRAME_SINGLE:
			pframetype = LoadSpriteFrame( pframetype + 1, &psprite->frames[i].frameptr );
			break;
		case FRAME_GROUP:
			pframetype = LoadSpriteGroup( pframetype + 1, &psprite->frames[i].frameptr );
			break;
		case FRAME_ANGLED:
			pframetype = LoadSpriteGroup( pframetype + 1, &psprite->frames[i].frameptr );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}

	char	basename[64];

	COM_FileBase( spritename, basename );

	if( !Q_strnicmp( basename, "v_", 2 ))
	{
		g_SPRViewer->checkboxSet( IDC_OPTIONS_WEAPONORIGIN, true );
		bUseWeaponOrigin = true;
	}
	else
	{
		g_SPRViewer->checkboxSet( IDC_OPTIONS_WEAPONORIGIN, false );
		bUseWeaponOrigin = false;
	}

	// reset all the changes
	g_viewerSettings.numModelChanges = 0;

	// free buffer
	m_iFileSize = 0;
	Mem_Free(buffer);

	return m_pspritehdr;
}

void SpriteModel :: FreeSprite( void )
{
	int	i;

	if( g_viewerSettings.numModelChanges )
	{
		if( !mxMessageBox( g_GlWindow, "Sprite has changes. Do you wish to save them?", g_appTitle, MX_MB_YESNO | MX_MB_QUESTION ))
		{
			char *ptr = (char *)mxGetSaveFileName( g_GlWindow , g_viewerSettings.spritePath, "GoldSrc Sprite (*.spr)" );
			if( ptr )
			{
				char filename[256];
				char ext[16];

				strcpy( filename, ptr );
				strcpy( ext, mx_getextension( filename ));
				if( mx_strcasecmp( ext, ".spr" ))
					strcat( filename, ".spr" );

				if( !SaveSprite( filename ))
					mxMessageBox( g_GlWindow, "Error saving sprite.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
			}
		}

		g_viewerSettings.numModelChanges = 0;	// all the settings are handled or invalidated
	}

	if( m_pspritehdr )
	{
		mspritegroup_t *pgroup;

		// deleting textures
		int textures[MAX_SPRITE_FRAMES];
		for (i = 0; i < m_loadframe; i++)
			textures[i] = g_tex_base + i;

		for( i = 0; i < m_pspritehdr->numframes; i++ )
		{
			if( m_pspritehdr->frames[i].type != FRAME_SINGLE )
			{
				pgroup = (mspritegroup_t *)m_pspritehdr->frames[i].frameptr;
				Mem_Free(pgroup->intervals);	// throw intervals
				// throw frames
				for( i = 0; i < pgroup->numframes; i++ )
					Mem_Free(pgroup->frames[i]);
				Mem_Free(pgroup); // throw himself
			}
			else Mem_Free(m_pspritehdr->frames[i].frameptr);
		}

		glDeleteTextures (m_loadframe, (const GLuint *)textures);
		m_loadframe = 0;
	}

	if (m_pspritehdr)
		Mem_Free(m_pspritehdr);

	m_frame = 0;
	m_pspritehdr = 0;
	m_iFileSize = 0;
}

bool SpriteModel :: SaveSprite( const char *spritename )
{
//	if (!spritename)
		return false;

	if (!m_pspritehdr)
		return false;

	FILE *file;
	
	file = fopen (spritename, "wb");
	if (!file)
		return false;

	fwrite (m_pspritehdr, sizeof (byte), m_iFileSize, file);
	fclose (file);
	return true;
}

void SpriteModel :: ExtractBbox( vec3_t &mins, vec3_t &maxs )
{
	if( !m_pspritehdr )
	{
		mins = g_vecZero;
		maxs = g_vecZero;
		return;
	}

	mins = m_spritemins;
	maxs = m_spritemaxs;
}

void SpriteModel:: GetMovement( vec3_t &delta )
{
	delta = g_vecZero;
}
