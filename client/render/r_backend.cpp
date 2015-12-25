//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_sprite.h"
#include "mathlib.h"

glState_t	glState;
glConfig_t glConfig;

/*
=============
R_GetSpriteTexture

=============
*/
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame )
{
	if( !m_pSpriteModel || m_pSpriteModel->type != mod_sprite || !m_pSpriteModel->cache.data )
		return 0;

	return R_GetSpriteFrame( m_pSpriteModel, frame )->gl_texturenum;
}

/*
=============
TriSpriteTexture

bind current texture
=============
*/
int TriSpriteTexture( model_t *pSpriteModel, int frame )
{
	int	gl_texturenum;
	msprite_t	*psprite;

	if(( gl_texturenum = R_GetSpriteTexture( pSpriteModel, frame )) == 0 )
		return 0;

	psprite = (msprite_t *)pSpriteModel->cache.data;
	if( psprite->texFormat == SPR_ALPHTEST )
	{
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GREATER, 0.0f );
	}

	GL_Bind( GL_TEXTURE0, gl_texturenum );

	return 1;
}

/*
==============
GL_DisableAllTexGens
==============
*/
void GL_DisableAllTexGens( void )
{
	GL_TexGen( GL_S, 0 );
	GL_TexGen( GL_T, 0 );
	GL_TexGen( GL_R, 0 );
	GL_TexGen( GL_Q, 0 );
}

/*
=================
GL_Cull
=================
*/
void GL_Cull( GLenum cull )
{
	if( !cull )
	{
		pglDisable( GL_CULL_FACE );
		glState.faceCull = 0;
		return;
	}

	pglEnable( GL_CULL_FACE );
	pglCullFace( cull );
	glState.faceCull = cull;
}

/*
=================
GL_FrontFace
=================
*/
void GL_FrontFace( GLenum front )
{
	pglFrontFace( front ? GL_CW : GL_CCW );
	glState.frontFace = front;
}

void GL_SetRenderMode( int mode )
{
	switch( mode )
	{
	case kRenderNormal:
	default:
		pglDisable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case kRenderTransColor:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case kRenderTransAlpha:
		pglDisable( GL_BLEND );
		pglEnable( GL_ALPHA_TEST );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case kRenderTransTexture:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case kRenderGlow:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case kRenderTransAdd:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	}
}

/*
=================
GL_LoadTexMatrix
=================
*/
void GL_LoadTexMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	GL_LoadTextureMatrix( dest );
}

/*
=================
GL_LoadMatrix
=================
*/
void GL_LoadMatrix( const matrix4x4 source )
{
	GLfloat	dest[16];

	source.CopyToArray( dest );
	pglLoadMatrixf( dest );
}

/*
================
R_BeginDrawProjection

Setup texture matrix for light texture
================
*/
void R_BeginDrawProjection( const plight_t *pl, bool decalPass )
{
	GLfloat	genVector[4][4];

	RI.currentlight = pl;
	pglEnable( GL_BLEND );

	if( glState.drawTrans )
	{
		// particle lighting
		pglDepthFunc( GL_LEQUAL );
		pglBlendFunc( GL_SRC_COLOR, GL_SRC_ALPHA );
		pglColor4ub( pl->color.r*0.3, pl->color.g*0.3, pl->color.b*0.3, 255 );
	}
	else
	{
		pglBlendFunc( GL_ONE, GL_ONE );

		if( R_OVERBRIGHT_SILENT() && !decalPass )
			pglColor4ub( pl->color.r / 2.0f, pl->color.g / 2.0f, pl->color.b / 2.0f, 255 );
		else
			pglColor4ub( pl->color.r, pl->color.g, pl->color.b, 255 );

		if( decalPass ) pglDepthFunc( GL_LEQUAL );
		else pglDepthFunc( GL_EQUAL );
	}

	GL_Bind( GL_TEXTURE0, pl->projectionTexture );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	for( int i = 0; i < 4; i++ )
	{
		genVector[0][i] = i == 0 ? 1 : 0;
		genVector[1][i] = i == 1 ? 1 : 0;
		genVector[2][i] = i == 2 ? 1 : 0;
		genVector[3][i] = i == 3 ? 1 : 0;
	}

	GL_TexGen( GL_S, GL_OBJECT_LINEAR );
	GL_TexGen( GL_T, GL_OBJECT_LINEAR );
	GL_TexGen( GL_R, GL_OBJECT_LINEAR );
	GL_TexGen( GL_Q, GL_OBJECT_LINEAR );

	pglTexGenfv( GL_S, GL_OBJECT_PLANE, genVector[0] );
	pglTexGenfv( GL_T, GL_OBJECT_PLANE, genVector[1] );
	pglTexGenfv( GL_R, GL_OBJECT_PLANE, genVector[2] );
	pglTexGenfv( GL_Q, GL_OBJECT_PLANE, genVector[3] );

	if( tr.modelviewIdentity )
		GL_LoadTexMatrix( pl->textureMatrix );
	else GL_LoadTexMatrix( pl->textureMatrix2 );

	glState.drawProjection = true;

	// setup attenuation texture
	if( pl->attenuationTexture != 0 )
	{
		GL_Bind( GL_TEXTURE1, pl->attenuationTexture );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		if( pl->pointlight )
		{
			float r = 1.0f / (pl->radius * 2);
			Vector origin;

			if( !tr.modelviewIdentity )
			{
				// rotate attenuation texture into local space
				if( RI.currententity->angles != g_vecZero )
					origin = RI.objectMatrix.VectorITransform( pl->origin );
				else origin = pl->origin - RI.currententity->origin;
			}
			else origin = pl->origin;

			GLfloat planeS[] = { r, 0, 0, -origin[0] * r + 0.5 };
			GLfloat planeT[] = { 0, r, 0, -origin[1] * r + 0.5 };
			GLfloat planeR[] = { 0, 0, r, -origin[2] * r + 0.5 };

			GL_TexGen( GL_S, GL_EYE_LINEAR );
			GL_TexGen( GL_T, GL_EYE_LINEAR );
			GL_TexGen( GL_R, GL_EYE_LINEAR );

			pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
			pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );
			pglTexGenfv( GL_R, GL_EYE_PLANE, planeR );
                    }
                    else
                    {
			GLfloat	genPlaneS[4];
			Vector	origin, normal;

			if( !tr.modelviewIdentity )
			{
				if( RI.currententity->angles != g_vecZero )
				{
					// rotate attenuation texture into local space
					normal = RI.objectMatrix.VectorIRotate( pl->frustum[5].normal );
					origin = RI.objectMatrix.VectorITransform( pl->origin );
				}
				else
				{
					normal = pl->frustum[5].normal;
					origin = pl->origin - RI.currententity->origin;
				}
			}
			else
			{
				normal = pl->frustum[5].normal;
				origin = pl->origin;
			}
			genPlaneS[0] = normal[0] / pl->radius;
			genPlaneS[1] = normal[1] / pl->radius;
			genPlaneS[2] = normal[2] / pl->radius;
			genPlaneS[3] = -(DotProduct( normal, origin ) / pl->radius);

			GL_TexGen( GL_S, GL_OBJECT_LINEAR );
			pglTexGenfv( GL_S, GL_OBJECT_PLANE, genPlaneS );
		}

		GL_LoadIdentityTexMatrix();
	}
	else
	{
		// can't draw shadows without attenuation texture
		return;
	}

	if( decalPass )
	{
		if( cg.decal0_shader && ( r_shadows->value <= 0.0f || pl->pointlight || FBitSet( pl->flags, CF_NOSHADOWS )))
		{
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, (pl->pointlight) ? cg.decal3_shader : cg.decal0_shader );
		}
		else if( r_shadows->value == 1.0f && cg.decal1_shader )
		{
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.decal1_shader );
		} 
		else if( r_shadows->value > 1.0f && cg.decal2_shader )
		{
			pglEnable( GL_FRAGMENT_PROGRAM_ARB );
			pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.decal2_shader );
		}
	}
	else if( r_shadows->value > 1.0f && cg.shadow_shader && !pl->pointlight && !FBitSet( pl->flags, CF_NOSHADOWS ))
	{
		pglEnable( GL_FRAGMENT_PROGRAM_ARB );
		pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.shadow_shader );
	}

	// TODO: allow shadows for pointlights
	if( r_shadows->value <= 0.0f || pl->pointlight || FBitSet( pl->flags, CF_NOSHADOWS ))
		return;		

	GL_Bind( GL_TEXTURE2, pl->shadowTexture );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	GL_TexGen( GL_T, GL_EYE_LINEAR );
	GL_TexGen( GL_R, GL_EYE_LINEAR );
	GL_TexGen( GL_Q, GL_EYE_LINEAR );

	pglTexGenfv( GL_S, GL_EYE_PLANE, genVector[0] );
	pglTexGenfv( GL_T, GL_EYE_PLANE, genVector[1] );
	pglTexGenfv( GL_R, GL_EYE_PLANE, genVector[2] );
	pglTexGenfv( GL_Q, GL_EYE_PLANE, genVector[3] );

	if( tr.modelviewIdentity )
		GL_LoadTexMatrix( pl->shadowMatrix );
	else GL_LoadTexMatrix( pl->shadowMatrix2 );
}

/*
================
R_BeginDrawProjection

Setup texture matrix for fog texture
================
*/
qboolean R_SetupFogProjection( void )
{
	Vector	origin, vieworg;
	float	r;

	if( !RI.fogEnabled && !RI.fogCustom )
		return false;

	if( !tr.fogTexture2D || !tr.fogTexture1D )
		return false;

	if( RI.params & RP_SHADOWVIEW )
		return false;
	else if( RI.params & RP_MIRRORVIEW )
		vieworg = r_lastRefdef.vieworg;
	else vieworg = RI.vieworg;

	if( !tr.modelviewIdentity )
	{
		// rotate attenuation texture into local space
		if( RI.currententity->angles != g_vecZero )
			origin = RI.objectMatrix.VectorITransform( vieworg );
		else origin = vieworg - RI.currententity->origin;
	}
	else origin = vieworg;

	if( RI.fogCustom )
		r = 1.0f / (( RI.fogStart + RI.fogEnd ) * 2.0f );
	else
		r = 1.0f / (( 6.0f / RI.fogDensity ) * 2.0f );

	GLfloat planeS[] = { r, 0, 0, -origin[0] * r + 0.5 };
	GLfloat planeT[] = { 0, r, 0, -origin[1] * r + 0.5 };
	GLfloat planeR[] = { 0, 0, r, -origin[2] * r + 0.5 };

	pglEnable( GL_BLEND );
	pglColor3fv( RI.fogColor );
	pglBlendFunc( GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA );

	GL_Bind( GL_TEXTURE0, tr.fogTexture2D );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	GL_TexGen( GL_T, GL_EYE_LINEAR );
	pglTexGenfv( GL_S, GL_EYE_PLANE, planeS );
	pglTexGenfv( GL_T, GL_EYE_PLANE, planeT );

	GL_Bind( GL_TEXTURE1, tr.fogTexture1D );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	GL_TexGen( GL_S, GL_EYE_LINEAR );
	pglTexGenfv( GL_S, GL_EYE_PLANE, planeR );

	return true;
}

/*
================
R_EndDrawProjection

Restore identity texmatrix
================
*/
void R_EndDrawProjection( void )
{
	if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
		pglDisable( GL_FRAGMENT_PROGRAM_ARB );

	GL_CleanUpTextureUnits( 0 );

	pglMatrixMode( GL_MODELVIEW );
	glState.drawProjection = false;
	pglColor4ub( 255, 255, 255, 255 );

	pglDepthFunc( GL_LEQUAL );
	pglDisable( GL_BLEND );
	RI.currentlight = NULL;
}

int R_AllocFrameBuffer( void )
{
	int i = tr.num_framebuffers;

	if( i >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_AllocateFrameBuffer: FBO limit exceeded!\n" );
		return -1; // disable
	}

	gl_fbo_t *fbo = &tr.frame_buffers[i];
	tr.num_framebuffers++;

	if( fbo->init )
	{
		ALERT( at_warning, "R_AllocFrameBuffer: buffer %i already initialized\n", i );
		return i;
	}

	int width, height;

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		// allow screen size
		width = bound( 96, glState.width, 1024 );
		height = bound( 72, glState.height, 768 );
	}
	else
	{
		width = NearestPOW( glState.width, true );
		height = NearestPOW( glState.height, true );
		width = bound( 128, width, 1024 );
		height = bound( 64, height, 512 );
	}

	// create a depth-buffer
	pglGenRenderbuffers( 1, &fbo->renderbuffer );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglRenderbufferStorage( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, width, height );
	pglBindRenderbuffer( GL_RENDERBUFFER_EXT, 0 );

	// create frame-buffer
	pglGenFramebuffers( 1, &fbo->framebuffer );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
	pglFramebufferRenderbuffer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, fbo->renderbuffer );
	pglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglReadBuffer( GL_COLOR_ATTACHMENT0_EXT );
	pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	fbo->init = true;

	return i;
}

void R_FreeFrameBuffer( int buffer )
{
	if( buffer < 0 || buffer >= MAX_FRAMEBUFFERS )
	{
		ALERT( at_error, "R_FreeFrameBuffer: invalid buffer enum %i\n", buffer );
		return;
	}

	gl_fbo_t *fbo = &tr.frame_buffers[buffer];

	pglDeleteRenderbuffers( 1, &fbo->renderbuffer );
	pglDeleteFramebuffers( 1, &fbo->framebuffer );
	memset( fbo, 0, sizeof( *fbo ));
}

void GL_BindFrameBuffer( int buffer, int texture )
{
	gl_fbo_t *fbo = NULL;

	if( buffer >= 0 && buffer < MAX_FRAMEBUFFERS )
		fbo = &tr.frame_buffers[buffer];

	if( !fbo )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
		glState.frameBuffer = buffer;
		return;
	}

	if( glState.frameBuffer != buffer )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo->framebuffer );
		glState.frameBuffer = fbo->framebuffer;
	}		

	if( fbo->texture != texture )
	{
		// change texture attachment
		GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, texture );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texnum, 0 );
		fbo->texture = texture;
	}
}

void R_SetupOverbright( qboolean active )
{
	if( R_OVERBRIGHT() )
	{
		if( active )
		{
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
			pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE );
			pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2 );
		}
		else
		{
			pglTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1 );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		}
	}
}
