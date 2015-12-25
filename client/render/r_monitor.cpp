/*
r_monitor.cpp - draw screen surfaces
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
#include "r_view.h"
#include "pm_movevars.h"

/*
================
R_BeginDrawScreen

Setup texture matrix for mirror texture
================
*/
void R_BeginDrawScreen( msurface_t *fa )
{
	cl_entity_t *ent;

	ent = RI.currententity;

	if( FBitSet( ent->curstate.iuser1, CF_MONOCHROME ) && cg.screen_shader )
	{
		pglEnable( GL_FRAGMENT_PROGRAM_ARB );
		pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, cg.screen_shader );
	}
}

/*
================
R_EndDrawScreen

Restore identity texmatrix
================
*/
void R_EndDrawScreen( void )
{
	cl_entity_t *ent = RI.currententity;

	if( FBitSet( ent->curstate.iuser1, CF_MONOCHROME ) && cg.screen_shader )
	{
		pglDisable( GL_FRAGMENT_PROGRAM_ARB );
	}
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
void R_SetupScreenView( cl_entity_t *camera )
{
	Vector origin, angles;
	cl_entity_t *ent;
	float fov = 90;

	if( camera->player && UTIL_IsLocal( camera->curstate.number ))
	{
		// player seen through camera. Restore firstperson view here
		if( RI.refdef.viewentity > RI.refdef.maxclients )
			V_CalcFirstPersonRefdef( &RI.refdef );

		RI.pvsorigin = RI.refdef.vieworg;
		return; // already has a valid view
          }

	ent = RI.currententity;
	origin = camera->origin;
	angles = camera->angles;

	// interpolate position for monsters
	if( camera->curstate.movetype == MOVETYPE_STEP ) 
	{
		float f;
                    
		// don't do it if the goalstarttime hasn't updated in a while.
		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
		if(( GET_CLIENT_TIME() < camera->curstate.animtime + 1.0f ) && ( camera->curstate.animtime != camera->latched.prevanimtime ))
		{
			f = (GET_CLIENT_TIME() - camera->curstate.animtime) / (camera->curstate.animtime - camera->latched.prevanimtime);
		}

		if( !( camera->curstate.effects & EF_NOINTERP ))
		{
			// ugly hack to interpolate angle, position.
			// current is reached 0.1 seconds after being set
			f = f - 1.0f;
		}
		else
		{
			f = 0.0f;
		}

		InterpolateOrigin( camera->latched.prevorigin, camera->origin, origin, f, true );
		InterpolateAngles( camera->latched.prevangles, camera->angles, angles, f, true );
	}

	studiohdr_t *viewmonster = (studiohdr_t *)IEngineStudio.Mod_Extradata( camera->model );

	if( viewmonster && camera->curstate.eflags & EFLAG_SLERP )
	{
		Vector forward;
		AngleVectors( angles, forward, NULL, NULL );

		Vector viewpos = viewmonster->eyeposition;

		if( viewpos == g_vecZero )
			viewpos = Vector( 0, 0, 8 );	// monster_cockroach

		origin += viewpos + forward * 8;	// best value for humans
	}

	// smooth step stair climbing
	// lasttime goes into ent->latched.sequencetime
	// oldz goes into ent->latched.prevanimtime
	if( origin[2] - ent->latched.prevanimtime > 0.0f )
	{
		float steptime;
		
		steptime = RI.refdef.time - ent->latched.sequencetime;
		if( steptime < 0 ) steptime = 0;

		ent->latched.prevanimtime += steptime * 150.0f;

		if( ent->latched.prevanimtime > origin[2] )
			ent->latched.prevanimtime = origin[2];
		if( origin[2] - ent->latched.prevanimtime > RI.refdef.movevars->stepsize )
			ent->latched.prevanimtime = origin[2] - RI.refdef.movevars->stepsize;

		origin[2] += ent->latched.prevanimtime - origin[2];
	}
	else
	{
		ent->latched.prevanimtime = origin[2];
	}

	ent->latched.sequencetime = RI.refdef.time;

 	// setup the screen fov
 	if( ent->curstate.fuser2 != 0.0f )
 		fov = bound( 10, ent->curstate.fuser2, 120 );

 	// setup the screen FOV
 	if( RI.viewport[2] == RI.viewport[3] )
 	{
  		RI.refdef.fov_x = fov;
 		RI.refdef.fov_y = fov;
 	}
	else
	{
 		RI.refdef.fov_x = fov;
 		RI.refdef.fov_y = V_CalcFov( RI.refdef.fov_x, RI.viewport[2], RI.viewport[3] );
	}

	RI.refdef.viewangles[0] = anglemod( angles[0] );
	RI.refdef.viewangles[1] = anglemod( angles[1] );
	RI.refdef.viewangles[2] = anglemod( angles[2] );
	RI.refdef.vieworg = RI.pvsorigin = origin;
}

/*
================
R_AllocateScreenTexture

Allocate the screen texture and make copy
================
*/
int R_AllocateScreenTexture( bool copyToImage = true )
{
	int i = tr.num_screens_used;

	if( i >= MAX_MIRRORS )
	{
		ALERT( at_error, "R_AllocateScreenTexture: screen textures limit exceeded!\n" );
		return 0;	// disable
	}

	int texture = tr.screenTextures[i];
	tr.num_screens_used++;

	if( !texture )
	{
		char txName[16];

		Q_snprintf( txName, sizeof( txName ), "*screencopy%i", i );

		// create new screen texture
		tr.screenTextures[i] = CREATE_TEXTURE( txName, RI.viewport[2], RI.viewport[3], NULL, TF_SCREEN ); 
		texture = tr.screenTextures[i];
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
R_DrawScreens

Draw all viewpasess from monitor camera position
Screen textures will be drawn in normal pass
================
*/
void R_DrawScreens( cl_entity_t *ignoreent )
{
	ref_instance_t	oldRI;
	int		i, j, oldframecount;
	mextrasurf_t	*es, *tmp, *screenchain;
	Vector		forward, right, up;
	Vector		origin, angles;
	cl_entity_t	*e, *e2, *camera;
	int		screentexture;
	mleaf_t		*leaf, *leaf2;
	model_t		*m;

	if( !tr.num_screen_entities ) return; // no screens for this frame

	oldRI = RI; // make refinst backup
	oldframecount = tr.framecount;

	leaf = r_viewleaf;
	leaf2 = r_viewleaf2;

	for( i = 0; i < tr.num_screen_entities; i++ )
	{
		RI.currententity = e = tr.screen_entities[i].ent;
		RI.currentmodel = m = RI.currententity->model;

		assert( RI.currententity != NULL );
		assert( RI.currentmodel != NULL );

		screentexture = 0;

		if( e == ignoreent )
			continue; // don't draw himself

		// handle screen camera
		camera = GET_ENTITY( e->curstate.sequence );

		if( !camera || camera->curstate.messagenum != r_currentMessageNum )
			continue; // bad camera, skip to next screen

		// NOTE: copy screentexture from another entity that has same camera
		// and potentially reduce number of viewpasses

		// make sure what we have one pass at least
		if( i != 0 )
		{
			for( j = 0; j < i; j++ )
			{
				e2 = tr.screen_entities[j].ent;
				screenchain = tr.screen_entities[j].chain;

				if( e->curstate.sequence != e2->curstate.sequence )
					continue;	// different cameras
				if( e->curstate.fuser2 != e2->curstate.fuser2 )
					continue;	// different fov specified
				break; // found screen with same camera				
			}

			if( i != j )
			{
				// apply the screen texture for all the surfaces of this entity
				for( es = tr.screen_entities[i].chain; es != NULL; es = es->mirrorchain )
				{
					// we don't use texture matrix for screens so just clear it
					es->mirrormatrix.Identity();
					es->mirrortexturenum = screenchain->mirrortexturenum; // just get from first surface
				}
				continue;	// we already has the screencopy for this entity. Ignore pass
			}
		}

		es = tr.screen_entities[i].chain;

		msurface_t *s = INFO_SURF( es, RI.currentmodel );

		if( s && s->texinfo && s->texinfo->texture && GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
		{
			texture_t *t = s->texinfo->texture;

			if(( t->width == 64 ) && ( t->height == 64 )) 
                              {
				// HACKHACK: for default texture
				RI.viewport[2] = RI.viewport[3] = 512;
                              }
                              else
                              {
				RI.viewport[2] = bound( 64, t->width * 2, 1024 );
				RI.viewport[3] = bound( 64, t->height * 2, 1024 );
			}
		}
		else RI.viewport[2] = RI.viewport[3] = 512;

		R_SetupScreenView( camera );

		RI.params = RP_SCREENVIEW;
		if( camera->player ) RI.params |= RP_FORCE_NOPLAYER; // camera set to player don't draw him

		RI.clipFlags |= ( 1<<5 );

		if( r_allow_mirrors->value && tr.insideView < MAX_MIRROR_DEPTH && !( oldRI.params & RP_SCREENVIEW ))
		{
			if( R_FindMirrors( &RI.refdef ))
			{
				// render mirrors
				tr.insideView++;
				R_DrawMirrors ();
				tr.insideView--;
			}
		}

		if( r_allow_portals->value && tr.insideView < MAX_MIRROR_DEPTH && !( oldRI.params & RP_PORTALVIEW ))
		{
			if( R_FindPortals( &RI.refdef ))
			{
				// render portals
				tr.insideView++;
				R_DrawPortals ();
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
			screentexture = R_AllocateScreenTexture( false );
			GL_BindFrameBuffer( tr.fbo[FBO_SCREENS], screentexture );
		}

		R_RenderScene( &RI.refdef );

		if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
			screentexture = R_AllocateScreenTexture();
		r_stats.c_screen_passes++;

		// apply the screen texture for all the surfaces of this entity
		for( es = tr.screen_entities[i].chain; es != NULL; es = es->mirrorchain )
		{
			// we don't use texture matrix for screens so just clear it
			es->mirrormatrix.Identity();
			es->mirrortexturenum = screentexture;
		}

		RI = oldRI; // restore ref instance
		r_viewleaf = leaf;
		r_viewleaf2 = leaf2;
	}

	// clear the screen chains
	for( i = 0; i < tr.num_screen_entities; i++ )
	{
		for( es = tr.screen_entities[i].chain; es != NULL; )
		{
			tmp = es->mirrorchain;
			es->mirrorchain = NULL;
			es = tmp;
		}

		tr.screen_entities[i].chain = NULL;
		tr.screen_entities[i].ent = NULL;
	}

	if( GL_Support( R_FRAMEBUFFER_OBJECT ))
	{
		// revert to main buffer
		GL_BindFrameBuffer( FBO_MAIN, 0 );
	}

	r_viewleaf = r_viewleaf2 = NULL;	// force markleafs next frame
	tr.framecount = oldframecount;	// restore real framecount
	tr.num_screen_entities = 0;
}

/*
=================
R_FindBmodelScreens

Check all bmodel surfaces and make personal screen chain
=================
*/
void R_FindBmodelScreens( cl_entity_t *e, bool static_entity, int &numScreens )
{
	mextrasurf_t	*extrasurf;
	Vector		mins, maxs;
	msurface_t	*psurf;
	model_t		*clmodel;
	bool		rotated;
	int		i, clipFlags;

	// fast reject
	if( !( e->curstate.effects & EF_SCREEN ))
		return;

	// disabled screen
	if( !e->curstate.body ) return; 

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
		if(!( psurf->flags & SURF_SCREEN ))
			continue;

		if( R_CullSurface( psurf, clipFlags ))
			continue;

		extrasurf = SURF_INFO( psurf, RI.currentmodel );
		extrasurf->mirrorchain = tr.screen_entities[tr.num_screen_entities].chain;
		tr.screen_entities[tr.num_screen_entities].chain = extrasurf;
	}

	// store new screen entity
	if( tr.screen_entities[tr.num_screen_entities].chain != NULL )
	{
		tr.screen_entities[tr.num_screen_entities].ent = RI.currententity;
		tr.num_screen_entities++;
		numScreens++;
	}
}

/*
=================
R_CheckScreenEntitiesOnList

Check all bmodels for screen surfaces
=================
*/
int R_CheckScreenEntitiesOnList( void )
{
	int	i, numScreens = 0;

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
			R_FindBmodelScreens( RI.currententity, true, numScreens );
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
			R_FindBmodelScreens( RI.currententity, false, numScreens );
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
			R_FindBmodelScreens( RI.currententity, false, numScreens );
			break;
		}
	}

	return numScreens;
}

/*
================
R_FindScreens

Build screen chains for this frame
================
*/
bool R_FindScreens( const ref_params_t *fd )
{
	if( !tr.world_has_screens || !worldmodel || RI.refdef.paused || RI.drawOrtho )
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

	if( R_CheckScreenEntitiesOnList( ))
		return true;
	return false;
}
