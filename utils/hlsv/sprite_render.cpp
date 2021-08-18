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
#include "GLWindow.h"
#include "ViewerSettings.h"
#include "stringlib.h" 

#include <mx.h>
#include "sprviewer.h"

vec3_t		g_lightcolor;

bool		bUseWeaponOrigin = false;
extern bool	g_bStopPlaying;

void SpriteModel :: centerView( bool reset )
{
	vec3_t	min, max;

	ExtractBbox( min, max );

	float dx = max[0] - min[0];
	float dy = max[1] - min[1];
	float dz = max[2] - min[2];
	float d = max( dx, max( dy, dz ));

	if( reset )
	{
		g_viewerSettings.trans[0] = 0;
		g_viewerSettings.trans[1] = 0;
		g_viewerSettings.trans[2] = 0;
	}
	else
	{
		g_viewerSettings.trans[0] = 0;
		g_viewerSettings.trans[1] = min[2] + dz / 2.0f;
		g_viewerSettings.trans[2] = d * 1.0f;
	}

	g_viewerSettings.rot[0] = -90.0f;
	g_viewerSettings.rot[1] = -90.0f;
	g_viewerSettings.rot[2] = 0.0f;

	g_viewerSettings.movementScale = max( 1.0f, d * 0.01f );
	m_yaw = 90.0;
}

int SpriteModel :: setFrame( int newframe )
{
	if( !m_pspritehdr )
		return 0;

	if( newframe == -1 )
		return m_frame;

	if( newframe < g_GlWindow->getStartFrame( ))
		return m_frame;

	if( newframe > g_GlWindow->getEndFrame( ))
		return m_frame;

	m_frame = newframe;
	return newframe;
}

/*
================
R_GetSpriteFrame

assume pModel is valid
================
*/
mspriteframe_t *SpriteModel :: GetSpriteFrame( int frame )
{
	msprite_t		*psprite = m_pspritehdr;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe = NULL;
	float		*pintervals, fullinterval;
	int		i, numframes;
	float		targettime;

	if( !psprite ) return NULL;

	if( frame < 0 )
	{
		frame = 0;
	}
	else if( frame >= psprite->numframes )
	{
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == FRAME_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == FRAME_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = m_flTime - ((int)( m_flTime / fullinterval )) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == FRAME_ANGLED )
	{
		int	angleframe = (int)(Q_rint(( m_yaw - g_viewerSettings.rot[1] + 45.0f ) / 360 * 8) - 4) & 7;

		// e.g. doom-style sprite monsters
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}

	return pspriteframe;
}

/*
================
GetSpriteFrameInterpolant

NOTE: we using prevblending[0] and [1] for holds interval
between frames where are we lerping
================
*/
float SpriteModel :: GetSpriteFrameInterpolant( int frame, mspriteframe_t **oldframe, mspriteframe_t **curframe )
{
	msprite_t		*psprite = m_pspritehdr;
	mspritegroup_t	*pspritegroup;
	int		i, j, numframes;
	float		lerpFrac, jtime, jinterval;
	float		*pintervals, fullinterval, targettime;
	int		m_fDoInterp = true;

	if( !psprite ) return 0.0f;
	lerpFrac = 1.0f;

	if( frame < 0 )
	{
		frame = 0;
	}          
	else if( frame >= psprite->numframes )
	{
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == FRAME_SINGLE )
	{
		if( m_fDoInterp )
		{
			if( m_prevblending[0] >= psprite->numframes || psprite->frames[m_prevblending[0]].type != FRAME_SINGLE )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				m_prevblending[0] = m_prevblending[1] = frame;
				m_sequencetime = m_flTime;
				lerpFrac = 1.0f;
			}
                              
			if( m_sequencetime < m_flTime )
			{
				if( frame != m_prevblending[1] )
				{
					m_prevblending[0] = m_prevblending[1];
					m_prevblending[1] = frame;
					m_sequencetime = m_flTime;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (m_flTime - m_sequencetime) * 11.0f;
			}
			else
			{
				m_prevblending[0] = m_prevblending[1] = frame;
				m_sequencetime = m_flTime;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			m_prevblending[0] = m_prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		if( m_prevblending[0] >= psprite->numframes )
		{
			// reset interpolation on change model
			m_prevblending[0] = m_prevblending[1] = frame;
			m_sequencetime = m_flTime;
			lerpFrac = 0.0f;
		}

		// get the interpolated frames
		if( oldframe ) *oldframe = psprite->frames[m_prevblending[0]].frameptr;
		if( curframe ) *curframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == FRAME_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		jinterval = pintervals[1] - pintervals[0];
		jtime = 0.0f;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = m_flTime - ((int)( m_flTime / fullinterval)) * fullinterval;

		// LordHavoc: since I can't measure the time properly when it loops from numframes - 1 to 0,
		// i instead measure the time of the first frame, hoping it is consistent
		for( i = 0, j = numframes - 1; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
			j = i;
			jinterval = pintervals[i] - jtime;
			jtime = pintervals[i];
		}

		if( m_fDoInterp )
			lerpFrac = (targettime - jtime) / jinterval;
		else j = i; // no lerping

		// get the interpolated frames
		if( oldframe ) *oldframe = pspritegroup->frames[j];
		if( curframe ) *curframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == FRAME_ANGLED )
	{
		// e.g. doom-style sprite monsters
		int	angleframe = (int)(Q_rint(( g_viewerSettings.rot[1] - m_yaw + 45.0f ) / 360 * 8) - 4) & 7;

		if( m_fDoInterp )
		{
			if( m_prevblending[0] >= psprite->numframes || psprite->frames[m_prevblending[0]].type != FRAME_ANGLED )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				m_prevblending[0] = m_prevblending[1] = frame;
				m_sequencetime = m_flTime;
				lerpFrac = 1.0f;
			}

			if( m_sequencetime < m_flTime )
			{
				if( frame != m_prevblending[1] )
				{
					m_prevblending[0] = m_prevblending[1];
					m_prevblending[1] = frame;
					m_sequencetime = m_flTime;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (m_flTime - m_sequencetime) * 11.0f;
			}
			else
			{
				m_prevblending[0] = m_prevblending[1] = frame;
				m_sequencetime = m_flTime;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			m_prevblending[0] = m_prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		pspritegroup = (mspritegroup_t *)psprite->frames[m_prevblending[0]].frameptr;
		if( oldframe ) *oldframe = pspritegroup->frames[angleframe];

		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		if( curframe ) *curframe = pspritegroup->frames[angleframe];
	}

	return lerpFrac;
}

/*
=================
DrawSpriteQuad
=================
*/
void SpriteModel :: DrawQuad( mspriteframe_t *frame, const vec3_t &org, const vec3_t &v_right, const vec3_t &v_up, float scale )
{
	vec3_t	point;

	glBegin( GL_QUADS );
		glTexCoord2f( 0.0f, 1.0f );
		point = org + v_up * (frame->down * scale);
		point = point + v_right * (frame->left * scale);
		glVertex3fv( point );
		glTexCoord2f( 0.0f, 0.0f );
		point = org + v_up * (frame->up * scale);
		point = point + v_right * (frame->left * scale);
		glVertex3fv( point );
		glTexCoord2f( 1.0f, 0.0f );
		point = org + v_up * (frame->up * scale);
		point = point + v_right * (frame->right * scale);
		glVertex3fv( point );
 	        	glTexCoord2f( 1.0f, 1.0f );
		point = org + v_up * (frame->down * scale);
		point = point + v_right * (frame->right * scale);
		glVertex3fv( point );
	glEnd();
}

/*
=================
R_SpriteAllowLerping
=================
*/
bool SpriteModel :: AllowLerping( void )
{
	if( !m_pspritehdr )
		return false;

	if( m_pspritehdr->numframes <= 1 )
		return false;

	if( g_viewerSettings.renderMode != SPR_ADDITIVE )
		return false;

	return true;
}

void SpriteModel :: SetupTransform( void )
{
	vec3_t	angles( 0.0f, 0.0f, 0.0f );
	vec3_t	origin( 0.0f, 0.0f, 0.0f );

	m_protationmatrix = matrix4x4( origin, angles, 1.0f );

	angles[0] = 270.0f - g_viewerSettings.rot[0];
	angles[1] = 270.0f - g_viewerSettings.rot[1];
	origin = g_viewerSettings.trans;
	m_viewVectors = matrix4x4( origin, angles, 1.0f );
}

/*
================
StudioModel::DrawModel
inputs:
	currententity
	r_entorigin
================
*/
void SpriteModel :: DrawSprite( void )
{
	mspriteframe_t	*frame, *oldframe;
	float		angle, dot, sr, cr;
	float		lerp = 1.0f, ilerp;
	vec3_t		v_right, v_up;
	vec3_t		origin, color;
	float		blend = 1.0f;
	msprite_t		*psprite;
	int		i, type;

	if( !m_pspritehdr ) return;

	SetupTransform();

	psprite = (msprite_t * )m_pspritehdr;
	origin = g_vecZero; // set render origin

	// all sprites can have color
	color = Vector( 1.0f, 1.0f, 1.0f );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if( g_viewerSettings.renderMode == SPR_ADDITIVE )
		blend = 0.5f;

	if( g_viewerSettings.renderMode == SPR_ALPHTEST )
	{
		color[0] = g_viewerSettings.lColor[0];
		color[1] = g_viewerSettings.lColor[1];
		color[2] = g_viewerSettings.lColor[2];
	}

	if( AllowLerping( ))
		lerp = GetSpriteFrameInterpolant( m_frame, &oldframe, &frame );
	else frame = oldframe = GetSpriteFrame( m_frame );

	type = psprite->type;

	switch( g_viewerSettings.orientType )
	{
	case SPR_ORIENTED:
		v_right = m_protationmatrix[1];
		v_up = m_protationmatrix[2];
		break;
	case SPR_FACING_UPRIGHT:
	case SPR_FWD_PARALLEL_UPRIGHT:
		dot = m_viewVectors[0][2];
		if(( dot > 0.999848f ) || ( dot < -0.999848f ))	// cos(1 degree) = 0.999848
			return; // invisible
		v_up = Vector( 0.0f, 0.0f, 1.0f );
		v_right = Vector( -m_viewVectors[0][1], m_viewVectors[0][0], 0.0f ).Normalize();
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = 0.0f * (M_PI2 / 360.0f);
		SinCos( angle, &sr, &cr );
		for( i = 0; i < 3; i++ )
		{
			v_right[i] = (m_viewVectors[1][i] * cr + m_viewVectors[2][i] * sr);
			v_up[i] = m_viewVectors[1][i] * -sr + m_viewVectors[2][i] * cr;
		}
		break;
	case SPR_FWD_PARALLEL: // normal sprite
	default:
		v_right = m_viewVectors[1]; 
		v_up = m_viewVectors[2];
		break;
	}

	if( psprite->facecull == SPR_CULL_NONE )
		glCullFace( GL_NONE );
		
	if( oldframe == frame )
	{
		// draw the single non-lerped frame
		glColor4f( color[0], color[1], color[2], blend );
		glBindTexture( GL_TEXTURE_2D, frame->gl_texturenum );
		DrawQuad( frame, origin, v_right, v_up, 1.0f );
	}
	else
	{
		// draw two combined lerped frames
		lerp = bound( 0.0f, lerp, 1.0f );
		ilerp = 1.0f - lerp;

		if( ilerp != 0.0f )
		{
			glColor4f( color[0], color[1], color[2], blend * ilerp );
			glBindTexture( GL_TEXTURE_2D, oldframe->gl_texturenum );
			DrawQuad( oldframe, origin, v_right, v_up, 1.0f );
		}

		if( lerp != 0.0f )
		{
			glColor4f( color[0], color[1], color[2], blend * lerp );
			glBindTexture( GL_TEXTURE_2D, frame->gl_texturenum );
			DrawQuad( frame, origin, v_right, v_up, 1.0f );
		}
	}

	if( psprite->facecull == SPR_CULL_NONE )
		glCullFace( GL_FRONT );

	glDisable( GL_ALPHA_TEST );
	glDepthMask( GL_TRUE );
	glDisable( GL_BLEND );
	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	glEnable( GL_DEPTH_TEST );
}