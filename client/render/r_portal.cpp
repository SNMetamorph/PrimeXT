/*
r_portal.cpp - draw portal surfaces
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
#include "mathlib.h"

/*
=============================================================

	PORTAL RENDERING

=============================================================
*/
/*
================
R_SetupPortalView

Setup portal view and near plane
================
*/
void R_SetupPortalView( msurface_t *surf, mplane_t &plane, cl_entity_t *camera, matrix4x4 &m )
{
	cl_entity_t *ent;

	ent = RI.currententity;
	matrix4x4 surfaceMat, cameraMat, viewMat;

	if(( camera->origin != g_vecZero ) || ( camera->angles != g_vecZero ))
	{
		if( camera->angles != g_vecZero )
			cameraMat = matrix4x4( camera->origin, camera->angles );
		else cameraMat = matrix4x4( camera->origin, g_vecZero );
	}
	else cameraMat.Identity();

	if(( ent->origin != g_vecZero ) || ( ent->angles != g_vecZero ))
	{
		if( ent->angles != g_vecZero )
			surfaceMat = matrix4x4( ent->origin, ent->angles );
		else surfaceMat = matrix4x4( ent->origin, g_vecZero );
	}
	else surfaceMat.Identity();

	Vector origin, angles;

	// now get the camera origin and orientation
	m = cameraMat;

	plane.normal = cameraMat.GetForward();
	plane.dist = DotProduct( cameraMat.GetOrigin(), plane.normal );

	viewMat.SetForward( (Vector4D)RI.vforward );
	viewMat.SetRight( (Vector4D)RI.vright );
	viewMat.SetUp( (Vector4D)RI.vup );
	viewMat.SetOrigin( (Vector4D)RI.vieworg );

	surfaceMat = surfaceMat.Invert();

	viewMat = surfaceMat.ConcatTransforms( viewMat );
	viewMat = cameraMat.ConcatTransforms( viewMat );

	viewMat.GetAngles( angles );
	viewMat.GetOrigin( origin );

	RI.refdef.viewangles[0] = anglemod( angles[0] );
	RI.refdef.viewangles[1] = anglemod( angles[1] );
	RI.refdef.viewangles[2] = anglemod( angles[2] );
	RI.refdef.viewangles[2] = -RI.refdef.viewangles[2];
	RI.refdef.vieworg = origin;
	RI.pvsorigin = camera->origin;
}

/*
================
R_AllocatePortalTexture

Allocate the screen texture and make copy
================
*/
int R_AllocatePortalTexture( bool copyToImage = true )
{
	int i = tr.num_portals_used;

	if( i >= MAX_MIRRORS )
	{
		ALERT( at_error, "R_AllocatePortalTexture: portal textures limit exceeded!\n" );
		return 0;	// disable
	}

	int texture = tr.portalTextures[i];
	tr.num_portals_used++;

	if( !texture )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*portalcopy%i", i );

		// create new portal texture
		tr.portalTextures[i] = CREATE_TEXTURE( txName, RI.viewport[2], RI.viewport[3], NULL, TF_IMAGE ); 
		texture = tr.portalTextures[i];
	}

	if( copyToImage )
	{
		GL_Bind( GL_TEXTURE0, texture );
		pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, RI.viewport[0], RI.viewport[1], RI.viewport[2], RI.viewport[3], 0 );
	}

	return texture;
}

/*
================
R_DrawPortals

Draw all viewpasess from portal camera position
Portal textures will be drawn in normal pass
================
*/
void R_DrawPortals( cl_entity_t *ignoreent )
{
	ref_instance_t	oldRI;
	mplane_t		plane;
	msurface_t	*surf;
	int		i, oldframecount;
	mextrasurf_t	*es, *tmp, *portalchain;
	Vector		forward, right, up;
	Vector		origin, angles;
	matrix4x4		portalmatrix;
	cl_entity_t	*e, *camera;
	mleaf_t		*leaf, *leaf2;
	model_t		*m;

	if( !tr.num_portal_entities ) return; // mo portals for this frame

	oldRI = RI; // make refinst backup
	oldframecount = tr.framecount;

	leaf = r_viewleaf;
	leaf2 = r_viewleaf2;

	for( i = 0; i < tr.num_portal_entities; i++ )
	{
		portalchain = tr.portal_entities[i].chain;

		for( es = portalchain; es != NULL; )
		{
			RI.currententity = e = tr.portal_entities[i].ent;
			RI.currentmodel = m = RI.currententity->model;

			surf = INFO_SURF( es, m );

			assert( RI.currententity != NULL );
			assert( RI.currentmodel != NULL );

			if( e == ignoreent )
				goto skip_portal;				

			// handle portal camera
			camera = GET_ENTITY( e->curstate.sequence );

			if( !camera || camera->curstate.messagenum != r_currentMessageNum )
				goto skip_portal; // bad camera, skip to next portal

			R_SetupPortalView( surf, plane, camera, portalmatrix );

			RI.params = RP_PORTALVIEW|RP_CLIPPLANE;

			RI.clipPlane = plane;
			RI.clipFlags |= ( 1<<5 );

			RI.frustum[5] = plane;
			RI.frustum[5].signbits = SignbitsForPlane( RI.frustum[5].normal );
			RI.frustum[5].type = PLANE_NONAXIAL;

			if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
			{
				// allow screen size
				RI.viewport[2] = bound( 96, RI.viewport[2], 1024 );
				RI.viewport[3] = bound( 72, RI.viewport[3], 768 );
			}
			else
			{
				RI.viewport[2] = NearestPOW( RI.viewport[2], true );
				RI.viewport[3] = NearestPOW( RI.viewport[3], true );
				RI.viewport[2] = bound( 128, RI.viewport[2], 1024 );
				RI.viewport[3] = bound( 64, RI.viewport[3], 512 );
			}

			if( r_allow_mirrors->value && tr.insideView < MAX_MIRROR_DEPTH && !( oldRI.params & RP_MIRRORVIEW ))
			{
				if( R_FindMirrors( &RI.refdef ))
				{
					// render mirrors
					tr.insideView++;
					R_DrawMirrors ();
					tr.insideView--;
				}
			}

			if( r_allow_screens->value && tr.insideView < MAX_MIRROR_DEPTH && !( oldRI.params & RP_SCREENVIEW ))
			{
				if( R_FindScreens( &RI.refdef ))
				{
					// render screens
					tr.insideView++;
					R_DrawScreens ();
					tr.insideView--;
				}
			}

			if( GL_Support( R_FRAMEBUFFER_OBJECT ))
			{
				es->mirrortexturenum = R_AllocatePortalTexture( false );
				GL_BindFrameBuffer( tr.fbo[FBO_PORTALS], es->mirrortexturenum );
			}

			R_RenderScene( &RI.refdef );

			if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
				es->mirrortexturenum = R_AllocatePortalTexture();
			r_stats.c_portal_passes++;

			// create personal projection matrix for portal
			if(( camera->origin == g_vecZero ) && ( camera->angles == g_vecZero ))
			{
				es->mirrormatrix = RI.worldviewProjectionMatrix;
			}
			else
			{
				RI.modelviewMatrix = RI.worldviewMatrix.ConcatTransforms( portalmatrix );
				es->mirrormatrix = RI.projectionMatrix.Concat( RI.modelviewMatrix );
			}
skip_portal:
			tmp = es->mirrorchain;
			es->mirrorchain = NULL;
			es = tmp;

			RI = oldRI; // restore ref instance
			r_viewleaf = leaf;
			r_viewleaf2 = leaf2;
		}

		tr.portal_entities[i].chain = NULL; // done
		tr.portal_entities[i].ent = NULL;
	}

	if( GL_Support( R_FRAMEBUFFER_OBJECT ))
	{
		// revert to main buffer
		GL_BindFrameBuffer( FBO_MAIN, 0 );
	}

	r_viewleaf = r_viewleaf2 = NULL;	// force markleafs next frame
	tr.framecount = oldframecount;	// restore real framecount
	tr.num_portal_entities = 0;
}

/*
=================
R_FindBmodelPortals

Check all bmodel surfaces and make personal portal chain
=================
*/
void R_FindBmodelPortals( cl_entity_t *e, bool static_entity, int &numPortals )
{
	mextrasurf_t	*extrasurf;
	Vector		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	bool		rotated;
	int		i, clipFlags;

	// fast reject
	if( !( e->curstate.effects & EF_PORTAL ))
		return;

	// check PVS
	if( !Mod_CheckEntityPVS( e ))
		return;

	clmodel = e->model;

	if( static_entity )
	{
		RI.objectMatrix.Identity();

		if( R_CullBox( clmodel->mins, clmodel->maxs, RI.clipFlags ))
			return;

		tr.modelorg = RI.pvsorigin;
		clipFlags = RI.clipFlags;
	}
	else
	{
		if( e->angles != g_vecZero )
		{
			for( i = 0; i < 3; i++ )
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

		if(( e->origin != g_vecZero ) || ( e->angles != g_vecZero ))
		{
			if( rotated ) RI.objectMatrix = matrix4x4( e->origin, e->angles );
			else RI.objectMatrix = matrix4x4( e->origin, g_vecZero );
		}
		else RI.objectMatrix.Identity();

		if( rotated ) tr.modelorg = RI.objectMatrix.VectorITransform( RI.pvsorigin );
		else tr.modelorg = RI.pvsorigin - e->origin;

		clipFlags = 0;
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if(!( psurf->flags & SURF_PORTAL ))
			continue;

		if( R_CullSurface( psurf, clipFlags ))
			continue;

		extrasurf = SURF_INFO( psurf, RI.currentmodel );
		extrasurf->mirrorchain = tr.portal_entities[tr.num_portal_entities].chain;
		tr.portal_entities[tr.num_portal_entities].chain = extrasurf;
	}

	// store new portal entity
	if( tr.portal_entities[tr.num_portal_entities].chain != NULL )
	{
		tr.portal_entities[tr.num_portal_entities].ent = RI.currententity;
		tr.num_portal_entities++;
		numPortals++;
	}
}

/*
=================
R_CheckPortalEntitiesOnList

Check all bmodels for portal surfaces
=================
*/
int R_CheckPortalEntitiesOnList( void )
{
	int	i, numPortals = 0;

	// check static entities
	for( i = 0; i < tr.num_static_entities; i++ )
	{
		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelPortals( RI.currententity, true, numPortals );
			break;
		}
	}

	// check solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI.currententity = tr.solid_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelPortals( RI.currententity, false, numPortals );
			break;
		}
	}

	// check translucent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		RI.currententity = tr.trans_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currentmodel->type )
		{
		case mod_brush:
			R_FindBmodelPortals( RI.currententity, false, numPortals );
			break;
		}
	}

	return numPortals;
}

/*
================
R_FindPortals

Build portal chains for this frame
================
*/
bool R_FindPortals( const ref_params_t *fd )
{
	if( !tr.world_has_portals || !worldmodel || RI.refdef.paused || RI.drawOrtho )
		return false;

	RI.refdef = *fd;

	// build the transformation matrix for the given view angles
	RI.vieworg = RI.refdef.vieworg;
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );

	if(!( RI.params & RP_OLDVIEWLEAF ))
		R_FindViewLeaf();

	// player is outside world. Don't update portals in speedup reasons
	if(( r_viewleaf - worldmodel->leafs - 1) == -1 )
		return false;

	R_SetupFrustum();
	R_MarkLeaves ();

	if( R_CheckPortalEntitiesOnList( ))
		return true;
	return false;
}
