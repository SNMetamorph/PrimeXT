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
#include "StudioModel.h"
#include "GlWindow.h"
#include "ViewerSettings.h"
#include "ControlPanel.h"
#include "activity.h"
#include "stringlib.h"
#include "cmdlib.h"

#include <mx.h>
#include "mdlviewer.h"
#include "app_info.h"

StudioModel g_studioModel;
extern bool bUseWeaponOrigin;
extern bool bUseParanoiaFOV;

////////////////////////////////////////////////////////////////////////

void CBaseBoneSetup :: debugMsg( char *szFmt, ... )
{
	static char	buffer[1024];
	va_list		args;
	int		result;

	va_start( args, szFmt );
	result = Q_vsnprintf( buffer, 99999, szFmt, args );
	va_end( args );

	Sys_PrintLog( buffer );
}

mstudioanim_t *CBaseBoneSetup :: GetAnimSourceData( mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	return (mstudioanim_t *)((byte *)g_studioModel.getAnimHeader( pseqdesc->seqgroup ) + pseqdesc->animindex);
}

void CBaseBoneSetup :: debugLine( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration )
{
	if( noDepthTest )
		glDisable( GL_DEPTH_TEST );

	glColor3ub( r, g, b );

	glBegin( GL_LINES );
		glVertex3fv( origin );
		glVertex3fv( dest );
	glEnd();
}

////////////////////////////////////////////////////////////////////////

static int g_tex_base = TEXTURE_COUNT;

//Mugsy - upped the maximum texture size to 512. All changes are the replacement of '256'
//with this define, MAX_TEXTURE_DIMS
#define MAX_TEXTURE_DIMS 1024	

void StudioModel::UploadTexture(mstudiotexture_t *ptexture, byte *data, byte *srcpal, int name)
{
	int	row1[MAX_TEXTURE_DIMS], row2[MAX_TEXTURE_DIMS], col1[MAX_TEXTURE_DIMS], col2[MAX_TEXTURE_DIMS];
	byte	*pix1, *pix2, *pix3, *pix4;
	byte	*tex, *in, *out, pal[768];
	int	i, j;

	// convert texture to power of 2
	int outwidth;
	for (outwidth = 1; outwidth < ptexture->width; outwidth <<= 1);

	if (outwidth > MAX_TEXTURE_DIMS)
		outwidth = MAX_TEXTURE_DIMS;

	int outheight;
	for (outheight = 1; outheight < ptexture->height; outheight <<= 1);

	if (outheight > MAX_TEXTURE_DIMS)
		outheight = MAX_TEXTURE_DIMS;

	in = (byte *)Mem_Alloc( ptexture->width * ptexture->height * 4);
	if (!in) return;

	if( ptexture->flags & STUDIO_NF_COLORMAP )
	{
		memcpy( pal, srcpal, 768 );

		if( !Q_strnicmp( ptexture->name, "DM_Base", 7 ))
		{
			PaletteHueReplace( pal, g_viewerSettings.topcolor, PLATE_HUE_START, PLATE_HUE_END );
			PaletteHueReplace( pal, g_viewerSettings.bottomcolor, SUIT_HUE_START, SUIT_HUE_END );
		}
		else
		{
			int	len = Q_strlen( ptexture->name );
			int	low, mid, high;
			char	sz[32];
#if 1
			// GoldSource parser
			if( len == 18 || len == 22 )
			{
				char	ch = ptexture->name[5];

				if( len != 18 || ch == 'c' || ch == 'C' )
				{
					Q_strncpy( sz, &ptexture->name[7], 4 );
					low = Q_atoi( sz );
					Q_strncpy( sz, &ptexture->name[11], 4 );
					mid = Q_atoi( sz );

					if( len == 22 )
					{
						Q_strncpy( sz, &ptexture->name[15], 4 );
						high = Q_atoi( sz );
					}
					else high = 0;

					PaletteHueReplace( pal, g_viewerSettings.topcolor, low, mid );
					if( high ) PaletteHueReplace( pal, g_viewerSettings.bottomcolor, mid + 1, high );
				}
			}
#else
			// Xash3D parser
			Q_strncpy( sz, ptexture->name + 7, 4 );  
			low = bound( 0, Q_atoi( sz ), 255 );
			Q_strncpy( sz, ptexture->name + 11, 4 ); 
			mid = bound( 0, Q_atoi( sz ), 255 );
			Q_strncpy( sz, ptexture->name + 15, 4 ); 
			high = bound( 0, Q_atoi( sz ), 255 );

			PaletteHueReplace( pal, g_viewerSettings.topcolor, low, mid );
			if( high ) PaletteHueReplace( pal, g_viewerSettings.bottomcolor, mid + 1, high );
#endif
		}
	}
	else
	{
		memcpy( pal, srcpal, 768 );
	}

	// expand pixels to rgba
	for (i=0 ; i < ptexture->width * ptexture->height; i++)
	{
		if(( ptexture->flags & STUDIO_NF_MASKED ) && data[i] == 255 )
		{
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

	tex = out = (byte *)Mem_Alloc( outwidth * outheight * 4);
	if (!out)
	{
		return;
	}

	for (i = 0; i < outwidth; i++)
	{
		col1[i] = (int) ((i + 0.25) * (ptexture->width / (float)outwidth));
		col2[i] = (int) ((i + 0.75) * (ptexture->width / (float)outwidth));
	}

	for (i = 0; i < outheight; i++)
	{
		row1[i] = (int) ((i + 0.25) * (ptexture->height / (float)outheight)) * ptexture->width;
		row2[i] = (int) ((i + 0.75) * (ptexture->height / (float)outheight)) * ptexture->width;
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

	GLint outFormat = GL_RGB;

	if( ptexture->flags & STUDIO_NF_MASKED )
		outFormat = GL_RGBA;

	glBindTexture( GL_TEXTURE_2D, name );		
	glHint( GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST );
	glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE );
	glTexImage2D( GL_TEXTURE_2D, 0, outFormat, outwidth, outheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
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

void StudioModel :: FreeModel( void )
{
	if( g_viewerSettings.numModelChanges )
	{
		if( !mxMessageBox( g_GlWindow, "Model has changes. Do you wish to save them?", APP_TITLE_STR, MX_MB_YESNO | MX_MB_QUESTION ))
		{
			char *ptr = (char *)mxGetSaveFileName( g_GlWindow, g_viewerSettings.modelPath, "GoldSrc Model (*.mdl)" );
			if( ptr )
			{
				char filename[256];
				char ext[16];

				strcpy( filename, ptr );
				strcpy( ext, mx_getextension( filename ));
				if( mx_strcasecmp( ext, ".mdl" ))
					strcat( filename, ".mdl" );

				if( !g_studioModel.SaveModel( filename ))
					mxMessageBox( g_GlWindow, "Error saving model.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
			}
		}

		g_viewerSettings.numModelChanges = 0;	// all the settings are handled or invalidated
	}

	if (m_pJiggleBones)
	{
		delete m_pJiggleBones;
		m_pJiggleBones = NULL;
	}

	int i;

	if( m_ptexturehdr )
	{
		// deleting textures
		int textures[MAXSTUDIOSKINS];
		for (i = 0; i < m_ptexturehdr->numtextures; i++)
			textures[i] = g_tex_base + i;

		glDeleteTextures (m_ptexturehdr->numtextures, (const GLuint *)textures);
	}

	if (m_pstudiohdr)
		Mem_Free(m_pstudiohdr);

	if (m_ptexturehdr && m_owntexmodel)
		Mem_Free(m_ptexturehdr);

	g_boneSetup.SetStudioPointers( NULL, NULL );

	g_viewerSettings.numSourceChanges = 0;
	m_pstudiohdr = m_ptexturehdr = 0;
	remap_textures = false;
	m_owntexmodel = false;
	m_numeditfields = 0;

	for (i = 0; i < m_panimhdr.size(); i++)
	{
		if (m_panimhdr[i])
		{
			Mem_Free(m_panimhdr[i]);
			m_panimhdr[i] = 0;
		}
	}
}

void StudioModel::RemapTextures( void )
{
	if( !remap_textures ) return;

	studiohdr_t *phdr = getTextureHeader();
	mstudiotexture_t *ptexture;
	byte *pin = (byte *)phdr;

	if (phdr && phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS)
	{
		ptexture = (mstudiotexture_t *)(pin + phdr->textureindex);

		for (int i = 0; i < phdr->numtextures; i++)
		{
			if( !( ptexture[i].flags & STUDIO_NF_COLORMAP ))
				continue;

			UploadTexture( &ptexture[i], pin + ptexture[i].index, pin + ptexture[i].width * ptexture[i].height + ptexture[i].index, g_tex_base + i );
		}
	}

	remap_textures = false;
}

studiohdr_t *StudioModel::LoadModel( char *modelname )
{
	FILE *fp;
	int size;
	void *buffer;

	if (!modelname)
		return 0;

	// load the model
	if( (fp = fopen( modelname, "rb" )) == NULL)
		return 0;

	fseek( fp, 0, SEEK_END );
	size = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	if (size <= 0 || (buffer = Mem_Alloc(size)) == nullptr)
	{
		fclose(fp);
		return 0;
	}

	fread( buffer, size, 1, fp );
	fclose( fp );

	byte				*pin;
	studiohdr_t			*phdr;
	mstudiotexture_t	*ptexture;

	pin = (byte *)buffer;
	phdr = (studiohdr_t *)pin;
	ptexture = (mstudiotexture_t *)(pin + phdr->textureindex);

	if (strncmp ((const char *) buffer, "IDST", 4) &&
		strncmp ((const char *) buffer, "IDSQ", 4))
	{
		if (!strncmp ((const char *) buffer, "IDPO", 4 ))
			mxMessageBox( g_GlWindow, "Quake 1 models doesn't supported.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR );
		else mxMessageBox( g_GlWindow, "Unknown file format.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR );

		Mem_Free(buffer);
		return 0;
	}

	if (!strncmp ((const char *) buffer, "IDSQ", 4) && !m_pstudiohdr)
	{
		Mem_Free(buffer);
		return 0;
	}

	if( phdr->version != STUDIO_VERSION )
	{
		mxMessageBox( g_GlWindow, "Unsupported studio version.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR );
		Mem_Free(buffer);
		return 0;
	}

	if (phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS)
	{
		for (int i = 0; i < phdr->numtextures; i++)
		{
			if( !Q_strnicmp( ptexture[i].name, "DM_Base", 7 ) || !Q_strnicmp( ptexture[i].name, "remap", 5 ))
				ptexture[i].flags |= STUDIO_NF_COLORMAP;

			UploadTexture( &ptexture[i], pin + ptexture[i].index, pin + ptexture[i].width * ptexture[i].height + ptexture[i].index, g_tex_base + i );
		}
	}

	m_pJiggleBones = NULL;
	m_numeditfields = 0;

	if (!m_pstudiohdr)
		m_pstudiohdr = (studiohdr_t *)buffer;

	return (studiohdr_t *)buffer;
}

bool StudioModel::PostLoadModel( char *modelname )
{
	// preload textures
	if (m_pstudiohdr->numtextures == 0)
	{
		char texturename[256];

		strcpy( texturename, modelname );
		strcpy( &texturename[strlen(texturename) - 4], "T.mdl" );

		m_ptexturehdr = LoadModel( texturename );
		if (!m_ptexturehdr)
		{
			FreeModel ();
			return false;
		}
		m_owntexmodel = true;
	}
	else
	{
		m_ptexturehdr = m_pstudiohdr;
		m_owntexmodel = false;
	}

	// preload animations
	if (m_pstudiohdr->numseqgroups > 1)
	{
		for (int i = 1; i < m_pstudiohdr->numseqgroups; i++)
		{
			char seqgroupname[256];

			strcpy( seqgroupname, modelname );
			sprintf( &seqgroupname[strlen(seqgroupname) - 4], "%02d.mdl", i );

			m_panimhdr[i] = LoadModel( seqgroupname );
			if (!m_panimhdr[i])
			{
				FreeModel ();
				return false;
			}
		}
	}

	SetSequence (0);
	SetController (0, 0.0f);
	SetController (1, 0.0f);
	SetController (2, 0.0f);
	SetController (3, 0.0f);
	SetMouth (0.0f);

	int n;
	for (n = 0; n < m_pstudiohdr->numbodyparts; n++)
		SetBodygroup (n, 0);

	SetSkin (0);

	char	basename[64];

	COM_FileBase( modelname, basename );

	if( !Q_strnicmp( basename, "v_", 2 ))
	{
		g_MDLViewer->checkboxSet( IDC_OPTIONS_WEAPONORIGIN, true );
		bUseWeaponOrigin = true;
	}
	else
	{
		g_MDLViewer->checkboxSet( IDC_OPTIONS_WEAPONORIGIN, false );
		bUseWeaponOrigin = false;
	}

	// reset all the changes
	g_viewerSettings.numModelChanges = 0;
	g_viewerSettings.numSourceChanges = 0;
	bUseParanoiaFOV = false;

	g_boneSetup.SetStudioPointers( m_pstudiohdr, m_poseparameter );

	// set poseparam sliders to their default values
	g_boneSetup.CalcDefaultPoseParameters( m_poseparameter );

	if( FBitSet( m_pstudiohdr->flags, STUDIO_HAS_BONEINFO ))
	{
		// NOTE: extended boneinfo goes immediately after bones
		mstudioboneinfo_t *boneinfo = (mstudioboneinfo_t *)((byte *)m_pstudiohdr + m_pstudiohdr->boneindex + m_pstudiohdr->numbones * sizeof( mstudiobone_t ));

		for( n = 0; n < m_pstudiohdr->numbones; n++ )
			LoadLocalMatrix( n, &boneinfo[n] );
	}

	// analyze ACT_VM_ for paranoia viewmodels
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex);

	for( n = 0; n < m_pstudiohdr->numseq; n++ )
	{
		if( pseqdesc[n].activity >= ACT_VM_NONE && pseqdesc[n].activity <= ACT_VM_RESERVED4 )
		{
			g_MDLViewer->checkboxSet( IDC_OPTIONS_WEAPONORIGIN, true );
			bUseWeaponOrigin = bUseParanoiaFOV = true;
			break; // it's P2 viewmodel!
		}
	}

	return true;
}

void StudioModel :: LoadLocalMatrix( int bone, mstudioboneinfo_t *boneinfo )
{
	// transform Valve matrix to Xash matrix
	m_plocaltransform[bone][0][0] = boneinfo->poseToBone[0][0];
	m_plocaltransform[bone][0][1] = boneinfo->poseToBone[1][0];
	m_plocaltransform[bone][0][2] = boneinfo->poseToBone[2][0];

	m_plocaltransform[bone][1][0] = boneinfo->poseToBone[0][1];
	m_plocaltransform[bone][1][1] = boneinfo->poseToBone[1][1];
	m_plocaltransform[bone][1][2] = boneinfo->poseToBone[2][1];

	m_plocaltransform[bone][2][0] = boneinfo->poseToBone[0][2];
	m_plocaltransform[bone][2][1] = boneinfo->poseToBone[1][2];
	m_plocaltransform[bone][2][2] = boneinfo->poseToBone[2][2];

	m_plocaltransform[bone][3][0] = boneinfo->poseToBone[0][3];
	m_plocaltransform[bone][3][1] = boneinfo->poseToBone[1][3];
	m_plocaltransform[bone][3][2] = boneinfo->poseToBone[2][3];
}

bool StudioModel::SaveModel ( char *modelname )
{
	if (!modelname)
		return false;

	if (!m_pstudiohdr)
		return false;

	FILE *file;
	
	file = fopen (modelname, "wb");
	if (!file)
		return false;

	fwrite (m_pstudiohdr, sizeof (byte), m_pstudiohdr->length, file);
	fclose (file);

	// write texture model
	if (m_owntexmodel && m_ptexturehdr)
	{
		char texturename[256];

		strcpy( texturename, modelname );
		strcpy( &texturename[strlen(texturename) - 4], "T.mdl" );

		file = fopen (texturename, "wb");
		if (file)
		{
			fwrite (m_ptexturehdr, sizeof (byte), m_ptexturehdr->length, file);
			fclose (file);
		}
	}

	// write seq groups
	if (m_pstudiohdr->numseqgroups > 1)
	{
		for (int i = 1; i < m_pstudiohdr->numseqgroups; i++)
		{
			char seqgroupname[256];

			strcpy( seqgroupname, modelname );
			sprintf( &seqgroupname[strlen(seqgroupname) - 4], "%02d.mdl", i );

			file = fopen (seqgroupname, "wb");
			if (file)
			{
				fwrite (m_panimhdr[i], sizeof (byte), m_panimhdr[i]->length, file);
				fclose (file);
			}
		}
	}

	return true;
}

void StudioModel::PaletteHueReplace( byte *palSrc, int newHue, int start, int end )
{
	float	r, g, b;
	float	maxcol, mincol;
	float	hue, val, sat;
	int	i;

	hue = (float)(newHue * ( 360.0f / 255 ));

	for( i = start; i <= end; i++ )
	{
		r = palSrc[i*3+0];
		g = palSrc[i*3+1];
		b = palSrc[i*3+2];
		
		maxcol = Q_max( Q_max( r, g ), b ) / 255.0f;
		mincol = Q_min( Q_min( r, g ), b ) / 255.0f;

		if( maxcol == 0 ) continue;
		
		val = maxcol;
		sat = (maxcol - mincol) / maxcol;

		mincol = val * (1.0f - sat);

		if( hue <= 120.0f )
		{
			b = mincol;
			if( hue < 60 )
			{
				r = val;
				g = mincol + hue * (val - mincol) / (120.0f - hue);
			}
			else
			{
				g = val;
				r = mincol + (120.0f - hue) * (val - mincol) / hue;
			}
		}
		else if( hue <= 240.0f )
		{
			r = mincol;
			if( hue < 180.0f )
			{
				g = val;
				b = mincol + (hue - 120.0f) * (val - mincol) / (240.0f - hue);
			}
			else
			{
				b = val;
				g = mincol + (240.0f - hue) * (val - mincol) / (hue - 120.0f);
			}
		}
		else
		{
			g = mincol;
			if( hue < 300.0f )
			{
				b = val;
				r = mincol + (hue - 240.0f) * (val - mincol) / (360.0f - hue);
			}
			else
			{
				r = val;
				b = mincol + (360.0f - hue) * (val - mincol) / (hue - 240.0f);
			}
		}

		palSrc[i*3+0] = (byte)(r * 255);
		palSrc[i*3+1] = (byte)(g * 255);
		palSrc[i*3+2] = (byte)(b * 255);
	}
}

////////////////////////////////////////////////////////////////////////

int StudioModel::GetSequence( )
{
	return m_sequence;
}

int StudioModel :: SetSequence( int iSequence )
{
	if (iSequence > m_pstudiohdr->numseq)
		return m_sequence;

	mstudioseqdesc_t *poldseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + m_sequence;
	mstudioseqdesc_t *pnewseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + iSequence;

	// sequence has changed, hold the previous sequence info
	if( iSequence != m_sequence && !bUseWeaponOrigin && !FBitSet( pnewseqdesc->flags, STUDIO_SNAP ))
	{
		blend_sequence_t	*seqblending;

		// move current sequence into circular buffer
		m_current_seqblend = (m_current_seqblend + 1) & MASK_SEQBLENDS;

		seqblending = &m_seqblend[m_current_seqblend];

		seqblending->blendtime = m_flTime;
		seqblending->sequence = m_sequence;
		seqblending->cycle = Q_min( m_cycle, 1.0f );
		seqblending->fadeout = Q_min( poldseqdesc->fadeouttime / 100.0f, pnewseqdesc->fadeintime / 100.0f );
		if( seqblending->fadeout <= 0.0f ) seqblending->fadeout = 0.2f; // force to default

		// save current blends to right lerping from last sequence
		for( int i = 0; i < 2; i++ )
			seqblending->blending[i] = m_blending[i];
	}

	m_sequence = iSequence;
	sequence_reset = true;
	m_cycle = 0;

	return m_sequence;
}


void StudioModel :: ExtractBbox( Vector &mins, Vector &maxs )
{
	mstudioseqdesc_t	*pseqdesc;

	if( !m_pstudiohdr || m_pstudiohdr->numseq <= 0 )
	{
		mins = maxs = g_vecZero;
		return;
	}

	m_sequence = bound( 0, m_sequence, m_pstudiohdr->numseq - 1 );

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + m_sequence;
	mins = pseqdesc->bbmin;
	maxs = pseqdesc->bbmax;
}

float StudioModel :: GetDuration( int iSequence )
{
	return g_boneSetup.LocalDuration( iSequence );
}

float StudioModel :: GetDuration( void )
{
	return GetDuration( m_sequence );
}

void StudioModel::GetSequenceInfo( float *pflFrameRate, float *pflGroundSpeed )
{
	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + (int)m_sequence;

	if (pseqdesc->numframes > 1)
	{
		*pflFrameRate = 256 * pseqdesc->fps / (pseqdesc->numframes - 1);
		*pflGroundSpeed = sqrt( pseqdesc->linearmovement[0]*pseqdesc->linearmovement[0]+ pseqdesc->linearmovement[1]*pseqdesc->linearmovement[1]+ pseqdesc->linearmovement[2]*pseqdesc->linearmovement[2] );
		*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		*pflFrameRate = 256.0;
		*pflGroundSpeed = 0.0;
	}
}

void StudioModel :: GetMovement( float &prevCycle, Vector &vecPos, Vector &vecAngles )
{
	if( !m_pstudiohdr )
	{
		vecPos.Init();
		vecAngles.Init();
		return;
	}
 
  	// assume that changes < -0.5 are loops....
  	if( m_cycle - prevCycle < -0.5f )
  	{
  		prevCycle = prevCycle - 1.0f;
  	}
 
	g_boneSetup.SeqMovement( m_sequence, prevCycle, m_cycle, vecPos, vecAngles );
	prevCycle = m_cycle;
}

int StudioModel :: getNumBlendings( void )
{
	if( !m_pstudiohdr ) return 0;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + m_sequence;

	return pseqdesc->numblends;
}

int StudioModel :: hasLocalBlending( void )
{
	if( !m_pstudiohdr ) return 0;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + m_sequence;

	return ( pseqdesc->numblends > 1 ) && !FBitSet( pseqdesc->flags, STUDIO_BLENDPOSE );
}

float StudioModel::SetController( int iController, float flValue )
{
	int i;
	if (!m_pstudiohdr)
		return 0.0f;

	mstudiobonecontroller_t	*pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bonecontrollerindex);

	// find first controller that matches the index
	for (i = 0; i < m_pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
	{
		if (pbonecontroller->index == iController)
			break;
	}
	if (i >= m_pstudiohdr->numbonecontrollers)
		return flValue;

	// wrap 0..360 if it's a rotational controller
	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pbonecontroller->end < pbonecontroller->start)
			flValue = -flValue;

		// does the controller not wrap?
		if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
		{
			if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
				flValue = flValue + 360;
		}
		else
		{
			if (flValue > 360)
				flValue = flValue - (int)(flValue / 360.0) * 360.0;
			else if (flValue < 0)
				flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
		}
	}

	int setting = (int) (255 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start));

	if (setting < 0) setting = 0;
	if (setting > 255) setting = 255;
	m_controller[iController] = setting;

	return setting * (1.0 / 255.0) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}

int StudioModel :: LookupPoseParameter( char const *szName )
{
	if( !m_pstudiohdr )
		return false;

	for( int iParameter = 0; iParameter < g_boneSetup.CountPoseParameters(); iParameter++ )
	{
		const mstudioposeparamdesc_t *pPose = g_boneSetup.pPoseParameter( iParameter );

		if( !Q_stricmp( szName, pPose->name ))
		{
			return iParameter;
		}
	}

	return -1;
}

float StudioModel::SetPoseParameter( char const *szName, float flValue )
{
	return SetPoseParameter( LookupPoseParameter( szName ), flValue );
}

float StudioModel::SetPoseParameter( int iParameter, float flValue )
{
	if( !m_pstudiohdr )
		return 0.0f;

	return g_boneSetup.SetPoseParameter( iParameter, flValue, m_poseparameter[iParameter] );
}

float StudioModel::GetPoseParameter( char const *szName )
{
	return GetPoseParameter( LookupPoseParameter( szName ));
}

float* StudioModel::GetPoseParameters()
{
	return m_poseparameter;
}

float StudioModel::GetPoseParameter( int iParameter )
{
	if( !m_pstudiohdr )
		return 0.0f;

	return g_boneSetup.GetPoseParameter( iParameter, m_poseparameter[iParameter] );
}

bool StudioModel::GetPoseParameterRange( int iParameter, float *pflMin, float *pflMax )
{
	*pflMin = 0;
	*pflMax = 0;

	if( !m_pstudiohdr )
		return false;

	if( iParameter < 0 || iParameter >= g_boneSetup.CountPoseParameters( ))
		return false;

	const mstudioposeparamdesc_t *pPose = g_boneSetup.pPoseParameter( iParameter );

	*pflMin = pPose->start;
	*pflMax = pPose->end;

	return true;
}

float StudioModel::SetMouth( float flValue )
{
	if (!m_pstudiohdr)
		return 0.0f;

	mstudiobonecontroller_t	*pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bonecontrollerindex);

	// find first controller that matches the mouth
	for (int i = 0; i < m_pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
	{
		if (pbonecontroller->index == STUDIO_MOUTH)
			break;
	}

	// wrap 0..360 if it's a rotational controller
	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pbonecontroller->end < pbonecontroller->start)
			flValue = -flValue;

		// does the controller not wrap?
		if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
		{
			if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
				flValue = flValue + 360;
		}
		else
		{
			if (flValue > 360)
				flValue = flValue - (int)(flValue / 360.0) * 360.0;
			else if (flValue < 0)
				flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
		}
	}

	int setting = (int) (64 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start));

	if (setting < 0) setting = 0;
	if (setting > 64) setting = 64;
	m_mouth = setting;

	return setting * (1.0 / 64.0) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}

void StudioModel :: SetBlendValue( int iBlender, int iValue )
{
	m_blending[iBlender] = bound( 0, iValue, 255 );

	if( hasLocalBlending( ))
		m_poseparameter[iBlender] = bound( 0.0f, (float)iValue / 255.0f, 1.0f );
}

float StudioModel::SetBlending( int iBlender, float flValue )
{
	mstudioseqdesc_t *pseqdesc;
	int iOutBlend = iBlender;

	if (!m_pstudiohdr)
		return 0.0f;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + (int)m_sequence;

	if( pseqdesc->numblends == 4 || pseqdesc->numblends == 9 )
		iBlender = 0; // grab info from first blender

	if (pseqdesc->blendtype[iBlender] == 0)
		return flValue;

	if (pseqdesc->blendtype[iBlender] & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender])
			flValue = -flValue;

		// does the controller not wrap?
		if (pseqdesc->blendstart[iBlender] + 359.0 >= pseqdesc->blendend[iBlender])
		{
			if (flValue > ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) - 180)
				flValue = flValue + 360;
		}
	}

	int setting = (int) (255 * (flValue - pseqdesc->blendstart[iBlender]) / (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]));

	if (setting < 0) setting = 0;
	if (setting > 255) setting = 255;

	m_blending[iOutBlend] = setting;

	if( hasLocalBlending( ))
		m_poseparameter[iOutBlend] = bound( 0.0f, (float)setting / 255.0f, 1.0f );

//	return setting * (1.0 / 255.0) * (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]) + pseqdesc->blendstart[iBlender];

	return setting;
}

int StudioModel::SetBodygroup( int iGroup, int iValue )
{
	if (!m_pstudiohdr)
		return 0;

	if (iGroup > m_pstudiohdr->numbodyparts)
		return -1;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex) + iGroup;

	int iCurrent = (m_bodynum / pbodypart->base) % pbodypart->nummodels;

	if (iValue >= pbodypart->nummodels)
		return iCurrent;

	m_bodynum = (m_bodynum - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));

	return m_bodynum;
}


int StudioModel::SetSkin( int iValue )
{
	if (!m_pstudiohdr)
		return 0;

	if (iValue >= m_pstudiohdr->numskinfamilies)
	{
		return m_skinnum;
	}

	m_skinnum = iValue;

	return iValue;
}

void StudioModel::ComputeWeightColor( mstudioboneweight_t *boneweights, Vector &result )
{
	int	numbones = 0;

	for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
	{
		if( boneweights->bone[i] != -1 )
			numbones++;
	}

	if( !g_viewerSettings.studio_blendweights )
		numbones = 1;

	switch( numbones )
	{
	case 0:
		result = Vector( 0.0f, 0.0f, 0.0f );
		break;
	case 1:
		result = Vector( 0.0f, 1.0f, 0.0f );
		break;
	case 2:
		result = Vector( 1.0f, 1.0f, 0.0f );
		break;
	case 3:
		result = Vector( 1.0f, 0.0f, 1.0f );
		break;
	case 4:
		result = Vector( 1.0f, 0.0f, 1.0f );
		break;
	default:
		result = Vector( 1.0f, 1.0f, 1.0f );
		break;
	}
}

void StudioModel::ComputeSkinMatrix( mstudioboneweight_t *boneweights, matrix3x4 &result )
{
	float	flWeight0, flWeight1, flWeight2, flWeight3;
	int	numbones = 0;

	for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
	{
		if( boneweights->bone[i] != -1 )
			numbones++;
	}

	if( !g_viewerSettings.studio_blendweights )
		numbones = 1;

	if( numbones == 4 )
	{
		matrix3x4 &boneMat0 = m_pworldtransform[boneweights->bone[0]];
		matrix3x4 &boneMat1 = m_pworldtransform[boneweights->bone[1]];
		matrix3x4 &boneMat2 = m_pworldtransform[boneweights->bone[2]];
		matrix3x4 &boneMat3 = m_pworldtransform[boneweights->bone[3]];
		flWeight0 = boneweights->weight[0] / 255.0f;
		flWeight1 = boneweights->weight[1] / 255.0f;
		flWeight2 = boneweights->weight[2] / 255.0f;
		flWeight3 = boneweights->weight[3] / 255.0f;

		result[0][0] = boneMat0[0][0] * flWeight0 + boneMat1[0][0] * flWeight1 + boneMat2[0][0] * flWeight2 + boneMat3[0][0] * flWeight3;
		result[0][1] = boneMat0[0][1] * flWeight0 + boneMat1[0][1] * flWeight1 + boneMat2[0][1] * flWeight2 + boneMat3[0][1] * flWeight3;
		result[0][2] = boneMat0[0][2] * flWeight0 + boneMat1[0][2] * flWeight1 + boneMat2[0][2] * flWeight2 + boneMat3[0][2] * flWeight3;
		result[1][0] = boneMat0[1][0] * flWeight0 + boneMat1[1][0] * flWeight1 + boneMat2[1][0] * flWeight2 + boneMat3[1][0] * flWeight3;
		result[1][1] = boneMat0[1][1] * flWeight0 + boneMat1[1][1] * flWeight1 + boneMat2[1][1] * flWeight2 + boneMat3[1][1] * flWeight3;
		result[1][2] = boneMat0[1][2] * flWeight0 + boneMat1[1][2] * flWeight1 + boneMat2[1][2] * flWeight2 + boneMat3[1][2] * flWeight3;
		result[2][0] = boneMat0[2][0] * flWeight0 + boneMat1[2][0] * flWeight1 + boneMat2[2][0] * flWeight2 + boneMat3[2][0] * flWeight3;
		result[2][1] = boneMat0[2][1] * flWeight0 + boneMat1[2][1] * flWeight1 + boneMat2[2][1] * flWeight2 + boneMat3[2][1] * flWeight3;
		result[2][2] = boneMat0[2][2] * flWeight0 + boneMat1[2][2] * flWeight1 + boneMat2[2][2] * flWeight2 + boneMat3[2][2] * flWeight3;
		result[3][0] = boneMat0[3][0] * flWeight0 + boneMat1[3][0] * flWeight1 + boneMat2[3][0] * flWeight2 + boneMat3[3][0] * flWeight3;
		result[3][1] = boneMat0[3][1] * flWeight0 + boneMat1[3][1] * flWeight1 + boneMat2[3][1] * flWeight2 + boneMat3[3][1] * flWeight3;
		result[3][2] = boneMat0[3][2] * flWeight0 + boneMat1[3][2] * flWeight1 + boneMat2[3][2] * flWeight2 + boneMat3[3][2] * flWeight3;
	}
	else if( numbones == 3 )
	{
		matrix3x4 &boneMat0 = m_pworldtransform[boneweights->bone[0]];
		matrix3x4 &boneMat1 = m_pworldtransform[boneweights->bone[1]];
		matrix3x4 &boneMat2 = m_pworldtransform[boneweights->bone[2]];
		flWeight0 = boneweights->weight[0] / 255.0f;
		flWeight1 = boneweights->weight[1] / 255.0f;
		flWeight2 = boneweights->weight[2] / 255.0f;

		result[0][0] = boneMat0[0][0] * flWeight0 + boneMat1[0][0] * flWeight1 + boneMat2[0][0] * flWeight2;
		result[0][1] = boneMat0[0][1] * flWeight0 + boneMat1[0][1] * flWeight1 + boneMat2[0][1] * flWeight2;
		result[0][2] = boneMat0[0][2] * flWeight0 + boneMat1[0][2] * flWeight1 + boneMat2[0][2] * flWeight2;
		result[1][0] = boneMat0[1][0] * flWeight0 + boneMat1[1][0] * flWeight1 + boneMat2[1][0] * flWeight2;
		result[1][1] = boneMat0[1][1] * flWeight0 + boneMat1[1][1] * flWeight1 + boneMat2[1][1] * flWeight2;
		result[1][2] = boneMat0[1][2] * flWeight0 + boneMat1[1][2] * flWeight1 + boneMat2[1][2] * flWeight2;
		result[2][0] = boneMat0[2][0] * flWeight0 + boneMat1[2][0] * flWeight1 + boneMat2[2][0] * flWeight2;
		result[2][1] = boneMat0[2][1] * flWeight0 + boneMat1[2][1] * flWeight1 + boneMat2[2][1] * flWeight2;
		result[2][2] = boneMat0[2][2] * flWeight0 + boneMat1[2][2] * flWeight1 + boneMat2[2][2] * flWeight2;
		result[3][0] = boneMat0[3][0] * flWeight0 + boneMat1[3][0] * flWeight1 + boneMat2[3][0] * flWeight2;
		result[3][1] = boneMat0[3][1] * flWeight0 + boneMat1[3][1] * flWeight1 + boneMat2[3][1] * flWeight2;
		result[3][2] = boneMat0[3][2] * flWeight0 + boneMat1[3][2] * flWeight1 + boneMat2[3][2] * flWeight2;
	}
	else if( numbones == 2 )
	{
		matrix3x4 &boneMat0 = m_pworldtransform[boneweights->bone[0]];
		matrix3x4 &boneMat1 = m_pworldtransform[boneweights->bone[1]];
		flWeight0 = boneweights->weight[0] / 255.0f;
		flWeight1 = boneweights->weight[1] / 255.0f;

		// NOTE: Inlining here seems to make a fair amount of difference
		result[0][0] = boneMat0[0][0] * flWeight0 + boneMat1[0][0] * flWeight1;
		result[0][1] = boneMat0[0][1] * flWeight0 + boneMat1[0][1] * flWeight1;
		result[0][2] = boneMat0[0][2] * flWeight0 + boneMat1[0][2] * flWeight1;
		result[1][0] = boneMat0[1][0] * flWeight0 + boneMat1[1][0] * flWeight1;
		result[1][1] = boneMat0[1][1] * flWeight0 + boneMat1[1][1] * flWeight1;
		result[1][2] = boneMat0[1][2] * flWeight0 + boneMat1[1][2] * flWeight1;
		result[2][0] = boneMat0[2][0] * flWeight0 + boneMat1[2][0] * flWeight1;
		result[2][1] = boneMat0[2][1] * flWeight0 + boneMat1[2][1] * flWeight1;
		result[2][2] = boneMat0[2][2] * flWeight0 + boneMat1[2][2] * flWeight1;
		result[3][0] = boneMat0[3][0] * flWeight0 + boneMat1[3][0] * flWeight1;
		result[3][1] = boneMat0[3][1] * flWeight0 + boneMat1[3][1] * flWeight1;
		result[3][2] = boneMat0[3][2] * flWeight0 + boneMat1[3][2] * flWeight1;
	}
	else
	{
		result = m_pworldtransform[boneweights->bone[0]];
	}
}

void StudioModel::scaleMeshes (float scale)
{
	if (!m_pstudiohdr)
		return;

	int i, j, k;

	// scale verts
	int tmp = m_bodynum;
	for (i = 0; i < m_pstudiohdr->numbodyparts; i++)
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex) + i;
		for (j = 0; j < pbodypart->nummodels; j++)
		{
			SetBodygroup (i, j);
			SetupModel (i);

			Vector *pstudioverts = (Vector *)((byte *)m_pstudiohdr + m_pmodel->vertindex);

			for (k = 0; k < m_pmodel->numverts; k++)
				pstudioverts[k] *= scale;
		}
	}

	m_bodynum = tmp;

	// scale complex hitboxes
	mstudiobbox_t *pbboxes = (mstudiobbox_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->hitboxindex);
	for (i = 0; i < m_pstudiohdr->numhitboxes; i++)
	{
		pbboxes[i].bbmin *= scale;
		pbboxes[i].bbmax *= scale;
	}

	// scale bounding boxes
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex);
	for (i = 0; i < m_pstudiohdr->numseq; i++)
	{
		pseqdesc[i].bbmin *= scale;
		pseqdesc[i].bbmax *= scale;
	}

	// maybe scale exeposition, pivots, attachments
	g_viewerSettings.numModelChanges++;
}



void StudioModel::scaleBones (float scale)
{
	if (!m_pstudiohdr)
		return;

	mstudiobone_t *pbones = (mstudiobone_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->boneindex);
	for (int i = 0; i < m_pstudiohdr->numbones; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			pbones[i].value[j] *= scale;
			pbones[i].scale[j] *= scale;
		}
	}	

	g_viewerSettings.numModelChanges++;
}

void StudioModel::SetTopColor( int color )
{
	if( g_viewerSettings.topcolor != color )
		remap_textures = true;
	g_viewerSettings.topcolor = color;
}

void StudioModel::SetBottomColor( int color )
{
	if( g_viewerSettings.bottomcolor != color )
		remap_textures = true;
	g_viewerSettings.bottomcolor = color;
}

bool StudioModel::SetEditType( int type )
{
	if( type < 0 || type >= m_numeditfields )
	{
		m_pedit = NULL;
		return false;
	}

	m_pedit = &m_editfields[type];

	// is allowed to edit size
	if( m_pedit->type == TYPE_BBOX || m_pedit->type == TYPE_CBOX || m_pedit->type == TYPE_HITBOX )
		return true;
	return false;
}

bool StudioModel::SetEditMode( int mode )
{
	char	str[256];

	if( mode == EDIT_MODEL && g_viewerSettings.editMode == EDIT_SOURCE && g_viewerSettings.numSourceChanges > 0 )
	{
		Q_strcpy( str, "we have some virtual changes for QC-code.\nApply them to real model or all the changes will be lost?" );
		int ret = mxMessageBox( g_GlWindow, str, APP_TITLE_STR, MX_MB_YESNOCANCEL | MX_MB_QUESTION );

		if( ret == 2 ) return false;	// cancelled
	
		if( ret == 0 ) UpdateEditFields( true );
		else UpdateEditFields( false );
	}

	g_viewerSettings.editMode = mode;
	return true;
}

void StudioModel::ReadEditField( studiohdr_t *phdr, edit_field_t *ed )
{
	if( !ed || !phdr ) return;

	// get initial values
	switch( ed->type )
	{
	case TYPE_ORIGIN:
		ed->origin = g_vecZero; // !!!
		break;
	case TYPE_BBOX:
		ed->mins = phdr->min;
		ed->maxs = phdr->max;
		break;
	case TYPE_CBOX:
		ed->mins = phdr->bbmin;
		ed->maxs = phdr->bbmax;
		break;
	case TYPE_EYEPOSITION:
		ed->origin = phdr->eyeposition;
		break;
	case TYPE_ATTACHMENT:
		{
			mstudioattachment_t *pattachment = (mstudioattachment_t *) ((byte *)phdr + phdr->attachmentindex) + ed->id;
			ed->origin = pattachment->org;
			ed->bone = pattachment->bone;
		}
		break;
	case TYPE_HITBOX:
		{
			mstudiobbox_t *phitbox = (mstudiobbox_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->hitboxindex) + ed->id;
			ed->hitgroup = phitbox->group;
			ed->mins = phitbox->bbmin;
			ed->maxs = phitbox->bbmax;
			ed->bone = phitbox->bone;
		}
		break;
	}
}

void StudioModel::WriteEditField( studiohdr_t *phdr, edit_field_t *ed )
{
	if( !ed || !phdr ) return;

	// get initial values
	switch( ed->type )
	{
	case TYPE_ORIGIN:
		{
			mstudiobone_t *pbone = (mstudiobone_t *)((byte *)m_pstudiohdr + m_pstudiohdr->boneindex);

			for( int i = 0; i < m_pstudiohdr->numbones; i++, pbone++ )
			{
				// studioformat is allow multiple root bones...
				if( pbone->parent != -1 ) continue;

				pbone->value[0] += ed->origin.x;
				pbone->value[1] += ed->origin.y;
				pbone->value[2] += ed->origin.z;
			}
			ed->origin = g_vecZero;
		}
		break;
	case TYPE_BBOX:
		phdr->min = ed->mins;
		phdr->max = ed->maxs;
		break;
	case TYPE_CBOX:
		phdr->bbmin = ed->mins;
		phdr->bbmax = ed->maxs;
		break;
	case TYPE_EYEPOSITION:
		phdr->eyeposition = ed->origin;
		break;
	case TYPE_ATTACHMENT:
		{
			mstudioattachment_t *pattachment = (mstudioattachment_t *) ((byte *)phdr + phdr->attachmentindex) + ed->id;
			pattachment->org = ed->origin;
			pattachment->bone = ed->bone;
		}
		break;
	case TYPE_HITBOX:
		{
			mstudiobbox_t *phitbox = (mstudiobbox_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->hitboxindex) + ed->id;
			phitbox->group = ed->hitgroup;
			phitbox->bbmin = ed->mins;
			phitbox->bbmax = ed->maxs;
			phitbox->bone = ed->bone;
		}
		break;
	default:	return;
	}

	g_viewerSettings.numModelChanges++;
}

void StudioModel::UpdateEditFields( bool write_to_model )
{
	studiohdr_t *phdr = g_studioModel.getStudioHeader ();

	for( int i = 0; i < m_numeditfields; i++ )
	{
		edit_field_t *ed = &m_editfields[i];

		if( write_to_model )
			WriteEditField( phdr, ed );
		else ReadEditField( phdr, ed );
	}

	if( write_to_model && phdr )
		g_viewerSettings.numSourceChanges = 0;
}

bool StudioModel::AddEditField( int type, int id )
{
	if( m_numeditfields >= MAX_EDITFIELDS )
	{
		mxMessageBox( g_GlWindow, "Edit fields limit exceeded.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR );
		return false;
	}

	studiohdr_t *phdr = g_studioModel.getStudioHeader ();

	if( phdr )
	{
		edit_field_t *ed = &m_editfields[m_numeditfields++];

		// initialize fields
		ed->origin = g_vecZero;
		ed->mins = g_vecZero;
		ed->maxs = g_vecZero;
		ed->hitgroup = 0;
		ed->type = type;
		ed->bone = -1;
		ed->id = id;

		// read initial values from studiomodel
		ReadEditField( phdr, ed );

		return true;
	}

	return false;
}

const char *StudioModel::getQCcode( void )
{
	studiohdr_t *phdr = g_studioModel.getStudioHeader ();
	static char str[256];

	if( !m_pedit ) return "";

	edit_field_t *ed = m_pedit;

	Q_strncpy( str, "not available", sizeof( str ));
	if( !phdr ) return str;

	// get initial values
	switch( ed->type )
	{
	case TYPE_ORIGIN:
		if( g_viewerSettings.editMode == EDIT_SOURCE )
			Q_snprintf( str, sizeof( str ), "$origin %g %g %g", -ed->origin.y, ed->origin.x, -ed->origin.z );
		break;
	case TYPE_BBOX:
		Q_snprintf( str, sizeof( str ), "$bbox %g %g %g %g %g %g", ed->mins.x, ed->mins.y, ed->mins.z, ed->maxs.x, ed->maxs.y, ed->maxs.z );
		break;
	case TYPE_CBOX:
		Q_snprintf( str, sizeof( str ), "$cbox %g %g %g %g %g %g", ed->mins.x, ed->mins.y, ed->mins.z, ed->maxs.x, ed->maxs.y, ed->maxs.z );
		break;
	case TYPE_EYEPOSITION:
		Q_snprintf( str, sizeof( str ), "$eyeposition %g %g %g", ed->origin.x, ed->origin.y, ed->origin.z );
		break;
	case TYPE_ATTACHMENT:
		{
			mstudiobone_t *pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex) + ed->bone;
			mstudioattachment_t *pattachment = (mstudioattachment_t *)((byte *)phdr + phdr->attachmentindex) + ed->id;
			Q_snprintf( str, sizeof( str ), "$attachment \"%s\" \"%s\" %g %g %g", pattachment->name, pbone->name, ed->origin.x, ed->origin.y, ed->origin.z );
		}
		break;
	case TYPE_HITBOX:
		{
			mstudiobone_t *pbone = (mstudiobone_t *)((byte *)m_pstudiohdr + m_pstudiohdr->boneindex) + ed->bone;
			Q_snprintf( str, sizeof( str ), "$hbox %i \"%s\" %g %g %g  %g %g %g", ed->hitgroup, pbone->name, ed->mins.x, ed->mins.y, ed->mins.z, ed->maxs.x, ed->maxs.y, ed->maxs.z );
		}
		break;
	}

	return str;
}

void StudioModel::updateModel( void )
{
	if( !update_model ) return;
	WriteEditField( m_pstudiohdr, m_pedit );
	update_model = false;
}

void StudioModel::editPosition( float step, int type )
{
	if( !m_pedit ) return;

	if( g_viewerSettings.editSize )
	{
		switch( type )
		{
		case IDC_MOVE_PX:
			m_pedit->mins[0] -= step;
			m_pedit->maxs[0] += step;
			break;
		case IDC_MOVE_NX:
			m_pedit->mins[0] += step;
			m_pedit->maxs[0] -= step;
			break;
		case IDC_MOVE_PY:
			m_pedit->mins[1] -= step;
			m_pedit->maxs[1] += step;
			break;
		case IDC_MOVE_NY:
			m_pedit->mins[1] += step;
			m_pedit->maxs[1] -= step;
			break;
		case IDC_MOVE_PZ:
			m_pedit->mins[2] -= step;
			m_pedit->maxs[2] += step;
			break;
		case IDC_MOVE_NZ:
			m_pedit->mins[2] += step;
			m_pedit->maxs[2] -= step;
			break;
		}
	}
	else
	{
		if( m_pedit->type == TYPE_BBOX || m_pedit->type == TYPE_CBOX || m_pedit->type == TYPE_HITBOX )
		{
			switch( type )
			{
			case IDC_MOVE_PX:
				m_pedit->mins[0] += step;
				m_pedit->maxs[0] += step;
				break;
			case IDC_MOVE_NX:
				m_pedit->mins[0] -= step;
				m_pedit->maxs[0] -= step;
				break;
			case IDC_MOVE_PY:
				m_pedit->mins[1] += step;
				m_pedit->maxs[1] += step;
				break;
			case IDC_MOVE_NY:
				m_pedit->mins[1] -= step;
				m_pedit->maxs[1] -= step;
				break;
			case IDC_MOVE_PZ:
				m_pedit->mins[2] += step;
				m_pedit->maxs[2] += step;
				break;
			case IDC_MOVE_NZ:
				m_pedit->mins[2] -= step;
				m_pedit->maxs[2] -= step;
				break;
			}
		}
		else
		{
			switch( type )
			{
			case IDC_MOVE_PX: m_pedit->origin[0] += step; break;
			case IDC_MOVE_NX: m_pedit->origin[0] -= step; break;
			case IDC_MOVE_PY: m_pedit->origin[1] += step; break;
			case IDC_MOVE_NY: m_pedit->origin[1] -= step; break;
			case IDC_MOVE_PZ: m_pedit->origin[2] += step; break;
			case IDC_MOVE_NZ: m_pedit->origin[2] -= step; break;
			}
		}
	}

	if( g_viewerSettings.editMode == EDIT_MODEL )
		update_model = true;
	else g_viewerSettings.numSourceChanges++;
}
