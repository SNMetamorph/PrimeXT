/*
gl_shadows.cpp - render shadowmaps for directional lights
Copyright (C) 2012 Uncle Mike

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
#include "const.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>
#include "gl_studio.h"
#include "gl_sprite.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"
#include "pm_defs.h"

/*
=============================================================

	SHADOW RENDERING

=============================================================
*/
TextureHandle R_AllocateShadowTexture( bool copyToImage = true )
{
	int i = tr.num_2D_shadows_used;

	if( i >= MAX_SHADOWS )
	{
		ALERT( at_error, "R_AllocateShadowTexture: shadow textures limit exceeded!\n" );
		return TextureHandle::Null(); // disable
	}

	TextureHandle texture = tr.shadowTextures[i];
	tr.num_2D_shadows_used++;

	if( !tr.shadowTextures[i].Initialized() )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*shadow2D%i", i );

		tr.shadowTextures[i] = CREATE_TEXTURE( txName, RI->view.port[2], RI->view.port[3], NULL, TF_SHADOW ); 
		texture = tr.shadowTextures[i];
	}

	if( copyToImage )
	{
		GL_Bind( GL_TEXTURE0, texture );
		pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, RI->view.port[0], RI->view.port[1], RI->view.port[2], RI->view.port[3], 0 );
	}

	return texture;
}

TextureHandle R_AllocateShadowCubemap( int side, bool copyToImage = true )
{
	TextureHandle texture = TextureHandle::Null();

	if( side != 0 )
	{
		int i = (tr.num_CM_shadows_used - 1);

		if( i >= MAX_SHADOWS )
		{
			ALERT( at_error, "R_AllocateShadowCubemap: shadow cubemaps limit exceeded!\n" );
			return TextureHandle::Null(); // disable
		}

		texture = tr.shadowCubemaps[i];

		if( !tr.shadowCubemaps[i].Initialized() )
		{
			ALERT( at_error, "R_AllocateShadowCubemap: cubemap not initialized!\n" );
			return TextureHandle::Null(); // disable
		}
	}
	else
	{
		int i = tr.num_CM_shadows_used;

		if( i >= MAX_SHADOWS )
		{
			ALERT( at_error, "R_AllocateShadowCubemap: shadow cubemaps limit exceeded!\n" );
			return TextureHandle::Null(); // disable
		}

		texture = tr.shadowCubemaps[i];
		tr.num_CM_shadows_used++;

		if( !tr.shadowCubemaps[i].Initialized() )
		{
			char txName[16];

			Q_snprintf( txName, sizeof( txName ), "*shadowCM%i", i );

			tr.shadowCubemaps[i] = CREATE_TEXTURE( txName, RI->view.port[2], RI->view.port[3], NULL, TF_SHADOW_CUBEMAP ); 
			texture = tr.shadowCubemaps[i];
		}
	}

	if( copyToImage )
	{
		GL_Bind( GL_TEXTURE0, texture );
		pglCopyTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB + side, 0, GL_DEPTH_COMPONENT, RI->view.port[0], RI->view.port[1], RI->view.port[2], RI->view.port[3], 0 );
	}

	return texture;
}

static int R_ComputeCropBounds( const matrix4x4 &lightViewProjection, Vector bounds[2] )
{
	Vector		worldBounds[2];
	int		numCasters = 0;
	ref_instance_t	*prevRI = R_GetPrevInstance();
	CFrustum		frustum;

	ClearBounds( bounds[0], bounds[1] );

	frustum.InitProjectionFromMatrix( lightViewProjection );

	// FIXME: nearplane culled studiomodels incorrectly. disabled for now
	frustum.DisablePlane( FRUSTUM_NEAR );
	frustum.DisablePlane( FRUSTUM_FAR );

	int i;

	for( i = 0; i < prevRI->frame.solid_faces.Count(); i++ )
	{
		CSolidEntry *entry = &prevRI->frame.solid_faces[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		mextrasurf_t *es = entry->m_pSurf->info;
		RI->currentmodel = es->parent->model;
		RI->currententity = es->parent;
		msurface_t *s = entry->m_pSurf;

		bool worldpos = R_StaticEntity( RI->currententity );
		if( !worldpos ) continue; // world polys only

		if( es->grass && CVAR_TO_BOOL( r_grass ))
		{
			// already included surface minmax
			worldBounds[0] = es->grass->mins;
			worldBounds[1] = es->grass->maxs;
		}
		else
		{
			worldBounds[0] = es->mins;
			worldBounds[1] = es->maxs;
		}

		if( frustum.CullBoxFast( worldBounds[0], worldBounds[1] ))
			continue;

		for( int j = 0; j < 8; j++ )
		{
			Vector4D point;
			point.x = worldBounds[(j >> 0) & 1].x;
			point.y = worldBounds[(j >> 1) & 1].y;
			point.z = worldBounds[(j >> 2) & 1].z;
			point.w = 1.0f;

			Vector4D transf = lightViewProjection.VectorTransform( point );

			transf.x /= transf.w;
			transf.y /= transf.w;
			transf.z /= transf.w;

			AddPointToBounds( transf, bounds[0], bounds[1] );
		}
		numCasters++;
	}

	// add studio models too
	for( int i = 0; i < prevRI->frame.solid_meshes.Count(); i++ )
	{
		if( !R_StudioGetBounds( &prevRI->frame.solid_meshes[i], worldBounds ))
			continue;

		if( frustum.CullBoxFast( worldBounds[0], worldBounds[1] ))
			continue;

		for( int j = 0; j < 8; j++ )
		{
			Vector4D point;
			point.x = worldBounds[(j >> 0) & 1].x;
			point.y = worldBounds[(j >> 1) & 1].y;
			point.z = worldBounds[(j >> 2) & 1].z;
			point.w = 1.0f;

			Vector4D transf = lightViewProjection.VectorTransform( point );
			transf.x /= transf.w;
			transf.y /= transf.w;
			transf.z /= transf.w;

			AddPointToBounds( transf, bounds[0], bounds[1] );
		}
		numCasters++;
	}

	return numCasters;
}

/*
===============
R_SetupLightDirectional
===============
*/
void R_SetupLightDirectional( CDynLight *pl, int split )
{
	matrix4x4	projectionMatrix, cropMatrix, s1;
	Vector	splitFrustumCorners[8];
	Vector	splitFrustumBounds[2];
	Vector	splitFrustumClipBounds[2];
	Vector	casterBounds[2];
	Vector	cropBounds[2];
	int	i;

	RI->view.splitFrustum[split].ComputeFrustumCorners( splitFrustumCorners );

	ClearBounds( splitFrustumBounds[0], splitFrustumBounds[1] );

	for( i = 0; i < 8; i++ )
		AddPointToBounds( splitFrustumCorners[i], splitFrustumBounds[0], splitFrustumBounds[1] );

	// find the bounding box of the current split in the light's view space
	ClearBounds( cropBounds[0], cropBounds[1] );

	for( i = 0; i < 8; i++ )
	{
		Vector4D	point( splitFrustumCorners[i] );
		Vector4D	transf = pl->viewMatrix.VectorTransform( point );

		transf.x /= transf.w;
		transf.y /= transf.w;
		transf.z /= transf.w;

		AddPointToBounds( transf, cropBounds[0], cropBounds[1] );
	}

	projectionMatrix.CreateOrthoRH( cropBounds[0].x, cropBounds[1].x, cropBounds[0].y, cropBounds[1].y, -cropBounds[1].z, -cropBounds[0].z );

	matrix4x4 viewProjectionMatrix = projectionMatrix.Concat( pl->viewMatrix );

	int numCasters = R_ComputeCropBounds( viewProjectionMatrix, casterBounds );

	// find the bounding box of the current split in the light's clip space
	ClearBounds( splitFrustumClipBounds[0], splitFrustumClipBounds[1] );

	for( i = 0; i < 8; i++ )
	{
		Vector4D	point( splitFrustumCorners[i] );
		Vector4D	transf = viewProjectionMatrix.VectorTransform( point );

		transf.x /= transf.w;
		transf.y /= transf.w;
		transf.z /= transf.w;

		AddPointToBounds( transf, splitFrustumClipBounds[0], splitFrustumClipBounds[1] );
	}

	// scene-dependent bounding volume
	cropBounds[0].x = Q_max( casterBounds[0].x, splitFrustumClipBounds[0].x );
	cropBounds[0].y = Q_max( casterBounds[0].y, splitFrustumClipBounds[0].y );
	cropBounds[0].z = Q_min( casterBounds[0].z, splitFrustumClipBounds[0].z );
	cropBounds[1].x = Q_min( casterBounds[1].x, splitFrustumClipBounds[1].x );
	cropBounds[1].y = Q_min( casterBounds[1].y, splitFrustumClipBounds[1].y );
	cropBounds[1].z = Q_max( casterBounds[1].z, splitFrustumClipBounds[1].z );

	if( numCasters == 0 )
	{
		cropBounds[0] = splitFrustumClipBounds[0];
		cropBounds[1] = splitFrustumClipBounds[1];
	}

	cropMatrix.Crop( cropBounds[0], cropBounds[1] );
	pl->projectionMatrix = cropMatrix.Concat( projectionMatrix );

	s1.CreateTranslate( 0.5f, 0.5f, 0.5f );
	s1.ConcatScale( 0.5f, 0.5f, 0.5f );

	viewProjectionMatrix = pl->projectionMatrix.Concat( pl->modelviewMatrix );

	// NOTE: texture matrix is not used. Save it for pssm show split debug tool
	pl->textureMatrix[split] = pl->projectionMatrix; 

	// build shadow matrices for each split
	pl->shadowMatrix[split] = s1.Concat( viewProjectionMatrix );
	pl->shadowMatrix[split].CopyToArray( pl->gl_shadowMatrix[split] );
	
	RI->view.frustum.InitProjectionFromMatrix( viewProjectionMatrix );
}

/*
===============
R_ShadowPassSetupViewCache
===============
*/
static void R_ShadowPassSetupViewCache( CDynLight *pl, int split = 0 )
{
	RI->glstate.viewport = RI->view.port;
	RI->view.farClip = pl->radius;
	RI->view.origin = pl->origin;
	RI->currentlight = pl;

	// setup the screen FOV
	RI->view.fov_x = pl->fov;
	RI->view.fov_y = pl->fov;

	// setup frustum
	if( pl->type == LIGHT_DIRECTIONAL )
	{
		pl->splitFrustum[split] = RI->view.splitFrustum[split];
		RI->view.matrix = pl->viewMatrix;
		R_SetupLightDirectional( pl, split );
	}
	else if( pl->type == LIGHT_OMNI )
	{
		RI->view.angles = CL_GetCubemapSideViewangles(split); // this is cube side of course
		RI->view.matrix = matrix4x4( RI->view.origin, RI->view.angles );
		RI->view.frustum.InitProjection( RI->view.matrix, 0.1f, pl->radius, 90.0f, 90.0f );
	}
	else
	{
		RI->view.matrix = pl->viewMatrix;
		RI->view.frustum = pl->frustum;
	}

	if( pl->type == LIGHT_OMNI )
	{
		Vector cubemapSideAngles = CL_GetCubemapSideViewangles(split);
		RI->view.worldMatrix.CreateModelview();
		RI->view.worldMatrix.ConcatRotate( -cubemapSideAngles.z, 1, 0, 0 );
		RI->view.worldMatrix.ConcatRotate( -cubemapSideAngles.x, 0, 1, 0 );
		RI->view.worldMatrix.ConcatRotate( -cubemapSideAngles.y, 0, 0, 1 );
		RI->view.worldMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );
		RI->view.projectionMatrix = pl->projectionMatrix;
	}
	else
	{
		// matrices already computed
		RI->view.worldMatrix = pl->modelviewMatrix;
		RI->view.projectionMatrix = pl->projectionMatrix;
	}

	RI->view.worldProjectionMatrix = RI->view.projectionMatrix.Concat( RI->view.worldMatrix );
	RI->view.worldProjectionMatrix.CopyToArray( RI->glstate.modelviewProjectionMatrix );
	RI->view.projectionMatrix.CopyToArray( RI->glstate.projectionMatrix );
	RI->view.worldMatrix.CopyToArray( RI->glstate.modelviewMatrix );
	
	// TODO optimize it with caching last view leaf
	RI->view.pvspoint = pl->origin;
	ENGINE_SET_PVS( RI->view.pvspoint, REFPVS_RADIUS, RI->view.pvsarray, false, true ); 
	R_MarkWorldVisibleFaces( worldmodel );

	// add all studio models, mark visible bmodels
	int localPlayerIndex = gEngfuncs.GetLocalPlayer()->index;
	for (int i = 0; i < tr.num_draw_entities; i++)
	{
		RI->currententity = tr.draw_entities[i];
		RI->currentmodel = RI->currententity->model;

		// skip entities with disabled shadow casting
		if (FBitSet(RI->currententity->curstate.effects, EF_NOSHADOW))
			continue;

		// disable rendering shadows for local player
		if (!CVAR_TO_BOOL(r_renderplayershadow)) 
		{
			if (RI->currententity->index == localPlayerIndex)
				continue;
		}

		// skip entities that marked as parent for dynlight and according flag set
		if (FBitSet(pl->flags, DLF_PARENTENTITY_NOSHADOW) && pl->parentEntity)
		{
			if (RI->currententity->index == pl->parentEntity->index)
				continue;
		}

		switch( RI->currentmodel->type )
		{
		case mod_studio:
			R_AddStudioToDrawList( RI->currententity );
			break;
		case mod_brush:
			R_MarkSubmodelVisibleFaces();
			break;
		}
	}

	// create drawlist for faces, do additional culling for world faces
	for (int i = 0; i < world->numsortedfaces; i++)
	{
		ASSERT( world->sortedfaces != NULL );
		int j = world->sortedfaces[i];
		ASSERT( j >= 0 && j < worldmodel->numsurfaces );

		if( CHECKVISBIT( RI->view.visfaces, j ))
		{
			msurface_t *surf = worldmodel->surfaces + j;
			mextrasurf_t *esrf = surf->info;

			// submodel faces already passed through this
			// operation but world is not
			if (FBitSet(surf->flags, SURF_OF_SUBMODEL))
			{
				RI->currententity = esrf->parent;
				RI->currentmodel = RI->currententity->model;
			}
			else
			{
				RI->currententity = GET_ENTITY(0);
				RI->currentmodel = RI->currententity->model;
				esrf->parent = RI->currententity; // setup dynamic upcast
			}

			// add grass which linked with this surface to shadow list too
			R_AddGrassToDrawList( surf, DRAWLIST_SHADOW );

			if (!FBitSet(surf->flags, SURF_OF_SUBMODEL))
			{
				if (R_CullSurface(surf, GetVieworg(), &RI->view.frustum))
				{
					CLEARVISBIT(RI->view.visfaces, j); // not visible
					continue;
				}
			}

			// only opaque faces interesting us
			if( R_OpaqueEntity( RI->currententity ))
			{
				R_AddSurfaceToDrawList( surf, DRAWLIST_SHADOW );
			}
		}
	}
}

/*
=============
R_ShadowPassSetupGL
=============
*/
static void R_ShadowPassSetupGL( const CDynLight *pl )
{
	R_SetupGLstate();

	pglEnable( GL_POLYGON_OFFSET_FILL );

	if( RI->currentlight->type == LIGHT_DIRECTIONAL )
	{
		if( r_shadows->value > 2.0f )
			pglPolygonOffset( 3.0f, 0.0f );
		else pglPolygonOffset( 2.0f, 0.0f );
		GL_Cull( GL_NONE );
	}
	else
	{
		if( r_shadows->value > 2.0f )
			pglPolygonOffset( 2.0f, 0.0f );
		else pglPolygonOffset( 1.0f, 0.0f );
		GL_Cull( GL_FRONT );
	}

	// HACKHACK to ignore paranoia opengl32.dll	
	GL_DepthRange( 0.0001f, 1.0f );
	pglEnable( GL_DEPTH_TEST );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_TRUE );
	GL_Blend( GL_FALSE );
}

/*
=============
R_ShadowPassEndGL
=============
*/
static void R_ShadowPassEndGL( void )
{
	pglDisable( GL_POLYGON_OFFSET_FILL );
	GL_DepthRange( gldepthmin, gldepthmax );
	pglPolygonOffset( -1, -2 );
	r_stats.c_shadow_passes++;
	GL_Cull( GL_FRONT );
}

/*
================
R_RenderShadowScene

fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadowScene( CDynLight *pl, int split = 0 )
{
	RI->params = RP_SHADOWVIEW;
	bool using_fbo = false;

	GL_DEBUG_SCOPE();
	if( pl->type == LIGHT_DIRECTIONAL )
	{
		if( tr.sunShadowFBO[split].Active( ))
		{
			RI->view.port[2] = RI->view.port[3] = sunSize[split];

			pl->shadowTexture[split] = tr.sunShadowFBO[split].GetTexture();
			tr.sunShadowFBO[split].Bind();
			using_fbo = true;
        }
		else {
			RI->view.port[2] = RI->view.port[3] = 512; // simple size if FBO was missed
		}
	}
	else
	{
		if( tr.fbo_shadow2D.Active( ))
		{
			RI->view.port[2] = tr.fbo_shadow2D.GetWidth();
			RI->view.port[3] = tr.fbo_shadow2D.GetHeight();

			pl->shadowTexture[0] = R_AllocateShadowTexture( false );
			tr.fbo_shadow2D.Bind( pl->shadowTexture[0] );
			using_fbo = true;
		}
		else RI->view.port[2] = RI->view.port[3] = 512;
	}

	R_ShadowPassSetupViewCache( pl, split );
	R_ShadowPassSetupGL( pl );

	pglClear( GL_DEPTH_BUFFER_BIT );

	R_RenderShadowBrushList();
	R_RenderShadowStudioList();
	R_DrawParticles( false );
	R_ShadowPassEndGL();

	if( !using_fbo )
		pl->shadowTexture[split] = R_AllocateShadowTexture();

}

/*
================
R_RenderShadowCubeSide

fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadowCubeSide( CDynLight *pl, int side )
{
	RI->params = RP_SHADOWVIEW;
	bool using_fbo = false;

	GL_DEBUG_SCOPE();
	if( tr.fbo_shadowCM.Active( ))
	{
		RI->view.port[2] = tr.fbo_shadowCM.GetWidth();
		RI->view.port[3] = tr.fbo_shadowCM.GetHeight();

		pl->shadowTexture[0] = R_AllocateShadowCubemap( side, false );
		tr.fbo_shadowCM.Bind( pl->shadowTexture[0], side );
		using_fbo = true;
	}
	else
	{
		// same size if FBO was missed
		RI->view.port[2] = RI->view.port[3] = 512;
		using_fbo = false;
	}

	R_ShadowPassSetupViewCache( pl, side );
	R_ShadowPassSetupGL( pl );

	pglClear( GL_DEPTH_BUFFER_BIT );

	R_RenderShadowBrushList();
	R_RenderShadowStudioList();
	R_DrawParticles( false );
	R_ShadowPassEndGL();

	if( !using_fbo )
		pl->shadowTexture[0] = R_AllocateShadowCubemap( side );
}

void R_RenderShadowmaps( void )
{
	ZoneScoped;

	unsigned int	oldFBO;

	if( R_FullBright() || !CVAR_TO_BOOL( r_shadows ) || tr.fGamePaused )
		return;

	if( FBitSet( RI->params, ( RP_NOSHADOWS|RP_ENVVIEW|RP_SKYVIEW )))
		return;

	// check for dynamic lights
	if( !HasDynamicLights( )) return;

	GL_DEBUG_SCOPE();
	R_PushRefState(); // make refinst backup
	oldFBO = glState.frameBuffer;

	for( int i = 0; i < MAX_DLIGHTS; i++ )
	{
		CDynLight *pl = &tr.dlights[i];

		if( !pl->Active() || FBitSet( pl->flags, DLF_NOSHADOWS ))
			continue;

		if( pl->type == LIGHT_OMNI )
		{
			if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
				continue;

			if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
				continue;

			if( R_CullBox( pl->absmin, pl->absmax ))
				continue;

			for( int j = 0; j < 6; j++ )
			{
				R_RenderShadowCubeSide( pl, j );
				R_ResetRefState(); // restore ref instance
			}
        }
		else if( pl->type == LIGHT_SPOT )
		{
			if( !Mod_CheckBoxVisible( pl->absmin, pl->absmax ))
				continue;

			if( R_CullBox( pl->absmin, pl->absmax ))
				continue;

			R_RenderShadowScene( pl );
			R_ResetRefState(); // restore ref instance
		}
		else if( pl->type == LIGHT_DIRECTIONAL )
		{
			 if( !CVAR_TO_BOOL( r_sunshadows ) || tr.sky_normal.z >= 0.0f )
				continue;	// shadows are invisible

			for( int j = 0; j <= NUM_SHADOW_SPLITS; j++ )
			{
				// PSSM: draw all the splits
				R_RenderShadowScene( pl, j );
				R_ResetRefState(); // restore ref instance
			}
		}
	}

	R_PopRefState(); // restore ref instance
	// restore FBO state
	GL_BindFBO( oldFBO );
	GL_BindShader( NULL );
	RI->currentlight = NULL;
}
