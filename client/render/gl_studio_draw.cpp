/*
gl_studio_draw.cpp - rendering studio models
Copyright (C) 2019 Uncle Mike

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
#include "com_model.h"
#include "r_studioint.h"
#include "pm_movevars.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "event_api.h"
#include <mathlib.h>
#include "pm_defs.h"
#include "stringlib.h"
#include "triangleapi.h"
#include "entity_types.h"
#include "gl_shader.h"
#include "gl_world.h"
#include "gl_cvars.h"

#define LIGHT_INTERP_UPDATE	0.1f
#define LIGHT_INTERP_FACTOR	(1.0f / LIGHT_INTERP_UPDATE)

/*
=================
SortSolidMeshes

sort by shaders to reduce state switches
=================
*/
int CStudioModelRenderer :: SortSolidMeshes( const CSolidEntry *a, const CSolidEntry *b )
{
	if( a->m_hProgram > b->m_hProgram )
		return -1;
	if( a->m_hProgram < b->m_hProgram )
		return 1;

	return 0;
}

/*
================
StudioExtractBbox

Extract bbox from current sequence
================
*/
int CStudioModelRenderer :: StudioExtractBbox( studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs )
{
	if( !phdr || sequence < 0 || sequence >= phdr->numseq )
		return 0;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex) + sequence;
	mins = pseqdesc->bbmin;
	maxs = pseqdesc->bbmax;

	return 1;
}

/*
================
StudioGetBounds

Get bounds for a current sequence
================
*/
int CStudioModelRenderer :: StudioGetBounds( cl_entity_t *e, Vector bounds[2] )
{
	if( !e || e->modelhandle == INVALID_HANDLE )
		return 0;

	ModelInstance_t *inst = &m_ModelInstances[e->modelhandle];
	bounds[0] = inst->absmin;
	bounds[1] = inst->absmax;

	return 1;
}

/*
================
StudioGetBounds

Get bounds for a given mesh
================
*/
int CStudioModelRenderer :: StudioGetBounds( CSolidEntry *entry, Vector bounds[2] )
{
	if( !entry || entry->m_bDrawType != DRAWTYPE_MESH )
		return 0;

	if( !entry->m_pParentEntity || entry->m_pParentEntity->modelhandle == INVALID_HANDLE )
		return 0;

	vbomesh_t *vbo = entry->m_pMesh;

	if( !vbo || vbo->parentbone == 0xFF )
		return 0;

	ModelInstance_t *inst = &m_ModelInstances[entry->m_pParentEntity->modelhandle];
	CBoundingBox meshBounds = StudioGetMeshBounds(inst, vbo);
	bounds[0] = meshBounds.GetMins();
	bounds[1] = meshBounds.GetMaxs();
	return 1;
}

int CStudioModelRenderer::StudioGetBounds( CSolidEntry *entry, CBoundingBox &bounds )
{
	if( !entry || entry->m_bDrawType != DRAWTYPE_MESH )
		return 0;

	if( !entry->m_pParentEntity || entry->m_pParentEntity->modelhandle == INVALID_HANDLE )
		return 0;

	vbomesh_t *vbo = entry->m_pMesh;

	if( !vbo || vbo->parentbone == 0xFF )
		return 0;

	ModelInstance_t *inst = &m_ModelInstances[entry->m_pParentEntity->modelhandle];
	CBoundingBox meshBounds = StudioGetMeshBounds(inst, vbo);
	bounds = meshBounds;
	return 1;
}

/*
================
StudioGetBounds

Get world-space bounds for a given mesh, accounting all contained bones
================
*/
CBoundingBox CStudioModelRenderer::StudioGetMeshBounds(ModelInstance_t *inst, const vbomesh_t *mesh)
{
	Vector mins, maxs;
	bool boneWeighting = inst->m_pModel->poseToBone != nullptr;
	const mposetobone_t	*m = inst->m_pModel->poseToBone;

	ClearBounds(mins, maxs);
	for (const auto &[boneIndex, bound] : mesh->boneBounds) 
	{
		vec3_t worldSpaceMins, worldSpaceMaxs;
		matrix3x4 out = inst->m_pbones[boneIndex];
		if (boneWeighting) {
			out.ConcatTransforms(m->posetobone[boneIndex]);
		}
		TransformAABB(out, bound.GetMins(), bound.GetMaxs(), worldSpaceMins, worldSpaceMaxs);
		AddPointToBounds(worldSpaceMins, mins, maxs);
		AddPointToBounds(worldSpaceMaxs, mins, maxs);
	}
	ExpandBounds(mins, maxs, 0.5f); // create sentinel border for refractions
	return CBoundingBox(mins, maxs);
}

/*
================
UpdateLatchedVars

lerp custom values
================
*/
void CStudioModelRenderer :: UpdateLatchedVars( cl_entity_t *e, qboolean reset )
{
	if( !g_fRenderInitialized ) return;

	if( !StudioSetEntity( e )) return;

	// store old values to right lerping from
	m_pModelInstance->m_oldposeparams[0] = e->prevstate.vuser1[0];
	m_pModelInstance->m_oldposeparams[1] = e->prevstate.vuser1[1];
	m_pModelInstance->m_oldposeparams[2] = e->prevstate.vuser1[2];

	m_pModelInstance->m_oldposeparams[3] = e->prevstate.vuser2[0];
	m_pModelInstance->m_oldposeparams[4] = e->prevstate.vuser2[1];
	m_pModelInstance->m_oldposeparams[5] = e->prevstate.vuser2[2];

	m_pModelInstance->m_oldposeparams[6] = e->prevstate.vuser3[0];
	m_pModelInstance->m_oldposeparams[7] = e->prevstate.vuser3[1];
	m_pModelInstance->m_oldposeparams[8] = e->prevstate.vuser3[2];

	m_pModelInstance->m_oldposeparams[ 9] = e->prevstate.vuser4[0];
	m_pModelInstance->m_oldposeparams[10] = e->prevstate.vuser4[1];
	m_pModelInstance->m_oldposeparams[11] = e->prevstate.vuser4[2];
}

/*
================
StudioComputeBBox

Compute a full bounding box for current sequence
================
*/
int CStudioModelRenderer :: StudioComputeBBox( void )
{
	Vector p1, p2, scale = Vector( 1.0f, 1.0f, 1.0f );
	int sequence = RI->currententity->curstate.sequence;
	Vector origin, mins, maxs;

	if( FBitSet( m_pModelInstance->info_flags, MF_STATIC_BOUNDS ))
		return true; // bounds already computed

	if( !StudioExtractBbox( m_pStudioHeader, sequence, mins, maxs ))
		return false;

	if (m_pModelInstance->m_bExternalBones)
	{
		mins = RI->currententity->curstate.mins;
		maxs = RI->currententity->curstate.maxs;
	}

	// FIXME: some problems in studiomdl compiler?
	if( BoundsIsCleared( mins, maxs ) || BoundsIsNull( mins, maxs ))
	{
		mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;
		ALERT( at_aiconsole, "%s: sequence: %s has invalid bbox\n", RI->currentmodel->name, pseqdesc->label );
		// some seqeunces can have invalid bbox size, so we need to start from default size
		AddPointToBounds( RI->currentmodel->mins, mins, maxs );
		AddPointToBounds( RI->currentmodel->maxs, mins, maxs );
	}

	// prevent to compute env_static bounds every frame
	if( FBitSet( RI->currententity->curstate.iuser1, CF_STATIC_ENTITY ) )
		SetBits( m_pModelInstance->info_flags, MF_STATIC_BOUNDS );

	if( FBitSet( RI->currententity->curstate.iuser1, CF_STATIC_ENTITY ))
	{
		if( RI->currententity->curstate.startpos != g_vecZero )
			scale = RI->currententity->curstate.startpos;
	}
	else if( RI->currententity->curstate.scale > 0.0f && RI->currententity->curstate.scale <= 16.0f )
	{
		// apply studiomodel scale (clamp scale to prevent too big sizes on some HL maps)
		scale = Vector( RI->currententity->curstate.scale );
	}

	Vector angles = StudioGetAngles();

	// don't rotate player model, only aim
	if( RI->currententity->player ) 
		angles[PITCH] = 0;

	origin = StudioGetOrigin();

	matrix3x4 transform = matrix3x4( g_vecZero, angles, scale );

	// rotate and scale bbox for env_static
	TransformAABB( transform, mins, maxs, mins, maxs );

	if( m_pModelInstance->origin != origin )
		SetBits( m_pModelInstance->info_flags, MF_POSITION_CHANGED );

	// compute abs box
	m_pModelInstance->absmin = mins + origin;
	m_pModelInstance->absmax = maxs + origin;
	m_pModelInstance->radius = RadiusFromBounds( mins, maxs );
	m_pModelInstance->origin = origin;

	return true;
}

/*
====================
AddBlendSequence

multiple blends
====================
*/
void CStudioModelRenderer :: AddBlendSequence( int oldseq, int newseq, float prevframe, bool gaitseq )
{
	mstudioseqdesc_t *poldseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + oldseq;
	mstudioseqdesc_t *pnewseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + newseq;

	// sequence has changed, hold the previous sequence info
	if( oldseq != newseq && !FBitSet( pnewseqdesc->flags, STUDIO_SNAP ))
	{
		mstudioblendseq_t	*pseqblending;

		// move current sequence into circular buffer
		m_pModelInstance->m_current_seqblend = (m_pModelInstance->m_current_seqblend + 1) & MASK_SEQBLENDS;

		pseqblending = &m_pModelInstance->m_seqblend[m_pModelInstance->m_current_seqblend];

		pseqblending->blendtime = tr.time;
		pseqblending->sequence = oldseq;
		pseqblending->cycle = prevframe / m_boneSetup.LocalMaxFrame( oldseq );
		pseqblending->gaitseq = gaitseq;
		pseqblending->fadeout = Q_min( poldseqdesc->fadeouttime / 100.0f, pnewseqdesc->fadeintime / 100.0f );
		if( pseqblending->fadeout <= 0.0f )
			pseqblending->fadeout = 0.2f; // force to default
	}
}

/*
====================
CalcStairSmoothValue

smoothing stepsize height
====================
*/
float CStudioModelRenderer :: CalcStairSmoothValue( float oldz, float newz, float smoothtime, float smoothvalue )
{
	if( oldz < newz )
		return bound( newz - tr.movevars->stepsize, oldz + smoothtime * smoothvalue, newz );
	if( oldz > newz )
		return bound( newz, oldz - smoothtime * smoothvalue, newz + tr.movevars->stepsize );
	return 0.0f;
}

/*
====================
StudioCheckLOD

check special bodygroup
====================
*/
int CStudioModelRenderer :: StudioCheckLOD( void )
{
	mstudiobodyparts_t    *m_pBodyPart;
    
	for( int i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;
        
		if( !Q_stricmp( m_pBodyPart->name, "studioLOD" ))
			return m_pBodyPart->nummodels;
	}

	return 0; // no lod-levels for this model
}

const Vector CStudioModelRenderer::StudioGetOrigin()
{
	return RI->currententity->origin;
}

const Vector CStudioModelRenderer::StudioGetAngles()
{
	return RI->currententity->angles;
}

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer :: StudioSetUpTransform( void )
{
	cl_entity_t	*e = RI->currententity;
	bool		disable_smooth = false;
	float		step, smoothtime;
	int		numLods;

	if( !m_fShootDecal && RI->currententity->curstate.renderfx != kRenderFxDeadPlayer )
	{
		if (m_iDrawModelType == DRAWSTUDIO_RUNEVENTS)
			e = gEngfuncs.GetLocalPlayer();

		// calculate how much time has passed since the last V_CalcRefdef
		smoothtime = bound( 0.0f, tr.time - m_pModelInstance->lerp.stairtime, 0.1f );
		m_pModelInstance->lerp.stairtime = tr.time;

		if( e->curstate.onground == -1 || FBitSet( e->curstate.effects, EF_NOINTERP ))
			disable_smooth = true;

		if( e->curstate.movetype != MOVETYPE_STEP && e->curstate.movetype != MOVETYPE_WALK )
			disable_smooth = true;

		if( !FBitSet( m_pModelInstance->info_flags, MF_INIT_SMOOTHSTAIRS ))
		{
			SetBits( m_pModelInstance->info_flags, MF_INIT_SMOOTHSTAIRS );
			disable_smooth = true;
		}

		if( disable_smooth )
		{
			m_pModelInstance->lerp.stairoldz = e->origin[2];
		}
		else
		{
			step = CalcStairSmoothValue( m_pModelInstance->lerp.stairoldz, e->origin[2], smoothtime, STAIR_INTERP_TIME );
			if( step ) m_pModelInstance->lerp.stairoldz = e->origin[2] = step;
		}
		e = RI->currententity;
	}

	Vector origin = StudioGetOrigin();
	Vector angles = StudioGetAngles();
	Vector scale = Vector( 1.0f, 1.0f, 1.0f );

	// apply lodnum to model
	if(( numLods = StudioCheckLOD( )) != 0 )
	{
		float  radius = Q_max( m_pModelInstance->radius, 1.0f ); // to avoid division by zero
#if 1
		float screenSize = ComputePixelWidthOfSphere( origin, 0.5f );
		float lodMetric = screenSize != 0.0f ? (100.0f / screenSize) : 0.0f;
		int lodnum = (int)( lodMetric / radius * 2.0f );
#else
		float lodDist = (origin - RI->view.origin).Length() * RI->view.lodScale;
		int lodnum = (int)( lodDist / radius );
		if( CVAR_TO_BOOL( m_pCvarLodScale ))
			lodnum /= (int)fabs( m_pCvarLodScale->value );
#endif
		if( CVAR_TO_BOOL( m_pCvarLodBias ))
			lodnum += (int)fabs( m_pCvarLodBias->value );

		// set derived LOD
		e->curstate.body = Q_min( lodnum, numLods - 1 );
	}

	if( e->curstate.renderfx != kRenderFxDeadPlayer )
	{
		int	gaitsequence = 0;

		// don't rotate clients, only aim
		if( e->player ) angles[PITCH] = 0.0f;

		if( e->curstate.gaitsequence != m_pModelInstance->lerp.gaitsequence )
		{
			AddBlendSequence( m_pModelInstance->lerp.gaitsequence, e->curstate.gaitsequence, m_pModelInstance->lerp.gaitframe, true );
			m_pModelInstance->lerp.gaitsequence = e->curstate.gaitsequence;
		}
	}

	if( FBitSet( RI->currententity->curstate.effects, EF_NOINTERP ) || ( tr.realframecount - m_pModelInstance->cached_frame ) > 1 )
	{
		m_pModelInstance->lerp.sequence = RI->currententity->curstate.sequence;
	}
	else if( RI->currententity->curstate.sequence != m_pModelInstance->lerp.sequence )
	{
		AddBlendSequence( m_pModelInstance->lerp.sequence, RI->currententity->curstate.sequence, m_pModelInstance->lerp.frame );
		m_pModelInstance->lerp.sequence = RI->currententity->curstate.sequence;
	}

	// store prevseqblending manually, engine doesn't it
	e->latched.prevseqblending[0] = e->curstate.blending[0];
	e->latched.prevseqblending[1] = e->curstate.blending[1];

	// don't blend sequences for a dead player or a viewmodel, faceprotect
	if( m_iDrawModelType > DRAWSTUDIO_NORMAL || RI->currententity->curstate.renderfx == kRenderFxDeadPlayer )
		memset( &m_pModelInstance->m_seqblend, 0, sizeof( m_pModelInstance->m_seqblend ));

	if( RI->currententity->curstate.iuser1 & CF_STATIC_ENTITY )
	{
		if( RI->currententity->curstate.startpos != g_vecZero )
			scale = RI->currententity->curstate.startpos;
	}
	else if( RI->currententity->curstate.scale > 0.0f && RI->currententity->curstate.scale <= 16.0f )
	{
		// apply studiomodel scale (clamp scale to prevent too big sizes on some HL maps)
		scale = Vector( RI->currententity->curstate.scale, RI->currententity->curstate.scale, RI->currententity->curstate.scale );
	}

	// build the rotation matrix
	m_pModelInstance->m_protationmatrix = matrix3x4( origin, angles, scale );

	if( RI->currententity == GET_VIEWMODEL() && CVAR_TO_BOOL( m_pCvarHand ))
	{
		// inverse the right vector
		m_pModelInstance->m_protationmatrix.SetRight( -m_pModelInstance->m_protationmatrix.GetRight() );
	}

	StudioFxTransform( e, m_pModelInstance->m_protationmatrix );
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer :: StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	int numframes = m_boneSetup.LocalMaxFrame( RI->currententity->curstate.sequence );
	double fps = m_boneSetup.LocalFPS( RI->currententity->curstate.sequence );
	double dfdt = 0, f = 0;

	if( !m_fShootDecal && tr.time >= RI->currententity->curstate.animtime )
		dfdt = (tr.time - RI->currententity->curstate.animtime) * RI->currententity->curstate.framerate * fps;

	if( numframes > 1 )
		f = (RI->currententity->curstate.frame * numframes) / 256.0;

	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( numframes > 1 )
		{
			f -= (int)(f / numframes) * numframes;
		}

		if( f < 0.0 ) 
		{
			f += numframes;
		}
	}
	else 
	{
		if( f >= numframes ) 
		{
			f = numframes;
		}

		if( f < 0.0 ) 
		{
			f = 0.0;
		}
	}

	return f;
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer :: StudioEstimateGaitFrame( mstudioseqdesc_t *pseqdesc )
{
	double f;
	cl_entity_t *e = RI->currententity;
	int numframes = m_boneSetup.LocalMaxFrame( e->curstate.gaitsequence );
	float t = StudioEstimateInterpolant();
	float currGaitFrame = e->curstate.fuser1;
	float &lastGaitFrame = m_pModelInstance->lerp.lastgaitframe;
	float &prevGaitFrame = m_pModelInstance->lerp.prevgaitframe;

	if (numframes > 1)
	{
		if (fabsf(currGaitFrame - lastGaitFrame) > 0.001) {
			prevGaitFrame = lastGaitFrame;
		}

		float normCurrFrame = currGaitFrame / (float)numframes;
		float normPrevFrame = prevGaitFrame / (float)numframes;
		f = LoopingLerp(t, normPrevFrame, normCurrFrame) * numframes;
		lastGaitFrame = currGaitFrame;
	}
	else {
		f = 0.0;
	}

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( numframes > 1 )
		{
			f -= (int)(f / numframes) * numframes;
		}

		if( f < 0.0 ) 
		{
			f += numframes;
		}
	}
	else 
	{
		if( f >= numframes - 1.001 ) 
		{
			f = numframes - 1.001;
		}

		if( f < 0.0 ) 
		{
			f = 0.0;
		}
	}

	return f;
}

/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer :: StudioEstimateInterpolant()
{
	cl_entity_t *e = RI->currententity;
	double dadt = 1.0;
	const double interval = 0.1; // animtime update interval is always fixed and equals 0.1 sec

	if (!m_fShootDecal && (e->curstate.animtime >= e->latched.prevanimtime + 0.01f))
	{
		dadt = (tr.time - e->curstate.animtime) / interval;
		dadt = bound(0.0, dadt, 2.0); // allow value to be above 1.0 for extrapolation in case of update didn't came in time
	}
	return static_cast<float>(dadt);
}

/*
====================
StudioInterpolatePoseParams

====================
*/
void CStudioModelRenderer :: StudioInterpolatePoseParams( cl_entity_t *e, float dadt )
{
	if( !m_boneSetup.CountPoseParameters( ))
	{
		// interpolate blends
		m_pModelInstance->m_poseparameter[0] = (e->curstate.blending[0] * dadt + e->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;
		m_pModelInstance->m_poseparameter[1] = (e->curstate.blending[1] * dadt + e->latched.prevblending[1] * (1.0f - dadt)) / 255.0f;
	}
	else
	{
		m_pModelInstance->m_poseparameter[0] = (e->curstate.vuser1[0] * dadt + m_pModelInstance->m_oldposeparams[0] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[1] = (e->curstate.vuser1[1] * dadt + m_pModelInstance->m_oldposeparams[1] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[2] = (e->curstate.vuser1[2] * dadt + m_pModelInstance->m_oldposeparams[2] * (1.0f - dadt));

		m_pModelInstance->m_poseparameter[3] = (e->curstate.vuser2[0] * dadt + m_pModelInstance->m_oldposeparams[3] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[4] = (e->curstate.vuser2[1] * dadt + m_pModelInstance->m_oldposeparams[4] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[5] = (e->curstate.vuser2[2] * dadt + m_pModelInstance->m_oldposeparams[5] * (1.0f - dadt));

		m_pModelInstance->m_poseparameter[6] = (e->curstate.vuser3[0] * dadt + m_pModelInstance->m_oldposeparams[6] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[7] = (e->curstate.vuser3[1] * dadt + m_pModelInstance->m_oldposeparams[7] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[8] = (e->curstate.vuser3[2] * dadt + m_pModelInstance->m_oldposeparams[8] * (1.0f - dadt));

		m_pModelInstance->m_poseparameter[ 9] = (e->curstate.vuser4[0] * dadt + m_pModelInstance->m_oldposeparams[ 9] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[10] = (e->curstate.vuser4[1] * dadt + m_pModelInstance->m_oldposeparams[10] * (1.0f - dadt));
		m_pModelInstance->m_poseparameter[11] = (e->curstate.vuser4[2] * dadt + m_pModelInstance->m_oldposeparams[11] * (1.0f - dadt));
	}
}

/*
====================
StudioInterpolateControllers

====================
*/
void CStudioModelRenderer :: StudioInterpolateControllers( cl_entity_t *e, float dadt )
{
	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	// interpolate controllers
	for( int j = 0; j < m_pStudioHeader->numbonecontrollers; j++ )
	{
		int i = pbonecontroller[j].index;
		float value;

		if( i <= 3 )
		{
			// check for 360% wrapping
			if( FBitSet( pbonecontroller[j].type, STUDIO_RLOOP ))
			{
				if( abs( e->curstate.controller[i] - e->latched.prevcontroller[i] ) > 128 )
				{
					int a = (e->curstate.controller[j] + 128) % 256;
					int b = (e->latched.prevcontroller[j] + 128) % 256;
					value = ((a * dadt) + (b * (1.0f - dadt)) - 128);
				}
				else 
				{
					value = ((e->curstate.controller[i] * dadt + (e->latched.prevcontroller[i]) * (1.0f - dadt)));
				}
			}
			else 
			{
				value = (e->curstate.controller[i] * dadt + e->latched.prevcontroller[i] * (1.0 - dadt));
			}
			m_pModelInstance->m_controller[i] = bound( 0, Q_rint( value ), 255 );
		}
	}
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t *CStudioModelRenderer :: StudioGetAnim( model_t *pModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t *pseqgroup;
	cache_user_t *paSequences;

	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	paSequences = (cache_user_t *)pModel->submodels;

	if( paSequences == NULL )
	{
		paSequences = (cache_user_t *)IEngineStudio.Mem_Calloc( MAXSTUDIOGROUPS, sizeof( cache_user_t ));
		pModel->submodels = (dmodel_t *)paSequences;
	}

	// check for already loaded
	if( !IEngineStudio.Cache_Check(( struct cache_user_s *)&(paSequences[pseqdesc->seqgroup] )))
	{
		char filepath[128], modelpath[128], modelname[64];

		COM_FileBase( pModel->name, modelname );
		COM_ExtractFilePath( pModel->name, modelpath );

		// NOTE: here we build real sub-animation filename because stupid user may rename model without recompile
		Q_snprintf( filepath, sizeof( filepath ), "%s/%s%i%i.mdl", modelpath, modelname, pseqdesc->seqgroup / 10, pseqdesc->seqgroup % 10 );

		ALERT( at_console, "loading: %s\n", filepath );
		IEngineStudio.LoadCacheFile( filepath, (struct cache_user_s *)&paSequences[pseqdesc->seqgroup] );			
	}

	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
Studio_FxTransform

====================
*/
void CStudioModelRenderer :: StudioFxTransform( cl_entity_t *ent, matrix3x4 &transform )
{
	switch( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if( RANDOM_LONG( 0, 49 ) == 0 )
		{
			// choose between x & z
			switch( RANDOM_LONG( 0, 1 ))
			{
			case 0:
				transform.SetForward( transform.GetForward() * RANDOM_FLOAT( 1.0f, 1.484f ));
				break; 
			case 1:
				transform.SetUp( transform.GetUp() * RANDOM_FLOAT( 1.0f, 1.484f ));
				break;
			}
		}
		else if( RANDOM_LONG( 0, 49 ) == 0 )
		{
			transform[3][RANDOM_LONG( 0, 2 )] += RANDOM_FLOAT( -10.0f, 10.0f );
		}
		break;
	case kRenderFxExplode:
		{
			float scale = 1.0f + ( tr.time - ent->curstate.animtime ) * 10.0f;
			if( scale > 2 ) scale = 2; // don't blow up more than 200%
			transform.SetRight( transform.GetRight() * scale );
		}
		break;
	}
}

void CStudioModelRenderer :: BlendSequence( Vector pos[], Vector4D q[], mstudioblendseq_t *pseqblend )
{
	float m_flGaitBoneWeights[MAXSTUDIOBONES];
	CIKContext *pIK = NULL;

	// to prevent division by zero
	if( pseqblend->fadeout <= 0.0f )
		pseqblend->fadeout = 0.2f;

	if( m_boneSetup.GetNumIKChains( ))
		pIK = &m_pModelInstance->m_ik;

	if( pseqblend->blendtime && ( pseqblend->blendtime + pseqblend->fadeout > tr.time ) && ( pseqblend->sequence < m_pStudioHeader->numseq ))
	{
		float	s = 1.0f - (tr.time - pseqblend->blendtime) / pseqblend->fadeout;

		if( s > 0 && s <= 1.0 )
		{
			// do a nice spline curve
			s = 3.0f * s * s - 2.0f * s * s * s;
		}
		else if( s > 1.0f )
		{
			// Shouldn't happen, but maybe curtime is behind animtime?
			s = 1.0f;
		}

		if( pseqblend->gaitseq )
		{
			mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
			bool copy = true;

			for( int i = 0; i < m_pStudioHeader->numbones; i++)
			{
				if( !Q_strcmp( pbones[i].name, "Bip01 Spine" ))
					copy = false;
				else if( !Q_strcmp( pbones[pbones[i].parent].name, "Bip01 Pelvis" ))
					copy = true;
				m_flGaitBoneWeights[i] = (copy) ? 1.0f : 0.0f;
			}

			m_boneSetup.SetBoneWeights( m_flGaitBoneWeights ); // install weightlist for gait sequence
		}

		m_boneSetup.AccumulatePose( pIK, pos, q, pseqblend->sequence, pseqblend->cycle, s );
		m_boneSetup.SetBoneWeights( NULL ); // back to default rules
	}
}

//-----------------------------------------------------------------------------
// Purpose: update latched IK contacts if they're in a moving reference frame.
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: UpdateIKLocks( CIKContext *pIK )
{
	if( !pIK ) return;

	int targetCount = pIK->m_target.Count();

	if( targetCount == 0 )
		return;

	for( int i = 0; i < targetCount; i++ )
	{
		CIKTarget *pTarget = &pIK->m_target[i];

		if( !pTarget->IsActive( ))
			continue;

		if( pTarget->GetOwner() != -1 )
		{
			cl_entity_t *pOwner = GET_ENTITY( pTarget->GetOwner() );

			if( pOwner != NULL )
			{
				pTarget->UpdateOwner( pOwner->index, pOwner->origin, pOwner->angles );
			}				
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find the ground or external attachment points needed by IK rules
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: CalculateIKLocks( CIKContext *pIK )
{
	if( !pIK ) return;

	int targetCount = pIK->m_target.Count();

	if( targetCount == 0 )
		return;

	Vector up = Vector( 0.0f, 0.0f, 1.0f );
	// FIXME: check number of slots?
	float minHeight = FLT_MAX;
	float maxHeight = -FLT_MAX;

	for( int i = 0, j = 0; i < targetCount; i++ )
	{
		pmtrace_t *trace;
		CIKTarget *pTarget = &pIK->m_target[i];
		float flDist = pTarget->est.radius;

		if( !pTarget->IsActive( ))
			continue;

		switch( pTarget->type )
		{
		case IK_GROUND:
			{
				Vector estGround;
				Vector p1, p2;

				// adjust ground to original ground position
				estGround = (pTarget->est.pos - StudioGetOrigin());
				estGround = estGround - (estGround * up) * up;
				estGround = StudioGetOrigin() + estGround + pTarget->est.floor * up;

				p1 = estGround + up * pTarget->est.height;
				p2 = estGround - up * pTarget->est.height;
				float r = Q_max( pTarget->est.radius, 1 );

				Vector mins = Vector( -r, -r, 0.0f );
				Vector maxs = Vector(  r,  r, r * 2.0f );

				// don't IK to other characters
				gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
				gEngfuncs.pEventAPI->EV_PushTraceBounds( 2, mins, maxs );
				trace = gEngfuncs.pEventAPI->EV_VisTraceLine( p1, p2, PM_STUDIO_IGNORE );
				physent_t *ve = gEngfuncs.pEventAPI->EV_GetVisent( trace->ent );
				cl_entity_t *m_pGround = (ve) ? GET_ENTITY( ve->info ) : NULL;
				gEngfuncs.pEventAPI->EV_PopTraceBounds();

				if( m_pGround != NULL && m_pGround->curstate.movetype == MOVETYPE_PUSH )
				{
					pTarget->SetOwner( m_pGround->index, m_pGround->origin, m_pGround->angles );
				}
				else
				{
					pTarget->ClearOwner();
				}

				if( trace->startsolid )
				{
					// trace from back towards hip
					Vector tmp = (estGround - pTarget->trace.closest).Normalize();

					p1 = estGround - tmp * pTarget->est.height;
					p2 = estGround;
					mins = Vector( -r, -r, 0.0f );
					maxs = Vector(  r,  r, 1.0f );

					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PushTraceBounds( 2, mins, maxs );
					trace = gEngfuncs.pEventAPI->EV_VisTraceLine( p1, p2, PM_STUDIO_IGNORE );
					ve = gEngfuncs.pEventAPI->EV_GetVisent( trace->ent );
					m_pGround = (ve) ? GET_ENTITY( ve->info ) : NULL;
					gEngfuncs.pEventAPI->EV_PopTraceBounds();

					if( !trace->startsolid )
					{
						p1 = trace->endpos;
						p2 = p1 - up * pTarget->est.height;

						gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
						trace = gEngfuncs.pEventAPI->EV_VisTraceLine( p1, p2, PM_STUDIO_IGNORE );
						ve = gEngfuncs.pEventAPI->EV_GetVisent( trace->ent );
						m_pGround = (ve) ? GET_ENTITY( ve->info ) : NULL;
					}
				}

				if( !trace->startsolid )
				{
					if( m_pGround == GET_ENTITY( 0 ))
					{
						// clamp normal to 33 degrees
						const float limit = 0.832;
						float dot = DotProduct( trace->plane.normal, up );

						if( dot < limit )
						{
							ASSERT( dot >= 0 );
							// subtract out up component
							Vector diff = trace->plane.normal - up * dot;
							// scale remainder such that it and the up vector are a unit vector
							float d = sqrt(( 1.0f - limit * limit ) / DotProduct( diff, diff ) );
							trace->plane.normal = up * limit + d * diff;
						}

						// FIXME: this is wrong with respect to contact position and actual ankle offset
						pTarget->SetPosWithNormalOffset( trace->endpos, trace->plane.normal );
						pTarget->SetNormal( trace->plane.normal );
						pTarget->SetOnWorld( true );

						// only do this on forward tracking or commited IK ground rules
						if( pTarget->est.release < 0.1f )
						{
							// keep track of ground height
							float offset = DotProduct( pTarget->est.pos, up );

							if( minHeight > offset )
								minHeight = offset;
							if( maxHeight < offset )
								maxHeight = offset;
						}
						// FIXME: if we don't drop legs, running down hills looks horrible
						/*
						if (DotProduct( pTarget->est.pos, up ) < DotProduct( estGround, up ))
						{
							pTarget->est.pos = estGround;
						}
						*/
					}
					else if( m_pGround != NULL )
					{
						pTarget->SetPos( trace->endpos );
						pTarget->SetAngles( StudioGetAngles() );

						// only do this on forward tracking or commited IK ground rules
						if( pTarget->est.release < 0.1f )
						{
							float offset = DotProduct( pTarget->est.pos, up );

							if( minHeight > offset )
								minHeight = offset;

							if( maxHeight < offset )
								maxHeight = offset;
						}
						// FIXME: if we don't drop legs, running down hills looks horrible
						/*
						if (DotProduct( pTarget->est.pos, up ) < DotProduct( estGround, up ))
						{
							pTarget->est.pos = estGround;
						}
						*/
					}
					else
					{
						pTarget->IKFailed();
					}
				}
				else
				{
					if( m_pGround != GET_ENTITY( 0 ))
					{
						pTarget->IKFailed( );
					}
					else
					{
						pTarget->SetPos( trace->endpos );
						pTarget->SetAngles( StudioGetAngles() );
						pTarget->SetOnWorld( true );
					}
				}
			}
			break;
		case IK_ATTACHMENT:
			flDist = pTarget->est.radius;
			for( j = 1; j < RENDER_GET_PARM( PARM_MAX_ENTITIES, 0 ); j++ )
			{
				cl_entity_t *m_pEntity = GET_ENTITY( j );
				float flRadius = 4096.0f; // (64.0f * 64.0f)

				if( !m_pEntity || m_pEntity->modelhandle == INVALID_HANDLE )
					continue;	// not a studiomodel or not in PVS

				ModelInstance_t *inst = &m_ModelInstances[m_pEntity->modelhandle];
				float distSquared = 0.0f, eorg;

				for( int k = 0; k < 3 && distSquared <= flRadius; k++ )
				{
					if( pTarget->est.pos[j] < inst->absmin[j] )
						eorg = pTarget->est.pos[j] - inst->absmin[j];
					else if( pTarget->est.pos[j] > inst->absmax[j] )
						eorg = pTarget->est.pos[j] - inst->absmax[j];
					else eorg = 0.0f;

					distSquared += eorg * eorg;
				}

				if( distSquared >= flRadius )
					continue;	// not in radius

				// Extract the bone index from the name
				if( pTarget->offset.attachmentIndex >= inst->numattachments )
					continue;

				// FIXME: how to validate a index?
				Vector origin = inst->attachment[pTarget->offset.attachmentIndex].origin;
				Vector angles = inst->attachment[pTarget->offset.attachmentIndex].angles;
				float d = (pTarget->est.pos - origin).Length();

				if( d >= flDist )
					continue;
				flDist = d;
				pTarget->SetPos( origin );
				pTarget->SetAngles( angles );
			}

			if( flDist >= pTarget->est.radius )
			{
				// no solution, disable ik rule
				pTarget->IKFailed( );
			}
			break;
		}
	}
}

/*
================
CheckBoneCache

validate cache
================
*/
bool CStudioModelRenderer :: CheckBoneCache( float f )
{
	if( !m_pModelInstance )
		return false;

	ModelInstance_t *inst = m_pModelInstance;	
	BoneCache_t *cache = &inst->bonecache;
	cl_entity_t *e = RI->currententity;

	if(( tr.time - inst->lighttimecheck ) > LIGHT_INTERP_UPDATE )
	{
		// shuffle states
		inst->oldlight = inst->newlight;
		inst->lighttimecheck = tr.time;
		inst->light_update = true;
	}

	// can't use cache if jiggle bones enabled, because it need to be updated every frame
	if (inst->m_pJiggleBones) {
		return false;
	}

	// external bones also need to be updated every frame
	if (m_pModelInstance->m_bExternalBones) {
		return false; 
	}

	// no cache for local player
	if( RP_LOCALCLIENT( RI->currententity ) && !FBitSet( RI->params, RP_THIRDPERSON ))
		return false;

	bool pos_valid = (cache->transform == inst->m_protationmatrix) ? true : false;
	bool param_valid = !memcmp( cache->poseparam, inst->m_poseparameter, sizeof( float ) * MAXSTUDIOPOSEPARAM );

	// make sure what all cached values are unchanged
	if( cache->frame == f && cache->sequence == e->curstate.sequence && pos_valid && !memcmp( cache->blending, e->curstate.blending, 2 )
	&& !memcmp( cache->controller, e->curstate.controller, 4 ) && cache->mouthopen == e->mouth.mouthopen && param_valid )
	{
		if( cache->gaitsequence == e->curstate.gaitsequence && cache->gaitframe == e->curstate.fuser1 )
			return true;
	}

	// time to check cubemaps
	if( cache->transform.GetOrigin() != inst->m_protationmatrix.GetOrigin() )
	{
		// search for center of bbox
		Vector pos = ( inst->absmin + inst->absmax ) * 0.5f;
		CL_FindTwoNearestCubeMap( pos, &inst->cubemap[0], &inst->cubemap[1] );

		float dist0 = ( inst->cubemap[0]->origin - pos ).Length();
		float dist1 = ( inst->cubemap[1]->origin - pos ).Length();
		inst->lerpFactor = dist0 / (dist0 + dist1);
	}

	// save rotationmatrix to GL-style array
	inst->m_protationmatrix.CopyToArray( inst->m_glmatrix );

	// update bonecache
	cache->frame = f;
	cache->mouthopen = e->mouth.mouthopen;
	cache->sequence = e->curstate.sequence;
	cache->transform = inst->m_protationmatrix;
	memcpy( cache->blending, e->curstate.blending, 2 );
	memcpy( cache->controller, e->curstate.controller, 4 );
	memcpy( cache->poseparam, inst->m_poseparameter, sizeof( float ) * MAXSTUDIOPOSEPARAM );
	cache->gaitsequence = e->curstate.gaitsequence;
	cache->gaitframe = e->curstate.fuser1;

	return false;
}

void CStudioModelRenderer :: StudioSetBonesExternal( const cl_entity_t *ent, const Vector pos[], const Radian ang[] )
{
	RI->currententity = (cl_entity_t *)ent;
	RI->currentmodel = ent->model;

	if (!RI->currentmodel)
		return;

	// setup global pointers
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata(RI->currentmodel);

	// aliasmodel instead of studio?
	if (!m_pStudioHeader)
		return;

	if (RI->currententity->modelhandle == INVALID_HANDLE)
		return; // out of memory ?

	m_pModelInstance = &m_ModelInstances[RI->currententity->modelhandle];

	for (int i = 0; i < m_pStudioHeader->numbones; i++)
	{
		m_pModelInstance->m_externalBonesAngles[i] = ang[i];
		m_pModelInstance->m_externalBonesOrigin[i] = pos[i];
	}

	m_pModelInstance->m_flLastExternalBonesUpdate = tr.time + 0.1f;
	m_pModelInstance->m_bExternalBones = true;
}

void CStudioModelRenderer :: StudioCalcBonesExternal( Vector pos[], Vector4D q[] )
{
	if (!m_pModelInstance->m_bExternalBones)
		return;

	for (int i = 0; i < m_pStudioHeader->numbones; i++)
	{
		q[i] = m_pModelInstance->m_externalBonesAngles[i];
		pos[i] = m_pModelInstance->m_externalBonesOrigin[i];
	}

	// update is expired, reset extenal bones status
	if (tr.time > m_pModelInstance->m_flLastExternalBonesUpdate) {
		m_pModelInstance->m_bExternalBones = false;
	}
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer :: StudioSetupBones( void )
{
	float		adj[MAXSTUDIOCONTROLLERS];
	cl_entity_t	*e = RI->currententity;	// for more readability
	CIKContext	*pIK = NULL;
	mstudioboneinfo_t	*pboneinfo;
	mstudioseqdesc_t	*pseqdesc;
	matrix3x4		bonematrix;
	mstudiobone_t	*pbones;

	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];

	if( e->curstate.sequence < 0 || e->curstate.sequence >= m_pStudioHeader->numseq ) 
	{
		int sequence = (short)e->curstate.sequence;
		ALERT( at_aiconsole, "StudioSetupBones: sequence %i/%i out of range for model %s\n",
		sequence, m_pStudioHeader->numseq, RI->currentmodel->name );
		e->curstate.sequence = 0;
    }

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.sequence;
	float f = StudioEstimateFrame( pseqdesc );
	float dadt = StudioEstimateInterpolant();
	
	StudioInterpolatePoseParams( e, dadt );

	if( CheckBoneCache( f )) 
		return; // using a cached bones no need transformations

	if( m_boneSetup.GetNumIKChains( ))
	{
		if( FBitSet( e->curstate.effects, EF_NOINTERP ))
			m_pModelInstance->m_ik.ClearTargets();
		m_pModelInstance->m_ik.Init( &m_boneSetup, StudioGetAngles(), StudioGetOrigin(), tr.time, tr.realframecount );
		pIK = &m_pModelInstance->m_ik;
	}

	float cycle = f / m_boneSetup.LocalMaxFrame( e->curstate.sequence );
	StudioInterpolateControllers( e, dadt );

	m_boneSetup.InitPose( pos, q );
	m_boneSetup.UpdateRealTime( tr.time );
	if( CVAR_TO_BOOL( m_pCvarCompatible ))
		m_boneSetup.CalcBoneAdj( adj, m_pModelInstance->m_controller, e->mouth.mouthopen );
	m_boneSetup.AccumulatePose( pIK, pos, q, e->curstate.sequence, cycle, 1.0 );
	m_pModelInstance->lerp.frame = f;

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	pboneinfo = (mstudioboneinfo_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex + m_pStudioHeader->numbones * sizeof( mstudiobone_t ));

	if( e->curstate.gaitsequence < 0 || e->curstate.gaitsequence >= m_pStudioHeader->numseq ) 
		e->curstate.gaitsequence = 0;

	// calc gait animation
	if( e->curstate.gaitsequence != 0 )
	{
		float m_flGaitBoneWeights[MAXSTUDIOBONES];
		bool copy = true;

		for( int i = 0; i < m_pStudioHeader->numbones; i++)
		{
			if( !Q_strcmp( pbones[i].name, "Bip01 Spine" ))
				copy = false;
			else if( !Q_strcmp( pbones[pbones[i].parent].name, "Bip01 Pelvis" ))
				copy = true;
			m_flGaitBoneWeights[i] = (copy) ? 1.0f : 0.0f;
		}

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.gaitsequence;
		f = StudioEstimateGaitFrame( pseqdesc );

		// convert gaitframe to cycle
		cycle = f / m_boneSetup.LocalMaxFrame( e->curstate.gaitsequence );

		m_boneSetup.SetBoneWeights( m_flGaitBoneWeights ); // install weightlist for gait sequence
		m_boneSetup.AccumulatePose( pIK, pos, q, e->curstate.gaitsequence, cycle, 1.0 );
		m_boneSetup.SetBoneWeights( NULL ); // back to default rules
		m_pModelInstance->lerp.gaitframe = f;
	}

	// run blends from previous sequences
	for( int i = 0; i < MAX_SEQBLENDS; i++ )
		BlendSequence( pos, q, &m_pModelInstance->m_seqblend[i] );

	CIKContext auto_ik;
	auto_ik.Init( &m_boneSetup, StudioGetAngles(), StudioGetOrigin(), 0.0f, 0 );
	m_boneSetup.CalcAutoplaySequences( &auto_ik, pos, q );
	if( !CVAR_TO_BOOL( m_pCvarCompatible ))
		m_boneSetup.CalcBoneAdj( pos, q, m_pModelInstance->m_controller, e->mouth.mouthopen );

	byte	boneComputed[MAXSTUDIOBONES];

	memset( boneComputed, 0, sizeof( boneComputed ));

	// don't calculate IK on ragdolls
	if( pIK != NULL )
	{
		UpdateIKLocks( pIK );
		pIK->UpdateTargets( pos, q, m_pModelInstance->m_pbones, boneComputed );
		CalculateIKLocks( pIK );
		pIK->SolveDependencies( pos, q, m_pModelInstance->m_pbones, boneComputed );
	}

	StudioCalcBonesExternal( pos, q );

	for( int i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		// animate all non-simulated bones
		if( CalcProceduralBone( m_pStudioHeader, i, m_pModelInstance->m_pbones ))
			continue;

		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( FBitSet( pbones[i].flags, BONE_JIGGLE_PROCEDURAL ) && FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEINFO ))
		{
			// Physics-based "jiggle" bone
			// Bone is assumed to be along the Z axis
			// Pitch around X, yaw around Y

			// compute desired bone orientation
			matrix3x4 goalMX;

			if (pbones[i].parent == -1) {
				goalMX = m_pModelInstance->m_protationmatrix.ConcatTransforms(bonematrix);
			}
			else {
				goalMX = m_pModelInstance->m_pbones[pbones[i].parent].ConcatTransforms(bonematrix);
			}

			// get jiggle properties from QC data
			mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)((byte *)m_pStudioHeader + pboneinfo[i].procindex);
			if (!m_pModelInstance->m_pJiggleBones) {
				m_pModelInstance->m_pJiggleBones = new CJiggleBones;
			}

			// do jiggle physics
			if (pboneinfo[i].proctype == STUDIO_PROC_JIGGLE) {
				m_pModelInstance->m_pJiggleBones->BuildJiggleTransformations(i, tr.time, jiggleInfo, goalMX, m_pModelInstance->m_pbones[i]);
			}
			else {
				m_pModelInstance->m_pbones[i] = goalMX; // fallback
			}
		}
		else
		{
			if (pbones[i].parent == -1) {
				m_pModelInstance->m_pbones[i] = m_pModelInstance->m_protationmatrix.ConcatTransforms(bonematrix);
			}
			else {
				m_pModelInstance->m_pbones[i] = m_pModelInstance->m_pbones[pbones[i].parent].ConcatTransforms(bonematrix);
			}
		}
	}

	// convert bones into compacted GLSL array
	mposetobone_t *m = m_pModelInstance->m_pModel->poseToBone;
	bool has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	if (has_boneweights)
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			matrix3x4 out = m_pModelInstance->m_pbones[i].ConcatTransforms( m->posetobone[i] );
			out.CopyToArray4x3( &m_pModelInstance->m_glstudiobones[i*3] );
			m_pModelInstance->m_studioquat[i] = out.GetQuaternion();
			m_pModelInstance->m_studiopos[i] = out.GetOrigin();
		}
	}
	else
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			m_pModelInstance->m_pbones[i].CopyToArray4x3( &m_pModelInstance->m_glstudiobones[i*3] );
			m_pModelInstance->m_studioquat[i] = m_pModelInstance->m_pbones[i].GetQuaternion();
			m_pModelInstance->m_studiopos[i] = m_pModelInstance->m_pbones[i].GetOrigin();
		}
	}
}

/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer :: StudioMergeBones( matrix3x4 &transform, matrix3x4 bones[], matrix3x4 cached_bones[], model_t *pModel, model_t *pParentModel )
{
	matrix3x4		bonematrix;
	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];
	float		poseparams[MAXSTUDIOPOSEPARAM];
	int		sequence = RI->currententity->curstate.sequence;
	model_t		*oldmodel = RI->currentmodel;
	studiohdr_t	*oldheader = m_pStudioHeader;
	bool		weapon_model = false;

	ASSERT( pModel != NULL && pModel->type == mod_studio );
	ASSERT( pParentModel != NULL && pParentModel->type == mod_studio );

	RI->currentmodel = pModel;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );

	// tell the bonesetup about current model
	m_boneSetup.SetStudioPointers( m_pStudioHeader, poseparams ); // don't touch original parameters
	m_boneSetup.CalcDefaultPoseParameters( poseparams ); // just to have something valid here

	if( pModel == IEngineStudio.GetModelByIndex( RI->currententity->curstate.weaponmodel ))
		weapon_model = true;

	// FIXME: how to specify custom sequence for p_model?
	if( weapon_model ) sequence = 0;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;
	float f = StudioEstimateFrame( pseqdesc );
	float cycle = f / m_boneSetup.LocalMaxFrame( sequence );

	m_boneSetup.InitPose( pos, q );
	m_boneSetup.AccumulatePose( NULL, pos, q, sequence, cycle, 1.0 );

	studiohdr_t *m_pParentHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pParentModel );

	ASSERT( m_pParentHeader != NULL );

	mstudiobone_t *pchildbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	mstudiobone_t *pparentbones = (mstudiobone_t *)((byte *)m_pParentHeader + m_pParentHeader->boneindex);

	for( int i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		int j;

		for( j = 0; j < m_pParentHeader->numbones; j++ )
		{
			if( !Q_stricmp( pchildbones[i].name, pparentbones[j].name ))
			{
				bones[i] = cached_bones[j];
				break;
			}
		}

		if( j >= m_pParentHeader->numbones )
		{
			// initialize bonematrix
			bonematrix = matrix3x4( pos[i], q[i] );
			if( pchildbones[i].parent == -1 ) bones[i] = transform.ConcatTransforms( bonematrix ); 
			else bones[i] = bones[pchildbones[i].parent].ConcatTransforms( bonematrix );
		}
	}

	RI->currentmodel = oldmodel;
	m_pStudioHeader = oldheader;

	// restore the bonesetup pointers
	m_boneSetup.SetStudioPointers( m_pStudioHeader, m_pModelInstance->m_poseparameter );
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer :: StudioCalcAttachments( matrix3x4 bones[] )
{
	if( RI->currententity->modelhandle == INVALID_HANDLE || !m_pModelInstance )
		return; // too early ?

	if( FBitSet( m_pModelInstance->info_flags, MF_ATTACHMENTS_DONE ))
		return; // already computed

	StudioAttachment_t *att = m_pModelInstance->attachment;
	mstudioattachment_t *pattachment;
	cl_entity_t *e = RI->currententity;

	// prevent to compute env_static bounds every frame
	if( FBitSet( e->curstate.iuser1, CF_STATIC_ENTITY ))
		SetBits( m_pModelInstance->info_flags, MF_ATTACHMENTS_DONE );

	if( m_pStudioHeader->numattachments <= 0 )
	{
		// clear attachments
		for( int i = 0; i < MAXSTUDIOATTACHMENTS; i++ )
		{
			if( i < 4 ) e->attachment[i] = StudioGetOrigin();
			att[i].angles = StudioGetAngles();
			att[i].origin = StudioGetOrigin();
		}
		return;
	}
	else if( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		ALERT( at_error, "Too many attachments on %s\n", e->model->name );
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);

	for( int i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		matrix3x4 world = bones[pattachment[i].bone].ConcatTransforms( att[i].local );
		Vector p1 = bones[pattachment[i].bone].GetOrigin();

		att[i].origin = world.GetOrigin();
		att[i].angles = world.GetAngles();

		// merge attachments position for viewmodel
		if( m_iDrawModelType == DRAWSTUDIO_VIEWMODEL || m_iDrawModelType == DRAWSTUDIO_RUNEVENTS )
		{
			StudioFormatAttachment( att[i].origin );
			StudioFormatAttachment( p1 );
		}

		if( i < 4 ) e->attachment[i] = att[i].origin;
		att[i].dir = (att[i].origin - p1).Normalize(); // forward vec
	}
}

/*
====================
StudioGetAttachment

====================
*/
void CStudioModelRenderer :: StudioGetAttachment( const cl_entity_t *ent, int iAttachment, Vector *pos, Vector *ang, Vector *dir)
{
	if( !ent || !ent->model || (!pos && !ang))
		return;

	studiohdr_t *phdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( ent->model );
	if (!phdr ) 
		return;

	if( ent->modelhandle == INVALID_HANDLE )
		return; // too early ?

	ModelInstance_t *inst = &m_ModelInstances[ent->modelhandle];

	// make sure we not overflow
	iAttachment = bound( 0, iAttachment, phdr->numattachments - 1 );

	if( pos ) *pos = inst->attachment[iAttachment].origin;
	if( ang ) *ang = inst->attachment[iAttachment].angles;
	if( dir ) *dir = inst->attachment[iAttachment].dir;
}

int CStudioModelRenderer::StudioGetAttachmentNumber(const cl_entity_t *ent, const char *attachment)
{
	if (!ent || ent->modelhandle == INVALID_HANDLE)
		return -1;

	ModelInstance_t *inst = &m_ModelInstances[ent->modelhandle];

	for (int i = 0; i < inst->numattachments; i++)
	{
		StudioAttachment_t *att = &inst->attachment[i];
		if (!Q_stricmp(att->name, attachment))
			return i;
	}

	return -1;
}

/*
====================
StudioSetupModel

used only by decals code
====================
*/
int CStudioModelRenderer :: StudioSetupModel( int bodypart, mstudiomodel_t **ppsubmodel, msubmodel_t **ppvbomodel )
{
	mstudiobodyparts_t *pbodypart;
	mstudiomodel_t *psubmodel;
	mbodypart_t *pCachedParts;
	msubmodel_t *pvbomodel;
	int index;

	if( bodypart > m_pStudioHeader->numbodyparts )
		bodypart = 0;

	if( m_pModelInstance->m_VlCache != NULL )
		pCachedParts = &m_pModelInstance->m_VlCache->bodyparts[bodypart];
	else pCachedParts = &RI->currentmodel->studiocache->bodyparts[bodypart];
	pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = RI->currententity->curstate.body / pbodypart->base;
	index = index % pbodypart->nummodels;

	psubmodel = (mstudiomodel_t *)((byte *)m_pStudioHeader + pbodypart->modelindex) + index;
	pvbomodel = pCachedParts->models[index];

	*ppsubmodel = psubmodel;
	*ppvbomodel = pvbomodel;

	return index;
}

/*
===============
StudioLighting

used for lighting decals
===============
*/
void CStudioModelRenderer :: StudioLighting( float *lv, int bone, int flags, const Vector &normal )
{
	mstudiolight_t	*light = &m_pModelInstance->light;

	if( FBitSet( flags, STUDIO_NF_FULLBRIGHT ) || FBitSet( RI->currententity->curstate.effects, EF_FULLBRIGHT ) || R_FullBright( ))
	{
		*lv = 1.0f;
		return;
	}

	float	illum = light->ambientlight;

	if( FBitSet( flags, STUDIO_NF_FLATSHADE ))
	{
		illum += light->shadelight * 0.8f;
	}
          else
          {
		float	lightcos;

		if( bone != -1 ) lightcos = DotProduct( normal, m_bonelightvecs[bone] );
		else lightcos = DotProduct( normal, -light->normal ); // -1 colinear, 1 opposite
		if( lightcos > 1.0f ) lightcos = 1.0f;

		illum += light->shadelight;

 		// do modified hemispherical lighting
		lightcos = (lightcos + ( SHADE_LAMBERT - 1.0f )) / SHADE_LAMBERT;
		if( lightcos > 0.0f )
			illum -= light->shadelight * lightcos; 
		illum = Q_max( illum, 0.0f );
	}

	illum = Q_min( illum, 255.0f );
	*lv = illum * (1.0f / 255.0f);
}

/*
===============
CacheVertexLight

===============
*/
void CStudioModelRenderer :: CacheVertexLight( cl_entity_t *ent )
{
	if( FBitSet( ent->curstate.iuser1, CF_STATIC_ENTITY ) && ( ent->curstate.colormap > 0 ) && world->vertex_lighting != NULL )
	{
		Vector		org = m_pModelInstance->m_protationmatrix[3]; // model origin
		int		cacheID = ent->curstate.colormap - 1;
		dvlightlump_t	*vl = world->vertex_lighting;

		if( FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
		{
			// does nothing
		}
		else if( !FBitSet( m_pModelInstance->info_flags, MF_VL_BAD_CACHE ))
		{
			// first initialization
			if( cacheID < vl->nummodels && vl->dataofs[cacheID] != -1 )
			{
				dmodelvertlight_t	*dml = (dmodelvertlight_t *)((byte *)world->vertex_lighting + vl->dataofs[cacheID]);
				byte		*vislight = ((byte *)world->vertex_lighting + vl->dataofs[cacheID]);

				// we have ID of vertex light cache and cache is present
				CreateStudioCacheVL( dml, cacheID );

				vislight += sizeof( *dml ) - ( sizeof( dvertlight_t ) * 3 ) + sizeof( dvertlight_t ) * dml->numverts;
				Mod_FindStaticLights( vislight, m_pModelInstance->lights, org );
			}
		}
	}
}

/*
===============
CacheSurfaceLight

===============
*/
void CStudioModelRenderer :: CacheSurfaceLight( cl_entity_t *ent )
{
	if( FBitSet( ent->curstate.iuser1, CF_STATIC_ENTITY ) && ( ent->curstate.colormap > 0 ) && world->surface_lighting != NULL )
	{
		Vector		org = m_pModelInstance->m_protationmatrix[3]; // model origin
		int		cacheID = ent->curstate.colormap - 1;
		dvlightlump_t	*vl = world->surface_lighting;

		if( FBitSet( m_pModelInstance->info_flags, MF_SURFACE_LIGHTING ))
		{
			if( m_pModelInstance->m_FlCache && m_pModelInstance->m_FlCache->update_light )
			{
				for( int j = 0; j < m_pModelInstance->m_FlCache->surfaces.size(); j++ )
				{
					mstudiosurface_t *surf = &m_pModelInstance->m_FlCache->surfaces[j];
					R_UpdateSurfaceParams( surf );
				}
				m_pModelInstance->m_FlCache->update_light = false;
			}
		}
		else if( !FBitSet( m_pModelInstance->info_flags, MF_VL_BAD_CACHE ))
		{
			// first initialization
			if( cacheID < vl->nummodels && vl->dataofs[cacheID] != -1 )
			{
				dmodelfacelight_t	*dml = (dmodelfacelight_t *)((byte *)world->surface_lighting + vl->dataofs[cacheID]);
				byte		*vislight = ((byte *)world->surface_lighting + vl->dataofs[cacheID]);

				// we have ID of vertex light cache and cache is present
				CreateStudioCacheFL( dml, cacheID );

				vislight += sizeof( *dml ) - ( sizeof( dfacelight_t ) * 3 ) + sizeof( dfacelight_t ) * dml->numfaces;
				Mod_FindStaticLights( vislight, m_pModelInstance->lights, org );
			}
		}
	}
}

/*
===============
StudioStaticLight

===============
*/
void CStudioModelRenderer :: StudioStaticLight( cl_entity_t *ent, mstudiolight_t *light )
{
	bool	ambient_light = false;
	bool	skipZCheck = true;
	bool	staticEntity = FBitSet(ent->curstate.iuser1, CF_STATIC_ENTITY);
	bool	badVertexLightCache = FBitSet(m_pModelInstance->info_flags, MF_VL_BAD_CACHE);
	bool	lightprobesAvailable = world->numleaflights > 0;

	if( FBitSet( ent->model->flags, STUDIO_AMBIENT_LIGHT ))
		ambient_light = true;
#if 0
	if( ent->curstate.movetype == MOVETYPE_STEP )
		skipZCheck = false;
#endif

	if( FBitSet( ent->curstate.iuser1, CF_STATIC_LIGHTMAPPED ))
		CacheSurfaceLight( ent );
	else CacheVertexLight( ent );

	if( m_pModelInstance->oldlight.nointerp )
	{
		m_pModelInstance->light_update = true;
		m_pModelInstance->lighttimecheck = 0.0f;
	}

	// refresh lighting every 0.1 secs
	if( m_pModelInstance->light_update )
	{
		if (staticEntity && badVertexLightCache || !lightprobesAvailable)
		{
			float dynamic = r_dynamic->value;
			alight_t lighting;
			Vector dir;

			lighting.plightvec = dir;

			// setup classic Half-Life lighting
			r_dynamic->value = 0.0f; // ignore dlights
			IEngineStudio.StudioDynamicLight( ent, &lighting );
			r_dynamic->value = dynamic;

			m_pModelInstance->newlight.ambientlight = lighting.ambientlight;
			m_pModelInstance->newlight.shadelight = lighting.shadelight;
			m_pModelInstance->newlight.diffuse = lighting.color;
			m_pModelInstance->newlight.normal = -Vector( lighting.plightvec );
			R_PointAmbientFromLeaf( ent->origin, &m_pModelInstance->newlight );
		}
		else
		{
			R_LightForStudio( ent->origin, &m_pModelInstance->newlight, ambient_light );
		}
		m_pModelInstance->light_update = false;

		// init state if was not in frustum
		if( Q_abs( m_pModelInstance->cached_frame - tr.realframecount ) > 2 )
			m_pModelInstance->oldlight = m_pModelInstance->newlight;
	}

	float frac = (tr.time - m_pModelInstance->lighttimecheck) * LIGHT_INTERP_FACTOR;
	frac = bound( 0.0f, frac, 1.0f );

	memset( light, 0, sizeof( *light ));
	InterpolateOrigin( m_pModelInstance->oldlight.diffuse, m_pModelInstance->newlight.diffuse, light->diffuse, frac );
	InterpolateOrigin( m_pModelInstance->oldlight.normal, m_pModelInstance->newlight.normal, light->normal, frac );
	light->ambientlight = Lerp( frac, m_pModelInstance->oldlight.ambientlight, m_pModelInstance->newlight.ambientlight );
	light->shadelight = Lerp( frac, m_pModelInstance->oldlight.shadelight, m_pModelInstance->newlight.shadelight );

	for( int i = 0; i < 6; i++ )
		InterpolateOrigin( m_pModelInstance->oldlight.ambient[i], m_pModelInstance->newlight.ambient[i], light->ambient[i], frac );

	// find worldlights for dynamic lighting
	if( FBitSet( m_pModelInstance->info_flags, MF_POSITION_CHANGED ) && !staticEntity)
	{
		Vector origin = (m_pModelInstance->absmin + m_pModelInstance->absmax) * 0.5f;
		Vector mins = m_pModelInstance->absmin - origin;
		Vector maxs = m_pModelInstance->absmax - origin;
		// search for worldlights that affected to model bbox
		R_FindWorldLights( origin, mins, maxs, m_pModelInstance->lights, skipZCheck );
		ClearBits( m_pModelInstance->info_flags, MF_POSITION_CHANGED );
	}
}

/*
===============
StudioClientEvents

===============
*/
void CStudioModelRenderer :: StudioClientEvents( void )
{
	mstudioseqdesc_t	*pSeqDesc;
	mstudioevent_t	*pEvent;
	int iSeqNum = RI->currententity->curstate.sequence;

	// invalid sequence number, just ignore it
	if (iSeqNum < 0 || iSeqNum >= m_pStudioHeader->numseq)
		return;

	pSeqDesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + iSeqNum;
	pEvent = (mstudioevent_t *)((byte *)m_pStudioHeader + pSeqDesc->eventindex);

	// no events for this animation or gamepaused
	if( pSeqDesc->numevents == 0 || tr.frametime == 0 )
		return;

	// don't playing events from thirdperson model
	if( RP_LOCALCLIENT( RI->currententity ) && !FBitSet( RI->params, RP_THIRDPERSON ))
		return;

	float frame = StudioEstimateFrame( pSeqDesc ); // get start offset
	float start = frame - RI->currententity->curstate.framerate * tr.frametime * pSeqDesc->fps;
	float end = frame;

	for( int i = 0; i < pSeqDesc->numevents; i++ )
	{
		// ignore all non-client-side events
		if( pEvent[i].event < EVENT_CLIENT )
			continue;

		if( (float)pEvent[i].frame > start && (float)pEvent[i].frame <= end )
			HUD_StudioEvent( &pEvent[i], RI->currententity );
	}
}

/*
===============
StudioDrawBones

===============
*/
void CStudioModelRenderer :: StudioDrawBones( void )
{
	mstudiobone_t	*pbones = (mstudiobone_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	Vector		point;

	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		if( pbones[i].parent >= 0 )
		{
			pglPointSize( 3.0f );
			pglColor3f( 1, 0.7f, 0 );
			pglBegin( GL_LINES );
			
			m_pModelInstance->m_pbones[pbones[i].parent].GetOrigin( point );
			pglVertex3fv( point );

			m_pModelInstance->m_pbones[i].GetOrigin( point );
			pglVertex3fv( point );
			
			pglEnd();

			pglColor3f( 0, 0, 0.8f );
			pglBegin( GL_POINTS );

			if( pbones[pbones[i].parent].parent != -1 )
			{
				m_pModelInstance->m_pbones[pbones[i].parent].GetOrigin( point );
				pglVertex3fv( point );
			}

			m_pModelInstance->m_pbones[i].GetOrigin( point );
			pglVertex3fv( point );
			pglEnd();
		}
		else
		{
			// draw parent bone node
			pglPointSize( 5.0f );
			pglColor3f( 0.8f, 0, 0 );
			pglBegin( GL_POINTS );

			m_pModelInstance->m_pbones[i].GetOrigin( point );
			pglVertex3fv( point );
			pglEnd();
		}
	}

	pglPointSize( 1.0f );
}

/*
===============
StudioDrawHulls

===============
*/
void CStudioModelRenderer :: StudioDrawHulls( int iHitbox )
{
	float	alpha, lv;
	int	i, j;

	if( r_drawentities->value == 4 )
		alpha = 0.5f;
	else alpha = 1.0f;

	// setup bone lighting
	for( i = 0; i < m_pStudioHeader->numbones; i++ )
		m_bonelightvecs[i] = m_pModelInstance->m_pbones[i].VectorIRotate( -m_pModelInstance->light.normal );

	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	for( i = 0; i < m_pStudioHeader->numhitboxes; i++ )
	{
		mstudiobbox_t	*pbbox = (mstudiobbox_t *)((byte *)m_pStudioHeader + m_pStudioHeader->hitboxindex);
		vec3_t		tmp, p[8];

		if( iHitbox >= 0 && iHitbox != i )
			continue;

		for( j = 0; j < 8; j++ )
		{
			tmp[0] = (j & 1) ? pbbox[i].bbmin[0] : pbbox[i].bbmax[0];
			tmp[1] = (j & 2) ? pbbox[i].bbmin[1] : pbbox[i].bbmax[1];
			tmp[2] = (j & 4) ? pbbox[i].bbmin[2] : pbbox[i].bbmax[2];
			p[j] = m_pModelInstance->m_pbones[pbbox[i].bone].VectorTransform( tmp );
		}

		j = (pbbox[i].group % ARRAYSIZE( g_hullcolor ));

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		gEngfuncs.pTriAPI->Color4f( g_hullcolor[j][0], g_hullcolor[j][1], g_hullcolor[j][2], alpha );

		for( j = 0; j < 6; j++ )
		{
			tmp = g_vecZero;
			tmp[j % 3] = (j < 3) ? 1.0f : -1.0f;
			StudioLighting( &lv, pbbox[i].bone, 0, tmp );

			gEngfuncs.pTriAPI->Brightness( Q_max( lv, tr.ambientFactor ));
			gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[j][0]] );
			gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[j][1]] );
			gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[j][2]] );
			gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[j][3]] );
		}
		gEngfuncs.pTriAPI->End();
	}
}

/*
===============
StudioDrawAbsBBox

===============
*/
void CStudioModelRenderer :: StudioDrawAbsBBox( void )
{
	Vector p[8], tmp;
	float lv;
	int i;

	// looks ugly, skip
	if( RI->currententity == GET_VIEWMODEL( ))
		return;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
  		p[i][0] = ( i & 1 ) ? m_pModelInstance->absmin[0] : m_pModelInstance->absmax[0];
  		p[i][1] = ( i & 2 ) ? m_pModelInstance->absmin[1] : m_pModelInstance->absmax[1];
  		p[i][2] = ( i & 4 ) ? m_pModelInstance->absmin[2] : m_pModelInstance->absmax[2];
	}

	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	gEngfuncs.pTriAPI->Color4f( 0.5f, 0.5f, 1.0f, 0.5f );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );

	gEngfuncs.pTriAPI->Begin( TRI_QUADS );

	for( i = 0; i < 6; i++ )
	{
		tmp = g_vecZero;
		tmp[i % 3] = (i < 3) ? 1.0f : -1.0f;
		StudioLighting( &lv, -1, 0, tmp );

		gEngfuncs.pTriAPI->Brightness( Q_max( lv, tr.ambientFactor ));
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][0]] );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][1]] );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][2]] );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][3]] );
	}

	gEngfuncs.pTriAPI->End();
}

void CStudioModelRenderer :: StudioDrawAttachments( bool bCustomFov )
{
	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglDisable( GL_DEPTH_TEST );
	
	for( int i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		mstudioattachment_t	*pattachments;
		Vector v[4];

		pattachments = (mstudioattachment_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);		

		v[0] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( pattachments[i].org );
		v[1] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( g_vecZero );
		v[2] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( g_vecZero );
		v[3] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( g_vecZero );

		for( int j = 0; j < 4 && bCustomFov; j++ )
			StudioFormatAttachment( v[j] );

		pglBegin( GL_LINES );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[1] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv (v[2] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv( v[3] );
		pglEnd();

		pglPointSize( 5.0f );
		pglColor3f( 0, 1, 0 );
		pglBegin( GL_POINTS );
		pglVertex3fv( v[0] );
		pglEnd();
		pglPointSize( 1.0f );
	}

	pglEnable( GL_DEPTH_TEST );
}

void CStudioModelRenderer::StudioDrawBodyPartsBBox()
{
	mbodypart_t *bodyparts;

	// looks ugly, skip
	if( RI->currententity == GET_VIEWMODEL( ))
		return;

	if (m_pModelInstance->m_FlCache != NULL)
		bodyparts = &m_pModelInstance->m_FlCache->bodyparts.front();
	else if (m_pModelInstance->m_VlCache != NULL)
		bodyparts = &m_pModelInstance->m_VlCache->bodyparts.front();
	else
		bodyparts = &RI->currentmodel->studiocache->bodyparts.front();

	if (!bodyparts) {
		HOST_ERROR("%s missed cache\n", RI->currententity->model->name);
		return;
	}

	for (int i = 0; i < m_pStudioHeader->numbodyparts; ++i)
	{
		mbodypart_t* pBodyPart = &bodyparts[i];
		int index = RI->currententity->curstate.body / pBodyPart->base;
		index = index % pBodyPart->models.size();

		msubmodel_t* pSubModel = pBodyPart->models[index];
		if (!pSubModel) 
			continue; // blank submodel, just ignore

		for (int j = 0; j < pSubModel->meshes.size(); j++)
		{
			Vector p[8];
			vbomesh_t *mesh = &pSubModel->meshes[j];

			// compute a full bounding box
			CBoundingBox meshBounds = StudioGetMeshBounds(m_pModelInstance, mesh);
			for (int k = 0; k < 8; k++)
			{
				p[k].x = (k & 1) ? meshBounds.GetMins().x : meshBounds.GetMaxs().x;
				p[k].y = (k & 2) ? meshBounds.GetMins().y : meshBounds.GetMaxs().y;
				p[k].z = (k & 4) ? meshBounds.GetMins().z : meshBounds.GetMaxs().z;
			}

			GL_Bind(GL_TEXTURE0, tr.whiteTexture);
			gEngfuncs.pTriAPI->Color4f(0.2f, 1.0f, 0.2f, 1.0f);
			gEngfuncs.pTriAPI->RenderMode(kRenderTransColor);
			gEngfuncs.pTriAPI->Begin(TRI_LINES);

			for (int k = 0; k < 6; k++)
			{				
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][0]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][1]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][1]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][2]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][2]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][3]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][3]]);
				gEngfuncs.pTriAPI->Vertex3fv(p[g_boxpnt[k][0]]);
			}

			gEngfuncs.pTriAPI->End();
		}
	}
}

void CStudioModelRenderer :: StudioDrawDebug( cl_entity_t *e )
{
	matrix4x4 projMatrix, worldViewProjMatrix;
	bool bCustomFov = false;

	// don't reflect this entity in mirrors
	if( e->curstate.effects & EF_NOREFLECT && FBitSet( RI->params, RP_MIRRORVIEW ))
		return;

	// draw only in mirrors
	if( e->curstate.effects & EF_REFLECTONLY && !FBitSet( RI->params, RP_MIRRORVIEW ))
		return;

	// ignore in thirdperson, camera view or client is died
	if (e == GET_VIEWMODEL())
	{
		if( FBitSet( RI->params, RP_THIRDPERSON ) || CL_IsDead() || !UTIL_IsLocal( RI->view.entity ))
			return;

		if( !RP_NORMALPASS( ))
			return;

		bCustomFov = ComputeCustomFov( projMatrix, worldViewProjMatrix );
	}

	if( !StudioSetEntity( e ))
		return;

	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_BindShader( NULL );

	switch( (int)r_drawentities->value )
	{
	case 2:
		StudioDrawBones();
		break;
	case 3:
		StudioDrawHulls();
		break;
	case 4:
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		StudioDrawHulls ();
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		break;
	case 5:
		StudioDrawAbsBBox();
		break;
	case 6:
		StudioDrawAttachments( bCustomFov );
		break;
	case 7:
		StudioDrawBodyPartsBBox();
		break;
	}

	if( bCustomFov ) RestoreNormalFov( projMatrix, worldViewProjMatrix );
}

/*
====================
StudioFormatAttachment

====================
*/
void CStudioModelRenderer :: StudioFormatAttachment( Vector &point )
{
	float worldx = tan( (float)RI->view.fov_x * M_PI / 360.0 );
	float viewx = tan( m_flViewmodelFov * M_PI / 360.0 );

	// BUGBUG: workaround
	if( viewx == 0.0f ) viewx = 1.0f;

	// aspect ratio cancels out, so only need one factor
	// the difference between the screen coordinates of the 2 systems is the ratio
	// of the coefficients of the projection matrices (tan (fov/2) is that coefficient)
	float factor = worldx / viewx;

	// get the coordinates in the viewer's space.
	Vector tmp = point - GetVieworg();
	Vector vTransformed;

	vTransformed.x = DotProduct( GetVLeft(), tmp );
	vTransformed.y = DotProduct( GetVUp(), tmp );
	vTransformed.z = DotProduct( GetVForward(), tmp );
	vTransformed.x *= factor;
	vTransformed.y *= factor;

	// Transform back to world space.
	Vector vOut = (GetVLeft() * vTransformed.x) + (GetVUp() * vTransformed.y) + (GetVForward() * vTransformed.z);
	point = GetVieworg() + vOut;
}

/*
===============
ChooseStudioProgram

Select the program for mesh (diffuse\bump\debug)
===============
*/
word CStudioModelRenderer :: ChooseStudioProgram( studiohdr_t *phdr, mstudiomaterial_t *mat, bool lightpass )
{
	bool bone_weights = FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS ) != 0;
	int lightmode = LIGHTSTATIC_NONE;

	if( FBitSet( m_pModelInstance->info_flags, MF_SURFACE_LIGHTING ))
		lightmode = LIGHTSTATIC_SURFACE;
	else if( FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
		lightmode = LIGHTSTATIC_VERTEX;

	if( FBitSet( RI->params, RP_SHADOWVIEW ))
		return ShaderSceneDepth( mat, bone_weights, phdr->numbones );

	if( lightpass )
	{
		return ShaderLightForward( RI->currentlight, mat, bone_weights, phdr->numbones );
	}
	else
	{
		return ShaderSceneForward( mat, lightmode, bone_weights, phdr->numbones );
	}
}

/*
====================
AddMeshToDrawList

====================
*/
void CStudioModelRenderer :: AddMeshToDrawList( studiohdr_t *phdr, vbomesh_t *mesh, bool lightpass )
{
	// static entities allows to cull each part individually
	const bool skipCulling = CVAR_TO_BOOL(r_nocull);
	if( FBitSet( RI->currententity->curstate.iuser1, CF_STATIC_ENTITY ) )
	{
		CBoundingBox meshBounds = StudioGetMeshBounds(m_pModelInstance, mesh);
		if (!skipCulling)
		{
			if (!Mod_CheckBoxVisible(meshBounds.GetMins(), meshBounds.GetMaxs()))
				return; // occulded

			if (R_CullBox(meshBounds.GetMins(), meshBounds.GetMaxs()))
				return; // culled
		}
	}

	int m_skinnum = bound( 0, RI->currententity->curstate.skin, phdr->numskinfamilies - 1 );
	short *pskinref = (short *)((byte *)phdr + phdr->skinindex);
	if( m_skinnum != 0 && m_skinnum < phdr->numskinfamilies )
		pskinref += (m_skinnum * phdr->numskinref);

	mstudiomaterial_t *mat = NULL;
	bool cache_from_model = false;

	if( RI->currentmodel == IEngineStudio.GetModelByIndex( RI->currententity->curstate.weaponmodel ))
		cache_from_model = true;

	if( cache_from_model )
		mat = &RI->currentmodel->materials[pskinref[mesh->skinref]];
	else mat = &m_pModelInstance->materials[pskinref[mesh->skinref]]; // NOTE: use local copy for right cache shadernums

	bool flagAdditive = FBitSet(mat->flags, STUDIO_NF_ADDITIVE);
	bool flagMasked = FBitSet(mat->flags, STUDIO_NF_MASKED);
	bool texAlphaBlend = (flagAdditive || flagMasked) && FBitSet(mat->flags, STUDIO_NF_HAS_ALPHA);
	bool texAlphaToCoverage = FBitSet(mat->flags, STUDIO_NF_ALPHATOCOVERAGE);
	bool transparent = texAlphaBlend && !texAlphaToCoverage;
	bool solid = R_OpaqueEntity(RI->currententity) && !transparent ? true : false;

	// goes into regular arrays
	if( FBitSet( RI->params, RP_SHADOWVIEW ))
	{
		if( FBitSet( mat->flags, STUDIO_NF_ADDITIVE ) && FBitSet( mat->flags, STUDIO_NF_HAS_ALPHA ))
			solid = true; // add alpha-glasses to shadowlist
		lightpass = false;
	}

	if( lightpass )
	{
		if( FBitSet( mat->flags, STUDIO_NF_FULLBRIGHT ))
			return; // can't light fullbrights

		if( RI->currentlight->type == LIGHT_DIRECTIONAL )
		{
			if( FBitSet( mat->flags, STUDIO_NF_NOSUNLIGHT ))
				return; // shader was failed to compile
		}
		else
		{
			if( FBitSet( mat->flags, STUDIO_NF_NODLIGHT ))
				return; // shader was failed to compile
		}
	}
	else
	{
		if( FBitSet( mat->flags, STUDIO_NF_NODRAW ))
			return; // shader was failed to compile
	}

	word hProgram = 0;

	if( !( hProgram = ChooseStudioProgram( phdr, mat, lightpass )))
		return; // failed to build shader, don't draw this surface
	glsl_program_t *shader = &glsl_programs[hProgram];

	if( lightpass )
	{
		CSolidEntry entry;
		entry.SetRenderMesh( mesh, hProgram );
		RI->frame.light_meshes.AddToTail( entry );
	}
	else if( solid )
	{
		CSolidEntry entry;
		entry.SetRenderMesh( mesh, hProgram );
		RI->frame.solid_meshes.AddToTail( entry );
	}
	else
	{
		CTransEntry entry;
		entry.SetRenderMesh(mesh, hProgram);
		if (mesh->parentbone != 0xFF)
		{
			CBoundingBox meshBounds = StudioGetMeshBounds(m_pModelInstance, mesh);
			if (ScreenCopyRequired(shader)) {
				entry.ComputeScissor(meshBounds.GetMins(), meshBounds.GetMaxs());
			}
			else {
				entry.ComputeViewDistance(meshBounds.GetMins(), meshBounds.GetMaxs());
			}
		}
		RI->frame.trans_list.AddToTail(entry);
	}
}

/*
====================
AddBodyPartToDrawList

====================
*/
void CStudioModelRenderer :: AddBodyPartToDrawList( studiohdr_t *phdr, mbodypart_t *bodyparts, int bodypart, bool lightpass )
{
	if( !bodyparts ) bodyparts = &RI->currentmodel->studiocache->bodyparts.front();
	if( !bodyparts ) HOST_ERROR( "%s missed cache\n", RI->currententity->model->name );

	bodypart = bound( 0, bodypart, phdr->numbodyparts );
	mbodypart_t *pBodyPart = &bodyparts[bodypart];
	int index = RI->currententity->curstate.body / pBodyPart->base;
	index = index % pBodyPart->models.size();

	msubmodel_t *pSubModel = pBodyPart->models[index];
	if( !pSubModel ) return; // blank submodel, just ignore

	for( int i = 0; i < pSubModel->meshes.size(); i++ )
		AddMeshToDrawList( phdr, &pSubModel->meshes[i], lightpass );
}

/*
=================
AddStudioModelToDrawList

do culling, compute bones, add meshes to list
=================
*/
void CStudioModelRenderer :: AddStudioModelToDrawList( cl_entity_t *e, bool update )
{
	if( !StudioSetEntity( e ))
		return;

	if( !StudioComputeBBox( ))
		return; // invalid sequence

	if( !Mod_CheckBoxVisible( m_pModelInstance->absmin, m_pModelInstance->absmax ))
	{
		r_stats.c_culled_entities++;
		return;
	}

	if( R_CullModel( RI->currententity, m_pModelInstance->absmin, m_pModelInstance->absmax ))
	{
		r_stats.c_culled_entities++;
		return; // culled
	}

	// no cache for local player in firstperson
	if( RP_LOCALCLIENT( RI->currententity ) && !FBitSet( RI->params, RP_THIRDPERSON ))
		m_pModelInstance->cached_frame = -1;

	bool has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	m_pModelInstance->visframe = tr.realframecount; // visible	

	if( m_pModelInstance->cached_frame != tr.realframecount )
	{
		StudioSetUpTransform( );

		if( RI->currententity->curstate.movetype == MOVETYPE_FOLLOW && RI->currententity->curstate.aiment > 0 )
		{
			cl_entity_t *parent = gEngfuncs.GetEntityByIndex( RI->currententity->curstate.aiment );
			if( parent != NULL && parent->modelhandle != INVALID_HANDLE )
			{
				ModelInstance_t *inst = &m_ModelInstances[parent->modelhandle];
				StudioMergeBones( m_pModelInstance->m_protationmatrix, m_pModelInstance->m_pbones, inst->m_pbones, RI->currentmodel, parent->model );

				mposetobone_t *m = m_pModelInstance->m_pModel->poseToBone;

				// convert bones into compacted GLSL array
				if (has_boneweights)
				{
					for( int i = 0; i < m_pStudioHeader->numbones; i++ )
					{
						matrix3x4 out = m_pModelInstance->m_pbones[i].ConcatTransforms( m->posetobone[i] );
						out.CopyToArray4x3( &m_pModelInstance->m_glstudiobones[i*3] );
						m_pModelInstance->m_studioquat[i] = out.GetQuaternion();
						m_pModelInstance->m_studiopos[i] = out.GetOrigin();
					}
				}
				else
				{
					for( int i = 0; i < m_pStudioHeader->numbones; i++ )
					{
						m_pModelInstance->m_pbones[i].CopyToArray4x3( &m_pModelInstance->m_glstudiobones[i*3] );
						m_pModelInstance->m_studioquat[i] = m_pModelInstance->m_pbones[i].GetQuaternion();
						m_pModelInstance->m_studiopos[i] = m_pModelInstance->m_pbones[i].GetOrigin();
					}
				}
			}
			else
			{
				ALERT( at_error, "FollowEntity: %i with model %s has missed parent!\n",
				RI->currententity->index, RI->currentmodel->name );
				return;
			}
		}
		else StudioSetupBones( );

		// calc attachments only once per frame
		StudioCalcAttachments( m_pModelInstance->m_pbones );
		StudioClientEvents( );

		if( RI->currententity->index > 0 )
		{
			// because RI->currententity may be not equal his index e.g. for viewmodel
			cl_entity_t *ent = GET_ENTITY( RI->currententity->index );
			memcpy( ent->attachment, RI->currententity->attachment, sizeof( Vector ) * 4 );
		}

		// grab the static lighting from world
		StudioStaticLight( RI->currententity, &m_pModelInstance->light );

		model_t *pweaponmodel = NULL;

		if( RI->currententity->curstate.weaponmodel )
			pweaponmodel = IEngineStudio.GetModelByIndex( RI->currententity->curstate.weaponmodel );

		if (pweaponmodel && RI->currentmodel)
		{
			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pweaponmodel );

			StudioMergeBones( m_pModelInstance->m_protationmatrix, m_pModelInstance->m_pwpnbones, m_pModelInstance->m_pbones, pweaponmodel, RI->currentmodel );

			mposetobone_t *m = pweaponmodel->poseToBone;

			// convert bones into compacted GLSL array
			if (has_boneweights)
			{
				for( int i = 0; i < m_pStudioHeader->numbones; i++ )
				{
					matrix3x4 out = m_pModelInstance->m_pwpnbones[i].ConcatTransforms( m->posetobone[i] );
					out.CopyToArray4x3( &m_pModelInstance->m_glweaponbones[i*3] );
					m_pModelInstance->m_weaponquat[i] = out.GetQuaternion();
					m_pModelInstance->m_weaponpos[i] = out.GetOrigin();
				}
			}
			else
			{
				for( int i = 0; i < m_pStudioHeader->numbones; i++ )
				{
					m_pModelInstance->m_pwpnbones[i].CopyToArray4x3( &m_pModelInstance->m_glweaponbones[i*3] );
					m_pModelInstance->m_weaponquat[i] = m_pModelInstance->m_pwpnbones[i].GetQuaternion();
					m_pModelInstance->m_weaponpos[i] = m_pModelInstance->m_pwpnbones[i].GetOrigin();
				}
			}

			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );
		}

		// add visible lights to vislight matrix
		R_MarkVisibleLights( m_pModelInstance->lights );

		// now this frame cached
		m_pModelInstance->cached_frame = tr.realframecount;
	}

	if(( m_iDrawModelType == DRAWSTUDIO_RUNEVENTS ) || r_drawentities->value == 2.0f )
		return;

	model_t *pweaponmodel = NULL;
	mbodypart_t *pbodyparts = NULL;

	if( update )
	{
		if( RI->currententity->curstate.weaponmodel )
			pweaponmodel = IEngineStudio.GetModelByIndex( RI->currententity->curstate.weaponmodel );
		m_pModelInstance = NULL;
		return;
	}

	// change shared model with instanced model for this entity (it has personal vertex light cache)
	if( m_pModelInstance->m_FlCache != NULL )
		pbodyparts = &m_pModelInstance->m_FlCache->bodyparts.front();
	else if( m_pModelInstance->m_VlCache != NULL )
		pbodyparts = &m_pModelInstance->m_VlCache->bodyparts.front();

	for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
		AddBodyPartToDrawList( m_pStudioHeader, pbodyparts, i, ( RI->currentlight != NULL ));

	if( RI->currententity->curstate.weaponmodel )
		pweaponmodel = IEngineStudio.GetModelByIndex( RI->currententity->curstate.weaponmodel );

	if (pweaponmodel && RI->currentmodel)
	{
		RI->currentmodel = pweaponmodel;
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );

		// add weaponmodel parts
		for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
			AddBodyPartToDrawList( m_pStudioHeader, NULL, i, ( RI->currentlight != NULL ));
	}

	m_pModelInstance = NULL;
}

/*
=================
RunViewModelEvents

=================
*/
void CStudioModelRenderer :: RunViewModelEvents( void )
{
	if( !CVAR_TO_BOOL( m_pCvarDrawViewModel ))
		return;

	// ignore in thirdperson, camera view or client is died
	if( FBitSet( RI->params, RP_THIRDPERSON ) || CL_IsDead() || !UTIL_IsLocal( RI->view.entity ))
		return;

	if( !RP_NORMALPASS( )) return;

	if( !IEngineStudio.Mod_Extradata( GET_VIEWMODEL()->model ))
		return;

	RI->currententity = GET_VIEWMODEL();
	RI->currentmodel = RI->currententity->model;
	if( !RI->currentmodel ) return;

	RI->currententity->curstate.renderamt = R_ComputeFxBlend( RI->currententity );

	// tell the particle system about visibility
	RI->currententity->curstate.messagenum = r_currentMessageNum;

	m_iDrawModelType = DRAWSTUDIO_RUNEVENTS;

	SET_CURRENT_ENTITY( RI->currententity );
	AddStudioModelToDrawList( RI->currententity );
	SET_CURRENT_ENTITY( NULL );

	m_iDrawModelType = DRAWSTUDIO_NORMAL;
	RI->currententity = NULL;
	RI->currentmodel = NULL;
}

bool CStudioModelRenderer :: ComputeCustomFov( matrix4x4 &projMatrix, matrix4x4 &worldViewProjMatrix )
{
	float flDesiredFOV = 90.0f;

	// bound FOV values
	if( m_pCvarViewmodelFov->value < 50 )
		gEngfuncs.Cvar_SetValue( "cl_viewmodel_fov", 50 );
	else if( m_pCvarViewmodelFov->value > 120 )
		gEngfuncs.Cvar_SetValue( "cl_viewmodel_fov", 120 );

	if (m_pCvarViewmodelZNear->value <= 0.0)
		m_pCvarViewmodelZNear->value = 4.0f;

	// Find the offset our current FOV is from the default value
	const float flFOVOffset = 90.0f - (float)RI->view.fov_x;
	const float viewmodelZNear = m_pCvarViewmodelZNear->value;

	switch( m_iDrawModelType )
	{
	case DRAWSTUDIO_VIEWMODEL:
		// Adjust the viewmodel's FOV to move with any FOV offsets on the viewer's end
		m_flViewmodelFov = flDesiredFOV = m_pCvarViewmodelFov->value - flFOVOffset;
		break;
	default:	// failed case, unchanged
		flDesiredFOV = RI->view.fov_x;
		break;
	}

	// calc local FOV
	float fov_x = flDesiredFOV;
	float fov_y = V_CalcFov( fov_x, ScreenWidth, ScreenHeight );

	if( fov_x != RI->view.fov_x )
	{
		projMatrix = RI->view.projectionMatrix;
		worldViewProjMatrix = RI->view.worldProjectionMatrix;
		R_SetupProjectionMatrix( fov_x, fov_y, RI->view.projectionMatrix, viewmodelZNear );
		RI->view.worldProjectionMatrix = RI->view.projectionMatrix.Concat( RI->view.worldMatrix );
		RI->view.worldProjectionMatrix.CopyToArray( RI->glstate.modelviewProjectionMatrix );
		RI->view.projectionMatrix.CopyToArray( RI->glstate.projectionMatrix );

		pglMatrixMode( GL_PROJECTION );
		pglLoadMatrixf( RI->glstate.projectionMatrix );

		return true;
	}

	return false;
}

void CStudioModelRenderer :: RestoreNormalFov( matrix4x4 &projMatrix, matrix4x4 &worldViewProjMatrix )
{
	// restore original matrix
	RI->view.projectionMatrix = projMatrix;
	RI->view.worldProjectionMatrix = worldViewProjMatrix;
	RI->view.worldProjectionMatrix.CopyToArray( RI->glstate.modelviewProjectionMatrix );
	RI->view.projectionMatrix.CopyToArray( RI->glstate.projectionMatrix );

	pglMatrixMode( GL_PROJECTION );
	pglLoadMatrixf( RI->glstate.projectionMatrix );
}

/*
=================
DrawViewModel

=================
*/
void CStudioModelRenderer :: DrawViewModel( void )
{
	cl_entity_t	*view = GET_VIEWMODEL();

	if( !CVAR_TO_BOOL( m_pCvarDrawViewModel ))
		return;

	// ignore in thirdperson, camera view or client is died
	if( FBitSet( RI->params, RP_THIRDPERSON ) || CL_IsDead() || !UTIL_IsLocal( RI->view.entity ))
		return;

	if( !RP_NORMALPASS( ) )
		return;

	// tell the particle system about visibility
	view->curstate.messagenum = r_currentMessageNum;

	// hack the depth range to prevent view model from poking into walls
	GL_DepthRange( gldepthmin, gldepthmin + 0.3f * ( gldepthmax - gldepthmin ));

	// backface culling for left-handed weapons
	if( CVAR_TO_BOOL( m_pCvarHand ))
		GL_FrontFace( !glState.frontFace );

	m_iDrawModelType = DRAWSTUDIO_VIEWMODEL;
	RI->frame.solid_meshes.RemoveAll();
	RI->frame.trans_list.RemoveAll();
	view->curstate.weaponmodel = 0;

	matrix4x4 projMatrix, worldViewProjMatrix;

	bool bCustom = ComputeCustomFov( projMatrix, worldViewProjMatrix );

	// prevent ugly blinking when weapon is changed
	view->model = MODEL_HANDLE( gHUD.m_iViewModelIndex );

	// copy viewhands modelindx into weaponmodel
	if( view->index > 0 )
	{
		cl_entity_t *ent = GET_ENTITY( view->index );
		view->curstate.weaponmodel = ent->curstate.iuser3;
	}

	// we can't draw head shield and viewmodel for once call
	// because water blur separates them
	AddStudioModelToDrawList( view );

	RenderSolidStudioList();
	R_RenderTransList();

	if( bCustom ) RestoreNormalFov( projMatrix, worldViewProjMatrix );

	// restore depth range
	GL_DepthRange( gldepthmin, gldepthmax );

	// backface culling for left-handed weapons
	if( CVAR_TO_BOOL( m_pCvarHand ))
		GL_FrontFace( !glState.frontFace );

	m_iDrawModelType = DRAWSTUDIO_NORMAL;
	GL_BindShader( NULL );
}

void CStudioModelRenderer :: DrawMeshFromBuffer( const vbomesh_t *mesh )
{
	pglBindVertexArray( mesh->vao );

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElements( GL_TRIANGLES, 0, mesh->numVerts - 1, mesh->numElems, GL_UNSIGNED_INT, 0 );
	else pglDrawElements( GL_TRIANGLES, mesh->numElems, GL_UNSIGNED_INT, 0 );

	r_stats.c_total_tris += (mesh->numElems / 3);
	r_stats.num_flushes_total++;
}

void CStudioModelRenderer :: BuildMeshListForLight( CDynLight *pl, bool solid )
{
	Vector bounds[2];
	const bool skipCulling = CVAR_TO_BOOL(r_nocull);
	RI->frame.light_meshes.RemoveAll();

	if( solid )
	{
		// check solid meshes for light intersection
		for( int i = 0; i < RI->frame.solid_meshes.Count(); i++ )
		{
			CSolidEntry *entry = &RI->frame.solid_meshes[i];
			StudioGetBounds( entry, bounds );

			if( !skipCulling && pl->frustum.CullBoxFast( bounds[0], bounds[1] ))
				continue;	// no interaction

			// setup the global pointers
			if( !StudioSetEntity( entry ))
				continue;

			if( FBitSet( RI->currententity->curstate.effects, EF_FULLBRIGHT ))
				continue;

			AddMeshToDrawList( m_pStudioHeader, entry->m_pMesh, true );
		}
	}
	else
	{
		// check trans meshes for light intersection
		for( int i = 0; i < RI->frame.trans_list.Count(); i++ )
		{
			CTransEntry *entry = &RI->frame.trans_list[i];

			if( entry->m_bDrawType != DRAWTYPE_MESH )
				continue;

			StudioGetBounds( entry, bounds );
			if( !skipCulling && pl->frustum.CullBoxFast( bounds[0], bounds[1] ))
				continue;	// no interaction

			// setup the global pointers
			if( !StudioSetEntity( entry ))
				continue;

			if( FBitSet( RI->currententity->curstate.effects, EF_FULLBRIGHT ))
				continue;

			AddMeshToDrawList( m_pStudioHeader, entry->m_pMesh, true );
		}
	}
}

void CStudioModelRenderer :: DrawLightForMeshList( CDynLight *pl )
{
	pglBlendFunc( GL_ONE, GL_ONE );
	float y2 = (float)RI->view.port[3] - pl->h - pl->y;
	pglScissor( pl->x, y2, pl->w, pl->h );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( cv_nosort ))
		RI->frame.light_meshes.Sort( SortSolidMeshes );

	pglAlphaFunc( GL_GEQUAL, 0.5f );
	RI->currententity = NULL;
	RI->currentmodel = NULL;
	m_pCurrentMaterial = NULL;

	for( int i = 0; i < RI->frame.light_meshes.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.light_meshes[i];
		DrawSingleMesh(entry, ( i == 0 ), true);
	}

	GL_Cull( GL_FRONT );
}

void CStudioModelRenderer :: RenderDynLightList( bool solid )
{
	if( FBitSet( RI->params, RP_ENVVIEW|RP_SKYVIEW ))
		return;

	if( !FBitSet( RI->view.flags, RF_HASDYNLIGHTS ))
		return;

	if( R_FullBright( )) return;

	GL_DEBUG_SCOPE();
	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );

	CDynLight *pl = tr.dlights;

	for( int i = 0; i < MAX_DLIGHTS; i++, pl++ )
	{
		if( pl->Expired( )) continue;

		if( pl->type == LIGHT_SPOT || pl->type == LIGHT_OMNI )
		{
			if( !pl->Active( )) continue;

			if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
				continue;

			if( R_CullFrustum( &pl->frustum ))
				continue;

			pglEnable( GL_SCISSOR_TEST );
		}
		else
		{
			// couldn't use scissor for sunlight
			pglDisable( GL_SCISSOR_TEST );
		}

		RI->currentlight = pl;

		// draw world from light position
		BuildMeshListForLight( pl, solid );

		if( !RI->frame.light_meshes.Count( ))
			continue;	// no interaction with this light?

		DrawLightForMeshList( pl );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	pglDisable( GL_SCISSOR_TEST );
	GL_CleanUpTextureUnits( 0 );
	RI->currentlight = NULL;
}

word CStudioModelRenderer :: ShaderSceneForward( mstudiomaterial_t *mat, int lightmode, bool bone_weighting, int numbones )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	bool shader_use_screencopy = false;
	bool using_screenrect = false;
	bool using_cubemaps = false;
	bool has_normalmap = false;

	if( mat->forwardScene.IsValid() && mat->lastRenderMode == RI->currententity->curstate.rendermode )
		return mat->forwardScene.GetHandle(); // valid

	bool flagAdditive = FBitSet(mat->flags, STUDIO_NF_ADDITIVE);
	bool flagMasked = FBitSet(mat->flags, STUDIO_NF_MASKED);
	bool texTransparent = flagAdditive || flagMasked;
	bool texAlphaToCoverage = FBitSet(mat->flags, STUDIO_NF_ALPHATOCOVERAGE);
	bool usingAlphaBlend = texTransparent && FBitSet(mat->flags, STUDIO_NF_HAS_ALPHA) && !texAlphaToCoverage;
	bool usingAberrations = mat->aberrationScale > 0.0f;
	bool usingRefractions = mat->refractScale > 0.0f;

	Q_strncpy( glname, "forward/scene_studio", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( numbones == 1 )
		GL_AddShaderDirective( options, "MAXSTUDIOBONES 1" );

	if( bone_weighting )
		GL_AddShaderDirective( options, "APPLY_BONE_WEIGHTING" );

	if( FBitSet( mat->flags, STUDIO_NF_CHROME ))
		GL_AddShaderDirective( options, "HAS_CHROME" );

	if( RI->currententity->curstate.rendermode == kRenderTransAdd || RI->currententity->curstate.rendermode == kRenderGlow )
	{
		// additive and glow is always fullbright
		GL_AddShaderDirective( options, "LIGHTING_FULLBRIGHT" );

		//if( RI->currententity->curstate.rendermode == kRenderTransAdd )
		//	shader_use_screencopy = true;
	}
	else if( FBitSet( mat->flags, STUDIO_NF_FULLBRIGHT ) || R_FullBright() || FBitSet( RI->currententity->curstate.effects, EF_FULLBRIGHT ))
	{
		GL_AddShaderDirective( options, "LIGHTING_FULLBRIGHT" );
	}
	else
	{
		if (CVAR_TO_BOOL(cv_brdf))
		{
			GL_AddShaderDirective(options, "APPLY_PBS");
			using_cubemaps = true;
		}

		if( lightmode == LIGHTSTATIC_VERTEX )
			GL_AddShaderDirective( options, "VERTEX_LIGHTING" );
		else if( lightmode == LIGHTSTATIC_SURFACE )
			GL_AddShaderDirective( options, "SURFACE_LIGHTING" );
		else if( FBitSet( mat->flags, STUDIO_NF_FLATSHADE ))
			GL_AddShaderDirective( options, "LIGHTING_FLATSHADE" );

		if( lightmode == LIGHTSTATIC_SURFACE )
		{
			// process lightstyles
			for( int i = 0; i < MAXLIGHTMAPS && m_pModelInstance->styles[i] != LS_NONE; i++ )
			{
				if (!R_UseSkyLightstyle(m_pModelInstance->styles[i]))
					continue;	// skip the sunlight due realtime sun is enabled

				GL_AddShaderDirective( options, va( "APPLY_STYLE%i", i ));
			}

			if( FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
				GL_AddShaderDirective( options, "HAS_DELUXEMAP" );
		}

		// debug visualization
		if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
		{
			if( r_lightmap->value == 1.0f && worldmodel->lightdata )
				GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
			else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
				GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
		}

		// deluxemap required
		if( !RP_CUBEPASS() && ( CVAR_TO_BOOL( cv_bump ) && FBitSet( world->features, WORLD_HAS_DELUXEMAP ) && FBitSet( mat->flags, STUDIO_NF_NORMALMAP )))
		{
			has_normalmap = true;
			GL_AddShaderDirective( options, "HAS_NORMALMAP" );
			GL_EncodeNormal( options, mat->gl_normalmap_id );
			GL_AddShaderDirective( options, "COMPUTE_TBN" );
		}

		// parallax mapping
		if (FBitSet(mat->flags, STUDIO_NF_HEIGHTMAP) && mat->reliefScale > 0.0f)
		{
			if (cv_parallax->value > 0.0f)
			{
				if (cv_parallax->value == 1.0f)
					GL_AddShaderDirective(options, "PARALLAX_SIMPLE");
				else if (cv_parallax->value >= 2.0f)
					GL_AddShaderDirective(options, "PARALLAX_OCCLUSION");
			}
		}

		// deluxemap required
		if( !RP_CUBEPASS() && ( CVAR_TO_BOOL( cv_specular ) && FBitSet( world->features, WORLD_HAS_DELUXEMAP ) && FBitSet( mat->flags, STUDIO_NF_GLOSSMAP )))
			GL_AddShaderDirective( options, "HAS_GLOSSMAP" );

		if( FBitSet( mat->flags, STUDIO_NF_LUMA ))
			GL_AddShaderDirective( options, "HAS_LUMA" );
	}

	if (has_normalmap)
	{
		if (usingAberrations) {
			GL_AddShaderDirective(options, "APPLY_ABERRATION");
		}
		if (usingRefractions) {
			GL_AddShaderDirective(options, "APPLY_REFRACTION");
		}
	}

	if(( world->num_cubemaps > 0 ) && CVAR_TO_BOOL( r_cubemap ) && (mat->reflectScale > 0.0f) && !RP_CUBEPASS( ))
	{
		GL_AddShaderDirective( options, "REFLECTION_CUBEMAP" );
		using_cubemaps = true;
	}

	if (tr.fogEnabled && !RP_CUBEPASS())
		GL_AddShaderDirective( options, "APPLY_FOG_EXP" );

	// mixed mode: solid & transparent controlled by alpha-channel
	if (texTransparent && RI->currententity->curstate.rendermode != kRenderGlow)
	{
		if (usingAlphaBlend) {
			GL_AddShaderDirective(options, "ALPHA_BLENDING");
		}
		else if (texAlphaToCoverage && GL_UsingAlphaToCoverage()) 
		{
			if (GL_Support(R_A2C_DITHER_CONTROL)) 
			{
				// enable macro only when we're disabled dithering for A2C
				GL_AddShaderDirective(options, "ALPHA_TO_COVERAGE");
			}
		}
	}

	//if( RI->currententity->curstate.rendermode == kRenderTransColor || RI->currententity->curstate.rendermode == kRenderTransTexture )
	//	shader_use_screencopy = true;

	if (has_normalmap && texTransparent && (usingAberrations || usingRefractions)) {
		shader_use_screencopy = true;
	}

	if( shader_use_screencopy )
		GL_AddShaderDirective( options, "USING_SCREENCOPY" );

	if( FBitSet( mat->flags, STUDIO_NF_HAS_DETAIL ) && CVAR_TO_BOOL( r_detailtextures ))
		GL_AddShaderDirective( options, "HAS_DETAIL" );

	word shaderNum = GL_FindUberShader( glname, options );
	if( !shaderNum )
	{
		SetBits( mat->flags, STUDIO_NF_NODRAW );
		return 0; // something bad happens
	}

	if( shader_use_screencopy )
		GL_AddShaderFeature( shaderNum, SHADER_USE_SCREENCOPY );

	if( using_cubemaps )
		GL_AddShaderFeature( shaderNum, SHADER_USE_CUBEMAPS );

	// done
	mat->lastRenderMode = RI->currententity->curstate.rendermode;
	ClearBits( mat->flags, STUDIO_NF_NODRAW );
	mat->forwardScene.SetShader( shaderNum );

	return shaderNum;
}

word CStudioModelRenderer :: ShaderLightForward( CDynLight *dl, mstudiomaterial_t *mat, bool bone_weighting, int numbones )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	int32_t lightShadowType = !FBitSet(dl->flags, DLF_NOSHADOWS) ? 0 : 1;

	switch( dl->type )
	{
		case LIGHT_SPOT:
			if (mat->forwardLightSpot[lightShadowType].IsValid()) {
				return mat->forwardLightSpot[lightShadowType].GetHandle(); // valid
			}
			break;
		case LIGHT_OMNI:
			if (mat->forwardLightOmni[lightShadowType].IsValid()) {
				return mat->forwardLightOmni[lightShadowType].GetHandle(); // valid
			}
			break;
		case LIGHT_DIRECTIONAL:
			if (mat->forwardLightProj.IsValid()) {
				return mat->forwardLightProj.GetHandle(); // valid
			}
			break;
	}

	Q_strncpy( glname, "forward/light_studio", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	switch( dl->type )
	{
	case LIGHT_SPOT:
		GL_AddShaderDirective( options, "LIGHT_SPOT" );
		break;
	case LIGHT_OMNI:
		GL_AddShaderDirective( options, "LIGHT_OMNI" );
		break;
	case LIGHT_DIRECTIONAL:
		GL_AddShaderDirective( options, "LIGHT_PROJ" );
		break;
	}

	if( numbones == 1 )
		GL_AddShaderDirective( options, "MAXSTUDIOBONES 1" );

	if( bone_weighting )
		GL_AddShaderDirective( options, "APPLY_BONE_WEIGHTING" );

	if( FBitSet( mat->flags, STUDIO_NF_CHROME ))
		GL_AddShaderDirective( options, "HAS_CHROME" );

	if( CVAR_TO_BOOL( cv_brdf ))
		GL_AddShaderDirective( options, "APPLY_PBS" );

	if( FBitSet( mat->flags, STUDIO_NF_HAS_DETAIL ) && CVAR_TO_BOOL( r_detailtextures ))
		GL_AddShaderDirective( options, "HAS_DETAIL" );

	if( CVAR_TO_BOOL( cv_bump ) && FBitSet( mat->flags, STUDIO_NF_NORMALMAP ) && !FBitSet( dl->flags, DLF_NOBUMP ))
	{
		GL_AddShaderDirective( options, "HAS_NORMALMAP" );
		GL_EncodeNormal( options, mat->gl_normalmap_id );
		GL_AddShaderDirective( options, "COMPUTE_TBN" );
	}

	// parallax mapping
	if (FBitSet(mat->flags, STUDIO_NF_HEIGHTMAP) && mat->reliefScale > 0.0f)
	{
		if (cv_parallax->value > 0.0f)
		{
			if (cv_parallax->value == 1.0f)
				GL_AddShaderDirective(options, "PARALLAX_SIMPLE");
			else if (cv_parallax->value >= 2.0f)
				GL_AddShaderDirective(options, "PARALLAX_OCCLUSION");
		}
	}

	// debug visualization
	if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
	{
		if( r_lightmap->value == 1.0f && worldmodel->lightdata )
			GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
		else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
			GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
	}

	if( CVAR_TO_BOOL( cv_specular ) && FBitSet( mat->flags, STUDIO_NF_GLOSSMAP ))
		GL_AddShaderDirective( options, "HAS_GLOSSMAP" );

	if( CVAR_TO_BOOL( r_shadows ) && !FBitSet( dl->flags, DLF_NOSHADOWS ))
	{
		int shadow_smooth_type = static_cast<int>(r_shadows->value);
		if( dl->type == LIGHT_DIRECTIONAL && CVAR_TO_BOOL( r_sunshadows ))
		{
			GL_AddShaderDirective( options, "APPLY_SHADOW" );
			if (shadow_smooth_type == 4)
				GL_AddShaderDirective(options, "SHADOW_VOGEL_DISK");
		}
		else
		{
			GL_AddShaderDirective( options, "APPLY_SHADOW" );
			if (shadow_smooth_type == 2)
				GL_AddShaderDirective(options, "SHADOW_PCF2X2");
			else if (shadow_smooth_type == 3)
				GL_AddShaderDirective(options, "SHADOW_PCF3X3");
			else if (shadow_smooth_type == 4)
				GL_AddShaderDirective(options, "SHADOW_VOGEL_DISK");
		}
	}

	word shaderNum = GL_FindUberShader( glname, options );

	if( !shaderNum )
	{
		if( dl->type == LIGHT_DIRECTIONAL )
			SetBits( mat->flags, STUDIO_NF_NOSUNLIGHT );
		else SetBits( mat->flags, STUDIO_NF_NODLIGHT );

		return 0; // something bad happens
	}

	// done
	switch( dl->type )
	{
	case LIGHT_SPOT:
		mat->forwardLightSpot[lightShadowType].SetShader(shaderNum);
		ClearBits( mat->flags, STUDIO_NF_NODLIGHT );
		break;
	case LIGHT_OMNI:
		mat->forwardLightOmni[lightShadowType].SetShader( shaderNum );
		ClearBits( mat->flags, STUDIO_NF_NODLIGHT );
		break;
	case LIGHT_DIRECTIONAL:
		mat->forwardLightProj.SetShader( shaderNum );
		ClearBits( mat->flags, STUDIO_NF_NOSUNLIGHT );
		break;
	}

	return shaderNum;
}

word CStudioModelRenderer :: ShaderSceneDepth( mstudiomaterial_t *mat, bool bone_weighting, int numbones )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	if( mat->forwardDepth.IsValid( ))
		return mat->forwardDepth.GetHandle(); // valid

	Q_strncpy( glname, "forward/depth_studio", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( numbones == 1 )
		GL_AddShaderDirective( options, "MAXSTUDIOBONES 1" );

	if( bone_weighting )
		GL_AddShaderDirective( options, "APPLY_BONE_WEIGHTING" );

	word shaderNum = GL_FindUberShader( glname, options );
	if( !shaderNum ) return 0; // something bad happens

	mat->forwardDepth.SetShader( shaderNum );

	return shaderNum;
}

void CStudioModelRenderer :: DrawSingleMesh( CSolidEntry *entry, bool force, bool specialPass )
{
	bool	cache_has_changed = false;
	static int cached_lightmap = -1;
	Vector4D	lightstyles, lightdir;
	bool	weapon_model = false;
	float	r, g, b, a, *v;
	int	map;

	if( entry->m_bDrawType != DRAWTYPE_MESH )
		return;

	if( RI->currentmodel != entry->m_pRenderModel )
		cache_has_changed = true;

	if( RI->currententity != entry->m_pParentEntity )
		cache_has_changed = true;

	RI->currentmodel = entry->m_pRenderModel;
	cl_entity_t *e = RI->currententity = entry->m_pParentEntity;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );
	int m_skinnum = bound( 0, RI->currententity->curstate.skin, MAXSTUDIOSKINS - 1 );

	ASSERT( RI->currententity->modelhandle != INVALID_HANDLE );

	ModelInstance_t *inst = m_pModelInstance = &m_ModelInstances[e->modelhandle];
	int num_bones = Q_min( m_pStudioHeader->numbones, glConfig.max_skinning_bones );
	vbomesh_t *pMesh = entry->m_pMesh;
	mstudiomaterial_t *mat;

	if( pMesh->lightmapnum != cached_lightmap )
	{
		cached_lightmap = pMesh->lightmapnum;
		cache_has_changed = true;
	}

	short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

	if( entry->m_pRenderModel == IEngineStudio.GetModelByIndex( e->curstate.weaponmodel ))
		weapon_model = true;

	if (weapon_model)
		mat = &entry->m_pRenderModel->materials[pskinref[pMesh->skinref]];
	else 
		mat = &m_pModelInstance->materials[pskinref[pMesh->skinref]]; // NOTE: use local copy for right cache shadernums

	mstudiolight_t *light = &inst->light;

	if( mat != m_pCurrentMaterial )
		cache_has_changed = true;

	m_pCurrentMaterial = mat;
	bool texHasAlpha = FBitSet(mat->flags, STUDIO_NF_HAS_ALPHA);
	bool texFlagMasked = FBitSet(mat->flags, STUDIO_NF_MASKED);
	bool texAlphaToCoverage = FBitSet(mat->flags, STUDIO_NF_ALPHATOCOVERAGE);

	if( force || ( RI->currentshader != &glsl_programs[entry->m_hProgram] ))
	{
		// force to bind new shader
		GL_BindShader( &glsl_programs[entry->m_hProgram] );	
		cache_has_changed = true;
	}

	bool screenCopyRequired = ScreenCopyRequired(RI->currentshader);
	if( FBitSet( RI->params, RP_SHADOWVIEW ))
	{
		if (texFlagMasked)
		{
			pglAlphaFunc( GL_GEQUAL, 0.5f );
			GL_AlphaTest( GL_TRUE );
		}
		else if (texHasAlpha)
		{
			pglAlphaFunc( GL_GEQUAL, 0.999f );
			GL_AlphaTest( GL_TRUE );
		}
		else GL_AlphaTest( GL_FALSE );
	}
	else
	{
		if (!specialPass)
		{
			if (!screenCopyRequired)
			{
				if (texFlagMasked && texHasAlpha && !texAlphaToCoverage)
				{
					GL_AlphaTest(GL_FALSE);
					GL_Blend(GL_TRUE);
				}
				else if (texFlagMasked)
				{
					GL_AlphaTest(GL_TRUE);
					GL_Blend(GL_FALSE);
				}
				else
				{
					GL_AlphaTest(GL_FALSE);
					GL_Blend(GL_FALSE);
				}
			}
			else
			{
				// we don't need alpha test nor alpha blending, because screencopy used for that
				GL_AlphaTest(GL_FALSE);
				GL_Blend(GL_FALSE);
			}
		}
		else 
		{
			// in shadow/light pass we shouldn't use alpha blending
			if (FBitSet(mat->flags, STUDIO_NF_MASKED))
				GL_AlphaTest(GL_TRUE);
			else
				GL_AlphaTest(GL_FALSE);
		}
	}

	if (!specialPass)
	{
		if (texFlagMasked && texHasAlpha && texAlphaToCoverage) {
			GL_AlphaToCoverage(true);
		}
		else {
			GL_AlphaToCoverage(false);
		}
	}

	if (FBitSet(mat->flags, STUDIO_NF_TWOSIDE)) {
		GL_Cull(GL_NONE);
	}
	else {
		GL_Cull(GL_FRONT);
	}

	// each transparent surfaces reqiured an actual screencopy
	if (screenCopyRequired && entry->IsTranslucent())
	{
		CTransEntry *trans = (CTransEntry *)entry;
		if (trans->m_bScissorReady)
		{
			trans->RequestScreencopy();
			cache_has_changed = true; // force to refresh uniforms
			r_stats.c_screen_copy++;
		}
	}

	glsl_program_t *shader = RI->currentshader;
	CDynLight *pl = RI->currentlight;	// may be NULL

	// sometime we can't set the uniforms
	if( !cache_has_changed || !shader || !shader->numUniforms || !shader->uniforms )
	{
		// just draw mesh and out
		DrawMeshFromBuffer( pMesh );
		return;
	}

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			u->SetValue( mat->gl_diffuse_id.ToInt() );
			break;
		case UT_NORMALMAP:
			u->SetValue( mat->gl_normalmap_id.ToInt() );
			break;
		case UT_GLOSSMAP:
			u->SetValue( mat->gl_specular_id.ToInt() );
			break;
		case UT_DETAILMAP:
			u->SetValue( mat->gl_detailmap_id.ToInt() );
			break;
		case UT_PROJECTMAP:
			if( pl && pl->type == LIGHT_SPOT )
				u->SetValue( pl->spotlightTexture.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SHADOWMAP:
		case UT_SHADOWMAP0:
			if( pl ) u->SetValue( pl->shadowTexture[0].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP1:
			if( pl ) u->SetValue( pl->shadowTexture[1].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP2:
			if( pl ) u->SetValue( pl->shadowTexture[2].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP3:
			if( pl ) u->SetValue( pl->shadowTexture[3].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_LIGHTMAP:
			if( pMesh->lightmapnum != -1 )
				u->SetValue( tr.lightmaps[pMesh->lightmapnum].lightmap.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_DELUXEMAP:
			if( pMesh->lightmapnum != -1 )
				u->SetValue( tr.lightmaps[pMesh->lightmapnum].deluxmap.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_DECALMAP:
			// unacceptable for studiomodels
			u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SCREENMAP:
			u->SetValue( tr.screen_color.ToInt() );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.screen_depth.ToInt() );
			break;
		case UT_ENVMAP0:
		case UT_ENVMAP:
			if (!RP_CUBEPASS() && inst->cubemap[0] != NULL) {
				u->SetValue(inst->cubemap[0]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_ENVMAP1:
			if (!RP_CUBEPASS() && inst->cubemap[1] != NULL) {
				u->SetValue(inst->cubemap[1]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL0:
			if (!RP_CUBEPASS() && inst->cubemap[0] != NULL) {
				u->SetValue(inst->cubemap[0]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL1:
			if (!RP_CUBEPASS() && inst->cubemap[1] != NULL) {
				u->SetValue(inst->cubemap[1]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_BRDFAPPROXMAP:
			u->SetValue(tr.brdfApproxTexture.ToInt());
			break;
		case UT_GLOWMAP:
			u->SetValue( mat->gl_glowmap_id.ToInt() );
			break;
		case UT_HEIGHTMAP:
			u->SetValue( mat->gl_heightmap_id.ToInt() );
			break;
		case UT_RELIEFPARAMS:
		{
			float width = mat->gl_heightmap_id.GetWidth();
			float height = mat->gl_heightmap_id.GetHeight();
			u->SetValue(width, height, mat->reliefScale, cv_shadow_offset->value);
			break;
		}
		case UT_FITNORMALMAP:
			u->SetValue( tr.normalsFitting.ToInt() );
			break;
		case UT_MODELMATRIX:
			u->SetValue( &inst->m_glmatrix[0] );
			break;
		case UT_BONESARRAY:
			if( weapon_model )
				u->SetValue( &inst->m_glweaponbones[0][0], num_bones * 3 );
			else u->SetValue( &inst->m_glstudiobones[0][0], num_bones * 3 );
			break;
		case UT_BONEQUATERNION:
			if( weapon_model )
				u->SetValue( &inst->m_weaponquat[0][0], num_bones );
			else u->SetValue( &inst->m_studioquat[0][0], num_bones );
			break;
		case UT_BONEPOSITION:
			if( weapon_model )
				u->SetValue( &inst->m_weaponpos[0][0], num_bones );
			else u->SetValue( &inst->m_studiopos[0][0], num_bones );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_ZFAR:
			u->SetValue( -tr.farclip * 1.74f );
			break;
		case UT_LIGHTSTYLES:
			for( map = 0; map < MAXLIGHTMAPS; map++ )
			{
				if( inst->styles[map] != 255 )
					lightstyles[map] = tr.lightstyle[inst->styles[map]];
				else lightstyles[map] = 0.0f;
			}
			u->SetValue( lightstyles.x, lightstyles.y, lightstyles.z, lightstyles.w );
			break;
		case UT_LIGHTSTYLEVALUES:
			u->SetValue( &tr.lightstyle[0], MAX_LIGHTSTYLES );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_DETAILSCALE:
			u->SetValue( mat->detailScale[0], mat->detailScale[1] );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		case UT_SHADOWPARMS:
			if( pl != NULL )
			{
				float shadowWidth = 1.0f / (float)pl->shadowTexture[0].GetWidth();
				float shadowHeight = 1.0f / (float)pl->shadowTexture[0].GetHeight();
				// depth scale and bias and shadowmap resolution
				u->SetValue( shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
			}
			else u->SetValue( 0.0f, 0.0f, 0.0f, 0.0f );
			break;
		case UT_TEXOFFSET: // FIXME: implement conveyors?
			u->SetValue( 0.0f, 0.0f );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( GetVieworg().x, GetVieworg().y, GetVieworg().z );
			break;
		case UT_VIEWRIGHT:
			u->SetValue( GetVRight().x, GetVRight().y, GetVRight().z );
			break;
		case UT_RENDERCOLOR:
			if( e->curstate.rendermode == kRenderNormal )
			{
				r = g = b = a = 1.0f;
			}
			else
			{
				int sum = (e->curstate.rendercolor.r + e->curstate.rendercolor.g + e->curstate.rendercolor.b);

				if( sum > 0 )
				{
					r = e->curstate.rendercolor.r / 255.0f;
					g = e->curstate.rendercolor.g / 255.0f;
					b = e->curstate.rendercolor.b / 255.0f;
				}
				else
				{
					r = g = b = 1.0f;
				}

				if( e->curstate.rendermode != kRenderTransAlpha )
					a = e->curstate.renderamt / 255.0f;
				else a = 1.0f;
			}

			if( FBitSet( mat->flags, STUDIO_NF_ADDITIVE ))
				a = 0.65f; // FIXME: 0.5 looks ugly
			u->SetValue( r, g, b, a );
			break;
		case UT_SMOOTHNESS:
			u->SetValue( mat->smoothness );
			break;
		case UT_SHADOWMATRIX:
			if( pl ) u->SetValue( &pl->gl_shadowMatrix[0][0], MAX_SHADOWMAPS );
			break;
		case UT_SHADOWSPLITDIST:
			v = RI->view.parallelSplitDistances;
			u->SetValue( v[0], v[1], v[2], v[3] );
			break;
		case UT_TEXELSIZE:
			u->SetValue( 1.0f / (float)sunSize[0], 1.0f / (float)sunSize[1], 1.0f / (float)sunSize[2], 1.0f / (float)sunSize[3] );
			break;
		case UT_LIGHTGAMMA:
			u->SetValue(tr.light_gamma);
			break;
		case UT_LIGHTDIR:
			if( pl )
			{
				if( pl->type == LIGHT_DIRECTIONAL ) lightdir = -tr.sky_normal;
				else lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
				u->SetValue( lightdir.x, lightdir.y, lightdir.z, pl->fov );
			}
			else u->SetValue( light->normal.x, light->normal.y, light->normal.z );
			break;
		case UT_LIGHTDIFFUSE:
			if( pl ) u->SetValue( pl->color.x, pl->color.y, pl->color.z );
			else u->SetValue( light->diffuse.x, light->diffuse.y, light->diffuse.z );
			break;
		case UT_LIGHTSHADE:
			u->SetValue( (float)light->ambientlight / 255.0f, (float)light->shadelight / 255.0f );
			break;
		case UT_LIGHTORIGIN:
			if( pl ) u->SetValue( pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius )); 
			break;
		case UT_LIGHTVIEWPROJMATRIX:
			if( pl )
			{
				GLfloat gl_lightViewProjMatrix[16];
				pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );
				u->SetValue( &gl_lightViewProjMatrix[0] );
			}
			break;
		case UT_AMBIENTFACTOR:
			if( pl && pl->type == LIGHT_DIRECTIONAL )
				u->SetValue( tr.sun_ambient );
			else u->SetValue( tr.ambientFactor );
			break;
		case UT_SUNREFRACT:
			u->SetValue( tr.sun_refract );
			break;
		case UT_AMBIENTCUBE:
			u->SetValue( &light->ambient[0][0], 6 );
			break;
		case UT_LERPFACTOR:
			u->SetValue( inst->lerpFactor );
#if 0
			ALERT(at_console, "lerp %f, c1 %d, c2 %d\n", inst->lerpFactor, inst->cubemap[0]->texture, inst->cubemap[1]->texture);
#endif
			break;
		case UT_REFRACTSCALE:
			u->SetValue( bound( 0.0f, mat->refractScale, 1.0f ));
			break;
		case UT_REFLECTSCALE:
			u->SetValue( bound( 0.0f, mat->reflectScale, 1.0f ));
			break;
		case UT_ABERRATIONSCALE:
			u->SetValue( bound( 0.0f, mat->aberrationScale, 1.0f ));
			break;
		case UT_BOXMINS:
			if( world->num_cubemaps > 0 )
			{
				Vector mins[2];
				mins[0] = inst->cubemap[0]->mins;
				mins[1] = inst->cubemap[1]->mins;
				u->SetValue( &mins[0][0], 2 );
			}
			break;
		case UT_BOXMAXS:
			if( world->num_cubemaps > 0 )
			{
				Vector maxs[2];
				maxs[0] = inst->cubemap[0]->maxs;
				maxs[1] = inst->cubemap[1]->maxs;
				u->SetValue( &maxs[0][0], 2 );
			}
			break;
		case UT_CUBEORIGIN:
			if( world->num_cubemaps > 0 )
			{
				Vector origin[2];
				origin[0] = inst->cubemap[0]->origin;
				origin[1] = inst->cubemap[1]->origin;
				u->SetValue( &origin[0][0], 2 );
			}
			break;
		case UT_CUBEMIPCOUNT:
			if( world->num_cubemaps > 0 )
			{
				r = Q_max( 1, inst->cubemap[0]->numMips - cv_cube_lod_bias->value );
				g = Q_max( 1, inst->cubemap[1]->numMips - cv_cube_lod_bias->value );
				u->SetValue( r, g );
			}
			break;
		case UT_LIGHTNUMS0:
			u->SetValue( (float)inst->lights[0], (float)inst->lights[1], (float)inst->lights[2], (float)inst->lights[3] );
			break;
		case UT_LIGHTNUMS1:
			u->SetValue( (float)inst->lights[4], (float)inst->lights[5], (float)inst->lights[6], (float)inst->lights[7] );
			break;
		case UT_LIGHTSCALE:
			u->SetValue( tr.direct_scale );
			break;
		case UT_LIGHTTHRESHOLD:
			u->SetValue( tr.light_threshold );
			break;
		case UT_SWAYHEIGHT:
			u->SetValue(mat->swayHeight);
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}

	DrawMeshFromBuffer( pMesh );
}

void CStudioModelRenderer :: RenderSolidStudioList( void )
{
	if( !RI->frame.solid_meshes.Count() )
		return;

	pglAlphaFunc( GL_GEQUAL, 0.5f );
	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_TRUE );

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( cv_nosort ))
		RI->frame.solid_meshes.Sort( SortSolidMeshes );

	RI->currententity = NULL;
	RI->currentmodel = NULL;
	m_pCurrentMaterial = NULL;

	int i;

	for( i = 0; i < RI->frame.solid_meshes.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.solid_meshes[i];

		if( entry->m_bDrawType != DRAWTYPE_MESH )
			continue;

		if( m_iDrawModelType == DRAWSTUDIO_NORMAL )
		{
			GL_DepthRange( gldepthmin, gldepthmax );
			GL_ClipPlane( true );
		}

		DrawSingleMesh(entry, (i == 0), false);
	}

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglDisable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	if( m_iDrawModelType == DRAWSTUDIO_NORMAL )
		GL_DepthRange( gldepthmin, gldepthmax );

	GL_AlphaToCoverage( false );
	GL_AlphaTest( GL_FALSE );
	GL_ClipPlane( true );
	GL_Cull( GL_FRONT );

	RenderDynLightList( true );

	GL_CleanupDrawState();

	// now draw studio decals (unsorted)
	for( i = 0; i < RI->frame.solid_meshes.Count(); i++ )
	{
		DrawDecal( &RI->frame.solid_meshes[i] );
	}
}

void CStudioModelRenderer :: RenderTransMesh( CTransEntry *entry )
{
	if( entry->m_bDrawType != DRAWTYPE_MESH )
		return;

	GL_DEBUG_SCOPE();
	pglAlphaFunc( GL_GEQUAL, 0.5f );
	GL_DepthMask( GL_TRUE );

	if( m_iDrawModelType == DRAWSTUDIO_NORMAL )
	{
		GL_DepthRange( gldepthmin, gldepthmax );
		GL_ClipPlane( true );
	}

	if( entry->m_pParentEntity->curstate.rendermode == kRenderGlow )
	{
		pglBlendFunc( GL_ONE, GL_ONE );
		if( FBitSet( entry->m_pParentEntity->curstate.effects, EF_NODEPTHTEST ))
			pglDisable( GL_DEPTH_TEST );
	}
	else
	{
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglEnable( GL_DEPTH_TEST );
	}

	// draw decals behind the glass
	DrawDecal( entry, GL_BACK );

	DrawSingleMesh( entry, true, false );
	GL_AlphaToCoverage( false );

	// draw decals that lies on glass
	DrawDecal( entry, GL_FRONT );

	pglEnable( GL_DEPTH_TEST );
	GL_Blend( GL_FALSE );
	GL_ClipPlane( true );
}

void CStudioModelRenderer :: RenderShadowStudioList( void )
{
	if( !RI->frame.solid_meshes.Count() )
		return;

	RI->currententity = NULL;
	RI->currentmodel = NULL;
	m_pCurrentMaterial = NULL;
	GL_DEBUG_SCOPE();

	for( int i = 0; i < RI->frame.solid_meshes.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.solid_meshes[i];
		DrawSingleMesh(entry, ( i == 0 ), true);
	}

	GL_AlphaToCoverage( false );
	GL_AlphaTest( GL_FALSE );
	GL_CleanupDrawState();
}

void CStudioModelRenderer :: RenderDebugStudioList( bool bViewModel )
{
	int i;

	if( r_drawentities->value <= 1.0f ) return;

	if( FBitSet( RI->params, RP_SHADOWVIEW|RP_ENVVIEW|RP_SKYVIEW ))
		return;

	GL_DEBUG_SCOPE();
	GL_CleanupDrawState();

	if( bViewModel )
	{
		StudioDrawDebug( GET_VIEWMODEL( ));
	}
	else
	{
		// render debug lines
		for( i = 0; i < tr.num_draw_entities; i++ )
			StudioDrawDebug( tr.draw_entities[i] );
	}
}

void CStudioModelRenderer :: ClearLightCache( void )
{
	for (int i = 0; i < m_ModelInstances.Count(); i++)
	{
		ModelInstance_t *instance = &m_ModelInstances[i];
		instance->light_update = true;
	}

	// clear models lightmaps cache
	for (int i = 0; i < MAX_LIGHTCACHE; i++)
	{
		mstudiocache_t *cache = tr.surface_light_cache[i];

		if (!cache || cache->surfaces.empty())
			continue;

		for (int j = 0; j < cache->surfaces.size(); j++)
		{
			mstudiosurface_t *surf = &cache->surfaces[j];
			SetBits(surf->flags, SURF_LM_UPDATE | SURF_GRASS_UPDATE);
		}
		cache->update_light = true;
	}
}