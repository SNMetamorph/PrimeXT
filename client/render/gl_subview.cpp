/*
gl_mirror.cpp - draw reflected surfaces
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
#include "gl_world.h"
#include "mathlib.h"
#include <stringlib.h>
#include "gl_occlusion.h"
#include "gl_cvars.h"

#define MIRROR_PLANE_EPSILON		0.1f

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
texFlags_t R_SetupMirrorView( msurface_t *surf, ref_viewpass_t *rvp )
{
	cl_entity_t	*ent = surf->info->parent;
	matrix3x3		matAngles;
	mplane_t		plane;
	gl_state_t	*glm;
	float		d;

	// setup mirror plane
	if( FBitSet( surf->flags, SURF_PLANEBACK ))
		SetPlane( &plane, -surf->plane->normal, -surf->plane->dist );
	else SetPlane( &plane, surf->plane->normal, surf->plane->dist );

	glm = GL_GetCache( ent->hCachedMatrix );

	if( ent->hCachedMatrix != WORLD_MATRIX )
		glm->transform.TransformPositivePlane( plane, plane );

	// reflect view by mirror plane
	d = -2.0f * ( DotProduct( GetVieworg(), plane.normal ) - plane.dist );
	Vector origin = GetVieworg() + d * plane.normal;

	d = -2.0f * DotProduct( GetVForward(), plane.normal );
	Vector forward = ( GetVForward() + d * plane.normal ).Normalize();

	d = -2.0f * DotProduct( GetVRight(), plane.normal );
	Vector right = ( GetVRight() + d * plane.normal ).Normalize();

	d = -2.0f * DotProduct( GetVUp(), plane.normal );
	Vector up = ( GetVUp() + d * plane.normal ).Normalize();

	// compute mirror angles
	matAngles.SetForward( forward );
	matAngles.SetRight( right );
	matAngles.SetUp( up );
	Vector angles = matAngles.GetAngles();

	plane.dist += ON_EPSILON; // to prevent z-fighting with reflective water

	rvp->flags = RP_MIRRORVIEW|RP_CLIPPLANE|RP_MERGE_PVS;
	RI->view.frustum.SetPlane( FRUSTUM_NEAR, plane.normal, plane.dist );
	RI->clipPlane = plane;

	rvp->viewangles[0] = anglemod( angles[0] );
	rvp->viewangles[1] = anglemod( angles[1] );
	rvp->viewangles[2] = anglemod( angles[2] );
	rvp->vieworigin = origin;

	rvp->fov_x = RI->view.fov_x;
	rvp->fov_y = RI->view.fov_y;

	// put pvsorigin before the mirror plane to avoid get recursion with himself
	if( ent == GET_ENTITY( 0 )) origin = surf->info->origin + (1.0f * plane.normal);
	else origin = glm->transform.VectorTransform( surf->info->origin ) + (1.0f * plane.normal);
	material_t *mat = R_TextureAnimation( surf )->material;

	RI->view.pvspoint = origin;
	rvp->viewport[0] = rvp->viewport[1] = 0;

	if( FBitSet( surf->flags, SURF_REFLECT_PUDDLE ))
	{
		rvp->viewport[2] = 192;
		rvp->viewport[3] = 192;
	}
	else
	{
		rvp->viewport[2] = RI->view.port[2];
		rvp->viewport[3] = RI->view.port[3];
	}

	if( GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		// allow screen size
//		rvp->viewport[2] = bound( 96, rvp->viewport[2], 1024 );
//		rvp->viewport[3] = bound( 72, rvp->viewport[3], 768 );
	}
	else
	{
		rvp->viewport[2] = NearestPOW( rvp->viewport[2], true );
		rvp->viewport[3] = NearestPOW( rvp->viewport[3], true );
//		rvp->viewport[2] = bound( 128, rvp->viewport[2], 1024 );
//		rvp->viewport[3] = bound( 64, rvp->viewport[3], 512 );
	}

	if( FBitSet( mat->flags, BRUSH_LIQUID ))
		SetBits( rvp->flags, RP_WATERPASS );
	else if( FBitSet( surf->flags, SURF_REFLECT_PUDDLE ))
		SetBits( rvp->flags, RP_NOSHADOWS|RP_WATERPASS|RP_NOGRASS ); // don't draw grass from puddles

	return static_cast<texFlags_t>(TF_SCREEN);
}

/*
================
R_AllocateSubviewTexture

Allocate the screen texture and make copy
================
*/
int R_AllocateSubviewTexture( int viewport[4], texFlags_t texFlags )
{
	int	i;

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
		tr.subviewTextures[i].texturenum = CREATE_TEXTURE( va( "*subview%i", i ), viewport[2], viewport[3], NULL, texFlags ); 
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
		GL_Bind( GL_TEXTURE0, tr.subviewTextures[i].texturenum );
		pglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, viewport[0], viewport[1], viewport[2], viewport[3], 0 );
	}

	return (i+1);
}

bool R_CheckMirrorClone( msurface_t *surf, msurface_t *check )
{
	if( !FBitSet( surf->flags, SURF_REFLECT|SURF_REFLECT_PUDDLE ))
		return false;

	if( !FBitSet( check->flags, SURF_REFLECT|SURF_REFLECT_PUDDLE ))
		return false;

	if( !check->info->subtexture[glState.stack_position-1] )
		return false;

	if( surf->info->parent != check->info->parent )
		return false;

	if( FBitSet( surf->flags, SURF_PLANEBACK ) != FBitSet( check->flags, SURF_PLANEBACK ))
		return false;

	if( surf->plane->normal != check->plane->normal )
		return false;

	if( Q_sign( surf->plane->dist ) != Q_sign( check->plane->dist ))
		return false;

	if( fabs( surf->plane->dist - check->plane->dist ) > MIRROR_PLANE_EPSILON )
		return false;

	// just reuse the handle
	surf->info->subtexture[glState.stack_position-1] = check->info->subtexture[glState.stack_position-1];

	return true;
}

bool R_CanSkipPass( int end, msurface_t *surf, ref_instance_t *prevRI )
{
	// IMPORTANT: limit the recursion depth
	if( surf == prevRI->reject_face )
		return true;

	for( int i = 0; i < end; i++ )
	{
		if( R_CheckMirrorClone( surf, prevRI->frame.subview_faces[i] ))
			return true;			
	}

	// occluded by query
	if( FBitSet( surf->flags, SURF_OCCLUDED ))
	{
		// don't use me
		surf->info->subtexture[glState.stack_position-1] = 0;
		return true;
          }

	return false;
}

/*
================
R_CheckOutside

check if main view is outside level
================
*/
bool R_CheckOutside( void )
{
	// only main view should be tested
	if( !RP_NORMALPASS( ))
		return false;

	mleaf_t	*leaf = Mod_PointInLeaf( RI->view.origin, worldmodel->nodes );

	if( RP_OUTSIDE( leaf ))
		return true;
	return false;
}

/*
================
R_RenderSubview

Draw scene from another points
e.g. for planar reflection,
remote cameras etc
================
*/
void R_RenderSubview( void )
{
	ref_instance_t	*prevRI;
	unsigned int	oldFBO;
	ref_viewpass_t	rvp;

	// player is outside world. Don't draw subview for speedup reasons
	if( R_CheckOutside( ))
		return;

	if( glState.stack_position > (unsigned int)r_recursion_depth->value )
		return; // too deep...

	if( FBitSet( RI->params, RP_DRAW_OVERVIEW ))
		return;

	if( !RI->frame.num_subview_faces )
		return; // nothing to render

	R_PushRefState(); // make refinst backup
	prevRI = R_GetPrevInstance();

	// draw the subviews through the list
	for( int i = 0; i < prevRI->frame.num_subview_faces; i++ )
	{
		msurface_t *surf = prevRI->frame.subview_faces[i];
		cl_entity_t *e = RI->currententity = surf->info->parent;
		mextrasurf_t *es = surf->info;
		texFlags_t texFlags = TF_COLORMAP;
		int subview = 0;
		RI->currentmodel = e->model;

		if( R_CanSkipPass( i, surf, prevRI ))
			continue;

		ASSERT( RI->currententity != NULL );
		ASSERT( RI->currentmodel != NULL );

		// NOTE: we can do additionaly culling here by PVS
		if( !e || e->curstate.messagenum != r_currentMessageNum )
			continue; // bad camera, ignore

		// setup view apropriate by type
		if( FBitSet( surf->flags, SURF_REFLECT|SURF_REFLECT_PUDDLE ))
		{
			texFlags = R_SetupMirrorView( surf, &rvp );
			r_stats.c_subview_passes++;
		}
		else continue; // ???

		oldFBO = glState.frameBuffer;

		if( GL_Support( R_FRAMEBUFFER_OBJECT ))
		{
			if(( subview = R_AllocateSubviewTexture( rvp.viewport, texFlags )) == 0 )
				continue;
		}

		// reset the subinfo
		surf->info->subtexture[glState.stack_position-1] = 0;
		RI->reject_face = surf;
		R_RenderScene( &rvp, rvp.flags );
		RI->reject_face = NULL;

		if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
			subview = R_AllocateSubviewTexture( rvp.viewport, texFlags );

		ASSERT( subview > 0 && subview < MAX_SUBVIEW_TEXTURES );
		surf->info->subtexture[glState.stack_position-1] = subview; // now it's valid
		tr.subviewTextures[subview-1].matrix = RI->view.worldProjectionMatrix;

		GL_BindFBO( oldFBO );
		R_ResetRefState();
	}

	R_PopRefState(); // restore ref instance
}