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

		R_InitViewBeams ();
	}

	// Success
	return 1;
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
	m_pCvarLerping		= IEngineStudio.GetCvar( "r_studio_lerping" );
	m_pCvarLambert		= IEngineStudio.GetCvar( "r_studio_lambert" );
	m_pCvarLighting		= IEngineStudio.GetCvar( "r_studio_lighting" );
	m_pCvarDrawViewModel	= IEngineStudio.GetCvar( "r_drawviewmodel" );
	m_pCvarHand		= IEngineStudio.GetCvar( "hand" );
	m_pCvarViewmodelFov		= CVAR_REGISTER( "cl_viewmodel_fov", "90", FCVAR_ARCHIVE );
	m_pCvarStudioCache		= CVAR_REGISTER( "r_studiocache", "1", FCVAR_ARCHIVE );

	m_pChromeSprite		= IEngineStudio.GetChromeSprite();

	m_nStudioModelCount = 0;
	m_nModelsDrawn = 0;
}

/*
====================
CStudioModelRenderer

====================
*/
CStudioModelRenderer :: CStudioModelRenderer( void )
{
	m_fDoInterp	= 1;
	m_fGaitEstimation	= 0;
	m_pCurrentEntity	= NULL;
	m_pCvarHiModels	= NULL;
	m_pCvarLerping	= NULL;
	m_pCvarLambert	= NULL;
	m_pCvarLighting	= NULL;
	m_pCvarDrawViewModel= NULL;
	m_pCvarHand	= NULL;
	m_pChromeSprite	= NULL;
	m_pStudioHeader	= NULL;
	m_pBodyPart	= NULL;
	m_pSubModel	= NULL;
	m_pPlayerInfo	= NULL;
	m_pRenderModel	= NULL;

	m_pLightInfo = new CStudioLight;
}

/*
====================
~CStudioModelRenderer

====================
*/
CStudioModelRenderer::~CStudioModelRenderer( void )
{
	DestroyAllModelInstances ();

	delete m_pLightInfo;
}

word CStudioModelRenderer::CreateInstance( cl_entity_t *pEnt )
{
	if( !IEngineStudio.Mod_Extradata( m_pRenderModel ))
	{
		return INVALID_HANDLE;
	}

	word handle = m_ModelInstances.AddToTail();
	m_ModelInstances[handle].m_pEntity = pEnt;
	m_ModelInstances[handle].m_DecalHandle = INVALID_HANDLE;
	m_ModelInstances[handle].m_pModel = m_pRenderModel;
	m_ModelInstances[handle].m_DecalCount = 0;
	memset( &m_ModelInstances[handle].attachment, 0, sizeof( StudioAttachment_t ) * MAXSTUDIOATTACHMENTS );

	CreateBoneCache( handle );
	CreateVertexCache( handle );

	return handle;
}

void CStudioModelRenderer :: DestroyInstance( word handle )
{
	if( handle != INVALID_HANDLE )
	{
		DestroyBoneCache( handle );
		DestroyVertexCache( handle );
		DestroyDecalList( m_ModelInstances[handle].m_DecalHandle );
		m_ModelInstances.Remove( handle );
	}
}

void CStudioModelRenderer :: CreateBoneCache( word handle )
{
	if( handle == INVALID_HANDLE )
	{
		ALERT( at_error, "%s failed to allocate bonecache\n", m_pRenderModel->name );
		return;
	}

	// create a bonecache too
	size_t cacheSize = sizeof( BoneCache_t ) + sizeof( matrix3x4 ) * m_pStudioHeader->numbones;
	m_ModelInstances[handle].cache = (BoneCache_t *)calloc( 1, cacheSize );
	m_ModelInstances[handle].cache->bones = (matrix3x4 *)((byte *)m_ModelInstances[handle].cache + sizeof( BoneCache_t )); // bone array
	m_ModelInstances[handle].cache->numBones = m_pStudioHeader->numbones;
}

void CStudioModelRenderer :: DestroyBoneCache( word handle )
{
	if( handle != INVALID_HANDLE )
	{
		if( m_ModelInstances[handle].cache != NULL )
		{
			free( m_ModelInstances[handle].cache );
			m_ModelInstances[handle].cache = NULL;
		}
	}
}

bool CStudioModelRenderer :: CheckBoneCache( cl_entity_t *ent, float f )
{
	if( !m_pCvarStudioCache->value )
	{
		// disable cache
		m_pbonetransform = m_pbonestransform;
		return false;
	}

	// create instance and bone cache
	if( ent->modelhandle == INVALID_HANDLE )
		ent->modelhandle = CreateInstance( ent );

	if( ent->modelhandle == INVALID_HANDLE )
	{
		ALERT( at_error, "%s failed to allocate bonecache\n", ent->model->name );
		m_pbonetransform = m_pbonestransform;
		return false; // out of memory?
	}

	if( !IsModelInstanceValid( ent->modelhandle, true ))
	{
		DestroyBoneCache( ent->modelhandle );
		DestroyVertexCache( ent->modelhandle );
		CreateBoneCache( ent->modelhandle );
		CreateVertexCache( ent->modelhandle );
		m_ModelInstances[ent->modelhandle].m_pModel = m_pRenderModel; // update model
	}

	BoneCache_t *cache = m_ModelInstances[ent->modelhandle].cache;

	if( cache == NULL ) HOST_ERROR( "Studio_CheckBoneCache: cache == NULL\n" );

	// set pointer for current bonecache
	m_pbonetransform = cache->bones;

	// make sure what all cached values are unchanged
	if( cache->frame == f && cache->sequence == ent->curstate.sequence && cache->transform == m_protationmatrix &&
	!memcmp( cache->blending, ent->curstate.blending, 2 ) && !memcmp( cache->controller, ent->curstate.controller, 4 )
	&& cache->mouthopen == ent->mouth.mouthopen )
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
	cache->transform = m_protationmatrix;
	cache->mouthopen = ent->mouth.mouthopen;
	cache->sequence = ent->curstate.sequence;
	memcpy( cache->blending, ent->curstate.blending, 2 );
	memcpy( cache->controller, ent->curstate.controller, 4 );
	cache->vertexCacheValid = false; // will be change later

	if( m_pPlayerInfo )
	{
		cache->gaitsequence = m_pPlayerInfo->gaitsequence;
		cache->gaitframe = m_pPlayerInfo->gaitframe;
	}

	return false;
}

void CStudioModelRenderer :: CreateVertexCache( word handle )
{
	int vertSize = 0, normSize = 0;
	int totalVertSize = 0, totalNormSize = 0;
	size_t vertOffsets[MAXSTUDIOMODELS];
	size_t normOffsets[MAXSTUDIOMODELS];
	int lastNormSize = 0;

	memset( vertOffsets, 0, sizeof( vertOffsets ));
	memset( normOffsets, 0, sizeof( normOffsets ));

	// through all bodies to determine max vertices count
	for( int j = 0; j < m_pStudioHeader->numbodyparts; j++ )
	{
		mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + j;

		int index = m_ModelInstances[handle].m_pEntity->curstate.body / pbodypart->base;
		index = index % pbodypart->nummodels;

		mstudiomodel_t *psubmodel = (mstudiomodel_t *)((byte *)m_pStudioHeader + pbodypart->modelindex) + index;
		vertSize = (psubmodel->numverts * sizeof(Vector));
		normSize = (psubmodel->numnorms * sizeof(Vector));

		// NOTE: vertex and normals array are interleaved into memory representation
		if( j == 0 )
		{
			vertOffsets[j] = 0;	// just in case
			normOffsets[j] = vertOffsets[j] + vertSize;
		}
		else
		{
			vertOffsets[j] = normOffsets[j-1] + lastNormSize;
			normOffsets[j] = vertOffsets[j] + vertSize;
		}

		// count total cache vertexes and normals
		totalVertSize += vertSize;
		totalNormSize += normSize;
		lastNormSize = normSize;
	}

	// create a vertexcache too
	size_t cacheSize = sizeof( VertCache_t ) + totalVertSize + totalNormSize;
	m_ModelInstances[handle].vertcache = (VertCache_t *)calloc( 1, cacheSize );
	byte *cache_base = (byte *)m_ModelInstances[handle].vertcache + sizeof( VertCache_t );

	// simple huh?
	for( int i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		m_ModelInstances[handle].vertcache->verts[i] = (Vector *)(cache_base + vertOffsets[i]); // vertex array
		m_ModelInstances[handle].vertcache->norms[i] = (Vector *)(cache_base + normOffsets[i]); // normal array
	}
}

void CStudioModelRenderer :: DestroyVertexCache( word handle )
{
	if( handle != INVALID_HANDLE )
	{
		if( m_ModelInstances[handle].vertcache != NULL )
		{
			free( m_ModelInstances[handle].vertcache );
			m_ModelInstances[handle].vertcache = NULL;
		}
	}
}

bool CStudioModelRenderer :: CheckVertexCache( cl_entity_t *ent )
{
	if( !m_pCvarStudioCache->value )
	{
		// disable cache
		m_verts = g_verts;
		m_norms = g_norms;
		return false;
	}

	// any cache changes should be done in StudioSetupBones
	// so we have only once reason for get this state:
	// draw attached weapon model.
	if( ent->modelhandle == INVALID_HANDLE )
	{
		// using intermediate array
		m_verts = g_verts;
		m_norms = g_norms;
		return false;
	}

	BoneCache_t *cache = m_ModelInstances[ent->modelhandle].cache;
	VertCache_t *vcache = m_ModelInstances[ent->modelhandle].vertcache;
	int body = abs( ent->curstate.body );

	if( cache == NULL ) HOST_ERROR( "Studio_CheckVertexCache: cache == NULL\n" );
	if( vcache == NULL ) HOST_ERROR( "Studio_CheckVertexCache: vcache == NULL\n" );

	// if user change body we need to reallocate cache
	if( vcache->body != body )
	{
		DestroyVertexCache( ent->modelhandle );
		CreateVertexCache( ent->modelhandle );

		// update cache pointer
		vcache = m_ModelInstances[ent->modelhandle].vertcache;
		if( vcache == NULL ) HOST_ERROR( "Studio_CheckVertexCache: vcache == NULL\n" );
	}

	assert( m_iBodyPartIndex >= 0 && m_iBodyPartIndex < MAXSTUDIOMODELS );

	// set pointer for current vertexcache
	m_verts = vcache->verts[m_iBodyPartIndex];
	m_norms = vcache->norms[m_iBodyPartIndex];

	// make sure what all cached values are unchanged
	if( cache->vertexCacheValid && vcache->body == body && vcache->cacheFlags & (1<<m_iBodyPartIndex))
	{
		// cache are valid
		return true;
	}
	else if( !cache->vertexCacheValid )
	{
		// bonecache is completely changed
		vcache->cacheFlags = 0;
	}

	// update vertexcache
	vcache->body = body;
	cache->vertexCacheValid = true;
	vcache->cacheFlags |= (1<<m_iBodyPartIndex); // this bodypart is now valid

	return false;
}

void CStudioModelRenderer::DestroyAllModelInstances( void )
{
	// NOTE: should destroy in reverse-oreder because it's linked list not array!
	for( int i = m_ModelInstances.Count(); --i >= 0; )
          {
		DestroyInstance( i );
	}

	m_DecalMaterial.RemoveAll();
}

//-----------------------------------------------------------------------------
// Create, destroy list of decals for a particular model
//-----------------------------------------------------------------------------
word CStudioModelRenderer::CreateDecalList( void )
{
	word handle = m_DecalList.AddToTail();
	m_DecalList[handle].m_FirstMaterial = m_DecalMaterial.InvalidIndex();

	return handle;
}

void CStudioModelRenderer::DestroyDecalList( word handle )
{
	if( handle != INVALID_HANDLE )
	{
		// blat out all geometry associated with all materials
		word mat = m_DecalList[handle].m_FirstMaterial;
		word next;
		while( mat != m_DecalMaterial.InvalidIndex( ))
		{
			next = m_DecalMaterial.Next( mat );
			m_DecalMaterial.Remove( mat );
			mat = next;
		}
		m_DecalList.Remove( handle );
	}
}

void CStudioModelRenderer::StudioClearDecals( void )
{
	// NOTE: should destroy in reverse-oreder because it's linked list not array!
	for( int i = m_ModelInstances.Count(); --i >= 0; )
          {
		ModelInstance_t& inst = m_ModelInstances[i];

		if( inst.m_DecalHandle == INVALID_HANDLE )
			continue;	// not initialized?

		DestroyDecalList( inst.m_DecalHandle );
		inst.m_DecalHandle = INVALID_HANDLE;
	}
}

/*
================
StudioExtractBbox

Extract bbox from current sequence
================
*/
int CStudioModelRenderer::StudioExtractBbox( cl_entity_t *e, studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs )
{
	if( sequence == -1 )
		return 0;

	mstudioseqdesc_t	*pseqdesc;
	float scale = 1.0f;

	if( !phdr ) return 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if( e->curstate.scale > 0.0f && e->curstate.scale <= 16.0f )
		scale = e->curstate.scale;
	
	mins = pseqdesc[sequence].bbmin * scale;
	maxs = pseqdesc[sequence].bbmax * scale;

	return 1;
}

/*
================
StudioComputeBBox

Compute a full bounding box for current sequence
================
*/
int CStudioModelRenderer::StudioComputeBBox( cl_entity_t *e, Vector bbox[8] )
{
	Vector	tmp_mins, tmp_maxs;
	Vector	angles, p1, p2;
	int	seq = e->curstate.sequence;
	float	scale = 1.0f;

	if( !StudioExtractBbox( e, m_pStudioHeader, seq, tmp_mins, tmp_maxs ))
		return false;

	if( e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	// copy original bbox
	studio_mins = m_pStudioHeader->bbmin * scale;
	studio_maxs = m_pStudioHeader->bbmax * scale;

	// rotate the bounding box
	angles = e->angles;

	// don't rotate player model, only aim
	if( e->player ) angles[PITCH] = 0;

	matrix3x3	vectors( angles );
	vectors = vectors.Transpose();

	// compute a full bounding box
	for( int i = 0; i < 8; i++ )
	{
  		p1[0] = ( i & 1 ) ? tmp_mins[0] : tmp_maxs[0];
  		p1[1] = ( i & 2 ) ? tmp_mins[1] : tmp_maxs[1];
  		p1[2] = ( i & 4 ) ? tmp_mins[2] : tmp_maxs[2];

		// rotate by YAW
		p2[0] = DotProduct( p1, vectors[0] );
		p2[1] = DotProduct( p1, vectors[1] );
		p2[2] = DotProduct( p1, vectors[2] );

		if( bbox ) bbox[i] = p2 + e->origin;

  		if( p2[0] < studio_mins[0] ) studio_mins[0] = p2[0];
  		if( p2[0] > studio_maxs[0] ) studio_maxs[0] = p2[0];
  		if( p2[1] < studio_mins[1] ) studio_mins[1] = p2[1];
  		if( p2[1] > studio_maxs[1] ) studio_maxs[1] = p2[1];
  		if( p2[2] < studio_mins[2] ) studio_mins[2] = p2[2];
  		if( p2[2] > studio_maxs[2] ) studio_maxs[2] = p2[2];
	}

	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );

	return true;
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int &pBlend, float &pPitch )
{
	if( RP_LOCALCLIENT( m_pCurrentEntity ))
	{
		if( RI.params & ( RP_MIRRORVIEW|RP_SCREENVIEW ))
			pBlend = (pPitch * -6);
		else pBlend = (pPitch * 3);
	}
	else
	{
		pBlend = (pPitch * 3);
	}

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

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer::StudioSetUpTransform( void )
{
	Vector origin = m_pCurrentEntity->origin;
	Vector angles = m_pCurrentEntity->angles;

	// don't rotate clients, only aim
	if( m_pCurrentEntity->player )
		angles[PITCH] = 0;

	if( m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP ) 
	{
		float f;
                    
		// don't do it if the goalstarttime hasn't updated in a while.
		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
		if(( m_clTime < m_pCurrentEntity->curstate.animtime + 1.0f ) && ( m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime ))
		{
			f = (m_clTime - m_pCurrentEntity->curstate.animtime) / (m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime);
		}

		if( m_fDoInterp )
		{
			// ugly hack to interpolate angle, position.
			// current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		InterpolateOrigin( m_pCurrentEntity->latched.prevorigin, m_pCurrentEntity->origin, origin, f, true );
		InterpolateAngles( m_pCurrentEntity->latched.prevangles, m_pCurrentEntity->angles, angles, f, true );
	}

// g-cont. now is fixed
//	angles[PITCH] = -angles[PITCH]; // stupid quake bug!

	float scale = 1.0f;

	// apply studiomodel scale (clamp scale to prevent too big sizes on some HL maps)
	if( m_pCurrentEntity->curstate.scale > 0.0f && m_pCurrentEntity->curstate.scale <= 16.0f )
		scale = m_pCurrentEntity->curstate.scale;

	// build the rotation matrix
	m_protationmatrix = matrix3x4( origin, angles, scale );

	if( m_pCurrentEntity == GET_VIEWMODEL() && m_pCvarHand->value == LEFT_HAND )
	{
		// inverse the right vector
		m_protationmatrix.SetRight( -m_protationmatrix.GetRight() );
	}
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer::StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	double dfdt = 0, f = 0;

	if( m_fDoInterp && m_clTime >= m_pCurrentEntity->curstate.animtime )
	{
		dfdt = (m_clTime - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * pseqdesc->fps;
	}

	if( pseqdesc->numframes > 1)
	{
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
		
	}
 
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
float CStudioModelRenderer::StudioEstimateInterpolant( void )
{
	float dadt = 1.0;

	if( m_fDoInterp && ( m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01f ))
	{
		dadt = (m_clTime - m_pCurrentEntity->curstate.animtime) / 0.1f;

		if( dadt > 2.0f )
		{
			dadt = 2.0f;
		}
	}
	return dadt;
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t *CStudioModelRenderer::StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
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
void CStudioModelRenderer::StudioFxTransform( cl_entity_t *ent, matrix3x4 &transform )
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
			float scale = 1.0f + ( m_clTime - ent->curstate.animtime ) * 10.0f;
			if( scale > 2 ) scale = 2; // don't blow up more than 200%
			transform.SetRight( transform.GetRight() * scale );
		}
		break;
	}
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CStudioModelRenderer::StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen )
{
	float value;
	mstudiobonecontroller_t *pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for( int j = 0; j < m_pStudioHeader->numbonecontrollers; j++ )
	{
		int i = pbonecontroller[j].index;
		if( i <= 3 )
		{
			// check for 360% wrapping
			if( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					int a = (pcontroller1[j] + 128) % 256;
					int b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1.0f - dadt)) - 128) * (360.0f / 256.0f) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0f - dadt))) * (360.0f / 256.0f) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				value = bound( 0.0f, value, 1.0f );
				value = (1.0f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
		}
		else
		{
			value = mouthopen / 64.0;
			value = bound( 0.0f, value, 1.0f );				
			value = (1.0f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
		}

		switch( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, Vector4D &q )
{
	Vector4D q1, q2;
	Vector angle1, angle2;
	mstudioanimvalue_t *panimvalue;

	for( int j = 0; j < 3; j++ )
	{
		if( panim->offset[j+3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			int k = frame;

			// DEBUG
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

				// DEBUG
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// bah, missing blend!
			if( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k+1].value;

				if( panimvalue->num.valid > k + 1 )
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if( panimvalue->num.total > k + 1 )
						angle2[j] = angle1[j];
					else angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if( panimvalue->num.total > k + 1 )
					angle2[j] = angle1[j];
				else angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
			}

			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if( pbone->bonecontroller[j+3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if( angle1 != angle2 )
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, Vector &pos )
{
	mstudioanimvalue_t *panimvalue;

	for( int j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;

		if( panim->offset[j] != 0 )
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			int k = frame;

			// DEBUG
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;

			// find span of values that includes the frame we want
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
  
				// DEBUG
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}

			// if we're inside the span
			if( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if( panimvalue->num.valid > k + 1 )
					pos[j] += (panimvalue[k+1].value * (1.0f - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				else pos[j] += panimvalue[k+1].value * pbone->scale[j];
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if( panimvalue->num.total <= k + 1 )
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0f - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				else pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
			}
		}

		if( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void CStudioModelRenderer::StudioSlerpBones( Vector4D q1[], Vector pos1[], Vector4D q2[], Vector pos2[], float s )
{
	float	s1;

	// clamp the frac
	s = bound( 0.0f, s, 1.0f );
	s1 = 1.0f - s;

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q1[i] );
		InterpolateOrigin( pos1[i], pos2[i], pos1[i], s );
	}
}

/*
====================
StudioCalcRotations

====================
*/
void CStudioModelRenderer::StudioCalcRotations( Vector pos[], Vector4D q[], mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	float adj[MAXSTUDIOCONTROLLERS];

	// bah, fix this bug with changing sequences too fast
	if( f > pseqdesc->numframes - 1 )
	{
		f = 0;
	}
	else if( f < -0.01f )
	{
		// BUG ( somewhere else ) but this code should validate this data.
		// This could cause a crash if the frame # is negative, so we'll go ahead
		// and clamp it here
		f = -0.01f;
	}

	int frame = (int)f;

	float dadt = StudioEstimateInterpolant();
	float s = (f - frame); // cut fractional part

	// add in programtic controllers
	mstudiobone_t *pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	StudioCalcBoneAdj( dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen );

	for( int i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++ ) 
	{
		StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone].x = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone].y = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone].z = 0.0f;

	// g-cont. probably this was autoanimating code
	// but is obsolete and disabled
	s = 0 * ((1.0f - s) / (pseqdesc->numframes)) * m_pCurrentEntity->curstate.framerate;

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone].x += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone].y += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone].z += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer::StudioSetupBones( void )
{
	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	matrix3x4		bonematrix;

	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];
	static Vector	pos2[MAXSTUDIOBONES];
	static Vector4D	q2[MAXSTUDIOBONES];
	static Vector	pos3[MAXSTUDIOBONES];
	static Vector4D	q3[MAXSTUDIOBONES];
	static Vector	pos4[MAXSTUDIOBONES];
	static Vector4D	q4[MAXSTUDIOBONES];

	if( m_pCurrentEntity->curstate.sequence < 0 || m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq ) 
		m_pCurrentEntity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	float f = StudioEstimateFrame( pseqdesc );

	if( CheckBoneCache( m_pCurrentEntity, f ))
		return;	// using a cached bones no need transformations

	panim = StudioGetAnim( m_pRenderModel, pseqdesc );
	StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		float dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;

		StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;
			StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0f - dadt)) / 255.0f;
			StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}
	
	if( m_fDoInterp && m_pCurrentEntity->latched.sequencetime && ( m_pCurrentEntity->latched.sequencetime + 0.2f > m_clTime ) && ( m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq ))
	{
		static Vector	pos1b[MAXSTUDIOBONES];
		static Vector4D	q1b[MAXSTUDIOBONES];
		float		s;

		// blend from last sequence
		// g-cont. blending between sequences should be done in 0.1 secs. See Xash3D cl_frame.c code for details
		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim( m_pRenderModel, pseqdesc );

		// clip prevframe
		StudioCalcRotations( pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

		if( pseqdesc->numblends > 1 )
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0f;
			StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if( pseqdesc->numblends == 4 )
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0f;
				StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0f;
				StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0f - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2f;
		StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if( m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0 )
	{
		if( m_pPlayerInfo->gaitsequence < 0 || m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq ) 
			m_pPlayerInfo->gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = StudioGetAnim( m_pRenderModel, pseqdesc );
		StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe );

		bool copy = true;

		for( int i = 0; i < m_pStudioHeader->numbones; i++)
		{
			if( !Q_strcmp( pbones[i].name, "Bip01 Spine" ))
				copy = false;
			else if( !Q_strcmp( pbones[pbones[i].parent].name, "Bip01 Pelvis" ))
				copy = true;

			if( !copy ) continue;

			pos[i] = pos2[i];
			q[i] = q2[i];
		}
	}

	for( int i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		// initialize bonematrix
		bonematrix = matrix3x4( pos[i], q[i] );

		if( pbones[i].parent == -1 ) 
		{
			m_pbonetransform[i] = m_protationmatrix.ConcatTransforms( bonematrix );

			// apply client-side effects to the transformation matrix
			StudioFxTransform( m_pCurrentEntity, m_pbonetransform[i] );
		}
		else
		{
			m_pbonetransform[i] = m_pbonetransform[pbones[i].parent].ConcatTransforms( bonematrix );
		}
	}
}

/*
====================
StudioSaveBones

====================
*/
void CStudioModelRenderer::StudioSaveBones( void )
{
	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	m_nCachedBones = m_pStudioHeader->numbones;

	for( int i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		Q_strcpy( m_nCachedBoneNames[i], pbones[i].name );
		m_rgCachedBoneTransform[i] = m_pbonetransform[i];
	}
}

/*
====================
StudioRestoreBones

====================
*/
void CStudioModelRenderer::StudioRestoreBones( void )
{
	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	m_nCachedBones = m_pStudioHeader->numbones;

	for( int i = 0; i < m_nCachedBones; i++ ) 
	{
		Q_strcpy( pbones[i].name, m_nCachedBoneNames[i] );
		m_pbonetransform[i] = m_rgCachedBoneTransform[i];
	}
}

/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer :: StudioMergeBones( model_t *m_pSubModel )
{
	mstudioanim_t	*panim;
	matrix3x4		bonematrix;

	static Vector	pos[MAXSTUDIOBONES];
	static Vector4D	q[MAXSTUDIOBONES];

	if( m_pCurrentEntity->curstate.sequence < 0 || m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq ) 
		m_pCurrentEntity->curstate.sequence = 0;

	// using an intermediate array for merging bones
	m_pbonetransform = m_pbonestransform;
	m_verts = g_verts;
	m_norms = g_norms;

	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	float f = StudioEstimateFrame( pseqdesc );
	panim = StudioGetAnim( m_pSubModel, pseqdesc );
	StudioCalcRotations( pos, q, pseqdesc, panim, f );

	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for( int i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		for( int j = 0; j < m_nCachedBones; j++ )
		{
			if( !Q_stricmp( pbones[i].name, m_nCachedBoneNames[j] ))
			{
				m_pbonetransform[i] = m_rgCachedBoneTransform[j];
				break;
			}
		}

		if( j >= m_nCachedBones )
		{
			// initialize bonematrix
			bonematrix = matrix3x4( pos[i], q[i] );

			if( pbones[i].parent == -1 ) 
			{
				m_pbonetransform[i] = m_protationmatrix.ConcatTransforms( bonematrix );

				// apply client-side effects to the transformation matrix
				StudioFxTransform( m_pCurrentEntity, m_pbonetransform[i] );
			}
			else
			{
				m_pbonetransform[i] = m_pbonetransform[pbones[i].parent].ConcatTransforms( bonematrix );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Computes the pose to decal plane transform 
//-----------------------------------------------------------------------------
bool CStudioModelRenderer::ComputePoseToDecal( const Vector &vecStart, const Vector &vecEnd )
{
	// Create a transform that projects world coordinates into a basis for the decal
	Vector decalU, decalV, decalN, vecDelta;
	matrix3x4	worldToDecal;

	vecDelta = vecEnd - vecStart;

	// Get the z axis
	decalN = vecDelta * -1.0f;
	if( decalN.Length() == 0.0f )
		return false;

	decalN = decalN.Normalize();

	// Deal with the u axis
	decalU = CrossProduct( Vector( 0, 0, 1 ), decalN );

	float length = decalU.Length();

	if( length < 1e-3 )
	{
		// if up parallel or antiparallel to ray, deal...
		Vector fixup( 0, 1, 0 );
		decalU = CrossProduct( fixup, decalN );

		float length = decalU.Length();
		if( length < 1e-3 )
			return false;
	}

	decalU = decalU.Normalize();
	decalV = CrossProduct( decalN, decalU );

	// Since I want world-to-decal, I gotta take the inverse of the decal
	// to world. Assuming post-multiplying column vectors, the decal to world = 
	// [ Ux Vx Nx | vecEnd[0] ]
	// [ Uy Vy Ny | vecEnd[1] ]
	// [ Uz Vz Nz | vecEnd[2] ]

	worldToDecal[0][0] = decalU.x;
	worldToDecal[1][0] = decalU.y;
	worldToDecal[2][0] = decalU.z;

	worldToDecal[0][1] = decalV.x;
	worldToDecal[1][1] = decalV.y;
	worldToDecal[2][1] = decalV.z;

	worldToDecal[0][2] = decalN.x;
	worldToDecal[1][2] = decalN.y;
	worldToDecal[2][2] = decalN.z;

	// g-cont. just invert matrix here?
	worldToDecal[3][0] = -DotProduct( vecEnd, decalU );
	worldToDecal[3][1] = -DotProduct( vecEnd, decalV );
	worldToDecal[3][2] = -DotProduct( vecEnd, decalN );

	// Compute transforms from pose space to decal plane space
	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		m_pdecaltransform[i] = worldToDecal.ConcatTransforms( m_pbonetransform[i] );
	}

	return true;
}

inline bool CStudioModelRenderer::IsFrontFacing( const Vector& norm, byte vertexBone )
{
	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to decal
	Vector decalN;

	decalN.x = m_pdecaltransform[vertexBone][0][2];
	decalN.y = m_pdecaltransform[vertexBone][1][2];
	decalN.z = m_pdecaltransform[vertexBone][2][2];

	float z = DotProduct( norm, decalN );

	return ( z >= 0.1f );
}

inline bool CStudioModelRenderer::TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, byte vertexBone, Vector2D& uv )
{
	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to world
	Vector decalU, decalV, decalN;

	decalU.x = m_pdecaltransform[vertexBone][0][0];
	decalU.y = m_pdecaltransform[vertexBone][1][0];
	decalU.z = m_pdecaltransform[vertexBone][2][0];

	decalV.x = m_pdecaltransform[vertexBone][0][1];
	decalV.y = m_pdecaltransform[vertexBone][1][1];
	decalV.z = m_pdecaltransform[vertexBone][2][1];

	decalN.x = m_pdecaltransform[vertexBone][0][2];
	decalN.y = m_pdecaltransform[vertexBone][1][2];
	decalN.z = m_pdecaltransform[vertexBone][2][2];

	uv.x = (DotProduct( pos, decalU ) + m_pdecaltransform[vertexBone][3][0]);
	uv.y = -(DotProduct( pos, decalV ) + m_pdecaltransform[vertexBone][3][1]);

	// do culling
	float z = DotProduct( pos, decalN ) + m_pdecaltransform[vertexBone][3][2];

	return ( fabs( z ) < max( build.m_Radius, 16.0f )); // optimal radius
}

void CStudioModelRenderer::ProjectDecalOntoMesh( DecalBuildInfo_t& build )
{
	float invRadius = (build.m_Radius != 0.0f) ? 1.0f / build.m_Radius : 1.0f;

	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// For this to work, the plane and intercept must have been transformed
	// into pose space. Also, we'll not be bothering with flexes.
	for( int j = 0; j < build.m_pDecalMesh->numvertices; j++ )
	{
		Vector vertex = m_arrayverts[m_arrayelems[build.m_pDecalMesh->firstvertex + j]];
		Vector normal = m_arraynorms[m_arrayelems[build.m_pDecalMesh->firstvertex + j]];
		byte vertexBone = m_vertexbone[m_arrayelems[build.m_pDecalMesh->firstvertex + j]];

		// No decal vertex yet...
		pVertexInfo[j].m_VertexIndex = 0xFFFF;

		// We need to know if the normal is pointing in the negative direction
		// if so, blow off all triangles connected to that vertex.
		pVertexInfo[j].m_FrontFacing = IsFrontFacing( normal, vertexBone );
		if( !pVertexInfo[j].m_FrontFacing )
		{
			continue;
		}

		bool inValidArea = TransformToDecalSpace( build, vertex, vertexBone, pVertexInfo[j].m_UV );
		pVertexInfo[j].m_InValidArea = inValidArea;

		pVertexInfo[j].m_UV *= invRadius * 0.5f;
		pVertexInfo[j].m_UV[0] += 0.5f;
		pVertexInfo[j].m_UV[1] += 0.5f;
	}
}

inline int CStudioModelRenderer::ComputeClipFlags( Vector2D const& uv )
{
	// Otherwise we gotta do the test
	int flags = 0;

	if( uv.x < 0.0f )
		flags |= DECAL_CLIP_MINUSU;
	else if( uv.x > 1.0f )
		flags |= DECAL_CLIP_PLUSU;

	if( uv.y < 0.0f )
		flags |= DECAL_CLIP_MINUSV;
	else if( uv.y > 1.0f )
		flags |= DECAL_CLIP_PLUSV;

	return flags;
}
//-----------------------------------------------------------------------------
// Converts a mesh index to a DecalVertex_t
//-----------------------------------------------------------------------------
void CStudioModelRenderer::ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int meshIndex, DecalVertex_t& decalVertex )
{
	// Copy over the data;
	// get the texture coords from the decal planar projection
	assert( meshIndex < MAXSTUDIOTRIANGLES );

	decalVertex.m_Position = m_arrayverts[m_arrayelems[build.m_pDecalMesh->firstvertex + meshIndex]];
	decalVertex.m_Normal = m_arraynorms[m_arrayelems[build.m_pDecalMesh->firstvertex + meshIndex]];
	decalVertex.m_TexCoord = build.m_pVertexInfo[meshIndex].m_UV;
	decalVertex.m_MeshVertexIndex = meshIndex;
	decalVertex.m_Mesh = build.m_Mesh;

	assert( decalVertex.m_Mesh < 100 );
	decalVertex.m_Model = build.m_Model;
	decalVertex.m_Body = build.m_Body;
	decalVertex.m_Bone = m_vertexbone[m_arrayelems[build.m_pDecalMesh->firstvertex + meshIndex]];
}

//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices for this material
//-----------------------------------------------------------------------------
inline word CStudioModelRenderer::AddVertexToDecal( DecalBuildInfo_t& build, int meshIndex )
{
	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// If we've never seen this vertex before, we need to add a new decal vert
	if( pVertexInfo[meshIndex].m_VertexIndex == 0xFFFF )
	{
		DecalVertexList_t& decalVertexList = build.m_pDecalMaterial->m_Vertices;
		int v = decalVertexList.AddToTail();

		// Copy over the data;
		ConvertMeshVertexToDecalVertex( build, meshIndex, build.m_pDecalMaterial->m_Vertices[v] );

#ifdef _DEBUG
		// Make sure clipped vertices are in the right range...
		if( build.m_UseClipVert )
		{
			assert(( decalVertexList[v].m_TexCoord[0] >= -1e-3 ) && ( decalVertexList[v].m_TexCoord[0] - 1.0f < 1e-3 ));
			assert(( decalVertexList[v].m_TexCoord[1] >= -1e-3 ) && ( decalVertexList[v].m_TexCoord[1] - 1.0f < 1e-3 ));
		}
#endif

		// Store off the index of this vertex so we can reference it again
		pVertexInfo[meshIndex].m_VertexIndex = build.m_VertexCount;
		++build.m_VertexCount;
		if( build.m_FirstVertex == decalVertexList.InvalidIndex( ))
			build.m_FirstVertex = v;
	}

	return pVertexInfo[meshIndex].m_VertexIndex;
}

//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices for this material
//-----------------------------------------------------------------------------
inline word CStudioModelRenderer::AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert )
{
	// This creates a unique vertex
	DecalVertexList_t& decalVertexList = build.m_pDecalMaterial->m_Vertices;

	// Try to see if the clipped vertex already exists in our decal list...
	// Only search for matches with verts appearing in the current decal
	word vertexCount = 0;

	for( word i = build.m_FirstVertex; i != decalVertexList.InvalidIndex(); i = decalVertexList.Next(i), ++vertexCount )
	{
		// Only bother to check against clipped vertices
		if( decalVertexList[i].GetMesh( build.m_pStudioHeader ))
			continue;

		// They must have the same position, and normal
		// texcoord will fall right out if the positions match
		Vector temp = decalVertexList[i].m_Position - vert.m_Position;
		if(( fabs( temp[0] ) > 1e-3 ) || (fabs( temp[1] ) > 1e-3 ) || ( fabs( temp[2] ) > 1e-3 ))
			continue;

		temp = decalVertexList[i].m_Normal - vert.m_Normal;
		if(( fabs( temp[0] ) > 1e-3 ) || ( fabs( temp[1] ) > 1e-3 ) || ( fabs( temp[2] ) > 1e-3 ))
			continue;

		return vertexCount;
	}

	// This path is the path taken by clipped vertices
	assert( (vert.m_TexCoord[0] >= -1e-3) && (vert.m_TexCoord[0] - 1.0f < 1e-3) );
	assert( (vert.m_TexCoord[1] >= -1e-3) && (vert.m_TexCoord[1] - 1.0f < 1e-3) );

	// Must create a new vertex...
	word idx = decalVertexList.AddToTail( vert );
	if( build.m_FirstVertex == decalVertexList.InvalidIndex( ))
		build.m_FirstVertex = idx;

	assert( vertexCount == build.m_VertexCount );

	return build.m_VertexCount++;
}


//-----------------------------------------------------------------------------
// Adds the clipped triangle to the decal
//-----------------------------------------------------------------------------
void CStudioModelRenderer::AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState )
{
	// FIXME: Clipped vertices will almost always be shared. We
	// need a way of associating clipped vertices with edges so we can share
	// the clipped vertices quickly
	assert( clipState.m_VertCount <= 7 );

	// Yeah baby yeah!!	Add this sucka
	int i;
	word indices[7];
	for( i = 0; i < clipState.m_VertCount; i++ )
	{
		// First add the vertices
		int vertIdx = clipState.m_Indices[clipState.m_Pass][i];

		if( vertIdx < 3 )
		{
			indices[i] = AddVertexToDecal( build, clipState.m_ClipVerts[vertIdx].m_MeshVertexIndex );
		}
		else
		{
			indices[i] = AddVertexToDecal( build, clipState.m_ClipVerts[vertIdx] );
		}
	}

	// Add a trifan worth of triangles
	for( i = 1; i < clipState.m_VertCount - 1; i++ )
	{
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[0] );
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[i] );
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[i+1] );
	}
}

//-----------------------------------------------------------------------------
// Creates a new vertex where the edge intersects the plane
//-----------------------------------------------------------------------------
int CStudioModelRenderer::IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val )
{
	DecalVertex_t& startVert = state.m_ClipVerts[start];
	DecalVertex_t& endVert = state.m_ClipVerts[end];

	Vector2D dir = endVert.m_TexCoord - startVert.m_TexCoord;

	assert( dir[normalInd] != 0.0f );
	float t = (val - startVert.m_TexCoord[normalInd]) / dir[normalInd];
				 
	// Allocate a clipped vertex
	DecalVertex_t& out = state.m_ClipVerts[state.m_ClipVertCount];
	int newVert = state.m_ClipVertCount++;

	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// the clipped vertex has no analogue in the original mesh
	out.m_MeshVertexIndex = 0xFFFF;
	out.m_Mesh = 0xFFFF;
	out.m_Model = 0xFFFF;
	out.m_Body = 0xFFFF;
	out.m_Bone = 0xFF;

	if( startVert.m_Bone == endVert.m_Bone )
		out.m_Bone = startVert.m_Bone;

	if( out.m_Bone == 0xFF )
	{
		for( int i = startVert.m_Bone; i != -1; i = pbones[i].parent )
		{
			if( i == endVert.m_Bone )
			{
				out.m_Bone = endVert.m_Bone;
				break;
			}
		}
	}

	if( out.m_Bone == 0xFF )
	{
		for( int i = endVert.m_Bone; i != -1; i = pbones[i].parent )
		{
			if( i == startVert.m_Bone )
			{
				out.m_Bone = startVert.m_Bone;
				break;
			}
		}
	}

	if( out.m_Bone == 0xFF )
	{
		out.m_Bone = endVert.m_Bone;
	}

	// Interpolate position
	out.m_Position = startVert.m_Position * (1.0f - t) + endVert.m_Position * t;

	// Interpolate normal
	out.m_Normal = startVert.m_Position * (1.0f - t) + endVert.m_Position * t;
	out.m_Normal = out.m_Normal.Normalize();

	// Interpolate texture coord
	out.m_TexCoord = startVert.m_TexCoord + (endVert.m_TexCoord - startVert.m_TexCoord) * t;

	// Compute the clip flags baby...
	state.m_ClipFlags[newVert] = ComputeClipFlags( out.m_TexCoord );

	return newVert;
}

//-----------------------------------------------------------------------------
// Clips a triangle against a plane, use clip flags to speed it up
//-----------------------------------------------------------------------------
void CStudioModelRenderer::ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val )
{
	// FIXME: Could compute the & of all the clip flags of all the verts
	// as we go through the loop to do another early out

	// Ye Olde Sutherland-Hodgman clipping algorithm
	int outVertCount = 0;
	int start = state.m_Indices[state.m_Pass][state.m_VertCount - 1];
	bool startInside = (state.m_ClipFlags[start] & flag) == 0;
	for (int i = 0; i < state.m_VertCount; ++i)
	{
		int end = state.m_Indices[state.m_Pass][i];

		bool endInside = (state.m_ClipFlags[end] & flag) == 0;
		if( endInside )
		{
			if (!startInside)
			{
				int clipVert = IntersectPlane( state, start, end, normalInd, val );
				state.m_Indices[!state.m_Pass][outVertCount++] = clipVert;
			}
			state.m_Indices[!state.m_Pass][outVertCount++] = end;
		}
		else
		{
			if (startInside)
			{
				int clipVert = IntersectPlane( state, start, end, normalInd, val );
				state.m_Indices[!state.m_Pass][outVertCount++] = clipVert;
			}
		}
		start = end;
		startInside = endInside;
	}

	state.m_Pass = !state.m_Pass;
	state.m_VertCount = outVertCount;
}

//-----------------------------------------------------------------------------
// Clips the triangle to +/- radius
//-----------------------------------------------------------------------------
bool CStudioModelRenderer::ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags )
{
	int i;

	DecalClipState_t clipState;
	clipState.m_VertCount = 3;

	ConvertMeshVertexToDecalVertex( build, i1, clipState.m_ClipVerts[0] );
	ConvertMeshVertexToDecalVertex( build, i2, clipState.m_ClipVerts[1] );
	ConvertMeshVertexToDecalVertex( build, i3, clipState.m_ClipVerts[2] );

	clipState.m_ClipVertCount = 3;

	for( i = 0; i < 3; i++ )
	{
		clipState.m_ClipFlags[i] = pClipFlags[i];
		clipState.m_Indices[0][i] = i;
	}

	clipState.m_Pass = 0;

	// Clip against each plane
	ClipTriangleAgainstPlane( clipState, 0, DECAL_CLIP_MINUSU, 0.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	ClipTriangleAgainstPlane( clipState, 0, DECAL_CLIP_PLUSU, 1.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	ClipTriangleAgainstPlane( clipState, 1, DECAL_CLIP_MINUSV, 0.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	ClipTriangleAgainstPlane( clipState, 1, DECAL_CLIP_PLUSV, 1.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	// Only add the clipped decal to the triangle if it's one bone
	// otherwise just return if it was clipped
	if( build.m_UseClipVert )
	{
		AddClippedDecalToTriangle( build, clipState );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Adds a decal to a triangle, but only if it should
//-----------------------------------------------------------------------------
void CStudioModelRenderer::AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 )
{
	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// All must be front-facing for a decal to be added
	// FIXME: Could make it work if not all are front-facing, need clipping for that
	if(( !pVertexInfo[i1].m_FrontFacing ) || ( !pVertexInfo[i2].m_FrontFacing ) || ( !pVertexInfo[i3].m_FrontFacing ))
	{
		return;
	}

	// This is used to prevent poke through; if the points are too far away
	// from the contact point, then don't add the decal
	if(( !pVertexInfo[i1].m_InValidArea ) && ( !pVertexInfo[i2].m_InValidArea ) && ( !pVertexInfo[i3].m_InValidArea ))
	{
		return;
	}

	// Clip to +/- radius
	int clipFlags[3];

	clipFlags[0] = ComputeClipFlags( pVertexInfo[i1].m_UV );
	clipFlags[1] = ComputeClipFlags( pVertexInfo[i2].m_UV );
	clipFlags[2] = ComputeClipFlags( pVertexInfo[i3].m_UV );

	// Cull... The result is non-zero if they're all outside the same plane
	if(( clipFlags[0] & ( clipFlags[1] & clipFlags[2] )) != 0 )
		return;

	bool doClip = true;
	
	// Trivial accept for skinned polys... if even one vert is inside
	// the draw region, accept
	if(( !build.m_UseClipVert ) && ( !clipFlags[0] || !clipFlags[1] || !clipFlags[2] ))
		doClip = false;

	// Trivial accept... no clip flags set means all in
	// Don't clip if we have more than one bone... we'll need to do skinning
	// and we can't clip the bone indices
	// We *do* want to clip in the one bone case though; useful for large
	// static props.
	if( doClip && ( clipFlags[0] || clipFlags[1] || clipFlags[2] ))
	{
		bool validTri = ClipDecal( build, i1, i2, i3, clipFlags );

		// Don't add the triangle if we culled the triangle or if 
		// we had one or less bones
		if( build.m_UseClipVert || ( !validTri ))
			return;
	}

	// Add the vertices to the decal since there was no clipping
	build.m_pDecalMaterial->m_Indices.AddToTail( AddVertexToDecal( build, i1 ));
	build.m_pDecalMaterial->m_Indices.AddToTail( AddVertexToDecal( build, i2 ));
	build.m_pDecalMaterial->m_Indices.AddToTail( AddVertexToDecal( build, i3 ));
}

void CStudioModelRenderer::AddDecalToMesh( DecalBuildInfo_t& build )
{
	build.m_pVertexInfo = (DecalVertexInfo_t *)malloc( build.m_pDecalMesh->numvertices * sizeof( DecalVertexInfo_t ));

	// project all vertices for this group into decal space
	// Note we do this work at a mesh level instead of a model level
	// because vertices are not shared across mesh boundaries
	ProjectDecalOntoMesh( build );

	for( int j = 0; j < build.m_pDecalMesh->numvertices; j += 3 )
	{
		int i1 = j + 0;//m_arrayelems[build.m_pDecalMesh->firstvertex + j + 0];
		int i2 = j + 1;//m_arrayelems[build.m_pDecalMesh->firstvertex + j + 1];
		int i3 = j + 2;//m_arrayelems[build.m_pDecalMesh->firstvertex + j + 2];

		AddTriangleToDecal( build, i1, i2, i3 );
	}
}

void CStudioModelRenderer::AddDecalToModel( DecalBuildInfo_t& buildInfo )
{
	// FIXME: We need to do some high-level culling to figure out exactly
	// which meshes we need to add the decals to
	// Turns out this solution may also be good for mesh sorting
	// we need to know the center of each mesh, could also store a
	// bounding radius for each mesh and test the ray against each sphere.
	Vector *pstudioverts = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	Vector *pstudionorms = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	byte *pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	mstudiotexture_t	*ptexture;
	short		*pskinref;
	int 		i, numVerts;

	m_nNumArrayVerts = m_nNumArrayElems = 0;

	DecalMesh_t *pDecalMesh = (DecalMesh_t *)malloc( m_pSubModel->nummesh * sizeof( DecalMesh_t ));
	ptexture = STUDIO_GET_TEXTURE( m_pCurrentEntity );
	pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);

	// build all the data for current submodel
	for( i = 0; i < m_pSubModel->nummesh; i++ ) 
	{
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + i;
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
		DecalMesh_t *pCurMesh = pDecalMesh + i;

		// looks ugly in any way, so skip them
		if( ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_TRANSPARENT )
		{
			pCurMesh->numvertices = 0;
			continue;
                    }

		pCurMesh->firstvertex = m_nNumArrayElems;
		pCurMesh->numvertices = m_nNumArrayElems;

		while( numVerts = *( ptricmds++ ))
		{
			int	vertexState = 0;
			qboolean	tri_strip = true;

			if( numVerts < 0 )
			{
				tri_strip = false;
				numVerts = -numVerts;
			}

			for( ; numVerts > 0; numVerts--, ptricmds += 4 )
			{
				// build in indices
				if( vertexState++ < 3 )
				{
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}
				else if( tri_strip )
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

				m_arrayverts[m_nNumArrayVerts] = pstudioverts[ptricmds[0]];
				m_arraynorms[m_nNumArrayVerts] = pstudionorms[ptricmds[1]];
				m_vertexbone[m_nNumArrayVerts] = pvertbone[ptricmds[0]];

				m_nNumArrayVerts++;
			}
		}
		pCurMesh->numvertices = m_nNumArrayElems - pCurMesh->numvertices;
	}

	for( i = 0; i < m_pSubModel->nummesh; i++ )
	{
		buildInfo.m_Mesh = i;
		buildInfo.m_pDecalMesh = pDecalMesh + i;
		buildInfo.m_pMesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + i;

		AddDecalToMesh( buildInfo );
	}

	free( pDecalMesh );
}

//-----------------------------------------------------------------------------
// It's not valid if the model index changed + we have non-zero instance data
//-----------------------------------------------------------------------------
bool CStudioModelRenderer::IsModelInstanceValid( word handle, bool skipDecals )
{
	ModelInstance_t *inst = &m_ModelInstances[handle];

	if( !skipDecals && inst->m_DecalHandle == INVALID_HANDLE )
		return true;

	const model_t *pModel;

	if( !m_fDrawViewModel && UTIL_IsPlayer( inst->m_pEntity->curstate.number ))
		pModel = IEngineStudio.SetupPlayerModel( inst->m_pEntity->curstate.number - 1 );
	else
		pModel = inst->m_pEntity->model;

	return inst->m_pModel == pModel;
}

//-----------------------------------------------------------------------------
// Gets the list of triangles for a particular material and lod
//-----------------------------------------------------------------------------
int CStudioModelRenderer::GetDecalMaterial( DecalModelList_t& decalList, int decalTexture )
{
	for( word j = decalList.m_FirstMaterial; j != m_DecalMaterial.InvalidIndex(); j = m_DecalMaterial.Next( j ))
	{
		if( m_DecalMaterial[j].decalTexture == decalTexture )
		{
			return j;
		}
	}

	// If we got here, this must be the first time we saw this material
	j = m_DecalMaterial.Alloc( true );
	
	// Link it into the list of data
	if( decalList.m_FirstMaterial != m_DecalMaterial.InvalidIndex( ))
		m_DecalMaterial.LinkBefore( decalList.m_FirstMaterial, j );
	decalList.m_FirstMaterial = j;

	m_DecalMaterial[j].decalTexture = decalTexture;

	return j;
}

//-----------------------------------------------------------------------------
// Removes a decal and associated vertices + indices from the history list
//-----------------------------------------------------------------------------
void CStudioModelRenderer::RetireDecal( DecalHistoryList_t& historyList )
{
	assert( historyList.Count() );
	DecalHistory_t& decalHistory = historyList[ historyList.Head() ];

	// Find the decal material for the decal to remove
	DecalMaterial_t& material = m_DecalMaterial[decalHistory.m_Material];

	DecalVertexList_t& vertices = material.m_Vertices;
	Decal_t& decalToRemove = material.m_Decals[decalHistory.m_Decal];
	
	// Now clear out the vertices referenced by the indices....
	word next; 
	word vert = vertices.Head();
	assert( vertices.Count() >= decalToRemove.m_VertexCount );

	int vertsToRemove = decalToRemove.m_VertexCount;

	while( vertsToRemove > 0 )
	{
		// blat out the vertices
		next = vertices.Next( vert );
		vertices.Remove( vert );
		vert = next;

		--vertsToRemove;
	}

	// FIXME: This does a memmove. How expensive is it?
	material.m_Indices.RemoveMultiple( 0, decalToRemove.m_IndexCount );

	// Remove the decal
	material.m_Decals.Remove( decalHistory.m_Decal );

	// Clear the decal out of the history
	historyList.Remove( historyList.Head() );
}

inline bool CStudioModelRenderer::ShouldRetireDecal( DecalMaterial_t* pDecalMaterial, DecalHistoryList_t const& decalHistory )
{
	// Check to see if we should retire the decal
	return ( decalHistory.Count() >= 50 ) || (pDecalMaterial->m_Indices.Count() > 2048 );
}

int CStudioModelRenderer::AddDecalToMaterialList( DecalMaterial_t* pMaterial )
{
	DecalList_t& decalList = pMaterial->m_Decals;
	return decalList.AddToTail();
}
	
/*
====================
StudioDecalShoot

NOTE: decalTexture is gl texture index
====================
*/
void CStudioModelRenderer::StudioDecalShoot( const Vector &vecStart, const Vector &vecEnd, int decalTexture, cl_entity_t *ent, int flags, modelstate_t *state )
{
	if( !g_fRenderInitialized )
		return;

	if( !ent || !IEngineStudio.Mod_Extradata( ent->model ))
		return;

	if( ent == gEngfuncs.GetViewModel( ))
		return;	// no decals for viewmodel

	SET_CURRENT_ENTITY( ent );
	m_fDoInterp = true;

	if( UTIL_IsPlayer( ent->curstate.number ))
		m_pRenderModel = IEngineStudio.SetupPlayerModel( ent->curstate.number - 1 );
	else
		m_pRenderModel = ent->model;

	if( !m_pRenderModel )
		return;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );

	if( m_pStudioHeader->numbodyparts == 0 )
		return;

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	entity_state_t savestate = m_pCurrentEntity->curstate;

	if( m_pCvarHiModels->value && m_pRenderModel != m_pCurrentEntity->model )
	{
		// show highest resolution multiplayer model
		m_pCurrentEntity->curstate.body = 255;
	}

	m_fDoInterp = false;

	m_pCurrentEntity->curstate.sequence = state->sequence;
	m_pCurrentEntity->curstate.frame = (float)state->frame * (1.0f / 8.0f);
	m_pCurrentEntity->curstate.blending[0] = state->blending[0];
	m_pCurrentEntity->curstate.blending[1] = state->blending[1];
	m_pCurrentEntity->curstate.controller[0] = state->controller[0];
	m_pCurrentEntity->curstate.controller[1] = state->controller[1];
	m_pCurrentEntity->curstate.controller[2] = state->controller[2];
	m_pCurrentEntity->curstate.controller[3] = state->controller[3];

	if( flags & FDECAL_LOCAL_SPACE )
	{
		// make sure what model is in local space
		m_pCurrentEntity->origin = g_vecZero;
		m_pCurrentEntity->angles = g_vecZero;
	}

	// setup bones
	StudioSetUpTransform();
	StudioSetupBones( );

	// bones are set, so we can restore original state
	m_pCurrentEntity->curstate = savestate;

	if( !ComputePoseToDecal( vecStart, vecEnd ))
		return;

	// create instance for store decals personal for each entity
	if( ent->modelhandle == INVALID_HANDLE )
		ent->modelhandle = CreateInstance( ent );

	if( ent->modelhandle == INVALID_HANDLE )
		return; // out of memory?

	float w = (float)RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, decalTexture ) * 0.5f;
	float h = (float)RENDER_GET_PARM( PARM_TEX_SRC_HEIGHT, decalTexture ) * 0.5f;
 	float radius = ((w > h) ? w : h) * 0.6f; // scale decals so the look same as on world geometry
	flags |= FDECAL_LOCAL_SPACE; // now it's in local space
 
	ModelInstance_t& inst = m_ModelInstances[ent->modelhandle];

	if( !IsModelInstanceValid( ent->modelhandle ))
	{
		DestroyDecalList( inst.m_DecalHandle );
		inst.m_DecalHandle = INVALID_HANDLE;
	}

	if( inst.m_DecalHandle == INVALID_HANDLE )
	{
		// allocate new decallist
		inst.m_DecalHandle = CreateDecalList();
	}

	// This sucker is state needed only when building decals
	DecalBuildInfo_t buildInfo;
	buildInfo.m_Radius = radius;
	buildInfo.m_pStudioHeader = m_pStudioHeader;
	buildInfo.m_UseClipVert = (flags & FDECAL_CLIPTEST) ? true : false;

	DecalModelList_t& list = m_DecalList[inst.m_DecalHandle];
	int materialIdx = GetDecalMaterial( list, decalTexture );
	buildInfo.m_pDecalMaterial = &m_DecalMaterial[materialIdx];

	// Check to see if we should retire the decal
	while( ShouldRetireDecal( buildInfo.m_pDecalMaterial, list.m_DecalHistory ))
	{
		RetireDecal( list.m_DecalHistory );
	}

	buildInfo.m_FirstVertex = buildInfo.m_pDecalMaterial->m_Vertices.InvalidIndex();
	buildInfo.m_VertexCount = 0;

	int prevIndexCount = buildInfo.m_pDecalMaterial->m_Indices.Count();

	// Step over all body parts + add decals to em all!
	for( int k = 0; k < m_pStudioHeader->numbodyparts; k++ ) 
	{
		// Grab the model for this body part
		int model = StudioSetupModel( k, (void **)&m_pBodyPart, (void **)&m_pSubModel );
		buildInfo.m_Body = k;
		buildInfo.m_Model = model;

		AddDecalToModel( buildInfo );
	}

	// Add this to the list of decals in this material
	if( buildInfo.m_VertexCount )
	{
		int decalIndexCount = buildInfo.m_pDecalMaterial->m_Indices.Count() - prevIndexCount;
		assert( decalIndexCount > 0 );

		int decalIndex = AddDecalToMaterialList( buildInfo.m_pDecalMaterial );
		Decal_t& decal = buildInfo.m_pDecalMaterial->m_Decals[decalIndex];
		decal.m_VertexCount = buildInfo.m_VertexCount;
		decal.m_IndexCount = decalIndexCount;

		decal.state = *state;
		decal.depth = inst.m_DecalCount++;
		decal.vecLocalStart = m_protationmatrix.VectorITransform( vecStart );
		decal.vecLocalEnd = m_protationmatrix.VectorITransform( vecEnd );
		decal.flags = (byte)flags;
		
		// Add this decal to the history...
		int h = list.m_DecalHistory.AddToTail();
		list.m_DecalHistory[h].m_Material = materialIdx;
		list.m_DecalHistory[h].m_Decal = decalIndex;
	}
}

int CStudioModelRenderer::StudioDecalList( decallist_t *pBaseList, int count, qboolean changelevel )
{
	if( !g_fRenderInitialized )
		return 0;

	int maxStudioDecals = MAX_STUDIO_DECALS + (MAX_STUDIO_DECALS - count);
	decallist_t *pList = pBaseList + count;	// shift list to first free slot
	int total = 0;

	cl_entity_t *pEntity = NULL;
	char decalname[64];

	for( int i = 0; i < m_ModelInstances.Count(); i++ )
          {
		word decalHandle = m_ModelInstances[i].m_DecalHandle;

		if( decalHandle == INVALID_HANDLE )
			continue;	// decal list is removed

		DecalModelList_t const& list = m_DecalList[decalHandle];
		word decalMaterial = m_DecalList[decalHandle].m_FirstMaterial;

		// setup the decal entity
		pEntity = m_ModelInstances[i].m_pEntity;

		for( word mat = list.m_FirstMaterial; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next( mat ))
		{
			DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];

			// setup the decal texture
			Q_strncpy( decalname, GET_TEXTURE_NAME( decalMaterial.decalTexture ), sizeof( decalname ));
			word decal = decalMaterial.m_Decals.Head();

			while( decal != decalMaterial.m_Decals.InvalidIndex( ))
			{
				Decal_t *pdecal = &decalMaterial.m_Decals[decal];

				if(!( pdecal->flags & FDECAL_DONTSAVE ))	
				{
					pList[total].depth = pdecal->depth;
					pList[total].flags = pdecal->flags|FDECAL_STUDIO;
					pList[total].entityIndex = pEntity->index;
					pList[total].studio_state = pdecal->state;
					pList[total].position = pdecal->vecLocalEnd;
					pList[total].impactPlaneNormal = pdecal->vecLocalStart;
					COM_FileBase( decalname, pList[total].name );
					total++;
				}

				// check for list overflow
				if( total >= maxStudioDecals )
				{
					ALERT( at_error, "StudioDecalList: too many studio decals on save\restore\n" );
					goto end_serialize;
				}
				decal = decalMaterial.m_Decals.Next( decal ); 
			}
		}
	}
end_serialize:

	return total;
}

//-----------------------------------------------------------------------------
// Purpose: Removes all the decals on a model instance
//-----------------------------------------------------------------------------
void CStudioModelRenderer::RemoveAllDecals( int entityIndex )
{
	if( !g_fRenderInitialized ) return;

	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( entityIndex );

	if( !ent || ent->modelhandle == INVALID_HANDLE )
		return;

	ModelInstance_t& inst = m_ModelInstances[ent->modelhandle];
	if( !IsModelInstanceValid( ent->modelhandle )) return;
	if( inst.m_DecalHandle == INVALID_HANDLE ) return;

	DestroyDecalList( inst.m_DecalHandle );
	inst.m_DecalHandle = INVALID_HANDLE;
}

void CStudioModelRenderer::ComputeDecalTransform( DecalMaterial_t& decalMaterial )
{
	DecalVertexList_t& verts = decalMaterial.m_Vertices; 
	m_nNumArrayVerts = m_nNumArrayElems = 0;
	byte alpha = 255;

	if( m_pCurrentEntity->curstate.rendermode == kRenderTransTexture )
		alpha = m_pCurrentEntity->curstate.renderamt;

	for( word i = verts.Head(); i != verts.InvalidIndex(); i = verts.Next( i ))
	{
		DecalVertex_t& vertex = verts[i];
		int bone = vertex.m_Bone;
		Vector vertexLight;

		// NOTE: studiolights used local coordinates for bonelighting so we pass untransformed normal
		StudioLighting( vertexLight, bone, 0, vertex.m_Normal );

		m_arrayverts[m_nNumArrayVerts] = m_pbonetransform[bone].VectorTransform( vertex.m_Position );
		m_arraycoord[m_nNumArrayVerts] = vertex.m_TexCoord;
		m_arraycolor[m_nNumArrayVerts][0] = vertexLight.x * 255;
		m_arraycolor[m_nNumArrayVerts][1] = vertexLight.y * 255;
		m_arraycolor[m_nNumArrayVerts][2] = vertexLight.z * 255;
		m_arraycolor[m_nNumArrayVerts][3] = alpha; // fade decals when fade corpse 
		m_nNumArrayVerts++;	
	}
}

//-----------------------------------------------------------------------------
// Draws all the decals using a particular material
//-----------------------------------------------------------------------------
void CStudioModelRenderer::DrawDecalMaterial( DecalMaterial_t& decalMaterial, studiohdr_t *pStudioHdr )
{
	// It's possible for the index count to become zero due to decal retirement
	int indexCount = decalMaterial.m_Indices.Count();
	if( indexCount == 0 ) return;

	int vertexCount = decalMaterial.m_Vertices.Count();

	ComputeDecalTransform( decalMaterial );

	// Set the indices
	// This is a little tricky. Because we can retire decals, the indices
	// for each decal start at 0. We output all the vertices in order of
	// each decal, and then fix up the indices based on how many vertices
	// we wrote out for the decals
	word decal = decalMaterial.m_Decals.Head();
	int indicesRemaining = decalMaterial.m_Decals[decal].m_IndexCount;
	int vertexOffset = 0;
	for( int i = 0; i < indexCount; i++ )
	{
		m_arrayelems[m_nNumArrayElems] = decalMaterial.m_Indices[i] + vertexOffset; 
		m_nNumArrayElems++;

		if( --indicesRemaining <= 0 )
		{
			vertexOffset += decalMaterial.m_Decals[decal].m_VertexCount;
			decal = decalMaterial.m_Decals.Next( decal ); 

			if( decal != decalMaterial.m_Decals.InvalidIndex( ))
			{
				indicesRemaining = decalMaterial.m_Decals[decal].m_IndexCount;
			}
#ifdef _DEBUG
			else
			{
				assert( i + 1 == indexCount );
			}
#endif
		}
	}

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, m_nNumArrayVerts, m_nNumArrayElems, GL_UNSIGNED_INT, m_arrayelems );
	else pglDrawElements( GL_TRIANGLES, m_nNumArrayElems, GL_UNSIGNED_INT, m_arrayelems );
}

//-----------------------------------------------------------------------------
// Draws all the decals on a particular model
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: DrawDecal( word handle, studiohdr_t *pStudioHdr )
{
	if( handle == INVALID_HANDLE )
		return;

	// All decal vertex data is are stored in pose space
	// So as long as the pose-to-world transforms are set, we're all ready!

	// Get the decal list for this lod
	DecalModelList_t const& list = m_DecalList[handle];
	m_pStudioHeader = pStudioHdr;

	if( m_pCurrentEntity->curstate.rendermode == kRenderNormal || m_pCurrentEntity->curstate.rendermode == kRenderTransAlpha )
	{
		pglDepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
	}

	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	pglEnable( GL_POLYGON_OFFSET_FILL );
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 12, m_arrayverts );

	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, 8, m_arraycoord );

	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_arraycolor );

	int lastTexture = -1;

	// Draw each set of decals using a particular material
	for( word mat = list.m_FirstMaterial; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next( mat ))
	{
		DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];

		if( lastTexture != decalMaterial.decalTexture )
		{
			// bind the decal texture
			GL_Bind( GL_TEXTURE0, decalMaterial.decalTexture );
			pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			R_SetupOverbright( true );
		}

		DrawDecalMaterial( decalMaterial, pStudioHdr );

		if( lastTexture != decalMaterial.decalTexture )
		{
			R_SetupOverbright( false );
			lastTexture = decalMaterial.decalTexture;
		}
	}

	pglDisableClientState( GL_COLOR_ARRAY );
	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	Vector origin = m_protationmatrix.GetOrigin();

	if( m_fHasProjectionLighting )
	{
		for( int i = 0; i < MAX_PLIGHTS; i++ )
		{
			plight_t *pl = &cl_plights[i];

			if( pl->die < GET_CLIENT_TIME() || !pl->radius )
				continue;

			float dist = (pl->origin - origin).Length();

			if( !dist || dist > ( pl->radius + studio_radius ))
				continue;

			if( R_CullSphereExt( pl->frustum, origin, studio_radius, pl->clipflags ))
				continue;

			lastTexture = -1;

			R_BeginDrawProjection( pl, true );

			for( word mat = list.m_FirstMaterial; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next( mat ))
			{
				DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];

				if( lastTexture != decalMaterial.decalTexture )
				{
					// bind the decal texture
					GL_Bind( GL_TEXTURE3, decalMaterial.decalTexture );
					lastTexture = decalMaterial.decalTexture;
				}

				pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
				pglTexCoordPointer( 2, GL_FLOAT, 8, m_arraycoord );
				DrawDecalMaterial( decalMaterial, pStudioHdr );
				pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
			}

			R_EndDrawProjection();
		}

		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	if( m_pCurrentEntity->curstate.rendermode == kRenderNormal || m_pCurrentEntity->curstate.rendermode == kRenderTransAlpha )
	{
		pglDepthMask( GL_TRUE );
		pglDisable( GL_BLEND );
	}

	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisable( GL_POLYGON_OFFSET_FILL );

	// restore blendfunc here
	if( m_pCurrentEntity->curstate.rendermode == kRenderTransAdd || m_pCurrentEntity->curstate.rendermode == kRenderGlow )
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
}

/*
====================
StudioSetupChrome

====================
*/
void CStudioModelRenderer::StudioSetupChrome( float *pchrome, int bone, Vector normal )
{
	float	n;

	if( m_chromeAge[bone] != m_nStudioModelCount )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		Vector	chromeupvec;	// g_chrome t vector in world reference frame
		Vector	chromerightvec;	// g_chrome s vector in world reference frame
		Vector	tmp, v_left;	// vector pointing at bone in world reference frame

		m_pbonetransform[bone].GetOrigin( tmp );
		tmp = (-RI.vieworg + tmp).Normalize();
		v_left = -RI.vright;

		if( m_nForceFaceFlags & STUDIO_NF_CHROME )
		{
			float angle, sr, cr;
			int i;

			angle = anglemod( m_clTime * 40 ) * (M_PI * 2.0f / 360.0f);
			SinCos( angle, &sr, &cr );

			for( i = 0; i < 3; i++ )
			{
				chromerightvec[i] = (v_left[i] * cr + RI.vup[i] * sr);
				chromeupvec[i] = v_left[i] * -sr + RI.vup[i] * cr;
			}
		}
		else
		{
			chromeupvec = CrossProduct( tmp, v_left ).Normalize();
			chromerightvec = CrossProduct( tmp, chromeupvec ).Normalize();
			chromeupvec = -chromeupvec;	// GoldSrc rules
		}

		m_chromeUp[bone] = m_pbonetransform[bone].VectorIRotate( chromeupvec );
		m_chromeRight[bone] = m_pbonetransform[bone].VectorIRotate( chromerightvec );
		m_chromeAge[bone] = m_nStudioModelCount;
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
void CStudioModelRenderer::StudioCalcAttachments( void )
{
	if( m_pCurrentEntity->modelhandle == INVALID_HANDLE )
		return; // too early ?

	StudioAttachment_t *att = m_ModelInstances[m_pCurrentEntity->modelhandle].attachment;
	mstudioattachment_t *pattachment;

	if( m_pStudioHeader->numattachments <= 0 )
	{
		// clear attachments
		for( int i = 0; i < MAXSTUDIOATTACHMENTS; i++ )
			m_pCurrentEntity->attachment[i] = att[i].pos = m_pCurrentEntity->origin;
		return;
	}
	else if( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		ALERT( at_error, "Too many attachments on %s\n", m_pCurrentEntity->model->name );
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);

	for( int i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		Vector p0 = m_pbonetransform[pattachment[i].bone].VectorTransform( pattachment[i].org );
		Vector p1 = m_pbonetransform[pattachment[i].bone].GetOrigin();

		// merge attachments position for viewmodel
		if( m_fDrawViewModel )
		{
			StudioFormatAttachment( p0, false );
			StudioFormatAttachment( p1, false );
		}

		att[i].pos = m_pCurrentEntity->attachment[i] = p0;
		att[i].dir = (p0 - p1).Normalize(); // forward vec
	}
}

/*
====================
StudioGetAttachment

====================
*/
void CStudioModelRenderer :: StudioGetAttachment( const cl_entity_t *ent, int iAttachment, Vector *pos, Vector *dir )
{
	if( !ent || !ent->model || ( !pos && !dir  )) return;

	studiohdr_t *phdr = (studiohdr_t *)IEngineStudio.Mod_Extradata( ent->model );
	if( !phdr ) return;

	if( ent->modelhandle == INVALID_HANDLE )
		return; // too early ?

	ModelInstance_t *inst = &m_ModelInstances[ent->modelhandle];

	// make sure we not overrun
	iAttachment = bound( 0, iAttachment, phdr->numattachments );

	if( pos ) *pos = inst->attachment[iAttachment].pos;
	if( dir ) *dir = inst->attachment[iAttachment].dir;
}

/*
====================
StudioSetupModel

====================
*/
int CStudioModelRenderer::StudioSetupModel( int bodypart, void **ppbodypart, void **ppsubmodel )
{
	mstudiobodyparts_t *pbodypart;
	mstudiomodel_t *psubmodel;
	int index;

	if( bodypart > m_pStudioHeader->numbodyparts )
		bodypart = 0;
	m_iBodyPartIndex = bodypart;

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
StudioCheckBBox

===============
*/
int CStudioModelRenderer::StudioCheckBBox( void )
{
	if( m_pCurrentEntity == GET_VIEWMODEL() && m_pCvarHand->value >= HIDE_WEAPON )
		return false; // hidden

	if( !StudioComputeBBox( m_pCurrentEntity, NULL ))
		return false; // invalid sequence

	Vector modelpos;

	// NOTE: extract real drawing origin from rotation matrix
	m_protationmatrix.GetOrigin( modelpos );

	if( R_CullModel( m_pCurrentEntity, modelpos, studio_mins, studio_maxs, studio_radius ))
		return false; // culled

	// to prevent drawing sky entities through normal view
	if( tr.sky_camera && tr.fIgnoreSkybox )
	{
		mleaf_t *leaf = Mod_PointInLeaf( tr.sky_camera->origin, worldmodel->nodes );
		if( Mod_CheckEntityLeafPVS( modelpos + studio_mins, modelpos + studio_maxs, leaf ))
			return false; // it's sky entity
	}

	r_stats.num_drawed_ents++;

	return true;
}

/*
===============
StudioDynamicLight

===============
*/
void CStudioModelRenderer::StudioDynamicLight( cl_entity_t *ent, alight_t *lightinfo )
{
	if( !lightinfo ) return;

	if( r_fullbright->value )
	{
		m_pLightInfo->lightColor[0] = 1.0f;
		m_pLightInfo->lightColor[1] = 1.0f;
		m_pLightInfo->lightColor[2] = 1.0f;
		lightinfo->color = m_pLightInfo->lightColor;
		lightinfo->shadelight = 255;
		lightinfo->ambientlight = 128;
		return;
	}

	Vector origin;

	if( m_pCvarLighting->value == 2 )
	{
		m_pbonetransform[0].GetOrigin( origin );

		// NOTE: in some cases bone origin may be stuck in the geometry
		// and produced completely black model. Run additional check for this case
		if( POINT_CONTENTS( origin ) == CONTENTS_SOLID )
			m_protationmatrix.GetOrigin( origin );
	}
	else m_protationmatrix.GetOrigin( origin );

	// setup light dir
	if( RI.refdef.movevars )
	{
		// pre-defined light vector
		m_pLightInfo->lightVec[0] = RI.refdef.movevars->skyvec_x;
		m_pLightInfo->lightVec[1] = RI.refdef.movevars->skyvec_y;
		m_pLightInfo->lightVec[2] = RI.refdef.movevars->skyvec_z;
	}
	else
	{
		m_pLightInfo->lightVec = Vector( 0.0f, 0.0f, -1.0f );
	}

	if( m_pLightInfo->lightVec == g_vecZero )
		m_pLightInfo->lightVec = Vector( 0.0f, 0.0f, -1.0f );

	lightinfo->plightvec = m_pLightInfo->lightVec;

	color24 ambient;

	// setup ambient lighting
	bool invLight = (ent->curstate.effects & EF_INVLIGHT) ? true : false;
	R_LightForPoint( origin, &ambient, invLight, true, 0.0f ); // ignore dlights

	m_pLightInfo->lightColor[0] = ambient.r * (1.0f / 255.0f);
	m_pLightInfo->lightColor[1] = ambient.g * (1.0f / 255.0f);
	m_pLightInfo->lightColor[2] = ambient.b * (1.0f / 255.0f);

	lightinfo->color = m_pLightInfo->lightColor;
	lightinfo->shadelight = (ambient.r + ambient.g + ambient.b) / 3;
	lightinfo->ambientlight = lightinfo->shadelight;

	// clamp lighting so it doesn't overbright as much
	if( lightinfo->ambientlight > 128 )
		lightinfo->ambientlight = 128;

	if( lightinfo->ambientlight + lightinfo->shadelight > 192 )
		lightinfo->shadelight = 192 - lightinfo->ambientlight;
}

/*
===============
StudioEntityLight

===============
*/
void CStudioModelRenderer::StudioEntityLight( alight_t *lightinfo )
{
	if( !lightinfo ) return;

	m_pLightInfo->numElights = 0;	// clear previous elights
	if( r_fullbright->value ) return;

	Vector origin;

	if( m_pCvarLighting->value == 2 )
		m_pbonetransform[0].GetOrigin( origin );
	else m_protationmatrix.GetOrigin( origin );

	if( !r_dynamic->value )
		return;

	for( int lnum = 0; lnum < MAX_ELIGHTS; lnum++ )
	{
		dlight_t *el = GET_ENTITY_LIGHT( lnum );
		
		if( el->die < m_clTime || !el->radius )
			continue;

		float dist = (el->origin - origin).Length();

		if( !dist || dist > el->radius + studio_radius )
			continue;

		float radius2 = el->radius * el->radius; // squared radius

		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			Vector	vec, org;
				
			m_pbonetransform[i].GetOrigin( org );
			vec = org - el->origin;
			
			float dist = DotProduct( vec, vec );
			float atten = (dist / radius2 - 1) * -1;
			if( atten < 0 ) atten = 0;
			dist = sqrt( dist );

			if( dist )
			{
				vec *= ( 1.0f / dist );
				lightinfo->ambientlight += atten;
				lightinfo->shadelight += atten;
			}

			m_pLightInfo->elightVec[m_pLightInfo->numElights][i] = m_pbonetransform[i].VectorIRotate( vec ) * atten;
		}

		m_pLightInfo->elightColor[m_pLightInfo->numElights][0] = el->color.r * (1.0f / 255.0f);
		m_pLightInfo->elightColor[m_pLightInfo->numElights][1] = el->color.g * (1.0f / 255.0f);
		m_pLightInfo->elightColor[m_pLightInfo->numElights][2] = el->color.b * (1.0f / 255.0f);
		m_pLightInfo->numElights++;
	}

	// clamp lighting so it doesn't overbright as much
	if( lightinfo->ambientlight > 128 )
		lightinfo->ambientlight = 128;

	if( lightinfo->ambientlight + lightinfo->shadelight > 192 )
		lightinfo->shadelight = 192 - lightinfo->ambientlight;
}

/*
===============
StudioSetupLighting

===============
*/
void CStudioModelRenderer::StudioSetupLighting( alight_t *lightinfo )
{
	// setup bone lighting
	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		m_pLightInfo->blightVec[i] = m_pbonetransform[i].VectorIRotate( m_pLightInfo->lightVec );

	m_fHasProjectionLighting = StudioLightingIntersect();
}

/*
===============
StudioLighting

===============
*/
void CStudioModelRenderer::StudioLighting( Vector &lv, int bone, int flags, Vector normal )
{
	if( m_pCurrentEntity->curstate.effects & EF_FULLBRIGHT || r_fullbright->value )
	{
		lv = Vector( 1.0f, 1.0f, 1.0f );
		return;
	}

	float ambient = max( 0.1f, r_lighting_ambient->value );
	Vector illum = m_pLightInfo->lightColor * ambient;

	if( flags & STUDIO_NF_FLATSHADE )
	{
		illum += m_pLightInfo->lightColor * 0.8f;
	}
          else
          {
		float	r, lightcos;
		int	i;

		lightcos = DotProduct( normal, m_pLightInfo->blightVec[bone] ); // -1 colinear, 1 opposite

		if( lightcos > 1.0f ) lightcos = 1;
		illum += m_pLightInfo->lightColor;

		r = m_pCvarLambert->value;
		if( r < 1.0f ) r = 1.0f;

		lightcos = (lightcos + ( r - 1.0f )) / r; // do modified hemispherical lighting
		if( lightcos > 0.0f ) illum += (m_pLightInfo->lightColor * -lightcos);

		// to avoid inverse lighting		
		if( illum[0] <= 0.0f ) illum[0] = 0.0f;
		if( illum[1] <= 0.0f ) illum[1] = 0.0f;
		if( illum[2] <= 0.0f ) illum[2] = 0.0f;

		// now add all entity lights
		for( i = 0; i < m_pLightInfo->numElights; i++)
		{
			lightcos = -DotProduct( normal, m_pLightInfo->elightVec[i][bone] );
			if( lightcos > 0 ) illum += m_pLightInfo->elightColor[i] * lightcos;
		}
	}

	// normalize light	
	float maxIllum = max( illum[0], max( illum[1], illum[2] ));

	if( maxIllum > 1.0f )
		lv = illum * ( 1.0f / maxIllum );
	else lv = illum;
}

/*
===============
StudioClientEvents

===============
*/
void CStudioModelRenderer::StudioClientEvents( void )
{
	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t	*pevent;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
	pevent = (mstudioevent_t *)((byte *)m_pStudioHeader + pseqdesc->eventindex);

	// no events for this animation or gamepaused
	if( pseqdesc->numevents == 0 || m_clTime == m_clOldTime )
		return;

	float f = StudioEstimateFrame( pseqdesc ) + 0.01f; // get start offset
	float start = f - m_pCurrentEntity->curstate.framerate * (m_clTime - m_clOldTime) * pseqdesc->fps;

	for( int i = 0; i < pseqdesc->numevents; i++ )
	{
		// ignore all non-client-side events
		if( pevent[i].event < EVENT_CLIENT )
			continue;

		if( (float)pevent[i].frame > start && f >= (float)pevent[i].frame )
			HUD_StudioEvent( &pevent[i], m_pCurrentEntity );
	}
}

bool CStudioModelRenderer :: StudioLightingIntersect( void )
{
	if( !worldmodel->lightdata || r_fullbright->value )
		return false;

	// no custom lighting for transparent entities
	if( RI.params & RP_SHADOWVIEW )
		return false;

	if( m_pCurrentEntity->curstate.rendermode != kRenderNormal && m_pCurrentEntity->curstate.rendermode != kRenderTransAlpha )
		return false;

	Vector origin = m_protationmatrix.GetOrigin();

	for( int i = 0; i < MAX_PLIGHTS; i++ )
	{
		plight_t *l = &cl_plights[i];

		if( l->die < GET_CLIENT_TIME() || !l->radius )
			continue;

		float dist = (l->origin - origin).Length();

		if( !dist || dist > ( l->radius + studio_radius ))
			continue;

		if( R_CullSphereExt( l->frustum, origin, studio_radius, l->clipflags ))
			continue;

		return true;
	}

	return false;
}

/*
===============
StudioSetRenderMode

===============
*/
void CStudioModelRenderer :: StudioSetRenderMode( const int rendermode )
{
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	switch( rendermode )
	{
	case kRenderNormal:
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglDisable( GL_ALPHA_TEST );
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		pglEnable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		break;
	case kRenderTransAlpha:
		pglDisable( GL_BLEND );
		pglDepthMask( GL_TRUE );
		pglEnable( GL_ALPHA_TEST );
		pglAlphaFunc( GL_GREATER, 0.0f );
		break;
	case kRenderGlow:
	case kRenderTransAdd:
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
		if( m_nForceFaceFlags & STUDIO_NF_CHROME )
			pglDepthMask( GL_TRUE );
		else pglDepthMask( GL_FALSE );
		break;
	default:
		ALERT( at_error, "StudioRenderMode: bad rendermode %i\n", rendermode );
		break;
	}
}

/*
===============
StudioDrawMesh

===============
*/
void CStudioModelRenderer::StudioDrawMesh( short *ptricmds, float s, float t, int nDrawFlags, int nFaceFlags )
{
	byte	alpha = 255;
	byte	scale = 255;
	int	i;

	if( FBitSet( nDrawFlags, MESH_GLOWSHELL ))
		scale = 1.0f + m_pCurrentEntity->curstate.renderamt * (1.0f / 255.0f);

	if( FBitSet( nDrawFlags, MESH_ALPHA_ENTITY ))
		alpha = m_pCurrentEntity->curstate.renderamt;
	else if( FBitSet( nFaceFlags, STUDIO_NF_TRANSPARENT ))
		alpha = 1; // to avoid black lines on transparent tex

	if( FBitSet( nDrawFlags, MESH_DRAWARRAY ))
		m_nNumArrayVerts = m_nNumArrayElems = 0;

	while( i = *( ptricmds++ ))
	{
		int	vertexState = 0;
		bool	tri_strip;

		if( i < 0 )
		{
			if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
				pglBegin( GL_TRIANGLE_FAN );
			tri_strip = false;
			i = -i;
		}
		else
		{
			if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
				pglBegin( GL_TRIANGLE_STRIP );
			tri_strip = true;
		}

		// count all the draws not only in main pass
		if( RP_NORMALPASS( )) r_stats.c_studio_polys += (i - 2);

		for( ; i > 0; i--, ptricmds += 4 )
		{
			// build indices for glDrawArrays
			if( FBitSet( nDrawFlags, MESH_DRAWARRAY ))
                              {
				if( vertexState++ < 3 )
				{
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}
				else if( tri_strip )
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
			}

			if( !FBitSet( nDrawFlags, MESH_NOTEXCOORDS ))
                              {
				Vector2D	uv;

				if( FBitSet( nDrawFlags, MESH_CHROME ))
				{
					uv.x = m_chrome[ptricmds[1]][0] * s;
					uv.y = m_chrome[ptricmds[1]][1] * t;
				}
				else if( FBitSet( nFaceFlags, STUDIO_NF_UV_COORDS ))
				{
					uv.x = m_chrome[ptricmds[1]][0] * (1.0f / 32768.0f);
					uv.y = m_chrome[ptricmds[1]][1] * (1.0f / 32768.0f);
				}
				else
				{
					uv.x = ptricmds[2] * s;
					uv.y = ptricmds[3] * t;
				}

				if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
					pglTexCoord2f( uv.x, uv.y );
				else m_arraycoord[m_nNumArrayVerts] = uv;
			}

			if( !FBitSet( nDrawFlags, MESH_NONORMALS ))
                              {
				Vector nv = m_norms[ptricmds[1]];

				if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
					pglNormal3fv( nv );
				else m_arraynorms[m_nNumArrayVerts] = nv;
                              }

			if( !FBitSet( nDrawFlags, MESH_NOCOLORS ))
			{
				byte cl[4];

				if( FBitSet( nDrawFlags, MESH_COLOR_LIGHTING ))
				{
					cl[0] = m_lightvalues[ptricmds[1]].x * 255;
					cl[1] = m_lightvalues[ptricmds[1]].y * 255;
					cl[2] = m_lightvalues[ptricmds[1]].z * 255;
				}
				else if( FBitSet( nDrawFlags, MESH_COLOR_ENTITY ))
				{
					cl[0] = m_pCurrentEntity->curstate.rendercolor.r;
					cl[1] = m_pCurrentEntity->curstate.rendercolor.g;
					cl[2] = m_pCurrentEntity->curstate.rendercolor.b;
				}
				else
				{
					cl[0] = cl[1] = cl[2] = 255;
				}

				cl[3] = alpha;

				if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
					pglColor4ubv( cl );
				else memcpy( m_arraycolor[m_nNumArrayVerts], cl, sizeof( cl ));
			}

			Vector av = m_verts[ptricmds[0]];

			// scale mesh by normal
			if( FBitSet( nDrawFlags, MESH_GLOWSHELL ))
				av = av + m_norms[ptricmds[1]] * scale;

			if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
                         		pglVertex3fv( av );
			else m_arrayverts[m_nNumArrayVerts++] = av;
		}

		if( !FBitSet( nDrawFlags, MESH_DRAWARRAY ))
			pglEnd();
	}

	// we ends here for glBegin
	if( !FBitSet( nDrawFlags, MESH_DRAWARRAY )) return;

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 12, m_arrayverts );

	if( !FBitSet( nDrawFlags, MESH_NONORMALS ))
	{
		pglEnableClientState( GL_NORMAL_ARRAY );
		pglNormalPointer( GL_FLOAT, 12, m_arraynorms );
	}

	if( !FBitSet( nDrawFlags, MESH_NOCOLORS ))
	{
		pglEnableClientState( GL_COLOR_ARRAY );
		pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_arraycolor );
	}

	if( !FBitSet( nDrawFlags, MESH_NOTEXCOORDS ))
	{
		pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		pglTexCoordPointer( 2, GL_FLOAT, 0, m_arraycoord );
	}

	// draw it now
	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, m_nNumArrayVerts, m_nNumArrayElems, GL_UNSIGNED_INT, m_arrayelems );
	else pglDrawElements( GL_TRIANGLES, m_nNumArrayElems, GL_UNSIGNED_INT, m_arrayelems );

	pglDisableClientState( GL_VERTEX_ARRAY );

	if( !FBitSet( nDrawFlags, MESH_NONORMALS ))
		pglDisableClientState( GL_NORMAL_ARRAY );

	if( !FBitSet( nDrawFlags, MESH_NOCOLORS ))
		pglDisableClientState( GL_COLOR_ARRAY );

	if( !FBitSet( nDrawFlags, MESH_NOTEXCOORDS ))
		pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
}

/*
===============
StudioDrawMeshes

===============
*/
void CStudioModelRenderer :: StudioDrawMeshes( mstudiotexture_t *ptexture, short *pskinref, StudioPassMode drawPass )
{
	if( drawPass == STUDIO_PASS_AMBIENT )
		pglDisable( GL_TEXTURE_2D );

	for( int i = 0; i < m_pSubModel->nummesh; i++ ) 
	{
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + i;
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
		int iRenderMode = m_iRenderMode; // can be overwriting by texture flags

		int nFaceFlags = ptexture[pskinref[pmesh->skinref]].flags;
		float s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
		float t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;
		int nDrawFlags = MESH_NONORMALS;

		// check bounds
		if( ptexture[pskinref[pmesh->skinref]].index < 0 || ptexture[pskinref[pmesh->skinref]].index > MAX_TEXTURES )
			ptexture[pskinref[pmesh->skinref]].index = tr.defaultTexture;

		if(( nFaceFlags & STUDIO_NF_ADDITIVE || iRenderMode == kRenderTransAdd ) && ( drawPass == STUDIO_PASS_AMBIENT || drawPass == STUDIO_PASS_LIGHT ))
			continue;

		if( drawPass == STUDIO_PASS_FOG )
		{
			if( nFaceFlags & STUDIO_NF_ADDITIVE || ( iRenderMode != kRenderTransAlpha && iRenderMode != kRenderNormal ))
				continue;

			nDrawFlags |= (MESH_NOCOLORS|MESH_NOTEXCOORDS);

			if( nFaceFlags & STUDIO_NF_TRANSPARENT )
				pglDepthFunc( GL_EQUAL );
		}
		else if( drawPass == STUDIO_PASS_GLOWSHELL )
		{
			nDrawFlags |= (MESH_GLOWSHELL|MESH_CHROME|MESH_COLOR_ENTITY);
			StudioSetRenderMode( kRenderTransAdd );
		}
		else if( drawPass == STUDIO_PASS_SHADOW )
		{
			// enable depth-mask on transparent textures
			if( nFaceFlags & STUDIO_NF_TRANSPARENT )
			{
				pglEnable( GL_ALPHA_TEST );
				pglEnable( GL_TEXTURE_2D );
				pglAlphaFunc( GL_GREATER, 0.0f );
				GL_Bind( GL_TEXTURE0, ptexture[pskinref[pmesh->skinref]].index );
				nDrawFlags |= MESH_NOCOLORS;
			}
			else nDrawFlags |= (MESH_NOCOLORS|MESH_NOTEXCOORDS);
		}
		else if( drawPass == STUDIO_PASS_AMBIENT )
		{
			// no diffuse texture! vertex lighting only

			// enable depth-mask on transparent textures
			if( nFaceFlags & STUDIO_NF_TRANSPARENT )
			{
				pglEnable( GL_TEXTURE_2D );
				GL_Bind( GL_TEXTURE0, ptexture[pskinref[pmesh->skinref]].index );
				StudioSetRenderMode( kRenderTransAlpha );
				pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
				pglTexEnvi( GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB );
				pglTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_REPLACE );
			}
			else
			{
				StudioSetRenderMode( kRenderNormal );
				nDrawFlags |= MESH_NOTEXCOORDS;
                              }

			if(!( nFaceFlags & STUDIO_NF_FULLBRIGHT ))
			{
				nDrawFlags |= MESH_COLOR_LIGHTING;
			}
		}
		else if( drawPass == STUDIO_PASS_LIGHT )
		{
			nDrawFlags |= (MESH_NOCOLORS|MESH_NOTEXCOORDS);
		}
		else if( drawPass == STUDIO_PASS_DIFFUSE )
		{
			GL_Bind( GL_TEXTURE0, ptexture[pskinref[pmesh->skinref]].index );

			if( nFaceFlags & STUDIO_NF_ADDITIVE || iRenderMode == kRenderTransAdd )
			{
				StudioSetRenderMode( kRenderTransAdd );
				nDrawFlags |= MESH_ALPHA_ENTITY;

				if( nFaceFlags & STUDIO_NF_CHROME )
					nDrawFlags |= MESH_CHROME;

				if(!( nFaceFlags & STUDIO_NF_FULLBRIGHT ))
				{
					nDrawFlags |= MESH_COLOR_LIGHTING;
				}
			}
			else
			{
				if( nFaceFlags & STUDIO_NF_TRANSPARENT )
					pglDepthFunc( GL_EQUAL );

				if( nFaceFlags & STUDIO_NF_CHROME )
					nDrawFlags |= MESH_CHROME;

				pglEnable( GL_BLEND );
				pglBlendFunc( R_OVERBRIGHT_SFACTOR(), GL_SRC_COLOR );
				pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
				nDrawFlags |= MESH_NOCOLORS;
                              }
			pglDisable( GL_ALPHA_TEST );	// doesn't need alpha-test here
		}
		else if( drawPass == STUDIO_PASS_NORMAL )
		{
			GL_Bind( GL_TEXTURE0, ptexture[pskinref[pmesh->skinref]].index );

			if( nFaceFlags & STUDIO_NF_TRANSPARENT )
				iRenderMode = kRenderTransAlpha;
			else if( nFaceFlags & STUDIO_NF_ADDITIVE )
			{
				iRenderMode = kRenderTransAdd;
				nDrawFlags |= MESH_COLOR_LIGHTING|MESH_ALPHA_ENTITY;
                              }

			if( m_nForceFaceFlags & STUDIO_NF_CHROME )
				nDrawFlags |= (MESH_GLOWSHELL|MESH_CHROME|MESH_COLOR_ENTITY);

			if( nFaceFlags & STUDIO_NF_CHROME )
				nDrawFlags |= MESH_CHROME;

			if( m_iRenderMode == kRenderTransColor )
				nDrawFlags |= MESH_COLOR_ENTITY;

			if( !FBitSet( nDrawFlags, MESH_COLOR_ENTITY ))
			{
				if( iRenderMode == kRenderNormal || iRenderMode == kRenderTransAlpha || iRenderMode == kRenderTransTexture )
					nDrawFlags |= MESH_COLOR_LIGHTING;
			}

			if( nFaceFlags & STUDIO_NF_FULLBRIGHT )
			{
				nDrawFlags &= ~MESH_COLOR_LIGHTING;
			}

			if( iRenderMode != kRenderTransAlpha && iRenderMode != kRenderNormal )
				nDrawFlags |= MESH_ALPHA_ENTITY;

			StudioSetRenderMode( iRenderMode );
			R_SetupOverbright( true );
                    }

		StudioDrawMesh( ptricmds, s, t, nDrawFlags, nFaceFlags );

		if( drawPass == STUDIO_PASS_SHADOW || drawPass == STUDIO_PASS_AMBIENT )
		{
			if( nFaceFlags & STUDIO_NF_TRANSPARENT )
			{
				pglDisable( GL_ALPHA_TEST );
				pglDisable( GL_TEXTURE_2D );
			}
		}
		else if( drawPass == STUDIO_PASS_DIFFUSE || drawPass == STUDIO_PASS_FOG )
		{
			pglDepthFunc( GL_LEQUAL );
		}
		else if( drawPass == STUDIO_PASS_NORMAL )
		{
			R_SetupOverbright( false );
		}
	}

	if( drawPass == STUDIO_PASS_AMBIENT )
		pglEnable( GL_TEXTURE_2D );
}

/*
===============
StudioDrawPoints

===============
*/
void CStudioModelRenderer::StudioDrawPoints( void )
{
	mstudiotexture_t	*ptexture;
	mstudiomesh_t	*pmesh;
	short		*pskinref;
	float		scale = 1.0f;
	int		nFaceFlags;
	bool		m_fRebuildCache;

	m_nNumArrayVerts = m_nNumArrayElems = 0;

	if( !m_pStudioHeader ) return;
	if( m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell )
		m_nStudioModelCount++;

	byte *pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	byte *pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	int m_skinnum = abs( m_pCurrentEntity->curstate.skin );

	// grab the model textures array (with remap infos)
	ptexture = STUDIO_GET_TEXTURE( m_pCurrentEntity );

	assert( ptexture != NULL );

	pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
	Vector *pstudioverts = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	Vector *pstudionorms = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->normindex);

	pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

	// check cache state
	m_fRebuildCache = CheckVertexCache( m_pCurrentEntity ) ? false : true;

	for( int k = 0; k < m_pSubModel->numverts && m_fRebuildCache; k++ )
		m_verts[k] = m_pbonetransform[pvertbone[k]].VectorTransform( pstudioverts[k] );

	if( RI.params & RP_SHADOWVIEW )
	{
		// NOTE: shadow pass is not required lighting, texcoords etc
		StudioDrawMeshes( ptexture, pskinref, STUDIO_PASS_SHADOW );
		return;
	}

	if( m_nForceFaceFlags & STUDIO_NF_CHROME )
	{
		scale = 1.0f + m_pCurrentEntity->curstate.renderamt * (1.0f / 255.0f);

		for( int i = 0; i < m_pSubModel->numnorms && m_fRebuildCache; i++ )
			m_norms[i] = m_pbonetransform[pnormbone[i]].VectorRotate( pstudionorms[i] );
	}

	Vector *lv = m_lightvalues;

	for( int i = 0; i < m_pSubModel->nummesh; i++ ) 
	{
		nFaceFlags = ptexture[pskinref[pmesh[i].skinref]].flags;

		for( int j = 0; j < pmesh[i].numnorms; j++, lv++, pstudionorms++, pnormbone++ )
		{
			StudioLighting( *lv, *pnormbone, nFaceFlags, *pstudionorms );

			if(( nFaceFlags & STUDIO_NF_CHROME ) || ( m_nForceFaceFlags & STUDIO_NF_CHROME ))
				StudioSetupChrome( m_chrome[lv - m_lightvalues], *pnormbone, *pstudionorms );
		}
	}

	// at least one of projection lights has intersection with model bbox
	if( m_fHasProjectionLighting )
	{
		// NOTE: first pass: store ambient lighting into vertex color
		StudioDrawMeshes( ptexture, pskinref, STUDIO_PASS_AMBIENT );
		Vector origin = m_protationmatrix.GetOrigin();

		for( int i = 0; i < MAX_PLIGHTS; i++ )
		{
			plight_t *pl = &cl_plights[i];

			if( pl->die < GET_CLIENT_TIME() || !pl->radius )
				continue;

			float dist = (pl->origin - origin).Length();

			if( !dist || dist > ( pl->radius + studio_radius ))
				continue;

			if( R_CullSphereExt( pl->frustum, origin, studio_radius, pl->clipflags ))
				continue;

			R_BeginDrawProjection( pl );
			StudioDrawMeshes( ptexture, pskinref, STUDIO_PASS_LIGHT );
			R_EndDrawProjection();
		}

		// NOTE: last pass: merge computed color with diffuse texture
		StudioDrawMeshes( ptexture, pskinref, STUDIO_PASS_DIFFUSE );
	}
	else
	{
		// NOTE: all possible projection lights will be combined with
		// coexisting vertex lighting and drawed without projection texture
		StudioDrawMeshes( ptexture, pskinref, STUDIO_PASS_NORMAL );
	}
}

/*
===============
StudioDrawPointsFog

===============
*/
void CStudioModelRenderer::StudioDrawPointsFog( void )
{
	mstudiotexture_t	*ptexture;
	short		*pskinref;

	if( !m_pStudioHeader ) return;

	int m_skinnum = abs( m_pCurrentEntity->curstate.skin );

	// grab the model textures array (with remap infos)
	ptexture = STUDIO_GET_TEXTURE( m_pCurrentEntity );

	assert( ptexture != NULL );

	pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);

	// check cache state
	// FIXME: this is totally wrong but we couldn't rebuiled cache here. revisit this
	if( CheckVertexCache( m_pCurrentEntity ) )
		StudioDrawMeshes( ptexture, pskinref, STUDIO_PASS_FOG );
}

/*
===============
StudioDrawHulls

===============
*/
void CStudioModelRenderer::StudioDrawHulls( int iHitbox )
{
	float hullcolor[8][3] = 
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

	float alpha;

	if( r_drawentities->value == 4 )
		alpha = 0.5f;
	else alpha = 1.0f;

	if( iHitbox >= 0 )
		pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	pglDisable( GL_TEXTURE_2D );

	for( int i = 0; i < m_pStudioHeader->numhitboxes; i++ )
	{
		mstudiobbox_t *pbboxes = (mstudiobbox_t *)((byte *)m_pStudioHeader + m_pStudioHeader->hitboxindex);
		Vector v[8], v2[8], bbmin, bbmax;

		if( iHitbox >= 0 && iHitbox != i )
			continue;

		bbmin = pbboxes[i].bbmin;
		bbmax = pbboxes[i].bbmax;

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

		v2[0] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[0] );
		v2[1] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[1] );
		v2[2] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[2] );
		v2[3] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[3] );
		v2[4] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[4] );
		v2[5] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[5] );
		v2[6] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[6] );
		v2[7] = m_pbonetransform[pbboxes[i].bone].VectorTransform( v[7] );

		int k = (pbboxes[i].group % 8);

		// set properly color for hull
		pglColor4f( hullcolor[k][0], hullcolor[k][1], hullcolor[k][2], alpha );

		pglBegin( GL_QUAD_STRIP );
		for( int j = 0; j < 10; j++ )
			pglVertex3fv( v2[j & 7] );
		pglEnd( );
	
		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv( v2[6] );
		pglVertex3fv( v2[0] );
		pglVertex3fv( v2[4] );
		pglVertex3fv( v2[2] );
		pglEnd( );

		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv( v2[1] );
		pglVertex3fv( v2[7] );
		pglVertex3fv( v2[3] );
		pglVertex3fv( v2[5] );
		pglEnd( );			
	}

	if( iHitbox >= 0 )
		pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglEnable( GL_TEXTURE_2D );
}

/*
===============
StudioDrawAbsBBox

===============
*/
void CStudioModelRenderer::StudioDrawAbsBBox( void )
{
	Vector bbox[8];

	// looks ugly, skip
	if( m_pCurrentEntity == GET_VIEWMODEL( ))
		return;

	if( !StudioComputeBBox( m_pCurrentEntity, bbox ))
		return;

	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );

	pglColor4f( 1.0f, 0.0f, 0.0f, 1.0f );	// red bboxes for studiomodels
	pglBegin( GL_LINES );

	for( int i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}
	pglEnd();

	pglEnable( GL_TEXTURE_2D );
	pglEnable( GL_DEPTH_TEST );
}

/*
===============
StudioDrawBones

===============
*/
void CStudioModelRenderer::StudioDrawBones( void )
{
	mstudiobone_t	*pbones = (mstudiobone_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	Vector		point;

	pglDisable( GL_TEXTURE_2D );

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		if( pbones[i].parent >= 0 )
		{
			pglPointSize( 3.0f );
			pglColor3f( 1, 0.7f, 0 );
			pglBegin( GL_LINES );
			
			m_pbonetransform[pbones[i].parent].GetOrigin( point );
			pglVertex3fv( point );

			m_pbonetransform[i].GetOrigin( point );
			pglVertex3fv( point );
			
			pglEnd();

			pglColor3f( 0, 0, 0.8f );
			pglBegin( GL_POINTS );

			if( pbones[pbones[i].parent].parent != -1 )
			{
				m_pbonetransform[pbones[i].parent].GetOrigin( point );
				pglVertex3fv( point );
			}

			m_pbonetransform[i].GetOrigin( point );
			pglVertex3fv( point );
			pglEnd();
		}
		else
		{
			// draw parent bone node
			pglPointSize( 5.0f );
			pglColor3f( 0.8f, 0, 0 );
			pglBegin( GL_POINTS );

			m_pbonetransform[i].GetOrigin( point );
			pglVertex3fv( point );
			pglEnd();
		}
	}

	pglPointSize( 1.0f );
	pglEnable( GL_TEXTURE_2D );
}

void CStudioModelRenderer::StudioDrawAttachments( void )
{
	pglDisable( GL_TEXTURE_2D );
	pglDisable( GL_DEPTH_TEST );
	
	for( int i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		mstudioattachment_t	*pattachments;
		Vector v[4];

		pattachments = (mstudioattachment_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);		
		v[0] = m_pbonetransform[pattachments[i].bone].VectorTransform( pattachments[i].org );
		v[1] = m_pbonetransform[pattachments[i].bone].VectorTransform( g_vecZero );
		v[2] = m_pbonetransform[pattachments[i].bone].VectorTransform( g_vecZero );
		v[3] = m_pbonetransform[pattachments[i].bone].VectorTransform( g_vecZero );
		
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
void CStudioModelRenderer::StudioSetupRenderer( int rendermode )
{
	m_iRenderMode = bound( 0, rendermode, kRenderTransAdd );
	if(!( RI.params & RP_SHADOWVIEW )) pglShadeModel( GL_SMOOTH ); // enable gouraud shading
	if( glState.faceCull != GL_NONE ) GL_Cull( GL_FRONT );
}

/*
===============
StudioRestoreRenderer

===============
*/
void CStudioModelRenderer::StudioRestoreRenderer( void )
{
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
	pglShadeModel( GL_FLAT );

	// restore depthmask state for sprites etc
	if( glState.drawTrans )
		pglDepthMask( GL_FALSE );
	else pglDepthMask( GL_TRUE );
}

/*
====================
StudioDrawModel

====================
*/
int CStudioModelRenderer::StudioDrawModel( int flags )
{
	alight_t lighting;
	Vector dir;
	
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
          
	if( m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer )
	{
		entity_state_t deadplayer;

		int result;
		int save_interp;

		if( m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > gEngfuncs.GetMaxClients( ))
			return 0;

		// get copy of player
		deadplayer = *(IEngineStudio.GetPlayerState( m_pCurrentEntity->curstate.renderamt - 1 ));

		// clear weapon, movement state
		deadplayer.number = m_pCurrentEntity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		deadplayer.angles = m_pCurrentEntity->curstate.angles;
		deadplayer.origin = m_pCurrentEntity->curstate.origin;

		save_interp = m_fDoInterp;
		m_fDoInterp = 0;
		
		// draw as though it were a player
		result = StudioDrawPlayer( flags, &deadplayer );
		
		m_fDoInterp = save_interp;

		return result;
	}

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	StudioSetUpTransform();

	if( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if( !StudioCheckBBox( ))
			return 0;

		m_nModelsDrawn++;
		m_nStudioModelCount++; // render data cache cookie
		r_stats.c_studio_models_drawn++;

		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	// remove all decals if model was changed (e.g. old entity is freed and new entity set into same slot)
	if( m_pCurrentEntity->modelhandle != INVALID_HANDLE && !IsModelInstanceValid( m_pCurrentEntity->modelhandle ))
	{
		ModelInstance_t& inst = m_ModelInstances[m_pCurrentEntity->modelhandle];

		if( inst.m_DecalHandle != INVALID_HANDLE )
		{
			DestroyDecalList( inst.m_DecalHandle );
			inst.m_DecalHandle = INVALID_HANDLE;
		}
	}

	if( m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW )
	{
		StudioMergeBones( m_pRenderModel );
	}
	else
	{
		StudioSetupBones( );
	}

	StudioSaveBones( );

	if( flags & STUDIO_EVENTS )
	{
		StudioCalcAttachments( );
		StudioClientEvents( );

		// copy attachments into global entity array
		// g-cont: share client attachments with viewmodel
		if( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = GET_ENTITY( m_pCurrentEntity->index );
			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( Vector ) * 4 );
		}
	}

	if( flags & STUDIO_RENDER )
	{
		lighting.plightvec = dir;

		if(!( RI.params & RP_SHADOWVIEW ))
		{
			StudioDynamicLight(m_pCurrentEntity, &lighting );

			StudioEntityLight( &lighting );

			// model and frame independant
			StudioSetupLighting( &lighting );
                    }

		// get remap colors
		m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;
                    
		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		StudioRenderModel( );

		if( m_pCurrentEntity->curstate.weaponmodel )
		{
			model_t *pweaponmodel = IEngineStudio.GetModelByIndex( m_pCurrentEntity->curstate.weaponmodel );

			if( pweaponmodel )
			{
				cl_entity_t saveent = *m_pCurrentEntity;

				m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pweaponmodel );
				IEngineStudio.StudioSetHeader( m_pStudioHeader );

				StudioMergeBones( pweaponmodel );
          			StudioSetupLighting( &lighting );

				m_pCurrentEntity->modelhandle = INVALID_HANDLE;

				StudioRenderModel( );
				StudioCalcAttachments( );

				*m_pCurrentEntity = saveent;
			}
		}
	}

	return 1;
}

/*
=================
R_PushMoveFilter
=================
*/
int R_PushMoveFilter( physent_t *pe )
{
	if( !pe || pe->solid != SOLID_BSP || pe->movetype != MOVETYPE_PUSH )
		return 1;

	// optimization. Ignore world to avoid
	// unneeded transformations
	if( pe->info == 0 )
		return 1;

	return 0;
}

/*
====================
StudioEstimateGait

====================
*/
void CStudioModelRenderer::StudioEstimateGait( entity_state_t *pplayer )
{
	float dt, trainYaw = 0;
	Vector est_velocity, gnd_velocity;
	cl_entity_t *m_pGround = NULL;

	dt = (m_clTime - m_clOldTime);
	dt = bound( 0.0f, dt, 1.0f );

	if( dt == 0 )
	{
		m_flGaitMovement = 0;
		return;
	}

	// g-cont. this method fails on rotating platforms
	if( m_fGaitEstimation )
	{
		Vector vecSrc( m_pCurrentEntity->origin.x, m_pCurrentEntity->origin.y, m_pCurrentEntity->origin.z );
 		Vector vecEnd( m_pCurrentEntity->origin.x, m_pCurrentEntity->origin.y, m_pCurrentEntity->origin.z - 36 ); 
		pmtrace_t	trace;

		gEngfuncs.pEventAPI->EV_SetTraceHull( 0 );
		gEngfuncs.pEventAPI->EV_PlayerTraceExt( vecSrc, vecEnd, PM_STUDIO_IGNORE, R_PushMoveFilter, &trace );
		m_pGround = GET_ENTITY( gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace ));

		if( m_pGround && m_pGround->curstate.movetype == MOVETYPE_PUSH )
		{
			gnd_velocity = m_pGround->curstate.origin - m_pGround->prevstate.origin;
			trainYaw = m_pGround->curstate.angles[YAW] - m_pGround->prevstate.angles[YAW];
		}
		else
		{
			gnd_velocity = g_vecZero;
		}

		est_velocity = (m_pCurrentEntity->origin - m_pPlayerInfo->prevgaitorigin) - gnd_velocity;
		m_pPlayerInfo->prevgaitorigin = m_pCurrentEntity->origin;
		m_flGaitMovement = est_velocity.Length();

		if( dt <= 0 || ( m_flGaitMovement / dt ) < 5.0f )
		{
			m_flGaitMovement = 0;
			est_velocity.x = 0;
			est_velocity.y = 0;
		}
	}
	else
	{
		est_velocity = pplayer->velocity;
		m_flGaitMovement = est_velocity.Length() * dt;
	}

	if( est_velocity.x == 0 && est_velocity.y == 0 )
	{
		float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;

		if( flYawDiff > 180 )
			flYawDiff -= 360;

		if( flYawDiff < -180 )
			flYawDiff += 360;

		if( dt < 0.25 )
			flYawDiff *= dt * 4;
		else flYawDiff *= dt;

		m_pPlayerInfo->gaityaw += flYawDiff;
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)(m_pPlayerInfo->gaityaw / 360) * 360;

		m_flGaitMovement = 0;
	}
	else
	{
		m_pPlayerInfo->gaityaw = (atan2( est_velocity.y, est_velocity.x ) * 180 / M_PI );

		if( m_pPlayerInfo->gaityaw > 180 )
			m_pPlayerInfo->gaityaw = 180;

		if( m_pPlayerInfo->gaityaw < -180 )
			m_pPlayerInfo->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void CStudioModelRenderer::StudioProcessGait( entity_state_t *pplayer )
{
	mstudioseqdesc_t	*pseqdesc;
	float dt;
	int iBlend;
	float flYaw;	 // view direction relative to movement

	if( m_pCurrentEntity->curstate.sequence < 0 || m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq ) 
		m_pCurrentEntity->curstate.sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	StudioPlayerBlend( pseqdesc, iBlend, m_pCurrentEntity->angles[PITCH] );
	
	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[0] = iBlend;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

	dt = (m_clTime - m_clOldTime);
	dt = bound( 0.0f, dt, 1.0f );

	StudioEstimateGait( pplayer );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;

	if( flYaw < -180 )
		flYaw = flYaw + 360;
	if( flYaw > 180 )
		flYaw = flYaw - 360;

	if( flYaw > 120 )
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if( flYaw < -120 )
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
	m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
	m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
	m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;

	if( m_pCurrentEntity->angles[YAW] < -0 )
		m_pCurrentEntity->angles[YAW] += 360;

	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

	if( pplayer->gaitsequence < 0 || pplayer->gaitsequence >= m_pStudioHeader->numseq ) 
		pplayer->gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if( pseqdesc->linearmovement[0] > 0 )
	{
		m_pPlayerInfo->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)(m_pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if( m_pPlayerInfo->gaitframe < 0 ) m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
int CStudioModelRenderer::StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	alight_t lighting;
	float gaitframe, gaityaw;
	Vector dir, prevgaitorigin;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );

	m_nPlayerIndex = pplayer->number - 1;
          
	if( m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients( ))
		return 0;

	m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );
         
	if( m_pRenderModel == NULL )
		return 0;

	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( m_pRenderModel );
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	// remove all decals if model was changed (player change his model)
	if( m_pCurrentEntity->modelhandle != INVALID_HANDLE && !IsModelInstanceValid( m_pCurrentEntity->modelhandle ))
	{
		ModelInstance_t& inst = m_ModelInstances[m_pCurrentEntity->modelhandle];

		if( inst.m_DecalHandle != INVALID_HANDLE )
		{
			DestroyDecalList( inst.m_DecalHandle );
			inst.m_DecalHandle = INVALID_HANDLE;
		}
	}

	// g-cont. restore gaitframe after indirect passes
	if( !RP_NORMALPASS() )
	{
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		prevgaitorigin = m_pPlayerInfo->prevgaitorigin;
		gaitframe = m_pPlayerInfo->gaitframe;
		gaityaw = m_pPlayerInfo->gaityaw;
		m_pPlayerInfo = NULL;
	}

	if( pplayer->gaitsequence )
	{
		Vector orig_angles;
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

		orig_angles = m_pCurrentEntity->angles;
	
		StudioProcessGait( pplayer );

		m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
		m_pPlayerInfo = NULL;

		StudioSetUpTransform();
		m_pCurrentEntity->angles = orig_angles;
	}
	else
	{
		m_pCurrentEntity->curstate.controller[0] = 127;
		m_pCurrentEntity->curstate.controller[1] = 127;
		m_pCurrentEntity->curstate.controller[2] = 127;
		m_pCurrentEntity->curstate.controller[3] = 127;
		m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
		m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
		m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
		m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];
		
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		m_pPlayerInfo->gaitsequence = 0;

		StudioSetUpTransform();
	}

	if( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if( !StudioCheckBBox( ))
		{
			if( !RP_NORMALPASS() )
			{
				m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
				m_pPlayerInfo->prevgaitorigin = prevgaitorigin;
				m_pPlayerInfo->gaitframe = gaitframe;
				m_pPlayerInfo->gaityaw = gaityaw;
				m_pPlayerInfo = NULL;
			}

			return 0;
                    }
		m_nModelsDrawn++;
		m_nStudioModelCount++; // render data cache cookie
		r_stats.c_studio_models_drawn++;

		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

	StudioSetupBones( );
	StudioSaveBones( );

	m_pPlayerInfo->renderframe = tr.framecount;
	m_pPlayerInfo = NULL;

	if( flags & STUDIO_EVENTS )
	{
		StudioCalcAttachments( );
		StudioClientEvents( );

		// copy attachments into global entity array
		// g-cont: share client attachments with viewmodel
		if( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = GET_ENTITY( m_pCurrentEntity->index );
			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( Vector ) * 4 );
		}
	}

	if( flags & STUDIO_RENDER )
	{
		if( m_pCvarHiModels->value && m_pRenderModel != m_pCurrentEntity->model )
		{
			// show highest resolution multiplayer model
			m_pCurrentEntity->curstate.body = 255;
		}

		lighting.plightvec = dir;
		StudioDynamicLight(m_pCurrentEntity, &lighting );

		StudioEntityLight( &lighting );

		// model and frame independant
		StudioSetupLighting( &lighting );

		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

		// get remap colors
		m_nTopColor = bound( 0, m_pPlayerInfo->topcolor, 360 );
		m_nBottomColor = bound( 0, m_pPlayerInfo->bottomcolor, 360 );

		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		StudioRenderModel( );
		m_pPlayerInfo = NULL;

		if( pplayer->weaponmodel )
		{
			model_t *pweaponmodel = IEngineStudio.GetModelByIndex( pplayer->weaponmodel );

			if( pweaponmodel )
			{
				cl_entity_t saveent = *m_pCurrentEntity;

				m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pweaponmodel );
				IEngineStudio.StudioSetHeader( m_pStudioHeader );

				StudioMergeBones( pweaponmodel );
				StudioSetupLighting( &lighting );

				m_pCurrentEntity->modelhandle = INVALID_HANDLE;

				StudioRenderModel( );

				StudioCalcAttachments( );
				*m_pCurrentEntity = saveent;
			}
		}
	}

	if( !RP_NORMALPASS() )
	{
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		m_pPlayerInfo->prevgaitorigin = prevgaitorigin;
		m_pPlayerInfo->gaitframe = gaitframe;
		m_pPlayerInfo->gaityaw = gaityaw;
		m_pPlayerInfo = NULL;
	}

	return 1;
}

/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer::StudioRenderModel( void )
{
	m_nForceFaceFlags = 0;

	if( m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell && !( RI.params & RP_SHADOWVIEW ))
	{
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;

		StudioRenderFinal( );	// draw normal model

		m_nForceFaceFlags = STUDIO_NF_CHROME;
		gEngfuncs.pTriAPI->SpriteTexture( m_pChromeSprite, 0 );
		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;

		StudioRenderFinal( );	// draw glow shell
	}
	else
	{
		StudioRenderFinal( );
	}
}

/*
====================
StudioFormatAttachment

====================
*/
void CStudioModelRenderer::StudioFormatAttachment( Vector &point, bool bInverse )
{
	float worldx = tan( (float)RI.refdef.fov_x * M_PI / 360.0 );
	float viewx = tan( m_flViewmodelFov * M_PI / 360.0 );

	// aspect ratio cancels out, so only need one factor
	// the difference between the screen coordinates of the 2 systems is the ratio
	// of the coefficients of the projection matrices (tan (fov/2) is that coefficient)
	float factor = worldx / viewx;

	// BUGBUG: workaround
	if( IS_NAN( factor )) factor = 1.0f;

	// get the coordinates in the viewer's space.
	Vector tmp = point - RI.vieworg;
	Vector vTransformed;

	vTransformed.x = DotProduct( RI.vright, tmp );
	vTransformed.y = DotProduct( RI.vup, tmp );
	vTransformed.z = DotProduct( RI.vforward, tmp );

	// now squash X and Y.
	if( bInverse )
	{
		if ( factor != 0.0f )
		{
			vTransformed.x /= factor;
			vTransformed.y /= factor;
		}
		else
		{
			vTransformed.x = 0.0f;
			vTransformed.y = 0.0f;
		}
	}
	else
	{
		vTransformed.x *= factor;
		vTransformed.y *= factor;
	}

	// Transform back to world space.
	Vector vOut = (RI.vright * vTransformed.x) + (RI.vup * vTransformed.y) + (RI.vforward * vTransformed.z);
	point = RI.vieworg + vOut;
}

/*
====================
StudioRenderFinal

====================
*/
void CStudioModelRenderer::StudioRenderFinal(void)
{
	int rendermode = (m_nForceFaceFlags & STUDIO_NF_CHROME) ? kRenderTransAdd : m_pCurrentEntity->curstate.rendermode;

	StudioSetupRenderer( rendermode );
	
	if( r_drawentities->value == 2 )
	{
		StudioDrawBones();
	}
	else if( r_drawentities->value == 3 )
	{
		StudioDrawHulls( -1 );
	}
	else
	{
		for( int i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
		{
			StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );
			StudioDrawPoints();
		}

		if( m_pCurrentEntity->modelhandle != INVALID_HANDLE && !( RI.params & RP_SHADOWVIEW ))
		{
			DrawDecal( m_ModelInstances[m_pCurrentEntity->modelhandle].m_DecalHandle, m_pStudioHeader );
		}

		if( !m_fDrawViewModel && !( RI.params & RP_SHADOWVIEW ) && !( m_nForceFaceFlags & STUDIO_NF_CHROME ) && R_SetupFogProjection( ))
		{
			for( i = 0 ; i < m_pStudioHeader->numbodyparts; i++ )
			{
				StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );
				StudioDrawPointsFog();
			}

			pglDisable( GL_BLEND );
			GL_CleanUpTextureUnits( 0 );
		}

		if( r_drawentities->value == 4 )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			StudioDrawHulls( -1 );
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
=================
DrawStudioModel
=================
*/
void CStudioModelRenderer :: DrawStudioModelInternal( cl_entity_t *e, qboolean follow_entity )
{
	int	i, flags, result;

	if( RI.params & RP_ENVVIEW )
		return;

	if( !IEngineStudio.Mod_Extradata( e->model ))
		return;

	if( e == gEngfuncs.GetViewModel( ))
	{
		// viewmodel run their events before at start the frame
		// so all muzzleflashes drawing correctly
		flags = STUDIO_RENDER;	
	}
	else flags = STUDIO_RENDER|STUDIO_EVENTS;

	if( e == gEngfuncs.GetViewModel( ))
		m_fDoInterp = true;	// viewmodel can't properly animate without lerping
	else if( m_pCvarLerping->value )
		m_fDoInterp = (e->curstate.effects & EF_NOINTERP) ? false : true;
	else m_fDoInterp = false;
	m_fDrawViewModel = false;

	m_pbonetransform = NULL;

	// select the properly method
	if( e->player )
		result = StudioDrawPlayer( flags, &e->curstate );
	else result = StudioDrawModel( flags );

	m_pbonetransform = NULL;

	if( !result || follow_entity ) return;

	// NOTE: we must draw all followed entities
	// immediately after drawing parent when cached bones is valid
	for( i = 0; i < tr.num_child_entities; i++ )
	{
		if( gEngfuncs.GetEntityByIndex( tr.child_entities[i]->curstate.aiment ) == e )
		{
			// copy the parent origin for right frustum culling
			tr.child_entities[i]->origin = e->origin;

			RI.currententity = tr.child_entities[i];
			RI.currentmodel = RI.currententity->model;
			gRenderfuncs.R_SetCurrentEntity( RI.currententity );

			DrawStudioModelInternal( RI.currententity, true );
		}
	} 
}

/*
=================
RunViewModelEvents

=================
*/
void CStudioModelRenderer :: RunViewModelEvents( void )
{
	if( RI.refdef.onlyClientDraw || m_pCvarDrawViewModel->value == 0 )
		return;

	// ignore in thirdperson, camera view or client is died
	if( RI.thirdPerson || RI.refdef.health <= 0 || !UTIL_IsLocal( RI.refdef.viewentity ))
		return;

	if( RI.params & RP_NONVIEWERREF )
		return;

	RI.currententity = GET_VIEWMODEL();
	RI.currentmodel = RI.currententity->model;
	if( !RI.currentmodel ) return;

	// viewmodel can't properly animate without lerping
	m_fDoInterp = true;
	m_fDrawViewModel = true;
	m_pbonetransform = NULL;

	SET_CURRENT_ENTITY( RI.currententity );
	StudioDrawModel( STUDIO_EVENTS );

	SET_CURRENT_ENTITY( NULL );
	RI.currententity = NULL;
	m_pbonetransform = NULL;
	RI.currentmodel = NULL;
}

/*
=================
DrawViewModel

=================
*/
void CStudioModelRenderer :: DrawViewModel( void )
{
	if( RI.refdef.onlyClientDraw || m_pCvarDrawViewModel->value == 0 )
		return;

	// ignore in thirdperson, camera view or client is died
	if( RI.thirdPerson || RI.refdef.health <= 0 || !UTIL_IsLocal( RI.refdef.viewentity ))
		return;

	if( RI.params & RP_NONVIEWERREF )
		return;

	if( !IEngineStudio.Mod_Extradata( GET_VIEWMODEL()->model ))
		return;

	RI.currententity = GET_VIEWMODEL();
	RI.currentmodel = RI.currententity->model;
	if( !RI.currentmodel ) return;

	SET_CURRENT_ENTITY( RI.currententity );
	RI.currententity->curstate.renderamt = R_ComputeFxBlend( RI.currententity );

	// hack the depth range to prevent view model from poking into walls
	pglDepthRange( gldepthmin, gldepthmin + 0.3f * ( gldepthmax - gldepthmin ));

	// backface culling for left-handed weapons
	if( m_pCvarHand->value == LEFT_HAND )
		GL_FrontFace( !glState.frontFace );

	// viewmodel can't properly animate without lerping
	m_fDoInterp = true;
	m_fDrawViewModel = true;
	m_pbonetransform = NULL;

	// bound FOV values
	if( m_pCvarViewmodelFov->value < 50 )
		gEngfuncs.Cvar_SetValue( "cl_viewmodel_fov", 50 );
	else if( m_pCvarViewmodelFov->value > 130 )
		gEngfuncs.Cvar_SetValue( "cl_viewmodel_fov", 120 );

	// Find the offset our current FOV is from the default value
	float flFOVOffset = gHUD.default_fov->value - (float)RI.refdef.fov_x;

	// Adjust the viewmodel's FOV to move with any FOV offsets on the viewer's end
	m_flViewmodelFov = m_pCvarViewmodelFov->value - flFOVOffset;

	// calc local FOV
	float x = (float)ScreenWidth / tan( m_flViewmodelFov / 360 * M_PI );

	float fov_x = m_flViewmodelFov;
	float fov_y = atan( (float)ScreenHeight / x ) * 360 / M_PI;

	if( fov_x != RI.refdef.fov_x )
	{
		matrix4x4	oldProjectionMatrix = RI.projectionMatrix;
		R_SetupProjectionMatrix( fov_x, fov_y, RI.projectionMatrix );

		pglMatrixMode( GL_PROJECTION );
		GL_LoadMatrix( RI.projectionMatrix );

		StudioDrawModel( STUDIO_RENDER );

		// restore original matrix
		RI.projectionMatrix = oldProjectionMatrix;

		pglMatrixMode( GL_PROJECTION );
		GL_LoadMatrix( RI.projectionMatrix );
	}
	else
	{
		StudioDrawModel( STUDIO_RENDER );
	}

	// restore depth range
	pglDepthRange( gldepthmin, gldepthmax );

	// backface culling for left-handed weapons
	if( m_pCvarHand->value == LEFT_HAND )
		GL_FrontFace( !glState.frontFace );

	SET_CURRENT_ENTITY( NULL );
	m_fDrawViewModel = false;
	RI.currententity = NULL;
	m_pbonetransform = NULL;
	RI.currentmodel = NULL;
}
