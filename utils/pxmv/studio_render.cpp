/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/
// studio_render.cpp: routines for drawing Half-Life 3DStudio models
// updates:
// 1-4-99	fixed AdvanceFrame wraping bug

#include <gl.h>
#include <GL/glu.h>
#include <mxMessageBox.h>
#include "StudioModel.h"
#include "ViewerSettings.h"
#include "ControlPanel.h"
#include "GlWindow.h"
#include "cmdlib.h"

typedef struct sortedmesh_s
{
	mstudiomesh_t	*mesh;
	int		flags;			// face flags
} sortedmesh_t;

////////////////////////////////////////////////////////////////////////
CUtlArray<Vector>	g_xformverts;	// transformed vertices
CUtlArray<Vector>	g_xformnorms;	// transformed vertices
CUtlArray<Vector>	g_lightvalues;	// light surface normals

Vector		g_lightvec;			// light vector in model reference frame
Vector		g_blightvec[MAXSTUDIOBONES];		// light vectors in bone reference frames
int		g_ambientlight;			// ambient world light
float		g_shadelight;			// direct world light
Vector		g_lightcolor;

int		g_smodels_total;			// cookie
sortedmesh_t	g_sorted_meshes[1024];		// sorted meshes

matrix3x4		m_protationmatrix;
Vector2D		g_chrome[MAXSTUDIOVERTS];		// texture coords for surface normals
int		g_chromeage[MAXSTUDIOBONES];		// last time chrome vectors were updated
Vector		g_chromeup[MAXSTUDIOBONES];		// chrome vector "up" in bone reference frames
Vector		g_chromeright[MAXSTUDIOBONES];	// chrome vector "right" in bone reference frames
bool		bUseWeaponOrigin = false;
bool		bUseWeaponLeftHand = false;
bool		bUseParanoiaFOV = false;
extern bool	g_bStopPlaying;

CBaseBoneSetup	g_boneSetup;			// new blender implementation with IK :-)

static float hullcolor[8][3] = 
{
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 0.5f, 0.5f },
{ 0.5f, 1.0f, 0.5f },
{ 1.0f, 1.0f, 0.5f },
{ 0.5f, 0.5f, 1.0f },
{ 1.0f, 0.5f, 1.0f },
{ 0.5f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
};

//-----------------------------------------------------------------------------
// Purpose: Keeps a global clock to autoplay sequences to run from
//			Also deals with speedScale changes
//-----------------------------------------------------------------------------
float GetAutoPlayTime()
{
	static double prevTime;
	static float g_time = 0.0f;

	double currentTime = I_FloatTime();
	g_time += (currentTime - prevTime) * g_viewerSettings.speedScale;
	prevTime = currentTime;

	return g_time;
}

//-----------------------------------------------------------------------------
// Purpose: Keeps a global clock for "realtime" overlays to run from
//-----------------------------------------------------------------------------
float GetRealtimeTime()
{
	// renamed static's so debugger doesn't get confused and show the wrong one
	static double g_prevRealtime;
	static float g_timeRT = 0.0f;

	double currentTime = I_FloatTime();
	g_timeRT += currentTime - g_prevRealtime;
	g_prevRealtime = currentTime;

	return g_timeRT;
}

/*
===============
MeshCompare

Sorting opaque entities by model type
===============
*/
static int MeshCompare( const void *s1, const void *s2 )
{
	sortedmesh_t	*a = (sortedmesh_t *)s1;
	sortedmesh_t	*b = (sortedmesh_t *)s2;

	if( FBitSet( a->flags, STUDIO_NF_ADDITIVE ))
		return 1;

	if( FBitSet( a->flags, STUDIO_NF_MASKED ))
		return -1;

	return 0;
}

////////////////////////////////////////////////////////////////////////

void StudioModel :: centerView( bool reset )
{
	Vector min, max;

	ExtractBbox( min, max );

	float dx = max[0] - min[0];
	float dy = max[1] - min[1];
	float dz = max[2] - min[2];
	float d = Q_max( dx, Q_max( dy, dz ));

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

	g_viewerSettings.movementScale = Q_max( 1.0f, d * 0.01f );
}

bool StudioModel :: AdvanceFrame( float dt )
{
	if( !m_pstudiohdr ) return false;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + m_sequence;

	if( dt > 0.1f )
		dt = 0.1f;
	m_dt = dt;

	float t = GetDuration( );

	if( t > 0.0f )
	{
		if( dt > 0.0f )
		{
			m_cycle += (dt / t);

			if( pseqdesc->flags & STUDIO_LOOPING || g_viewerSettings.sequence_autoplay )
				m_cycle -= (int)(m_cycle);
			else m_cycle = bound( 0.0f, m_cycle, 1.0f );
		}
	}
	else
	{
		m_cycle = 0;
	}

	return true;
}

float StudioModel::GetInterval( void )
{
	return m_dt;
}

float StudioModel::GetCycle( void )
{
	return m_cycle;
}

float StudioModel::GetFrame( void )
{
	return GetCycle() * GetMaxFrame();
}

int StudioModel::GetMaxFrame( void )
{
	return g_boneSetup.LocalMaxFrame( m_sequence );
}

int StudioModel :: SetFrame( int frame )
{
	if( frame == -1 )
		return GetFrame();

	if ( frame <= 0 )
		frame = 0;

	int maxFrame = GetMaxFrame();
	if ( frame >= maxFrame )
	{
		frame = maxFrame;
		m_cycle = 0.99999;
		return frame;
	}

	m_cycle = frame / (float)maxFrame;
	return frame;
}

void StudioModel :: SetupTransform( bool bMirror )
{
	Vector origin, angles;
	float scale = 1.0f;

	origin = angles = g_vecZero;

	if( !bUseWeaponOrigin && FBitSet( m_pstudiohdr->flags, STUDIO_ROTATE ))
		angles[1] = anglemod( 100.0f * m_flTime );

	if( g_viewerSettings.editMode == EDIT_SOURCE )
		origin = m_editfields[0].origin;

	// build the rotation matrix
	m_protationmatrix = matrix3x4( origin, angles, scale );

	if( bMirror )
	{
		m_protationmatrix.SetUp( -m_protationmatrix.GetUp() );
	}

	if( bUseWeaponOrigin && bUseWeaponLeftHand )
	{
		// inverse the right vector
		m_protationmatrix.SetRight( -m_protationmatrix.GetRight() );
	}
}

void StudioModel :: BlendSequence( Vector pos[], Vector4D q[], blend_sequence_t *seqblend )
{
	static Vector	pos1b[MAXSTUDIOBONES];
	static Vector4D	q1b[MAXSTUDIOBONES];
	float		s;

	if( seqblend->blendtime != 0.0f && ( seqblend->blendtime + seqblend->fadeout > m_flTime ) && ( seqblend->sequence < m_pstudiohdr->numseq ))
	{
		s = 1.0f - (m_flTime - seqblend->blendtime) / seqblend->fadeout;

		if( s > 0 && s <= 1.0 )
		{
			// do a nice spline curve
			s = 3 * s * s - 2 * s * s * s;
		}
		else if( s > 1.0f )
		{
			// Shouldn't happen, but maybe curtime is behind animtime?
			s = 1.0f;
		}

		g_boneSetup.AccumulatePose( &m_ik, pos, q, seqblend->sequence, seqblend->cycle, s );
	}
}

void StudioModel :: SetUpBones( bool bMirror )
{
	int		i;

	mstudiobone_t	*pbones;
	mstudioboneinfo_t	*pboneinfo;
	CIKContext	*pIK = NULL;
	float		adj[MAXSTUDIOCONTROLLERS];
	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];
	matrix3x4		bonematrix;

	if( m_sequence >= m_pstudiohdr->numseq )
		m_sequence = 0;

	Vector	a1 = m_protationmatrix.GetAngles();
	Vector	p1 = m_protationmatrix.GetOrigin();

	m_ik.Init( &g_boneSetup, a1, p1, GetRealtimeTime(), m_iFramecounter );
	pIK = NULL;

	if( g_viewerSettings.enableIK && !bMirror )
	{
		pIK = &m_ik;
	}

	g_boneSetup.InitPose( pos, q );
	g_boneSetup.UpdateRealTime( GetRealtimeTime() );
	g_boneSetup.CalcBoneAdj( adj, m_controller, m_mouth );	
	g_boneSetup.AccumulatePose( pIK, pos, q, m_sequence, m_cycle, 1.0 );

	// blends from previous sequences
	for( i = 0; i < MAX_SEQBLENDS; i++ )
		BlendSequence( pos, q, &m_seqblend[i] );

	CIKContext auto_ik;
	auto_ik.Init( &g_boneSetup, a1, p1, 0.0f, 0 );

	g_boneSetup.UpdateRealTime( GetAutoPlayTime() );
	g_boneSetup.CalcAutoplaySequences( &auto_ik, pos, q );
//	g_boneSetup.CalcBoneAdj( pos, q, m_controller, m_mouth );

	if( pIK )
	{
		Vector deltaPos;
		Vector deltaAngles;

		GetMovement( m_prevcycle, deltaPos, deltaAngles );

		deltaPos = m_protationmatrix.VectorRotate( deltaPos );

		pIK->UpdateTargets( pos, q, m_pbonetransform );

		glDisable (GL_TEXTURE_2D);
		glDisable (GL_CULL_FACE);

		if (g_viewerSettings.transparency < 1.0f)
			glDisable (GL_DEPTH_TEST);
		else
			glEnable (GL_DEPTH_TEST);

		glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// FIXME: check number of slots?
		for( int i = 0; i < pIK->m_target.Count(); i++ )
		{
			CIKTarget *pTarget = &pIK->m_target[i];

			switch( pTarget->type )
			{
			case IK_GROUND:
				{
					// g_boneSetup.debugLine( pTarget->est.pos, pTarget->est.pos + pTarget->offset.pos, 0, 255, 0 );

					// hack in movement
					pTarget->est.pos -= deltaPos;

					Vector tmp = m_protationmatrix.VectorITransform( pTarget->est.pos );
					tmp.z = pTarget->est.floor;
					pTarget->est.pos = m_protationmatrix.VectorTransform( tmp );
					pTarget->est.q = m_protationmatrix.GetQuaternion();

					float color[4] = { 1.0f, 1.0f, 0.0f, 1.0f };

					if( pTarget->est.latched > 0.0f )
						color[1] = 1.0 - Q_min( pTarget->est.flWeight, 1.0 );
					else color[0] = 1.0 - Q_min( pTarget->est.flWeight, 1.0 );

					float r = Q_max( pTarget->est.radius, 1.0f );
					Vector p0 = tmp + Vector( -r, -r, 0.1f );
					Vector p2 = tmp + Vector( r, r, 0.1f );

					drawTransparentBox( p0, p2, m_protationmatrix, color );
				}
				break;
			case IK_ATTACHMENT:
				{
					matrix3x4 m = matrix3x4( pTarget->est.pos, pTarget->est.q );
					drawTransform( m, 4 );
				}
				break;
			}

			// g_boneSetup.drawLine( pTarget->est.pos, pTarget->latched.pos, 255, 0, 0 );
		}
		
		pIK->SolveDependencies( pos, q, m_pbonetransform );

		g_GlWindow->setupRenderMode(); // restore right rendermode
	}

	pbones = (mstudiobone_t *)((byte *)m_pstudiohdr + m_pstudiohdr->boneindex);
	pboneinfo = (mstudioboneinfo_t *)((byte *)pbones + m_pstudiohdr->numbones * sizeof( mstudiobone_t ));

	for( i = 0; i < m_pstudiohdr->numbones; i++ )
	{
		m_pbonecolor[i] = Vector( 0.0f, 0.0f, 1.0f );	// animated bone is blue (guess)

		// animate all non-simulated bones
		if( CalcProceduralBone( m_pstudiohdr, i, m_pbonetransform ))
		{
			m_pbonecolor[i] = Vector( 0.0f, 1.0f, 0.0f ); // procedural bone is green
			continue;
		}
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( FBitSet( pbones[i].flags, BONE_JIGGLE_PROCEDURAL ) && FBitSet( m_pstudiohdr->flags, STUDIO_HAS_BONEINFO ))
		{
			// Physics-based "jiggle" bone
			// Bone is assumed to be along the Z axis
			// Pitch around X, yaw around Y

			// compute desired bone orientation
			matrix3x4 goalMX;

			if( pbones[i].parent == -1 ) goalMX = m_protationmatrix.ConcatTransforms( bonematrix );
			else goalMX = m_pbonetransform[pbones[i].parent].ConcatTransforms( bonematrix );

			// get jiggle properties from QC data
			mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)((byte *)m_pstudiohdr + pboneinfo[i].procindex);
			if( !m_pJiggleBones ) m_pJiggleBones = new CJiggleBones;

			// do jiggle physics
			if( pboneinfo[i].proctype == STUDIO_PROC_JIGGLE )
			{
				m_pJiggleBones->BuildJiggleTransformations( i, m_flTime, jiggleInfo, goalMX, m_pbonetransform[i] );
				m_pbonecolor[i] = Vector( 1.0f, 0.5f, 0.0f ); // jiggle bone is orange
			}
			else m_pbonetransform[i] = goalMX; // fallback
		}
		else if( pbones[i].parent == -1 )
		{
			m_pbonetransform[i] = m_protationmatrix.ConcatTransforms( bonematrix );
		}
		else
		{
			m_pbonetransform[i] = m_pbonetransform[pbones[i].parent].ConcatTransforms( bonematrix );
		}
	}

	for( i = 0; i < m_pstudiohdr->numbones; i++ )
	{
		m_pworldtransform[i] = m_pbonetransform[i].ConcatTransforms( m_plocaltransform[i] );
	}
}

/*
================
StudioModel::TransformFinalVert
================
*/
void StudioModel :: Lighting( Vector &lv, int bone, int flags, const Vector &normal )
{
	float 	illum;
	float	lightcos;

	illum = g_ambientlight;

	if (flags & STUDIO_NF_FULLBRIGHT)
	{
		lv = Vector( 1.0f );
		return;
	}

	if (flags & STUDIO_NF_FLATSHADE)
	{
		illum += g_shadelight * 0.8;
	} 
	else 
	{
		float r;

		if( bone != -1 ) lightcos = DotProduct (normal, g_blightvec[bone]);
		else lightcos = DotProduct (normal, g_lightvec);
		if (lightcos > 1.0f) lightcos = 1.0f; // -1 colinear, 1 opposite

		illum += g_shadelight;

		r = 1.5f;	// lambert

 		// do modified hemispherical lighting
		if( r <= 1.0f )
		{
			r += 1.0f;
			lightcos = (( r - 1.0f ) - lightcos) / r;
			if( lightcos > 0.0f ) 
				illum += g_shadelight * lightcos; 
		}
		else
		{
			lightcos = (lightcos + ( r - 1.0f )) / r;
			if( lightcos > 0.0f )
				illum -= g_shadelight * lightcos; 
		}

		if (illum <= 0)
			illum = 0;
	}

	if (illum > 255) 
		illum = 255;

	lv = g_lightcolor * Vector( illum / 255.0 );	// Light from 0 to 1.0
}


void StudioModel :: Chrome( Vector2D &chrome, int bone, const Vector &normal )
{
	float n;

	if (g_chromeage[bone] != g_smodels_total)
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		Vector	chromeupvec;	// g_chrome t vector in world reference frame
		Vector	chromerightvec;	// g_chrome s vector in world reference frame
		Vector	tmp, v_left;	// vector pointing at bone in world reference frame

		tmp = -m_protationmatrix.GetOrigin();
		tmp += m_pbonetransform[bone].GetOrigin();
		tmp = tmp.Normalize();
		v_left = Vector( 0.0f, -1.0f, 0.0f );

		chromeupvec = CrossProduct( tmp, v_left ).Normalize();
		chromerightvec = CrossProduct( tmp, chromeupvec ).Normalize();
		chromeupvec = -chromeupvec;	// GoldSrc rules

		g_chromeup[bone] = m_pbonetransform[bone].VectorIRotate( chromeupvec );
		g_chromeright[bone] = m_pbonetransform[bone].VectorIRotate( chromerightvec );
		g_chromeage[bone] = g_smodels_total;
	}

	// calc s coord
	n = DotProduct( normal, g_chromeright[bone] );
	chrome.x = (n + 1.0f) * 32.0f;

	// calc t coord
	n = DotProduct( normal, g_chromeup[bone] );
	chrome.y = (n + 1.0f) * 32.0f;
}

void StudioModel :: DrawSpriteQuad( const Vector &org, const Vector &right, const Vector &up, float scale )
{
	Vector point;

	glBegin( GL_QUADS );
		glTexCoord2f( 0.0f, 1.0f );
		point = org + up * (-32.0f * scale);
		point = point + right * (-32.0f * scale);
		glVertex3fv( point );
		glTexCoord2f( 0.0f, 0.0f );
		point = org + up * (32.0f * scale);
		point = point + right * (-32.0f * scale);
		glVertex3fv( point );
		glTexCoord2f( 1.0f, 0.0f );
		point = org + up * (32.0f * scale);
		point = point + right * (32.0f * scale);
		glVertex3fv( point );
 	        	glTexCoord2f( 1.0f, 1.0f );
		point = org + up * (-32.0f * scale);
		point = point + right * (32.0f * scale);
		glVertex3fv( point );
	glEnd();
}

void StudioModel::RenderMuzzleFlash( muzzleflash_t *muzzle )
{
	Vector	v_right, v_up;
	float	sr, cr;

	if( muzzle->time < m_flTime )
	{
		memset( muzzle, 0, sizeof( muzzleflash_t ));
		return;	// expired
          }

	SinCos( DEG2RAD( muzzle->rotate ), &sr, &cr );
	for( int i = 0; i < 3; i++ )
	{
		v_right[i] = (g_GlWindow->vectors[1][i] * cr + g_GlWindow->vectors[0][i] * sr);
		v_up[i] = g_GlWindow->vectors[1][i] * -sr + g_GlWindow->vectors[0][i] * cr;
	}

	if( bUseWeaponOrigin && bUseWeaponLeftHand )
		v_right = -v_right;

	glEnable( GL_BLEND );
	glDepthMask( GL_FALSE );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE );
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	glBindTexture( GL_TEXTURE_2D, TEXTURE_MUZZLEFLASH1 + muzzle->texture );
	DrawSpriteQuad( muzzle->origin, v_right, v_up, muzzle->scale );
	glDisable( GL_BLEND );
	glDepthMask( GL_TRUE );
}

void StudioModel::MuzzleFlash( int attachment, int type )
{
	if( attachment >= m_pstudiohdr->numattachments )
		return; // bad attachment

	muzzleflash_t	*muzzle;

	// move current sequence into circular buffer
	m_current_muzzle = (m_current_muzzle + 1) & MASK_MUZZLEFLASHES;

	muzzle = &m_muzzleflash[m_current_muzzle];

	muzzle->texture = ( type % 10 ) % 3;
	muzzle->scale = ( type / 10 ) * 0.1f;
	if( muzzle->scale == 0.0f )
		muzzle->scale = 0.5f;

	// don't rotate on paused
	if( !g_viewerSettings.pause && !g_bStopPlaying )
	{
		if( muzzle->texture == 0 )
			muzzle->rotate = RANDOM_LONG( 0, 20 ); // rifle flash
		else muzzle->rotate = RANDOM_LONG( 0, 359 );
	}

	bool isInEditMode = (g_ControlPanel->getTableIndex() == TAB_MODELEDITOR) ? true : false;
	bool isEditSource = (g_viewerSettings.editMode == EDIT_SOURCE) ? true : false;

	if( isInEditMode && isEditSource )
	{
		for( int i = 0; i < m_numeditfields; i++ )
		{
			edit_field_t *ed = &m_editfields[i];

			if( ed->type != TYPE_ATTACHMENT || ed->id != attachment )
				continue;

			muzzle->origin = m_pbonetransform[ed->bone].VectorTransform( ed->origin );
			break;				
		}
	}
	else
	{
		mstudioattachment_t *pattachment = (mstudioattachment_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->attachmentindex) + attachment;
		muzzle->origin = m_pbonetransform[pattachment->bone].VectorTransform( pattachment->org );
	}

	if( !g_viewerSettings.pause && !g_bStopPlaying )
		muzzle->time = m_flTime + 0.015f;
	else muzzle->time = m_flTime + 0.0099f;
}

void StudioModel::ClientEvents( void )
{
	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t	*pevent;
	float		end, start;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + m_sequence;

	// no events for this animation
	if( pseqdesc->numevents == 0 )
		return;

	start = GetFrame() - g_viewerSettings.speedScale * m_flFrameTime * pseqdesc->fps;
	end = GetFrame();

	if( sequence_reset )
	{
		if( !FBitSet( pseqdesc->flags, STUDIO_LOOPING ))
			start = -0.01f;
		sequence_reset = false;
	}

	pevent = (mstudioevent_t *)((byte *)m_pstudiohdr + pseqdesc->eventindex);

	for( int i = 0; i < pseqdesc->numevents; i++ )
	{
		if( (float)pevent[i].frame > start && pevent[i].frame <= end )
		{
			switch( pevent[i].event )
			{
			case 5001:
				MuzzleFlash( 0, atoi( pevent[i].options ));
				break;
			case 5011:
				MuzzleFlash( 1, atoi( pevent[i].options ));
				break;
			case 5021:
				MuzzleFlash( 2, atoi( pevent[i].options ));
				break;
			case 5031:
				MuzzleFlash( 3, atoi( pevent[i].options ));
				break;
			}
		}
	}
}

/*
================
StudioModel::SetupLighting
	set some global variables based on entity position
inputs:
outputs:
	g_ambientlight
	g_shadelight
================
*/
void StudioModel::SetupLighting ( )
{
	g_ambientlight = 95;
	g_shadelight = 160;

	g_lightvec[0] = g_viewerSettings.gLightVec[0];
	g_lightvec[1] = g_viewerSettings.gLightVec[1];
	g_lightvec[2] = g_viewerSettings.gLightVec[2];

	g_lightcolor[0] = g_viewerSettings.lColor[0];
	g_lightcolor[1] = g_viewerSettings.lColor[1];
	g_lightcolor[2] = g_viewerSettings.lColor[2];

	g_lightvec = g_lightvec.Normalize();

	// TODO: only do it for bones that actually have textures
	for( int i = 0; i < m_pstudiohdr->numbones; i++ )
		g_blightvec[i] = m_pbonetransform[i].VectorIRotate( g_lightvec ).Normalize();
}


/*
=================
StudioModel::SetupModel
	based on the body part, figure out which mesh it should be using.
inputs:
	currententity
outputs:
	pstudiomesh
	pmdl
=================
*/

void StudioModel::SetupModel ( int bodypart )
{
	int index;

	if (bodypart > m_pstudiohdr->numbodyparts)
	{
		// Con_DPrintf ("StudioModel::SetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	mstudiobodyparts_t   *pbodypart = (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex) + bodypart;

	index = m_bodynum / pbodypart->base;
	index = index % pbodypart->nummodels;

	m_pmodel = (mstudiomodel_t *)((byte *)m_pstudiohdr + pbodypart->modelindex) + index;
}


void drawBox (Vector *v, float const * color = NULL)
{
	if( color ) glColor4fv( color );

	glBegin (GL_QUAD_STRIP);
	for (int i = 0; i < 10; i++)
		glVertex3fv (v[i & 7]);
	glEnd ();
	
	glBegin  (GL_QUAD_STRIP);
	glVertex3fv (v[6]);
	glVertex3fv (v[0]);
	glVertex3fv (v[4]);
	glVertex3fv (v[2]);
	glEnd ();

	glBegin  (GL_QUAD_STRIP);
	glVertex3fv (v[1]);
	glVertex3fv (v[7]);
	glVertex3fv (v[3]);
	glVertex3fv (v[5]);
	glEnd ();

}

//-----------------------------------------------------------------------------
// Draws the position and axies of a transformation matrix, x=red,y=green,z=blue
//-----------------------------------------------------------------------------
void StudioModel :: drawTransform( matrix3x4 &m, float flLength )
{
	glBegin( GL_LINES );
	for( int k = 0; k < 3; k++ )
	{
		glColor3f( 1, 0, 0 );
		glVertex3fv( m.GetOrigin() );
		glColor3f( 1, 1, 1 );
		glVertex3fv( m.GetOrigin() + m.GetRow( k ) * 4.0f );
	}
	glEnd();
}

//-----------------------------------------------------------------------------
// Draws a transparent box with a wireframe outline
//-----------------------------------------------------------------------------
void StudioModel :: drawTransparentBox( Vector const &bbmin, Vector const &bbmax, const matrix3x4 &m, float const *color )
{
	Vector v[8], v2[8];

	v[0][0] = bbmin[0];
	v[0][1] = bbmax[1];
	v[0][2] = bbmin[2];

	v[1][0] = bbmin[0];
	v[1][1] = bbmin[1];
	v[1][2] = bbmin[2];

	v[2][0] = bbmax[0];
	v[2][1] = bbmax[1];
	v[2][2] = bbmin[2];

	v[3][0] = bbmax[0];
	v[3][1] = bbmin[1];
	v[3][2] = bbmin[2];

	v[4][0] = bbmax[0];
	v[4][1] = bbmax[1];
	v[4][2] = bbmax[2];

	v[5][0] = bbmax[0];
	v[5][1] = bbmin[1];
	v[5][2] = bbmax[2];

	v[6][0] = bbmin[0];
	v[6][1] = bbmax[1];
	v[6][2] = bbmax[2];

	v[7][0] = bbmin[0];
	v[7][1] = bbmin[1];
	v[7][2] = bbmax[2];

	v2[0] = m.VectorTransform( v[0] );
	v2[1] = m.VectorTransform( v[1] );
	v2[2] = m.VectorTransform( v[2] );
	v2[3] = m.VectorTransform( v[3] );
	v2[4] = m.VectorTransform( v[4] );
	v2[5] = m.VectorTransform( v[5] );
	v2[6] = m.VectorTransform( v[6] );
	v2[7] = m.VectorTransform( v[7] );

	drawBox (v2, color);
}

/*
================
StudioModel::DrawModel
inputs:
	currententity
	r_entorigin
================
*/
void StudioModel :: DrawModel( bool bMirror )
{
	int	i;

	if( !m_pstudiohdr ) return;

	g_smodels_total++; // render data cache cookie

	if( m_pstudiohdr->numbodyparts == 0 )
		return;

	bool isInEditMode = (g_ControlPanel->getTableIndex() == TAB_MODELEDITOR) ? true : false;
	bool drawEyePos = g_viewerSettings.showAttachments;
	bool drawAttachments = g_viewerSettings.showAttachments;
	bool drawHitboxes = g_viewerSettings.showHitBoxes;
	bool drawAbsBox = false;
	int drawIndex = -1; // draw all
	int colorIndex = -1;

	if( isInEditMode && m_pedit )
	{
		drawEyePos = (m_pedit->type == TYPE_EYEPOSITION) ? true : false;
		drawAttachments = (m_pedit->type == TYPE_ATTACHMENT) ? true : false;
		drawHitboxes = (m_pedit->type == TYPE_HITBOX) ? true : false;
		drawAbsBox = (m_pedit->type == TYPE_BBOX || m_pedit->type == TYPE_CBOX) ? true : false;
		drawIndex = m_pedit->id;

		if(m_pedit->type == TYPE_HITBOX)
		{
			mstudiobbox_t *phitbox = (mstudiobbox_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->hitboxindex) + m_pedit->id;

			if( g_viewerSettings.editMode == EDIT_MODEL )
				colorIndex = (phitbox->group % 8);
			else colorIndex = (m_pedit->hitgroup % 8);
		}
	}

	SetupTransform( bMirror );

	SetUpBones( bMirror );

	if( !bMirror ) 
	{
		updateModel();
	}

	SetupLighting( );
	ClientEvents( );

	for( i = 0; i < m_pstudiohdr->numbodyparts; i++ ) 
	{
		SetupModel( i );

		if( g_viewerSettings.transparency > 0.0f )
			DrawPoints();

		if( g_viewerSettings.showWireframeOverlay && g_viewerSettings.renderMode != RM_WIREFRAME )
			DrawPoints( true );
	}

	glDisable( GL_MULTISAMPLE );

	for( i = 0; i < MAX_MUZZLEFLASHES; i++ )
	{
		if( m_muzzleflash[i].time == 0.0f )
			continue;
		RenderMuzzleFlash ( &m_muzzleflash[i] );
	}

	if( drawEyePos )
	{
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );

		glPointSize( 7.0f );
		glColor3f( 1.0f, 0.5f, 1.0f );

		glBegin( GL_POINTS );
			if( isInEditMode && m_pedit && g_viewerSettings.editMode == EDIT_SOURCE )
				glVertex3fv( m_protationmatrix.VectorTransform( m_pedit->origin ));
			else glVertex3fv( m_protationmatrix.VectorTransform( m_pstudiohdr->eyeposition ));
		glEnd();

		glPointSize( 1.0f );
	}

	// draw abs box
	if( drawAbsBox )
	{
		vec3_t	tmp, bbox[8];

		glDisable( GL_MULTISAMPLE );
		glDisable (GL_TEXTURE_2D);
		glDisable (GL_CULL_FACE);
		glDisable( GL_BLEND );

		if (g_viewerSettings.transparency < 1.0f)
			glDisable (GL_DEPTH_TEST);
		else
			glEnable (GL_DEPTH_TEST);

		if( m_pedit && m_pedit->type == TYPE_BBOX )
			glColor4f (1, 0, 0, 1.0f);
		else glColor4f (1, 0.5, 0, 1.0f);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if( g_ControlPanel->getTableIndex() == TAB_SEQUENCES )
		{
			mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pstudiohdr + m_pstudiohdr->seqindex) + (int)m_sequence;

			for( int j = 0; j < 8; j++ )
			{
				tmp[0] = (j & 1) ? pseqdesc->bbmin[0] : pseqdesc->bbmax[0];
				tmp[1] = (j & 2) ? pseqdesc->bbmin[1] : pseqdesc->bbmax[1];
				tmp[2] = (j & 4) ? pseqdesc->bbmin[2] : pseqdesc->bbmax[2];
				bbox[j] = tmp;
			}
		}
		else if( g_viewerSettings.editMode == EDIT_MODEL )
		{
			if( m_pedit->type == TYPE_BBOX )
			{
				for( int j = 0; j < 8; j++ )
				{
					tmp[0] = (j & 1) ? m_pstudiohdr->min[0] : m_pstudiohdr->max[0];
					tmp[1] = (j & 2) ? m_pstudiohdr->min[1] : m_pstudiohdr->max[1];
					tmp[2] = (j & 4) ? m_pstudiohdr->min[2] : m_pstudiohdr->max[2];
					bbox[j] = tmp;
				}
			}
			else
			{
				for( int j = 0; j < 8; j++ )
				{
					tmp[0] = (j & 1) ? m_pstudiohdr->bbmin[0] : m_pstudiohdr->bbmax[0];
					tmp[1] = (j & 2) ? m_pstudiohdr->bbmin[1] : m_pstudiohdr->bbmax[1];
					tmp[2] = (j & 4) ? m_pstudiohdr->bbmin[2] : m_pstudiohdr->bbmax[2];
					bbox[j] = tmp;
				}
			}
		}
		else
		{
			for( int j = 0; j < 8; j++ )
			{
				tmp[0] = (j & 1) ? m_pedit->mins[0] : m_pedit->maxs[0];
				tmp[1] = (j & 2) ? m_pedit->mins[1] : m_pedit->maxs[1];
				tmp[2] = (j & 4) ? m_pedit->mins[2] : m_pedit->maxs[2];
				bbox[j] = tmp;
			}
		}

		glBegin( GL_LINES );
		for( int i = 0; i < 2; i += 1 )
		{
			glVertex3fv( bbox[i+0] );
			glVertex3fv( bbox[i+2] );
			glVertex3fv( bbox[i+4] );
			glVertex3fv( bbox[i+6] );
			glVertex3fv( bbox[i+0] );
			glVertex3fv( bbox[i+4] );
			glVertex3fv( bbox[i+2] );
			glVertex3fv( bbox[i+6] );
			glVertex3fv( bbox[i*2+0] );
			glVertex3fv( bbox[i*2+1] );
			glVertex3fv( bbox[i*2+4] );
			glVertex3fv( bbox[i*2+5] );
		}
		glEnd();
	}

	// draw bones
	if( g_viewerSettings.showBones )
	{
		mstudiobone_t *pbones = (mstudiobone_t *)((byte *) m_pstudiohdr + m_pstudiohdr->boneindex);
		glDisable( GL_MULTISAMPLE );
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );

		for( i = 0; i < m_pstudiohdr->numbones; i++ )
		{
			if( pbones[i].parent >= 0 )
			{
				glPointSize( 4.0f );
				glColor3f( 1.0f, 0.7f, 0.0f );
				glBegin( GL_LINES );
					glVertex3fv( m_pbonetransform[pbones[i].parent].GetOrigin());
					glVertex3fv( m_pbonetransform[i].GetOrigin());
				glEnd();

				glBegin( GL_POINTS );
					if( pbones[pbones[i].parent].parent != -1 )
					{
						glColor3fv( m_pbonecolor[pbones[i].parent] );
						glVertex3fv( m_pbonetransform[pbones[i].parent].GetOrigin());
					}
					glColor3fv( m_pbonecolor[i] );
					glVertex3fv( m_pbonetransform[i].GetOrigin());
				glEnd ();
			}
			else
			{
				// draw parent bone node
				glPointSize( 6.0f );
				glColor3f( 0.8f, 0, 0 );

				glBegin( GL_POINTS );
					glVertex3fv( m_pbonetransform[i].GetOrigin() );
				glEnd ();
			}
		}

		glPointSize( 1.0f );
	}

	if( drawAttachments )
	{
		glDisable( GL_MULTISAMPLE );
		glDisable( GL_TEXTURE_2D );
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_BLEND );

		for (i = 0; i < m_pstudiohdr->numattachments; i++)
		{
			matrix3x4	local, world;

			if( drawIndex != -1 && i != drawIndex )
				continue;

			mstudioattachment_t *pattachments = (mstudioattachment_t *)((byte *) m_pstudiohdr + m_pstudiohdr->attachmentindex);

			local.SetForward( pattachments[i].vectors[0] );
			local.SetRight( pattachments[i].vectors[1] );
			local.SetUp( pattachments[i].vectors[2] );

			if( drawIndex != -1 && g_viewerSettings.editMode == EDIT_SOURCE )
				local.SetOrigin( m_pedit->origin );
			else local.SetOrigin( pattachments[i].org );

			world = m_pbonetransform[pattachments[i].bone].ConcatTransforms( local );

			// draw the vector from bone to attachment
			glBegin( GL_LINES );
				glColor3f( 1, 0, 0 );
				glVertex3fv( world.GetOrigin() );
				glColor3f( 1, 1, 1 );
				glVertex3fv( m_pbonetransform[pattachments[i].bone].GetOrigin() );
			glEnd();

			// draw the matrix axes
			if( FBitSet( pattachments[i].flags, STUDIO_ATTACHMENT_LOCAL ))
				drawTransform( world );

			glPointSize( 5 );
			glColor3f( 0, 1, 0 );

			glBegin( GL_POINTS );
				glVertex3fv( world.GetOrigin() );
			glEnd();

			glPointSize (1);
		}
	}

	if( drawHitboxes )
	{
		glDisable( GL_MULTISAMPLE );
		glDisable (GL_TEXTURE_2D);
		glDisable (GL_CULL_FACE);
		glDisable( GL_BLEND );

		if (g_viewerSettings.transparency < 1.0f)
			glDisable (GL_DEPTH_TEST);
		else
			glEnable (GL_DEPTH_TEST);

		if( colorIndex == -1 )
			glColor4f (1, 0, 0, 0.5f);
		else glColor4f( hullcolor[colorIndex][0], hullcolor[colorIndex][1], hullcolor[colorIndex][2], 1.0f );

		glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (i = 0; i < m_pstudiohdr->numhitboxes; i++)
		{
			int	bone;

			if( g_viewerSettings.showHitBoxes && isInEditMode )
			{
				if( i != drawIndex )
					glColor4f( 1.0f, 0.0f, 0.0f, 1.0f );
				else glColor4f( 0.0f, 1.0f, 0.0f, 1.0f );
			}
			else
			{
				if( drawIndex != -1 && i != drawIndex )
					continue;
			}

			mstudiobbox_t *pbboxes = (mstudiobbox_t *) ((byte *) m_pstudiohdr + m_pstudiohdr->hitboxindex);
			Vector v[8], v2[8], bbmin, bbmax;

			if( g_viewerSettings.showHitBoxes && isInEditMode && g_viewerSettings.editMode == EDIT_SOURCE )
			{
				for( int j = 0; j < m_numeditfields; j++ )
				{
					edit_field_t *ed = &m_editfields[j];

					if( ed->type != TYPE_HITBOX || ed->id != i )
						continue;

					bbmin = ed->mins;
					bbmax = ed->maxs;
					bone = ed->bone;
					break;				
				}
			}
			else
			{
				if( drawIndex != -1 && g_viewerSettings.editMode == EDIT_SOURCE )
				{
					bbmin = m_pedit->mins;
					bbmax = m_pedit->maxs;
					bone = m_pedit->bone;
				}
				else
				{
					bbmin = pbboxes[i].bbmin;
					bbmax = pbboxes[i].bbmax;
					bone = pbboxes[i].bone;
                              	}
			}

			drawTransparentBox( bbmin, bbmax, m_pbonetransform[bone] );
		}
	}
}

/*
================

================
*/
void StudioModel::DrawPoints ( bool bWireframe )
{
	int		i, j, k;
	mstudiomesh_t	*pmesh;
	byte		*pvertbone;
	byte		*pnormbone;
	Vector		*pstudioverts;
	Vector		*pstudionorms;
	mstudioboneweight_t	*pvertweight;
	mstudioboneweight_t	*pnormweight;
	mstudiotexture_t	*ptexture;
	bool		texEnabled;
	bool		need_sort;
	matrix3x4		skinMat;
	float 		*av, *nv;
	Vector		*lv;
	Vector		lv_tmp;
	short		*pskinref;
	
	if (!m_pmodel->nummesh) {
		return; // blank submodel, ignore it
	}

	pvertbone = ((byte *)m_pstudiohdr + m_pmodel->vertinfoindex);
	pnormbone = ((byte *)m_pstudiohdr + m_pmodel->norminfoindex);
	ptexture = (mstudiotexture_t *)((byte *)m_ptexturehdr + m_ptexturehdr->textureindex);

	pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex);

	pstudioverts = (Vector *)((byte *)m_pstudiohdr + m_pmodel->vertindex);
	pstudionorms = (Vector *)((byte *)m_pstudiohdr + m_pmodel->normindex);

	pskinref = (short *)((byte *)m_ptexturehdr + m_ptexturehdr->skinindex);
	if (m_skinnum != 0 && m_skinnum < m_ptexturehdr->numskinfamilies)
		pskinref += (m_skinnum * m_ptexturehdr->numskinref);

	// pre-allocate buffers
	g_xformverts.SetSize(m_pmodel->numverts);
	g_xformnorms.SetSize(m_pmodel->numnorms);
	g_lightvalues.SetSize(m_pmodel->numnorms);

	if( FBitSet( m_pstudiohdr->flags, STUDIO_HAS_BONEWEIGHTS ) && m_pmodel->blendvertinfoindex != 0 && m_pmodel->blendnorminfoindex != 0 )
	{
		pvertweight = (mstudioboneweight_t *)((byte *)m_pstudiohdr + m_pmodel->blendvertinfoindex);
		pnormweight = (mstudioboneweight_t *)((byte *)m_pstudiohdr + m_pmodel->blendnorminfoindex);

		for (i = 0; i < m_pmodel->numverts; i++)
		{
			ComputeSkinMatrix( &pvertweight[i], skinMat );
			g_xformverts[i] = skinMat.VectorTransform( pstudioverts[i] );
		}

		for (i = 0; i < m_pmodel->numnorms; i++)
		{
			ComputeSkinMatrix( &pnormweight[i], skinMat );
			g_xformnorms[i] = skinMat.VectorRotate( pstudionorms[i] );

			if( g_viewerSettings.renderMode == RM_BONEWEIGHTS )
				ComputeWeightColor( &pnormweight[i], g_lightvalues[i] );
		}
	}
	else
	{
		for( i = 0; i < m_pmodel->numverts; i++ )
		{
			g_xformverts[i] = m_pbonetransform[pvertbone[i]].VectorTransform( pstudioverts[i] );
		}

		for( i = 0; i < m_pmodel->numnorms; i++ )
		{
			g_xformnorms[i] = m_pbonetransform[pnormbone[i]].VectorRotate( pstudionorms[i] );
		}

		if( g_viewerSettings.renderMode == RM_BONEWEIGHTS )
		{
			for( i = 0; i < m_pmodel->numnorms; i++ )
				g_lightvalues[i] = Vector( 0.0f, 1.0f, 0.0f );
		}
	}

	texEnabled = glIsEnabled( GL_TEXTURE_2D ) ? true : false;

	if( bWireframe )
	{
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		glDisable( GL_TEXTURE_2D );
		glColor4f( 1.0f, 0.0f, 0.0f, 0.99f ); 

		glEnable( GL_LINE_SMOOTH );
		glEnable( GL_POLYGON_SMOOTH );
		glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
		glHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );
	}
	else if (g_viewerSettings.transparency < 1.0f)
	{
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask( GL_TRUE );
	}

	if( bWireframe == false )
		glEnable( GL_MULTISAMPLE );
//
// clip and draw all triangles
//

	lv = &g_lightvalues[0];
	need_sort = false;

	for( j = k = 0; j < m_pmodel->nummesh; j++ ) 
	{
		int flags = ptexture[pskinref[pmesh[j].skinref]].flags;

		// fill in sortedmesh info
		g_sorted_meshes[j].mesh = &pmesh[j];
		g_sorted_meshes[j].flags = flags;

		if( FBitSet( flags, STUDIO_NF_MASKED|STUDIO_NF_ADDITIVE ))
			need_sort = true;

		for( i = 0; i < pmesh[j].numnorms; i++, k++, lv++, pstudionorms++, pnormbone++ )
		{
			if( g_viewerSettings.renderMode != RM_BONEWEIGHTS )
            {
				if( FBitSet( m_pstudiohdr->flags, STUDIO_HAS_BONEWEIGHTS ))
					Lighting ( *lv, -1, flags, g_xformnorms[k] );
				else Lighting ( *lv, *pnormbone, flags, *pstudionorms );
            }

			// FIX: move this check out of the inner loop
			if (flags & STUDIO_NF_CHROME)
				Chrome( g_chrome[k], *pnormbone, *pstudionorms );
		}
	}

	if( need_sort )
	{
		// resort opaque and translucent meshes draw order
		qsort( g_sorted_meshes, m_pmodel->nummesh, sizeof( sortedmesh_t ), MeshCompare );
	}

	for (j = 0; j < m_pmodel->nummesh; j++) 
	{
		float 	s, t;
		float	transparency = g_viewerSettings.transparency;
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex) + j;
		pmesh = g_sorted_meshes[j].mesh;

		ptricmds = (short *)((byte *)m_pstudiohdr + pmesh->triindex);

		s = 1.0/(float)ptexture[pskinref[pmesh->skinref]].width;
		t = 1.0/(float)ptexture[pskinref[pmesh->skinref]].height;

		if( bWireframe == false )
		{
			//glBindTexture( GL_TEXTURE_2D, ptexture[pskinref[pmesh->skinref]].index );
			glBindTexture( GL_TEXTURE_2D, TEXTURE_COUNT + pskinref[pmesh->skinref] );

			if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_TWOSIDE)
				glDisable( GL_CULL_FACE );
			else glEnable( GL_CULL_FACE );

			if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_MASKED)
			{
				glEnable( GL_ALPHA_TEST );
				glAlphaFunc( GL_GREATER, 0.5f );
			}
			else glDisable( GL_ALPHA_TEST );

			if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_ADDITIVE)
			{
				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ONE );
//				glDepthMask( GL_FALSE );
			}
			else if (g_viewerSettings.transparency < 1.0f)
			{
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
//				glDepthMask( GL_FALSE );
			}
		}

		if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_CHROME)
		{
			while (i = *(ptricmds++))
			{
				if (i < 0)
				{
					glBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					glBegin( GL_TRIANGLE_STRIP );
				}

				if( bWireframe == false )
					g_viewerSettings.drawn_polys += (i - 2);

				for( ; i > 0; i--, ptricmds += 4)
				{
					if( bWireframe == false )
					{
						// FIX: put these in as integer coords, not floats
						glTexCoord2f(g_chrome[ptricmds[1]].x * s, g_chrome[ptricmds[1]].y * t);
					
						lv = &g_lightvalues[ptricmds[1]];
						glColor4f( lv->x, lv->y, lv->z, transparency);
                                                  }

					av = g_xformverts[ptricmds[0]];
					glVertex3f(av[0], av[1], av[2]);
				}
				glEnd( );
			}	
		} 
		else 
		{
			while (i = *(ptricmds++))
			{
				if (i < 0)
				{
					glBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					glBegin( GL_TRIANGLE_STRIP );
				}

				if( bWireframe == false )
					g_viewerSettings.drawn_polys += (i - 2);

				for( ; i > 0; i--, ptricmds += 4)
				{
					if( bWireframe == false )
					{
						if( ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_UV_COORDS )
						{
							glTexCoord2f( HalfToFloat( ptricmds[2] ), HalfToFloat( ptricmds[3] ));
						}
						else
						{
							// FIX: put these in as integer coords, not floats
							glTexCoord2f(ptricmds[2]*s, ptricmds[3]*t);
						}

						lv = &g_lightvalues[ptricmds[1]];
						glColor4f( lv->x, lv->y, lv->z, transparency);
                                                  }

					av = g_xformverts[ptricmds[0]];
					glVertex3f(av[0], av[1], av[2]);
				}
				glEnd( );
			}	
		}

		if( bWireframe == false )
		{
			if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_MASKED)
				glDisable( GL_ALPHA_TEST );

			if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_ADDITIVE || g_viewerSettings.transparency < 1.0f)
			{
				glDepthMask( GL_TRUE );
				glDisable( GL_BLEND );
			}
		}
	}

	if( bWireframe )
	{
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		if( texEnabled ) glEnable( GL_TEXTURE_2D );
		glDisable( GL_BLEND );
		glDisable( GL_LINE_SMOOTH );
		glDisable(GL_POLYGON_SMOOTH);
	}
	else
	{
		glDisable( GL_MULTISAMPLE );
	}

	if( g_viewerSettings.showNormals )
	{
		if( texEnabled ) glDisable( GL_TEXTURE_2D );
		glColor4f( 0.3f, 0.4f, 0.5f, 0.99f ); 
		glBegin( GL_LINES );
		for (j = 0; j < m_pmodel->nummesh; j++) 
		{
			short	*ptricmds;

			pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex) + j;
			ptricmds = (short *)((byte *)m_pstudiohdr + pmesh->triindex);

			while( i = *(ptricmds++))
			{
				for( i = abs( i ); i > 0; i--, ptricmds += 4 )
				{
					av = g_xformverts[ptricmds[0]];
					nv = g_xformnorms[ptricmds[1]];
					glVertex3f( av[0], av[1], av[2] );
					glVertex3f( av[0] + nv[0] * 2.0f, av[1] + nv[1] * 2.0f, av[2] + nv[2] * 2.0f );
				}
			}	
		}
		glEnd();
		if( texEnabled ) glEnable( GL_TEXTURE_2D );
	}

}

/*
================

================
*/
void StudioModel::DrawUVMapPoints()
{
	int		i, j, k;
	mstudiomesh_t	*pmesh;
	byte		*pvertbone;
	byte		*pnormbone;
	Vector		*pstudioverts;
	Vector		*pstudionorms;
	mstudiotexture_t	*ptexture;
	short		*pskinref_src;
	short		*pskinref;

	pvertbone = ((byte *)m_pstudiohdr + m_pmodel->vertinfoindex);
	pnormbone = ((byte *)m_pstudiohdr + m_pmodel->norminfoindex);
	ptexture = (mstudiotexture_t *)((byte *)m_ptexturehdr + m_ptexturehdr->textureindex);

	pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex);

	pstudioverts = (Vector *)((byte *)m_pstudiohdr + m_pmodel->vertindex);
	pstudionorms = (Vector *)((byte *)m_pstudiohdr + m_pmodel->normindex);

	pskinref_src = (short *)((byte *)m_ptexturehdr + m_ptexturehdr->skinindex);

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	glDisable( GL_TEXTURE_2D );
	glColor4f( 0.1f, 0.85f, 0.2f, 0.9f );
//
// clip and draw all triangles
//
	for (k = 0; k < m_ptexturehdr->numskinfamilies; k++ )
	{
		// try all the skinfamilies
		pskinref = pskinref_src + (k * m_ptexturehdr->numskinref);

		for (j = 0; j < m_pmodel->nummesh; j++) 
		{
			float	s, t, x, y;
			float	tex_w, tex_h;
			short	*ptricmds;

			pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex) + j;
			ptricmds = (short *)((byte *)m_pstudiohdr + pmesh->triindex);

			if( pskinref[pmesh->skinref] != g_viewerSettings.texture )
				continue;

			tex_w = (float)ptexture[pskinref[pmesh->skinref]].width;
			tex_h = (float)ptexture[pskinref[pmesh->skinref]].height;
			s = 1.0 / tex_w;
			t = 1.0 / tex_h;

			glBindTexture( GL_TEXTURE_2D, TEXTURE_COUNT + pskinref[pmesh->skinref] );

			while (i = *(ptricmds++))
			{
				if (i < 0)
				{
					glBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					glBegin( GL_TRIANGLE_STRIP );
				}

				for( ; i > 0; i--, ptricmds += 4)
				{
					if( ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_UV_COORDS )
					{
						x = HalfToFloat( ptricmds[2] ) * tex_w;
						y = HalfToFloat( ptricmds[3] ) * tex_h;

						if(( x < 0.0f || x > tex_w ) || ( y < 0.0f || y > tex_h ))
							glColor3f( 0.85f, 0.2f, 0.1f );
						else 
							glColor3f( 0.1f, 0.85f, 0.2f );
					}
					else
					{
						x = (float)ptricmds[2];
						y = (float)ptricmds[3];
					}

					x *= g_viewerSettings.textureScale;
					y *= g_viewerSettings.textureScale;

					glVertex2f( offset_x + x, offset_y + y );
				}

				glEnd( );
			}	
		}
	}

	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glEnable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );
	glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
}

void StudioModel :: DrawModelUVMap( void )
{
	if( !m_pstudiohdr ) return;

	// draw UV from all the bodyparts and skinfamilies
	for( int i = 0; i < m_pstudiohdr->numbodyparts; i++ ) 
	{
		mstudiobodyparts_t   *pbodypart = (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex) + i;

		for( int j = 0; j < pbodypart->nummodels; j++ )
		{
			int index = j / pbodypart->base;
			index = index % pbodypart->nummodels;

			m_pmodel = (mstudiomodel_t *)((byte *)m_pstudiohdr + pbodypart->modelindex) + index;
			DrawUVMapPoints();
		}
	}
}

void StudioModel :: ConvertTexCoords( void )
{
	if( !m_pstudiohdr ) return;

	// draw UV from all the bodyparts and skinfamilies
	for( int i = 0; i < m_pstudiohdr->numbodyparts; i++ ) 
	{
		mstudiobodyparts_t   *pbodypart = (mstudiobodyparts_t *)((byte *)m_pstudiohdr + m_pstudiohdr->bodypartindex) + i;

		for( int j = 0; j < pbodypart->nummodels; j++ )
		{
			int index = j / pbodypart->base;
			index = index % pbodypart->nummodels;

			m_pmodel = (mstudiomodel_t *)((byte *)m_pstudiohdr + pbodypart->modelindex) + index;
			mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)m_ptexturehdr + m_ptexturehdr->textureindex);
			mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex);
			short *pskinref_src = (short *)((byte *)m_ptexturehdr + m_ptexturehdr->skinindex);

			for( int k = 0; k < m_ptexturehdr->numskinfamilies; k++ )
			{
				// try all the skinfamilies
				short *pskinref = pskinref_src + (k * m_ptexturehdr->numskinref);

				for( int m = 0; m < m_pmodel->nummesh; m++ ) 
				{
					short	*ptricmds;

					pmesh = (mstudiomesh_t *)((byte *)m_pstudiohdr + m_pmodel->meshindex) + m;
					ptricmds = (short *)((byte *)m_pstudiohdr + pmesh->triindex);

					while( int l = *( ptricmds++ ))
					{
						for( l = abs( l ); l > 0; l--, ptricmds += 4 )
						{
							if( ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_UV_COORDS )
							{
								ptricmds[2] = FloatToHalf((float)ptricmds[2] * (1.0f / 32768.0f));
								ptricmds[3] = FloatToHalf((float)ptricmds[3] * (1.0f / 32768.0f));
								g_viewerSettings.numModelChanges++;
							}
						}
					}	
				}
			}
		}
	}
}