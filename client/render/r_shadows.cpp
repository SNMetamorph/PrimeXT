/*
r_shadows.cpp - render shadowmaps for directional lights
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
#include "r_local.h"
#include "mathlib.h"
#include "event_api.h"
#include "r_studio.h"
#include "r_sprite.h"

/*
=============================================================

	SHADOW RENDERING

=============================================================
*/
int R_AllocateShadowTexture( void )
{
	int i = tr.num_shadows_used;

	if( i >= MAX_SHADOWS )
	{
		ALERT( at_error, "R_AllocateShadowTexture: shadow textures limit exceeded!\n" );
		return 0; // disable
	}

	int texture = tr.shadowTextures[i];
	tr.num_shadows_used++;

	if( !tr.shadowTextures[i] )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*shadow%i", i );

		tr.shadowTextures[i] = CREATE_TEXTURE( txName, RI.viewport[2], RI.viewport[3], NULL, TF_SHADOW ); 
		texture = tr.shadowTextures[i];
	}

	GL_Bind( GL_TEXTURE0, texture );
	pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3], 0 );

	return texture;
}

/*
===============
R_ShadowPassSetupFrame
===============
*/
static void R_ShadowPassSetupFrame( const plight_t *pl )
{
	// build the transformation matrix for the given view angles
	RI.refdef.viewangles[0] = anglemod( pl->angles[0] );
	RI.refdef.viewangles[1] = anglemod( pl->angles[1] );
	RI.refdef.viewangles[2] = anglemod( pl->angles[2] );	
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );
	RI.vieworg = RI.refdef.vieworg = pl->origin;

	// setup the screen FOV
	RI.refdef.fov_x = pl->fov;

	if( pl->flags & CF_ASPECT3X4 )
		RI.refdef.fov_y = pl->fov * (5.0f / 4.0f); 
	else if( pl->flags & CF_ASPECT4X3 )
		RI.refdef.fov_y = pl->fov * (4.0f / 5.0f);
	else RI.refdef.fov_y = pl->fov;

	// setup frustum
	memcpy( RI.frustum, pl->frustum, sizeof( RI.frustum ));
	RI.clipFlags = pl->clipflags;

	if(!( RI.params & RP_OLDVIEWLEAF ))
		R_FindViewLeaf();

	RI.currentlight = pl;
}

/*
=============
R_ShadowPassSetupGL
=============
*/
static void R_ShadowPassSetupGL( const plight_t *pl )
{
	// matrices already computed
	RI.worldviewMatrix = pl->modelviewMatrix;
	RI.projectionMatrix = pl->projectionMatrix;
	RI.worldviewProjectionMatrix = RI.projectionMatrix.Concat( RI.worldviewMatrix );

	GLfloat dest[16];

	// tell engine about worldviewprojection matrix so TriWorldToScreen and TriScreenToWorld
	// will be working properly
	RI.worldviewProjectionMatrix.CopyToArray( dest );
	SET_ENGINE_WORLDVIEW_MATRIX( dest );

	pglScissor( RI.scissor[0], RI.scissor[1], RI.scissor[2], RI.scissor[3] );
	pglViewport( RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3] );

	pglMatrixMode( GL_PROJECTION );
	GL_LoadMatrix( RI.projectionMatrix );

	pglMatrixMode( GL_MODELVIEW );
	GL_LoadMatrix( RI.worldviewMatrix );

	GL_Cull( GL_FRONT );

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
	pglEnable( GL_POLYGON_OFFSET_FILL );
	pglDisable( GL_TEXTURE_2D );
	pglDepthMask( GL_TRUE );
	pglPolygonOffset( 8, 30 );
	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_ALPHA_TEST );
	pglDisable( GL_BLEND );
}

/*
=============
R_ShadowPassEndGL
=============
*/
static void R_ShadowPassEndGL( void )
{
	pglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	pglDisable( GL_POLYGON_OFFSET_FILL );
	pglEnable( GL_TEXTURE_2D );
	pglPolygonOffset( -1, -2 );
}


/*
=============
R_ShadowPassDrawWorld
=============
*/
static void R_ShadowPassDrawWorld( const plight_t *pl )
{
	if( pl->flags & CF_NOWORLD_PROJECTION )
		return;	// no worldlight, no worldshadows

	// restore worldmodel
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	tr.modelorg = RI.vieworg;

	R_LoadIdentity();
	R_RecursiveLightNode( worldmodel->nodes, pl->frustum, pl->clipflags );
	R_LightStaticBrushes( pl );

	R_DrawShadowChains();
}

/*
=================
R_ShadowPassDrawBrushModel
=================
*/
void R_ShadowPassDrawBrushModel( cl_entity_t *e, const plight_t *pl )
{
	Vector	mins, maxs;
	model_t	*clmodel;
	bool	rotated;

	clmodel = e->model;

	if( e->angles != g_vecZero )
	{
		for( int i = 0; i < 3; i++ )
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
		rotated = true;
	}
	else
	{
		mins = e->origin + clmodel->mins;
		maxs = e->origin + clmodel->maxs;
		rotated = false;
	}

	if( R_CullBox( mins, maxs, RI.clipFlags ))
		return;

	if( RI.params & ( RP_SKYPORTALVIEW|RP_PORTALVIEW|RP_SCREENVIEW ))
	{
		if( rotated )
		{
			if( R_VisCullSphere( e->origin, clmodel->radius ))
				return;
		}
		else
		{
			if( R_VisCullBox( mins, maxs ))
				return;
		}
	}

	if( rotated ) R_RotateForEntity( e );
	else R_TranslateForEntity( e );

	if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.vieworg );
	else tr.modelorg = RI.vieworg - e->origin;

	// accumulate lit surfaces
	msurface_t *psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		float *v;
		int k;

		if( psurf->flags & (SURF_DRAWTILED|SURF_PORTAL|SURF_REFLECT))
			continue;

		R_AddToGrassChain( psurf, pl->frustum, pl->clipflags, false );

		if( R_CullSurfaceExt( psurf, pl->frustum, 0 ))
			continue;

		// draw depth-mask on transparent textures
		if( psurf->flags & SURF_TRANSPARENT )
		{
			pglEnable( GL_ALPHA_TEST );
			pglEnable( GL_TEXTURE_2D );
			pglAlphaFunc( GL_GREATER, 0.0f );
			GL_Bind( GL_TEXTURE0, psurf->texinfo->texture->gl_texturenum );
		}

		pglBegin( GL_POLYGON );
		for( k = 0, v = psurf->polys->verts[0]; k < psurf->polys->numverts; k++, v += VERTEXSIZE )
		{
			if( psurf->flags & SURF_TRANSPARENT )
				pglTexCoord2f( v[3], v[4] );
			pglVertex3fv( v );
		}
		pglEnd();

		if( psurf->flags & SURF_TRANSPARENT )
		{
			pglDisable( GL_ALPHA_TEST );
			pglDisable( GL_TEXTURE_2D );
		}
	}

	R_LoadIdentity();	// restore worldmatrix
}

void R_ShadowPassDrawSolidEntities( const plight_t *pl )
{
	glState.drawTrans = false;

	// draw solid entities only.
	for( int i = 0; i < tr.num_solid_entities; i++ )
	{
		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;

		// tell engine about current entity
		SET_CURRENT_ENTITY( RI.currententity );
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_ShadowPassDrawBrushModel( RI.currententity, pl );
			break;
		case mod_studio:
			R_DrawStudioModel( RI.currententity );
			break;
		case mod_sprite:
//			R_DrawSpriteModel( RI.currententity );
			break;
		default:
			break;
		}
	}

	// may be solid cables
	CL_DrawBeams( false );

	// NOTE: some mods with custom renderer may generate glErrors
	// so we clear it here
	while( pglGetError() != GL_NO_ERROR );
}

/*
================
R_RenderScene

RI.refdef must be set before the first call
fast version of R_RenderScene: no colors, no texcords etc
================
*/
void R_RenderShadowScene( const ref_params_t *pparams, const plight_t *pl )
{
	// set the worldmodel
	worldmodel = GET_ENTITY( 0 )->model;

	if( !worldmodel )
	{
		ALERT( at_error, "R_RenderShadowView: NULL worldmodel\n" );
		return;
	}

	RI.refdef = *pparams;
	r_stats.num_passes++;
	r_stats.num_drawed_ents = 0;
	tr.framecount++;

	R_ShadowPassSetupFrame( pl );
	R_ShadowPassSetupGL( pl );
	pglClear( GL_DEPTH_BUFFER_BIT );

	R_MarkLeaves();
	R_ShadowPassDrawWorld( pl );
	R_DrawGrass( GRASS_PASS_SHADOW );
	R_ShadowPassDrawSolidEntities( pl );

	R_ShadowPassEndGL();
}

void R_RenderShadowmaps( void )
{
	ref_instance_t	oldRI;

	if( r_fullbright->value || !r_shadows->value || RI.refdef.paused || RI.drawOrtho )
		return;

	// check for dynamic lights
	if( !R_CountPlights( true )) return;

	oldRI = RI; // make refinst backup
	int oldframecount = tr.framecount;

	for( int i = 0; i < MAX_PLIGHTS; i++ )
	{
		plight_t *pl = &cl_plights[i];

		if( pl->die < GET_CLIENT_TIME() || !pl->radius )
			continue;

		// TODO: allow shadows for pointlights
		if( pl->pointlight || FBitSet( pl->flags, CF_NOSHADOWS ))
		{
			pl->shadowTexture = 0;
			continue;
                    }

		// don't cull by PVS, because we may cull visible shadow here
		if( R_CullSphereExt( RI.frustum, pl->origin, pl->radius, RI.clipFlags ))
			continue;

		RI.params = RP_SHADOWVIEW|RP_MERGEVISIBILITY;

		// allow screen size
		RI.viewport[2] = RI.viewport[3] = 512;

		R_RenderShadowScene( &RI.refdef, pl );
		r_stats.c_shadow_passes++;

		pl->shadowTexture = R_AllocateShadowTexture();
		RI = oldRI; // restore ref instance
	}

	tr.framecount = oldframecount;	// restore real framecount
}
