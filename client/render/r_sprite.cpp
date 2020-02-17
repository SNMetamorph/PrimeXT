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
#include "triangleapi.h"
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
	m_pCvarTraceGlow		= IEngineStudio.GetCvar( "r_traceglow" );
}

/*
====================
CSpriteModelRenderer

====================
*/
CSpriteModelRenderer :: CSpriteModelRenderer( void )
{
	m_fDoInterp	= true;
	m_pCurrentEntity	= NULL;
	m_pCvarLerping	= NULL;
	m_pCvarLighting	= NULL;
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
mspriteframe_t *CSpriteModelRenderer :: GetSpriteFrame( int frame, float yaw )
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
		int i;
		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;

		float *pintervals = pspritegroup->intervals;
		int numframes = pspritegroup->numframes;
		float fullinterval = pintervals[numframes-1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		float targettime = tr.time - ((int)( tr.time / fullinterval )) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( m_pSpriteHeader->frames[frame].type == FRAME_ANGLED )
	{
		int angleframe = (int)(Q_rint(( RI->viewangles[YAW] - yaw + 45.0f ) / 360 * 8) - 4) & 7;

		// doom-style sprite monsters
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
float CSpriteModelRenderer :: GetSpriteFrameInterpolant( mspriteframe_t **oldframe, mspriteframe_t **curframe )
{
	float	framerate = 10.0f;
	float	lerpFrac = 0.0f, frame;
	float	frametime = (1.0f / framerate); // 10 fps as default
	int	i, j, iframe, oldf, newf;

	// misc info
	m_fDoInterp = FBitSet( m_pCurrentEntity->curstate.effects, EF_NOINTERP ) ? false : true;

	if( !m_fDoInterp || m_pCurrentEntity->curstate.framerate < 0.0f )
	{
		// interpolation disabled for some reasons
		*oldframe = *curframe = GetSpriteFrame( m_pCurrentEntity->curstate.frame, m_pCurrentEntity->angles[YAW] );
		return lerpFrac;
	}

	frame = Q_max( 0.0f, m_pCurrentEntity->curstate.frame );
	iframe = (int)m_pCurrentEntity->curstate.frame;

	if( m_pCurrentEntity->curstate.framerate > 0.0f )
	{
		frametime = (1.0f / m_pCurrentEntity->curstate.framerate);
		framerate = m_pCurrentEntity->curstate.framerate;
	}

	if( iframe < 0 )
	{
		iframe = 0;
	}
	else if( iframe >= m_pSpriteHeader->numframes )
	{
		ALERT( at_warning, "R_GetSpriteFrameInterpolant: no such frame %d (%s)\n", frame, m_pRenderModel->name );
		iframe = m_pSpriteHeader->numframes - 1;
	}

	// calc interpolant range
	oldf = (int)Q_floor( frame - 0.5 );
	newf = (int)Q_ceil( frame - 0.5 );

	// FIXME: allow interp between first and last frame
	oldf = oldf % ( m_pSpriteHeader->numframes - 1 );
	newf = newf % ( m_pSpriteHeader->numframes + 1 );

	// NOTE: we allow interpolation between single and angled frames e.g. for Doom monsters
	if( m_pSpriteHeader->frames[iframe].type == FRAME_SINGLE || m_pSpriteHeader->frames[iframe].type == FRAME_ANGLED )
	{
		// frame was changed
		if( newf != m_pCurrentEntity->latched.prevframe )
		{
			m_pCurrentEntity->latched.prevanimtime = tr.time + frametime;
			m_pCurrentEntity->latched.prevframe = newf;
			lerpFrac = 0.0f; // reset lerp
		}
                              
		if( m_pCurrentEntity->latched.prevanimtime != 0.0f && m_pCurrentEntity->latched.prevanimtime >= tr.time )
			lerpFrac = (m_pCurrentEntity->latched.prevanimtime - tr.time) * framerate;

		// compute lerp factor
		lerpFrac = (int)(10000 * lerpFrac) / 10000.0f;
		lerpFrac = bound( 0.0f, 1.0f - lerpFrac, 1.0f );

		// get the interpolated frames
		if( oldframe ) *oldframe = GetSpriteFrame( oldf, m_pCurrentEntity->angles[YAW] );
		if( curframe ) *curframe = GetSpriteFrame( newf, m_pCurrentEntity->angles[YAW] );
	}
	else if( m_pSpriteHeader->frames[iframe].type == FRAME_GROUP ) 
	{
		mspritegroup_t	*pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[iframe].frameptr;
		float		*pintervals = pspritegroup->intervals;
		float		fullinterval, targettime, jinterval;
		float		jtime = 0.0f;

		fullinterval = pintervals[pspritegroup->numframes-1];
		jinterval = pintervals[1] - pintervals[0];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = tr.time - ((int)( tr.time / fullinterval )) * fullinterval;

		// LordHavoc: since I can't measure the time properly when it loops from numframes - 1 to 0,
		// i instead measure the time of the first frame, hoping it is consistent
		for( i = 0, j = (pspritegroup->numframes - 1); i < (pspritegroup->numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
			j = i;
			jinterval = pintervals[i] - jtime;
			jtime = pintervals[i];
		}

		lerpFrac = (targettime - jtime) / jinterval;

		// get the interpolated frames
		if( oldframe ) *oldframe = pspritegroup->frames[j];
		if( curframe ) *curframe = pspritegroup->frames[i];
	}

	return lerpFrac;
}

/*
================
CSpriteModelRenderer

Compute renderer origin (include parent movement, sky movement, etc)
================
*/
void CSpriteModelRenderer :: SpriteComputeOrigin( cl_entity_t *e )
{
	sprite_origin = e->origin; // set render origin

	// link sprite with parent (if present)
	if( e->curstate.aiment > 0 && e->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t *parent = GET_ENTITY( e->curstate.aiment );

		if( parent && parent->model )
		{
			if( parent->model->type == mod_studio && e->curstate.body > 0 )
			{
				int num = bound( 1, e->curstate.body, MAXSTUDIOATTACHMENTS );
				sprite_origin = R_StudioAttachmentOrigin( parent, num - 1 );
			}
			else
			{
				sprite_origin = parent->origin;
			}
		}
	}

	tr.modelorg = sprite_origin;
}

/*
================
SspriteComputeBBox

Compute a full bounding box for reference
================
*/
void CSpriteModelRenderer :: SpriteComputeBBox( cl_entity_t *e, Vector bbox[8] )
{
	// always compute origin first
	if( bbox != NULL ) SpriteComputeOrigin( e );

	// copy original bbox (no rotation for sprites)
	sprite_absmin = e->model->mins;
	sprite_absmax = e->model->maxs;

	float scale = 1.0f;

	if( e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	sprite_absmin *= scale;
	sprite_absmax *= scale;

	sprite_absmin += sprite_origin;
	sprite_absmax += sprite_origin;

	// compute a full bounding box
	for( int i = 0; bbox != NULL && i < 8; i++ )
	{
  		bbox[i][0] = ( i & 1 ) ? sprite_absmin[0] : sprite_absmax[0];
  		bbox[i][1] = ( i & 2 ) ? sprite_absmin[1] : sprite_absmax[1];
  		bbox[i][2] = ( i & 4 ) ? sprite_absmin[2] : sprite_absmax[2];
	}
}

/*
================
CullSpriteModel

Cull sprite model by bbox
================
*/
bool CSpriteModelRenderer :: CullSpriteModel( void )
{
	if( !m_pSpriteHeader )
		return true;

	SpriteComputeBBox( m_pCurrentEntity, NULL );

	if( !Mod_CheckBoxVisible( sprite_absmin, sprite_absmax ))
		return true;

	return R_CullModel( m_pCurrentEntity, sprite_absmin, sprite_absmax );
}

/*
================
GlowSightDistance

Calc sight distance for glow-sprites
================
*/
float CSpriteModelRenderer :: GlowSightDistance( void )
{
	pmtrace_t	tr;

	float dist = (sprite_origin - RI->vieworg).Length();

	if( !FBitSet( RI->params, RP_MIRRORVIEW ))
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( RI->vieworg, sprite_origin, PM_GLASS_IGNORE, -1, &tr );

		if(( 1.0f - tr.fraction ) * dist > 8.0f )
			return 0.0f;
	}

	return dist;
}

/*
================
R_SpriteGlowBlend

Set sprite brightness factor
================
*/
float CSpriteModelRenderer :: SpriteGlowBlend( int rendermode, int renderfx, float &scale )
{
	float dist = GlowSightDistance();
	float brightness;

	if( dist <= 0 ) return 0.0f; // occluded

	if( renderfx == kRenderFxNoDissipation )
		return 1.0f;

	scale = 0.0f; // variable sized glow

	brightness = 19000.0 / ( dist * dist );
	brightness = bound( 0.05f, brightness, 1.0f );

	// make the glow fixed size in screen space, taking into consideration the scale setting.
	if( scale == 0.0f ) scale = 1.0f;
	scale *= dist * ( 1.0f / 200.0f );

	return brightness;
}

/*
================
SpriteOccluded

Do occlusion test for glow-sprites
================
*/
int CSpriteModelRenderer :: SpriteOccluded( float &pscale )
{
	// always compute origin first
	SpriteComputeOrigin( m_pCurrentEntity );

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow )
	{
		Vector	screenVec;
		float	blend;

		// don't reflect this entity in mirrors
		if( FBitSet( m_pCurrentEntity->curstate.effects, EF_NOREFLECT ) && FBitSet( RI->params, RP_MIRRORVIEW ))
			return true;

		// draw only in mirrors
		if( FBitSet( m_pCurrentEntity->curstate.effects, EF_REFLECTONLY ) && !FBitSet( RI->params, RP_MIRRORVIEW ))
			return true;

		WorldToScreen( sprite_origin, screenVec );

		if( screenVec[0] < RI->viewport[0] || screenVec[0] > RI->viewport[0] + RI->viewport[2] )
			return true; // out of screen

		if( screenVec[1] < RI->viewport[1] || screenVec[1] > RI->viewport[1] + RI->viewport[3] )
			return true; // out of screen

		blend = SpriteGlowBlend( m_pCurrentEntity->curstate.rendermode, m_pCurrentEntity->curstate.renderfx, pscale );
		tr.blend *= blend;

		if( blend <= 0.01f )
			return true; // faded
	}
	else
	{
		if( CullSpriteModel( ))
			return true;
	}

	return false;	
}

/*
=================
DrawSpriteQuad

=================
*/
void CSpriteModelRenderer :: DrawSpriteQuad( mspriteframe_t *frame, const Vector &org, const Vector &right, const Vector &up, float scale )
{
	Vector point;

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

	r_stats.c_total_tris += 2;
	r_stats.c_sprite_polys++;
}

void CSpriteModelRenderer :: DrawLighting( mspriteframe_t *frame, const Vector &org, const Vector &right, const Vector &up, const Vector &light, float scale, float flAlpha )
{
	Vector point, color = light, color2;
	float lightscale = scale * 0.5f;

	color += R_LightsForPoint( org, lightscale ) * 0.5f;

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		point = org + up * (frame->down * scale);
		point = point + right * (frame->left * scale);
		color2 = color + R_LightsForPoint( point, lightscale ) * 0.5f;
		pglColor4f( color2[0], color2[1], color2[2], flAlpha );
		pglVertex3fv( point );

		pglTexCoord2f( 0.0f, 0.0f );
		point = org + up * (frame->up * scale);
		point = point + right * (frame->left * scale);
		color2 = color + R_LightsForPoint( point, lightscale ) * 0.5f;
		pglColor4f( color2[0], color2[1], color2[2], flAlpha );
		pglVertex3fv( point );

		pglTexCoord2f( 1.0f, 0.0f );
		point = org + up * (frame->up * scale);
		point = point + right * (frame->right * scale);
		color2 = color + R_LightsForPoint( point, lightscale ) * 0.5f;
		pglColor4f( color2[0], color2[1], color2[2], flAlpha );
		pglVertex3fv( point );

 	        	pglTexCoord2f( 1.0f, 1.0f );
		point = org + up * (frame->down * scale);
		point = point + right * (frame->right * scale);
		color2 = color + R_LightsForPoint( point, lightscale ) * 0.5f;
		pglColor4f( color2[0], color2[1], color2[2], flAlpha );
		pglVertex3fv( point );
	pglEnd();

	r_stats.c_total_tris += 2;
}
	
int CSpriteModelRenderer :: SpriteHasLightmap( int texFormat )
{
	if( !CVAR_TO_BOOL( m_pCvarLighting ))
		return false;
	
	if( texFormat != SPR_ALPHTEST )
		return false;

	if( FBitSet( m_pCurrentEntity->curstate.effects, EF_FULLBRIGHT ))
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
R_SpriteAllowLerping
=================
*/
int CSpriteModelRenderer :: SpriteAllowLerping( cl_entity_t *e, msprite_t *psprite )
{
	if( !CVAR_TO_BOOL( m_pCvarLerping ))
		return false;

	if( psprite->numframes <= 1 )
		return false;

	if( psprite->texFormat != SPR_ADDITIVE )
		return false;

	if( e->curstate.rendermode == kRenderNormal || e->curstate.rendermode == kRenderTransAlpha )
		return false;

	return true;
}


/*
=================
R_DrawSpriteModel
=================
*/
void CSpriteModelRenderer :: SpriteDrawModel( void )
{
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	m_pRenderModel = m_pCurrentEntity->model;
	m_pSpriteHeader = (msprite_t *)Mod_Extradata( m_pRenderModel );

	float scale = m_pCurrentEntity->curstate.scale;
	if( !scale ) scale = 1.0f;

	if( SpriteOccluded( scale ))
		return; // sprite culled

	r_stats.c_sprite_models_drawn++;

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST && m_pCurrentEntity->curstate.rendermode != kRenderTransAdd )
	{
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GREATER, 0.0f );
	}

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow )
		pglDisable( GL_DEPTH_TEST );

	// select properly rendermode
	switch( m_pCurrentEntity->curstate.rendermode )
	{
	case kRenderTransAlpha:
		pglDepthMask( GL_FALSE );
	case kRenderTransColor:
	case kRenderTransTexture:
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		break;
	case kRenderGlow:
		pglDisable( GL_DEPTH_TEST );
	case kRenderTransAdd:
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		pglDepthMask( GL_FALSE );
		break;
	case kRenderNormal:
	default:
		pglDisable( GL_BLEND );
		break;
	}

	if( tr.fogEnabled && m_pSpriteHeader->texFormat != SPR_ALPHTEST )
	{
		// do software fog here
		float depth = DotProduct( sprite_origin, RI->vforward ) - RI->viewplanedist;
		float fogFactor = pow( 2.0, -tr.fogDensity * depth );
		fogFactor = bound( 0.0f, fogFactor, 1.0f );
		tr.blend = Lerp( fogFactor, 0.0f, tr.blend );
	}

	Vector color, color2;

	// all sprites can have color
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
	if( m_pCurrentEntity->curstate.rendercolor.r || m_pCurrentEntity->curstate.rendercolor.g || m_pCurrentEntity->curstate.rendercolor.b )
	{
		color[0] = (float)m_pCurrentEntity->curstate.rendercolor.r * ( 1.0f / 255.0f );
		color[1] = (float)m_pCurrentEntity->curstate.rendercolor.g * ( 1.0f / 255.0f );
		color[2] = (float)m_pCurrentEntity->curstate.rendercolor.b * ( 1.0f / 255.0f );
	}
	else
	{
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
	}
          
	if( SpriteHasLightmap( m_pSpriteHeader->texFormat ))
	{
		Vector lightColor;

		gEngfuncs.pTriAPI->LightAtPoint( sprite_origin, (float *)&lightColor );
		color2 = lightColor * ( 1.0f / 255.0f );

		if( glState.drawTrans )
			pglDepthMask( GL_TRUE );

		// NOTE: sprites with 'lightmap' looks ugly when alpha func is GL_GREATER 0.0
		pglAlphaFunc( GL_GEQUAL, 0.5f );
	}

	float lerp = 1.0f, ilerp;
	mspriteframe_t *frame, *oldframe;

	if( SpriteAllowLerping( m_pCurrentEntity, m_pSpriteHeader )) lerp = GetSpriteFrameInterpolant( &oldframe, &frame );
	else frame = oldframe = GetSpriteFrame( m_pCurrentEntity->curstate.frame, m_pCurrentEntity->angles[YAW] );

	int type = m_pSpriteHeader->type;

	// automatically roll parallel sprites if requested
	if( m_pCurrentEntity->angles[ROLL] != 0.0f && type == SPR_FWD_PARALLEL )
		type = SPR_FWD_PARALLEL_ORIENTED;

	Vector v_forward, v_right, v_up;

	switch( type )
	{
	case SPR_ORIENTED:
		AngleVectors( m_pCurrentEntity->angles, v_forward, v_right, v_up );
		sprite_origin = sprite_origin - v_forward * 0.05f; // to avoid z-fighting
		break;
	case SPR_FACING_UPRIGHT:
		v_right.x = sprite_origin.y - RI->vieworg.y;
		v_right.y = -(sprite_origin.x - RI->vieworg.x);
		v_right.z = 0.0f;
		v_up.x = v_up.y = 0.0f;
		v_up.z = 1.0f;
		v_right = v_right.Normalize();
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		{
			float dot = RI->vforward.z;
			if(( dot > 0.999848f ) || ( dot < -0.999848f ))	// cos(1 degree) = 0.999848
				return; // invisible

			v_right.x = RI->vforward.y;
			v_right.y = -RI->vforward.x;
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
				v_right[i] = (RI->vright[i] * cr + RI->vup[i] * sr);
				v_up[i] = RI->vright[i] * -sr + RI->vup[i] * cr;
			}
		}
		break;
	case SPR_FWD_PARALLEL: // normal sprite
	default:
		v_right = RI->vright; 
		v_up = RI->vup;
		break;
	}

	R_LoadIdentity();

	if( m_pSpriteHeader->facecull == SPR_CULL_NONE )
		GL_Cull( GL_NONE );
		
	if( oldframe == frame )
	{
		// draw the single non-lerped frame
		pglColor4f( color[0], color[1], color[2], tr.blend );
		GL_Bind( GL_TEXTURE0, frame->gl_texturenum );
		DrawSpriteQuad( frame, sprite_origin, v_right, v_up, scale );
	}
	else
	{
		// draw two combined lerped frames
		lerp = bound( 0.0f, lerp, 1.0f );
		ilerp = 1.0f - lerp;

		if( ilerp != 0 )
		{
			pglColor4f( color[0], color[1], color[2], tr.blend * ilerp );
			GL_Bind( GL_TEXTURE0, oldframe->gl_texturenum );
			DrawSpriteQuad( oldframe, sprite_origin, v_right, v_up, scale );
		}

		if( lerp != 0 )
		{
			pglColor4f( color[0], color[1], color[2], tr.blend * lerp );
			GL_Bind( GL_TEXTURE0, frame->gl_texturenum );
			DrawSpriteQuad( frame, sprite_origin, v_right, v_up, scale );
		}
	}

	// draw the sprite 'lightmap' :-)
	if( SpriteHasLightmap( m_pSpriteHeader->texFormat ))
	{
		if( !r_lightmap->value )
			pglEnable( GL_BLEND );
		else pglDisable( GL_BLEND );
		pglDepthFunc( GL_EQUAL );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_ZERO, GL_SRC_COLOR );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglShadeModel( GL_SMOOTH );

		GL_Bind( GL_TEXTURE0, tr.whiteTexture );
		DrawLighting( frame, sprite_origin, v_right, v_up, color2, scale, tr.blend );

		pglAlphaFunc( GL_GREATER, DEFAULT_ALPHATEST );
		if( glState.drawTrans ) 
			pglDepthMask( GL_FALSE );
		pglShadeModel( GL_FLAT );
	}

	if( m_pSpriteHeader->facecull == SPR_CULL_NONE )
		GL_Cull( GL_FRONT );

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow )
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
	m_pRenderModel = m_pCurrentEntity->model;
	m_pSpriteHeader = (msprite_t *)Mod_Extradata( m_pRenderModel );

	// always compute origin first
	SpriteComputeOrigin( m_pCurrentEntity );

	if( CullSpriteModel( )) return;

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST )
	{
		pglAlphaFunc( GL_GREATER, 0.0f );
		pglEnable( GL_ALPHA_TEST );
	}

	mspriteframe_t *frame = GetSpriteFrame( m_pCurrentEntity->curstate.frame, m_pCurrentEntity->angles[YAW] );

	int type = m_pSpriteHeader->type;
	float scale = m_pCurrentEntity->curstate.scale;

	// automatically roll parallel sprites if requested
	if( m_pCurrentEntity->angles[ROLL] != 0.0f && type == SPR_FWD_PARALLEL )
		type = SPR_FWD_PARALLEL_ORIENTED;

	Vector v_forward, v_right, v_up;
	Vector g_forward, g_right, g_up, g_vieworg;
	float cr, sr, dot, angle;

	// g-cont. this is a very stupid case but...
	matrix3x3	vectors( tr.cached_viewangles - RI->viewangles );
	g_vieworg = RI->vieworg;
	g_forward = vectors[0];
	g_right = vectors[1];
	g_up = vectors[2];

	switch( type )
	{
	case SPR_ORIENTED:
		AngleVectors( m_pCurrentEntity->angles, v_forward, v_right, v_up );
		sprite_origin = sprite_origin - v_forward * 0.01f; // to avoid z-fighting
		break;
	case SPR_FACING_UPRIGHT:
		v_right.x = sprite_origin.y - g_vieworg.y;
		v_right.y = -(sprite_origin.x - g_vieworg.x);
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
	DrawSpriteQuad( frame, sprite_origin, v_right, v_up, scale );

	pglDisable( GL_TEXTURE_2D );
	GL_Cull( GL_FRONT );

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST )
		pglDisable( GL_ALPHA_TEST );
}

void CSpriteModelRenderer::DrawSpriteModelInternal( cl_entity_t *e )
{
	if( RI->params & RP_ENVVIEW )
		return;

	if( !Mod_Extradata( e->model ))
		return;

	if( m_pCvarLerping->value )
		m_fDoInterp = true;
	else m_fDoInterp = false;

	if(!( RI->params & RP_SHADOWVIEW ))
		SpriteDrawModel();
	else SpriteDrawModelShadowPass();
}

mspriteframe_t *CSpriteModelRenderer::GetSpriteFrame( const model_t *m_pSpriteModel, int frame )
{
	if(( m_pSpriteHeader = (msprite_t *)Mod_Extradata( (model_t *)m_pSpriteModel )) == NULL )
		return NULL;

	m_pCurrentEntity = NULL;
	return GetSpriteFrame( frame, 0.0f );
}
