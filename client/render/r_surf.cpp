/*
r_surf.cpp - surface-related refresh code
Copyright (C) 2011 Uncle Mike

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
#include "pm_movevars.h"
#include "features.h"
#include "mathlib.h"
#include "r_weather.h"
#include "r_particle.h"

#define DECAL_TRANSPARENT_THRESHOLD	230	// transparent decals draw with GL_MODULATE

typedef struct
{
	int		allocated[BLOCK_SIZE_MAX];
	int		current_lightmap_texture;
	msurface_t	*dynamic_surfaces;
	msurface_t	*lightmap_surfaces[MAX_LIGHTMAPS];
	byte		lightmap_buffer[BLOCK_SIZE_MAX*BLOCK_SIZE_MAX*4];
} gllightmapstate_t;

static unsigned int		r_blocklights[BLOCK_SIZE_MAX*BLOCK_SIZE_MAX*3];
static glpoly_t		*fullbright_polys[MAX_TEXTURES];
static bool		draw_fullbrights = false;
static mextrasurf_t		*detail_surfaces[MAX_TEXTURES];
static bool		draw_details = false;
static mextrasurf_t		*chrome_surfaces;
static bool		draw_chromes = false;
static gllightmapstate_t	gl_lms;
static msurface_t		*skychain = NULL;
static msurface_t		*draw_surfaces[8192];	// used for sorting translucent surfaces
static msurface_t		*light_surfaces[8192];	// used for draw projected lights
static Vector2D		world_orthocenter;
static Vector2D		world_orthohalf;

static void LM_UploadBlock( int lightmapnum );

/*
==================
HUD_SetOrthoBounds

setup ortho min\maxs for draw overview
==================
*/
void HUD_SetOrthoBounds( const float *mins, const float *maxs )
{
	if( !g_fRenderInitialized ) return;

	world_orthocenter.x = ((maxs[0] + mins[0]) * 0.5f);
	world_orthocenter.y = ((maxs[1] + mins[1]) * 0.5f);

	world_orthohalf.x = maxs[0] - world_orthocenter.x;
	world_orthohalf.y = maxs[1] - world_orthocenter.y;
}

BOOL R_IsWater( void )
{
	if( !RI.currententity || RI.currententity == GET_ENTITY( 0 ))
		return FALSE;

	entity_state_t *state = &RI.currententity->curstate;

	if( state->rendermode == kRenderTransAlpha )
		return FALSE;

	if( state->solid == SOLID_NOT && state->skin < CONTENTS_SOLID && state->skin > CONTENTS_CLIP )
		return TRUE;	// it's func_water 

	return FALSE;
}

void GetPolyTexOffs( glpoly_t *p, cl_entity_t *e, float *s, float *t )
{
	if( p->flags & SURF_CONVEYOR )
	{
		int	currentTex;
		float	sOffset, tOffset;
		float	flConveyorSpeed, sy, cy;
		float	flRate, flAngle, flWidth;

		flConveyorSpeed = (e->curstate.rendercolor.g<<8|e->curstate.rendercolor.b) / 16.0f;
		if( e->curstate.rendercolor.r ) flConveyorSpeed = -flConveyorSpeed;

		pglGetIntegerv( GL_TEXTURE_BINDING_2D, &currentTex );
		flWidth = (float)RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, currentTex );

		if( flWidth == 0.0f ) ALERT( at_error, "Width is 0!\n" );

		flRate = abs( flConveyorSpeed ) / flWidth;
		flAngle = ( flConveyorSpeed >= 0 ) ? 180 : 0;

		SinCos( flAngle * ( M_PI / 180.0f ), &sy, &cy );
		sOffset = RI.refdef.time * cy * flRate;
		tOffset = RI.refdef.time * sy * flRate;
		
		// make sure that we are positive
		if( sOffset < 0.0f ) sOffset += 1.0f + -(int)sOffset;
		if( tOffset < 0.0f ) tOffset += 1.0f + -(int)tOffset;

		// make sure that we are in a [0,1] range
		*s = sOffset - (int)sOffset;
		*t = tOffset - (int)tOffset;
	}
	else
	{
		*s = 0.0f;
		*t = 0.0f;
	}
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation( texture_t *base, int surfacenum )
{
	int	reletive;
	int	count, speed;

	// random tiling textures
	if( base->anim_total < 0 )
	{
		reletive = abs( surfacenum ) % abs( base->anim_total );

		count = 0;
		while( base->anim_min > reletive || base->anim_max <= reletive )
		{
			base = base->anim_next;
			if( !base ) HOST_ERROR( "R_TextureRandomTiling: broken loop\n" );
			if( ++count > 100 ) HOST_ERROR( "R_TextureRandomTiling: infinite loop\n" );
		}
		return base;
	}

	if( RI.currententity->curstate.frame )
	{
		if( base->alternate_anims )
			base = base->alternate_anims;
	}
	
	if( !base->anim_total )
		return base;

	// GoldSrc and Quake1 has different animating speed
	if( RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ) || RENDER_GET_PARM( PARM_WORLD_VERSION, 0 ) == 29 )
		speed = 10;
	else speed = 20;

	reletive = (int)(GET_CLIENT_TIME() * speed) % base->anim_total;

	count = 0;	
	while( base->anim_min > reletive || base->anim_max <= reletive )
	{
		base = base->anim_next;
		if( !base ) HOST_ERROR( "R_TextureAnimation: broken loop\n" );
		if( ++count > 100 ) HOST_ERROR( "R_TextureAnimation: infinite loop\n" );
	}
	return base;
}

/*
================
R_SetCacheState
================
*/
void R_SetCacheState( msurface_t *surf )
{
	for( int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++ )
	{
		surf->cached_light[maps] = RI.lightstylevalue[surf->styles[maps]];
	}
}

/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/
static void LM_InitBlock( void )
{
	memset( gl_lms.allocated, 0, sizeof( gl_lms.allocated ));
}

static int LM_AllocBlock( int w, int h, int *x, int *y )
{
	int	i, j;
	int	best, best2;

	best = BLOCK_SIZE;

	for( i = 0; i < BLOCK_SIZE - w; i++ )
	{
		best2 = 0;

		for( j = 0; j < w; j++ )
		{
			if( gl_lms.allocated[i+j] >= best )
				break;
			if( gl_lms.allocated[i+j] > best2 )
				best2 = gl_lms.allocated[i+j];
		}

		if( j == w )
		{	
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if( best + h > BLOCK_SIZE )
		return false;

	for( i = 0; i < w; i++ )
		gl_lms.allocated[*x + i] = best + h;

	return true;
}

static void LM_UploadBlock( qboolean dynamic )
{
	int	i;

	if( dynamic )
	{
		int	height = 0;

		for( i = 0; i < BLOCK_SIZE; i++ )
		{
			if( gl_lms.allocated[i] > height )
				height = gl_lms.allocated[i];
		}

		if( RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_LARGE_LIGHTMAPS )
			GL_Bind( GL_TEXTURE0, tr.dlightTexture2 );
		else GL_Bind( GL_TEXTURE0, tr.dlightTexture );
		pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, BLOCK_SIZE, height, GL_RGBA, GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer );
	}
	else
	{
		int texture;

		i = gl_lms.current_lightmap_texture;

		// get the engine lightmap index and update lightmap
		texture = RENDER_GET_PARM( PARM_TEX_LIGHTMAP, i );
		GL_Bind( GL_TEXTURE0, texture );
		pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, BLOCK_SIZE, BLOCK_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer );
		tr.lightmapTextures[i] = texture;

		if( ++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS )
			HOST_ERROR( "AllocBlock: full\n" );
	}
}

/*
=================
R_BuildLightmap

Combine and scale multiple lightmaps into the floating
format in r_blocklights
=================
*/
static void R_BuildLightMap( msurface_t *surf, byte *dest, int stride )
{
	int smax, tmax;
	unsigned int *bl, scale;
	int i, map, size, s, t;
	color24 *lm;

	smax = ( surf->extents[0] / LM_SAMPLE_SIZE ) + 1;
	tmax = ( surf->extents[1] / LM_SAMPLE_SIZE ) + 1;
	size = smax * tmax;

	lm = surf->samples;

	memset( r_blocklights, 0, sizeof( unsigned int ) * size * 3 );

	// add all the lightmaps
	for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255 && lm; map++ )
	{
		scale = RI.lightstylevalue[surf->styles[map]];

		for( i = 0, bl = r_blocklights; i < size; i++, bl += 3, lm++ )
		{
			bl[0] += TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
			bl[1] += TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
			bl[2] += TEXTURE_TO_TEXGAMMA( lm->b ) * scale;
		}
	}

	// Put into texture format
	stride -= (smax << 2);
	bl = r_blocklights;

	for( t = 0; t < tmax; t++, dest += stride )
	{
		for( s = 0; s < smax; s++ )
		{
			dest[0] = min((bl[0] >> 7), 255 );
			dest[1] = min((bl[1] >> 7), 255 );
			dest[2] = min((bl[2] >> 7), 255 );
			dest[3] = 255;

			bl += 3;
			dest += 4;
		}
	}
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly( glpoly_t *p, float xScale, float yScale )
{
	float		*v;
	float		sOffset, tOffset;
	cl_entity_t	*e = RI.currententity;
	int		i, hasScale = false;

	if( !p ) return;

	GetPolyTexOffs( p, e, &sOffset, &tOffset );

	if( xScale != 0.0f && yScale != 0.0f )
		hasScale = true;

	pglBegin( GL_POLYGON );

	for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
	{
		if( hasScale )
			pglTexCoord2f(( v[3] + sOffset ) * xScale, ( v[4] + tOffset ) * yScale );
		else pglTexCoord2f( v[3] + sOffset, v[4] + tOffset );

		pglVertex3fv( v );
	}

	pglEnd();
}

/*
================
DrawGLPolyChain

Render lightmaps
================
*/
void DrawGLPolyChain( glpoly_t *p, float soffset, float toffset )
{
	qboolean dynamic = true;

	if( soffset == 0.0f && toffset == 0.0f )
		dynamic = false;

	pglBegin( GL_POLYGON );

	for( ; p != NULL; p = p->chain )
	{
		float	*v;
		int	i;

		for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
		{
			if( !dynamic ) pglTexCoord2f( v[5], v[6] );
			else pglTexCoord2f( v[5] - soffset, v[6] - toffset );
			pglVertex3fv( v );
		}
	}

	pglEnd ();
}

/*
================
DrawGLPolyZPass
================
*/
static _forceinline void DrawGLPolyZPass( glpoly_t *p )
{
	float	*v;
	int	i;

	pglBegin( GL_POLYGON );

	for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
	{
		pglVertex3fv( v );
	}
	pglEnd();
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps( void )
{
	cl_entity_t *e = RI.currententity;
	msurface_t *surf, *newsurf = NULL;
	mextrasurf_t *info;

	if( r_fullbright->value || !worldmodel->lightdata )
		return;

	if( RI.currententity )
	{
		if( e->curstate.effects & EF_FULLBRIGHT )
			return;	// disabled by user

		switch( e->curstate.rendermode )
		{
		case kRenderGlow:
		case kRenderTransAdd:
		case kRenderTransTexture:
			return;
		case kRenderTransColor:
			pglEnable( GL_BLEND );
			pglDisable( GL_ALPHA_TEST );
			pglBlendFunc( GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			pglColor4ub( e->curstate.rendercolor.r, e->curstate.rendercolor.g,
			e->curstate.rendercolor.b, e->curstate.renderamt );
			break;
		case kRenderTransAlpha:
			pglDepthMask( GL_FALSE );
			pglDepthFunc( GL_EQUAL );
		default:	
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
			pglDisable( GL_BLEND );
			break;
		}
	}

	// render static lightmaps first
	for( int i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if( gl_lms.lightmap_surfaces[i] )
		{
			GL_Bind( GL_TEXTURE0, tr.lightmapTextures[i] );

			for( surf = gl_lms.lightmap_surfaces[i]; surf != NULL; surf = surf->lightmapchain )
			{
				if( surf->polys ) DrawGLPolyChain( surf->polys, 0.0f, 0.0f );
			}
		}
	}

	// render dynamic lightmaps
	if( r_dynamic->value )
	{
		LM_InitBlock();

		if( RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_LARGE_LIGHTMAPS )
			GL_Bind( GL_TEXTURE0, tr.dlightTexture2 );
		else GL_Bind( GL_TEXTURE0, tr.dlightTexture );

		newsurf = gl_lms.dynamic_surfaces;

		for( surf = gl_lms.dynamic_surfaces; surf != NULL; surf = surf->lightmapchain )
		{
			int	smax, tmax;
			byte	*base;

			smax = ( surf->extents[0] / LM_SAMPLE_SIZE ) + 1;
			tmax = ( surf->extents[1] / LM_SAMPLE_SIZE ) + 1;
			info = SURF_INFO( surf, RI.currentmodel );

			if( LM_AllocBlock( smax, tmax, &info->dlight_s, &info->dlight_t ))
			{
				base = gl_lms.lightmap_buffer;
				base += ( info->dlight_t * BLOCK_SIZE + info->dlight_s ) * 4;

				R_BuildLightMap( surf, base, BLOCK_SIZE * 4 );
			}
			else
			{
				msurface_t	*drawsurf;

				// upload what we have so far
				LM_UploadBlock( true );

				// draw all surfaces that use this lightmap
				for( drawsurf = newsurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain )
				{
					if( drawsurf->polys )
					{
						info = SURF_INFO( drawsurf, RI.currentmodel );

						DrawGLPolyChain( drawsurf->polys,
						( drawsurf->light_s - info->dlight_s ) * ( 1.0f / (float)BLOCK_SIZE ), 
						( drawsurf->light_t - info->dlight_t ) * ( 1.0f / (float)BLOCK_SIZE ));
					}
				}

				newsurf = drawsurf;

				// clear the block
				LM_InitBlock();

				info = SURF_INFO( surf, RI.currentmodel );

				// try uploading the block now
				if( !LM_AllocBlock( smax, tmax, &info->dlight_s, &info->dlight_t ))
					HOST_ERROR( "AllocBlock: full\n" );

				base = gl_lms.lightmap_buffer;
				base += ( info->dlight_t * BLOCK_SIZE + info->dlight_s ) * 4;

				R_BuildLightMap( surf, base, BLOCK_SIZE * 4 );
			}
		}

		// draw remainder of dynamic lightmaps that haven't been uploaded yet
		if( newsurf ) LM_UploadBlock( true );

		for( surf = newsurf; surf != NULL; surf = surf->lightmapchain )
		{
			if( surf->polys )
			{
				info = SURF_INFO( surf, RI.currentmodel );

				DrawGLPolyChain( surf->polys,
				( surf->light_s - info->dlight_s ) * ( 1.0f / (float)BLOCK_SIZE ),
				( surf->light_t - info->dlight_t ) * ( 1.0f / (float)BLOCK_SIZE ));
			}
		}
	}

	if( !glState.drawTrans )
	{
		pglDepthMask( GL_TRUE );
		pglDepthFunc( GL_LEQUAL );
	}
}

/*
================
R_RenderFullbrights
================
*/
void R_RenderFullbrights( void )
{
	glpoly_t	*p;
	int	i;

	if( !draw_fullbrights )
		return;

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDisable( GL_ALPHA_TEST );
	pglBlendFunc( GL_ONE, GL_ONE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	for( i = 1; i < MAX_TEXTURES; i++ )
	{
		if( !fullbright_polys[i] )
			continue;

		GL_Bind( GL_TEXTURE0, i );

		for( p = fullbright_polys[i]; p; p = p->next )
		{
			if( p->flags & SURF_DRAWTURB )
				EmitWaterPolys( p, ( p->flags & SURF_NOCULL ));
			else DrawGLPoly( p, 0.0f, 0.0f );
		}

		fullbright_polys[i] = NULL;		
	}

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	draw_fullbrights = false;
}

/*
================
R_RenderDetails
================
*/
void R_RenderDetails( void )
{
	if( !draw_details ) return;

	pglEnable( GL_BLEND );
	pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

	if( RI.currententity->curstate.rendermode == kRenderTransAlpha )
		pglDepthFunc( GL_EQUAL );

	for( int i = 1; i < MAX_TEXTURES; i++ )
	{
		mextrasurf_t *es = detail_surfaces[i];
		if( !es ) continue;

		float xScale, yScale;

		GL_Bind( GL_TEXTURE0, i );

		for( mextrasurf_t *p = es; p; p = p->detailchain )
		{
			msurface_t *fa = INFO_SURF( p, RI.currentmodel ); // get texture scale
			GET_DETAIL_SCALE( fa->texinfo->texture->gl_texturenum, &xScale, &yScale );
			DrawGLPoly( fa->polys, xScale, yScale );
                    }

		detail_surfaces[i] = NULL;
		es->detailchain = NULL;
	}

	pglDisable( GL_BLEND );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	if( RI.currententity->curstate.rendermode == kRenderTransAlpha )
		pglDepthFunc( GL_LEQUAL );

	draw_details = false;
}

/*
================
R_RenderChrome
================
*/
void R_RenderChrome( void )
{
	if( !draw_chromes ) return;

	pglEnable( GL_BLEND );
	pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if( RI.currententity->curstate.rendermode == kRenderTransAlpha )
		pglDepthFunc( GL_EQUAL );

	GL_TexGen( GL_S, GL_SPHERE_MAP );
	GL_TexGen( GL_T, GL_SPHERE_MAP );

	GL_Bind( GL_TEXTURE0, tr.chromeTexture );

	mextrasurf_t *es = chrome_surfaces;

	for( mextrasurf_t *p = es; p; p = p->chromechain )
	{
		msurface_t *fa = INFO_SURF( p, RI.currentmodel ); // get texture scale
		float *v;
		int j;

		pglBegin( GL_POLYGON );

		for( j = 0, v = fa->polys->verts[0]; j < fa->polys->numverts; j++, v += VERTEXSIZE )
			pglVertex3fv( v );
		pglEnd();
	}

	chrome_surfaces = NULL;
	es->chromechain = NULL;

	pglDisable( GL_BLEND );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	if( RI.currententity->curstate.rendermode == kRenderTransAlpha )
		pglDepthFunc( GL_LEQUAL );

	draw_chromes = false;

	GL_DisableAllTexGens();
}

/*
================
R_DrawProjectDecal
================
*/
void R_DrawProjectDecal( struct decal_s *pDecal, struct msurface_s *fa, float *v, int numVerts )
{
	GL_Bind( GL_TEXTURE3, pDecal->texture );

	pglBegin( GL_POLYGON );

	for( int i = 0; i < numVerts; i++, v += VERTEXSIZE )
	{
		pglMultiTexCoord2f( GL_TEXTURE3_ARB, v[3], v[4] );
		pglVertex3fv( v );
	}

	pglEnd();
}

/*
================
R_DrawLMDecal
================
*/
void R_DrawLMDecal( struct decal_s *pDecal, struct msurface_s *fa, float *v, int numVerts )
{
	pglEnable( GL_BLEND );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	GL_Bind( GL_TEXTURE0, pDecal->texture );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	GL_Bind( GL_TEXTURE1, tr.lightmapTextures[fa->lightmaptexturenum] );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	R_SetupOverbright( true );

	pglBegin( GL_POLYGON );

	for( int i = 0; i < numVerts; i++, v += VERTEXSIZE )
	{
		pglMultiTexCoord2f( GL_TEXTURE0_ARB, v[3], v[4] );
		pglMultiTexCoord2f( GL_TEXTURE1_ARB, v[5], v[6] );
		pglVertex3fv( v );
	}

	pglEnd();

	R_SetupOverbright( false );
	GL_CleanUpTextureUnits( 0 );
}

void R_DrawSurfaceDecals( msurface_t *fa )
{
	decal_t	*p;
	float	*v;
	int	i, numVerts;

	if( !fa->pdecals ) return;

	cl_entity_t *e = RI.currententity;
	assert( e != NULL );

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		GL_Cull( GL_NONE );

	if( fa->flags & SURF_TRANSPARENT && glState.stencilEnabled )
	{
		mtexinfo_t *tex = fa->texinfo;
		GL_Bind( GL_TEXTURE0, tex->texture->gl_texturenum );

		for( p = fa->pdecals; p; p = p->pnext )
		{
			float *o = DECAL_SETUP_VERTS( p, fa, p->texture, &numVerts );
			if( !numVerts ) continue;

			pglEnable( GL_STENCIL_TEST );
			pglStencilFunc( GL_ALWAYS, 1, 0xFFFFFFFF );
			pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );

			pglStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			pglBegin( GL_POLYGON );

			for( i = 0, v = o; i < numVerts; i++, v += VERTEXSIZE )
			{
				// HACKHACK: store main texture scissor into lightmap coords
				v[5] = (DotProductFast( v, tex->vecs[0]) + tex->vecs[0][3] ) / tex->texture->width;
				v[6] = (DotProductFast( v, tex->vecs[1]) + tex->vecs[1][3] ) / tex->texture->height;

				pglTexCoord2f( v[5], v[6] );
				pglVertex3fv( v );
			}

			pglEnd();
			pglStencilOp( GL_KEEP, GL_KEEP, GL_DECR );

			pglEnable( GL_ALPHA_TEST );
			pglBegin( GL_POLYGON );

			for( i = 0, v = o; i < numVerts; i++, v += VERTEXSIZE )
			{
				pglTexCoord2f( v[5], v[6] );
				pglVertex3fv( v );
			}

			pglEnd();
			pglDisable( GL_ALPHA_TEST );

			pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			pglStencilFunc( GL_EQUAL, 0, 0xFFFFFFFF );
			pglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		}
	}

	for( p = fa->pdecals; p; p = p->pnext )
	{
		v = DECAL_SETUP_VERTS( p, fa, p->texture, &numVerts );
		if( !numVerts ) continue;

		if( RI.currententity->curstate.rendermode == kRenderTransTexture || RI.currententity->curstate.rendermode == kRenderTransAdd )
		{
			color24	decalColor;
			byte	decalTrns;

			pglEnable( GL_BLEND );
			pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

			GET_EXTRA_PARAMS( p->texture, &decalColor.r, &decalColor.g, &decalColor.b, &decalTrns );
			GL_Bind( GL_TEXTURE0, p->texture );

			// draw transparent decals with GL_MODULATE
			if( decalTrns > DECAL_TRANSPARENT_THRESHOLD )
				pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			else pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

			pglBegin( GL_POLYGON );

			for( int i = 0; i < numVerts; i++, v += VERTEXSIZE )
			{
				pglTexCoord2f( v[3], v[4] );
				pglVertex3fv( v );
			}

			pglEnd();
		}
		else
		{
			R_DrawLMDecal( p, fa, v, numVerts );

			if( R_CountPlights( ))
			{
				pglBlendFunc( GL_ONE, GL_ONE );
			
				for( i = 0; i < MAX_PLIGHTS; i++ )
				{
					plight_t *pl = &cl_plights[i];

					if( pl->die < GET_CLIENT_TIME() || !pl->radius )
						continue;

					R_BeginDrawProjection( pl, true );
					R_DrawProjectDecal( p, fa, v, numVerts );
					R_EndDrawProjection();
				}
			}
		}
	}

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		GL_Cull( GL_FRONT );

	if( fa->flags & SURF_TRANSPARENT && glState.stencilEnabled )
		pglDisable( GL_STENCIL_TEST );
}

/*
================
R_AddSurfaceLayers
================
*/
void R_AddSurfaceLayers( msurface_t *fa )
{
	texture_t	*t;
	cl_entity_t *e = RI.currententity;
	bool update_lightcache = false;
	bool ignore_lightmaps = false;

	if( glState.drawProjection )
		return;

	if( fa->flags & ( SURF_DRAWTILED|SURF_PORTAL ))
		return; // no additional layers anyway

	RI.params |= RP_WORLDSURFVISIBLE;

	t = R_TextureAnimation( fa->texinfo->texture, fa - RI.currententity->model->surfaces );

	if( t->fb_texturenum )
	{
		// HACKHACK: store fullbrights in poly->next (only for non-water surfaces)
		fa->polys->next = fullbright_polys[t->fb_texturenum];
		fullbright_polys[t->fb_texturenum] = fa->polys;
		draw_fullbrights = true;
	}
	else if( r_detailtextures->value )
	{
		mextrasurf_t *es = SURF_INFO( fa, RI.currentmodel );

		if( t->dt_texturenum )
		{
			es->detailchain = detail_surfaces[t->dt_texturenum];
			detail_surfaces[t->dt_texturenum] = es;
			draw_details = true;
		}
	}

	if( fa->flags & SURF_CHROME && tr.chromeTexture )
	{
		mextrasurf_t *es = SURF_INFO( fa, RI.currentmodel );

		// draw common chrome texture (use particle texture as chrome)
		es->chromechain = chrome_surfaces;
		chrome_surfaces = es;
		draw_chromes = true;
	}

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		ignore_lightmaps = true;

	if( fa->flags & SURF_REFLECT && RP_NORMALPASS())
		ignore_lightmaps = true; // no lightmaps for mirror

	// g-cont. enabled display has self-lighting :-)
	if( fa->flags & ( SURF_SCREEN|SURF_MOVIE ) && RI.currententity->curstate.body )
		ignore_lightmaps = true;

	// NOTE: the surface has actually ignore lightmap but need to keep them state an actual
	// because it's may be used e.g. for decals

	// check for lightmap modification
	for( int maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		if( RI.lightstylevalue[fa->styles[maps]] != fa->cached_light[maps] )
		{
			update_lightcache = true;
			break;
		}
	}

	if( r_dynamic->value && update_lightcache )
	{
		if( fa->styles[maps] >= 32 || fa->styles[maps] == 0 )
		{
			byte	temp[68*68*4];
			int	smax, tmax;

			smax = ( fa->extents[0] / LM_SAMPLE_SIZE ) + 1;
			tmax = ( fa->extents[1] / LM_SAMPLE_SIZE ) + 1;

			R_BuildLightMap( fa, temp, smax * 4 );
			R_SetCacheState( fa );
                              
			GL_Bind( GL_TEXTURE0, tr.lightmapTextures[fa->lightmaptexturenum] );

			pglTexSubImage2D( GL_TEXTURE_2D, 0, fa->light_s, fa->light_t, smax, tmax,
			GL_RGBA, GL_UNSIGNED_BYTE, temp );

			if( !ignore_lightmaps )
			{
				fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
				gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
			}
		}
		else if( !ignore_lightmaps )
		{
			fa->lightmapchain = gl_lms.dynamic_surfaces;
			gl_lms.dynamic_surfaces = fa;
		}
	}
	else if( !ignore_lightmaps )
	{
		fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
		gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
	}
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly( msurface_t *fa, qboolean zpass )
{
	qboolean	is_mirror = false;
	qboolean	is_screen = false;
	qboolean	is_cinema = false;
	qboolean	draw_mirror = false;
	qboolean	draw_screen = false;
	cl_entity_t *e = RI.currententity;
	qboolean	force_no_lightmaps = false;
	
	texture_t	*t = R_TextureAnimation( fa->texinfo->texture, fa - RI.currententity->model->surfaces );

	if( !zpass )
	{
		if( e->curstate.rendermode == kRenderNormal || e->curstate.rendermode == kRenderTransAlpha )
			force_no_lightmaps = ( worldmodel->lightdata && !r_fullbright->value );
	}

	// draw mirrors through portal pass
	if(( RI.params & RP_PORTALVIEW ) && ( fa->flags & SURF_REFLECT ))
		draw_mirror = true;

	// draw mirrors through portal pass
	if(( RI.params & RP_PORTALVIEW ) && ( fa->flags & SURF_SCREEN ))
		draw_screen = true;

	// reflect portals through mirrors
	if(( RI.params & RP_MIRRORVIEW ) && ( fa->flags & SURF_PORTAL ))
		draw_mirror = true;

	// reflect portals through mirrors
	if(( RI.params & RP_MIRRORVIEW ) && ( fa->flags & SURF_SCREEN ))
		draw_screen = true;

	// draw portals through screens
	if(( RI.params & RP_SCREENVIEW ) && ( fa->flags & (SURF_PORTAL|SURF_REFLECT)))
		draw_mirror = true;

	// draw the screens from the himself
	if(( RI.params & RP_SCREENVIEW ) && ( fa->flags & SURF_SCREEN))
		draw_screen = true;

	if( RP_NORMALPASS() && fa->flags & ( SURF_REFLECT|SURF_PORTAL ))
		draw_mirror = true;

	if( RP_NORMALPASS() && fa->flags & ( SURF_SCREEN ))
		draw_screen = true;

	if( tr.insideView >= MAX_MIRROR_DEPTH )
	{
		draw_mirror = false;
		draw_screen = false;
	}

	if( draw_mirror )
	{
		if( SURF_INFO( fa, RI.currentmodel )->mirrortexturenum )
		{
			GL_Bind( GL_TEXTURE0, SURF_INFO( fa, RI.currentmodel )->mirrortexturenum );
			is_mirror = true;

			if( force_no_lightmaps )
			{
				pglDepthFunc( GL_LEQUAL );
				pglDepthMask( GL_TRUE );
				pglDisable( GL_BLEND );
			}
		}
		else GL_Bind( GL_TEXTURE0, t->gl_texturenum ); // dummy

		// DEBUG: reset the mirror texture after drawing
//		SURF_INFO( fa, RI.currentmodel )->mirrortexturenum = 0;
	}
	else if( draw_screen )
	{
		if( SURF_INFO( fa, RI.currentmodel )->mirrortexturenum && RI.currententity->curstate.body )
		{
			GL_Bind( GL_TEXTURE0, SURF_INFO( fa, RI.currentmodel )->mirrortexturenum );
			is_screen = true;

			if( force_no_lightmaps )
			{
				pglDepthFunc( GL_LEQUAL );
				pglDepthMask( GL_TRUE );
				pglDisable( GL_BLEND );
			}
		}
		else GL_Bind( GL_TEXTURE0, t->gl_texturenum ); // dummy

		// screen textures is drawing from himself so not reset the texture
//		SURF_INFO( fa, RI.currentmodel )->mirrortexturenum = 0;
	}
	else if(( RI.currententity->curstate.effects & EF_SCREENMOVIE ) && ( fa->flags & SURF_MOVIE ))
	{
		if( R_DrawCinematic( fa, t ))
		{
			is_cinema = true;

			if( force_no_lightmaps )
			{
				pglDepthFunc( GL_LEQUAL );
				pglDepthMask( GL_TRUE );
				pglDisable( GL_BLEND );
			}
		}
	}
	else
	{
		GL_Bind( GL_TEXTURE0, t->gl_texturenum );
	}

	if( is_mirror ) R_BeginDrawMirror( fa );
	if( is_screen || is_cinema ) R_BeginDrawScreen( fa );

	DrawGLPoly( fa->polys, 0.0f, 0.0f );

	if( RI.currententity == GET_ENTITY( 0 ))
		r_stats.c_world_polys++;
	else r_stats.c_brush_polys++; 

	if( is_screen || is_cinema ) R_EndDrawScreen();
	if( is_mirror ) R_EndDrawMirror();

	if(( is_mirror || is_screen || is_cinema ) && ( worldmodel->lightdata && !r_fullbright->value ))
	{
		if( force_no_lightmaps )
		{
			pglDepthFunc( GL_EQUAL );
			pglDepthMask( GL_FALSE );
			pglEnable( GL_BLEND );
		}
	}
}

/*
================
R_DrawTextureChains
================
*/
void R_DrawTextureChains( void )
{
	int		i;
	msurface_t	*s;
	texture_t		*t;

	// make sure what color is reset
	pglColor4ub( 255, 255, 255, 255 );
	R_LoadIdentity(); // set identity matrix

	// restore worldmodel
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	// clip skybox surfaces
	for( s = skychain; s != NULL; s = s->texturechain )
		R_AddSkyBoxSurface( s );

	pglDisable( GL_ALPHA_TEST );

	if( worldmodel->lightdata && !r_fullbright->value )
	{
		pglEnable( GL_BLEND );
		pglDepthMask( GL_FALSE );
		pglDepthFunc( GL_EQUAL );
		pglBlendFunc( R_OVERBRIGHT_SFACTOR(), GL_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}
	else
	{
		pglDisable( GL_BLEND );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	}

	for( i = 0; i < worldmodel->numtextures; i++ )
	{
		t = worldmodel->textures[i];
		if( !t ) continue;

		s = t->texturechain;
		if( !s ) continue;

		if( s->flags & SURF_DRAWTURB )
			continue;	// draw translucent water later

		if( s->flags & SURF_DRAWSKY && ( RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ) || r_wireframe->value ))
			continue;	// draw sky later

		for( ; s != NULL; s = s->texturechain )
		{
			if( !r_lightmap->value || r_fullbright->value )
				R_RenderBrushPoly( s, false );
		}

		if( !worldmodel->lightdata || r_fullbright->value )
		{
			pglEnable( GL_BLEND );
			pglDepthMask( GL_FALSE );
		}
		else pglDepthFunc( GL_LEQUAL );

		pglEnable( GL_POLYGON_OFFSET_FILL );

		// draw the all surface decals
		for( s = t->texturechain; s != NULL; s = s->texturechain )
			R_DrawSurfaceDecals( s );

		pglDisable( GL_POLYGON_OFFSET_FILL );
		pglBlendFunc( R_OVERBRIGHT_SFACTOR(), GL_SRC_COLOR );

		if( worldmodel->lightdata && !r_fullbright->value )
		{
			pglEnable( GL_BLEND );
			pglDepthFunc( GL_EQUAL );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		}
		else
		{
			pglDisable( GL_BLEND );
			pglDepthMask( GL_TRUE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		}
	}

	if( worldmodel->lightdata && !r_fullbright->value )
	{
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglDepthFunc( GL_LEQUAL );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	}
}

void R_DrawWorldFog( void )
{
	qboolean draw = R_SetupFogProjection();
	msurface_t *s;
	texture_t *t;

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		t = worldmodel->textures[i];
		if( !t ) continue;

		s = t->texturechain;
		if( !s ) continue;

		if( s->flags & SURF_DRAWTURB )
			continue;	// draw translucent water later

		if( s->flags & SURF_DRAWSKY && ( RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ) || r_wireframe->value ))
			continue;	// draw sky later

		if( draw )
		{
			for( s = t->texturechain; s != NULL; s = s->texturechain )
			{
				glpoly_t *p = s->polys;
				float *v;
				int o;

				if( !p ) continue;

				pglBegin( GL_POLYGON );
				for( o = 0, v = p->verts[0]; o < p->numverts; o++, v += VERTEXSIZE )
					pglVertex3fv( v );
				pglEnd();
			}
		}

		t->texturechain = NULL;
	}

	if( draw )
	{
		pglDisable( GL_BLEND );
		GL_CleanUpTextureUnits( 0 );
		pglColor4ub( 255, 255, 255, 255 );
	}
}

/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces( bool fTrans )
{
	msurface_t *s;
	texture_t *t;

	if( fTrans && RI.refdef.movevars->wateralpha >= 1.0f )
		return;

	if( !fTrans && RI.refdef.movevars->wateralpha < 1.0f )
		return;

	// go back to the world matrix
	R_LoadIdentity();

	if( fTrans )
	{
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4f( 1.0f, 1.0f, 1.0f, RI.refdef.movevars->wateralpha );
	}
	else
	{
		pglDisable( GL_BLEND );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		pglColor4ub( 255, 255, 255, 255 );
	}

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		t = worldmodel->textures[i];
		if( !t ) continue;

		s = t->texturechain;
		if( !s ) continue;

		if(!( s->flags & SURF_DRAWTURB ))
			continue;

		for( ; s; s = s->texturechain )
		{
			if( RP_NORMALPASS() && FBitSet( s->flags, SURF_REFLECT ))
			{
				GL_Bind( GL_TEXTURE0, SURF_INFO( s, worldmodel )->mirrortexturenum );
				R_BeginDrawMirror( s );
				EmitWaterPolysReflection( s );
				R_EndDrawMirror();
			}
			else
			{
				// set modulate mode explicitly
				GL_Bind( GL_TEXTURE0, t->gl_texturenum );
				EmitWaterPolys( s->polys, ( s->flags & SURF_NOCULL ));
                    	}
                    }

		if( !fTrans && R_SetupFogProjection() )
		{
			for( s = t->texturechain; s != NULL; s = s->texturechain )
				EmitWaterPolys( s->polys, ( s->flags & SURF_NOCULL ));

			pglDisable( GL_BLEND );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
			pglColor4ub( 255, 255, 255, 255 );

			GL_CleanUpTextureUnits( 0 );
		}
			
		t->texturechain = NULL;
	}

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	pglColor4ub( 255, 255, 255, 255 );
}

/*
================
R_DrawSkySurfaces
================
*/
void R_DrawSkySurfaces( void )
{
	msurface_t *s;
	texture_t *t;

	if( !RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ))
		return;

	// go back to the world matrix
	R_LoadIdentity();

	if( tr.skytexturenum > 0 && tr.skytexturenum < worldmodel->numtextures )
	{
		t = worldmodel->textures[tr.skytexturenum];
		if( t )
		{
			s = t->texturechain;
			if( s ) R_DrawSkyChain( s );
			t->texturechain = NULL;
		}
	}
}

/*
=================
R_SurfaceCompare

compare translucent surfaces
=================
*/
static int R_SurfaceCompare( const msurface_t **a, const msurface_t **b )
{
	msurface_t	*surf1, *surf2;
	mextrasurf_t	*info1, *info2;

	surf1 = (msurface_t *)*a;
	surf2 = (msurface_t *)*b;

	info1 = SURF_INFO( surf1, RI.currentmodel );
	info2 = SURF_INFO( surf2, RI.currentmodel );

	Vector org1 = RI.currententity->origin + info1->origin;
	Vector org2 = RI.currententity->origin + info2->origin;

	// compare by plane dists
	float len1 = DotProduct( org1, RI.vforward ) - RI.viewplanedist;
	float len2 = DotProduct( org2, RI.vforward ) - RI.viewplanedist;

	if( len1 > len2 )
		return -1;
	if( len1 < len2 )
		return 1;

	return 0;
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel( cl_entity_t *e )
{
	int		num_lightsurfs;
	int		i, j, k, num_surfaces;
	qboolean		need_sort = false;
	Vector		origin_l, oldorigin;
	Vector		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	qboolean		rotated;

	clmodel = e->model;

	if( e->angles != g_vecZero )
	{
		for( i = 0; i < 3; i++ )
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
		rotated = true;
	}
	else
	{
		mins = e->origin + clmodel->mins;
		maxs = e->origin + clmodel->maxs;
		rotated = false;
	}

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
		return;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
		return;

	if( R_CullBox( mins, maxs, RI.clipFlags ))
		return;

	if( RI.params & ( RP_SKYPORTALVIEW|RP_PORTALVIEW|RP_SCREENVIEW ))
	{
		if( rotated )
		{
			if( R_VisCullSphere( e->origin, clmodel->radius ))
				return;
		}
		else
		{
			if( R_VisCullBox( mins, maxs ))
				return;
		}
	}

	// to prevent drawing sky entities through normal view
	if( tr.sky_camera && tr.fIgnoreSkybox )
	{
		mleaf_t *leaf = Mod_PointInLeaf( tr.sky_camera->origin, worldmodel->nodes );
		if( Mod_CheckEntityLeafPVS( mins, maxs, leaf )) return;	// it's sky entity
	}

	r_stats.num_drawed_ents++;

	RI.currentWaveHeight = RI.currententity->curstate.scale * 32.0f;
	memset( gl_lms.lightmap_surfaces, 0, sizeof( gl_lms.lightmap_surfaces ));
	gl_lms.dynamic_surfaces = NULL;

	if( rotated ) R_RotateForEntity( e );
	else R_TranslateForEntity( e );

	if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.vieworg );
	else tr.modelorg = RI.vieworg - e->origin;

	// accumulate surfaces, build the lightmaps
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( num_surfaces = i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		// in some cases surface is invisible but grass is visible
		R_AddToGrassChain( psurf, RI.frustum, RI.clipFlags, false );

		if( R_CullSurface( psurf, 0 ))
			continue;

		R_AddSurfaceLayers( psurf );
		draw_surfaces[num_surfaces] = psurf;
		num_surfaces++;
		assert( ARRAYSIZE( draw_surfaces ) >= num_surfaces );
	}

	// setup the color and alpha
	switch( e->curstate.rendermode )
	{
	case kRenderTransAdd:
	case kRenderTransTexture:
		need_sort = true;
		break;
	case kRenderTransAlpha:
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		pglAlphaFunc( GL_GEQUAL, 0.5f );
		pglColor4ub( 0, 0, 0, 255 );
		pglEnable( GL_ALPHA_TEST );
		pglDisable( GL_BLEND );

		// draw transparent mask for lightmaps
		for( i = 0; i < num_surfaces; i++ )
			R_RenderBrushPoly( draw_surfaces[i], true );
	default:	
		pglColor4ub( 255, 255, 255, 255 );
		break;
	}

	if( need_sort )
		qsort( draw_surfaces, num_surfaces, sizeof( msurface_t* ), (cmpfunc)R_SurfaceCompare );

	R_BlendLightmaps();

	if( R_CountPlights( ) && !need_sort )
	{
		for( int i = 0; i < MAX_PLIGHTS; i++ )
		{
			plight_t *pl = &cl_plights[i];
			float *v;

			if( pl->die < GET_CLIENT_TIME() || !pl->radius )
				continue;

			if( R_CullBoxExt( pl->frustum, mins, maxs, 0 ))
				continue;

			if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( pl->origin );
			else tr.modelorg = pl->origin - e->origin;

			// accumulate lit surfaces
			psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
			for( num_lightsurfs = j = 0; j < clmodel->nummodelsurfaces; j++, psurf++ )
			{
				R_AddToGrassChain( psurf, RI.frustum, RI.clipFlags, true );

				if( R_CullSurfaceExt( psurf, pl->frustum, 0 ))
					continue;
                                        
				if( !( psurf->flags & (SURF_DRAWTILED|SURF_PORTAL|SURF_REFLECT)))
				{
					light_surfaces[num_lightsurfs] = psurf;
					num_lightsurfs++;
					assert( ARRAYSIZE( light_surfaces ) >= num_lightsurfs );
				}
			}

			R_DrawGrass( GRASS_PASS_AMBIENT );
			R_MergeLightProjection( pl );
			R_BeginDrawProjection( pl );

			for( j = 0; j < num_lightsurfs; j++ )
			{
				psurf = light_surfaces[j];
				
				pglBegin( GL_POLYGON );
				for( k = 0, v = psurf->polys->verts[0]; k < psurf->polys->numverts; k++, v += VERTEXSIZE )
					pglVertex3fv( v );
				pglEnd();
                              }

			R_DrawGrass( GRASS_PASS_LIGHT );
			R_EndDrawProjection();
		}
	}

	switch( e->curstate.rendermode )
	{
	case kRenderTransAdd:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4ub( 255, 255, 255, e->curstate.renderamt );
		break;
	case kRenderTransTexture:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		if( r_lighting_extended->value >= 2.0f )
			pglBlendFunc( GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA );
		else pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4ub( 255, 255, 255, e->curstate.renderamt );
		break;
	case kRenderTransColor:
		pglEnable( GL_BLEND );
		pglDisable( GL_TEXTURE_2D );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4ub( e->curstate.rendercolor.r, e->curstate.rendercolor.g,
		e->curstate.rendercolor.b, e->curstate.renderamt );
		break;
	case kRenderTransAlpha:
	default:	
		if( worldmodel->lightdata && !r_fullbright->value && !R_IsWater( ))
		{
			pglEnable( GL_BLEND );
			pglDepthMask( GL_FALSE );
			pglDepthFunc( GL_EQUAL );
			pglBlendFunc( R_OVERBRIGHT_SFACTOR(), GL_SRC_COLOR );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		}
		else
		{
			pglDisable( GL_BLEND );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		}
		break;
	}

	for( i = 0; i < num_surfaces; i++ )
	{
		psurf = draw_surfaces[i];

		if( psurf->flags & SURF_DRAWSKY )
		{
			if( RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ))
			{
				// warp texture, no lightmaps
				EmitSkyLayers( psurf );
			}
		}
		else if( psurf->flags & SURF_DRAWTURB )
		{
			if( RP_NORMALPASS() && FBitSet( psurf->flags, SURF_REFLECT ))
			{
				GL_Bind( GL_TEXTURE0, SURF_INFO( psurf, RI.currentmodel )->mirrortexturenum );
				R_BeginDrawMirror( psurf );
				EmitWaterPolysReflection( psurf );
				R_EndDrawMirror();
			}
			else
			{
				// warp texture, no lightmaps
				GL_Bind( GL_TEXTURE0, psurf->texinfo->texture->gl_texturenum );
				EmitWaterPolys( psurf->polys, ( psurf->flags & SURF_NOCULL ), ( psurf->flags & SURF_REFLECT ));
			}
		}
		else if( !r_lightmap->value || r_fullbright->value )
			R_RenderBrushPoly( psurf, false );
	}

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_LEQUAL );
	pglDisable( GL_ALPHA_TEST );
	pglEnable( GL_POLYGON_OFFSET_FILL );

	if( e->curstate.rendermode == kRenderTransColor )
		pglEnable( GL_TEXTURE_2D );

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		GL_Cull( GL_NONE );

	for( i = 0; i < num_surfaces; i++ )
	{
		psurf = draw_surfaces[i];
		if( psurf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
			continue;

		R_DrawSurfaceDecals( psurf );
	}

	pglDisable( GL_BLEND );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	if( !glState.drawTrans )
		pglDepthMask( GL_TRUE );

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		GL_Cull( GL_FRONT );

	R_RenderFullbrights();
	R_RenderChrome();
	R_RenderDetails();

	if( !need_sort && R_SetupFogProjection() )
	{
		pglDepthFunc( GL_EQUAL );

		for( i = 0; i < num_surfaces; i++ )
		{
			glpoly_t *p = draw_surfaces[i]->polys;
			float *v;
			int o;

			if( !p ) continue;

			pglBegin( GL_POLYGON );
			for( o = 0, v = p->verts[0]; o < p->numverts; o++, v += VERTEXSIZE )
				pglVertex3fv( v );
			pglEnd();
		}

		pglDepthFunc( GL_LEQUAL );
		GL_CleanUpTextureUnits( 0 );
		pglColor4ub( 255, 255, 255, 255 );
	}

	// grass stays in local space
	// so we use bmodel matrix for right transform
	R_DrawGrass( GRASS_PASS_NORMAL );
	R_DrawGrass( GRASS_PASS_DIFFUSE );
	R_DrawGrass( GRASS_PASS_FOG );

	R_LoadIdentity();	// restore worldmatrix
}

/*
=================
R_DrawStaticModel

Merge static model brushes with world surfaces
=================
*/
void R_DrawStaticModel( cl_entity_t *e )
{
	model_t		*clmodel;
	msurface_t	*psurf;
	
	clmodel = e->model;

	if( R_CullBox( clmodel->mins, clmodel->maxs, RI.clipFlags ))
		return;

	if( RI.params & ( RP_SKYPORTALVIEW|RP_PORTALVIEW|RP_SCREENVIEW ))
	{
		if( R_VisCullBox( clmodel->mins, clmodel->maxs ))
			return;
	}

	// to prevent drawing sky entities through normal view
	if( tr.sky_camera && tr.fIgnoreSkybox )
	{
		mleaf_t *leaf = Mod_PointInLeaf( tr.sky_camera->origin, worldmodel->nodes );
		if( Mod_CheckEntityLeafPVS( clmodel->mins, clmodel->maxs, leaf ))
			return;	// it's sky entity
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurface( psurf, RI.clipFlags ))
			continue;

		if( psurf->flags & SURF_DRAWSKY && !RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ))
		{
			if( !tr.fIgnoreSkybox )
			{
				// make sky chain to right clip the skybox
				psurf->texturechain = skychain;
				skychain = psurf;
			}
		}
		else
		{ 
			psurf->texturechain = psurf->texinfo->texture->texturechain;
			psurf->texinfo->texture->texturechain = psurf;
			R_AddSurfaceLayers( psurf );
		}
	}
}

/*
=================
R_DrawStaticBrushes

Insert static brushes into world texture chains
=================
*/
void R_DrawStaticBrushes( void )
{
	// draw static entities
	for( int i = 0; i < tr.num_static_entities; i++ )
	{
		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currententity->model->type )
		{
		case mod_brush:
			R_DrawStaticModel( RI.currententity );
			break;
		default:
			HOST_ERROR( "R_DrawStatics: non bsp model in static list!\n" );
			break;
		}
	}
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/
/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags )
{
	const mplane_t	*clipplane;
	int		i, clipped;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	int		c, side;
	float		dot;

	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe != tr.visframecount )
		return;

	if( clipflags )
	{
		for( i = 0; i < 6; i++ )
		{
			clipplane = &frustum[i];

			if(!( clipflags & ( 1<<i )))
				continue;

			clipped = BoxOnPlaneSide( node->minmaxs, node->minmaxs + 3, clipplane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipflags &= ~(1<<i);
		}
	}

	// if a leaf node, draw stuff
	if( node->contents < 0 )
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if( c )
		{
			do
			{
				(*mark)->visframe = tr.framecount;
				mark++;
			} while( --c );
		}

		// deal with model fragments in this leaf
		if( pleaf->efrags )
			STORE_EFRAGS( &pleaf->efrags, tr.realframecount );
		r_stats.c_world_leafs++;
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( tr.modelorg, node->plane );
	side = (dot >= 0.0f) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode( node->children[side], frustum, clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		// in some cases surface is invisible but grass is visible
		R_AddToGrassChain( surf, frustum, clipflags, false );

		if( R_CullSurfaceExt( surf, frustum, clipflags ))
			continue;

		if( surf->flags & SURF_DRAWSKY && !RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ))
		{
			if( !tr.fIgnoreSkybox )
			{
				// make sky chain to right clip the skybox
				surf->texturechain = skychain;
				skychain = surf;
			}
		}
		else
		{ 
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
			R_AddSurfaceLayers( surf );
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode( node->children[!side], frustum, clipflags );
}

/*
================
R_CullNodeTopView

cull node by user rectangle (simple scissor)
================
*/
qboolean R_CullNodeTopView( mnode_t *node )
{
	Vector2D	delta, size;
	Vector	center, half, mins, maxs;

	// build the node center and half-diagonal
	mins = node->minmaxs, maxs = node->minmaxs + 3;
	center = (mins + maxs) * 0.5f;
	half = maxs - center;

	// cull against the screen frustum or the appropriate area's frustum.
	delta.x = center.x - world_orthocenter.x;
	delta.y = center.y - world_orthocenter.y;
	size.x = half.x + world_orthohalf.x;
	size.y = half.y + world_orthohalf.y;

	return ( fabs( delta.x ) > size.x ) || ( fabs( delta.y ) > size.y );
}

/*
================
R_DrawTopViewLeaf
================
*/
static void R_DrawTopViewLeaf( mleaf_t *pleaf, const mplane_t frustum[6], unsigned int clipflags )
{
	msurface_t	**mark, *surf;
	int		i;

	for( i = 0, mark = pleaf->firstmarksurface; i < pleaf->nummarksurfaces; i++, mark++ )
	{
		surf = *mark;

		// don't process the same surface twice
		if( surf->visframe == tr.framecount )
			continue;

		surf->visframe = tr.framecount;

		if( R_CullSurfaceExt( surf, frustum, clipflags ))
			continue;

		if( !( surf->flags & SURF_DRAWSKY ))
		{ 
			surf->texturechain = surf->texinfo->texture->texturechain;
			surf->texinfo->texture->texturechain = surf;
			R_AddSurfaceLayers( surf );
		}
	}

	// deal with model fragments in this leaf
	if( pleaf->efrags )
		STORE_EFRAGS( &pleaf->efrags, tr.realframecount );
	r_stats.c_world_leafs++;
}

/*
================
R_DrawWorldTopView
================
*/
void R_DrawWorldTopView( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags )
{
	const mplane_t	*clipplane;
	int		c, clipped;
	msurface_t	*surf;

	do
	{
		if( node->contents == CONTENTS_SOLID )
			return;	// hit a solid leaf

		if( node->visframe != tr.visframecount )
			return;

		if( clipflags )
		{
			for( c = 0; c < 6; c++ )
			{
				clipplane = &frustum[c];

				if(!( clipflags & ( 1<<c )))
					continue;

				clipped = BoxOnPlaneSide( node->minmaxs, node->minmaxs + 3, clipplane );
				if( clipped == 2 ) return;
				if( clipped == 1 ) clipflags &= ~(1<<c);
			}
		}

		// cull against the screen frustum or the appropriate area's frustum.
		if( R_CullNodeTopView( node ))
			return;

		// if a leaf node, draw stuff
		if( node->contents < 0 )
		{
			R_DrawTopViewLeaf( (mleaf_t *)node, frustum, clipflags );
			return;
		}

		// draw stuff
		for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
		{
			// don't process the same surface twice
			if( surf->visframe == tr.framecount )
				continue;

			surf->visframe = tr.framecount;

			if( R_CullSurfaceExt( surf, frustum, clipflags ))
				continue;

			if( !( surf->flags & SURF_DRAWSKY ))
			{ 
				surf->texturechain = surf->texinfo->texture->texturechain;
				surf->texinfo->texture->texturechain = surf;
				R_AddSurfaceLayers( surf );
			}
		}

		// recurse down both children, we don't care the order...
		R_DrawWorldTopView( node->children[0], frustum, clipflags );
		node = node->children[1];

	} while( node );
}

/*
=============
R_DrawTriangleOutlines
=============
*/
void R_DrawTriangleOutlines( void )
{
	int		i, j;
	msurface_t	*surf;
	glpoly_t		*p;
	float		*v;
		
	if( !r_wireframe->value )
		return;

	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	// render static surfaces first
	for( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		for( surf = gl_lms.lightmap_surfaces[i]; surf != NULL; surf = surf->lightmapchain )
		{
			p = surf->polys;
			for( ; p != NULL; p = p->chain )
			{
				pglBegin( GL_POLYGON );
				v = p->verts[0];
				for( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
					pglVertex3fv( v );
				pglEnd ();
			}
		}
	}

	// render surfaces with dynamic lightmaps
	for( surf = gl_lms.dynamic_surfaces; surf != NULL; surf = surf->lightmapchain )
	{
		p = surf->polys;

		for( ; p != NULL; p = p->chain )
		{
			pglBegin( GL_POLYGON );
			v = p->verts[0];
			for( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
				pglVertex3fv( v );
			pglEnd ();
		}
	}

	pglColor4f( 0.0f, 1.0f, 0.0f, 1.0f );

	// render surfaces with sky polygons
	for( surf = skychain; surf != NULL; surf = surf->texturechain )
	{
		p = surf->polys;

		for( ; p != NULL; p = p->chain )
		{
			pglBegin( GL_POLYGON );
			v = p->verts[0];
			for( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
				pglVertex3fv( v );
			pglEnd ();
		}
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglEnable( GL_DEPTH_TEST );
	pglEnable( GL_TEXTURE_2D );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	if( RI.refdef.onlyClientDraw )
		return;

	tr.modelorg = RI.vieworg;

	memset( gl_lms.lightmap_surfaces, 0, sizeof( gl_lms.lightmap_surfaces ));
	memset( fullbright_polys, 0, sizeof( fullbright_polys ));
	memset( detail_surfaces, 0, sizeof( detail_surfaces ));
	chrome_surfaces = NULL;

	RI.currentWaveHeight = RI.waveHeight;
	GL_SetRenderMode( kRenderNormal );
	gl_lms.dynamic_surfaces = NULL;

	R_ClearSkyBox ();

	if( RI.drawOrtho )
	{
		R_DrawWorldTopView( worldmodel->nodes, RI.frustum, RI.clipFlags );
	}
	else
	{
		R_RecursiveWorldNode( worldmodel->nodes, RI.frustum, RI.clipFlags );
	}

	R_DrawStaticBrushes();
	R_DrawSkySurfaces();

	R_BlendLightmaps();

	if( R_CountPlights( ))
	{
		int oldframecount = tr.framecount;

		for( int i = 0; i < MAX_PLIGHTS; i++ )
		{
			plight_t *pl = &cl_plights[i];

			if( pl->die < GET_CLIENT_TIME() || !pl->radius )
				continue;

			tr.framecount++;
			tr.modelorg = pl->origin;

			if(!( pl->flags & CF_NOWORLD_PROJECTION ))
				R_RecursiveLightNode( worldmodel->nodes, pl->frustum, pl->clipflags );

			R_DrawGrass( GRASS_PASS_AMBIENT );
			R_LightStaticBrushes( pl );

			R_BeginDrawProjection( pl );
			R_DrawLightChains();
			R_EndDrawProjection();
		}
		tr.framecount = oldframecount;
	}

	R_DrawTextureChains();
	R_RenderFullbrights();
	R_RenderChrome();
	R_RenderDetails();
	R_DrawWorldFog();

	if( skychain )
		R_DrawSkyBox();

	R_DrawTriangleOutlines ();
	skychain = NULL;

	R_DrawGrass( GRASS_PASS_NORMAL );
	R_DrawGrass( GRASS_PASS_DIFFUSE );
	R_DrawGrass( GRASS_PASS_FOG );
	tr.grassframecount++;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current leaf
===============
*/
void R_MarkLeaves( void )
{
	byte	*vis;
	mnode_t	*node;
	int	i;

	if( r_novis->value || tr.fResetVis )
	{
		// force recalc viewleaf
		tr.fResetVis = false;
		r_viewleaf = NULL;
	}

	if( r_viewleaf == r_oldviewleaf && r_viewleaf2 == r_oldviewleaf2 && !r_novis->value && r_viewleaf != NULL )
		return;

	// development aid to let you run around
	// and see exactly where the pvs ends
	if( r_lockpvs->value ) return;

	if( !FBitSet( RI.params, RP_MERGEVISIBILITY ))
		tr.visframecount++;

	// cache current values
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	int longs = ( worldmodel->numleafs + 31 ) >> 5;
		
	if( r_novis->value || RI.drawOrtho || !r_viewleaf || !worldmodel->visdata )
	{
		// mark everything
		for( i = 0; i < worldmodel->numleafs; i++ )
			worldmodel->leafs[i+1].visframe = tr.visframecount;
		for( i = 0; i < worldmodel->numnodes; i++ )
			worldmodel->nodes[i].visframe = tr.visframecount;
		memset( RI.visbytes, 0xFF, longs << 2 );
		return;
	}

	if( RI.params & RP_MERGEVISIBILITY )
	{
		// merge main visibility with additional pass
		vis = Mod_LeafPVS( r_viewleaf, worldmodel );

		for( i = 0; i < longs; i++ )
			((int *)RI.visbytes)[i] |= ((int *)vis)[i];
	}
	else
	{
		// set primary vis info
		memcpy( RI.visbytes, Mod_LeafPVS( r_viewleaf, worldmodel ), longs << 2 );
          }

	// may have to combine two clusters
	// because of solid water boundaries
	if( r_viewleaf != r_viewleaf2 )
	{
		vis = Mod_LeafPVS( r_viewleaf2, worldmodel );

		for( i = 0; i < longs; i++ )
			((int *)RI.visbytes)[i] |= ((int *)vis)[i];
	}

	for( i = 0; i < worldmodel->numleafs; i++ )
	{
		if( RI.visbytes[i>>3] & ( 1<<( i & 7 )))
		{
			node = (mnode_t *)&worldmodel->leafs[i+1];
			do
			{
				if( node->visframe == tr.visframecount )
					break;
				node->visframe = tr.visframecount;
				node = node->parent;
			} while( node );
		}
	}
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap( msurface_t *surf )
{
	int	smax, tmax;
	byte	*base;

	if( !worldmodel->lightdata ) return;
	if( surf->flags & SURF_DRAWTILED )
		return;

	smax = ( surf->extents[0] / LM_SAMPLE_SIZE ) + 1;
	tmax = ( surf->extents[1] / LM_SAMPLE_SIZE ) + 1;

	if( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ))
	{
		LM_UploadBlock( false );
		LM_InitBlock();

		if( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ))
			HOST_ERROR( "AllocBlock: full\n" );
	}

	surf->lightmaptexturenum = gl_lms.current_lightmap_texture;

	base = gl_lms.lightmap_buffer;
	base += ( surf->light_t * BLOCK_SIZE + surf->light_s ) * 4;

	R_BuildLightMap( surf, base, BLOCK_SIZE * 4 );
	R_ReLightGrass( surf, true );
	R_SetCacheState( surf );
}

/*
==================
GL_BuildLightmaps

Builds the lightmap texture on a new level
or when gamma is changed
==================
*/
void HUD_BuildLightmaps( void )
{
	int	i, j;
	texture_t	*tx;
	model_t	*m;

	if( !g_fRenderInitialized ) return;

	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;
	if( !worldmodel ) return; // wait for worldmodel

	// not a gamma change, just a restart or change level
	if( !RENDER_GET_PARM( PARM_REBUILD_GAMMA, 0 ))
	{
		// Engine already released entity array so we can't release
		// model instance for each entity pesronally 
		g_StudioRenderer.DestroyAllModelInstances();

		// invalidate model handles
		for( i = 1; i < RENDER_GET_PARM( PARM_MAX_ENTITIES, 0 ); i++ )
		{
			cl_entity_t *e = GET_ENTITY( i );
			if( !e ) break;

			e->modelhandle = INVALID_HANDLE;
		}

		GET_VIEWMODEL()->modelhandle = INVALID_HANDLE;

		// clear partsystem
		g_pParticleSystems->ClearSystems();

		// clear weather system
		R_ResetWeather();
	}

	if( RENDER_GET_PARM( PARM_WORLD_VERSION, 0 ) == 31 )
		tr.lm_sample_size = 8;
	else tr.lm_sample_size = 16;

	if( RENDER_GET_PARM( PARM_FEATURES, 0 ) & ENGINE_LARGE_LIGHTMAPS )
		glConfig.block_size = BLOCK_SIZE_MAX;
	else glConfig.block_size = BLOCK_SIZE_DEFAULT;

	memset( tr.lightmapTextures, 0, sizeof( tr.lightmapTextures ));
	gl_lms.current_lightmap_texture = 0;
	tr.world_has_portals = false;
	tr.world_has_screens = false;
	tr.world_has_movies = false;
	tr.num_cin_used = 0;

	skychain = NULL;

	// setup all the lightstyles
	R_AnimateLight();

	// clearing texture chains
	for( i = 0; i < worldmodel->numtextures; i++ )
	{
		if( !worldmodel->textures[i] )
			continue;

		tx = worldmodel->textures[i];
 		tx->lightchain = NULL;
	}

	LM_InitBlock();	

	for( i = 1; i < MAX_MODELS; i++ )
	{
		if(( m = IEngineStudio.GetModelByIndex( i )) == NULL )
			continue;

		if( m->name[0] == '*' || m->type != mod_brush )
			continue;

		RI.currentmodel = m;

		for( j = 0; j < m->numsurfaces; j++ )
		{
			msurface_t *fa = m->surfaces + j;
			texture_t *tex = fa->texinfo->texture;
			mextrasurf_t *info = SURF_INFO( fa, m );

			info->mirrortexturenum = 0;
			info->checkcount = -1;

			if( !Q_strncmp( tex->name, "portal", 6 ))
			{
				fa->flags |= SURF_PORTAL;
				tr.world_has_portals = true;
			}
			else if( !Q_strncmp( tex->name, "monitor", 7 ))
			{
				fa->flags |= SURF_SCREEN;
				tr.world_has_screens = true;
			}
			else if( !Q_strncmp( tex->name, "movie", 5 ))
			{
				fa->flags |= SURF_MOVIE;
				tr.world_has_movies = true;
			}
			else if( !Q_strncmp( tex->name, "chrome", 6 ))
			{
				fa->flags |= SURF_CHROME;
			}

			GL_CreateSurfaceLightmap( fa );
		}
	}

	LM_UploadBlock( false );
}
