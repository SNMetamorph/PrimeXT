/*
r_subview.cpp - handle all subview passes
Copyright (C) 2018 Uncle Mike

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
#include "r_world.h"
#include "r_view.h"
#include "pm_movevars.h"

#define MIRROR_PLANE_EPSILON		32.0f	// g-cont. tune this by taste

/*
=============================================================

	MIRROR RENDERING

=============================================================
*/
/*
================
R_SetupMirrorView

Prepare view for mirroring
================
*/
int R_SetupMirrorView( msurface_t *surf, cl_entity_t *camera, matrix4x4 &viewmatrix )
{
	cl_entity_t	*ent = surf->info->parent;
	matrix3x3		matAngles;
	mplane_t		plane;
	float		d;

	// setup mirror plane
	// setup portal plane
	if( FBitSet( surf->flags, SURF_PLANEBACK ))
		SetPlane( &plane, -surf->plane->normal, -surf->plane->dist );
	else SetPlane( &plane, surf->plane->normal, surf->plane->dist );

	if( !R_StaticEntity( ent ))
	{
		if( ent->angles != g_vecZero )
			viewmatrix = matrix4x4( ent->origin, ent->angles );
		else viewmatrix = matrix4x4( ent->origin, g_vecZero );
		viewmatrix.TransformPositivePlane( plane, plane );
	}
	else viewmatrix.Identity();

	// reflect view by mirror plane
	d = -2.0f * ( DotProduct( RI->vieworg, plane.normal ) - plane.dist );
	Vector origin = RI->vieworg + d * plane.normal;

	d = -2.0f * DotProduct( RI->vforward, plane.normal );
	Vector forward = ( RI->vforward + d * plane.normal ).Normalize();

	d = -2.0f * DotProduct( RI->vright, plane.normal );
	Vector right = ( RI->vright + d * plane.normal ).Normalize();

	d = -2.0f * DotProduct( RI->vup, plane.normal );
	Vector up = ( RI->vup + d * plane.normal ).Normalize();

	// compute mirror angles
	matAngles.SetForward( forward );
	matAngles.SetRight( right );
	matAngles.SetUp( up );
	Vector angles = matAngles.GetAngles();

	plane.dist += ON_EPSILON; // to prevent z-fighting with reflective water

	RI->params = RP_MIRRORVIEW|RP_CLIPPLANE|RP_OLDVIEWLEAF;
	RI->frustum.SetPlane( FRUSTUM_NEAR, plane.normal, plane.dist );
	RI->clipPlane = plane;

	RI->viewangles[0] = anglemod( angles[0] );
	RI->viewangles[1] = anglemod( angles[1] );
	RI->viewangles[2] = anglemod( angles[2] );
	RI->vieworg = origin;

	// put pvsorigin before the mirror plane to avoid get recursion with himself
	if( ent == GET_ENTITY( 0 )) origin = surf->info->origin + (1.0f * plane.normal);
	else origin = viewmatrix.VectorTransform( surf->info->origin ) + (1.0f * plane.normal);

	RI->pvsorigin = origin;

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		// allow screen size
		RI->viewport[2] = bound( 96, RI->viewport[2], 1024 );
		RI->viewport[3] = bound( 72, RI->viewport[3], 768 );
	}
	else
	{
		RI->viewport[2] = NearestPOW( RI->viewport[2], true );
		RI->viewport[3] = NearestPOW( RI->viewport[3], true );
		RI->viewport[2] = bound( 128, RI->viewport[2], 1024 );
		RI->viewport[3] = bound( 64, RI->viewport[3], 512 );
	}

	return TF_SCREEN;
}

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
int R_SetupPortalView( msurface_t *surf, cl_entity_t *camera, matrix4x4 &viewmatrix )
{
	cl_entity_t	*ent = surf->info->parent;
	matrix4x4		surfaceMat, cameraMat, viewMat;
	Vector		origin, angles;
	mplane_t		plane;

	if( !R_StaticEntity( ent ))
	{
		if( ent->angles != g_vecZero )
			surfaceMat = matrix4x4( ent->origin, ent->angles );
		else surfaceMat = matrix4x4( ent->origin, g_vecZero );
	}
	else surfaceMat.Identity();

	if( !R_StaticEntity( camera ))
	{
		if( camera->angles != g_vecZero )
			cameraMat = matrix4x4( camera->origin, camera->angles );
		else cameraMat = matrix4x4( camera->origin, g_vecZero );
	}
	else cameraMat.Identity();

	// now get the camera origin and orientation
	viewmatrix = cameraMat;

	viewMat.SetForward( (Vector4D)RI->vforward );
	viewMat.SetRight( (Vector4D)RI->vright );
	viewMat.SetUp( (Vector4D)RI->vup );
	viewMat.SetOrigin( (Vector4D)RI->vieworg );

	surfaceMat = surfaceMat.Invert();

	viewMat = surfaceMat.ConcatTransforms( viewMat );
	viewMat = cameraMat.ConcatTransforms( viewMat );

	viewMat.GetAngles( angles );
	viewMat.GetOrigin( origin );

	RI->viewangles[0] = anglemod( angles[0] );
	RI->viewangles[1] = anglemod( angles[1] );
	RI->viewangles[2] = anglemod( angles[2] );
	RI->viewangles[2] = -RI->viewangles[2];
	RI->pvsorigin = camera->origin;
	RI->vieworg = origin;

	plane.dist += ON_EPSILON; // to prevent z-fighting with reflective water

	// set clipping plane
	SetPlane( &plane, cameraMat.GetForward(), DotProduct( cameraMat.GetForward(), camera->origin ));
	RI->params = RP_PORTALVIEW|RP_CLIPPLANE;
	RI->frustum.SetPlane( FRUSTUM_NEAR, plane.normal, plane.dist );
	RI->clipPlane = plane;

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		// allow screen size
		RI->viewport[2] = bound( 96, RI->viewport[2], 1024 );
		RI->viewport[3] = bound( 72, RI->viewport[3], 768 );
	}
	else
	{
		RI->viewport[2] = NearestPOW( RI->viewport[2], true );
		RI->viewport[3] = NearestPOW( RI->viewport[3], true );
		RI->viewport[2] = bound( 128, RI->viewport[2], 1024 );
		RI->viewport[3] = bound( 64, RI->viewport[3], 512 );
	}

	return TF_SCREEN;
}

/*
=============================================================

	SCREEN RENDERING

=============================================================
*/
/*
================
R_SetupScreenView

Setup screen view
================
*/
int R_SetupScreenView( msurface_t *surf, cl_entity_t *camera, matrix4x4 &viewmatrix )
{
	cl_entity_t	*ent = surf->info->parent;
	Vector		origin, angles;
	float		fov = 90;

	viewmatrix.Identity();

	if( camera->player && UTIL_IsLocal( camera->curstate.number ))
	{
		ref_params_t	viewparams;

		memcpy( &viewparams, &tr.viewparams, sizeof( viewparams ));

		// player seen through camera. Restore firstperson view here
		if( tr.viewparams.viewentity > tr.viewparams.maxclients )
			V_CalcFirstPersonRefdef( &viewparams );

		SetBits( RI->params, RP_SCREENVIEW|RP_FORCE_NOPLAYER );
		RI->pvsorigin = viewparams.vieworg;
		return TF_NOMIPMAP;
	}

	origin = camera->origin;
	angles = camera->angles;

	studiohdr_t *viewmonster = (studiohdr_t *)IEngineStudio.Mod_Extradata( camera->model );

	// it's monster
	if( viewmonster && FBitSet( camera->curstate.eflags, EFLAG_SLERP ))
	{
		Vector forward;
		AngleVectors( angles, forward, NULL, NULL );

		Vector viewpos = viewmonster->eyeposition;

		if( viewpos == g_vecZero )
			viewpos = Vector( 0, 0, 8.0f );// monster_cockroach
		origin += viewpos + forward * 8.0f;	// best value for humans
	}

	// smooth step stair climbing
	// lasttime goes into ent->latched.sequencetime
	// oldz goes into ent->latched.prevanimtime
	// HACKHACK we store interpolate values into func_monitor entity to avoid broke lerp
	if( origin[2] - ent->latched.prevanimtime > 0.0f )
	{
		float steptime;
		
		steptime = tr.time - ent->latched.sequencetime;
		if( steptime < 0 ) steptime = 0;

		ent->latched.prevanimtime += steptime * 150.0f;

		if( ent->latched.prevanimtime > origin[2] )
			ent->latched.prevanimtime = origin[2];
		if( origin[2] - ent->latched.prevanimtime > tr.movevars->stepsize )
			ent->latched.prevanimtime = origin[2] - tr.movevars->stepsize;

		origin[2] += ent->latched.prevanimtime - origin[2];
	}
	else
	{
		ent->latched.prevanimtime = origin[2];
	}

	ent->latched.sequencetime = tr.time;
 
	// setup the screen fov
 	if( ent->curstate.fuser2 != 0.0f )
 		fov = bound( 10, ent->curstate.fuser2, 120 );

 	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		texture_t	*t = surf->texinfo->texture;

		if(( t->width == 64 ) && ( t->height == 64 )) 
		{
			// HACKHACK: for default texture
			RI->viewport[2] = RI->viewport[3] = 512;
		}
		else
		{
			RI->viewport[2] = bound( 64, t->width * 2, 1024 );
			RI->viewport[3] = bound( 64, t->height * 2, 1024 );
		}
	}
	else RI->viewport[2] = RI->viewport[3] = 512;

 	// setup the screen FOV
 	if( RI->viewport[2] == RI->viewport[3] )
 	{
  		RI->fov_x = fov;
 		RI->fov_y = fov;
 	}
	else
	{
 		RI->fov_x = fov;
 		RI->fov_y = V_CalcFov( RI->fov_x, RI->viewport[2], RI->viewport[3] );
	}

	RI->viewangles[0] = anglemod( angles[0] );
	RI->viewangles[1] = anglemod( angles[1] );
	RI->viewangles[2] = anglemod( angles[2] );
	RI->pvsorigin = camera->origin;
	RI->vieworg = origin;

	RI->params = RP_SCREENVIEW;
	if( camera->player )
		SetBits( RI->params, RP_FORCE_NOPLAYER ); // camera set to player don't draw him

	return TF_NOMIPMAP;
}

/*
================
R_AllocateSubviewTexture

Allocate the screen texture and make copy
================
*/
int R_AllocateSubviewTexture( int viewport[4], int texFlags )
{
	int	i, texture;

	// FIXME: get rid of this
	if( tr.realframecount <= 1 )
		return 0;

	// first, search for available mirror texture
	for( i = 0; i < tr.num_subview_used; i++ )
	{
		if( tr.subviewTextures[i].texframe == tr.realframecount )
			continue;	// already used for this frame

		if( viewport[2] != RENDER_GET_PARM( PARM_TEX_WIDTH, tr.subviewTextures[i].texturenum ))
			continue;	// width mismatched

		if( viewport[3] != RENDER_GET_PARM( PARM_TEX_HEIGHT, tr.subviewTextures[i].texturenum ))
			continue;	// height mismatched

		// screens don't want textures with 'clamp' modifier
		if( FBitSet( texFlags, TF_CLAMP ) != FBitSet( RENDER_GET_PARM( PARM_TEX_FLAGS, tr.subviewTextures[i].texturenum ), TF_CLAMP ))
			continue;	// mismatch texture flags

		// found a valid spot
		tr.subviewTextures[i].texframe = tr.realframecount; // now used
		texture = tr.subviewTextures[i].texturenum;
		break;
	}

	if( i == tr.num_subview_used )
	{
		if( i == MAX_SUBVIEW_TEXTURES )
		{
			ALERT( at_error, "R_AllocSubviewTexture: texture limit exceeded (per frame)!\n" );
			return 0;	// disable
		}

		// create new mirror texture
		tr.subviewTextures[i].texturenum = texture = CREATE_TEXTURE( va( "*subview%i", i ), viewport[2], viewport[3], NULL, texFlags ); 
		tr.subviewTextures[i].texframe = tr.realframecount; // now used
		if( GL_Support( R_FRAMEBUFFER_OBJECT ))
			tr.subviewTextures[i].framebuffer = R_AllocFrameBuffer( viewport );
		tr.num_subview_used++; // allocate new one
	}

	if( GL_Support( R_FRAMEBUFFER_OBJECT ))
 	{
		GL_BindFrameBuffer( tr.subviewTextures[i].framebuffer, tr.subviewTextures[i].texturenum );
	}
	else 
	{
		GL_Bind( GL_TEXTURE0, texture );
		pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, viewport[0], viewport[1], viewport[2], viewport[3], 0 );
	}

	return (i+1);
}

bool R_CheckMirrorClone( msurface_t *surf, msurface_t *check )
{
	if( !FBitSet( surf->flags, SURF_REFLECT ))
		return false;

	if( !FBitSet( check->flags, SURF_REFLECT ))
		return false;

	if( !check->info->subtexture[glState.stack_position-1] )
		return false;

	if( surf->info->parent != check->info->parent )
		return false;

	if( FBitSet( surf->flags, SURF_PLANEBACK ) != FBitSet( check->flags, SURF_PLANEBACK ))
		return false;

	if( surf->plane->normal != check->plane->normal )
		return false;

	if( fabs( surf->plane->dist - check->plane->dist ) > MIRROR_PLANE_EPSILON )
		return false;

	// just reuse the handle
	surf->info->subtexture[glState.stack_position-1] = check->info->subtexture[glState.stack_position-1];

	return true;
}

bool R_CheckScreenSource( msurface_t *surf, msurface_t *check )
{
	if( !FBitSet( surf->flags, SURF_SCREEN ))
		return false;

	if( !FBitSet( check->flags, SURF_SCREEN ))
		return false;

	if( !check->info->subtexture[glState.stack_position-1] )
		return false;

	// different fov
	if( surf->info->parent->curstate.fuser2 != check->info->parent->curstate.fuser2 )
		return false;

	// different cameras
	if( surf->info->parent->curstate.sequence != check->info->parent->curstate.sequence )
		return false;

	// just reuse the handle
	surf->info->subtexture[glState.stack_position-1] = check->info->subtexture[glState.stack_position-1];

	return true;
}

bool R_CheckPortalSource( msurface_t *surf, msurface_t *check )
{
	if( !FBitSet( surf->flags, SURF_PORTAL ))
		return false;

	if( !FBitSet( check->flags, SURF_PORTAL ))
		return false;

	if( !check->info->subtexture[glState.stack_position-1] )
		return false;

	// different cameras
	if( surf->info->parent->curstate.sequence != check->info->parent->curstate.sequence )
		return false;

	// just reuse the handle
	surf->info->subtexture[glState.stack_position-1] = check->info->subtexture[glState.stack_position-1];

	return true;
}

bool R_CanSkipPass( int end, msurface_t *surf, ref_instance_t *prevRI )
{
	// IMPORTANT: limit the recursion depth
	if( surf == prevRI->reject_face )
	{
		if( !FBitSet( RI->params, RP_SCREENVIEW ))
		{
			return true;
		}
		else if( glState.stack_position > 1 )
		{
			ref_instance_t *oldRI = &glState.stack[glState.stack_position - 2];
			if( FBitSet( oldRI->params, RP_SCREENVIEW ))			
				return true;
		}
	}

	for( int i = 0; i < end; i++ )
	{
		if( R_CheckMirrorClone( surf, prevRI->subview_faces[i] ))
			return true;			

		if( R_CheckPortalSource( surf, prevRI->subview_faces[i] ))
			return true;

		if( R_CheckScreenSource( surf, prevRI->subview_faces[i] ))
			return true;
	}

	return false;
}

void R_CalcSubviewMatrix( int subtexturenum, const matrix4x4 &viewmatrix, cl_entity_t *camera )
{
	matrix4x4	worldView, modelView, projection;

	if( subtexturenum <= 0 )
		return;

	R_SetupModelviewMatrix( worldView );
	R_SetupProjectionMatrix( RI->fov_x, RI->fov_y, projection );

	// create personal projection matrix for mirror
	if( R_StaticEntity( camera ))
	{
		tr.subviewTextures[subtexturenum-1].matrix = projection.Concat( worldView );
	}
	else
	{
		modelView = worldView.ConcatTransforms( viewmatrix );
		tr.subviewTextures[subtexturenum-1].matrix = projection.Concat( modelView );
	}
}

void R_DrawSubviewPases( void )
{
	int		viewport[4];
	ref_instance_t	*prevRI;
	cl_entity_t	*camera;
	unsigned int	oldFBO;

	R_PushRefState();
	prevRI = R_GetPrevInstance();

	for( int i = 0; i < prevRI->num_subview_faces; i++ )
	{
		msurface_t *surf = prevRI->subview_faces[i];
		int texFlags = 0, subview;
		matrix4x4	viewmatrix;

		if( R_CanSkipPass( i, surf, prevRI ))
			continue;

		// handle camera
		if( !FBitSet( surf->flags, SURF_REFLECT ))
		{
			if( surf->info->parent->curstate.sequence <= 0 )
				continue;	// target is missed
			camera = GET_ENTITY( surf->info->parent->curstate.sequence );
		}
		else camera = surf->info->parent; // mirror camera is himself

		// NOTE: we can do additionaly culling here by PVS
		if( !camera || camera->curstate.messagenum != r_currentMessageNum )
			continue; // bad camera, ignore

		// setup view apropriate by type
		if( FBitSet( surf->flags, SURF_REFLECT ))
		{
			texFlags = R_SetupMirrorView( surf, camera, viewmatrix );
			r_stats.c_mirror_passes++;
		}
		else if( FBitSet( surf->flags, SURF_PORTAL ))
		{
			texFlags = R_SetupPortalView( surf, camera, viewmatrix );
			r_stats.c_portal_passes++;
		}
		else if( FBitSet( surf->flags, SURF_SCREEN ))
		{
			texFlags = R_SetupScreenView( surf, camera, viewmatrix );
			r_stats.c_screen_passes++;
		}
		else continue; // ???

		memcpy( viewport, RI->viewport, sizeof( viewport ));
		oldFBO = glState.frameBuffer;

		if( GL_Support( R_FRAMEBUFFER_OBJECT ))
		{
			if(( subview = R_AllocateSubviewTexture( viewport, texFlags )) == 0 )
				continue;
			R_CalcSubviewMatrix( subview, viewmatrix, camera );
		}

		// reset the subinfo
		surf->info->subtexture[glState.stack_position-1] = 0;
		RI->reject_face = surf;
		R_RenderScene();
		RI->reject_face = NULL;

		if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		{
			subview = R_AllocateSubviewTexture( viewport, texFlags );
			R_CalcSubviewMatrix( subview, viewmatrix, camera );
		}

		surf->info->subtexture[glState.stack_position-1] = subview; // now it's valid
		GL_BindFBO( oldFBO );
		R_ResetRefState();
	}

	prevRI->num_subview_faces = 0;
	R_PopRefState();
}

void R_WorldFindMirrors( void )
{
	CFrustum		*frustum = &RI->frustum;
	msurface_t	*surf, **mark;
	mworldleaf_t	*leaf;
	int		i, j;

	if( !CVAR_TO_BOOL( r_allow_mirrors ))
		return; // disabled

	memset( RI->visfaces, 0x00, ( world->numsortedfaces + 7) >> 3 );

	// always skip the leaf 0, because is outside leaf
	for( i = 1, leaf = &world->leafs[1]; i < world->numleafs; i++, leaf++ )
	{
		if( CHECKVISBIT( RI->visbytes, leaf->cluster ) && ( leaf->efrags || leaf->nummarksurfaces ))
		{
			if( RI->frustum.CullBox( leaf->mins, leaf->maxs ))
				continue;

			if( leaf->nummarksurfaces )
			{
				for( j = 0, mark = leaf->firstmarksurface; j < leaf->nummarksurfaces; j++, mark++ )
					SETVISBIT( RI->visfaces, *mark - worldmodel->surfaces );
			}
		}
	}

	// create drawlist for faces, do additional culling for world faces
	for( i = 0; i < world->numsortedfaces; i++ )
	{
		j = world->sortedfaces[i];

		if( j >= worldmodel->nummodelsurfaces )
			continue;	// not a world face

		surf = worldmodel->surfaces + j;

		if( !FBitSet( surf->flags, SURF_REFLECT ))
			continue;	// only mirrors interesting us

		if( CHECKVISBIT( RI->visfaces, j ))
		{
			if( R_CullSurface( surf ))
				continue;

			if( RI->num_subview_faces >= MAX_SUBVIEWS )
				return; // limit is out

			RI->subview_faces[RI->num_subview_faces] = surf;
			surf->info->parent = RI->currententity;
			RI->num_subview_faces++;

		}
	}
}

/*
=================
R_FindBmodelSubview

Check all bmodel surfaces and store subview faces
=================
*/
void R_FindBmodelSubview( cl_entity_t *e )
{
	Vector		absmin, absmax;
	model_t		*clmodel;
	msurface_t	*psurf;
	mplane_t		plane;

	clmodel = e->model;

	// disabled screen
	if( FBitSet( e->curstate.effects, EF_SCREEN ) && !e->curstate.body )
		return;

	gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];

	if( e->angles != g_vecZero )
	{
		// cull portals accuracy
		TransformAABB( glm->transform, clmodel->mins, clmodel->maxs, absmin, absmax );
	}
	else
	{
		absmin = e->origin + clmodel->mins;
		absmax = e->origin + clmodel->maxs;
	}

	if( !Mod_CheckBoxVisible( absmin, absmax ))
		return;

	if( R_CullModel( e, absmin, absmax ))
		return;

	if( FBitSet( e->curstate.effects, EF_SCREEN|EF_PORTAL ))
	{
		if( e->angles != g_vecZero )
			tr.modelorg = glm->transform.VectorITransform( RI->vieworg );
		else tr.modelorg = RI->vieworg - e->origin;
	}
	else
	{
		// mirror handled by pvsorigin
		if( e->angles != g_vecZero )
			tr.modelorg = glm->transform.VectorITransform( RI->pvsorigin );
		else tr.modelorg = RI->pvsorigin - e->origin;
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		// not a subview faces
		if( !FBitSet( psurf->flags, SURF_REFLECT|SURF_PORTAL|SURF_SCREEN ))
			continue;

		if( FBitSet( psurf->flags, SURF_REFLECT ) && !CVAR_TO_BOOL( r_allow_mirrors ))
			continue;

		if( FBitSet( psurf->flags, SURF_SCREEN ) && !CVAR_TO_BOOL( r_allow_screens ))
			continue;

		if( FBitSet( psurf->flags, SURF_PORTAL ) && !CVAR_TO_BOOL( r_allow_portals ))
			continue;

		if( FBitSet( psurf->flags, SURF_DRAWTURB ))
		{
			if( FBitSet( psurf->flags, SURF_PLANEBACK ))
				SetPlane( &plane, -psurf->plane->normal, -psurf->plane->dist );
			else SetPlane( &plane, psurf->plane->normal, psurf->plane->dist );

			if( e->hCachedMatrix != WORLD_MATRIX )
				glm->transform.TransformPositivePlane( plane, plane );

			if( psurf->plane->type != PLANE_Z && !FBitSet( e->curstate.effects, EF_WATERSIDES ))
				continue;

			if( absmin[2] + 1.0 >= plane.dist )
				continue;
		}

		if( R_CullSurface( psurf ))
			continue;

		if( RI->num_subview_faces >= MAX_SUBVIEWS )
			return; // limit is out

		RI->subview_faces[RI->num_subview_faces] = psurf;
		psurf->info->parent = RI->currententity;
		RI->num_subview_faces++;
	}
}

/*
=================
R_FindSubviewEnts

Check all bmodels for subview surfaces
=================
*/
void R_FindSubviewEnts( void )
{
	int	i;

	// check solid entities
	for( i = 0; i < tr.num_solid_entities; i++ )
	{
		RI->currententity = tr.solid_entities[i];
		RI->currentmodel = RI->currententity->model;

		if( RI->currentmodel->type != mod_brush )
			continue;

		if( RI->num_subview_faces >= MAX_SUBVIEWS )
			return; // limit is out

		R_FindBmodelSubview( RI->currententity );
	}

	// check translucent entities
	for( i = 0; i < tr.num_trans_entities; i++ )
	{
		RI->currententity = tr.trans_entities[i];
		RI->currentmodel = RI->currententity->model;
	
		if( RI->currentmodel->type != mod_brush )
			continue;

		if( RI->num_subview_faces >= MAX_SUBVIEWS )
			return; // limit is out

		R_FindBmodelSubview( RI->currententity );
	}
}

/*
================
R_RenderSubview

find all the subview faces
================
*/
void R_RenderSubview( void )
{
	int	flags = world->features;

	if( glState.stack_position > (unsigned int)r_recursion_depth->value )
		return; // too deep...

	if( FBitSet( RI->params, RP_OVERVIEW ))
		return;

	// no mirrors, portals or screens anyway
	if( !FBitSet( flags, WORLD_HAS_MIRRORS ) && !FBitSet( flags, WORLD_HAS_SCREENS ) && !FBitSet( flags, WORLD_HAS_PORTALS ))
		return;

	// player is outside world. Don't draw subview for speedup reasons
	if( RP_OUTSIDE( RI->viewleaf ))
		return;

	if( !FBitSet( RI->params, RP_OLDVIEWLEAF ))
		R_FindViewLeaf();
	R_SetupFrustum();
	R_MarkLeaves ();

	if( FBitSet( RI->params, RP_MIRRORVIEW ))
		tr.modelorg = RI->pvsorigin;
	else tr.modelorg = RI->vieworg;
	RI->currententity = GET_ENTITY( 0 );
	RI->currentmodel = RI->currententity->model;
	RI->num_subview_faces = 0;

	R_WorldFindMirrors();	// yes, only mirrors allow in the world
	R_FindSubviewEnts();	// include mirrors, portals and screens

	if( RI->num_subview_faces != 0 )
		R_DrawSubviewPases();
}
