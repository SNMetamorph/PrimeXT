/*
r_studio.cpp - studio model rendering
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
#include "com_model.h"
#include "r_studioint.h"
#include "pm_movevars.h"
#include "r_studio.h"
#include "r_sprite.h"
#include "event_api.h"
#include <mathlib.h>
#include "pm_defs.h"
#include "triangleapi.h"
#include "entity_types.h"
#include "r_shader.h"
#include "r_world.h"

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

// the renderer object, created on the stack.
CStudioModelRenderer g_StudioRenderer;

//================================================================================================
//			HUD_GetStudioModelInterface
//	Export this function for the engine to use the studio renderer class to render objects.
//================================================================================================
int HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio )
{
	if( version != STUDIO_INTERFACE_VERSION )
		return 0;

	// Copy in engine helper functions
	memcpy( &IEngineStudio, pstudio, sizeof( IEngineStudio ));

	if( g_fRenderInitialized )
	{
		// Initialize local variables, etc.
		g_StudioRenderer.Init();

		g_SpriteRenderer.Init();
	}

	// Success
	return 1;
}

//================================================================================================
//
//	Implementation of bone setup class
//
//================================================================================================
void CBaseBoneSetup :: debugMsg( char *szFmt, ... )
{
	char	buffer[2048];	// must support > 1k messages
	va_list	args;

	if( developer_level <= DEV_NONE )
		return;

	va_start( args, szFmt );
	Q_vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	gEngfuncs.Con_Printf( buffer );
}

mstudioanim_t *CBaseBoneSetup :: GetAnimSourceData( mstudioseqdesc_t *pseqdesc )
{
	return g_StudioRenderer.StudioGetAnim( RI->currentmodel, pseqdesc );
}

void CBaseBoneSetup :: debugLine( const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration )
{
	if( noDepthTest )
		pglDisable( GL_DEPTH_TEST );

	pglColor3ub( r, g, b );

	pglBegin( GL_LINES );
		pglVertex3fv( origin );
		pglVertex3fv( dest );
	pglEnd();
}

/*
====================
Init

====================
*/
void CStudioModelRenderer :: Init( void )
{
	// Set up some variables shared with engine
	m_pCvarHiModels		= IEngineStudio.GetCvar( "cl_himodels" );
	m_pCvarDrawViewModel	= IEngineStudio.GetCvar( "r_drawviewmodel" );
	m_pCvarGlowShellFreq	= IEngineStudio.GetCvar( "r_glowshellfreq" );
	m_pCvarHand		= CVAR_REGISTER( "cl_righthand", "0", FCVAR_ARCHIVE );
	m_pCvarViewmodelFov		= CVAR_REGISTER( "cl_viewmodel_fov", "90", FCVAR_ARCHIVE );
	m_pCvarCompatible		= CVAR_REGISTER( "r_studio_compatible", "1", FCVAR_ARCHIVE );
	m_pCvarLodScale		= CVAR_REGISTER( "cl_lod_scale", "5.0", FCVAR_ARCHIVE );
	m_pCvarLodBias		= CVAR_REGISTER( "cl_lod_bias", "0", FCVAR_ARCHIVE );

	m_pChromeSprite		= IEngineStudio.GetChromeSprite();
	m_chromeCount		= 0;
}

/*
====================
VidInit

====================
*/
void CStudioModelRenderer :: VidInit( void )
{
}

/*
====================
CStudioModelRenderer

====================
*/
CStudioModelRenderer :: CStudioModelRenderer( void )
{
	m_fDoInterp	= true;
	m_pCurrentEntity	= NULL;
	m_pCvarHiModels	= NULL;
	m_pCvarDrawViewModel= NULL;
	m_pCvarHand	= NULL;
	m_pChromeSprite	= NULL;
	m_pStudioHeader	= NULL;
	m_pBodyPart	= NULL;
	m_pSubModel	= NULL;
	m_pPlayerInfo	= NULL;
	m_pRenderModel	= NULL;
}

/*
====================
~CStudioModelRenderer

====================
*/
CStudioModelRenderer :: ~CStudioModelRenderer( void )
{
}

/*
====================
Prepare all the pointers for
working with current entity

====================
*/
bool CStudioModelRenderer :: StudioSetEntity( cl_entity_t *pEnt )
{
	if( !pEnt || !pEnt->model || pEnt->model->type != mod_studio )
		return false;

	m_pCurrentEntity = RI->currententity = pEnt;
	SET_CURRENT_ENTITY( m_pCurrentEntity );
	m_pPlayerInfo = NULL;

	if( !m_fDrawViewModel && ( m_pCurrentEntity->player || m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer ))
	{
		int	iPlayerIndex;

		if( m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer )
			iPlayerIndex = m_pCurrentEntity->curstate.renderamt - 1;
		else iPlayerIndex = m_pCurrentEntity->curstate.number - 1;
          
		if( iPlayerIndex < 0 || iPlayerIndex >= GET_MAX_CLIENTS( ))
			return false;

		if( RP_NORMALPASS() && RP_LOCALCLIENT( m_pCurrentEntity ) && !FBitSet( RI->params, RP_THIRDPERSON ))
		{
			// hide playermodel in firstperson
			return false;
		}
		else
		{
			RI->currentmodel = m_pRenderModel = IEngineStudio.SetupPlayerModel( iPlayerIndex );

			// show highest resolution multiplayer model
			if( CVAR_TO_BOOL( m_pCvarHiModels ) && m_pRenderModel != m_pCurrentEntity->model )
				m_pCurrentEntity->curstate.body = 255;

			if( !( !developer_level && GET_MAX_CLIENTS() == 1 ) && ( m_pRenderModel == m_pCurrentEntity->model ))
				m_pCurrentEntity->curstate.body = 1; // force helmet
		}

		// setup the playerinfo
		if( m_pCurrentEntity->player )
			m_pPlayerInfo = IEngineStudio.PlayerInfo( iPlayerIndex );
	}
	else
	{
		RI->currentmodel = m_pRenderModel = RI->currententity->model;
	}

	if( m_pRenderModel == NULL )
		return false;

	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

 	// downloading in-progress ?
	if( m_pStudioHeader == NULL )
		return false;

	// tell the engine about model
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	if( !StudioSetupInstance( ))
	{
		ALERT( at_error, "Couldn't create instance for entity %d\n", pEnt->index );
		return false; // out of memory ?
	}

	// all done
	return true;
}

//-----------------------------------------------------------------------------
// Fast version without data reconstruction or changing
//-----------------------------------------------------------------------------
bool CStudioModelRenderer :: StudioSetEntity( gl_studiomesh_t *entry )
{
	studiohdr_t *phdr;

	if( !entry || !entry->parent || !entry->model )
		return false; // bad entry?

	if( entry->parent->modelhandle == INVALID_HANDLE )
		return false; // not initialized?

	if(( phdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel )) == NULL )
		return false; // no model?

	RI->currentmodel = m_pRenderModel = entry->model;
	RI->currententity = m_pCurrentEntity = entry->parent;
	m_pModelInstance = &m_ModelInstances[entry->parent->modelhandle];
	m_pStudioHeader = phdr;
	m_pPlayerInfo = NULL;

	return true;
}

bool CStudioModelRenderer :: StudioSetupInstance( void )
{
	// first call ?
	if( m_pCurrentEntity->modelhandle == INVALID_HANDLE )
	{
		m_pCurrentEntity->modelhandle = m_ModelInstances.AddToTail();

		if( m_pCurrentEntity->modelhandle == INVALID_HANDLE )
			return false; // out of memory ?

		m_pModelInstance = &m_ModelInstances[m_pCurrentEntity->modelhandle];
		ClearInstanceData( true );
	}
	else
	{
		m_pModelInstance = &m_ModelInstances[m_pCurrentEntity->modelhandle];

		// model has been changed or something like
		if( !IsModelInstanceValid( m_pModelInstance ))
			ClearInstanceData( false );
	}

	m_boneSetup.SetStudioPointers( m_pStudioHeader, m_pModelInstance->m_poseparameter );

	return true;
}

//-----------------------------------------------------------------------------
// It's not valid if the model index changed + we have non-zero instance data
//-----------------------------------------------------------------------------
bool CStudioModelRenderer :: IsModelInstanceValid( ModelInstance_t *inst )
{
	const model_t *pModel;

	if( !m_fDrawViewModel && ( inst->m_pEntity->player || m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer ))
	{
		if( m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer )
			pModel = IEngineStudio.SetupPlayerModel( inst->m_pEntity->curstate.renderamt - 1 );
		else pModel = IEngineStudio.SetupPlayerModel( inst->m_pEntity->curstate.number - 1 );
	}
	else pModel = inst->m_pEntity->model;

	return inst->m_pModel == pModel;
}

void CStudioModelRenderer :: DestroyInstance( word handle )
{
	if( !m_ModelInstances.IsValidIndex( handle ))
		return;

	ModelInstance_t *inst = &m_ModelInstances[handle];

	DestroyDecalList( inst->m_DecalHandle );
	inst->m_DecalHandle = INVALID_HANDLE;

	if( inst->materials != NULL )
		Mem_Free( inst->materials );
	inst->materials = NULL;

	if( inst->m_pJiggleBones != NULL )
		delete inst->m_pJiggleBones;
	inst->m_pJiggleBones = NULL;

	m_ModelInstances.Remove( handle );
}

void CStudioModelRenderer :: UpdateInstanceMaterials( void )
{
	ASSERT( m_pStudioHeader != NULL );
	ASSERT( m_pModelInstance != NULL );

	// model was changed, so we need to realloc materials
	if( m_pModelInstance->materials != NULL )
		Mem_Free( m_pModelInstance->materials );

	// create a local copy of all the model material for cache uber-shaders
	m_pModelInstance->materials = (mstudiomaterial_t *)Mem_Alloc( sizeof( mstudiomaterial_t ) * m_pStudioHeader->numtextures );
	memcpy( m_pModelInstance->materials, m_pRenderModel->materials, sizeof( mstudiomaterial_t ) * m_pStudioHeader->numtextures );

	// invalidate sequences when a new instance was created
	for( int i = 0; i < m_pStudioHeader->numtextures; i++ )
	{
		m_pModelInstance->materials[i].glsl_sequence = -1;
		m_pModelInstance->materials[i].glsl_sequence_omni = -1;
		m_pModelInstance->materials[i].glsl_sequence_proj[0] = -1;
		m_pModelInstance->materials[i].glsl_sequence_proj[1] = -1;
	}
}

void CStudioModelRenderer :: ClearInstanceData( bool create )
{
	if( create )
	{
		m_pModelInstance->m_pJiggleBones = NULL;
		m_pModelInstance->materials = NULL;
	}
	else
	{
		DestroyDecalList( m_pModelInstance->m_DecalHandle );
		if( m_pModelInstance->m_pJiggleBones != NULL )
			delete m_pModelInstance->m_pJiggleBones;
		m_pModelInstance->m_pJiggleBones = NULL;
	}

	m_pModelInstance->m_pEntity = m_pCurrentEntity;
	m_pModelInstance->m_pModel = m_pRenderModel;
	m_pModelInstance->m_DecalHandle = INVALID_HANDLE;
	m_pModelInstance->m_VlCache = NULL;
	m_pModelInstance->m_DecalCount = 0;
	m_pModelInstance->cached_frame = -1;
	m_pModelInstance->visframe = -1;
	m_pModelInstance->radius = 0.0f;
	m_pModelInstance->info_flags = 0;

	ClearBounds( m_pModelInstance->absmin, m_pModelInstance->absmax );
	memset( &m_pModelInstance->bonecache, 0, sizeof( BoneCache_t ));
	memset( m_pModelInstance->m_protationmatrix, 0, sizeof( matrix3x4 ));
	memset( m_pModelInstance->m_pbones, 0, sizeof( matrix3x4 ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_pwpnbones, 0, sizeof( matrix3x4 ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->attachment, 0, sizeof( StudioAttachment_t ) * MAXSTUDIOATTACHMENTS );
	memset( m_pModelInstance->m_studioquat, 0, sizeof( Vector4D ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_studiopos, 0, sizeof( Vector ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_weaponquat, 0, sizeof( Vector4D ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_weaponpos, 0, sizeof( Vector ) * MAXSTUDIOBONES );
	memset( &m_pModelInstance->lighting, 0, sizeof( mstudiolight_t ));

	memset( &m_pModelInstance->lerp, 0, sizeof( mstudiolerp_t ));
	memset( &m_pModelInstance->m_controller, 0, sizeof( m_pModelInstance->m_controller ));
	memset( &m_pModelInstance->m_seqblend, 0, sizeof( m_pModelInstance->m_seqblend ));
	m_pModelInstance->m_bProceduralBones = false;
	m_pModelInstance->m_current_seqblend = 0;
	m_pModelInstance->lerp.stairoldz = m_pCurrentEntity->origin[2];
	m_pModelInstance->lerp.stairtime = tr.time;
	m_pModelInstance->m_DecalHandle = INVALID_HANDLE;
	m_pModelInstance->m_pModel = m_pRenderModel;
	m_pModelInstance->m_flLastBoneUpdate = 0.0f;
	m_pModelInstance->m_pJiggleBones = NULL;
	m_pModelInstance->cached_frame = -1;
	m_pModelInstance->m_DecalCount = 0;
	m_pModelInstance->visframe = -1;
	m_pModelInstance->radius = 0.0f;
	m_pModelInstance->info_flags = 0;

	m_boneSetup.SetStudioPointers( m_pStudioHeader, m_pModelInstance->m_poseparameter );

	// set poseparam sliders to their default values
	m_boneSetup.CalcDefaultPoseParameters( m_pModelInstance->m_poseparameter );

	// refresh the materials list
	UpdateInstanceMaterials();

	// copy attachments names
	mstudioattachment_t *pattachment = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	StudioAttachment_t *att = m_pModelInstance->attachment;

	// setup attachment names
	for( int i = 0; i < Q_min( MAXSTUDIOATTACHMENTS, m_pStudioHeader->numattachments ); i++ )
		Q_strncpy( att[i].name, pattachment[i].name, sizeof( att[0].name ));
	m_pModelInstance->numattachments = m_pStudioHeader->numattachments;

	for( int map = 0; map < MAXLIGHTMAPS; map++ )
		m_pModelInstance->styles[map] = 255;
}

void CStudioModelRenderer :: ProcessUserData( model_t *mod, qboolean create, const byte *buffer )
{
	m_pRenderModel = mod;

	if( !( m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel )))
		return;

	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	if( create )
	{
		// compute model CRC to verify vertexlighting data
		// NOTE: source buffer is not equal to Mod_Extradata!
		studiohdr_t *src = (studiohdr_t *)buffer;
		m_pRenderModel->modelCRC = FILE_CRC32( buffer, src->length );
		double start = Sys_DoubleTime();
		m_pRenderModel->studiocache = CreateMeshCache();
		double end = Sys_DoubleTime();
		tr.buildtime += (end - start);
	}
	else
	{
		DestroyMeshCache();
	}
}

bool CStudioModelRenderer :: CheckBoneCache( float f )
{
	if( m_fShootDecal || !m_pModelInstance )
		return false;

	if( m_pModelInstance->m_bProceduralBones )
		return false; // need to be updated every frame

	BoneCache_t *cache = &m_pModelInstance->bonecache;
	cl_entity_t *e = m_pCurrentEntity;

	bool pos_valid = (cache->transform == m_pModelInstance->m_protationmatrix) ? true : false;

	// make sure what all cached values are unchanged
	if( cache->frame == f && cache->sequence == e->curstate.sequence && pos_valid && !memcmp( cache->blending, e->curstate.blending, 2 )
	&& !memcmp( cache->controller, e->curstate.controller, 4 ) && cache->mouthopen == e->mouth.mouthopen )
	{
		if( m_pPlayerInfo )
		{
			if( cache->gaitsequence == m_pPlayerInfo->gaitsequence && cache->gaitframe == m_pPlayerInfo->gaitframe )
				return true;
		}
		else
		{
			// cache are valid
			return true;
		}
	}

	// update bonecache
	cache->frame = f;
	cache->mouthopen = e->mouth.mouthopen;
	cache->sequence = e->curstate.sequence;
	cache->transform = m_pModelInstance->m_protationmatrix;
	memcpy( cache->blending, e->curstate.blending, 2 );
	memcpy( cache->controller, e->curstate.controller, 4 );

	if( m_pPlayerInfo )
	{
		cache->gaitsequence = m_pPlayerInfo->gaitsequence;
		cache->gaitframe = m_pPlayerInfo->gaitframe;
	}

	return false;
}

void CStudioModelRenderer :: LoadLocalMatrix( int bone, mstudioboneinfo_t *boneinfo )
{
	mposetobone_t *m = m_pRenderModel->poseToBone;

	// transform Valve matrix to Xash matrix
	m->posetobone[bone][0][0] = boneinfo->poseToBone[0][0];
	m->posetobone[bone][0][1] = boneinfo->poseToBone[1][0];
	m->posetobone[bone][0][2] = boneinfo->poseToBone[2][0];

	m->posetobone[bone][1][0] = boneinfo->poseToBone[0][1];
	m->posetobone[bone][1][1] = boneinfo->poseToBone[1][1];
	m->posetobone[bone][1][2] = boneinfo->poseToBone[2][1];

	m->posetobone[bone][2][0] = boneinfo->poseToBone[0][2];
	m->posetobone[bone][2][1] = boneinfo->poseToBone[1][2];
	m->posetobone[bone][2][2] = boneinfo->poseToBone[2][2];

	m->posetobone[bone][3][0] = boneinfo->poseToBone[0][3];
	m->posetobone[bone][3][1] = boneinfo->poseToBone[1][3];
	m->posetobone[bone][3][2] = boneinfo->poseToBone[2][3];
}

void CStudioModelRenderer :: ComputeSkinMatrix( mstudioboneweight_t *boneweights, const matrix3x4 worldtransform[], matrix3x4 &result )
{
	float	flWeight0, flWeight1, flWeight2, flWeight3;
	int	numbones = 0;
	float	flTotal;

	for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
	{
		if( boneweights->bone[i] != -1 )
			numbones++;
	}

	if( numbones == 4 )
	{
		const matrix3x4 &boneMat0 = worldtransform[boneweights->bone[0]];
		const matrix3x4 &boneMat1 = worldtransform[boneweights->bone[1]];
		const matrix3x4 &boneMat2 = worldtransform[boneweights->bone[2]];
		const matrix3x4 &boneMat3 = worldtransform[boneweights->bone[3]];
		flWeight0 = boneweights->weight[0] / 255.0f;
		flWeight1 = boneweights->weight[1] / 255.0f;
		flWeight2 = boneweights->weight[2] / 255.0f;
		flWeight3 = boneweights->weight[3] / 255.0f;
		flTotal = flWeight0 + flWeight1 + flWeight2 + flWeight3;

		if( flTotal < 1.0f ) flWeight0 += 1.0f - flTotal;	// compensate rounding error

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
		const matrix3x4 &boneMat0 = worldtransform[boneweights->bone[0]];
		const matrix3x4 &boneMat1 = worldtransform[boneweights->bone[1]];
		const matrix3x4 &boneMat2 = worldtransform[boneweights->bone[2]];
		flWeight0 = boneweights->weight[0] / 255.0f;
		flWeight1 = boneweights->weight[1] / 255.0f;
		flWeight2 = boneweights->weight[2] / 255.0f;
		flTotal = flWeight0 + flWeight1 + flWeight2;

		if( flTotal < 1.0f ) flWeight0 += 1.0f - flTotal;	// compensate rounding error

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
		const matrix3x4 &boneMat0 = worldtransform[boneweights->bone[0]];
		const matrix3x4 &boneMat1 = worldtransform[boneweights->bone[1]];
		flWeight0 = boneweights->weight[0] / 255.0f;
		flWeight1 = boneweights->weight[1] / 255.0f;
		flTotal = flWeight0 + flWeight1;

		if( flTotal < 1.0f ) flWeight0 += 1.0f - flTotal;	// compensate rounding error

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
		result = worldtransform[boneweights->bone[0]];
	}
}

void CStudioModelRenderer :: UploadBufferBase( vbomesh_t *pOut, svert_t *arrayxvert )
{
	static svert_v0_t	arraysvert[MAXARRAYVERTS];

	// convert to GLSL-compacted array
	for( int i = 0; i <m_nNumArrayVerts; i++ )
	{
		arraysvert[i].vertex = arrayxvert[i].vertex;
		arraysvert[i].normal = arrayxvert[i].normal;
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
	}

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, m_nNumArrayVerts * sizeof( svert_v0_t ), &arraysvert[0], GL_STATIC_DRAW_ARB );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v0_t ), (void *)offsetof( svert_v0_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 2, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v0_t ), (void *)offsetof( svert_v0_t, stcoord ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v0_t ), (void *)offsetof( svert_v0_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_BYTE, GL_FALSE, sizeof( svert_v0_t ), (void *)offsetof( svert_v0_t, boneid )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );
}

void CStudioModelRenderer :: UploadBufferVLight( vbomesh_t *pOut, svert_t *arrayxvert )
{
	static svert_v1_t	arraysvert[MAXARRAYVERTS];

	// convert to GLSL-compacted array
	for( int i = 0; i < m_nNumArrayVerts; i++ )
	{
		arraysvert[i].vertex = arrayxvert[i].vertex;
		arraysvert[i].normal = arrayxvert[i].normal;
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
	}

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, m_nNumArrayVerts * sizeof( svert_v1_t ), &arraysvert[0], GL_STATIC_DRAW_ARB );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_t ), (void *)offsetof( svert_v1_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 2, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v1_t ), (void *)offsetof( svert_v1_t, stcoord ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v1_t ), (void *)offsetof( svert_v1_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v1_t ), (void *)offsetof( svert_v1_t, light )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
}

void CStudioModelRenderer :: UploadBufferWeight( vbomesh_t *pOut, svert_t *arrayxvert )
{
	static svert_v2_t	arraysvert[MAXARRAYVERTS];

	// convert to GLSL-compacted array
	for( int i = 0; i < m_nNumArrayVerts; i++ )
	{
		arraysvert[i].vertex = arrayxvert[i].vertex;
		arraysvert[i].normal = arrayxvert[i].normal;
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
	}

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, m_nNumArrayVerts * sizeof( svert_v2_t ), &arraysvert[0], GL_STATIC_DRAW_ARB );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v2_t ), (void *)offsetof( svert_v2_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 2, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v2_t ), (void *)offsetof( svert_v2_t, stcoord ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v2_t ), (void *)offsetof( svert_v2_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_BYTE, GL_FALSE, sizeof( svert_v2_t ), (void *)offsetof( svert_v2_t, boneid )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );

	pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( svert_v2_t ), (void *)offsetof( svert_v2_t, weight )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );
}

void CStudioModelRenderer :: UploadBufferGeneric( vbomesh_t *pOut, svert_t *arrayxvert, bool vertex_light )
{
	static svert_v3_t	arraysvert[MAXARRAYVERTS];

	// convert to GLSL-compacted array
	for( int i = 0; i < m_nNumArrayVerts; i++ )
	{
		arraysvert[i].vertex = arrayxvert[i].vertex;
		arraysvert[i].normal[0] = FloatToHalf( arrayxvert[i].normal[0] );
		arraysvert[i].normal[1] = FloatToHalf( arrayxvert[i].normal[1] );
		arraysvert[i].normal[2] = FloatToHalf( arrayxvert[i].normal[2] );
		arraysvert[i].stcoord[0] = arrayxvert[i].stcoord[0];
		arraysvert[i].stcoord[1] = arrayxvert[i].stcoord[1];
		arraysvert[i].boneid[0] = arrayxvert[i].boneid[0];
		arraysvert[i].boneid[1] = arrayxvert[i].boneid[1];
		arraysvert[i].boneid[2] = arrayxvert[i].boneid[2];
		arraysvert[i].boneid[3] = arrayxvert[i].boneid[3];
		arraysvert[i].weight[0] = arrayxvert[i].weight[0];
		arraysvert[i].weight[1] = arrayxvert[i].weight[1];
		arraysvert[i].weight[2] = arrayxvert[i].weight[2];
		arraysvert[i].weight[3] = arrayxvert[i].weight[3];
		arraysvert[i].light[0] = arrayxvert[i].light[0];
		arraysvert[i].light[1] = arrayxvert[i].light[1];
		arraysvert[i].light[2] = arrayxvert[i].light[2];
		arraysvert[i].light[3] = arrayxvert[i].light[3];
	}

	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, pOut->vbo );
	pglBufferDataARB( GL_ARRAY_BUFFER_ARB, m_nNumArrayVerts * sizeof( svert_v3_t ), &arraysvert[0], GL_STATIC_DRAW_ARB );

	pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_t ), (void *)offsetof( svert_v3_t, vertex ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );

	pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 2, GL_HALF_FLOAT_ARB, GL_FALSE, sizeof( svert_v3_t ), (void *)offsetof( svert_v3_t, stcoord ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );

	pglVertexAttribPointerARB( ATTR_INDEX_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof( svert_v3_t ), (void *)offsetof( svert_v3_t, normal ));
	pglEnableVertexAttribArrayARB( ATTR_INDEX_NORMAL );

	pglVertexAttribPointerARB( ATTR_INDEX_BONE_INDEXES, 4, GL_BYTE, GL_FALSE, sizeof( svert_v3_t ), (void *)offsetof( svert_v3_t, boneid )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_INDEXES );

	pglVertexAttribPointerARB( ATTR_INDEX_BONE_WEIGHTS, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof( svert_v3_t ), (void *)offsetof( svert_v3_t, weight )); 
	pglEnableVertexAttribArrayARB( ATTR_INDEX_BONE_WEIGHTS );

	if( vertex_light )
	{
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof( svert_v3_t ), (void *)offsetof( svert_v3_t, light )); 
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_COLOR );
	}
}

void CStudioModelRenderer :: MeshCreateBuffer( vbomesh_t *pOut, const mstudiomesh_t *pMesh, const mstudiomodel_t *pSubModel, const matrix3x4 bones[], dmodellight_t *dml )
{
	// FIXME: if various skinfamilies has different sizes then our texcoords probably will be invalid for pev->skin != 0
	short		*pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex); // setup skinref for skin == 0
	mstudiotexture_t	*ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);
	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)m_pRenderModel->materials;
	ptexture = &ptexture[pskinref[pMesh->skinref]];
	pmaterial = &pmaterial[pskinref[pMesh->skinref]];

	mstudiobone_t	*pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	short		*ptricmds = (short *)((byte *)m_pStudioHeader + pMesh->triindex);
	Vector		*pstudioverts = (Vector *)((byte *)m_pStudioHeader + pSubModel->vertindex);
	Vector		*pstudionorms = (Vector *)((byte *)m_pStudioHeader + pSubModel->normindex);
	byte		*pvertbone = ((byte *)m_pStudioHeader + pSubModel->vertinfoindex);
	byte		*pnormbone = ((byte *)m_pStudioHeader + pSubModel->norminfoindex);

	// if weights was missed their offsets just equal to 0
	mstudioboneweight_t	*pvertweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + pSubModel->blendvertinfoindex);
	mstudioboneweight_t	*pnormweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + pSubModel->blendnorminfoindex);
	bool 		has_boneweights = ( m_pRenderModel->poseToBone != NULL ) ? true : false;
	bool		has_vertexlight = ( dml != NULL && dml->numverts > 0 ) ? true : false;
	static svert_t	arrayxvert[MAXARRAYVERTS];
	matrix3x4		skinMat;
	int		i;

	float s = 1.0f / (float)ptexture->width;
	float t = 1.0f / (float)ptexture->height;

	pOut->skinref = pMesh->skinref;

	// init temporare arrays
	m_nNumArrayVerts = m_nNumArrayElems = 0;

	// first create trifan array from studiomodel mesh
	while( i = *( ptricmds++ ))
	{
		bool	strip = ( i < 0 ) ? false : true;
		int	vertexState = 0;

		if( i < 0 ) i = -i;

		for( ; i > 0; i--, ptricmds += 4 )
		{
			if( vertexState++ < 3 )
			{
				m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
			}
			else if( strip )
			{
				// flip triangles between clockwise and counter clockwise
				if( vertexState & 1 )
				{
					// draw triangle [n-2 n-1 n]
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 2;
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 1;
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}
				else
				{
					// draw triangle [n-1 n-2 n]
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 1;
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 2;
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}
			}
			else
			{
				// draw triangle fan [0 n-1 n]
				m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - ( vertexState - 1 );
				m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 1;
				m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
			}

			// don't concat by matrix here - it's should be done on GPU
			arrayxvert[m_nNumArrayVerts].vertex = pstudioverts[ptricmds[0]];
			arrayxvert[m_nNumArrayVerts].normal = pstudionorms[ptricmds[1]];

			if( dml != NULL && dml->numverts > 0 )
			{
				dvertlight_t	*vl = &dml->verts[m_nNumLightVerts++];

				// pack lightvalues into single float
				for( int map = 0; map < MAXLIGHTMAPS; map++ )
				{
					byte r = vl->light[map][0], g = vl->light[map][1], b = vl->light[map][2];
					float packDirect = (float)((double)((r << 16) | (g << 8) | b) / (double)(1 << 24));
					arrayxvert[m_nNumArrayVerts].light[map] = packDirect;
				}
			}

			if( FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
			{
				// probably always equal 64 (see studiomdl.c for details)
				arrayxvert[m_nNumArrayVerts].stcoord[0] = FloatToHalf( s );
				arrayxvert[m_nNumArrayVerts].stcoord[1] = FloatToHalf( t );
			}
			else if( FBitSet( ptexture->flags, STUDIO_NF_UV_COORDS ))
			{
				arrayxvert[m_nNumArrayVerts].stcoord[0] = ptricmds[2];
				arrayxvert[m_nNumArrayVerts].stcoord[1] = ptricmds[3];
			}
			else
			{
				arrayxvert[m_nNumArrayVerts].stcoord[0] = FloatToHalf( ptricmds[2] * s );
				arrayxvert[m_nNumArrayVerts].stcoord[1] = FloatToHalf( ptricmds[3] * t );
			}

			if( m_pRenderModel->poseToBone != NULL )
			{
				mstudioboneweight_t	*pCurWeight = &pvertweight[ptricmds[0]];

				arrayxvert[m_nNumArrayVerts].boneid[0] = pCurWeight->bone[0];
				arrayxvert[m_nNumArrayVerts].boneid[1] = pCurWeight->bone[1];
				arrayxvert[m_nNumArrayVerts].boneid[2] = pCurWeight->bone[2];
				arrayxvert[m_nNumArrayVerts].boneid[3] = pCurWeight->bone[3];
				arrayxvert[m_nNumArrayVerts].weight[0] = pCurWeight->weight[0];
				arrayxvert[m_nNumArrayVerts].weight[1] = pCurWeight->weight[1];
				arrayxvert[m_nNumArrayVerts].weight[2] = pCurWeight->weight[2];
				arrayxvert[m_nNumArrayVerts].weight[3] = pCurWeight->weight[3];
			}
			else
			{
				arrayxvert[m_nNumArrayVerts].boneid[0] = pvertbone[ptricmds[0]];
				arrayxvert[m_nNumArrayVerts].boneid[1] = -1;
				arrayxvert[m_nNumArrayVerts].boneid[2] = -1;
				arrayxvert[m_nNumArrayVerts].boneid[3] = -1;
				arrayxvert[m_nNumArrayVerts].weight[0] = 255;
				arrayxvert[m_nNumArrayVerts].weight[1] = 0;
				arrayxvert[m_nNumArrayVerts].weight[2] = 0;
				arrayxvert[m_nNumArrayVerts].weight[3] = 0;
			}

			m_nNumArrayVerts++;
		}
	}

	pOut->numVerts = m_nNumArrayVerts;
	pOut->numElems = m_nNumArrayElems;

	// create GPU static buffer
	pglGenBuffersARB( 1, &pOut->vbo );
	pglGenVertexArrays( 1, &pOut->vao );

	// create vertex array object
	pglBindVertexArray( pOut->vao );

	if( m_pStudioHeader->numbones <= 1 && has_vertexlight )
	{
		// special case for single bone vertex lighting
		UploadBufferVLight( pOut, arrayxvert );
	}
	else if( !has_boneweights && !has_vertexlight )
	{
		// typical GoldSrc models without weights
		UploadBufferBase( pOut, arrayxvert );
	}
	else if( has_boneweights && !has_vertexlight )
	{
		// extended Xash3D models with boneweights
		UploadBufferWeight( pOut, arrayxvert );
	}
	else
	{
		// all other cases
		if( m_pStudioHeader->numbones > 1 && has_vertexlight )
			ALERT( at_aiconsole, "%s static model have skeleton\n", m_pRenderModel->name );
		UploadBufferGeneric( pOut, arrayxvert, has_vertexlight );
	}

	// create index array buffer
	pglGenBuffersARB( 1, &pOut->ibo );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, pOut->ibo );
	pglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, m_nNumArrayElems * sizeof( unsigned int ), &m_arrayelems[0], GL_STATIC_DRAW_ARB );

	// don't forget to unbind them
	pglBindVertexArray( GL_FALSE );
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
}

mvbocache_t *CStudioModelRenderer :: CreateMeshCache( dmodellight_t *dml )
{
	bool		unique_model = (dml == NULL);	// just for more readable code
	TmpModel_t	submodel[MAXSTUDIOMODELS];	// list of unique models
	static matrix3x4	bones[MAXSTUDIOBONES];
	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];
	int		i, j, k, bufSize = 0;
	int		num_submodels = 0;
	byte		*buffer, *bufend;		// simple bounds check
	mvbocache_t	*studiocache;
	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t	*psubmodel;
	msubmodel_t	*pModel;
	mstudiobone_t	*pbones;

	// materials goes first to determine bump
	if( unique_model ) LoadStudioMaterials ();

	// build default pose to build seamless TBN-space
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_boneSetup.SetStudioPointers( m_pStudioHeader, m_pModelInstance->m_poseparameter );

	// setup local bone matrices
	if( unique_model && FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEINFO ))
	{
		// NOTE: extended boneinfo goes immediately after bones
		mstudioboneinfo_t *boneinfo = (mstudioboneinfo_t *)((byte *)pbones + m_pStudioHeader->numbones * sizeof( mstudiobone_t ));

		// alloc storage for bone array
		m_pRenderModel->poseToBone = (mposetobone_t *)Mem_Alloc( sizeof( mposetobone_t ));

		for( j = 0; j < m_pStudioHeader->numbones; j++ )
			LoadLocalMatrix( j, &boneinfo[j] );
	}

	// compute default pose with no anim
	m_boneSetup.InitPose( pos, q );

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		if( pbones[i].parent == -1 ) bones[i] = matrix3x4( pos[i], q[i] );
		else bones[i] = bones[pbones[i].parent].ConcatTransforms( matrix3x4( pos[i], q[i] ));
	}

	if( m_pRenderModel->poseToBone != NULL )
	{
		// convert bones into worldtransform
		for( i = 0; i < m_pStudioHeader->numbones; i++ )
			bones[i] = bones[i].ConcatTransforms( m_pRenderModel->poseToBone->posetobone[i] );
	}

	memset( submodel, 0, sizeof( submodel ));
	num_submodels = 0;

	// build list of unique submodels (by name)
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		for( j = 0; j < pbodypart->nummodels; j++ )
		{
			psubmodel = (mstudiomodel_t *)((byte *)m_pStudioHeader + pbodypart->modelindex) + j;
			if( !psubmodel->nummesh ) continue; // blank submodel, ignore it

			for( k = 0; k < num_submodels; k++ )
			{
				if( !Q_stricmp( submodel[k].name, psubmodel->name ))
					break;
			}

			// add new one
			if( k == num_submodels )
			{
				Q_strncpy( submodel[k].name, psubmodel->name, sizeof( submodel[k].name ));
				submodel[k].pmodel = psubmodel;
				num_submodels++;
			}
		}
	}

	// compute cache size (include individual meshes)
	bufSize = sizeof( mvbocache_t ) + sizeof( mbodypart_t ) * m_pStudioHeader->numbodyparts;

	for( i = 0; i < num_submodels; i++ )
		bufSize += sizeof( msubmodel_t ) + sizeof( vbomesh_t ) * submodel[i].pmodel->nummesh;

	buffer = (byte *)Mem_Alloc( bufSize );
	bufend = buffer + bufSize;

	// setup pointers
	studiocache = (mvbocache_t *)buffer;
	buffer += sizeof( mvbocache_t );
	studiocache->bodyparts = (mbodypart_t *)buffer;
	buffer += sizeof( mbodypart_t ) * m_pStudioHeader->numbodyparts;
	studiocache->numbodyparts = m_pStudioHeader->numbodyparts;

	// begin to building submodels
	for( i = 0; i < num_submodels; i++ )
	{
		psubmodel = submodel[i].pmodel;
		pModel = (msubmodel_t *)buffer;
		buffer += sizeof( msubmodel_t );
		pModel->nummesh = psubmodel->nummesh;

		// setup meshes
		pModel->meshes = (vbomesh_t *)buffer;
		buffer += sizeof( vbomesh_t ) * psubmodel->nummesh;

		// sanity check
		if( dml != NULL && dml->numverts > 0 )
		{
			// search for submodel offset
			int	offset = (byte *)psubmodel - (byte *)m_pStudioHeader;

			for( j = 0; j < MAXSTUDIOMODELS; j++ )
			{
				if( dml->submodels[j].submodel_offset == offset )
					break;			
			}

			ASSERT( j != MAXSTUDIOMODELS );
			ASSERT( m_nNumLightVerts == dml->submodels[j].vertex_offset );
		}

		for( j = 0; j < psubmodel->nummesh; j++ )
		{
			mstudiomesh_t *pSrc = (mstudiomesh_t *)((byte *)m_pStudioHeader + psubmodel->meshindex) + j;
			vbomesh_t *pDst = &pModel->meshes[j];

			MeshCreateBuffer( pDst, pSrc, psubmodel, bones, dml );
		}
		submodel[i].pout = pModel; // store unique submodel
	}

	// and finally setup bodyparts
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;
		mbodypart_t *pBodyPart = &studiocache->bodyparts[i];

		pBodyPart->base = pbodypart->base;
		pBodyPart->nummodels = pbodypart->nummodels;

		// setup pointers to unique models	
		for( j = 0; j < pBodyPart->nummodels; j++ )
		{
			psubmodel = (mstudiomodel_t *)((byte *)m_pStudioHeader + pbodypart->modelindex) + j;
			if( !psubmodel->nummesh ) continue; // blank submodel, leave null pointer

			// find supposed model
			for( k = 0; k < num_submodels; k++ )
			{
				if( !Q_stricmp( submodel[k].name, psubmodel->name ))
				{
					pBodyPart->models[j] = submodel[k].pout;
					break;
				}
			}

			if( k == num_submodels )
				ALERT( at_error, "Couldn't find submodel %s for bodypart %i\n", psubmodel->name, i );
		}
	}

	// bounds checking
	if( buffer != bufend )
	{
		if( buffer > bufend )
			ALERT( at_error, "CreateMeshCache: memory buffer overrun\n" );
		else ALERT( at_error, "CreateMeshCache: memory buffer underrun\n" );
	}

	return studiocache;
}

//-----------------------------------------------------------------------------
// all the caches should be build before starting the new map
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: CreateMeshCacheVL( const char *modelname, int cacheID )
{
	dvlightlump_t	*vl = world->vertex_lighting;

	// first we need throw previous mesh
	ReleaseVBOCache( &tr.vertex_light_cache[cacheID] );

	if( world->vertex_lighting == NULL )
		return; // for some reasons we missed this lump

	m_pRenderModel = IEngineStudio.Mod_ForName( modelname, false );
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

	if( !m_pRenderModel || !m_pStudioHeader )
		return; // download in progress?

	// first initialization
	if( cacheID < vl->nummodels && vl->dataofs[cacheID] != -1 )
	{
		dmodellight_t	*dml = (dmodellight_t *)((byte *)vl + vl->dataofs[cacheID]);

		m_nNumLightVerts = 0;

		if( m_pRenderModel->modelCRC == dml->modelCRC )
		{
			double start = Sys_DoubleTime();
			// now create mesh per entity with instanced vertex lighting
			tr.vertex_light_cache[cacheID] = CreateMeshCache( dml );
			double end = Sys_DoubleTime();
			tr.buildtime += (end - start);
		}

		if( dml->numverts == m_nNumLightVerts )
			ALERT( at_aiconsole, "vertexlit instance created, model verts %i, total verts %i\n", dml->numverts, m_nNumLightVerts );
		else if( m_pRenderModel->modelCRC != dml->modelCRC )
			ALERT( at_error, "failed to create vertex lighting: model CRC %p != %p\n", m_pRenderModel->modelCRC, dml->modelCRC );
		else ALERT( at_error, "failed to create vertex lighting: model verts %i != total verts %i\n", dml->numverts, m_nNumLightVerts );
		m_nNumLightVerts = 0;
	}
}

//-----------------------------------------------------------------------------
// all the caches should be build before starting the new map
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: FreeMeshCacheVL( void )
{
	for( int i = 0; i < MAX_LIGHTCACHE; i++ )
		ReleaseVBOCache( &tr.vertex_light_cache[i] );
}

void CStudioModelRenderer :: CreateMeshCacheVL( dmodellight_t *dml, int cacheID )
{
	if( m_pRenderModel->modelCRC == dml->modelCRC )
	{
		// get lighting cache
		m_pModelInstance->m_VlCache = tr.vertex_light_cache[cacheID];
	}

	if( m_pModelInstance->m_VlCache != NULL )
	{
		SetBits( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING );

		for( int map = 0; map < MAXLIGHTMAPS; map++ )
			m_pModelInstance->styles[map] = dml->styles[map];

		for( int i = 0; i < m_pStudioHeader->numtextures; i++ )
		{
			mstudiomaterial_t *mat = &m_pModelInstance->materials[i];
			mat->glsl_sequence = 0; // refresh shaders
		}
	}
	else
	{
		ALERT( at_warning, "failed to create vertex lighting for %s\n", m_pRenderModel->name );
		SetBits( m_pModelInstance->info_flags, MF_VL_BAD_CACHE );
	}
}

void CStudioModelRenderer :: ReleaseVBOCache( mvbocache_t **ppvbocache )
{
	ASSERT( ppvbocache != NULL );

	mvbocache_t *pvbocache = *ppvbocache;
	if( !pvbocache ) return;

	for( int i = 0; i < pvbocache->numbodyparts; i++ )
	{
		mbodypart_t *pBodyPart = &pvbocache->bodyparts[i];

		for( int j = 0; j < pBodyPart->nummodels; j++ )
		{
			msubmodel_t *pSubModel = pBodyPart->models[j];

			if( !pSubModel || pSubModel->nummesh <= 0 )
				continue; // blank submodel

			for( int k = 0; k < pSubModel->nummesh; k++ )
			{
				vbomesh_t *pMesh = &pSubModel->meshes[k];

				// purge all GPU data
				if( pMesh->vao ) pglDeleteVertexArrays( 1, &pMesh->vao );
				if( pMesh->vbo ) pglDeleteBuffersARB( 1, &pMesh->vbo );
				if( pMesh->ibo ) pglDeleteBuffersARB( 1, &pMesh->ibo );
			}
		}
	}

	if( pvbocache != NULL )
		Mem_Free( pvbocache );
	*ppvbocache = NULL;
}

void CStudioModelRenderer :: DestroyMeshCache( void )
{
	FreeStudioMaterials ();

	ReleaseVBOCache( &m_pRenderModel->studiocache );

	if( m_pRenderModel->poseToBone != NULL )
		Mem_Free( m_pRenderModel->poseToBone );
	m_pRenderModel->poseToBone = NULL;
}

void CStudioModelRenderer :: LoadStudioMaterials( void )
{
	// first we need alloc copy of all the materials to prevent modify mstudiotexture_t
	m_pRenderModel->materials = (mstudiomaterial_t *)Mem_Alloc( sizeof( mstudiomaterial_t ) * m_pStudioHeader->numtextures );

	mstudiotexture_t	*ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);
	bool		bone_weights = FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS ) ? true : false;
	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)m_pRenderModel->materials;
	char		diffuse[64], texname[64], mdlname[64];
	int		i;

	COM_FileBase( m_pRenderModel->name, mdlname );

	// loading studio materials from studio textures
	for( i = 0; i < m_pStudioHeader->numtextures; i++, ptexture++, pmaterial++ )
	{
		COM_FileBase( ptexture->name, texname );

		// build material names
		Q_snprintf( diffuse, sizeof( diffuse ), "textures/%s/%s", mdlname, texname );

		pmaterial->pSource = ptexture;
		pmaterial->flags = ptexture->flags;

		if( IMAGE_EXISTS( diffuse ) && !FBitSet( ptexture->flags, STUDIO_NF_COLORMAP ))
		{
			int	texture_ext = LOAD_TEXTURE( diffuse, NULL, 0, 0 );
			int	encodeType = RENDER_GET_PARM( PARM_TEX_ENCODE, texture_ext );

			// NOTE: default renderer can't unpack encoded textures
			// so keep lowres copies for this case
			if( encodeType == DXT_ENCODE_DEFAULT )
			{
				pmaterial->gl_diffuse_id = texture_ext;

				// semi-transparent textures must have additive flag to invoke renderer insert supposed mesh into translist
				if( FBitSet( pmaterial->flags, STUDIO_NF_ADDITIVE ))
				{
					if( RENDER_GET_PARM( PARM_TEX_FLAGS, pmaterial->gl_diffuse_id ) & TF_HAS_ALPHA )
						pmaterial->flags |= STUDIO_NF_HAS_ALPHA;
				}

				if( RENDER_GET_PARM( PARM_TEX_FLAGS, pmaterial->gl_diffuse_id ) & TF_HAS_ALPHA )
				{
					ptexture->flags |= STUDIO_NF_MASKED;
					pmaterial->flags |= STUDIO_NF_MASKED;
				}
			}
			else
			{
				// can't use encoded textures
				FREE_TEXTURE( texture_ext );
			}
		}

		if( pmaterial->gl_diffuse_id != 0 )
		{
			// so engine can be draw HQ image for gl_renderer 0
			FREE_TEXTURE( ptexture->index );
			ptexture->index = pmaterial->gl_diffuse_id;
		}
		else
		{
			// reuse original texture
			pmaterial->gl_diffuse_id = ptexture->index;
		}

		// precache as many shaders as possible
		GL_UberShaderForSolidStudio( pmaterial, false, bone_weights, false, m_pStudioHeader->numbones );
	}
}

void CStudioModelRenderer :: FreeStudioMaterials( void )
{
	if( !m_pRenderModel->materials ) return;

	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)m_pRenderModel->materials;

	// release textures for current model
	for( int i = 0; i < m_pStudioHeader->numtextures; i++, pmaterial++ )
	{
		if( pmaterial->pSource->index != pmaterial->gl_diffuse_id )
			FREE_TEXTURE( pmaterial->gl_diffuse_id );
	}

	Mem_Free( m_pRenderModel->materials );
	m_pRenderModel->materials = NULL;
}

void CStudioModelRenderer :: DestroyAllModelInstances( void )
{
	// NOTE: should destroy in reverse-order because it's linked list not array!
	for( int i = m_ModelInstances.Count(); --i >= 0; )
          {
		DestroyInstance( i );
	}

	m_DecalMaterial.RemoveAll();
}

/*
================
R_QSortStudioMeshes

Quick sort
================
*/
void CStudioModelRenderer :: QSortStudioMeshes( gl_studiomesh_t *meshes, int Li, int Ri )
{
	int lstack[QSORT_MAX_STACKDEPTH], rstack[QSORT_MAX_STACKDEPTH];
	int li, ri, stackdepth = 0, total = Ri + 1;
	gl_studiomesh_t median, tempbuf;
mark0:
	if( Ri - Li > 8 )
	{
		li = Li;
		ri = Ri;

		R_MeshCopy( meshes[( Li+Ri ) >> 1], median );

		if( R_MeshCmp( meshes[Li], median ) )
		{
			if( R_MeshCmp( meshes[Ri], meshes[Li] ) )
				R_MeshCopy( meshes[Li], median );
		}
		else if( R_MeshCmp( median, meshes[Ri] ) )
		{
			R_MeshCopy( meshes[Ri], median );
		}

		do
		{
			while( R_MeshCmp( median, meshes[li] ) ) li++;
			while( R_MeshCmp( meshes[ri], median ) ) ri--;

			if( li <= ri )
			{
				R_MeshCopy( meshes[ri], tempbuf );
				R_MeshCopy( meshes[li], meshes[ri] );
				R_MeshCopy( tempbuf, meshes[li] );

				li++;
				ri--;
			}
		} while( li < ri );

		if( ( Li < ri ) && ( stackdepth < QSORT_MAX_STACKDEPTH ) )
		{
			lstack[stackdepth] = li;
			rstack[stackdepth] = Ri;
			stackdepth++;
			li = Li;
			Ri = ri;
			goto mark0;
		}

		if( li < Ri )
		{
			Li = li;
			goto mark0;
		}
	}

	if( stackdepth )
	{
		--stackdepth;
		Ri = ri = rstack[stackdepth];
		Li = li = lstack[stackdepth];
		goto mark0;
	}

	for( li = 1; li < total; li++ )
	{
		R_MeshCopy( meshes[li], tempbuf );
		ri = li - 1;

		while( ( ri >= 0 ) && ( R_MeshCmp( meshes[ri], tempbuf ) ) )
		{
			R_MeshCopy( meshes[ri], meshes[ri+1] );
			ri--;
		}

		if( li != ri+1 )
			R_MeshCopy( tempbuf, meshes[ri+1] );
	}
}

/*
================
R_GetEntityRenderMode

check for texture flags
================
*/
int CStudioModelRenderer :: GetEntityRenderMode( cl_entity_t *ent )
{
	cl_entity_t	*oldent = IEngineStudio.GetCurrentEntity();
	int		i, opaque, trans;
	mstudiotexture_t	*ptexture;
	model_t		*model;
	studiohdr_t	*phdr;

	SET_CURRENT_ENTITY( ent );

	if( ent->player ) // check it for real playermodel
		model = IEngineStudio.SetupPlayerModel( ent->curstate.number - 1 );
	else if( ent->curstate.renderfx == kRenderFxDeadPlayer )
		model = IEngineStudio.SetupPlayerModel( ent->curstate.renderamt - 1 );
	else model = ent->model;

	SET_CURRENT_ENTITY( oldent );

	if(( phdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( model )) == NULL )
	{
		if( R_ModelOpaque( ent->curstate.rendermode ))
		{
			// forcing to choose right sorting type
			if(( model && model->type == mod_brush ) && FBitSet( model->flags, MODEL_TRANSPARENT ))
				return kRenderTransAlpha;
		}
		return ent->curstate.rendermode;
	}
	ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);

	for( opaque = trans = i = 0; i < phdr->numtextures; i++, ptexture++ )
	{
		// ignore chrome & additive it's just a specular-like effect
		if( FBitSet( ptexture->flags, STUDIO_NF_ADDITIVE ) && !FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
			trans++;
		else opaque++;
	}

	// if model is more additive than opaque
	if( trans > opaque )
		return kRenderTransAdd;
	return ent->curstate.rendermode;
}

/*
================
StudioExtractBbox

Extract bbox from current sequence
================
*/
int CStudioModelRenderer :: StudioExtractBbox( cl_entity_t *e, studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs )
{
	if( !phdr || sequence < 0 || sequence >= phdr->numseq )
		return 0;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mins = pseqdesc[sequence].bbmin;
	maxs = pseqdesc[sequence].bbmax;

	return 1;
}

/*
================
StudioComputeBBox

Compute a full bounding box for current sequence
================
*/
int CStudioModelRenderer :: StudioComputeBBox( cl_entity_t *e )
{
	Vector scale = Vector( 1.0f, 1.0f, 1.0f );
	Vector angles, mins, maxs;

	if( FBitSet( m_pModelInstance->info_flags, MF_STATIC_BOUNDS ))
		return true; // bounds already computed

	if( !StudioExtractBbox( e, m_pStudioHeader, e->curstate.sequence, mins, maxs ))
		return false;

	if( m_pModelInstance->m_bProceduralBones )
	{
		mins = e->curstate.mins;
		maxs = e->curstate.maxs;
	}

	// prevent to compute env_static bounds every frame
	if( FBitSet( e->curstate.iuser1, CF_STATIC_ENTITY ))
		SetBits( m_pModelInstance->info_flags, MF_STATIC_BOUNDS );

	if( FBitSet( e->curstate.iuser1, CF_STATIC_ENTITY ))
	{
		if( e->curstate.vuser2 != g_vecZero )
			scale = e->curstate.vuser2;
	}
	else if( e->curstate.scale > 0.0f && e->curstate.scale <= 16.0f )
	{
		// apply studiomodel scale (clamp scale to prevent too big sizes on some HL maps)
		scale = Vector( e->curstate.scale, e->curstate.scale, e->curstate.scale );
	}

	// rotate the bounding box
	angles = e->angles;

	// don't rotate player model, only aim
	if( e->player ) angles[PITCH] = 0;

	matrix3x4 transform = matrix3x4( g_vecZero, angles, scale );

	// rotate and scale bbox for env_static
	TransformAABB( transform, mins, maxs, mins, maxs );

	// compute abs box
	m_pModelInstance->absmin = mins + e->origin;
	m_pModelInstance->absmax = maxs + e->origin;
	m_pModelInstance->radius = RadiusFromBounds( mins, maxs );

	return true;
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer :: StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int &pBlend, float &pPitch )
{
	pBlend = (pPitch * 3.0f);

	if( pBlend < pseqdesc->blendstart[0] )
	{
		pPitch -= pseqdesc->blendstart[0] / 3.0f;
		pBlend = 0;
	}
	else if( pBlend > pseqdesc->blendend[0] )
	{
		pPitch -= pseqdesc->blendend[0] / 3.0f;
		pBlend = 255;
	}
	else
	{
		if( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1f ) // catch qc error
			pBlend = 127;
		else pBlend = 255 * (pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);

		pPitch = 0;
	}
}

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

float CStudioModelRenderer :: CalcStairSmoothValue( float oldz, float newz, float smoothtime, float smoothvalue )
{
	if( oldz < newz )
		return bound( newz - tr.movevars->stepsize, oldz + smoothtime * smoothvalue, newz );
	if( oldz > newz )
		return bound( newz, oldz - smoothtime * smoothvalue, newz + tr.movevars->stepsize );
	return 0.0f;
}

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

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer :: StudioSetUpTransform( void )
{
	cl_entity_t	*e = m_pCurrentEntity;
	qboolean		disable_smooth = false;
	float		step, smoothtime;

	if( !m_fShootDecal && m_pCurrentEntity->curstate.renderfx != kRenderFxDeadPlayer )
	{
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
	}

	Vector origin = m_pCurrentEntity->origin;
	Vector angles = m_pCurrentEntity->angles;
	Vector scale = Vector( 1.0f, 1.0f, 1.0f );

	float lodDist = (origin - RI->vieworg).Length() * tr.lodScale;
	float  radius = Q_max( m_pModelInstance->radius, 1.0f ); // to avoid division by zero
	int lodnum = (int)( lodDist / radius );
	int numLods;

	if( CVAR_TO_BOOL( m_pCvarLodScale ))
		lodnum /= (int)fabs( m_pCvarLodScale->value );
	if( CVAR_TO_BOOL( m_pCvarLodBias ))
		lodnum += (int)fabs( m_pCvarLodBias->value );

	// apply lodnum to model
	if(( numLods = StudioCheckLOD( )) != 0 )
	{
		// set derived LOD
		e->curstate.body = Q_min( lodnum, numLods - 1 );
	}

	if( m_pPlayerInfo )
	{
		int		iBlend, m_iGaitSequence = 0;
		mstudioseqdesc_t	*pseqdesc;

		if( m_pCurrentEntity->curstate.renderfx != kRenderFxDeadPlayer )
			m_iGaitSequence = IEngineStudio.GetPlayerState( m_nPlayerIndex )->gaitsequence;

		if( m_iGaitSequence )
		{
			pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

			// calc blend (FXIME: move to the server)
			StudioPlayerBlend( pseqdesc, iBlend, angles[PITCH] );
			m_pCurrentEntity->curstate.blending[0] = iBlend;
			m_pCurrentEntity->latched.prevblending[0] = iBlend;
			m_pPlayerInfo->gaitsequence = m_iGaitSequence;
		}
		else
		{
			m_pPlayerInfo->gaitsequence = 0;
		}

		// don't rotate clients, only aim
		angles[PITCH] = 0.0f;

		if( m_pPlayerInfo->gaitsequence != m_pModelInstance->lerp.gaitsequence )
		{
			AddBlendSequence( m_pModelInstance->lerp.gaitsequence, m_pPlayerInfo->gaitsequence, m_pModelInstance->lerp.gaitframe, true );
			m_pModelInstance->lerp.gaitsequence = m_pPlayerInfo->gaitsequence;
		}
		m_pModelInstance->lerp.gaitframe = m_pPlayerInfo->gaitframe;
	}

	if( FBitSet( m_pCurrentEntity->curstate.effects, EF_NOINTERP ) || ( tr.realframecount - m_pModelInstance->cached_frame ) > 1 )
	{
		m_pModelInstance->lerp.sequence = m_pCurrentEntity->curstate.sequence;
	}
	else if( m_pCurrentEntity->curstate.sequence != m_pModelInstance->lerp.sequence )
	{
		AddBlendSequence( m_pModelInstance->lerp.sequence, m_pCurrentEntity->curstate.sequence, m_pModelInstance->lerp.frame );
		m_pModelInstance->lerp.sequence = m_pCurrentEntity->curstate.sequence;
	}

	// don't blend sequences for a dead player or a viewmodel
	if( m_fDrawViewModel || m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer )
		memset( &m_pModelInstance->m_seqblend, 0, sizeof( m_pModelInstance->m_seqblend ));

	if( m_pCurrentEntity->curstate.vuser2 != g_vecZero )
	{
		scale = m_pCurrentEntity->curstate.vuser2;
	}
	else if( m_pCurrentEntity->curstate.scale > 0.0f && m_pCurrentEntity->curstate.scale <= 16.0f )
	{
		// apply studiomodel scale (clamp scale to prevent too big sizes on some HL maps)
		scale = Vector( m_pCurrentEntity->curstate.scale, m_pCurrentEntity->curstate.scale, m_pCurrentEntity->curstate.scale );
	}

	// build the rotation matrix
	m_pModelInstance->m_protationmatrix = matrix3x4( origin, angles, scale );
	m_pModelInstance->m_plightmatrix = m_pModelInstance->m_protationmatrix;

	if( m_pCurrentEntity == GET_VIEWMODEL() && CVAR_TO_BOOL( m_pCvarHand ))
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
	double dfdt = 0, f = 0;

	if( m_fDoInterp && tr.time >= m_pCurrentEntity->curstate.animtime )
		dfdt = (tr.time - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;

	if( pseqdesc->numframes > 1 )
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;

	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 )
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		}

		if( f < 0.0 ) 
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 ) 
		{
			f = pseqdesc->numframes - 1.001;
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
	double dfdt = 0, f = 0;

	if( m_fDoInterp && tr.time >= m_pCurrentEntity->curstate.animtime )
	{
		dfdt = (tr.time - m_pCurrentEntity->curstate.animtime) / 0.1f;
	}

	if( pseqdesc->numframes > 1)
		f = m_pCurrentEntity->curstate.fuser1;

	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 )
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		}

		if( f < 0.0 ) 
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 ) 
		{
			f = pseqdesc->numframes - 1.001;
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
float CStudioModelRenderer :: StudioEstimateInterpolant( void )
{
	float dadt = 1.0;

	if( m_fDoInterp && ( m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01f ))
	{
		dadt = (tr.time - m_pCurrentEntity->curstate.animtime) / 0.1f;

		if( dadt > 2.0f )
		{
			dadt = 2.0f;
		}
	}
	return dadt;
}

/*
====================
StudioInterpolateBlends

====================
*/
void CStudioModelRenderer :: StudioInterpolateBlends( cl_entity_t *e, float dadt )
{
	mstudiobonecontroller_t *pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	if( !m_boneSetup.CountPoseParameters( ))
	{
		// interpolate blends
		m_pModelInstance->m_poseparameter[0] = (e->curstate.blending[0] * dadt + e->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;
		m_pModelInstance->m_poseparameter[1] = (e->curstate.blending[1] * dadt + e->latched.prevblending[1] * (1.0f - dadt)) / 255.0f;
	}
	else
	{
		// interpolate pose parameters here...
	}

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
mstudioanim_t *CStudioModelRenderer :: StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t *pseqgroup;
	cache_user_t *paSequences;

	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		paSequences = (cache_user_t *)IEngineStudio.Mem_Calloc( MAXSTUDIOGROUPS, sizeof( cache_user_t ));
		m_pSubModel->submodels = (dmodel_t *)paSequences;
	}

	// check for already loaded
	if( !IEngineStudio.Cache_Check(( struct cache_user_s *)&(paSequences[pseqdesc->seqgroup] )))
	{
		char filepath[128], modelpath[128], modelname[64];

		COM_FileBase( m_pSubModel->name, modelname );
		COM_ExtractFilePath( m_pSubModel->name, modelpath );

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
	CIKContext	*pIK = NULL;

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

	// FIXME: trace based on gravity or trace based on angles?
	Vector up;
	AngleVectors( m_pCurrentEntity->angles, NULL, NULL, (float *)&up );

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
				estGround = (pTarget->est.pos - m_pCurrentEntity->origin);
				estGround = estGround - (estGround * up) * up;
				estGround = m_pCurrentEntity->origin + estGround + pTarget->est.floor * up;

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
						pTarget->SetAngles( m_pCurrentEntity->angles );

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
						pTarget->SetAngles( m_pCurrentEntity->angles );
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
				Vector origin = inst->attachment[pTarget->offset.attachmentIndex].pos;
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

void CStudioModelRenderer :: StudioSetBonesExternal( const cl_entity_t *ent, const Vector pos[], const Radian ang[] )
{
	m_pCurrentEntity = (cl_entity_t *)ent;
	m_pRenderModel = ent->model;

	if( m_pRenderModel == NULL )
		return;

	// setup global pointers
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

	// aliasmodel instead of studio?
	if( m_pStudioHeader == NULL )
		return;

	if( m_pCurrentEntity->modelhandle == INVALID_HANDLE )
		return; // out of memory ?

	m_pModelInstance = &m_ModelInstances[m_pCurrentEntity->modelhandle];

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		m_pModelInstance->m_procangles[i] = ang[i];
		m_pModelInstance->m_procorigin[i] = pos[i];
	}

	m_pModelInstance->m_flLastBoneUpdate = tr.time + 0.1f;
	m_pModelInstance->m_bProceduralBones = true;
}

void CStudioModelRenderer :: StudioCalcBonesProcedural( Vector pos[], Vector4D q[] )
{
	if( !m_pModelInstance->m_bProceduralBones )
		return;

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		q[i] = m_pModelInstance->m_procangles[i];
		pos[i] = m_pModelInstance->m_procorigin[i];
	}

	// update is expired
	if( tr.time > m_pModelInstance->m_flLastBoneUpdate )
		m_pModelInstance->m_bProceduralBones = false;
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer :: StudioSetupBones( void )
{
	float		adj[MAXSTUDIOCONTROLLERS];
	cl_entity_t	*e = m_pCurrentEntity;	// for more readability
	CIKContext	*pIK = NULL;
	mstudioboneinfo_t	*pboneinfo;
	mstudioseqdesc_t	*pseqdesc;
	matrix3x4		bonematrix;
	mstudiobone_t	*pbones;
	int		i;

	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];

	if( e->curstate.sequence < 0 || e->curstate.sequence >= m_pStudioHeader->numseq ) 
	{
		int sequence = (short)e->curstate.sequence;
		ALERT( at_warning, "StudioSetupBones: sequence %i/%i out of range for model %s\n", sequence, m_pStudioHeader->numseq, m_pRenderModel->name );
		e->curstate.sequence = 0;
          }

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->curstate.sequence;
	float f = StudioEstimateFrame( pseqdesc );
	if( CheckBoneCache( f )) return; // using a cached bones no need transformations

	if( m_boneSetup.GetNumIKChains( ))
	{
		if( FBitSet( e->curstate.effects, EF_NOINTERP ))
			m_pModelInstance->m_ik.ClearTargets();
		m_pModelInstance->m_ik.Init( &m_boneSetup, e->angles, e->origin, tr.time, tr.realframecount );
		pIK = &m_pModelInstance->m_ik;
	}

	float dadt = StudioEstimateInterpolant();
	float cycle = f / m_boneSetup.LocalMaxFrame( e->curstate.sequence );

	StudioInterpolateBlends( e, dadt );

	m_boneSetup.InitPose( pos, q );
	m_boneSetup.UpdateRealTime( tr.time );
	if( CVAR_TO_BOOL( m_pCvarCompatible ))
		m_boneSetup.CalcBoneAdj( adj, m_pModelInstance->m_controller, e->mouth.mouthopen );
	m_boneSetup.AccumulatePose( pIK, pos, q, e->curstate.sequence, cycle, 1.0 );
	m_pModelInstance->lerp.frame = f;

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	pboneinfo = (mstudioboneinfo_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex + m_pStudioHeader->numbones * sizeof( mstudiobone_t ));

	if( m_pPlayerInfo && ( m_pPlayerInfo->gaitsequence < 0 || m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq )) 
		m_pPlayerInfo->gaitsequence = 0;

	// calc gait animation
	if( m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0 )
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

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;
		f = StudioEstimateGaitFrame( pseqdesc );

		// convert gaitframe to cycle
		cycle = f / m_boneSetup.LocalMaxFrame( m_pPlayerInfo->gaitsequence );

		m_boneSetup.SetBoneWeights( m_flGaitBoneWeights ); // install weightlist for gait sequence
		m_boneSetup.AccumulatePose( pIK, pos, q, m_pPlayerInfo->gaitsequence, cycle, 1.0 );
		m_boneSetup.SetBoneWeights( NULL ); // back to default rules
		m_pPlayerInfo->gaitframe = f;
	}

	// blends from previous sequences
	for( i = 0; i < MAX_SEQBLENDS; i++ )
		BlendSequence( pos, q, &m_pModelInstance->m_seqblend[i] );

	CIKContext auto_ik;
	auto_ik.Init( &m_boneSetup, e->angles, e->origin, 0.0f, 0 );
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

	StudioCalcBonesProcedural( pos, q );

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
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

			if( pbones[i].parent == -1 ) goalMX = bonematrix;
			else goalMX = m_pModelInstance->m_pbones[pbones[i].parent].ConcatTransforms( bonematrix );

			// get jiggle properties from QC data
			mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)((byte *)m_pStudioHeader + pboneinfo[i].procindex);
			if( !m_pModelInstance->m_pJiggleBones ) m_pModelInstance->m_pJiggleBones = new CJiggleBones;

			// do jiggle physics
			if( pboneinfo[i].proctype == STUDIO_PROC_JIGGLE )
				m_pModelInstance->m_pJiggleBones->BuildJiggleTransformations( i, tr.time, jiggleInfo, goalMX, m_pModelInstance->m_pbones[i] );
			else m_pModelInstance->m_pbones[i] = goalMX; // fallback
		}
		else
		{
			if( pbones[i].parent == -1 ) m_pModelInstance->m_pbones[i] = bonematrix;
			else m_pModelInstance->m_pbones[i] = m_pModelInstance->m_pbones[pbones[i].parent].ConcatTransforms( bonematrix );
		}
	}
}

/*
====================
StudioMergeBones

FIXME: this need a been rewrited with bone weights
====================
*/
void CStudioModelRenderer :: StudioMergeBones( matrix3x4 bones[], matrix3x4 cached_bones[], model_t *pModel, model_t *pParentModel )
{
	matrix3x4		bonematrix;
	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];
	float		poseparams[MAXSTUDIOPOSEPARAM];
	int		sequence = m_pCurrentEntity->curstate.sequence;
	model_t		*oldmodel = m_pRenderModel;
	studiohdr_t	*oldheader = m_pStudioHeader;

	ASSERT( pModel != NULL && pModel->type == mod_studio );
	ASSERT( pParentModel != NULL && pParentModel->type == mod_studio );

	RI->currentmodel = m_pRenderModel = pModel;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

	// tell the bonesetup about current model
	m_boneSetup.SetStudioPointers( m_pStudioHeader, poseparams ); // don't touch original parameters

	if( sequence < 0 || sequence >= m_pStudioHeader->numseq ) 
		sequence = 0;

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
		for( int j = 0; j < m_pParentHeader->numbones; j++ )
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
			if( pchildbones[i].parent == -1 ) bones[i] = bonematrix;
			else bones[i] = bones[pchildbones[i].parent].ConcatTransforms( bonematrix );
		}
	}

	RI->currentmodel = m_pRenderModel = oldmodel;
	m_pStudioHeader = oldheader;

	// restore the bonesetup pointers
	m_boneSetup.SetStudioPointers( m_pStudioHeader, m_pModelInstance->m_poseparameter );
}

/*
====================
StudioBuildNormalTable

NOTE: m_pSubModel must be set
====================
*/
void CStudioModelRenderer :: StudioBuildNormalTable( void )
{
	cl_entity_t	*e = m_pCurrentEntity;
	mstudiomesh_t	*pmesh;
	int		i, j;

	ASSERT( m_pSubModel != NULL );

	// reset chrome cache
	for( i = 0; i < m_pStudioHeader->numbones; i++ )
		m_chromeAge[i] = 0;

	for( i = 0; i < m_pSubModel->numverts; i++ )
		m_normaltable[i] = -1;

	for( j = 0; j < m_pSubModel->nummesh; j++ ) 
	{
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
		ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

		while( i = *( ptricmds++ ))
		{
			if( i < 0 ) i = -i;

			for( ; i > 0; i--, ptricmds += 4 )
			{
				if( m_normaltable[ptricmds[0]] < 0 )
					m_normaltable[ptricmds[0]] = ptricmds[1];
			}
		}
	}

	m_chromeOrigin.x = cos( m_pCvarGlowShellFreq->value * tr.time ) * 4000.0f;
	m_chromeOrigin.y = sin( m_pCvarGlowShellFreq->value * tr.time ) * 4000.0f;
	m_chromeOrigin.z = cos( m_pCvarGlowShellFreq->value * tr.time * 0.33f ) * 4000.0f;

	if( e->curstate.rendercolor.r || e->curstate.rendercolor.g || e->curstate.rendercolor.b )
		gEngfuncs.pTriAPI->Color4ub( e->curstate.rendercolor.r, e->curstate.rendercolor.g, e->curstate.rendercolor.b, 255 );
	else gEngfuncs.pTriAPI->Color4ub( 255, 255, 255, 255 );
}

/*
====================
StudioGenerateNormals

NOTE: m_pSubModel must be set
m_verts must be computed
====================
*/
void CStudioModelRenderer :: StudioGenerateNormals( void )
{
	Vector		e0, e1, norm;
	int		v0, v1, v2;
	mstudiomesh_t	*pmesh;
	int		i, j;

	ASSERT( m_pSubModel != NULL );

	for( i = 0; i < m_pSubModel->numverts; i++ )
		m_norms[i] = g_vecZero;

	for( j = 0; j < m_pSubModel->nummesh; j++ ) 
	{
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
		ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

		while( i = *( ptricmds++ ))
		{
			if( i < 0 )
			{
				i = -i;

				if( i > 2 )
				{
					v0 = ptricmds[0]; ptricmds += 4;
					v1 = ptricmds[0]; ptricmds += 4;

					for( i -= 2; i > 0; i--, ptricmds += 4 )
					{
						v2 = ptricmds[0];

						e0 = m_verts[v1] - m_verts[v0];
						e1 = m_verts[v2] - m_verts[v0];
						norm = CrossProduct( e1, e0 );

						m_norms[v0] += norm;
						m_norms[v1] += norm;
						m_norms[v2] += norm;

						v1 = v2;
					}
				}
				else
				{
					ptricmds += i;
				}
			}
			else
			{
				if( i > 2 )
				{
					qboolean	odd = false;

					v0 = ptricmds[0]; ptricmds += 4;
					v1 = ptricmds[0]; ptricmds += 4;

					for( i -= 2; i > 0; i--, ptricmds += 4 )
					{
						v2 = ptricmds[0];

						e0 = m_verts[v1] - m_verts[v0];
						e1 = m_verts[v2] - m_verts[v0];
						norm = CrossProduct( e1, e0 );

						m_norms[v0] += norm;
						m_norms[v1] += norm;
						m_norms[v2] += norm;

						if( odd ) v1 = v2;
						else v0 = v2;

						odd = !odd;
					}
				}
				else
				{
					ptricmds += i;
				}
			}
		}
	}

	for( i = 0; i < m_pSubModel->numverts; i++ )
		m_norms[i] = m_norms[i].Normalize();
}

/*
====================
StudioSetupChrome

====================
*/
void CStudioModelRenderer :: StudioSetupChrome( float *pchrome, int bone, const Vector &normal )
{
	float	n;

	if( m_chromeAge[bone] != m_chromeCount )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		Vector	chromeupvec;	// g_chrome t vector in world reference frame
		Vector	chromerightvec;	// g_chrome s vector in world reference frame
		Vector	tmp;		// vector pointing at bone in world reference frame

		tmp = (-m_chromeOrigin + m_pworldtransform[bone][3]).Normalize();
		chromeupvec = CrossProduct( tmp, RI->vright ).Normalize();
		chromerightvec = CrossProduct( tmp, chromeupvec ).Normalize();
		m_chromeUp[bone] = m_pworldtransform[bone].VectorIRotate( chromeupvec );
		m_chromeRight[bone] = m_pworldtransform[bone].VectorIRotate( chromerightvec );
		m_chromeAge[bone] = m_chromeCount;
	}

	// calc s coord
	n = DotProduct( normal, m_chromeRight[bone] );
	pchrome[0] = (n + 1.0f) * 32.0f;

	// calc t coord
	n = DotProduct( normal, m_chromeUp[bone] );
	pchrome[1] = (n + 1.0f) * 32.0f;
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer :: StudioCalcAttachments( matrix3x4 bones[] )
{
	if( m_pCurrentEntity->modelhandle == INVALID_HANDLE || !m_pModelInstance )
		return; // too early ?

	if( FBitSet( m_pModelInstance->info_flags, MF_ATTACHMENTS_DONE ))
		return; // already computed

	StudioAttachment_t *att = m_pModelInstance->attachment;
	mstudioattachment_t *pattachment;
	cl_entity_t *e = m_pCurrentEntity;

	// prevent to compute env_static bounds every frame
	if( FBitSet( e->curstate.iuser1, CF_STATIC_ENTITY ))
		SetBits( m_pModelInstance->info_flags, MF_ATTACHMENTS_DONE );

	if( m_pStudioHeader->numattachments <= 0 )
	{
		// clear attachments
		for( int i = 0; i < MAXSTUDIOATTACHMENTS; i++ )
		{
			if( i < 4 ) e->attachment[i] = e->origin;
			att[i].pos = e->origin;
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
		Vector p0 = bones[pattachment[i].bone].VectorTransform( pattachment[i].org );
		Vector p1 = bones[pattachment[i].bone].GetOrigin();

		// turn back to worldspace
		p0 = m_pModelInstance->m_protationmatrix.VectorTransform( p0 );
		p1 = m_pModelInstance->m_protationmatrix.VectorTransform( p1 );

		// merge attachments position for viewmodel
		if( m_fDrawViewModel )
		{
			StudioFormatAttachment( p0 );
			StudioFormatAttachment( p1 );
		}

		att[i].pos = p0;
		att[i].dir = (p0 - p1).Normalize(); // forward vec
//		VectorAngles( att[i].dir, att[i].angles );
		if( i < 4 ) e->attachment[i] = p0;
	}
}

/*
====================
StudioGetAttachment

====================
*/
void CStudioModelRenderer :: StudioGetAttachment( const cl_entity_t *ent, int iAttachment, Vector *pos, Vector *dir )
{
	if( !ent || !ent->model || ( !pos && !dir  ))
		return;

	studiohdr_t *phdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( ent->model );
	if( !phdr ) return;

	if( ent->modelhandle == INVALID_HANDLE )
		return; // too early ?

	ModelInstance_t *inst = &m_ModelInstances[ent->modelhandle];

	// make sure we not overflow
	iAttachment = bound( 0, iAttachment, phdr->numattachments - 1 );

	if( pos ) *pos = inst->attachment[iAttachment].pos;
	if( dir ) *dir = inst->attachment[iAttachment].dir;
}

/*
====================
StudioSetupModel

glow shell used it
====================
*/
int CStudioModelRenderer :: StudioSetupModel( int bodypart, void **ppbodypart, void **ppsubmodel )
{
	mstudiobodyparts_t *pbodypart;
	mstudiomodel_t *psubmodel;
	int index;

	if( bodypart > m_pStudioHeader->numbodyparts )
		bodypart = 0;
	pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = m_pCurrentEntity->curstate.body / pbodypart->base;
	index = index % pbodypart->nummodels;

	psubmodel = (mstudiomodel_t *)((byte *)m_pStudioHeader + pbodypart->modelindex) + index;

	*ppbodypart = pbodypart;
	*ppsubmodel = psubmodel;

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
	mstudiolight_t	*light = &m_pModelInstance->lighting;

	if( FBitSet( flags, STUDIO_NF_FULLBRIGHT ) || FBitSet( m_pCurrentEntity->curstate.effects, EF_FULLBRIGHT ) || R_FullBright( ))
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
		else lightcos = DotProduct( normal, light->plightvec ); // -1 colinear, 1 opposite
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
StudioStaticLight

===============
*/
void CStudioModelRenderer :: StudioStaticLight( cl_entity_t *ent )
{
	// setup advanced vertexlighting for env_static entities
	if(( ent->curstate.iuser3 > 0 ) && world->vertex_lighting != NULL )
	{
		int		cacheID = ent->curstate.iuser3 - 1;
		dvlightlump_t	*vl = world->vertex_lighting;

		if( FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
		{
			// here we does nothing
		}
		else if( !FBitSet( m_pModelInstance->info_flags, MF_VL_BAD_CACHE ))
		{
			// first initialization
			if( cacheID < vl->nummodels && vl->dataofs[cacheID] != -1 )
			{
				// we have ID of vertex light cache and cache is present
				CreateMeshCacheVL( (dmodellight_t *)((byte *)world->vertex_lighting + vl->dataofs[cacheID]), cacheID );
			}
		}
	}

	if( !FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
	{
		float	dynamic = r_dynamic->value;
		alight_t	lighting;
		Vector	dir;

		lighting.plightvec = dir;

		// setup classic Half-Life lighting
		r_dynamic->value = 0.0f; // ignore dlights
		IEngineStudio.StudioDynamicLight( ent, &lighting );
		r_dynamic->value = dynamic;

		m_pModelInstance->lighting.ambientlight = lighting.ambientlight;
		m_pModelInstance->lighting.shadelight = lighting.shadelight;
		m_pModelInstance->lighting.color = lighting.color;
		m_pModelInstance->lighting.plightvec = m_pModelInstance->m_plightmatrix.VectorIRotate( lighting.plightvec ); // turn back to model space
	}
}

/*
===============
StudioClientEvents

===============
*/
void CStudioModelRenderer :: StudioClientEvents( void )
{
	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t	*pevent;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
	pevent = (mstudioevent_t *)((byte *)m_pStudioHeader + pseqdesc->eventindex);

	// no events for this animation or gamepaused
	if( pseqdesc->numevents == 0 || tr.time == tr.oldtime )
		return;

	float f = StudioEstimateFrame( pseqdesc ) + 0.01f; // get start offset
	float start = f - m_pCurrentEntity->curstate.framerate * (tr.time - tr.oldtime) * pseqdesc->fps;

	for( int i = 0; i < pseqdesc->numevents; i++ )
	{
		// ignore all non-client-side events
		if( pevent[i].event < EVENT_CLIENT )
			continue;

		if( (float)pevent[i].frame > start && f >= (float)pevent[i].frame )
			HUD_StudioEvent( &pevent[i], m_pCurrentEntity );
	}
}

/*
===============
StudioSetRenderMode

===============
*/
void CStudioModelRenderer :: StudioSetRenderMode( const int rendermode )
{
	pglDisable( GL_ALPHA_TEST );

	switch( rendermode )
	{
	case kRenderNormal:
		break;
	case kRenderTransColor:
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglEnable( GL_BLEND );
		break;
	case kRenderTransAdd:
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglColor4f( tr.blend, tr.blend, tr.blend, 1.0f );
		pglBlendFunc( GL_ONE, GL_ONE );
		GL_DepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
		break;
	default:
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglColor4f( 1.0f, 1.0f, 1.0f, tr.blend );
		GL_DepthMask( GL_TRUE );
		pglEnable( GL_BLEND );
		break;
	}
}

/*
===============
StudioDrawMeshChrome

===============
*/
void CStudioModelRenderer :: StudioDrawMeshChrome( short *ptricmds, float s, float t, float scale )
{
	int	i;

	while( i = *( ptricmds++ ))
	{
		if( i < 0 )
		{
			pglBegin( GL_TRIANGLE_FAN );
			i = -i;
		}
		else pglBegin( GL_TRIANGLE_STRIP );

		for( ; i > 0; i--, ptricmds += 4 )
		{
			int idx = m_normaltable[ptricmds[0]];
			Vector v = m_verts[ptricmds[0]] + m_norms[ptricmds[0]] * scale;
			pglTexCoord2f( m_chrome[idx][0] * s, m_chrome[idx][1] * t );
			pglVertex3fv( v );
		}

		pglEnd();
	}
}

/*
===============
StudioDrawPoints

===============
*/
void CStudioModelRenderer :: StudioDrawPoints( void )
{
	float	shellscale = 0.0f;
	int	i, j, k;

	ASSERT( m_pStudioHeader != NULL );


	// grab the model textures array (with remap infos)
	mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);
	Vector *pstudioverts = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
	int m_skinnum = bound( 0, m_pCurrentEntity->curstate.skin, MAXSTUDIOSKINS - 1 );
	short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);
	m_chromeCount++;

	if( FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS ) && m_pSubModel->blendvertinfoindex != 0 )
	{
		mstudioboneweight_t	*pvertweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + m_pSubModel->blendvertinfoindex);
		matrix3x4 skinMat;

		// compute weighted vertexes
		for( int i = 0; i < m_pSubModel->numverts; i++ )
		{
			ComputeSkinMatrix( &pvertweight[i], m_pworldtransform, skinMat );
			m_verts[i] = skinMat.VectorTransform( pstudioverts[i] );
		}
	}
	else
	{
		byte *pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);

		// compute unweighted vertexes
		for( int i = 0; i < m_pSubModel->numverts; i++ )
			m_verts[i] = m_pworldtransform[pvertbone[i]].VectorTransform( pstudioverts[i] );
	}

	// generate shared normals for properly scaling glowing shell
	shellscale = Q_max( GLOWSHELL_FACTOR, RI->currententity->curstate.renderamt * GLOWSHELL_FACTOR );
	StudioBuildNormalTable();
	StudioGenerateNormals();

	Vector *pstudionorms = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	byte *pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);

	for( j = k = 0; j < m_pSubModel->nummesh; j++ ) 
	{
		for( i = 0; i < pmesh[j].numnorms; i++, k++, pstudionorms++, pnormbone++ )
			StudioSetupChrome( m_chrome[k], *pnormbone, (float *)pstudionorms );
	}

	for( j = 0; j < m_pSubModel->nummesh; j++ ) 
	{
		short	*ptricmds = (short *)((byte *)m_pStudioHeader + pmesh[j].triindex);
		float	s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
		float	t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;

		StudioDrawMeshChrome( ptricmds, s, t, shellscale );
	}
}

/*
===============
StudioDrawPoints

===============
*/
void CStudioModelRenderer :: StudioDrawShell( void )
{
	if( FBitSet( RI->params, RP_SHADOWVIEW ))
		return;

	if( m_pCurrentEntity->curstate.renderfx != kRenderFxGlowShell )
		return;

	// setup worldtransform array
	if( m_pRenderModel->poseToBone != NULL )
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i].ConcatTransforms( m_pRenderModel->poseToBone->posetobone[i] );
	}
	else
	{
		// no pose to bone just copy the bones
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i];
	}

	gEngfuncs.pTriAPI->SpriteTexture( m_pChromeSprite, 0 );

	for( int i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );
		IEngineStudio.GL_SetRenderMode( kRenderTransAdd );
		StudioDrawPoints();
	}

	model_t *pweaponmodel = NULL;

	if( m_pCurrentEntity->curstate.weaponmodel )
		pweaponmodel = IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel );

	if( pweaponmodel )
	{
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pweaponmodel );

		// FIXME: allow boneweights for weaponmodel?
		for( i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pwpnbones[i];

		for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
		{
			StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );
			IEngineStudio.GL_SetRenderMode( kRenderTransAdd );
			StudioDrawPoints();
		}

		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
	}
}

/*
===============
StudioDrawHulls

===============
*/
void CStudioModelRenderer :: StudioDrawHulls( void )
{
	float	alpha, lv;
	int	i, j;

	if( r_drawentities->value == 4 )
		alpha = 0.5f;
	else alpha = 1.0f;

	R_TransformForEntity( m_pModelInstance->m_protationmatrix );

	// setup bone lighting
	for( i = 0; i < m_pStudioHeader->numbones; i++ )
		m_bonelightvecs[i] = m_pModelInstance->m_pbones[i].VectorIRotate( m_pModelInstance->lighting.plightvec );

	GL_Bind( GL_TEXTURE0, tr.whiteTexture );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	for( i = 0; i < m_pStudioHeader->numhitboxes; i++ )
	{
		mstudiobbox_t	*pbbox = (mstudiobbox_t *)((byte *)m_pStudioHeader + m_pStudioHeader->hitboxindex);
		vec3_t		tmp, p[8];

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

			gEngfuncs.pTriAPI->Brightness( lv );
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
	if( m_pCurrentEntity == GET_VIEWMODEL( ))
		return;

	R_LoadIdentity();

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

		gEngfuncs.pTriAPI->Brightness( lv );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][0]] );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][1]] );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][2]] );
		gEngfuncs.pTriAPI->Vertex3fv( p[g_boxpnt[i][3]] );
	}

	gEngfuncs.pTriAPI->End();
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

	R_TransformForEntity( m_pModelInstance->m_protationmatrix );
	pglDisable( GL_TEXTURE_2D );

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
	pglEnable( GL_TEXTURE_2D );
}

void CStudioModelRenderer :: StudioDrawAttachments( void )
{
	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );
	
	for( int i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		mstudioattachment_t	*pattachments;
		Vector v[4];

		pattachments = (mstudioattachment_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);		
		v[0] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( pattachments[i].org );
		v[1] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( pattachments[i].vectors[0] );
		v[2] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( pattachments[i].vectors[1] );
		v[3] = m_pModelInstance->m_pbones[pattachments[i].bone].VectorTransform( pattachments[i].vectors[2] );
		
		pglBegin( GL_LINES );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv( v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv (v[1] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv (v[0] );
		pglColor3f( 1, 1, 1 );
		pglVertex3fv (v[2] );
		pglColor3f( 1, 0, 0 );
		pglVertex3fv (v[0] );
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

	pglEnable( GL_TEXTURE_2D );
	pglEnable( GL_DEPTH_TEST );
}

/*
===============
StudioSetupRenderer

===============
*/
void CStudioModelRenderer :: StudioSetupRenderer( int rendermode )
{
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	pglDisable( GL_ALPHA_TEST );
	pglShadeModel( GL_SMOOTH );
}

/*
===============
StudioRestoreRenderer

===============
*/
void CStudioModelRenderer :: StudioRestoreRenderer( void )
{
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	pglShadeModel( GL_FLAT );
	GL_Blend( GL_FALSE );
}

/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer :: StudioRenderModel( void )
{
#if 0
	m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
	m_pCurrentEntity->curstate.renderamt = 255;
#endif
	StudioSetupRenderer( m_pCurrentEntity->curstate.rendermode );
	
	if( r_drawentities->value == 2 )
	{
		StudioDrawBones ();
	}
	else if( r_drawentities->value == 3 )
	{
		StudioDrawHulls ();
	}
	else
	{
		DrawStudioMeshes ();
		GL_BindShader( NULL );
		StudioDrawShell ();

		if( r_drawentities->value == 4 )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			StudioDrawHulls ();
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		}
		else if( r_drawentities->value == 5 )
		{
			StudioDrawAbsBBox( );
		}
		else if( r_drawentities->value == 6 )
		{
			StudioDrawAttachments( );
		}
	}

	StudioRestoreRenderer();
}

/*
====================
StudioDrawModel

====================
*/
int CStudioModelRenderer :: StudioDrawModel( int flags )
{
	alight_t lighting;
	Vector dir;

	if( !StudioSetEntity( IEngineStudio.GetCurrentEntity( )))
		return 0;

	if( FBitSet( flags, STUDIO_RENDER ))
	{
		if( !StudioComputeBBox( m_pCurrentEntity ))
			return 0; // invalid sequence

		if( !Mod_CheckBoxVisible( m_pModelInstance->absmin, m_pModelInstance->absmax ))
			return 0;

		// see if the bounding box lets us trivially reject, also sets
		if( R_CullModel( m_pCurrentEntity, m_pModelInstance->absmin, m_pModelInstance->absmax ))
			return 0;

		m_pModelInstance->visframe = tr.realframecount; // visible

		r_stats.c_studio_models_drawn++; // render data cache cookie

		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	if( m_pModelInstance->cached_frame != tr.realframecount )
	{
		if( m_pCurrentEntity->player || m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer )
			m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

		StudioSetUpTransform ();

		if( m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW && m_pCurrentEntity->curstate.aiment > 0 )
		{
			cl_entity_t *parent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->curstate.aiment );
			if( parent != NULL && parent->modelhandle != INVALID_HANDLE )
			{
				ModelInstance_t *inst = &m_ModelInstances[parent->modelhandle];
				StudioMergeBones( m_pModelInstance->m_pbones, inst->m_pbones, m_pRenderModel, parent->model );
			}
			else
			{
				ALERT( at_error, "FollowEntity: %i with model %s has missed parent!\n", m_pCurrentEntity->index, m_pRenderModel->name );
				return 0;
			}
		}
		else StudioSetupBones ( );

		m_pPlayerInfo = NULL;

		// grab the static lighting from world
		StudioStaticLight( m_pCurrentEntity );

		// convert bones into compacted GLSL array
		if( m_pRenderModel->poseToBone != NULL )
		{
			for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			{
				matrix3x4 out = m_pModelInstance->m_pbones[i].ConcatTransforms( m_pRenderModel->poseToBone->posetobone[i] );
				m_pModelInstance->m_studioquat[i] = out.GetQuaternion();
				m_pModelInstance->m_studiopos[i] = out.GetOrigin();
			}
		}
		else
		{
			for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			{
				m_pModelInstance->m_studioquat[i] = m_pModelInstance->m_pbones[i].GetQuaternion();
				m_pModelInstance->m_studiopos[i] = m_pModelInstance->m_pbones[i].GetOrigin();
			}
		}

		model_t *pweaponmodel = NULL;

		if( m_pCurrentEntity->curstate.weaponmodel )
			pweaponmodel = IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel );

		// compute weaponmodel matrices too
		if( pweaponmodel )
		{
			StudioMergeBones( m_pModelInstance->m_pwpnbones, m_pModelInstance->m_pbones, pweaponmodel, m_pRenderModel );

			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pweaponmodel );

			// convert weapon bones into compacted GLSL array
			if( pweaponmodel->poseToBone != NULL )
			{
				for( int i = 0; i < m_pStudioHeader->numbones; i++ )
				{
					matrix3x4 out = m_pModelInstance->m_pwpnbones[i].ConcatTransforms( pweaponmodel->poseToBone->posetobone[i] );
					m_pModelInstance->m_weaponquat[i] = out.GetQuaternion();
					m_pModelInstance->m_weaponpos[i] = out.GetOrigin();
				}
			}
			else
			{
				for( int i = 0; i < m_pStudioHeader->numbones; i++ )
				{
					m_pModelInstance->m_weaponquat[i] = m_pModelInstance->m_pwpnbones[i].GetQuaternion();
					m_pModelInstance->m_weaponpos[i] = m_pModelInstance->m_pwpnbones[i].GetOrigin();
				}
			}

			// restore right header
			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
		}

		// now this frame cached
		m_pModelInstance->cached_frame = tr.realframecount;
	}

	if( FBitSet( flags, STUDIO_EVENTS ))
	{
		// calc attachments only once per frame
		StudioCalcAttachments( m_pModelInstance->m_pbones );
		StudioClientEvents( );

		// copy attachments into global entity array
		// g-cont: share client attachments with viewmodel
		if( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = GET_ENTITY( m_pCurrentEntity->index );
			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( Vector ) * 4 );
		}
	}

	if( FBitSet( flags, STUDIO_RENDER ))
	{
		mbodypart_t *pbodyparts = NULL;
		model_t *pweaponmodel = NULL;

		if( !FBitSet( RI->params, RP_SHADOWVIEW ))
		{
			if( m_pCurrentEntity->player )
				m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

			// get remap colors
			if( m_pPlayerInfo != NULL )
			{
				m_nTopColor = bound( 0, m_pPlayerInfo->topcolor, 360 );
				m_nBottomColor = bound( 0, m_pPlayerInfo->bottomcolor, 360 );
			}
			else
			{
				m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
				m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;
			}

			IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );
                    }

		m_nNumDrawMeshes = 0;

		// change shared model with instanced model for this entity (it has personal vertex light cache)
		if( m_pModelInstance->m_VlCache != NULL )
			pbodyparts = m_pModelInstance->m_VlCache->bodyparts;

		for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
			AddBodyPartToDrawList( m_pStudioHeader, pbodyparts, i, ( RI->currentlight != NULL ), true );

		if( m_pCurrentEntity->curstate.weaponmodel )
			pweaponmodel = IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel );

		if( pweaponmodel )
		{
			m_pRenderModel = RI->currentmodel = pweaponmodel;
			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

			// add weaponmodel parts (weaponmodel can't cache materials because doesn't has instance)
			for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
				AddBodyPartToDrawList( m_pStudioHeader, NULL, i, ( RI->currentlight != NULL ), false );

			m_pRenderModel = RI->currentmodel = m_pModelInstance->m_pModel;
			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

			r_stats.c_studio_models_drawn++;
		}

		StudioRenderModel( );
	}

	return 1;
}

/*
====================
StudioFormatAttachment

====================
*/
void CStudioModelRenderer :: StudioFormatAttachment( Vector &point )
{
	float worldx = tan( (float)RI->fov_x * M_PI / 360.0 );
	float viewx = tan( m_flViewmodelFov * M_PI / 360.0 );

	// BUGBUG: workaround
	if( viewx == 0.0f ) viewx = 1.0f;

	// aspect ratio cancels out, so only need one factor
	// the difference between the screen coordinates of the 2 systems is the ratio
	// of the coefficients of the projection matrices (tan (fov/2) is that coefficient)
	float factor = worldx / viewx;

	// get the coordinates in the viewer's space.
	Vector tmp = point - RI->vieworg;
	Vector vTransformed;

	vTransformed.x = DotProduct( RI->vright, tmp );
	vTransformed.y = DotProduct( RI->vup, tmp );
	vTransformed.z = DotProduct( RI->vforward, tmp );
	vTransformed.x *= factor;
	vTransformed.y *= factor;

	// Transform back to world space.
	Vector vOut = (RI->vright * vTransformed.x) + (RI->vup * vTransformed.y) + (RI->vforward * vTransformed.z);
	point = RI->vieworg + vOut;
}

/*
=================
DrawStudioModel
=================
*/
void CStudioModelRenderer :: DrawStudioModelInternal( cl_entity_t *e )
{
	if( RI->params & RP_ENVVIEW )
		return;

	if( e->player )
	{
		StudioDrawModel( STUDIO_RENDER|STUDIO_EVENTS );
	}
	else
	{
		if( e->curstate.movetype == MOVETYPE_FOLLOW && e->curstate.aiment > 0 )
		{
			cl_entity_t *parent = GET_ENTITY( e->curstate.aiment );

			if( parent && parent->model && parent->model->type == mod_studio )
			{
				RI->currententity = parent;
				StudioDrawModel( 0 );
				e->curstate.origin = parent->curstate.origin;
				e->origin = parent->origin;
				RI->currententity = e;
			}
		}

		StudioDrawModel( STUDIO_RENDER|STUDIO_EVENTS );
	}
}

/*
===============
ChooseStudioProgram

Select the program for mesh (diffuse\bump\debug)
===============
*/
word CStudioModelRenderer :: ChooseStudioProgram( studiohdr_t *phdr, mstudiomaterial_t *mat, bool lightpass )
{
	bool vertex_lighting = FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ) ? true : false;
	bool bone_weighting = (m_pRenderModel->poseToBone != NULL) ? true : false;
	bool fullbright = false;

	if( FBitSet( RI->params, RP_SHADOWVIEW ))
		return (glsl.studioDepthFill[bone_weighting] - glsl_programs);

	switch( m_pCurrentEntity->curstate.rendermode )
	{
	case kRenderTransAdd: 
		fullbright = true;
		break;
	}

	if( lightpass )
		return GL_UberShaderForDlightStudio( RI->currentlight, mat, bone_weighting, phdr->numbones );
	return GL_UberShaderForSolidStudio( mat, vertex_lighting, bone_weighting, fullbright, phdr->numbones );
}

/*
====================
AddMeshToDrawList

====================
*/
void CStudioModelRenderer :: AddMeshToDrawList( studiohdr_t *phdr, const vbomesh_t *mesh, bool lightpass, bool cached_materials )
{
	int m_skinnum = bound( 0, m_pCurrentEntity->curstate.skin, phdr->numskinfamilies - 1 );
	short *pskinref = (short *)((byte *)phdr + phdr->skinindex);
	pskinref += (m_skinnum * phdr->numskinref);

	mstudiomaterial_t *mat = NULL;
	if( cached_materials )
		mat = &m_pModelInstance->materials[pskinref[mesh->skinref]]; // NOTE: use local copy for right cache shadernums
	else mat = &m_pRenderModel->materials[pskinref[mesh->skinref]];

	// goes into regular arrays
	if( FBitSet( RI->params, RP_SHADOWVIEW ))
		lightpass = false;

	if( lightpass )
	{
		if( FBitSet( mat->flags, STUDIO_NF_FULLBRIGHT ))
			return; // can't light fullbrights

		if( FBitSet( mat->flags, STUDIO_NF_NODLIGHT ))
			return; // shader was failed to compile

		if( m_nNumLightMeshes >= MAX_MODEL_MESHES )
		{
			ALERT( at_error, "R_AddMeshToLightList: light list is full\n" );
			return;
		}
	}
	else
	{
		if( FBitSet( mat->flags, STUDIO_NF_NODRAW ))
			return; // shader was failed to compile

		if( m_nNumDrawMeshes >= MAX_MODEL_MESHES )
		{
			ALERT( at_error, "R_AddMeshToDrawList: mesh draw list is full\n" );
			return;
		}
	}
	gl_studiomesh_t *entry = NULL;
	word hProgram = 0;

	if( !( hProgram = ChooseStudioProgram( phdr, mat, lightpass )))
		return; // failed to build shader, don't draw this surface

	if( lightpass ) entry = &m_LightMeshes[m_nNumLightMeshes++];
	else entry = &m_DrawMeshes[m_nNumDrawMeshes++];

	entry->mesh = (vbomesh_t *)mesh;
	entry->hProgram = hProgram;
	entry->parent = m_pCurrentEntity;
	entry->model = m_pRenderModel;
	entry->additive = FBitSet( mat->flags, STUDIO_NF_ADDITIVE ) ? true : false;
}

/*
====================
AddBodyPartToDrawList

====================
*/
void CStudioModelRenderer :: AddBodyPartToDrawList( studiohdr_t *phdr, mbodypart_s *bodyparts, int bodypart, bool lightpass, bool cached_materials )
{
	if( !bodyparts ) bodyparts = m_pRenderModel->studiocache->bodyparts;
	if( !bodyparts ) HOST_ERROR( "%s missed cache\n", m_pCurrentEntity->model->name );

	bodypart = bound( 0, bodypart, phdr->numbodyparts );
	mbodypart_t *pBodyPart = &bodyparts[bodypart];
	int index = m_pCurrentEntity->curstate.body / pBodyPart->base;
	index = index % pBodyPart->nummodels;

	msubmodel_t *pSubModel = pBodyPart->models[index];
	if( !pSubModel ) return; // blank submodel, just ignore

	for( int i = 0; i < pSubModel->nummesh; i++ )
		AddMeshToDrawList( phdr, &pSubModel->meshes[i], lightpass, cached_materials );
}

void CStudioModelRenderer :: DrawMeshFromBuffer( const vbomesh_t *mesh )
{
	pglBindVertexArray( mesh->vao );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, mesh->ibo );

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, mesh->numVerts - 1, mesh->numElems, GL_UNSIGNED_INT, 0 );
	else pglDrawElements( GL_TRIANGLES, mesh->numElems, GL_UNSIGNED_INT, 0 );

	r_stats.c_total_tris += (mesh->numElems / 3);
	r_stats.num_flushes++;
}

void CStudioModelRenderer :: AddStudioToLightList( plight_t *pl )
{
	mbodypart_t *pbodyparts = NULL;
	model_t *pweaponmodel = NULL;

	if( FBitSet( m_pCurrentEntity->curstate.effects, EF_FULLBRIGHT ))
		return;

	ASSERT( m_pModelInstance != NULL );

	if( m_pModelInstance->visframe != tr.realframecount )
		return;

	RI->currentmodel = m_pRenderModel = m_pModelInstance->m_pModel;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

	Vector modelpos = m_pModelInstance->m_protationmatrix.GetOrigin();
	float dist = (pl->origin - modelpos).Length();

	if( !dist || dist > ( pl->radius + m_pModelInstance->radius ))
		return;

	if( pl->frustum.CullBox( m_pModelInstance->absmin, m_pModelInstance->absmax ))
		return;

	// change shared model with instanced model for this entity (it has personal vertex light cache)
	if( m_pModelInstance->m_VlCache != NULL )
		pbodyparts = m_pModelInstance->m_VlCache->bodyparts;

	// all checks are passed, now all the model meshes will lighted
	for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
		AddBodyPartToDrawList( m_pStudioHeader, pbodyparts, i, true, true );

	if( m_pCurrentEntity->curstate.weaponmodel )
		pweaponmodel = IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel );

	if( pweaponmodel )
	{
		m_pRenderModel = RI->currentmodel = pweaponmodel;
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

		// add weaponmodel parts (weaponmodel can't cache materials because doesn't has instance)
		for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
			AddBodyPartToDrawList( m_pStudioHeader, NULL, i, true, false );

		m_pRenderModel = RI->currentmodel = m_pModelInstance->m_pModel;
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
	}
}

void CStudioModelRenderer :: BuildMeshListForLight( plight_t *pl )
{
	m_nNumLightMeshes = 0;
	tr.modelorg = pl->origin;

	AddStudioToLightList( pl );
}

void CStudioModelRenderer :: DrawLightForMeshList( plight_t *pl )
{
	mstudiomaterial_t	*cached_material = NULL;
	model_t		*cached_model = NULL;
	GLfloat		gl_lightViewProjMatrix[16];

	float y2 = (float)RI->viewport[3] - pl->h - pl->y;
	pglScissor( pl->x, y2, pl->w, pl->h );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( r_nosort )) QSortStudioMeshes( m_LightMeshes, 0, m_nNumLightMeshes - 1 );

	tr.modelorg = m_pModelInstance->m_protationmatrix.VectorITransform( RI->vieworg );
	Vector right = m_pModelInstance->m_plightmatrix.VectorIRotate( RI->vright );
	Vector lightorg = m_pModelInstance->m_protationmatrix.VectorITransform( pl->origin );
	Vector lightdir = m_pModelInstance->m_plightmatrix.VectorIRotate( pl->frustum.GetPlane( FRUSTUM_FAR )->normal );

	matrix4x4 lightView = pl->modelviewMatrix.ConcatTransforms( m_pModelInstance->m_protationmatrix );
	matrix4x4 projectionView = pl->projectionMatrix.Concat( lightView );
	projectionView.CopyToArray( gl_lightViewProjMatrix );

	// sorting list to reduce shader switches
	for( int i = 0; i < m_nNumLightMeshes; i++ )
	{
		gl_studiomesh_t *entry = &m_LightMeshes[i];
		RI->currentmodel = m_pRenderModel = entry->model;
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
		int m_skinnum = bound( 0, m_pCurrentEntity->curstate.skin, m_pStudioHeader->numskinfamilies - 1 );
		vbomesh_t *pMesh = entry->mesh;

		short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

		mstudiomaterial_t *mat = &m_pRenderModel->materials[pskinref[pMesh->skinref]];

		// begin draw the sorted list
		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &glsl_programs[entry->hProgram] );			

			ASSERT( RI->currentshader != NULL );

			// write constants
			pglUniformMatrix4fvARB( RI->currentshader->u_LightViewProjectionMatrix, 1, GL_FALSE, &gl_lightViewProjMatrix[0] );
			float shadowWidth = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_WIDTH, pl->shadowTexture );
			float shadowHeight = 1.0f / (float)RENDER_GET_PARM( PARM_TEX_HEIGHT, pl->shadowTexture );

			// depth scale and bias and shadowmap resolution
			pglUniform4fARB( RI->currentshader->u_LightDir, lightdir.x, lightdir.y, lightdir.z, pl->fov );
			pglUniform4fARB( RI->currentshader->u_LightDiffuse, pl->color.r / 255.0f, pl->color.g / 255.0f, pl->color.b / 255.0f, pl->lightFalloff );
			pglUniform4fARB( RI->currentshader->u_ShadowParams, shadowWidth, shadowHeight, -pl->projectionMatrix[2][2], pl->projectionMatrix[3][2] );
			pglUniform4fARB( RI->currentshader->u_LightOrigin, lightorg.x, lightorg.y, lightorg.z, ( 1.0f / pl->radius ));
			pglUniform3fARB( RI->currentshader->u_ViewOrigin, tr.modelorg.x, tr.modelorg.y, tr.modelorg.z );
			pglUniform3fARB( RI->currentshader->u_ViewRight, right.x, right.y, right.z );
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );

			GL_Bind( GL_TEXTURE1, pl->projectionTexture );
			GL_Bind( GL_TEXTURE2, pl->shadowTexture );

			// reset cache
			cached_material = NULL;
			cached_model = NULL;
		}

		if( cached_model != m_pRenderModel )
		{
			int num_bones = Q_min( m_pStudioHeader->numbones, glConfig.max_skinning_bones );

			// update bones array
			if( m_pRenderModel == IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel ))
			{
				pglUniform4fvARB( RI->currentshader->u_BoneQuaternion, num_bones, &m_pModelInstance->m_weaponquat[0][0] );
				pglUniform3fvARB( RI->currentshader->u_BonePosition, num_bones, &m_pModelInstance->m_weaponpos[0][0] );
			}
			else
			{
				pglUniform4fvARB( RI->currentshader->u_BoneQuaternion, num_bones, &m_pModelInstance->m_studioquat[0][0] );
				pglUniform3fvARB( RI->currentshader->u_BonePosition, num_bones, &m_pModelInstance->m_studiopos[0][0] );
			}

			cached_model = m_pRenderModel;
		}

		if( cached_material != mat )
		{
			if( CVAR_TO_BOOL( r_lightmap ) && !CVAR_TO_BOOL( r_fullbright ))
				GL_Bind( GL_TEXTURE0, tr.whiteTexture );
			else GL_Bind( GL_TEXTURE0, mat->gl_diffuse_id );

			if( mat->flags & STUDIO_NF_TWOSIDE )
				GL_Cull( GL_NONE );
			else GL_Cull( GL_FRONT );

			cached_material = mat;
		}

		DrawMeshFromBuffer( pMesh );
	}

	GL_Cull( GL_FRONT );
}

void CStudioModelRenderer :: RenderDynLightList( void )
{
	if( FBitSet( RI->params, RP_ENVVIEW ))
		return;

	if( !R_CountPlights( ))
		return;

	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	pglBlendFunc( GL_ONE, GL_ONE );
	pglEnable( GL_SCISSOR_TEST );
	plight_t *pl = cl_plights;

	for( int i = 0; i < MAX_PLIGHTS; i++, pl++ )
	{
		if( pl->die < tr.time || !pl->radius || pl->culled )
			continue;

		RI->currentlight = pl;

		if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
			continue;

		if( R_CullBox( pl->absmin, pl->absmax ))
			continue;

		// draw world from light position
		BuildMeshListForLight( pl );

		if( !m_nNumLightMeshes )
			continue;	// no interaction with this light?

		DrawLightForMeshList( pl );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	pglDisable( GL_SCISSOR_TEST );
	GL_CleanUpTextureUnits( 0 );
	RI->currentlight = NULL;
}

void CStudioModelRenderer :: DrawStudioMeshes( void )
{
	mstudiomaterial_t	*cached_material = NULL;
	cl_entity_t	*cached_entity = NULL;
	model_t		*cached_model = NULL;
	int		i;

	if( FBitSet( RI->params, RP_SHADOWVIEW ))
	{
		DrawStudioMeshesShadow();
		return;
	}

	if( !m_nNumDrawMeshes )
		return;

	if( m_pCurrentEntity->modelhandle == INVALID_HANDLE )
		return; // out of memory ?

	m_pModelInstance = &m_ModelInstances[m_pCurrentEntity->modelhandle];
	tr.modelorg = m_pModelInstance->m_protationmatrix.VectorITransform( RI->vieworg );
	Vector right = m_pModelInstance->m_plightmatrix.VectorIRotate( RI->vright );

	R_TransformForEntity( m_pModelInstance->m_protationmatrix );
//	R_LoadIdentity();
	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_TRUE );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( r_nosort )) QSortStudioMeshes( m_DrawMeshes, 0, m_nNumDrawMeshes - 1 );

	// sorting list to reduce shader switches
	for( i = 0; i < m_nNumDrawMeshes; i++ )
	{
		gl_studiomesh_t *entry = &m_DrawMeshes[i];
		RI->currentmodel = m_pRenderModel = entry->model;
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
		int m_skinnum = bound( 0, m_pCurrentEntity->curstate.skin, m_pStudioHeader->numskinfamilies - 1 );
		vbomesh_t *pMesh = entry->mesh;

		short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

		mstudiomaterial_t *mat = &m_pRenderModel->materials[pskinref[pMesh->skinref]];

		ASSERT( m_pCurrentEntity->modelhandle != INVALID_HANDLE );

		// begin draw the sorted list
		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &glsl_programs[entry->hProgram] );			

			ASSERT( RI->currentshader != NULL );

			// write constants
			pglUniform3fARB( RI->currentshader->u_ViewOrigin, tr.modelorg.x, tr.modelorg.y, tr.modelorg.z );
			pglUniform3fARB( RI->currentshader->u_ViewRight, right.x, right.y, right.z );
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );

			if( FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
				pglUniform4fvARB( RI->currentshader->u_GammaTable, 64, &tr.gamma_table[0][0] );

			// reset cache
			cached_material = NULL;
			cached_entity = NULL;
			cached_model = NULL;
		}

		if( cached_entity != m_pCurrentEntity || ( cached_model != m_pRenderModel ))
		{
			// update bones array
			mstudiolight_t *light = &m_pModelInstance->lighting;
			int num_bones = Q_min( m_pStudioHeader->numbones, glConfig.max_skinning_bones );
			Vector4D lightstyles;

			for( int map = 0; map < MAXLIGHTMAPS; map++ )
			{
				if( m_pModelInstance->styles[map] != 255 )
					lightstyles[map] = tr.lightstyles[m_pModelInstance->styles[map]];
				else lightstyles[map] = 0.0f;
			}

			if( m_pRenderModel == IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel ))
			{
				pglUniform4fvARB( RI->currentshader->u_BoneQuaternion, num_bones, &m_pModelInstance->m_weaponquat[0][0] );
				pglUniform3fvARB( RI->currentshader->u_BonePosition, num_bones, &m_pModelInstance->m_weaponpos[0][0] );
			}
			else
			{
				pglUniform4fvARB( RI->currentshader->u_BoneQuaternion, num_bones, &m_pModelInstance->m_studioquat[0][0] );
				pglUniform3fvARB( RI->currentshader->u_BonePosition, num_bones, &m_pModelInstance->m_studiopos[0][0] );
			}

			if( FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
			{
				pglUniform4fARB( RI->currentshader->u_LightStyleValues, lightstyles.x, lightstyles.y, lightstyles.z, lightstyles.w );
			}
			else
			{
				pglUniform3fARB( RI->currentshader->u_LightColor, light->color.x, light->color.y, light->color.z );
				pglUniform3fARB( RI->currentshader->u_LightDir, light->plightvec[0], light->plightvec[1], light->plightvec[2] );
				pglUniform1fARB( RI->currentshader->u_LightAmbient, light->ambientlight );
				pglUniform1fARB( RI->currentshader->u_LightShade, light->shadelight );
			}

			R_SetRenderColor( m_pCurrentEntity );
			cached_entity = m_pCurrentEntity;
			cached_model = m_pRenderModel;
		}

		if( cached_material != mat )
		{
			if( CVAR_TO_BOOL( r_lightmap ) && !CVAR_TO_BOOL( r_fullbright ))
				GL_Bind( GL_TEXTURE0, tr.whiteTexture );
			else if( FBitSet( mat->flags, STUDIO_NF_COLORMAP ))
				IEngineStudio.StudioSetupSkin( m_pStudioHeader, pskinref[pMesh->skinref] );
			else GL_Bind( GL_TEXTURE0, mat->gl_diffuse_id );

			if( mat->flags & STUDIO_NF_TWOSIDE )
				GL_Cull( GL_NONE );
			else GL_Cull( GL_FRONT );

			if( FBitSet( mat->flags, STUDIO_NF_MASKED|STUDIO_NF_HAS_ALPHA ))
			{
				pglAlphaFunc( GL_GREATER, 0.5f );
				GL_AlphaTest( GL_TRUE );
				GL_DepthMask( GL_TRUE );
				GL_Blend( GL_FALSE );
			}
			else if( FBitSet( mat->flags, STUDIO_NF_ADDITIVE ))
			{
				if( R_ModelOpaque( RI->currententity->curstate.rendermode ))
				{
					pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
					GL_DepthMask( GL_FALSE );
					GL_Blend( GL_TRUE );
				}
				else pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
			}
			else
			{
				if( R_ModelOpaque( RI->currententity->curstate.rendermode ))
				{
					GL_DepthMask( GL_TRUE );
					GL_Blend( GL_FALSE );
				}
				StudioSetRenderMode( m_pCurrentEntity->curstate.rendermode );
			}
			cached_material = mat;
		}

		r_stats.c_studio_polys += (pMesh->numElems / 3);
		DrawMeshFromBuffer( pMesh );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	GL_AlphaTest( GL_FALSE );
	GL_Cull( GL_FRONT );

	RenderDynLightList ();

	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	pglBindVertexArray( GL_FALSE );

	DrawDecal( RI->currententity );
	m_nNumDrawMeshes = 0;
}

void CStudioModelRenderer :: DrawStudioMeshesShadow( void )
{
	mstudiomaterial_t	*cached_material = NULL;
	model_t		*cached_model = NULL;
	int		i;

	if( !m_nNumDrawMeshes )
		return;

	if( m_pCurrentEntity->modelhandle == INVALID_HANDLE )
		return; // out of memory ?

	m_pModelInstance = &m_ModelInstances[m_pCurrentEntity->modelhandle];

	R_TransformForEntity( m_pModelInstance->m_protationmatrix );
//	R_LoadIdentity();
	GL_Blend( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_TRUE );

	// sorting list to reduce shader switches
	if( !CVAR_TO_BOOL( r_nosort )) QSortStudioMeshes( m_DrawMeshes, 0, m_nNumDrawMeshes - 1 );

	// sorting list to reduce shader switches
	for( i = 0; i < m_nNumDrawMeshes; i++ )
	{
		gl_studiomesh_t *entry = &m_DrawMeshes[i];
		RI->currentmodel = m_pRenderModel = entry->model;
		m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
		int m_skinnum = bound( 0, m_pCurrentEntity->curstate.skin, m_pStudioHeader->numskinfamilies - 1 );
		bool bone_weighting = (m_pRenderModel->poseToBone != NULL) ? true : false;
		vbomesh_t *pMesh = entry->mesh;

		short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

		mstudiomaterial_t *mat = &m_pRenderModel->materials[pskinref[pMesh->skinref]];

		ASSERT( m_pCurrentEntity->modelhandle != INVALID_HANDLE );

		// begin draw the sorted list
		if(( i == 0 ) || ( RI->currentshader != &glsl_programs[entry->hProgram] ))
		{
			GL_BindShader( &glsl_programs[entry->hProgram] );			

			ASSERT( RI->currentshader != NULL );

			// reset cache
			cached_material = NULL;
			cached_model = NULL;
		}

		if( cached_model != m_pRenderModel )
		{
			// update bones array
			int num_bones = Q_min( m_pStudioHeader->numbones, glConfig.max_skinning_bones );

			if( m_pRenderModel == IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel ))
			{
				pglUniform4fvARB( RI->currentshader->u_BoneQuaternion, num_bones, &m_pModelInstance->m_weaponquat[0][0] );
				pglUniform3fvARB( RI->currentshader->u_BonePosition, num_bones, &m_pModelInstance->m_weaponpos[0][0] );
			}
			else
			{
				pglUniform4fvARB( RI->currentshader->u_BoneQuaternion, num_bones, &m_pModelInstance->m_studioquat[0][0] );
				pglUniform3fvARB( RI->currentshader->u_BonePosition, num_bones, &m_pModelInstance->m_studiopos[0][0] );
			}
			cached_model = m_pRenderModel;
		}

		if( cached_material != mat )
		{
			if( FBitSet( mat->flags, STUDIO_NF_MASKED ))
				GL_Bind( GL_TEXTURE0, mat->gl_diffuse_id );
			else GL_Bind( GL_TEXTURE0, tr.whiteTexture );

			if( mat->flags & STUDIO_NF_TWOSIDE )
				GL_Cull( GL_NONE );
			else GL_Cull( GL_FRONT );

			if( FBitSet( mat->flags, STUDIO_NF_MASKED|STUDIO_NF_HAS_ALPHA ))
			{
				pglAlphaFunc( GL_GREATER, 0.5f );
				GL_AlphaTest( GL_TRUE );
			}
			else
			{
				GL_AlphaTest( GL_FALSE );
			}

			cached_material = mat;
		}

		DrawMeshFromBuffer( pMesh );
	}

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	GL_AlphaTest( GL_FALSE );
	GL_Cull( GL_FRONT );

	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	pglBindVertexArray( GL_FALSE );
	m_nNumDrawMeshes = 0;
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
	if( FBitSet( RI->params, RP_THIRDPERSON ) || CL_IsDead() || !UTIL_IsLocal( RI->viewentity ))
		return;

	if( RI->params & RP_NONVIEWERREF )
		return;

	RI->currententity = GET_VIEWMODEL();
	RI->currentmodel = RI->currententity->model;
	if( !RI->currentmodel ) return;

	m_fDrawViewModel = true;

	SET_CURRENT_ENTITY( RI->currententity );
	StudioDrawModel( STUDIO_EVENTS );

	SET_CURRENT_ENTITY( NULL );
	m_fDrawViewModel = false;
	RI->currententity = NULL;
	RI->currentmodel = NULL;
}

/*
=================
DrawViewModel

=================
*/
void CStudioModelRenderer :: DrawViewModel( void )
{
	if( !CVAR_TO_BOOL( m_pCvarDrawViewModel ))
		return;

	// ignore in thirdperson, camera view or client is died
	if( FBitSet( RI->params, RP_THIRDPERSON ) || CL_IsDead() || !UTIL_IsLocal( RI->viewentity ))
		return;

	if( RI->params & RP_NONVIEWERREF )
		return;

	if( !IEngineStudio.Mod_Extradata( GET_VIEWMODEL()->model ))
		return;

	RI->currententity = GET_VIEWMODEL();
	RI->currentmodel = RI->currententity->model;
	if( !RI->currentmodel ) return;

	SET_CURRENT_ENTITY( RI->currententity );

	tr.blend = CL_FxBlend( RI->currententity ) / 255.0f;
	if( !R_ModelOpaque( RI->currententity->curstate.rendermode ) && tr.blend <= 0.0f )
		return; // invisible ?

	// hack the depth range to prevent view model from poking into walls
	pglDepthRange( gldepthmin, gldepthmin + 0.3f * ( gldepthmax - gldepthmin ));

	// backface culling for left-handed weapons
	if( CVAR_TO_BOOL( m_pCvarHand ))
		GL_FrontFace( !glState.frontFace );

	m_fDrawViewModel = true;

	// bound FOV values
	if( m_pCvarViewmodelFov->value < 50 )
		gEngfuncs.Cvar_SetValue( "cl_viewmodel_fov", 50 );
	else if( m_pCvarViewmodelFov->value > 130 )
		gEngfuncs.Cvar_SetValue( "cl_viewmodel_fov", 120 );

	// Find the offset our current FOV is from the default value
	float flFOVOffset = gHUD.default_fov->value - (float)RI->fov_x;

	// Adjust the viewmodel's FOV to move with any FOV offsets on the viewer's end
	m_flViewmodelFov = m_pCvarViewmodelFov->value - flFOVOffset;

	// calc local FOV
	float x = (float)ScreenWidth / tan( m_flViewmodelFov / 360 * M_PI );

	float fov_x = m_flViewmodelFov;
	float fov_y = atan( (float)ScreenHeight / x ) * 360 / M_PI;

	if( fov_x != RI->fov_x )
	{
		matrix4x4	oldProjectionMatrix = RI->projectionMatrix;
		R_SetupProjectionMatrix( fov_x, fov_y, RI->projectionMatrix );

		pglMatrixMode( GL_PROJECTION );
		GL_LoadMatrix( RI->projectionMatrix );

		StudioDrawModel( STUDIO_RENDER );

		// restore original matrix
		RI->projectionMatrix = oldProjectionMatrix;

		pglMatrixMode( GL_PROJECTION );
		GL_LoadMatrix( RI->projectionMatrix );
	}
	else
	{
		StudioDrawModel( STUDIO_RENDER );
	}

	// restore depth range
	pglDepthRange( gldepthmin, gldepthmax );

	// backface culling for left-handed weapons
	if( CVAR_TO_BOOL( m_pCvarHand ))
		GL_FrontFace( !glState.frontFace );

	SET_CURRENT_ENTITY( NULL );
	m_fDrawViewModel = false;
	RI->currententity = NULL;
	RI->currentmodel = NULL;
}

void CStudioModelRenderer :: ClearLightCache( void )
{
	// force to recalc static light again
	for( int i = m_ModelInstances.Count(); --i >= 0; )
          {
		ModelInstance_t *inst = &m_ModelInstances[i];
	}
}
