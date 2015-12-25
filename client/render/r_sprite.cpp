/*
r_sprite.cpp - sprite model rendering
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
#include <mathlib.h>
#include "r_sprite.h"
#include "event_api.h"
#include "pm_defs.h"

CSpriteModelRenderer g_SpriteRenderer;

/*
====================
Init

====================
*/
void CSpriteModelRenderer::Init( void )
{
	// Set up some variables shared with engine
	m_pCvarLerping		= IEngineStudio.GetCvar( "r_sprite_lerping" );
	m_pCvarLighting		= IEngineStudio.GetCvar( "r_sprite_lighting" );
	m_pCvarFlareSize		= IEngineStudio.GetCvar( "r_flaresize" );
}

/*
====================
CSpriteModelRenderer

====================
*/
CSpriteModelRenderer :: CSpriteModelRenderer( void )
{
	m_fDoInterp	= 1;
	m_pCurrentEntity	= NULL;
	m_pCvarLerping	= NULL;
	m_pCvarLighting	= NULL;
	m_pCvarFlareSize	= NULL;
	m_pSpriteHeader	= NULL;
	m_pRenderModel	= NULL;
}

CSpriteModelRenderer :: ~CSpriteModelRenderer( void )
{
}

/*
================
GetSpriteFrame

================
*/
mspriteframe_t *CSpriteModelRenderer::GetSpriteFrame( int frame )
{
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe = NULL;

	if( frame < 0 )
	{
		frame = 0;
	}
	else if( frame >= m_pSpriteHeader->numframes )
	{
		ALERT( at_warning, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, m_pRenderModel->name );
		frame = m_pSpriteHeader->numframes - 1;
	}

	if( m_pSpriteHeader->frames[frame].type == SPR_SINGLE )
	{
		pspriteframe = (mspriteframe_t *)m_pSpriteHeader->frames[frame].frameptr;
	}
	else if( m_pSpriteHeader->frames[frame].type == SPR_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;

		float *pintervals = pspritegroup->intervals;
		int numframes = pspritegroup->numframes;
		float fullinterval = pintervals[numframes-1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		float targettime = m_clTime - ((int)(m_clTime / fullinterval)) * fullinterval;

		for( int i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( m_pSpriteHeader->frames[frame].type == FRAME_ANGLED && m_pCurrentEntity != NULL )
	{
		int	angleframe = (int)(Q_rint(( RI.refdef.viewangles[1] - m_pCurrentEntity->angles[YAW] + 45.0f ) / 360 * 8) - 4) & 7;

		// e.g. doom-style sprite monsters
		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;
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
float CSpriteModelRenderer::GetSpriteFrameInterpolant( int frame, mspriteframe_t **oldframe, mspriteframe_t **curframe )
{
	mspritegroup_t *pspritegroup;
	float lerpFrac = 1.0f;

	if( frame < 0 )
	{
		frame = 0;
	}          
	else if( frame >= m_pSpriteHeader->numframes )
	{
		ALERT( at_console, "GetSpriteFrameInterpolant: no such frame %d (%s)\n", frame, m_pRenderModel->name );
		frame = m_pSpriteHeader->numframes - 1;
	}

	if( m_pSpriteHeader->frames[frame].type == FRAME_SINGLE )
	{
		if( m_fDoInterp )
		{
			if( m_pCurrentEntity->latched.prevblending[0] >= m_pSpriteHeader->numframes ||
			m_pSpriteHeader->frames[m_pCurrentEntity->latched.prevblending[0]].type != FRAME_SINGLE )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
				m_pCurrentEntity->latched.prevanimtime = m_clTime;
				lerpFrac = 1.0f;
			}
                              
			if( m_pCurrentEntity->latched.prevanimtime < m_clTime )
			{
				if( frame != m_pCurrentEntity->latched.prevblending[1] )
				{
					m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1];
					m_pCurrentEntity->latched.prevblending[1] = frame;
					m_pCurrentEntity->latched.prevanimtime = m_clTime;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (m_clTime - m_pCurrentEntity->latched.prevanimtime) * 10; // SetNextThink( 0.1 )
			}
			else
			{
				m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
				m_pCurrentEntity->latched.prevanimtime = m_clTime;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		if( m_pCurrentEntity->latched.prevblending[0] >= m_pSpriteHeader->numframes )
		{
			// reset interpolation on change model
			m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
			m_pCurrentEntity->latched.prevanimtime = m_clTime;
			lerpFrac = 0.0f;
		}

		// get the interpolated frames
		if( oldframe ) *oldframe = m_pSpriteHeader->frames[m_pCurrentEntity->latched.prevblending[0]].frameptr;
		if( curframe ) *curframe = m_pSpriteHeader->frames[frame].frameptr;
	}
	else if( m_pSpriteHeader->frames[frame].type == FRAME_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;
		float *pintervals = pspritegroup->intervals;
		int numframes = pspritegroup->numframes;
		float fullinterval = pintervals[numframes-1];
		float jinterval = pintervals[1] - pintervals[0];
		float jtime = 0.0f;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		float targettime = m_clTime - ((int)(m_clTime / fullinterval)) * fullinterval;

		int i, j;

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
	else if( m_pSpriteHeader->frames[frame].type == FRAME_ANGLED )
	{
		// e.g. doom-style sprite monsters
		float	yaw = m_pCurrentEntity->angles[YAW];
		int	angleframe = (int)(Q_rint(( RI.refdef.viewangles[1] - yaw + 45.0f ) / 360 * 8) - 4) & 7;

		if( m_fDoInterp )
		{
			if( m_pCurrentEntity->latched.prevblending[0] >= m_pSpriteHeader->numframes ||
			m_pSpriteHeader->frames[m_pCurrentEntity->latched.prevblending[0]].type != FRAME_ANGLED )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
				m_pCurrentEntity->latched.prevanimtime = m_clTime;
				lerpFrac = 1.0f;
			}

			if( m_pCurrentEntity->latched.prevanimtime < m_clTime )
			{
				if( frame != m_pCurrentEntity->latched.prevblending[1] )
				{
					m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1];
					m_pCurrentEntity->latched.prevblending[1] = frame;
					m_pCurrentEntity->latched.prevanimtime = m_clTime;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (m_clTime - m_pCurrentEntity->latched.prevanimtime) * 10; // SetNextThink( 0.1 )
			}
			else
			{
				m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
				m_pCurrentEntity->latched.prevanimtime = m_clTime;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->latched.prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[m_pCurrentEntity->latched.prevblending[0]].frameptr;
		if( oldframe ) *oldframe = pspritegroup->frames[angleframe];

		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;
		if( curframe ) *curframe = pspritegroup->frames[angleframe];
	}

	return lerpFrac;
}

/*
================
SspriteComputeBBox

Compute a full bounding box for reference
================
*/
int CSpriteModelRenderer::SpriteComputeBBox( cl_entity_t *e, Vector bbox[8] )
{
	// copy original bbox (no rotation for sprites)
	sprite_mins = e->model->mins;
	sprite_maxs = e->model->maxs;

	// compute a full bounding box
	for( int i = 0; bbox != NULL && i < 8; i++ )
	{
  		bbox[i][0] = ( i & 1 ) ? sprite_mins[0] : sprite_maxs[0];
  		bbox[i][1] = ( i & 2 ) ? sprite_mins[1] : sprite_maxs[1];
  		bbox[i][2] = ( i & 4 ) ? sprite_mins[2] : sprite_maxs[2];
	}

	float scale = 1.0f;

	if( e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	sprite_radius = RadiusFromBounds( sprite_mins, sprite_maxs ) * scale;

	return true;
}

/*
================
CullSpriteModel

Cull sprite model by bbox
================
*/
int CSpriteModelRenderer::CullSpriteModel( const Vector &origin )
{
	if( !m_pSpriteHeader )
		return true;

	if( !SpriteComputeBBox( m_pCurrentEntity, NULL ))
		return true; // invalid frame

	return R_CullModel( m_pCurrentEntity, origin, sprite_mins, sprite_maxs, sprite_radius );
}

/*
================
GlowSightDistance

Calc sight distance for glow-sprites
================
*/
float CSpriteModelRenderer::GlowSightDistance( const Vector &glowOrigin )
{
	pmtrace_t	tr;

	float dist = (glowOrigin - RI.vieworg).Length();

	if( !( RI.params & RP_MIRRORVIEW ))
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( RI.vieworg, (Vector)glowOrigin, PM_GLASS_IGNORE|PM_STUDIO_IGNORE, -1, &tr );

		if(( 1.0f - tr.fraction ) * dist > 8 )
			return -1;
	}

	return dist;
}

/*
================
R_GlowSightDistance

Set sprite brightness factor
================
*/
float CSpriteModelRenderer::SpriteGlowBlend( const Vector &origin, int rendermode, int renderfx, int alpha, float &scale )
{
	float dist = GlowSightDistance( origin );
	float brightness;

	if( dist <= 0 ) return 0.0f; // occluded

	if( renderfx == kRenderFxNoDissipation )
		return (float)alpha * (1.0f / 255.0f);

	scale = 0.0f; // variable sized glow

	// UNDONE: Tweak these magic numbers (19000 - falloff & 200 - sprite size)
	brightness = 19000.0 / ( dist * dist );
	brightness = bound( 0.01f, brightness, 1.0f );

	if( rendermode != kRenderWorldGlow )
	{
		// make the glow fixed size in screen space, taking into consideration the scale setting.
		if( scale == 0.0f ) scale = 1.0f;
		scale *= dist * ( 1.0f / bound( 100.0f, m_pCvarFlareSize->value, 300.0f ));
	}

	return brightness;
}

/*
================
SpriteOccluded

Do occlusion test for glow-sprites
================
*/
int CSpriteModelRenderer::SpriteOccluded( const Vector &origin, int &alpha, float &pscale )
{
	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow || m_pCurrentEntity->curstate.rendermode == kRenderWorldGlow )
	{
		// don't reflect this entity in mirrors
		if( m_pCurrentEntity->curstate.effects & EF_NOREFLECT && RI.params & RP_MIRRORVIEW )
			return true;

		// draw only in mirrors
		if( m_pCurrentEntity->curstate.effects & EF_REFLECTONLY && !( RI.params & RP_MIRRORVIEW ))
			return true;

		Vector v;

		WorldToScreen( origin, v );

		if( v[0] < RI.refdef.viewport[0] || v[0] > RI.refdef.viewport[0] + RI.refdef.viewport[2] )
			return true; // do scissor
		if( v[1] < RI.refdef.viewport[1] || v[1] > RI.refdef.viewport[1] + RI.refdef.viewport[3] )
			return true; // do scissor

		float blend = 1.0f;
		blend *= SpriteGlowBlend( origin, m_pCurrentEntity->curstate.rendermode, m_pCurrentEntity->curstate.renderfx, alpha, pscale );
		alpha *= blend;

		if( blend <= 0.01f )
			return true; // faded
	}
	else
	{
		if( CullSpriteModel( origin ))
			return true;
	}
	return false;	
}

/*
=================
DrawSpriteQuad

=================
*/
void CSpriteModelRenderer::DrawSpriteQuad( mspriteframe_t *frame, const Vector &org, const Vector &right, const Vector &up, float scale )
{
	Vector point;

	r_stats.c_sprite_polys++;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		point = org + up * (frame->down * scale);
		point = point + right * (frame->left * scale);
		pglVertex3fv( point );
		pglTexCoord2f( 0.0f, 0.0f );
		point = org + up * (frame->up * scale);
		point = point + right * (frame->left * scale);
		pglVertex3fv( point );
		pglTexCoord2f( 1.0f, 0.0f );
		point = org + up * (frame->up * scale);
		point = point + right * (frame->right * scale);
		pglVertex3fv( point );
 	        	pglTexCoord2f( 1.0f, 1.0f );
		point = org + up * (frame->down * scale);
		point = point + right * (frame->right * scale);
		pglVertex3fv( point );
	pglEnd();
}

int CSpriteModelRenderer::SpriteHasLightmap( int texFormat )
{
	if( m_pCvarLighting->value == 0 )
		return false;
	
	if( texFormat != SPR_ALPHTEST )
		return false;

	if( m_pCurrentEntity->curstate.effects & EF_FULLBRIGHT )
		return false;

	if( m_pCurrentEntity->curstate.renderamt <= 127 )
		return false;

	switch( m_pCurrentEntity->curstate.rendermode )
	{
	case kRenderNormal:
	case kRenderTransAlpha:
	case kRenderTransTexture:
		break;
	default:
		return false;
	}

	return true;
}

/*
=================
R_DrawSpriteModel
=================
*/
void CSpriteModelRenderer::SpriteDrawModel( void )
{
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );

	m_pRenderModel = m_pCurrentEntity->model;
	m_pSpriteHeader = (msprite_t *)Mod_Extradata( m_pRenderModel );

	Vector origin = m_pCurrentEntity->origin; // set render origin

	// link sprite with parent (if present)
	if( m_pCurrentEntity->curstate.aiment > 0 && m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t *parent = GET_ENTITY( m_pCurrentEntity->curstate.aiment );

		if( parent && parent->model )
		{
			if( parent->model->type == mod_studio && m_pCurrentEntity->curstate.body > 0 )
			{
				int num = bound( 1, m_pCurrentEntity->curstate.body, MAXSTUDIOATTACHMENTS );
				origin = parent->attachment[num-1];
			}
			else
			{
				origin = parent->origin;
			}
		}
	}

	int alpha = m_pCurrentEntity->curstate.renderamt;
	float scale = m_pCurrentEntity->curstate.scale;

	if( SpriteOccluded( origin, alpha, scale ))
		return; // sprite culled

	r_stats.c_sprite_models_drawn++;
	r_stats.num_drawed_ents++;

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST && m_pCurrentEntity->curstate.rendermode != kRenderTransAdd )
	{
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GREATER, 0.0f );
	}

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow || m_pCurrentEntity->curstate.rendermode == kRenderWorldGlow )
		pglDisable( GL_DEPTH_TEST );

	qboolean fog = false;

	// select properly rendermode
	switch( m_pCurrentEntity->curstate.rendermode )
	{
	case kRenderTransAlpha:
	case kRenderTransColor:
	case kRenderTransTexture:
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		break;
	case kRenderGlow:
	case kRenderTransAdd:
	case kRenderWorldGlow:
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		break;
	case kRenderNormal:
	default:
		if( m_pSpriteHeader->texFormat == SPR_INDEXALPHA )
		{
			pglEnable( GL_BLEND );
			pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		}
		else 
		{
			fog = true;
			pglDisable( GL_BLEND );
		}
		break;
	}

	Vector color, color2;

	// all sprites can have color
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	// add basecolor (any rendermode can have colored sprite)
	color[0] = (float)m_pCurrentEntity->curstate.rendercolor.r * ( 1.0f / 255.0f );
	color[1] = (float)m_pCurrentEntity->curstate.rendercolor.g * ( 1.0f / 255.0f );
	color[2] = (float)m_pCurrentEntity->curstate.rendercolor.b * ( 1.0f / 255.0f );
          
	if( SpriteHasLightmap( m_pSpriteHeader->texFormat ))
	{
		color24 lightColor;
		bool invLight;

		invLight = (m_pCurrentEntity->curstate.effects & EF_INVLIGHT) ? true : false;
		R_LightForPoint( origin, &lightColor, invLight, true, sprite_radius );

		color2[0] = (float)lightColor.r * ( 1.0f / 255.0f );
		color2[1] = (float)lightColor.g * ( 1.0f / 255.0f );
		color2[2] = (float)lightColor.b * ( 1.0f / 255.0f );

		if( glState.drawTrans )
			pglDepthMask( GL_TRUE );

		// NOTE: sprites with 'lightmap' looks ugly when alpha func is GL_GREATER 0.0
		pglAlphaFunc( GL_GEQUAL, 0.5f );
	}

	float lerp = 1.0f, ilerp;
	mspriteframe_t *frame, *oldframe;

	if( m_pCurrentEntity->curstate.rendermode == kRenderNormal || m_pCurrentEntity->curstate.rendermode == kRenderTransAlpha )
		frame = oldframe = GetSpriteFrame( m_pCurrentEntity->curstate.frame );
	else lerp = GetSpriteFrameInterpolant( m_pCurrentEntity->curstate.frame, &oldframe, &frame );

	int type = m_pSpriteHeader->type;

	// automatically roll parallel sprites if requested
	if( m_pCurrentEntity->angles[ROLL] != 0.0f && type == SPR_FWD_PARALLEL )
		type = SPR_FWD_PARALLEL_ORIENTED;

	Vector v_forward, v_right, v_up;

	switch( type )
	{
	case SPR_ORIENTED:
		{
			AngleVectors( m_pCurrentEntity->angles, v_forward, v_right, v_up );
			origin = origin - v_forward * 0.01f; // to avoid z-fighting
		}
		break;
	case SPR_FACING_UPRIGHT:
		{
			v_right.x = origin.y - RI.vieworg.y;
			v_right.y = -(origin.x - RI.vieworg.x);
			v_right.z = 0.0f;
			v_up.x = v_up.y = 0.0f;
			v_up.z = 1.0f;
			v_right = v_right.Normalize();
		}
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		{
			float dot = RI.vforward.z;
			if(( dot > 0.999848f ) || ( dot < -0.999848f ))	// cos(1 degree) = 0.999848
				return; // invisible

			v_right.x = RI.vforward.y;
			v_right.y = -RI.vforward.x;
			v_right.z = 0.0f;
			v_up.x = v_up.y = 0.0f;
			v_up.z = 1.0f;
			v_right = v_right.Normalize();
		}
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		{
			float angle = m_pCurrentEntity->angles[ROLL] * (M_PI * 2.0f / 360.0f);
			float sr, cr;

			SinCos( angle, &sr, &cr );

			for( int i = 0; i < 3; i++ )
			{
				v_right[i] = (RI.vright[i] * cr + RI.vup[i] * sr);
				v_up[i] = RI.vright[i] * -sr + RI.vup[i] * cr;
			}
		}
		break;
	case SPR_FWD_PARALLEL: // normal sprite
	default:
		{
			v_right = RI.vright; 
			v_up = RI.vup;
		}
		break;
	}

	float flAlpha = (float)alpha * ( 1.0f / 255.0f );

	if( m_pSpriteHeader->facecull == SPR_CULL_NONE )
		GL_Cull( GL_NONE );
		
	if( oldframe == frame )
	{
		// draw the single non-lerped frame
		pglColor4f( color[0], color[1], color[2], flAlpha );
		GL_Bind( GL_TEXTURE0, frame->gl_texturenum );
		DrawSpriteQuad( frame, origin, v_right, v_up, scale );
	}
	else
	{
		// draw two combined lerped frames
		lerp = bound( 0.0f, lerp, 1.0f );
		ilerp = 1.0f - lerp;

		if( ilerp != 0 )
		{
			pglColor4f( color[0], color[1], color[2], flAlpha * ilerp );
			GL_Bind( GL_TEXTURE0, oldframe->gl_texturenum );
			DrawSpriteQuad( oldframe, origin, v_right, v_up, scale );
		}

		if( lerp != 0 )
		{
			pglColor4f( color[0], color[1], color[2], flAlpha * lerp );
			GL_Bind( GL_TEXTURE0, frame->gl_texturenum );
			DrawSpriteQuad( frame, origin, v_right, v_up, scale );
		}
	}

	// draw the sprite 'lightmap' :-)
	if( SpriteHasLightmap( m_pSpriteHeader->texFormat ))
	{
		pglEnable( GL_BLEND );
		pglDepthFunc( GL_EQUAL );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( R_OVERBRIGHT_SFACTOR(), GL_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

		pglColor4f( color2[0], color2[1], color2[2], flAlpha );
		GL_Bind( GL_TEXTURE0, tr.whiteTexture );
		DrawSpriteQuad( frame, origin, v_right, v_up, scale );

		if( glState.drawTrans ) 
			pglDepthMask( GL_FALSE );
	}

	if( fog && R_SetupFogProjection( ))
	{
		DrawSpriteQuad( frame, origin, v_right, v_up, scale );
		GL_CleanUpTextureUnits( 0 );
	}

	if( m_pSpriteHeader->facecull == SPR_CULL_NONE )
		GL_Cull( GL_FRONT );

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow || m_pCurrentEntity->curstate.rendermode == kRenderWorldGlow )
		pglEnable( GL_DEPTH_TEST );

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST && m_pCurrentEntity->curstate.rendermode != kRenderTransAdd )
		pglDisable( GL_ALPHA_TEST );

	pglDisable( GL_BLEND );
	pglDepthFunc( GL_LEQUAL );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	pglColor4ub( 255, 255, 255, 255 );
}

/*
=================
R_DrawSpriteModelShadowPass
=================
*/
void CSpriteModelRenderer::SpriteDrawModelShadowPass( void )
{
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );

	m_pRenderModel = m_pCurrentEntity->model;
	m_pSpriteHeader = (msprite_t *)Mod_Extradata( m_pRenderModel );

	Vector origin = m_pCurrentEntity->origin; // set render origin

	// link sprite with parent (if present)
	if( m_pCurrentEntity->curstate.aiment > 0 && m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t *parent = GET_ENTITY( m_pCurrentEntity->curstate.aiment );

		if( parent && parent->model )
		{
			if( parent->model->type == mod_studio && m_pCurrentEntity->curstate.body > 0 )
			{
				int num = bound( 1, m_pCurrentEntity->curstate.body, MAXSTUDIOATTACHMENTS );
				origin = parent->attachment[num-1];
			}
			else
			{
				origin = parent->origin;
			}
		}
	}

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST )
	{
		pglAlphaFunc( GL_GREATER, 0.0f );
		pglEnable( GL_ALPHA_TEST );
	}

	mspriteframe_t *frame = GetSpriteFrame( m_pCurrentEntity->curstate.frame );

	int type = m_pSpriteHeader->type;
	float scale = m_pCurrentEntity->curstate.scale;

	// automatically roll parallel sprites if requested
	if( m_pCurrentEntity->angles[ROLL] != 0.0f && type == SPR_FWD_PARALLEL )
		type = SPR_FWD_PARALLEL_ORIENTED;

	Vector v_forward, v_right, v_up;
	Vector g_forward, g_right, g_up, g_vieworg;
	float cr, sr, dot, angle;

	// g-cont. this is a very stupid case but...
	matrix3x3	vectors( r_lastRefdef.viewangles - RI.refdef.viewangles );
	g_vieworg = RI.vieworg;
	g_forward = vectors[0];
	g_right = vectors[1];
	g_up = vectors[2];

	switch( type )
	{
	case SPR_ORIENTED:
		AngleVectors( m_pCurrentEntity->angles, v_forward, v_right, v_up );
		origin = origin - v_forward * 0.01f; // to avoid z-fighting
		break;
	case SPR_FACING_UPRIGHT:
		v_right.x = origin.y - g_vieworg.y;
		v_right.y = -(origin.x - g_vieworg.x);
		v_right.z = 0.0f;
		v_up.x = v_up.y = 0.0f;
		v_up.z = 1.0f;
		v_right = v_right.Normalize();
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		dot = g_forward.z;
		if(( dot > 0.999848f ) || ( dot < -0.999848f ))	// cos(1 degree) = 0.999848
			return; // invisible

		v_right.x = g_forward.y;
		v_right.y = -g_forward.x;
		v_right.z = 0.0f;
		v_up.x = v_up.y = 0.0f;
		v_up.z = 1.0f;
		v_right = v_right.Normalize();
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = m_pCurrentEntity->angles[ROLL] * (M_PI * 2.0f / 360.0f);
		SinCos( angle, &sr, &cr );

		v_right = (g_right * cr + g_up * sr);
		v_up = g_right * -sr + g_up * cr;
		break;
	case SPR_FWD_PARALLEL: // normal sprite
	default:
		v_right = g_right; 
		v_up = g_up;
		break;
	}

	GL_Cull( GL_NONE );
	pglEnable( GL_TEXTURE_2D );

	GL_Bind( GL_TEXTURE0, frame->gl_texturenum );
	DrawSpriteQuad( frame, origin, v_right, v_up, scale );

	pglDisable( GL_TEXTURE_2D );
	GL_Cull( GL_FRONT );

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST )
		pglDisable( GL_ALPHA_TEST );
}

void CSpriteModelRenderer::DrawSpriteModelInternal( cl_entity_t *e )
{
	if( RI.params & RP_ENVVIEW )
		return;

	if( !Mod_Extradata( e->model ))
		return;

	if( m_pCvarLerping->value )
		m_fDoInterp = (e->curstate.effects & EF_NOINTERP) ? false : true;
	else m_fDoInterp = false;

	if(!( RI.params & RP_SHADOWVIEW ))
		SpriteDrawModel();
	else SpriteDrawModelShadowPass();
}

mspriteframe_t *CSpriteModelRenderer::GetSpriteFrame( const model_t *m_pSpriteModel, int frame )
{
	if(( m_pSpriteHeader = (msprite_t *)Mod_Extradata( (model_t *)m_pSpriteModel )) == NULL )
		return NULL;

	m_pCurrentEntity = NULL;
	return GetSpriteFrame( frame );
}
