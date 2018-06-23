/*
r_bloom.cpp -  2D lighting post process effect

Copyright (C) Forest Hale
Copyright (C) 2006-2007 German Garcia

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "mathlib.h"

/*
==============================================================================

LIGHT BLOOMS

==============================================================================
*/

static float Diamond8x[8][8] =
{
{ 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, },
{ 0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, },
{ 0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, },
{ 0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, },
{ 0.1f, 0.3f, 0.6f, 0.9f, 0.9f, 0.6f, 0.3f, 0.1f, },
{ 0.0f, 0.2f, 0.4f, 0.6f, 0.6f, 0.4f, 0.2f, 0.0f, },
{ 0.0f, 0.0f, 0.2f, 0.3f, 0.3f, 0.2f, 0.0f, 0.0f, },
{ 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, },
};

static float Diamond6x[6][6] =
{
{ 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, },
{ 0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, },
{ 0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, },
{ 0.1f, 0.5f, 0.9f, 0.9f, 0.5f, 0.1f, },
{ 0.0f, 0.3f, 0.5f, 0.5f, 0.3f, 0.0f, },
{ 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, },
};

static float Diamond4x[4][4] =
{
{ 0.3f, 0.4f, 0.4f, 0.3f, },
{ 0.4f, 0.9f, 0.9f, 0.4f, },
{ 0.4f, 0.9f, 0.9f, 0.4f, },
{ 0.3f, 0.4f, 0.4f, 0.3f, },
};

static int BLOOM_SIZE;

static int r_bloomscreentexture;
static int r_bloomeffecttexture;
static int r_bloombackuptexture;
static int r_bloomdownsamplingtexture;

static int r_screendownsamplingtexture_size;
static int screen_texture_width, screen_texture_height;
static int r_screenbackuptexture_width, r_screenbackuptexture_height;

// current refdef size:
static int curView_x;
static int curView_y;
static int curView_width;
static int curView_height;

// texture coordinates of screen data inside screentexture
static float screenTex_tcw;
static float screenTex_tch;

static int sample_width;
static int sample_height;

// texture coordinates of adjusted textures
static float sampleText_tcw;
static float sampleText_tch;

/*
=================
R_Bloom_InitBackUpTexture
=================
*/
static void R_Bloom_InitBackUpTexture( int width, int height )
{
	r_screenbackuptexture_width = width;
	r_screenbackuptexture_height = height;

	r_bloombackuptexture = CREATE_TEXTURE( "*bloombackuptexture", width, height, NULL, TF_IMAGE ); 
}

/*
=================
R_Bloom_InitEffectTexture
=================
*/
static void R_Bloom_InitEffectTexture( void )
{
	int	limit;
	
	if( r_bloom_sample_size->value < 32.0f )
		CVAR_SET_FLOAT( "r_bloom_sample_size", 32.0f );

	// make sure bloom size doesn't have stupid values
	limit = min( r_bloom_sample_size->value, min( screen_texture_width, screen_texture_height ));

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		BLOOM_SIZE = limit;
	}
	else
	{	// make sure bloom size is a power of 2
		for( BLOOM_SIZE = 32; (BLOOM_SIZE<<1) <= limit; BLOOM_SIZE <<= 1 );
	}

	if( BLOOM_SIZE != r_bloom_sample_size->value )
		CVAR_SET_FLOAT( "r_bloom_sample_size", BLOOM_SIZE );

	r_bloomeffecttexture = CREATE_TEXTURE( "*bloomeffecttexture", BLOOM_SIZE, BLOOM_SIZE, NULL, TF_IMAGE ); 
}

/*
=================
R_Bloom_InitTextures
=================
*/
static void R_Bloom_InitTextures( void )
{
	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		screen_texture_width = glState.width;
		screen_texture_height = glState.height;
	}
	else
	{
		// find closer power of 2 to screen size 
		for( screen_texture_width = 1; screen_texture_width < glState.width; screen_texture_width <<= 1 );
		for( screen_texture_height = 1; screen_texture_height < glState.height; screen_texture_height <<= 1 );
	}

	// disable blooms if we can't handle a texture of that size
	if( screen_texture_width > glConfig.max_2d_texture_size || screen_texture_height > glConfig.max_2d_texture_size )
	{
		screen_texture_width = screen_texture_height = 0;
		CVAR_SET_FLOAT( "r_bloom", 0.0f );
		ALERT( at_warning, "'R_InitBloomScreenTexture' too high resolution for light bloom, effect disabled\n" );
		return;
	}

	r_bloomscreentexture = CREATE_TEXTURE( "*bloomscreentexture", screen_texture_width, screen_texture_height, NULL, TF_IMAGE ); 

	// validate bloom size and init the bloom effect texture
	R_Bloom_InitEffectTexture();

	// if screensize is more than 2x the bloom effect texture, set up for stepped downsampling
	r_bloomdownsamplingtexture = 0;
	r_screendownsamplingtexture_size = 0;

	if(( glState.width > (BLOOM_SIZE * 2) || glState.height > (BLOOM_SIZE * 2)) && !r_bloom_fast_sample->value )
	{
		r_screendownsamplingtexture_size = (int)( BLOOM_SIZE * 2 );
		r_bloomdownsamplingtexture = CREATE_TEXTURE( "*bloomdownsampletexture", r_screendownsamplingtexture_size,
		r_screendownsamplingtexture_size, NULL, TF_IMAGE ); 
	}

	// init the screen backup texture
	if( r_screendownsamplingtexture_size )
		R_Bloom_InitBackUpTexture( r_screendownsamplingtexture_size, r_screendownsamplingtexture_size );
	else R_Bloom_InitBackUpTexture( BLOOM_SIZE, BLOOM_SIZE );
}

/*
=================
R_InitBloomTextures
=================
*/
void R_InitBloomTextures( void )
{
	BLOOM_SIZE = 0;

	if( r_bloomscreentexture )
		FREE_TEXTURE( r_bloomscreentexture );
	if( r_bloomeffecttexture )
		FREE_TEXTURE( r_bloomeffecttexture );
	if( r_bloombackuptexture )
		FREE_TEXTURE( r_bloombackuptexture );
	if( r_bloomdownsamplingtexture )
		FREE_TEXTURE( r_bloomdownsamplingtexture );

	r_bloomscreentexture = r_bloomeffecttexture = 0;
	r_bloombackuptexture = r_bloomdownsamplingtexture = 0;

	if( !r_bloom->value )
		return;

	R_Bloom_InitTextures();
}

/*
=================
R_Bloom_SamplePass
=================
*/
inline static void R_Bloom_SamplePass( int xpos, int ypos )
{
	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, sampleText_tch );
	pglVertex2f( xpos, ypos );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( xpos, ypos+sample_height );
	pglTexCoord2f( sampleText_tcw, 0 );
	pglVertex2f( xpos+sample_width, ypos+sample_height );
	pglTexCoord2f( sampleText_tcw, sampleText_tch );
	pglVertex2f( xpos+sample_width, ypos );
	pglEnd();
}

/*
=================
R_Bloom_Quad
=================
*/
inline static void R_Bloom_Quad( int x, int y, int w, int h, float texwidth, float texheight )
{
	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, texheight );
	pglVertex2f( x, y );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( x, y+h );
	pglTexCoord2f( texwidth, 0 );
	pglVertex2f( x+w, y+h );
	pglTexCoord2f( texwidth, texheight );
	pglVertex2f( x+w, y );
	pglEnd();
}

/*
=================
R_Bloom_DrawEffect
=================
*/
static void R_Bloom_DrawEffect( void )
{
	GL_Bind( GL_TEXTURE0, r_bloomeffecttexture );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglColor4f( r_bloom_alpha->value, r_bloom_alpha->value, r_bloom_alpha->value, 1.0f );
	pglBlendFunc( GL_ONE, GL_ONE );
	pglEnable( GL_BLEND );

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, sampleText_tch );
	pglVertex2f( curView_x, curView_y );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( curView_x, curView_y+curView_height );
	pglTexCoord2f( sampleText_tcw, 0 );
	pglVertex2f( curView_x+curView_width, curView_y+curView_height );
	pglTexCoord2f( sampleText_tcw, sampleText_tch );
	pglVertex2f( curView_x+curView_width, curView_y );
	pglEnd();

	pglDisable( GL_BLEND );
}

/*
=================
R_Bloom_GeneratexDiamonds
=================
*/
static void R_Bloom_GeneratexDiamonds( void )
{
	int i, j;
	float intensity;

	// set up sample size workspace
	pglScissor( 0, 0, sample_width, sample_height );
	pglViewport( 0, 0, sample_width, sample_height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, sample_width, sample_height, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	// copy small scene into r_bloomeffecttexture
	GL_Bind( GL_TEXTURE0, r_bloomeffecttexture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );

	// start modifying the small scene corner
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglEnable( GL_BLEND );

	// darkening passes
	if( r_bloom_darken->value )
	{
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglBlendFunc( GL_DST_COLOR, GL_ZERO );

		for( i = 0; i < (int)r_bloom_darken->value; i++ )
			R_Bloom_SamplePass( 0, 0 );

		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );
	}

	// bluring passes
	pglBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );

	if( r_bloom_diamond_size->value > 7.0f || r_bloom_diamond_size->value <= 3.0f )
	{
		if( r_bloom_diamond_size->value != 8.0f )
			CVAR_SET_FLOAT( "r_bloom_diamond_size", 8.0f );

		for( i = 0; i < r_bloom_diamond_size->value; i++ )
		{
			for( j = 0; j < r_bloom_diamond_size->value; j++ )
			{
				intensity = r_bloom_intensity->value * 0.3f * Diamond8x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0f );
				R_Bloom_SamplePass( i-4, j-4 );
			}
		}
	}
	else if( r_bloom_diamond_size->value > 5.0f )
	{
		if( r_bloom_diamond_size->value != 6.0f )
			CVAR_SET_FLOAT( "r_bloom_diamond_size", 6.0f );

		for( i = 0; i < r_bloom_diamond_size->value; i++ )
		{
			for( j = 0; j < r_bloom_diamond_size->value; j++ )
			{
				intensity = r_bloom_intensity->value * 0.5f * Diamond6x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0f );
				R_Bloom_SamplePass( i-3, j-3 );
			}
		}
	}
	else if( r_bloom_diamond_size->value > 3.0f )
	{
		if( r_bloom_diamond_size->value != 4.0f )
			CVAR_SET_FLOAT( "r_bloom_diamond_size", 4.0f );

		for( i = 0; i < r_bloom_diamond_size->value; i++ )
		{
			for( j = 0; j < r_bloom_diamond_size->value; j++ )
			{
				intensity = r_bloom_intensity->value * 0.8f * Diamond4x[i][j];
				if( intensity < 0.01f ) continue;
				pglColor4f( intensity, intensity, intensity, 1.0f );
				R_Bloom_SamplePass( i-2, j-2 );
			}
		}
	}

	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, sample_width, sample_height );

	// restore full screen workspace
	pglScissor( 0, 0, glState.width, glState.height );
	pglViewport( 0, 0, glState.width, glState.height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho( 0, glState.width, glState.height, 0, -10, 100 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();
}

/*
=================
R_Bloom_DownsampleView
=================
*/
static void R_Bloom_DownsampleView( void )
{
	pglDisable( GL_BLEND );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	if( r_screendownsamplingtexture_size )
	{
		// stepped downsample
		int midsample_width = ( r_screendownsamplingtexture_size * sampleText_tcw );
		int midsample_height = ( r_screendownsamplingtexture_size * sampleText_tch );

		// copy the screen and draw resized
		GL_Bind( GL_TEXTURE0, r_bloomscreentexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, curView_x, glState.height - ( curView_y + curView_height ), curView_width, curView_height );
		R_Bloom_Quad( 0, glState.height - midsample_height, midsample_width, midsample_height, screenTex_tcw, screenTex_tch );

		// now copy into downsampling (mid-sized) texture
		GL_Bind( GL_TEXTURE0, r_bloomdownsamplingtexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, midsample_width, midsample_height );

		// now draw again in bloom size
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		R_Bloom_Quad( 0, glState.height - sample_height, sample_width, sample_height, sampleText_tcw, sampleText_tch );

		// now blend the big screen texture into the bloom generation space (hoping it adds some blur)
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_ONE, GL_ONE );
		pglColor4f( 0.5f, 0.5f, 0.5f, 1.0f );
		GL_Bind( GL_TEXTURE0, r_bloomscreentexture );
		R_Bloom_Quad( 0, glState.height - sample_height, sample_width, sample_height, screenTex_tcw, screenTex_tch );
		pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
		pglDisable( GL_BLEND );
	}
	else
	{
		// downsample simple
		GL_Bind( GL_TEXTURE0, r_bloomscreentexture );
		pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, curView_x, glState.height - ( curView_y + curView_height ), curView_width, curView_height );
		R_Bloom_Quad( 0, glState.height - sample_height, sample_width, sample_height, screenTex_tcw, screenTex_tch );
	}
}

/*
=================
R_BloomBlend
=================
*/
void R_BloomBlend( void )
{
	if( !r_bloom->value )
		return;

	if( !BLOOM_SIZE )
		R_Bloom_InitTextures();

	if( screen_texture_width < BLOOM_SIZE || screen_texture_height < BLOOM_SIZE )
		return;

	// set up full screen workspace
	pglScissor( 0, 0, glState.width, glState.height );
	pglViewport( 0, 0, glState.width, glState.height );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();

	pglOrtho( 0, glState.width, glState.height, 0, -10, 100 );

	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();

	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_BLEND );

	GL_Cull( 0 );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	// set up current sizes
	curView_x = RI->viewport[0];
	curView_y = RI->viewport[1];
	curView_width = RI->viewport[2];
	curView_height = RI->viewport[3];

	screenTex_tcw = ( (float)curView_width / (float)screen_texture_width );
	screenTex_tch = ( (float)curView_height / (float)screen_texture_height );
	if( curView_height > curView_width )
	{
		sampleText_tcw = ( (float)curView_width / (float)curView_height );
		sampleText_tch = 1.0f;
	}
	else
	{
		sampleText_tcw = 1.0f;
		sampleText_tch = ( (float)curView_height / (float)curView_width );
	}

	sample_width = ( BLOOM_SIZE * sampleText_tcw );
	sample_height = ( BLOOM_SIZE * sampleText_tch );

	// copy the screen space we'll use to work into the backup texture
	GL_Bind( GL_TEXTURE0, r_bloombackuptexture );
	pglCopyTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 0, 0, r_screenbackuptexture_width * sampleText_tcw, r_screenbackuptexture_height * sampleText_tch );

	// create the bloom image
	R_Bloom_DownsampleView();
	R_Bloom_GeneratexDiamonds();

	pglDisable( GL_BLEND );
	// restore the screen-backup to the screen
	GL_Bind( GL_TEXTURE0, r_bloombackuptexture );

	pglColor4f( 1, 1, 1, 1 );

	R_Bloom_Quad( 0, 
		glState.height - (r_screenbackuptexture_height * sampleText_tch),
		r_screenbackuptexture_width * sampleText_tcw,
		r_screenbackuptexture_height * sampleText_tch,
		sampleText_tcw, sampleText_tch );

	R_Bloom_DrawEffect();
}
