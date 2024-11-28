/*
gl_sprite.cpp - sprite model rendering
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
#include "gl_local.h"
#include <mathlib.h>
#include "gl_sprite.h"
#include "gl_studio.h"
#include "gl_cvars.h"
#include "event_api.h"
#include "pm_defs.h"
#include "studio.h"
#include "triangleapi.h"

CSpriteModelRenderer g_SpriteRenderer;

/*
====================
Init

====================
*/
void CSpriteModelRenderer :: Init( void )
{
	// Set up some variables shared with engine
	m_pCvarLerping	= IEngineStudio.GetCvar( "r_sprite_lerping" );
	m_pCvarLighting	= IEngineStudio.GetCvar( "r_sprite_lighting" );
}

/*
====================
CSpriteModelRenderer

====================
*/
CSpriteModelRenderer :: CSpriteModelRenderer( void )
{
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
		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;

		float *pintervals = pspritegroup->intervals;
		int numframes = pspritegroup->numframes;
		float fullinterval = pintervals[numframes-1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		float targettime = m_clTime - ((int)( m_clTime / fullinterval )) * fullinterval;

		int i;
		for( i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( m_pSpriteHeader->frames[frame].type == SPR_ANGLED )
	{
		int angleframe = (int)(Q_rint(( RI->view.angles[YAW] - yaw + 45.0f ) / 360 * 8 ) - 4) & 7;

		// doom-style sprite monsters
		pspritegroup = (mspritegroup_t *)m_pSpriteHeader->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}

	return pspriteframe;
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
				sprite_origin = R_StudioAttachmentPos( parent, num - 1 );
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
	if (!m_pSpriteHeader)
		return true;

	SpriteComputeBBox(m_pCurrentEntity, NULL);

	if (R_CullModel(m_pCurrentEntity, sprite_absmin, sprite_absmax))
		return true;

	return !Mod_CheckBoxVisible(sprite_absmin, sprite_absmax);
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

	float dist = (sprite_origin - GetVieworg( )).Length();

	if( !FBitSet( RI->params, RP_MIRRORVIEW ))
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( GetVieworg(), sprite_origin, PM_GLASS_IGNORE, -1, &tr );

		if((( 1.0f - tr.fraction ) * dist ) > 8.0f )
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
float CSpriteModelRenderer :: SpriteGlowBlend( int rendermode, int renderfx, int alpha, float &scale )
{
	float dist = GlowSightDistance();
	float brightness;

	if( dist <= 0 ) return 0.0f; // occluded

	if( renderfx == kRenderFxNoDissipation )
		return (float)alpha * (1.0f / 255.0f);

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
int CSpriteModelRenderer :: SpriteOccluded( int &alpha, float &pscale )
{
	// always compute origin first
	SpriteComputeOrigin( m_pCurrentEntity );

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow )
	{
		// don't reflect this entity in mirrors
		if( FBitSet( m_pCurrentEntity->curstate.effects, EF_NOREFLECT ) && FBitSet( RI->params, RP_MIRRORVIEW ))
			return true;

		// draw only in mirrors
		if( FBitSet( m_pCurrentEntity->curstate.effects, EF_REFLECTONLY ) && !FBitSet( RI->params, RP_MIRRORVIEW ))
			return true;

		// original glow occlusion code by BUzer from Paranoia
		if( m_pCurrentEntity->curstate.renderfx == kRenderFxNoDissipation && CVAR_TO_BOOL( v_glows ))
		{
			mspriteframe_t *frame = GetSpriteFrame( m_pCurrentEntity->curstate.frame, m_pCurrentEntity->angles[YAW] );
			Vector left = Vector( 0.0f, ( frame->left * pscale ) / 5.0f, 0.0f ); 
			Vector right = Vector( 0.0f, ( frame->right * pscale ) / 5.0f, 0.0f ); 
			matrix4x4	sprite_transform = RI->view.matrix;

			sprite_transform.SetOrigin( sprite_origin );
			Vector aleft = sprite_transform.VectorTransform( left );
			Vector aright = sprite_transform.VectorTransform( right );
			Vector dist = aright - aleft;

			float dst = DotProduct( GetVForward(), sprite_origin - GetVieworg() );
			dst = bound( 0.0f, dst, 64.0f );
			dst = dst / 64.0f;

			int numtraces = GLOW_NUM_TRACES;
			if( numtraces < 1 ) numtraces = 1;
			Vector step = dist * (1.0f / ( numtraces + 1 ));
			float frac = 1.0f / numtraces;
			float totalfrac = 0.0f;

			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );

			for( int j = 0; j < numtraces; j++ )
			{
				Vector start = aleft + step * (j+1);

				pmtrace_t pmtrace;
				gEngfuncs.pEventAPI->EV_PlayerTrace( GetVieworg(), start, PM_GLASS_IGNORE|PM_STUDIO_IGNORE, -1, &pmtrace );

				if( pmtrace.fraction == 1.0f )
					totalfrac += frac;
			}

			float targetalpha = totalfrac;
			float blend;

			if( m_pCurrentEntity->latched.sequencetime > targetalpha )
			{
				m_pCurrentEntity->latched.sequencetime -= tr.frametime * GLOW_INTERP_SPEED;
				if( m_pCurrentEntity->latched.sequencetime <= targetalpha )
					m_pCurrentEntity->latched.sequencetime = targetalpha;
			}
			else if( m_pCurrentEntity->latched.sequencetime < targetalpha )
			{
				m_pCurrentEntity->latched.sequencetime += tr.frametime * GLOW_INTERP_SPEED;
				if( m_pCurrentEntity->latched.sequencetime >= targetalpha )
					m_pCurrentEntity->latched.sequencetime = targetalpha;
			}

			blend = m_pCurrentEntity->latched.sequencetime * dst;
			alpha *= blend;

			if( blend <= 0.01f )
				return true; // faded
		}
		else
		{
			Vector	screenVec;
			float	blend;

			WorldToScreen( sprite_origin, screenVec );

			if( screenVec[0] < RI->view.port[0] || screenVec[0] > RI->view.port[0] + RI->view.port[2] )
				return true; // out of screen
			if( screenVec[1] < RI->view.port[1] || screenVec[1] > RI->view.port[1] + RI->view.port[3] )
				return true; // out of screen

			blend = SpriteGlowBlend( m_pCurrentEntity->curstate.rendermode, m_pCurrentEntity->curstate.renderfx, alpha, pscale );
			alpha *= blend;

			if( blend <= 0.01f )
				return true; // faded
		}
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
}

int CSpriteModelRenderer :: SpriteHasLightmap( int texFormat )
{
	if( m_pCvarLighting->value == 0 )
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
R_DrawSpriteModel
=================
*/
void CSpriteModelRenderer :: AddSpriteModelToDrawList( cl_entity_t *e, bool update )
{
	// obviously we can't update sprites...
	if( update ) return;

	m_pCurrentEntity = RI->currententity;

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );

	m_pRenderModel = m_pCurrentEntity->model;
	m_pSpriteHeader = (msprite_t *)Mod_Extradata( m_pRenderModel );

	if( !m_pSpriteHeader ) return;

	int alpha = m_pCurrentEntity->curstate.renderamt;
	float scale = m_pCurrentEntity->curstate.scale;
	int rendermode = m_pCurrentEntity->curstate.rendermode;

	if( SpriteOccluded( alpha, scale ))
		return; // sprite culled

	if( m_pSpriteHeader->texFormat == SPR_ALPHTEST )
	{
		if( rendermode != kRenderGlow && rendermode != kRenderTransAdd )
			rendermode = kRenderTransAlpha;
	}

	Vector color, color2;

	// add basecolor (any rendermode can have colored sprite)
	color[0] = (float)m_pCurrentEntity->curstate.rendercolor.r * ( 1.0f / 255.0f );
	color[1] = (float)m_pCurrentEntity->curstate.rendercolor.g * ( 1.0f / 255.0f );
	color[2] = (float)m_pCurrentEntity->curstate.rendercolor.b * ( 1.0f / 255.0f );
          
	if( SpriteHasLightmap( m_pSpriteHeader->texFormat ))
	{
		gEngfuncs.pTriAPI->LightAtPoint( sprite_origin, (float *)&color2 );
		color2 *= (1.0f / 255.0f);
		color *= color2;
	}

	mspriteframe_t *frame = GetSpriteFrame( m_pCurrentEntity->curstate.frame, m_pCurrentEntity->angles[YAW] );

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
			sprite_origin = sprite_origin - v_forward * 0.01f; // to avoid z-fighting
		}
		break;
	case SPR_FACING_UPRIGHT:
		{
			v_right.x = sprite_origin.y - GetVieworg().y;
			v_right.y = -(sprite_origin.x - GetVieworg().x);
			v_right.z = 0.0f;
			v_up.x = v_up.y = 0.0f;
			v_up.z = 1.0f;
			v_right = v_right.Normalize();
		}
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		{
			float dot = GetVForward().z;
			if(( dot > 0.999848f ) || ( dot < -0.999848f ))	// cos(1 degree) = 0.999848
				return; // invisible

			v_right.x = GetVForward().y;
			v_right.y = -GetVForward().x;
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
				v_right[i] = (GetVLeft()[i] * cr + GetVUp()[i] * sr);
				v_up[i] = GetVLeft()[i] * -sr + GetVUp()[i] * cr;
			}
		}
		break;
	case SPR_FWD_PARALLEL: // normal sprite
	default:
		{
			v_right = GetVLeft(); 
			v_up = GetVUp();
		}
		break;
	}

	float flAlpha = (float)alpha * ( 1.0f / 255.0f );

	if( m_pCurrentEntity->curstate.rendermode == kRenderGlow && m_pCurrentEntity->curstate.renderfx == kRenderFxNoDissipation )
	{
		if( CVAR_TO_BOOL( v_glows ))
		{
			flAlpha = m_pCurrentEntity->curstate.renderamt * ( 1.0f/ 255.0f );
			color *= (float)alpha * ( 1.0f/ 255.0f );
		}
	}

	Vector point[4];
	Vector4D spriteColor;
	Vector absmin, absmax;

	spriteColor = Vector4D( color[0], color[1], color[2], flAlpha );
	point[0] = sprite_origin + v_up * (frame->down * scale);
	point[0] = point[0] + v_right * (frame->left * scale);
	point[1] = sprite_origin + v_up * (frame->up * scale);
	point[1] = point[1] + v_right * (frame->left * scale);
	point[2] = sprite_origin + v_up * (frame->up * scale);
	point[2] = point[2] + v_right * (frame->right * scale);
	point[3] = sprite_origin + v_up * (frame->down * scale);
	point[3] = point[3] + v_right * (frame->right * scale);

	// more precision bounds than sprite_absmin\absmax
	ClearBounds( absmin, absmax );
	for( int i = 0; i < 4; i++ )
		AddPointToBounds( point[i], absmin, absmax );

	CTransEntry entry;
	entry.SetRenderPrimitive( point, spriteColor, TextureHandle(frame), rendermode);
	entry.ComputeViewDistance( absmin, absmax );
	RI->frame.trans_list.AddToTail( entry );
}

mspriteframe_t *CSpriteModelRenderer :: GetSpriteFrame( const model_t *m_pSpriteModel, int frame )
{
	if(( m_pSpriteHeader = (msprite_t *)Mod_Extradata( (model_t *)m_pSpriteModel )) == NULL )
		return NULL;

	m_pCurrentEntity = NULL;
	return GetSpriteFrame( frame, 0.0f );
}
