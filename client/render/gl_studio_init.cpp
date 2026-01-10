/*
gl_studio_init.cpp - loading studio models
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
#include <cmath>
#include <vector>
#include <algorithm>

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

// the renderer object, created on the stack.
CStudioModelRenderer g_StudioRenderer;

//================================================================================================
//			HUD_GetStudioModelInterface
//	Export this function for the engine to use the studio renderer class to render objects.
//================================================================================================
extern "C" int DLLEXPORT HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio )
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

vbomesh_t::~vbomesh_t()
{
	// purge all GPU data
	if( vao ) pglDeleteVertexArrays( 1, &vao );
	if( vbo ) pglDeleteBuffersARB( 1, &vbo );
	if( ibo ) pglDeleteBuffersARB( 1, &ibo );
	tr.total_vbo_memory -= cacheSize;
	cacheSize = 0;
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
	m_pCvarHand		= CVAR_REGISTER( "cl_righthand", "0", FCVAR_ARCHIVE );
	m_pCvarViewmodelFov		= CVAR_REGISTER( "cl_viewmodel_fov", "90", FCVAR_ARCHIVE );
	m_pCvarViewmodelZNear	= CVAR_REGISTER( "cl_viewmodel_znear", "4.0", FCVAR_ARCHIVE );
	m_pCvarCompatible		= CVAR_REGISTER( "r_studio_compatible", "1", FCVAR_ARCHIVE );
	m_pCvarLodScale		= CVAR_REGISTER( "cl_lod_scale", "5.0", FCVAR_ARCHIVE );
	m_pCvarLodBias		= CVAR_REGISTER( "cl_lod_bias", "0", FCVAR_ARCHIVE );
}

/*
====================
Init

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
	m_iDrawModelType	= DRAWSTUDIO_NORMAL;
	m_pCurrentMaterial	= NULL;
	m_pCvarHiModels	= NULL;
	m_pCvarDrawViewModel= NULL;
	m_pCvarHand	= NULL;
	m_pStudioHeader	= NULL;
	m_pVboModel	= NULL;
	m_pSubModel	= NULL;
	m_pModelInstance	= NULL;
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

	RI->currententity = pEnt;
	SET_CURRENT_ENTITY( RI->currententity );

	if( m_iDrawModelType == DRAWSTUDIO_NORMAL && ( RI->currententity->player || RI->currententity->curstate.renderfx == kRenderFxDeadPlayer ))
	{
		int	iPlayerIndex;

		if (RP_NORMALPASS() && RP_LOCALCLIENT(RI->currententity) && !FBitSet(RI->params, RP_THIRDPERSON))
		{
			return false;
		}
		else
		{
			if( RI->currententity->curstate.renderfx == kRenderFxDeadPlayer )
				iPlayerIndex = RI->currententity->curstate.renderamt - 1;
			else iPlayerIndex = RI->currententity->curstate.number - 1;

			if( iPlayerIndex < 0 || iPlayerIndex >= GET_MAX_CLIENTS( ))
				return false;

			RI->currentmodel = IEngineStudio.SetupPlayerModel( iPlayerIndex );

			// show highest resolution multiplayer model
			if( CVAR_TO_BOOL( m_pCvarHiModels ) && RI->currentmodel != RI->currententity->model )
				RI->currententity->curstate.body = 255;

			if( !( !developer_level && GET_MAX_CLIENTS() == 1 ) && ( RI->currentmodel == RI->currententity->model ))
				RI->currententity->curstate.body = 1; // force helmet
		}
	}
	else
	{
		RI->currentmodel = RI->currententity->model;
	}

	if( RI->currentmodel == NULL )
		return false;

	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );

 	// downloading in-progress ?
	if( m_pStudioHeader == NULL )
		return false;

	// tell the engine about model
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( RI->currentmodel );

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
bool CStudioModelRenderer :: StudioSetEntity( CSolidEntry *entry )
{
	studiohdr_t *phdr;

	if( !entry || entry->m_bDrawType != DRAWTYPE_MESH )
		return false;

	if( !entry->m_pParentEntity || !entry->m_pRenderModel )
		return false; // bad entry?

	if( entry->m_pParentEntity->modelhandle == INVALID_HANDLE )
		return false; // not initialized?

	if(( phdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( entry->m_pRenderModel )) == NULL )
		return false; // no model?

	RI->currentmodel = entry->m_pRenderModel;
	RI->currententity = entry->m_pParentEntity;
	m_pModelInstance = &m_ModelInstances[entry->m_pParentEntity->modelhandle];
	m_pStudioHeader = phdr;

	return true;
}

bool CStudioModelRenderer :: StudioSetupInstance( void )
{
	if (RI->currententity->model->type != mod_studio) {
		// hotfix for bug when worldspawn == RI->currententity but why???
		return false; 
	}
	if( RI->currententity->modelhandle == INVALID_HANDLE )
	{
		RI->currententity->modelhandle = m_ModelInstances.AddToTail();

		if( RI->currententity->modelhandle == INVALID_HANDLE )
			return false; // out of memory ?

		m_pModelInstance = &m_ModelInstances[RI->currententity->modelhandle];
		ClearInstanceData( true );
	}
	else
	{
		m_pModelInstance = &m_ModelInstances[RI->currententity->modelhandle];

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

	if( m_iDrawModelType == DRAWSTUDIO_NORMAL && ( inst->m_pEntity->player || RI->currententity->curstate.renderfx == kRenderFxDeadPlayer ))
	{
		if( RI->currententity->curstate.renderfx == kRenderFxDeadPlayer )
			pModel = IEngineStudio.SetupPlayerModel( inst->m_pEntity->curstate.renderamt - 1 );
		else pModel = IEngineStudio.SetupPlayerModel( inst->m_pEntity->curstate.number - 1 );
	}
	else
	{
		pModel = inst->m_pEntity->model;
	}

	return inst->m_pModel == pModel;
}

void CStudioModelRenderer :: DeleteStudioCache( mstudiocache_t **ppstudiocache )
{
	ASSERT(ppstudiocache != nullptr);

	mstudiocache_t *pstudiocache = *ppstudiocache;
	if (!pstudiocache)
		return;

	if (pstudiocache) {
		delete pstudiocache;
	}
	*ppstudiocache = nullptr;
}

void CStudioModelRenderer :: DestroyMeshCache( void )
{
	FreeStudioMaterials ();

	DeleteStudioCache( &RI->currentmodel->studiocache );

	if( RI->currentmodel->poseToBone != NULL )
		Mem_Free( RI->currentmodel->poseToBone );
	RI->currentmodel->poseToBone = NULL;
}

void CStudioModelRenderer :: DestroyInstance( word handle )
{
	if( !m_ModelInstances.IsValidIndex( handle ))
		return;

	ModelInstance_t *inst = &m_ModelInstances[handle];

	PurgeDecals( inst );

	if( inst->materials != NULL )
		Mem_Free( inst->materials );
	inst->materials = NULL;

	if( inst->m_pJiggleBones != NULL )
		delete inst->m_pJiggleBones;
	inst->m_pJiggleBones = NULL;

	m_ModelInstances.Remove( handle );
}

void CStudioModelRenderer :: DestroyAllModelInstances( void )
{
	// if caused by Host_Error during draw the viewmodel or gasmask
	m_iDrawModelType = DRAWSTUDIO_NORMAL;
	m_fShootDecal = false;

	// NOTE: should destroy in reverse-order because it's linked list not array!
	for( int i = m_ModelInstances.Count(); --i >= 0; )
		DestroyInstance( i );
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
	memcpy( m_pModelInstance->materials, RI->currentmodel->materials, sizeof( mstudiomaterial_t ) * m_pStudioHeader->numtextures );

	// invalidate sequences when a new instance was created
	for( int i = 0; i < m_pStudioHeader->numtextures; i++ )
	{
		m_pModelInstance->materials[i].forwardScene.Invalidate();
		m_pModelInstance->materials[i].forwardLightSpot[0].Invalidate();
		m_pModelInstance->materials[i].forwardLightSpot[1].Invalidate();
		m_pModelInstance->materials[i].forwardLightOmni[0].Invalidate();
		m_pModelInstance->materials[i].forwardLightOmni[1].Invalidate();
		m_pModelInstance->materials[i].forwardLightProj.Invalidate();
	}
}

void CStudioModelRenderer :: ClearInstanceData( bool create )
{
	if( create )
	{
		m_pModelInstance->m_DecalList.Purge();
		m_pModelInstance->m_pJiggleBones = NULL;
		m_pModelInstance->materials = NULL;
	}
	else
	{
		if( m_pModelInstance->m_pJiggleBones != NULL )
			delete m_pModelInstance->m_pJiggleBones;
		m_pModelInstance->m_pJiggleBones = NULL;
		PurgeDecals( m_pModelInstance );
	}

	m_pModelInstance->m_pEntity = RI->currententity;
	m_pModelInstance->m_pModel = RI->currentmodel;
	m_pModelInstance->m_VlCache = NULL;
	m_pModelInstance->m_FlCache = NULL;
	m_pModelInstance->m_DecalCount = 0;
	m_pModelInstance->m_bExternalBones = false;
	m_pModelInstance->cached_frame = -1;
	m_pModelInstance->visframe = -1;
	m_pModelInstance->radius = 0.0f;
	m_pModelInstance->info_flags = 0;
	m_pModelInstance->lerpFactor = 0.0f;
	m_pModelInstance->cubemap[0] = &world->defaultCubemap;
	m_pModelInstance->cubemap[1] = &world->defaultCubemap;

	ClearBounds( m_pModelInstance->absmin, m_pModelInstance->absmax );
	memset( &m_pModelInstance->bonecache, 0, sizeof( BoneCache_t ));
	memset( m_pModelInstance->m_protationmatrix, 0, sizeof( matrix3x4 ));
	memset( m_pModelInstance->m_pbones, 0, sizeof( matrix3x4 ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_pwpnbones, 0, sizeof( matrix3x4 ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->attachment, 0, sizeof( StudioAttachment_t ) * MAXSTUDIOATTACHMENTS );
	memset( m_pModelInstance->m_glstudiobones, 0, sizeof( Vector4D ) * MAXSTUDIOBONES * 3 );
	memset( m_pModelInstance->m_glweaponbones, 0, sizeof( Vector4D ) * MAXSTUDIOBONES * 3 );
	memset( m_pModelInstance->m_studioquat, 0, sizeof( Vector4D ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_studiopos, 0, sizeof( Vector ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_weaponquat, 0, sizeof( Vector4D ) * MAXSTUDIOBONES );
	memset( m_pModelInstance->m_weaponpos, 0, sizeof( Vector ) * MAXSTUDIOBONES );
	memset( &m_pModelInstance->lerp, 0, sizeof( mstudiolerp_t ));
	memset( &m_pModelInstance->light, 0, sizeof( mstudiolight_t ));
	memset( &m_pModelInstance->oldlight, 0, sizeof( mstudiolight_t ));
	memset( &m_pModelInstance->newlight, 0, sizeof( mstudiolight_t ));
	memset( &m_pModelInstance->lights, 255, sizeof( byte[MAXDYNLIGHTS] ));
	memset( &m_pModelInstance->m_controller, 0, sizeof( m_pModelInstance->m_controller ));
	memset( &m_pModelInstance->m_seqblend, 0, sizeof( m_pModelInstance->m_seqblend ));
	m_pModelInstance->lerp.stairoldz = StudioGetOrigin().z;
	m_pModelInstance->lerp.stairtime = tr.time;
	m_pModelInstance->m_current_seqblend = 0;
	m_pModelInstance->light_update = false;
	m_pModelInstance->lighttimecheck = 0.0f;

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
	{
		Q_strncpy( att[i].name, pattachment[i].name, sizeof( att[0].name ));
		att[i].local.SetOrigin( pattachment[i].org );

		if( FBitSet( pattachment[i].flags, STUDIO_ATTACHMENT_LOCAL ))
		{
		  att[i].local[0][0] = pattachment->vectors[0][0];
		  att[i].local[0][1] = pattachment->vectors[0][1];
		  att[i].local[0][2] = pattachment->vectors[0][2];
		  att[i].local[1][0] = pattachment->vectors[1][0];
		  att[i].local[1][1] = pattachment->vectors[1][1];
		  att[i].local[1][2] = pattachment->vectors[1][2];
		  att[i].local[2][0] = pattachment->vectors[2][0];
		  att[i].local[2][1] = pattachment->vectors[2][1];
		  att[i].local[2][2] = pattachment->vectors[2][2];
		}
		else {
			att[i].local.Identity();
		}

		if( !Q_strnicmp( att[i].name, "LightProbe.", 11 ))
			SetBits( m_pModelInstance->info_flags, MF_CUSTOM_LIGHTGRID );
	}
	m_pModelInstance->numattachments = m_pStudioHeader->numattachments;

	for( int map = 0; map < MAXLIGHTMAPS; map++ )
		m_pModelInstance->styles[map] = 255;
}

void CStudioModelRenderer :: PrecacheStudioShaders( void )
{
	bool bone_weights = FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS ) != 0;
	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials;

	// this function is called when loading vertexlight cache, so we guessed what vertex light is active
	for( int i = 0; i < m_pStudioHeader->numtextures; i++, pmaterial++ )
	{
		ShaderSceneForward( pmaterial, true, bone_weights, m_pStudioHeader->numbones );
	}
}
#include "material.h"
void CStudioModelRenderer :: LoadStudioMaterials( void )
{
	// first we need alloc copy of all the materials to prevent modify mstudiotexture_t
	RI->currentmodel->materials = (mstudiomaterial_t *)Mem_Alloc( sizeof( mstudiomaterial_t ) * m_pStudioHeader->numtextures );

	bool bone_weights = FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS ) != 0;
	mstudiotexture_t	*ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);
	char		diffuse[128], bumpmap[128], glossmap[128], glowmap[128], heightmap[128];
	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials;
	char		texname[128], matname[64], mdlname[64];

	COM_FileBase( RI->currentmodel->name, mdlname );

	// loading studio materials from studio textures
	for( int i = 0; i < m_pStudioHeader->numtextures; i++, ptexture++, pmaterial++ )
	{
		COM_FileBase( ptexture->name, texname );

		// build material names
		Q_snprintf( matname, sizeof( matname ), "%s/%s", mdlname, texname ); // material description
		// setup material constants
		matdesc_t *desc = CL_FindMaterial( matname );
#if 0
		if( Q_strlen( desc->diffusemap ) && !IMAGE_EXISTS( desc->diffusemap ))
			Msg( "need: %s\n", desc->diffusemap );
#endif
		if( Q_strlen( desc->diffusemap ) && IMAGE_EXISTS( desc->diffusemap ))
			Q_strncpy( diffuse, desc->diffusemap, sizeof( diffuse ));
		else Q_snprintf( diffuse, sizeof( diffuse ), "textures/%s/%s", mdlname, texname );

		if( Q_strlen( desc->normalmap ) && IMAGE_EXISTS( desc->normalmap ))
			Q_strncpy( bumpmap, desc->normalmap, sizeof( bumpmap ));
		else Q_snprintf( bumpmap, sizeof( bumpmap ), "textures/%s/%s_norm", mdlname, texname );

		if( Q_strlen( desc->glossmap ) && IMAGE_EXISTS( desc->glossmap ))
			Q_strncpy( glossmap, desc->glossmap, sizeof( glossmap ));
		else Q_snprintf( glossmap, sizeof( glossmap ), "textures/%s/%s_gloss", mdlname, texname );

		Q_snprintf( glowmap, sizeof( glowmap ), "textures/%s/%s_luma", mdlname, texname );
		Q_snprintf( heightmap, sizeof( heightmap ), "textures/%s/%s_hmap", mdlname, texname );
#if 0
		Msg( "\"%s\"\n", matname );
		Msg( "{\n" );
		Msg( "\t\"diffuseMap\"\t\"%s\"\n", diffuse );
		Msg( "\t\"normalMap\"\t\"%s\"\n", bumpmap );
		Msg( "\t\"glossMap\"\t\"%s\"\n", glossmap );
		Msg( "\t\"material\"\t\"%s\"\n", desc->effects->name );
		Msg( "}\n\n" );
#endif
		pmaterial->pSource = ptexture;
		pmaterial->flags = ptexture->flags;

		if( IMAGE_EXISTS( diffuse ))
		{
			pmaterial->gl_diffuse_id = LOAD_TEXTURE( diffuse, NULL, 0, 0 );

			// semi-transparent textures must have additive flag to invoke renderer insert supposed mesh into translist
			if( FBitSet( pmaterial->flags, STUDIO_NF_ADDITIVE ))
			{
				if( pmaterial->gl_diffuse_id.GetFlags() & TF_HAS_ALPHA)
					SetBits( pmaterial->flags, STUDIO_NF_HAS_ALPHA );
			}

			if( FBitSet( pmaterial->flags, STUDIO_NF_MASKED ))
				SetBits( pmaterial->flags, STUDIO_NF_HAS_ALPHA );
		}

		TextureHandle embeddedTexture = ptexture;
		if (pmaterial->gl_diffuse_id != TextureHandle::Null())
		{
			// so engine can be draw HQ image for gl_renderer 0
			if (embeddedTexture != tr.defaultTexture) {
				FREE_TEXTURE(embeddedTexture);
			}
			ptexture->index = pmaterial->gl_diffuse_id.ToInt();
		}
		else
		{
			// reuse original texture
			pmaterial->gl_diffuse_id = embeddedTexture;
		}

		if( IMAGE_EXISTS( bumpmap ))
		{
			pmaterial->gl_normalmap_id = LOAD_TEXTURE( bumpmap, NULL, 0, TF_NORMALMAP );
			if (pmaterial->gl_normalmap_id.Initialized()) {
				SetBits(pmaterial->flags, STUDIO_NF_NORMALMAP);
			}
		}
		else
		{
			// try alternate suffix
			Q_snprintf( bumpmap, sizeof( bumpmap ), "textures/%s/%s_local", mdlname, texname );
			if( IMAGE_EXISTS( bumpmap ))
				pmaterial->gl_normalmap_id = LOAD_TEXTURE( bumpmap, NULL, 0, TF_NORMALMAP );
			else pmaterial->gl_normalmap_id = tr.normalmapTexture; // blank bumpy
         }

		if( IMAGE_EXISTS( glossmap ))
		{
			pmaterial->gl_specular_id = LOAD_TEXTURE( glossmap, NULL, 0, 0 );
		}
		else
		{
			// try alternate suffix
			Q_snprintf( glossmap, sizeof( glossmap ), "textures/%s/%s_gloss", mdlname, texname );
			if( IMAGE_EXISTS( glossmap ))
				pmaterial->gl_specular_id = LOAD_TEXTURE( glossmap, NULL, 0, 0 );
			else pmaterial->gl_specular_id = tr.blackTexture;
		}

		if( IMAGE_EXISTS( glowmap ))
			pmaterial->gl_glowmap_id = LOAD_TEXTURE( glowmap, NULL, 0, 0 );
		else pmaterial->gl_glowmap_id = tr.blackTexture;

		if( IMAGE_EXISTS( heightmap ))
		{
			pmaterial->gl_heightmap_id = LOAD_TEXTURE( heightmap, NULL, 0, 0 );
		}
		else
		{
			// try alternate suffix
			Q_snprintf( heightmap, sizeof( heightmap ), "textures/%s/%s_bump", mdlname, texname );
			if( IMAGE_EXISTS( heightmap ))
				pmaterial->gl_heightmap_id = LOAD_TEXTURE( heightmap, NULL, 0, 0 );
			else pmaterial->gl_heightmap_id = tr.blackTexture;
		}

		// detail map loading
		if (IMAGE_EXISTS(desc->detailmap)) {
			pmaterial->gl_detailmap_id = LOAD_TEXTURE(desc->detailmap, NULL, 0, TF_FORCE_COLOR);
		}
		else {
			pmaterial->gl_detailmap_id = tr.grayTexture;
		}

		// current model has bumpmapping effect
		if( pmaterial->gl_normalmap_id.Initialized() && pmaterial->gl_normalmap_id != tr.normalmapTexture)
			SetBits( m_pStudioHeader->flags, STUDIO_HAS_BUMP );

		if( pmaterial->gl_specular_id != tr.blackTexture )
			SetBits( pmaterial->flags, STUDIO_NF_GLOSSMAP );

		if( pmaterial->gl_glowmap_id != tr.blackTexture )
			SetBits( pmaterial->flags, STUDIO_NF_LUMA );

		if( pmaterial->gl_heightmap_id != tr.blackTexture )
			SetBits( pmaterial->flags, STUDIO_NF_HEIGHTMAP );

		pmaterial->smoothness = desc->smoothness;
		pmaterial->detailScale[0] = desc->detailScale[0];
		pmaterial->detailScale[1] = desc->detailScale[1];
		pmaterial->reflectScale = desc->reflectScale;
		pmaterial->refractScale = desc->refractScale;
		pmaterial->aberrationScale = desc->aberrationScale;
		pmaterial->reliefScale = desc->reliefScale;
		pmaterial->effects = desc->effects;
		pmaterial->swayHeight = desc->swayHeight;

		if( pmaterial->gl_detailmap_id.Initialized() && pmaterial->gl_detailmap_id != tr.grayTexture)
			SetBits( pmaterial->flags, STUDIO_NF_HAS_DETAIL );

		// time to precache shaders
		ShaderSceneForward( pmaterial, false, bone_weights, m_pStudioHeader->numbones );
		ShaderLightForward( &tr.defaultlightSpot, pmaterial, bone_weights, m_pStudioHeader->numbones );
		ShaderLightForward( &tr.defaultlightOmni, pmaterial, bone_weights, m_pStudioHeader->numbones );
		ShaderLightForward( &tr.defaultlightProj, pmaterial, bone_weights, m_pStudioHeader->numbones );
		pmaterial->forwardScene.Invalidate(); // don't keep shadernum
	}
}

void CStudioModelRenderer :: FreeStudioMaterials( void )
{
	if( !RI->currentmodel->materials ) return;

	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials;

	// release textures for current model
	for( int i = 0; i < m_pStudioHeader->numtextures; i++, pmaterial++ )
	{
		if( TextureHandle(pmaterial->pSource) != pmaterial->gl_diffuse_id )
			FREE_TEXTURE( pmaterial->gl_diffuse_id );

		if( pmaterial->gl_normalmap_id != tr.normalmapTexture )
			FREE_TEXTURE( pmaterial->gl_normalmap_id );

		if( pmaterial->gl_specular_id != tr.blackTexture )
			FREE_TEXTURE( pmaterial->gl_specular_id );

		if( pmaterial->gl_glowmap_id != tr.blackTexture )
			FREE_TEXTURE( pmaterial->gl_glowmap_id );
	}

	Mem_Free( RI->currentmodel->materials );
	RI->currentmodel->materials = NULL;
}

void CStudioModelRenderer :: LoadLocalMatrix( int bone, mstudioboneinfo_t *boneinfo )
{
	mposetobone_t *m = RI->currentmodel->poseToBone;

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

void CStudioModelRenderer :: ComputeSkinMatrix( svert_t *vertex, const matrix3x4 worldtransform[], matrix3x4 &result )
{
	float	flWeight0, flWeight1, flWeight2, flWeight3;
	int	numbones = 0;
	float	flTotal;

	for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
	{
		if( vertex->boneid[i] != -1 )
			numbones++;
	}

	if( numbones == 4 )
	{
		const matrix3x4 &boneMat0 = worldtransform[vertex->boneid[0]];
		const matrix3x4 &boneMat1 = worldtransform[vertex->boneid[1]];
		const matrix3x4 &boneMat2 = worldtransform[vertex->boneid[2]];
		const matrix3x4 &boneMat3 = worldtransform[vertex->boneid[3]];
		flWeight0 = vertex->weight[0] / 255.0f;
		flWeight1 = vertex->weight[1] / 255.0f;
		flWeight2 = vertex->weight[2] / 255.0f;
		flWeight3 = vertex->weight[3] / 255.0f;
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
		const matrix3x4 &boneMat0 = worldtransform[vertex->boneid[0]];
		const matrix3x4 &boneMat1 = worldtransform[vertex->boneid[1]];
		const matrix3x4 &boneMat2 = worldtransform[vertex->boneid[2]];
		flWeight0 = vertex->weight[0] / 255.0f;
		flWeight1 = vertex->weight[1] / 255.0f;
		flWeight2 = vertex->weight[2] / 255.0f;
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
		const matrix3x4 &boneMat0 = worldtransform[vertex->boneid[0]];
		const matrix3x4 &boneMat1 = worldtransform[vertex->boneid[1]];
		flWeight0 = vertex->weight[0] / 255.0f;
		flWeight1 = vertex->weight[1] / 255.0f;
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
		result = worldtransform[vertex->boneid[0]];
	}
}

bool CStudioModelRenderer :: StudioSaveTBN( void )
{
	char szFilename[MAX_PATH];
	char szModelname[MAX_PATH];

	Q_strncpy( szModelname, RI->currentmodel->name + Q_strlen( "models/" ), sizeof( szModelname ));
	COM_StripExtension( szModelname );
	Q_snprintf( szFilename, sizeof( szFilename ), "cache/%s.tbn", szModelname );
	size_t tbn_size = sizeof( dmodeltbn_t ) + (m_tbnverts->numverts - 1) * sizeof( dvertmatrix_t );

	if( SAVE_FILE( szFilename, m_tbnverts, tbn_size ))
		return true;

	ALERT( at_error, "StudioSaveCache: couldn't store %s\n", szFilename );
	return false;
}

bool CStudioModelRenderer :: StudioLoadTBN( void )
{
	char	szFilename[MAX_PATH];
	char	szModelname[MAX_PATH];
	int	length, iCompare;

	Q_strncpy( szModelname, RI->currentmodel->name + Q_strlen( "models/" ), sizeof( szModelname ));
	COM_StripExtension( szModelname );
	Q_snprintf( szFilename, sizeof( szFilename ), "cache/%s.tbn", szModelname );

	if( COMPARE_FILE_TIME( RI->currentmodel->name, szFilename, &iCompare ))
	{
		// MDL file is newer.
		if( iCompare > 0 )
			return false;
	}
	else
	{
		return false;
	}

	byte *aMemFile = LOAD_FILE( szFilename, &length );
	if( !aMemFile ) return false;

	// TBN is a footprint
	m_tbnverts = (dmodeltbn_t *)aMemFile;

	if( m_tbnverts->ident != IDTBNHEADER )
	{
		ALERT( at_warning, "%s has wrong id (%x should be %x)\n", szFilename, m_tbnverts->ident, IDTBNHEADER );
		FREE_FILE( aMemFile );
		m_tbnverts = NULL;
		return false;
	}

	if( m_tbnverts->version != TBN_VERSION )
	{
		ALERT( at_warning, "%s has wrong version (%i should be %i)\n", szFilename, m_tbnverts->version, TBN_VERSION );
		FREE_FILE( aMemFile );
		m_tbnverts = NULL;
		return false;
	}

	if( m_tbnverts->modelCRC != RI->currentmodel->modelCRC )
	{
		ALERT( at_console, "%s was changed, TBN cache will be updated\n", szFilename );
		FREE_FILE( aMemFile );
		m_tbnverts = NULL;
		return false;
	}

	// all done, we can load TBN from disk
	m_iTBNState = TBNSTATE_LOADING;
	return true;
}

bool CStudioModelRenderer :: CalcLightmapAxis( mstudiosurface_t *surf, const dfacelight_t *fl, const dmodelfacelight_t *dfl )
{
	int	ssize = dfl->texture_step;
	Vector	mins, maxs, size, t1, t2;
	Vector	planeNormal;
	Vector	lmvecs[2];
	int	i, axis;

	ClearBounds( mins, maxs );
	for( i = 0; i < 3; i++ )
		AddPointToBounds( m_arrayverts[m_nNumTempVerts+i], mins, maxs );

	// compute triangle normal
	t1 = m_arrayverts[m_nNumTempVerts+0] - m_arrayverts[m_nNumTempVerts+1];
	t2 = m_arrayverts[m_nNumTempVerts+2] - m_arrayverts[m_nNumTempVerts+1];
	planeNormal = CrossProduct( t1, t2 );
	planeNormal = planeNormal.Normalize();

	// round to the lightmap resolution
	for( i = 0; i < 3; i++ )
	{
		mins[i] = ssize * floor( mins[i] / ssize );
		maxs[i] = ssize * ceil( maxs[i] / ssize );
		size[i] = (maxs[i] - mins[i]) / ssize;
	}

	// the two largest axis will be the lightmap size
	planeNormal.x = fabs( planeNormal.x );
	planeNormal.y = fabs( planeNormal.y );
	planeNormal.z = fabs( planeNormal.z );
	lmvecs[0] = lmvecs[1] = g_vecZero;

	if( planeNormal.x >= planeNormal.y && planeNormal.x >= planeNormal.z )
	{
		surf->lightextents[0] = size[1];
		surf->lightextents[1] = size[2];
		lmvecs[0][1] = 1.0 / ssize;
		lmvecs[1][2] = 1.0 / ssize;
		axis = 0;
	}
	else if( planeNormal.y >= planeNormal.x && planeNormal.y >= planeNormal.z )
	{
		surf->lightextents[0] = size[0];
		surf->lightextents[1] = size[2];
		lmvecs[0][0] = 1.0 / ssize;
		lmvecs[1][2] = 1.0 / ssize;
		axis = 1;
	}
	else
	{
		surf->lightextents[0] = size[0];
		surf->lightextents[1] = size[1];
		lmvecs[0][0] = 1.0 / ssize;
		lmvecs[1][1] = 1.0 / ssize;
		axis = 2;
	}

	// bad normal?
	if( !planeNormal[axis] )
	{
		// set lightmap number to avoid assertation
		surf->lightmaptexturenum = tr.current_lightmap_texture;
		return true;
	}

	if( surf->lightextents[0] > ( MAX_STUDIO_LIGHTMAP_SIZE - 1 ))
	{
		lmvecs[0] *= (float)(MAX_STUDIO_LIGHTMAP_SIZE - 1.0f) / surf->lightextents[0];
		surf->lightextents[0] = MAX_STUDIO_LIGHTMAP_SIZE - 1;
	}
	
	if( surf->lightextents[1] > ( MAX_STUDIO_LIGHTMAP_SIZE - 1 ))
	{
		lmvecs[1] *= (float)(MAX_STUDIO_LIGHTMAP_SIZE - 1.0f) / surf->lightextents[1];
		surf->lightextents[1] = MAX_STUDIO_LIGHTMAP_SIZE - 1;
	}

	memcpy( surf->styles, fl->styles, sizeof( surf->styles ));
	surf->texture_step = ssize;

	if( fl->lightofs != -1 )
	{
		if( worldmodel->lightdata != NULL )
			surf->samples = worldmodel->lightdata + (fl->lightofs/3);
		if( world->deluxedata != NULL )
			surf->normals = world->deluxedata + (fl->lightofs/3);
		if( world->shadowdata != NULL )
			surf->shadows = world->shadowdata + (fl->lightofs/3);
	}

	// calculate the world coordinates of the lightmap samples
	if( !GL_AllocLightmapForFace( surf ))
		return false; // page is full

	for( i = 0; i < 3; i++ )
	{
		svert_t *v = &m_arrayxvert[m_nNumTempVerts+i];
		Vector point = m_arrayverts[m_nNumTempVerts+i] - mins;
		R_LightmapCoords( surf, point, lmvecs, v->lmcoord0, 0 ); // styles 0-1
		R_LightmapCoords( surf, point, lmvecs, v->lmcoord1, 2 ); // styles 2-3
	}

	return true;
}

void CStudioModelRenderer :: AllocLightmapsForMesh( StudioMesh_t *pCurMesh, const dmodelfacelight_t *dfl )
{
	int	start_light_faces = m_nNumLightFaces;
	int	start_temp_verts = m_nNumTempVerts;
	bool	lightmap_restarted = false;

	// should have the surface cache to store lightmap results
	if( !m_pStudioCache || m_pStudioCache->surfaces.empty() )
		return;

	if( !dfl || dfl->numfaces <= 0 )
		return;
lm_restart:
	pCurMesh->lightmapnum = tr.current_lightmap_texture;
	m_nNumLightFaces = start_light_faces;
	m_nNumTempVerts = start_temp_verts;

	// now vertexes are stored into intermediate array and we can using it to compute lightmatix
	for( int i = 0; i < (pCurMesh->numindices / 3); i++ )
	{
		const dfacelight_t *fl = &dfl->faces[m_nNumLightFaces];
		mstudiosurface_t *surf = &m_pStudioCache->surfaces[m_nNumLightFaces];

		if( !CalcLightmapAxis( surf, fl, dfl ))
		{
			if( !lightmap_restarted )
			{
				lightmap_restarted = true;
				goto lm_restart;
			}
			Msg( "%s: mesh %d failed to allocate lightmap\n", RI->currentmodel->name, pCurMesh - m_pTempMesh );
		}

		ASSERT( surf->lightmaptexturenum == pCurMesh->lightmapnum );
//		Msg( "face %d, lightofs %d, extents %d %d\n", m_nNumLightFaces, fl->lightofs, surf->lightextents[0], surf->lightextents[1] );
		ASSERT( m_nNumTempVerts < MAXARRAYVERTS );

		m_nNumTempVerts += 3;
		m_nNumLightFaces++;
	}
}

void CStudioModelRenderer :: SetupSubmodelVerts( const mstudiomodel_t *pSubModel, const matrix3x4 bones[], void *srclight, int lightmode )
{
	short		*pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex); // setup skinref for skin == 0
	mstudiotexture_t	*ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);
	mstudioboneweight_t	*pvertweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + pSubModel->blendvertinfoindex);
	mstudioboneweight_t	*pnormweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + pSubModel->blendnorminfoindex);
	mstudiobone_t	*pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	Vector		*pstudioverts = (Vector *)((byte *)m_pStudioHeader + pSubModel->vertindex);
	Vector		*pstudionorms = (Vector *)((byte *)m_pStudioHeader + pSubModel->normindex);
	bool		has_vertlight = ( lightmode == LIGHTSTATIC_VERTEX ) ? true : false;
	bool		has_surflight = ( lightmode == LIGHTSTATIC_SURFACE ) ? true : false;
	bool		has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	byte		*pvertbone = ((byte *)m_pStudioHeader + pSubModel->vertinfoindex);
	byte		*pnormbone = ((byte *)m_pStudioHeader + pSubModel->norminfoindex);
	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials;
	std::vector<Vector> localverts;
	bool		use_fan_sequence = false;
	bool		has_invalid_normals = false;
	dmodelvertlight_t	*dvl = NULL;
	dmodelfacelight_t	*dfl = NULL;
	int		i, count;
	matrix3x4 skinMat;

	switch( lightmode )
	{
	case LIGHTSTATIC_VERTEX:
		dvl = (dmodelvertlight_t *)srclight;
		break;
	case LIGHTSTATIC_SURFACE:
		dfl = (dmodelfacelight_t *)srclight;
		use_fan_sequence = true;
		break;
	}

	localverts.resize(pSubModel->numverts);

	if( m_iTBNState == TBNSTATE_GENERATE || use_fan_sequence )
	{
		// we need to build TBN in refrence pose to avoid seams
		if (has_boneweights)
		{
			// compute weighted vertexes
			for( int i = 0; i < pSubModel->numverts; i++ )
			{
				ComputeSkinMatrix( &pvertweight[i], bones, skinMat );
				localverts[i] = skinMat.VectorTransform( pstudioverts[i] );
			}
		}
		else
		{
			// compute unweighted vertexes
			for( int i = 0; i < pSubModel->numverts; i++ )
				localverts[i] = bones[pvertbone[i]].VectorTransform( pstudioverts[i] );
		}
	}

	memset( m_pTempMesh, 0 , sizeof( m_pTempMesh ));
	m_nNumArrayVerts = m_nNumArrayElems = 0;
	m_nNumTempVerts = 0;

	// build all the data for current submodel
	for( i = 0; i < pSubModel->nummesh; i++ ) 
	{
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + pSubModel->meshindex) + i;
		mstudiomaterial_t *pmaterial = &RI->currentmodel->materials[pskinref[pmesh->skinref]];
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
		StudioMesh_t *pCurMesh = &m_pTempMesh[i];

		// fill temp mesh info
		pCurMesh->firstvertex = m_nNumArrayVerts;
		pCurMesh->firstindex = m_nNumArrayElems;
		pCurMesh->lightmapnum = -1;
		pCurMesh->numvertices = 0;
		pCurMesh->numindices = 0;

		mstudiotexture_t *ptexture = pmaterial->pSource;
		float s = 1.0f / (float)ptexture->width;
		float t = 1.0f / (float)ptexture->height;

		// first create trifan array from studiomodel mesh
		while(( count = *( ptricmds++ )))
		{
			bool	strip = ( count < 0 ) ? false : true;
			int	vertexState = 0;

			if( count < 0 ) count = -count;

			for( ; count > 0; count--, ptricmds += 4 )
			{
				ASSERT( m_nNumArrayVerts < MAXARRAYVERTS );
				ASSERT( m_nNumArrayElems < MAXARRAYVERTS * 3 );

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
				m_arrayxvert[m_nNumArrayVerts].vertex = pstudioverts[ptricmds[0]];
				m_arrayxvert[m_nNumArrayVerts].normal = pstudionorms[ptricmds[1]];

				// warn about invalid normals, because null normals causes NaNs in vertex shader after normalizing.
				// but since a lot of legacy content has null normals, we need to replace them with something that won't cause NaNs
				if (std::abs(m_arrayxvert[m_nNumArrayVerts].normal.Length() - 1.0f) >= 0.1f) 
				{
					if (!has_invalid_normals)
					{
						ALERT( at_warning, "Invalid normals found in submodel \"%s\" of \"%s\", this may cause visual artifacts.\n", pSubModel->name, RI->currentmodel->name ); 
						has_invalid_normals = true;
					}
					m_arrayxvert[m_nNumArrayVerts].normal = Vector(0.0f, 0.0f, 1.0f);
				}

				if( m_iTBNState == TBNSTATE_GENERATE || use_fan_sequence )
				{
					// transformed vertices to build TBN
					m_arrayverts[m_nNumArrayVerts] = localverts[ptricmds[0]];
				}

				if( m_iTBNState == TBNSTATE_LOADING && m_tbnverts != NULL )
				{
					// loading TBN from cache
					m_arrayxvert[m_nNumArrayVerts].tangent = m_tbnverts->verts[m_nNumTBNVerts].tangent;
					m_arrayxvert[m_nNumArrayVerts].binormal = m_tbnverts->verts[m_nNumTBNVerts].binormal;
					m_arrayxvert[m_nNumArrayVerts].normal = m_tbnverts->verts[m_nNumTBNVerts].normal;
					m_nNumTBNVerts++;
				}

				if( dvl != NULL && dvl->numverts > 0 )
				{
					dvertlight_t *vl = &dvl->verts[m_nNumLightVerts++];

					// now setup light and deluxe vector
					for( int map = 0; map < MAXLIGHTMAPS; map++ )
					{
						m_arrayxvert[m_nNumArrayVerts].light[map] = PackColor( vl->light[map] );
						m_arrayxvert[m_nNumArrayVerts].deluxe[map] = PackColor( vl->deluxe[map] );
					}
				}

				if( FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
				{
					// probably always equal 64 (see studiomdl.c for details)
					m_arrayxvert[m_nNumArrayVerts].stcoord[0] = s;
					m_arrayxvert[m_nNumArrayVerts].stcoord[1] = t;
				}
				else if( FBitSet( ptexture->flags, STUDIO_NF_UV_COORDS ))
				{
					m_arrayxvert[m_nNumArrayVerts].stcoord[0] = HalfToFloat( ptricmds[2] );
					m_arrayxvert[m_nNumArrayVerts].stcoord[1] = HalfToFloat( ptricmds[3] );
				}
				else
				{
					m_arrayxvert[m_nNumArrayVerts].stcoord[0] = ptricmds[2] * s;
					m_arrayxvert[m_nNumArrayVerts].stcoord[1] = ptricmds[3] * t;
				}

				m_arrayxvert[m_nNumArrayVerts].lmcoord0[0] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord0[1] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord0[2] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord0[3] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord1[0] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord1[1] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord1[2] = 0.0f;
				m_arrayxvert[m_nNumArrayVerts].lmcoord1[3] = 0.0f;

				if (has_boneweights)
				{
					mstudioboneweight_t	*pCurWeight = &pvertweight[ptricmds[0]];

					m_arrayxvert[m_nNumArrayVerts].boneid[0] = pCurWeight->bone[0];
					m_arrayxvert[m_nNumArrayVerts].boneid[1] = pCurWeight->bone[1];
					m_arrayxvert[m_nNumArrayVerts].boneid[2] = pCurWeight->bone[2];
					m_arrayxvert[m_nNumArrayVerts].boneid[3] = pCurWeight->bone[3];
					m_arrayxvert[m_nNumArrayVerts].weight[0] = pCurWeight->weight[0];
					m_arrayxvert[m_nNumArrayVerts].weight[1] = pCurWeight->weight[1];
					m_arrayxvert[m_nNumArrayVerts].weight[2] = pCurWeight->weight[2];
					m_arrayxvert[m_nNumArrayVerts].weight[3] = pCurWeight->weight[3];
				}
				else
				{
					m_arrayxvert[m_nNumArrayVerts].boneid[0] = pvertbone[ptricmds[0]];
					m_arrayxvert[m_nNumArrayVerts].boneid[1] = -1;
					m_arrayxvert[m_nNumArrayVerts].boneid[2] = -1;
					m_arrayxvert[m_nNumArrayVerts].boneid[3] = -1;
					m_arrayxvert[m_nNumArrayVerts].weight[0] = 255;
					m_arrayxvert[m_nNumArrayVerts].weight[1] = 0;
					m_arrayxvert[m_nNumArrayVerts].weight[2] = 0;
					m_arrayxvert[m_nNumArrayVerts].weight[3] = 0;
				}

				m_nNumArrayVerts++;
			}
		}

		// store counts
		pCurMesh->numvertices = m_nNumArrayVerts - pCurMesh->firstvertex;
		pCurMesh->numindices = m_nNumArrayElems - pCurMesh->firstindex;
	}
#if 0
	if( lightmode == LIGHTSTATIC_SURFACE )
	{
		Vector	*normals = (Vector *)Mem_Alloc( m_nNumArrayVerts * sizeof( Vector ));

		for( int hashSize = 1; hashSize < m_nNumArrayVerts; hashSize <<= 1 );
		hashSize = hashSize >> 2;

		// build a map from vertex to a list of triangles that share the vert.
		CUtlArray<CIntVector> vertHashMap;

		vertHashMap.AddMultipleToTail( hashSize );

		for( int vertID = 0; vertID < m_nNumArrayVerts; vertID++ )
		{
			svert_t *v = &m_arrayxvert[vertID];
			uint hash = VertexHashKey( v->vertex, hashSize );
			vertHashMap[hash].AddToTail( vertID );
		}

		for( int hashID = 0; hashID < hashSize; hashID++ )
		{
			for( int i = 0; i < vertHashMap[hashID].Size(); i++ )
			{
				int vertID = vertHashMap[hashID][i];
				svert_t *v0 = &m_arrayxvert[vertID];

				for( int j = 0; j < vertHashMap[hashID].Size(); j++ )
				{
					svert_t *v1 = &m_arrayxvert[vertHashMap[hashID][j]];

					if( !VectorCompareEpsilon( v0->vertex, v1->vertex, ON_EPSILON ))
						continue;

					if( DotProduct( v0->normal, v1->normal ) >= tr.smoothing_threshold )
						VectorAdd( normals[vertID], v1->normal, normals[vertID] );
				}
			}
		}

		// copy smoothed normals back
		for( i = 0; i < m_nNumArrayVerts; i++ )
			m_arrayxvert[i].normal = normals[i].Normalize();
		Mem_Free( normals );
	}
#endif
	if( use_fan_sequence )
	{
		CUtlArray<svert_t>	tempxvert;
		CUtlArray<Vector>	tempvert;

		// convert strips to fan sequences (that never exceeds MAXARRAYVERTS)
		for( i = 0; i < m_nNumArrayElems; i++ )
		{
			tempxvert.AddToTail( m_arrayxvert[m_arrayelems[i]] );
			tempvert.AddToTail( m_arrayverts[m_arrayelems[i]] );
		}

		// also update mesh data
		for( i = 0; i < pSubModel->nummesh; i++ ) 
		{
			StudioMesh_t *pCurMesh = &m_pTempMesh[i];
			pCurMesh->firstvertex = pCurMesh->firstindex;
			pCurMesh->numvertices = pCurMesh->numindices;
		}

		// convert indexes
		for( i = 0; i < m_nNumArrayElems; i++ )
			m_arrayelems[i] = i;

		ASSERT( tempxvert.Count() < MAXARRAYVERTS );
		ASSERT( tempvert.Count() < MAXARRAYVERTS );

		// copy unstripified vertexes back
		memcpy( m_arrayxvert, tempxvert.Base(), tempxvert.Count() * sizeof( svert_t ));
		memcpy( m_arrayverts, tempvert.Base(), tempvert.Count() * sizeof( Vector ));
		m_nNumArrayVerts = tempxvert.Count();
	}

	if( dfl != NULL )
	{
		// build all the data for current submodel
		for( i = 0; i < pSubModel->nummesh; i++ ) 
		{
			StudioMesh_t *pCurMesh = &m_pTempMesh[i];
			// allocate lightmaps for static meshes
			AllocLightmapsForMesh( pCurMesh, dfl );
		}
	}

	// compute tangent space for all submodel meshes to avoid seams
	if( m_iTBNState == TBNSTATE_GENERATE )
	{
		for (int vertID = 0; vertID < m_nNumArrayVerts; vertID++) {
			m_arrayxvert[vertID].tangent = m_arrayxvert[vertID].binormal = g_vecZero;
		}

		float *v[3], *tc[3];
		Vector triSVect, triTVect;
		for (int triID = 0; triID < (m_nNumArrayElems / 3); triID++)
		{
			for (int i = 0; i < 3; i++)
			{
				v[i] = (float *)&m_arrayverts[m_arrayelems[triID * 3 + i]]; // transformed to global pose to avoid seams
				tc[i] = (float *)&m_arrayxvert[m_arrayelems[triID * 3 + i]].stcoord;
			}

			CalcTBN(v[0], v[1], v[2], tc[0], tc[1], tc[2], triSVect, triTVect);

			for (int i = 0; i < 3; i++)
			{
				m_arrayxvert[m_arrayelems[triID * 3 + i]].tangent += triSVect;
				m_arrayxvert[m_arrayelems[triID * 3 + i]].binormal += triTVect;
			}
		}

		std::vector<int> sort_IDs;
		sort_IDs.reserve(m_nNumArrayVerts);
		for (int i = 0; i < m_nNumArrayVerts; i++) {
			sort_IDs.push_back(i);
		}

		auto compareVertsFunc = [&](int a, int b) {
			if (m_arrayverts[a].x > m_arrayverts[b].x) return  1;
			if (m_arrayverts[a].x < m_arrayverts[b].x) return -1;
			if (m_arrayverts[a].y > m_arrayverts[b].y) return  1;
			if (m_arrayverts[a].y < m_arrayverts[b].y) return -1;
			if (m_arrayverts[a].z > m_arrayverts[b].z) return  1;
			if (m_arrayverts[a].z < m_arrayverts[b].z) return -1;

			if (m_arrayxvert[a].normal.x > m_arrayxvert[b].normal.x) return  1;
			if (m_arrayxvert[a].normal.x < m_arrayxvert[b].normal.x) return -1;
			if (m_arrayxvert[a].normal.y > m_arrayxvert[b].normal.y) return  1;
			if (m_arrayxvert[a].normal.y < m_arrayxvert[b].normal.y) return -1;
			if (m_arrayxvert[a].normal.z > m_arrayxvert[b].normal.z) return  1;
			if (m_arrayxvert[a].normal.z < m_arrayxvert[b].normal.z) return -1;

			if (m_arrayxvert[a].stcoord[0] > m_arrayxvert[b].stcoord[0]) return  1;
			if (m_arrayxvert[a].stcoord[0] < m_arrayxvert[b].stcoord[0]) return -1;
			if (m_arrayxvert[a].stcoord[1] > m_arrayxvert[b].stcoord[1]) return  1;
			if (m_arrayxvert[a].stcoord[1] < m_arrayxvert[b].stcoord[1]) return -1;

			return 0;
		};
		std::sort(sort_IDs.begin(), sort_IDs.end(), [&](const int &a, const int &b){
			return compareVertsFunc(a, b) < 0;
		});

		int dups = 0;
		for (i = 1; i < m_nNumArrayVerts; i++)
		{
			if (compareVertsFunc(sort_IDs[i], sort_IDs[i - 1]) == 0)
			{
				dups++;
				m_arrayxvert[sort_IDs[i]].tangent += m_arrayxvert[sort_IDs[i - 1]].tangent;
				m_arrayxvert[sort_IDs[i]].binormal += m_arrayxvert[sort_IDs[i - 1]].binormal;
			}
			else
			{
				if (dups > 0)
				{
					for (int j = i - dups - 1; j < i - 1; j++)
					{
						m_arrayxvert[sort_IDs[j]].tangent = m_arrayxvert[sort_IDs[i - 1]].tangent;
						m_arrayxvert[sort_IDs[j]].binormal = m_arrayxvert[sort_IDs[i - 1]].binormal;
					}
					dups = 0;
				}
			}
		}
		if (dups > 0)
		{
			for (int j = i - dups - 1; j < i - 1; j++)
			{
				m_arrayxvert[sort_IDs[j]].tangent = m_arrayxvert[sort_IDs[i - 1]].tangent;
				m_arrayxvert[sort_IDs[j]].binormal = m_arrayxvert[sort_IDs[i - 1]].binormal;
			}
		}

		// calculate an average tangent space for each vertex
		// optimized implementation by ncuxonaT
		for (int vertID = 0; vertID < m_nNumArrayVerts; vertID++)
		{
			Vector tangent = m_arrayxvert[vertID].tangent.Normalize();
			Vector binormal = m_arrayxvert[vertID].binormal.Normalize();
			Vector normal = m_arrayxvert[vertID].normal;

			ComputeSkinMatrix(&m_arrayxvert[vertID], bones, skinMat);
			tangent = skinMat.VectorIRotate(tangent);
			binormal = skinMat.VectorIRotate(binormal);

			// orthogonalization		
			tangent = tangent - normal * DotProduct(normal, tangent);
			binormal = CrossProduct(normal, tangent) * Q_sign(DotProduct(CrossProduct(normal, tangent), binormal));

			m_arrayxvert[vertID].tangent = tangent.Normalize();
			m_arrayxvert[vertID].binormal = binormal.Normalize();
		}

		// search for submodel offset
		int	offset = (byte *)pSubModel - (byte *)m_pStudioHeader;
		int j;

		for( j = 0; j < MAXSTUDIOMODELS; j++ )
		{
			if( m_tbnverts->submodels[j].submodel_offset == offset )
				break;			
		}

		// store vertex offset for bounds checking
		m_tbnverts->submodels[j].vertex_offset = m_nNumTBNVerts;

		// store precomputed TBN into our cache
		for( int i = 0; i < m_nNumArrayVerts; i++ )
		{
			m_tbnverts->verts[m_nNumTBNVerts].tangent = m_arrayxvert[i].tangent;
			m_tbnverts->verts[m_nNumTBNVerts].binormal = m_arrayxvert[i].binormal;
			m_tbnverts->verts[m_nNumTBNVerts].normal = m_arrayxvert[i].normal;
			m_nNumTBNVerts++;
		} 
	}
}

void CStudioModelRenderer :: MeshCreateBuffer( vbomesh_t *pOut, const mstudiomesh_t *pMesh, const StudioMesh_t *pMeshInfo, int lightmode )
{
	// FIXME: if various skinfamilies has different sizes then our texcoords probably will be invalid for pev->skin != 0
	short		*pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex); // setup skinref for skin == 0
	mstudiotexture_t	*ptexture = (mstudiotexture_t *)((byte *)m_pStudioHeader + m_pStudioHeader->textureindex);
	mstudiomaterial_t	*pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials + pskinref[pMesh->skinref];
	mstudiobone_t	*pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	bool		has_bumpmap = FBitSet( pmaterial->flags, STUDIO_NF_NORMALMAP ) ? true : false;
	bool 		has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	bool		has_vertexlight = ( lightmode == LIGHTSTATIC_VERTEX ) ? true : false;
	bool		has_lightmap = ( lightmode == LIGHTSTATIC_SURFACE ) ? true : false;
	const mposetobone_t	*m = RI->currentmodel->poseToBone;
	std::vector<uint32_t> arrayelems;

	pOut->skinref = pMesh->skinref;
	pOut->parentbone = 0xFF;

	// we need to compute some things individually per mesh
	for (int i = 0; i < pMeshInfo->numvertices; i++)
	{
		svert_t	*vert = &m_arrayxvert[pMeshInfo->firstvertex + i];
		int boneid = vert->boneid[0];

		if (pOut->parentbone == 0xFF)
			pOut->parentbone = boneid;

		// update bone if it was parent of current bone
		if (pOut->parentbone != boneid)
		{
			for (int j = pOut->parentbone; j != -1; j = pbones[j].parent)
			{
				if (j == boneid)
				{
					pOut->parentbone = boneid;
					break;
				}
			}
		}

		for (int j = 0; j < 4; j++) 
		{
			int32_t boneIndex = vert->boneid[j];
			if (boneIndex < 0) {
				continue;
			}

			vec3_t tempVert = vert->vertex;
			if (has_boneweights) {
				tempVert = m->posetobone[boneIndex].VectorTransform(tempVert);
			}
			pOut->boneBounds[boneIndex].ExpandToPoint(tempVert);
		}
	}

	// remap indices to local range
	arrayelems.resize(pMeshInfo->numindices);
	for (int i = 0; i < pMeshInfo->numindices; i++)
		arrayelems[i] = m_arrayelems[pMeshInfo->firstindex + i] - pMeshInfo->firstvertex;

	pOut->lightmapnum = pMeshInfo->lightmapnum;
	pOut->numVerts = pMeshInfo->numvertices;
	pOut->numElems = pMeshInfo->numindices;

	GL_CheckVertexArrayBinding();

	// determine optimal mesh loader
	uint attribs = ComputeAttribFlags( m_pStudioHeader->numbones, has_bumpmap, has_boneweights, has_vertexlight, has_lightmap );
	uint type = SelectMeshLoader( m_pStudioHeader->numbones, has_bumpmap, has_boneweights, has_vertexlight, has_lightmap );

	// move data to video memory
	if( glConfig.version < ACTUAL_GL_VERSION )
		m_pfnMeshLoaderGL21[type].CreateBuffer( pOut, &m_arrayxvert[pMeshInfo->firstvertex] );
	else m_pfnMeshLoaderGL30[type].CreateBuffer( pOut, &m_arrayxvert[pMeshInfo->firstvertex] );
	CreateIndexBuffer( pOut, arrayelems.data() );

//	Msg( "%s -> %s\n", m_pfnMeshLoaderGL21[type].BufferName, RI->currentmodel->name );

	// link it with vertex array object
	pglGenVertexArrays( 1, &pOut->vao );
	pglBindVertexArray( pOut->vao );
	if( glConfig.version < ACTUAL_GL_VERSION )
		m_pfnMeshLoaderGL21[type].BindBuffer( pOut, attribs );
	else m_pfnMeshLoaderGL30[type].BindBuffer( pOut, attribs );
	BindIndexBuffer( pOut );
	pglBindVertexArray( GL_FALSE );

	// update stats
	tr.total_vbo_memory += pOut->cacheSize;
}

mstudiocache_t *CStudioModelRenderer :: CreateStudioCache( void *srclight, int lightmode )
{
	float		start_time = Sys_DoubleTime();
	bool		unique_model = (srclight == NULL);	// just for more readable code
	bool 		has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	TmpModel_t	submodel[MAXSTUDIOMODELS];	// list of unique models
	float		poseparams[MAXSTUDIOPOSEPARAM];
	static matrix3x4	bones[MAXSTUDIOBONES];
	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];
	int		num_submodels = 0;
	dmodelvertlight_t	*dvl = NULL;
	dmodelfacelight_t	*dfl = NULL;
	mstudiocache_t	*studiocache;
	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t	*psubmodel;
	msubmodel_t	*pModel;
	mstudiobone_t	*pbones;
	matrix3x4		root;

	switch( lightmode )
	{
	case LIGHTSTATIC_VERTEX:
		dvl = (dmodelvertlight_t *)srclight;
		break;
	case LIGHTSTATIC_SURFACE:
		dfl = (dmodelfacelight_t *)srclight;
		break;
	}

	// materials goes first to determine bump
	if( unique_model ) LoadStudioMaterials ();
	else PrecacheStudioShaders ();

	// build default pose to build seamless TBN-space
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_boneSetup.SetStudioPointers( m_pStudioHeader, poseparams );
	m_boneSetup.CalcDefaultPoseParameters( poseparams );
	root.Identity();

	// setup local bone matrices
	if( unique_model && FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEINFO ))
	{
		// NOTE: extended boneinfo goes immediately after bones
		mstudioboneinfo_t *boneinfo = (mstudioboneinfo_t *)((byte *)pbones + m_pStudioHeader->numbones * sizeof( mstudiobone_t ));

		// alloc storage for bone array
		RI->currentmodel->poseToBone = (mposetobone_t *)Mem_Alloc( sizeof( mposetobone_t ));

		for( int j = 0; j < m_pStudioHeader->numbones; j++ )
			LoadLocalMatrix( j, &boneinfo[j] );
	}

	if( lightmode == LIGHTSTATIC_SURFACE )
	{	
		root = matrix3x4( Vector( dfl->origin ), Vector( dfl->angles ), Vector( dfl->scale ));
		m_boneSetup.InitPose( pos, q );
		m_boneSetup.AccumulatePose( NULL, pos, q, 0, 0.0f, 1.0f );
	}
	else
	{
		// compute default pose with no anim
		m_boneSetup.InitPose( pos, q );
	}

	for( int i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		// initialize bonematrix
		matrix3x4 bonematrix = matrix3x4( pos[i], q[i] );

		if( pbones[i].parent == -1 ) bones[i] = root.ConcatTransforms( bonematrix );
		else bones[i] = bones[pbones[i].parent].ConcatTransforms( matrix3x4( pos[i], q[i] ));
	}

	if (has_boneweights)
	{
		// convert bones into worldtransform
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			bones[i] = bones[i].ConcatTransforms( RI->currentmodel->poseToBone->posetobone[i] );
	}

	memset( submodel, 0, sizeof( submodel ));
	word meshUniqueID = 0;
	m_nNumTBNVerts = 0;	// counting through all the submodels
	num_submodels = 0;

	// build list of unique submodels (by name)
	for( int i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;

		for( int k, j = 0; j < pbodypart->nummodels; j++ )
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

	m_iTBNState = TBNSTATE_INACTIVE;

	// only models with bump-mapping is required to build TBN matrices
	if( FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BUMP ))
	{
		if( !StudioLoadTBN( ))
		{
			int	max_model_verts = 0;

			// multiplier 8 is a good enough to predict max vertices count
			for( int i = 0; i < num_submodels; i++ )
				max_model_verts += submodel[i].pmodel->numverts * 8;

			// reserve space for all the model verts
			m_tbnverts = (dmodeltbn_t *)calloc( sizeof( dmodeltbn_t ), max_model_verts );
			m_tbnverts->ident = IDTBNHEADER;
			m_tbnverts->version = TBN_VERSION;
			m_tbnverts->modelCRC = RI->currentmodel->modelCRC;

			// store submodel offsets here
			for( int i = 0; i < num_submodels; i++ )
			{
				psubmodel = submodel[i].pmodel;
				m_tbnverts->submodels[i].submodel_offset = (byte *)psubmodel - (byte *)m_pStudioHeader;
			}
			m_iTBNState = TBNSTATE_GENERATE;
		}

		if( !unique_model && m_iTBNState == TBNSTATE_GENERATE )
			ALERT( at_warning, "%s: generate TBN due loading light cache\n", RI->currentmodel->name ); 
	}

	// setup pointers
	studiocache = new mstudiocache_t();
	m_pStudioCache = studiocache;

	if( dfl != NULL && dfl->numfaces > 0 ) {
		studiocache->surfaces.resize(dfl->numfaces);
	}

	studiocache->bodyparts.resize(m_pStudioHeader->numbodyparts);
	studiocache->submodels.resize(num_submodels);

	// begin to building submodels
	for( int j, i = 0; i < num_submodels; i++ )
	{
		psubmodel = submodel[i].pmodel;
		pModel = &studiocache->submodels[i];
		pModel->meshes.resize(psubmodel->nummesh);

		// sanity check for vertexlighting
		if( dvl != NULL && dvl->numverts > 0 )
		{
			// search for submodel offset
			int	offset = (byte *)psubmodel - (byte *)m_pStudioHeader;

			for( j = 0; j < MAXSTUDIOMODELS; j++ )
			{
				if( dvl->submodels[j].submodel_offset == offset )
					break;			
			}

			ASSERT( j != MAXSTUDIOMODELS );
			ASSERT( m_nNumLightVerts == dvl->submodels[j].vertex_offset );
		}

		// sanity check for surfacelighting
		if( dfl != NULL && dfl->numfaces > 0 )
		{
			// search for submodel offset
			int	offset = (byte *)psubmodel - (byte *)m_pStudioHeader;

			for( j = 0; j < MAXSTUDIOMODELS; j++ )
			{
				if( dfl->submodels[j].submodel_offset == offset )
					break;			
			}

			ASSERT( j != MAXSTUDIOMODELS );
			ASSERT( m_nNumLightFaces == dfl->submodels[j].surface_offset );
		}

		// sanity check for TBN matrix
		if( m_iTBNState == TBNSTATE_LOADING && m_tbnverts != NULL )
		{
			// search for submodel offset
			int	offset = (byte *)psubmodel - (byte *)m_pStudioHeader;

			for( j = 0; j < MAXSTUDIOMODELS; j++ )
			{
				if( m_tbnverts->submodels[j].submodel_offset == offset )
					break;			
			}

			ASSERT( j != MAXSTUDIOMODELS );
			ASSERT( m_nNumTBNVerts == m_tbnverts->submodels[j].vertex_offset );
		}

		// setup all the vertices for a given submodel
		SetupSubmodelVerts( psubmodel, bones, srclight, lightmode );

		for( int j = 0; j < psubmodel->nummesh; j++ )
		{
			mstudiomesh_t *pSrc = (mstudiomesh_t *)((byte *)m_pStudioHeader + psubmodel->meshindex) + j;
			vbomesh_t *pDst = &pModel->meshes[j];

			MeshCreateBuffer( pDst, pSrc, &m_pTempMesh[j], lightmode );
			pDst->uniqueID = meshUniqueID++;
		}
		submodel[i].pout = pModel; // store unique submodel
	}

	// and finally setup bodyparts
	for( int i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + i;
		mbodypart_t *pBodyPart = &studiocache->bodyparts[i];

		pBodyPart->base = pbodypart->base;
		pBodyPart->models.resize(pbodypart->nummodels);

		// setup pointers to unique models	
		for( int k, j = 0; j < pbodypart->nummodels; j++ )
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

	// TBN are created, time to store it
	if( m_iTBNState == TBNSTATE_GENERATE && m_tbnverts != NULL )
	{
		// store total vertices count
		m_tbnverts->numverts = m_nNumTBNVerts;
		if( StudioSaveTBN( ))
			ALERT( at_console, "%s: TBN build time %g secs\n", RI->currentmodel->name, Sys_DoubleTime() - start_time );
		m_iTBNState = TBNSTATE_INACTIVE;
		free( m_tbnverts );
		m_tbnverts = NULL;
	}
	else if( m_iTBNState == TBNSTATE_LOADING && m_tbnverts != NULL )
	{
		m_iTBNState = TBNSTATE_INACTIVE;
		FREE_FILE( m_tbnverts );
		m_tbnverts = NULL;
	}

	// load lightmaps
	m_pStudioCache->update_light = true;

	// invalidate
	m_pStudioCache = NULL;

	return studiocache;
}

//-----------------------------------------------------------------------------
// all the caches should be build before starting the new map
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: CreateStudioCacheVL( const char *modelname, int cacheID )
{
	dvlightlump_t	*vl = world->vertex_lighting;

	// first we need throw previous mesh
	DeleteStudioCache( &tr.vertex_light_cache[cacheID] );

	if( world->vertex_lighting == NULL )
		return; // for some reasons we missed this lump

	RI->currentmodel = IEngineStudio.Mod_ForName( modelname, false );
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );

	if( !RI->currentmodel || !m_pStudioHeader )
		return; // download in progress?

	// first initialization
	if( cacheID < vl->nummodels && vl->dataofs[cacheID] != -1 )
	{
		dmodelvertlight_t	*dml = (dmodelvertlight_t *)((byte *)vl + vl->dataofs[cacheID]);

		m_nNumLightVerts = 0;

		if( RI->currentmodel->modelCRC == dml->modelCRC )
		{
			// now create mesh per entity with instanced vertex lighting
			tr.vertex_light_cache[cacheID] = CreateStudioCache( dml, LIGHTSTATIC_VERTEX );
		}

		if( dml->numverts == m_nNumLightVerts )
		{
			ALERT( at_aiconsole, "%s vertexlit instance created\n", modelname );
		}
		else if( RI->currentmodel->modelCRC != dml->modelCRC )
		{
			ALERT( at_error, "%s failed to create vertex lighting: model CRC %08X != %08X\n",
				modelname, RI->currentmodel->modelCRC, dml->modelCRC );
		}
		else
		{
			ALERT( at_error, "%s failed to create vertex lighting: model verts %i != total verts %i\n",
				modelname, dml->numverts, m_nNumLightVerts );
		}
		m_nNumLightVerts = 0;
	}
}

void CStudioModelRenderer :: CreateStudioCacheFL( const char *modelname, int cacheID )
{
	dvlightlump_t	*vl = world->surface_lighting;

	// first we need throw previous mesh
	DeleteStudioCache( &tr.surface_light_cache[cacheID] );

	if( world->surface_lighting == NULL )
		return; // for some reasons we missed this lump

	RI->currentmodel = IEngineStudio.Mod_ForName( modelname, false );
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel );

	if( !RI->currentmodel || !m_pStudioHeader )
		return; // download in progress?

	// first initialization
	if( cacheID < vl->nummodels && vl->dataofs[cacheID] != -1 )
	{
		dmodelfacelight_t	*dml = (dmodelfacelight_t *)((byte *)vl + vl->dataofs[cacheID]);

		m_nNumLightFaces = 0;

		if( RI->currentmodel->modelCRC == dml->modelCRC )
		{
			// now create mesh per entity with instanced surface lighting
			tr.surface_light_cache[cacheID] = CreateStudioCache( dml, LIGHTSTATIC_SURFACE );
		}

		if( dml->numfaces == m_nNumLightFaces )
		{
			ALERT( at_aiconsole, "%s surfacelit instance created\n", modelname );
		}
		else if( RI->currentmodel->modelCRC != dml->modelCRC )
		{
			ALERT( at_error, "%s failed to create surface lighting: model CRC %08X != %08X\n",
				modelname, RI->currentmodel->modelCRC, dml->modelCRC );
		}
		else
		{
			ALERT( at_error, "%s failed to create surface lighting: model faces %i != total faces %i\n",
				modelname, dml->numfaces, m_nNumLightFaces );
		}
		m_nNumLightFaces = 0;
	}
}

void CStudioModelRenderer :: CreateStudioCacheVL( dmodelvertlight_t *dml, int cacheID )
{
	if( RI->currentmodel->modelCRC == dml->modelCRC )
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
			mat->forwardScene.Invalidate(); // refresh shaders
		}
	}
	else
	{
		ALERT( at_warning, "failed to create vertex lighting for %s\n", RI->currentmodel->name );
		SetBits( m_pModelInstance->info_flags, MF_VL_BAD_CACHE );
	}
}

void CStudioModelRenderer :: CreateStudioCacheFL( dmodelfacelight_t *dml, int cacheID )
{
	if( RI->currentmodel->modelCRC == dml->modelCRC )
	{
		// get lighting cache
		m_pModelInstance->m_FlCache = tr.surface_light_cache[cacheID];
	}

	if( m_pModelInstance->m_FlCache != NULL )
	{
		SetBits( m_pModelInstance->info_flags, MF_SURFACE_LIGHTING );

		for( int map = 0; map < MAXLIGHTMAPS; map++ )
			m_pModelInstance->styles[map] = dml->styles[map];

		for( int i = 0; i < m_pStudioHeader->numtextures; i++ )
		{
			mstudiomaterial_t *mat = &m_pModelInstance->materials[i];
			mat->forwardScene.Invalidate(); // refresh shaders
		}
	}
	else
	{
		ALERT( at_warning, "failed to create surface lighting for %s\n", RI->currentmodel->name );
		SetBits( m_pModelInstance->info_flags, MF_VL_BAD_CACHE );
	}
}

//-----------------------------------------------------------------------------
// all the caches should be build before starting the new map
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: FreeStudioCacheVL( void )
{
	for( int i = 0; i < MAX_LIGHTCACHE; i++ )
		DeleteStudioCache( &tr.vertex_light_cache[i] );
}

void CStudioModelRenderer :: FreeStudioCacheFL( void )
{
	for( int i = 0; i < MAX_LIGHTCACHE; i++ )
		DeleteStudioCache( &tr.surface_light_cache[i] );
}

void CStudioModelRenderer :: ProcessUserData( model_t *mod, qboolean create, const byte *buffer )
{
	// to get something valid here
	RI->currententity = GET_ENTITY( 0 );
	RI->currentmodel = mod;

	if( !( m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( RI->currentmodel )))
		return;

	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( RI->currentmodel );

	if( create )
	{
		// compute model CRC to verify vertexlighting data
		// NOTE: source buffer is not equal to Mod_Extradata!
		studiohdr_t *src = (studiohdr_t *)buffer;
		RI->currentmodel->modelCRC = FILE_CRC32( buffer, src->length );
		double start = Sys_DoubleTime();
		RI->currentmodel->studiocache = CreateStudioCache();
		double end = Sys_DoubleTime();
		r_buildstats.create_buffer_object += (end - start);
		r_buildstats.total_buildtime += (end - start);
	}
	else {
		DestroyMeshCache();
	}
}
