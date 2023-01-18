/*
gl_rmain.cpp - renderer main loop
Copyright (C) 2013 Uncle Mike

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
#include "entity_types.h"
#include "gl_local.h"
#include <mathlib.h>
#include "gl_aurora.h"
#include "gl_rpart.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "event_api.h"
#include "gl_world.h"
#include "gl_grass.h"
#include "gl_cvars.h"
#include "gl_debug.h"
#include "r_weather.h"
#include "tri.h"

ref_globals_t	tr;
ref_instance_t	*RI = NULL;
ref_stats_t	r_stats;
ref_buildstats_t	r_buildstats;
char		r_speeds_msg[2048];
char		r_depth_msg[2048];
model_t		*worldmodel = NULL;
float		gldepthmin, gldepthmax;
int		sunSize[MAX_SHADOWMAPS] = { 1024, 1024, 1024, 1024 };
bool g_fRenderInitialized = false;

bool R_SkyIsVisible( void )
{
	return FBitSet( RI->view.flags, RF_SKYVISIBLE ) ? true : false;
}

void R_InitRefState( void )
{
	glState.stack_position = 0;
	RI = &glState.stack[glState.stack_position];
}

const char *R_GetNameForView( void )
{
	static char	passName[256];

	passName[0] = '\0';

	if (FBitSet(RI->params, RP_MIRRORVIEW))
		Q_strncat(passName, "mirror ", sizeof(passName));
	if (FBitSet(RI->params, RP_ENVVIEW))
		Q_strncat(passName, "cubemap ", sizeof(passName));
	if (FBitSet(RI->params, RP_SKYPORTALVIEW))
		Q_strncat(passName, "3d_sky ", sizeof(passName));
	if (FBitSet(RI->params, RP_SKYVIEW))
		Q_strncat(passName, "skybox ", sizeof(passName));
	if (FBitSet(RI->params, RP_PORTALVIEW))
		Q_strncat(passName, "portal ", sizeof(passName));
	if (FBitSet(RI->params, RP_SCREENVIEW))
		Q_strncat(passName, "screen ", sizeof(passName));
	if (FBitSet(RI->params, RP_SHADOWVIEW))
		Q_strncat(passName, "shadow ", sizeof(passName));
	if (RP_NORMALPASS())
		Q_strncat(passName, "general ", sizeof(passName));

	return passName;	
}

/*
===============
R_BuildViewPassHierarchy

debug thing
===============
*/
void R_BuildViewPassHierarchy( void )
{
	char	empty[MAX_REF_STACK];

	if( (int)r_speeds->value == 5 )
	{
		int	num_faces = 0;

		if( glState.stack_position > 0 )
			num_faces = R_GetPrevInstance()->frame.num_subview_faces;
		unsigned int i;
		for( i = 0; i < glState.stack_position; i++ )
			empty[i] = ' ';
		empty[i] = '\0';

		// build pass hierarchy
		const char *string = va( "%s->%d %s (%d subview)\n", empty, glState.stack_position, R_GetNameForView( ), num_faces );
		Q_strncat( r_depth_msg, string, sizeof( r_depth_msg ));
	}
}

void R_ClearFrameLists( void )
{
	RI->frame.solid_faces.Purge();
	RI->frame.solid_meshes.Purge();
	RI->frame.grass_list.Purge();
	RI->frame.trans_list.Purge();
	RI->frame.light_meshes.Purge();
	RI->frame.light_faces.Purge();
	RI->frame.light_grass.Purge();
	RI->frame.primverts.Purge();
	RI->frame.num_subview_faces = 0;
}

void R_ResetRefState( void )
{
	ref_instance_t	*prevRI;

	ASSERT( glState.stack_position > 0 );

	prevRI = &glState.stack[glState.stack_position - 1];
	RI = &glState.stack[glState.stack_position];

	// copy params from old refresh
	RI->params = RP_NONE;
	memcpy( &RI->view, &prevRI->view, sizeof( ref_viewcache_t ));
	memcpy( &RI->glstate, &prevRI->glstate, sizeof( ref_glstate_t ));

	RI->view.client_frame = 0;
	R_ClearFrameLists();

	// send info about visible lights
	if( FBitSet( prevRI->view.flags, RF_HASDYNLIGHTS ))
		SetBits( RI->view.flags, RF_HASDYNLIGHTS );

	RI->currententity = NULL;
	RI->currentmodel = NULL;
	RI->currentlight = NULL;
	GL_BindShader( NULL );
}

void R_PushRefState( void )
{
	if( ++glState.stack_position >= MAX_REF_STACK )
		HOST_ERROR( "Refresh stack overflow\n" );

	RI = &glState.stack[glState.stack_position];
	R_ResetRefState();
}

void R_PopRefState( void )
{
	if( --glState.stack_position < 0 )
		HOST_ERROR( "Refresh stack underflow\n" );
	RI = &glState.stack[glState.stack_position];

	// frametime is valid only for normal pass
	if( RP_NORMALPASS( ))
		tr.frametime = tr.saved_frametime;
	else tr.frametime = 0.0;

	// need to rebuild scissors
	R_SetupDynamicLights();
}

ref_instance_t *R_GetPrevInstance( void )
{
	ASSERT( glState.stack_position > 0 );

	return &glState.stack[glState.stack_position - 1];
}

const Vector gl_state_t :: GetModelOrigin( void )
{
	return transform.VectorITransform( RI->view.origin );
}

/*
===============
GL_CacheState

build matrix pack or find already existed
===============
*/
unsigned short GL_CacheState( const Vector &origin, const Vector &angles, bool skyentity )
{
	gl_state_t state;
	state.transform = matrix4x4( origin, angles, 1.0f );
	state.transform.CopyToArray( state.modelMatrix );

	for (int i = 0; i < tr.cached_state.Count(); i++)
	{
		// NOTE: (MVP == MV == M). So no reason to compare all the matrices
		if( !memcmp( tr.cached_state[i].modelMatrix, state.modelMatrix, sizeof( state.modelMatrix ))) {
			return i;
		}
	}

	// store results
	tr.cached_state.AddToTail( state );

	return tr.cached_state.Count() - 1;
}

/*
===============
GL_CacheState

build matrix pack or find already existed
===============
*/
gl_state_t *GL_GetCache( word hCachedMatrix )
{
	static gl_state_t	skycache;
	gl_state_t *glm;

	if( hCachedMatrix >= tr.cached_state.Count( ))
	{
		ASSERT( tr.cached_state.Count() > 0 );
		//ALERT( at_aiconsole, "GL_GetCache: requested invalid cachenum %d\n", hCachedMatrix );
		return &tr.cached_state[WORLD_MATRIX];
	}

	glm = &tr.cached_state[hCachedMatrix];
	return glm;
}

/*
===============
R_MarkWorldVisibleFaces

instead of RecursiveWorldNode
===============
*/
void R_MarkWorldVisibleFaces( model_t *model )
{
	float		maxdist = 0.0f;
	msurface_t	**mark;
	mleaf_t		*leaf;
	int		i, j;

	memset( RI->view.visfaces, 0x00, (worldmodel->numsurfaces + 7) >> 3 );
	memset( RI->view.vislight, 0x00, (world->numworldlights + 7) >> 3 );
	ClearBounds( RI->view.visMins, RI->view.visMaxs );

	if( !model ) return; // just clear visibility and out

	// always skip the leaf 0, because is outside leaf
	for( i = 1, leaf = &model->leafs[1]; i < model->numleafs + 1; i++, leaf++ )
	{
		mextraleaf_t *eleaf = LEAF_INFO( leaf, model );	// named like personal vaporisers manufacturer he-he

		if( CHECKVISBIT( RI->view.pvsarray, leaf->cluster ) && ( leaf->efrags || leaf->nummarksurfaces ))
		{
			if( RI->view.frustum.CullBox( eleaf->mins, eleaf->maxs ))
				continue;

			// do additional culling in dev_overview mode
			if( FBitSet( RI->params, RP_DRAW_OVERVIEW ) && R_CullNodeTopView( (mnode_t *)leaf ))
				continue;

			// deal with model fragments in this leaf
			if( leaf->efrags && RP_NORMALPASS( ))
				STORE_EFRAGS( &leaf->efrags, tr.realframecount );

			if( leaf->contents == CONTENTS_EMPTY )
			{
				// unrolled for speedup reasons
				RI->view.visMins[0] = Q_min( RI->view.visMins[0], eleaf->mins[0] );
				RI->view.visMaxs[0] = Q_max( RI->view.visMaxs[0], eleaf->maxs[0] );
				RI->view.visMins[1] = Q_min( RI->view.visMins[1], eleaf->mins[1] );
				RI->view.visMaxs[1] = Q_max( RI->view.visMaxs[1], eleaf->maxs[1] );
				RI->view.visMins[2] = Q_min( RI->view.visMins[2], eleaf->mins[2] );
				RI->view.visMaxs[2] = Q_max( RI->view.visMaxs[2], eleaf->maxs[2] );
			}

			r_stats.c_world_leafs++;

			if( leaf->nummarksurfaces )
			{
				for( j = 0, mark = leaf->firstmarksurface; j < leaf->nummarksurfaces; j++, mark++ )
				{
					msurface_t *surf = *mark;

					// construct grass and update leaf bounds
					if( surf->info->grasscount && RP_NORMALPASS( ))
						R_PrecacheGrass( surf, eleaf );
					SETVISBIT( RI->view.visfaces, *mark - model->surfaces );
				}
			}
		}
	}

	// now we have actual vismins\vismaxs and can calc farplane distance
	for( i = 0; i < 8; i++ )
	{
		Vector	v, dir;
		float	dist;

		v[0] = ( i & 1 ) ? RI->view.visMins[0] : RI->view.visMaxs[0];
		v[1] = ( i & 2 ) ? RI->view.visMins[1] : RI->view.visMaxs[1];
		v[2] = ( i & 4 ) ? RI->view.visMins[2] : RI->view.visMaxs[2];

		dir = v - RI->view.origin;
		dist = DotProduct( dir, dir );
		maxdist = Q_max( dist, maxdist );
	}

//	RI->view.farClip = sqrt( maxdist ) + 64.0f; // add some bias
}

/*
===============
R_SetupViewCache

check for changes and build visibility
===============
*/
static void R_SetupViewCache( const ref_viewpass_t *rvp )
{
	const ref_overview_t *ov = GET_OVERVIEW_PARMS();
	model_t *model = worldmodel;

	RI->view.changed = 0; // always clearing changes at start of frame

	if( !model && FBitSet( RI->params, RP_DRAW_WORLD ))
		HOST_ERROR( "R_SetupViewCache: NULL worldmodel\n" );

	// client frame was changed: refresh the draw lists
	// FIXME: get rid of this when renderer will be finalized
	if( RI->view.client_frame != tr.realframecount )
		SetBits( RI->view.changed, RC_FORCE_UPDATE );

	if( tr.params_changed )
		SetBits( RI->view.changed, RC_FORCE_UPDATE );

	// viewentity was changed
	if( RI->view.entity != rvp->viewentity )
	{
		SetBits( RI->view.changed, RC_FORCE_UPDATE );
		RI->view.entity = rvp->viewentity;
	}

	// viewport was changed, recompute
	if (RI->view.port.CompareWith(rvp->viewport))
	{
		RI->view.port = CViewport(rvp->viewport);
		if (RP_NORMALPASS( ) && !FBitSet( RI->params, RP_DEFERREDLIGHT ))
		{
			// set up viewport (main, playersetup)
			int x = floor(RI->view.port.GetX() * glState.width / glState.width);
			int x2 = ceil((RI->view.port.GetX() + RI->view.port.GetWidth()) * glState.width / glState.width);
			int y = floor(glState.height - RI->view.port.GetY() * glState.height / glState.height);
			int y2 = ceil(glState.height - (RI->view.port.GetY() + RI->view.port.GetHeight()) * glState.height / glState.height);

			RI->glstate.viewport.SetX(x);
			RI->glstate.viewport.SetY(y2);
			RI->glstate.viewport.SetWidth(x2 - x);
			RI->glstate.viewport.SetHeight(y - y2);
		}
		else {
			RI->glstate.viewport = RI->view.port; // additional passes
		}
	}

	// vieworigin was changed
	if( RI->view.origin != rvp->vieworigin )
	{
		if( !FBitSet( RI->params, RP_MERGE_PVS ))
			RI->view.pvspoint = rvp->vieworigin;
		SetBits( RI->view.changed, RC_ORIGIN_CHANGED );
		RI->view.origin = rvp->vieworigin;
	}

	if( RI->view.angles != rvp->viewangles )
	{
		SetBits( RI->view.changed, RC_ANGLES_CHANGED ); 
		RI->view.angles = rvp->viewangles;
	}

	// check if overview settings was changed
	if( FBitSet( RI->params, RP_DRAW_OVERVIEW ) && memcmp( &RI->view.over, ov, sizeof( RI->view.over )))
	{
		SetBits( RI->view.changed, RC_ORIGIN_CHANGED|RC_ANGLES_CHANGED|RC_FOV_CHANGED );
		memcpy( &RI->view.over, ov, sizeof( RI->view.over ));
	}

	if( FBitSet( RI->view.changed, RC_ORIGIN_CHANGED ) && ( model != NULL ))
	{
		mleaf_t	*leaf = Mod_PointInLeaf( RI->view.pvspoint, model->nodes );

		if(( RI->view.leaf == NULL ) || ( RI->view.leaf != leaf ))
		{
			SetBits( RI->view.changed, RC_VIEWLEAF_CHANGED ); 
			RI->view.leaf = leaf;
		}
	}

	// development cvars invoke to recompute PVS
	if(( RI->view.novis_cached != CVAR_TO_BOOL( r_novis )) || ( RI->view.lockpvs_cached != CVAR_TO_BOOL( r_lockpvs )))
	{
		SetBits( RI->view.changed, RC_VIEWLEAF_CHANGED );
		RI->view.novis_cached = CVAR_TO_BOOL( r_novis );
		RI->view.lockpvs_cached = CVAR_TO_BOOL( r_lockpvs );
	}

	// now setup the frame visibility
	if( FBitSet( RI->view.changed, RC_VIEWLEAF_CHANGED ))
	{
		bool	mergevis = FBitSet( RI->params, RP_MERGE_PVS ) ? true : false;
		bool	fullvis = false;

		if( CVAR_TO_BOOL( r_novis ) || FBitSet( RI->params, RP_DRAW_OVERVIEW ) || ( !RI->view.leaf ))
			fullvis = true;

		ENGINE_SET_PVS( RI->view.pvspoint, REFPVS_RADIUS, RI->view.pvsarray, mergevis, fullvis );
		SetBits( RI->view.changed, RC_PVS_CHANGED );
	}

	// check FOV for changes
	if(( RI->view.fov_x != rvp->fov_x ) || ( RI->view.fov_y != rvp->fov_y ))
	{
		SetBits( RI->view.changed, RC_FOV_CHANGED ); 
		RI->view.fov_x = rvp->fov_x;
		RI->view.fov_y = rvp->fov_y;

		if( RI->view.fov_x <= 0.0f || RI->view.fov_y <= 0.0f )
			HOST_ERROR( "R_SetupViewCache: bad fov!\n" );

		if( IS_NAN( RI->view.fov_x ) || IS_NAN( RI->view.fov_y ))
			HOST_ERROR( "R_SetupViewCache: NAN fov!\n" );

		RI->view.lodScale = tan( DEG2RAD( RI->view.fov_x ) * 0.5f );
	}

	if( FBitSet( RI->view.changed, RC_ORIGIN_CHANGED|RC_ANGLES_CHANGED ))
	{
		// build the transformation matrix for the given view angles and view origin
		RI->view.matrix = matrix4x4( RI->view.origin, RI->view.angles, 1.0f );
		RI->view.planedist = DotProduct( GetVieworg(), GetVForward() );

		// create modelview
		RI->view.worldMatrix.CreateModelview(); // init quake world orientation
		RI->view.worldMatrix.ConcatRotate( -RI->view.angles[2], 1, 0, 0 );
		RI->view.worldMatrix.ConcatRotate( -RI->view.angles[0], 0, 1, 0 );
		RI->view.worldMatrix.ConcatRotate( -RI->view.angles[1], 0, 0, 1 );
		RI->view.worldMatrix.ConcatTranslate( -RI->view.origin[0], -RI->view.origin[1], -RI->view.origin[2] );
		RI->view.worldMatrix.CopyToArray( RI->glstate.modelviewMatrix );
	}

	// first calculation frustum ignores precision farclip, and will be recomputed with actual farclip
	if( FBitSet( RI->view.changed, RC_ORIGIN_CHANGED|RC_ANGLES_CHANGED|RC_FOV_CHANGED ))
	{
		// compute initial farclip
		RI->view.farClip = tr.farclip * 1.74f;

		if( FBitSet( RI->params, RP_DRAW_OVERVIEW ))
		{
			RI->view.frustum.InitOrthogonal( RI->view.matrix, ov->xLeft, ov->xRight, ov->yBottom, ov->yTop, ov->zNear, ov->zFar );
			RI->view.projectionMatrix.CreateOrtho( ov->xLeft, ov->xRight, ov->yTop, ov->yBottom, ov->zNear, ov->zFar );
			RI->view.projectionMatrix.CopyToArray( RI->glstate.projectionMatrix );
		}
		else RI->view.frustum.InitProjection( RI->view.matrix, 0.0f, RI->view.farClip, RI->view.fov_x, RI->view.fov_y );
		SetBits( RI->view.changed, RC_FRUSTUM_CHANGED );
	}

	// time to determine visible leafs and recalc farclip
	if( FBitSet( RI->view.changed, RC_PVS_CHANGED|RC_FRUSTUM_CHANGED|RC_FORCE_UPDATE ))
	{
		CFrustum		*frustum = &RI->view.frustum;
		float		maxdist = 0.0f;
		msurface_t	*surf;
		mextrasurf_t	*esrf;
		int		i, j;

		if( FBitSet( RI->params, RP_SKYVIEW ))
		{
			for( i = 0; i < 6; i++ )
			{
				RI->view.skyMins[0][i] = RI->view.skyMins[1][i] = -1.0f;
				RI->view.skyMaxs[0][i] = RI->view.skyMaxs[1][i] =  1.0f;
			}

			// force to draw skybox (used for create default cubemap)
			SetBits( RI->view.flags, RF_SKYVISIBLE );

			// force to ignore draw the world
			model = NULL;
		}
		else
		{
			for( i = 0; i < 6; i++ )
			{
				RI->view.skyMins[0][i] = RI->view.skyMins[1][i] = 9999999.0f;
				RI->view.skyMaxs[0][i] = RI->view.skyMaxs[1][i] = -9999999.0f;
			}

			ClearBits( RI->view.flags, RF_SKYVISIBLE ); // now sky is invisible
		}

		// we have dynamic lights for this frame
		if( HasDynamicLights( )) SetBits( RI->view.flags, RF_HASDYNLIGHTS );
		else ClearBits( RI->view.flags, RF_HASDYNLIGHTS );

		R_MarkWorldVisibleFaces( model );

		// update the frustum
		if( !FBitSet( RI->params, RP_DRAW_OVERVIEW ))
		{
			RI->view.frustum.InitProjection( RI->view.matrix, 0.0f, RI->view.farClip, RI->view.fov_x, RI->view.fov_y );
			RI->view.projectionMatrix.CreateProjection( RI->view.fov_x, RI->view.fov_y, Z_NEAR, RI->view.farClip );
			RI->view.projectionMatrix.CopyToArray( RI->glstate.projectionMatrix );
			SetBits( RI->view.changed, RC_FRUSTUM_CHANGED );
		}

		// recompute worldview projection
		RI->view.worldProjectionMatrix = RI->view.projectionMatrix.Concat( RI->view.worldMatrix );
		RI->view.worldProjectionMatrix.CopyToArray( RI->glstate.modelviewProjectionMatrix );
		R_ClearFrameLists();

		// update the split frustum
		if( model != NULL && RP_NORMALPASS() && !FBitSet( RI->params, RP_DRAW_OVERVIEW ) && FBitSet( RI->view.changed, RC_FRUSTUM_CHANGED ))
		{
			float	farClip = RI->view.farClip;
			float	lambda = bound( 0.1f, r_shadow_split_weight->value, 1.0f );
			float	ratio = farClip / Z_NEAR;
			Vector	planeOrigin;
			float	zNear, zFar;
			int	i;

			// initialize all the frustums
			for( i = 0; i < MAX_SHADOWMAPS; i++ )
				RI->view.splitFrustum[i] = RI->view.frustum;

			for( i = 1; i < MAX_SHADOWMAPS; i++ )
			{
				float si = i / (float)(MAX_SHADOWMAPS);

				zNear = lambda * (Z_NEAR * powf( ratio, si )) + (1.0f - lambda) * (Z_NEAR + (farClip - Z_NEAR) * si);
				zFar = zNear * 1.005f;

				planeOrigin = GetVieworg() + GetVForward() * zNear;
				RI->view.splitFrustum[i].SetPlane( FRUSTUM_NEAR, GetVForward(), DotProduct( planeOrigin, GetVForward()));

				planeOrigin = GetVieworg() + GetVForward() * zFar;
				RI->view.splitFrustum[i-1].SetPlane( FRUSTUM_FAR, -GetVForward(), DotProduct( planeOrigin, -GetVForward()));

				RI->view.parallelSplitDistances[i - 1] = zFar;
			}

			planeOrigin = GetVieworg() + GetVForward() * farClip;
			RI->view.splitFrustum[NUM_SHADOW_SPLITS].SetPlane( FRUSTUM_FAR, -GetVForward(), DotProduct( planeOrigin, -GetVForward()));
			RI->view.parallelSplitDistances[NUM_SHADOW_SPLITS] = farClip;
		}

		// don't draw the entities while render skybox
		if (!FBitSet(RI->params, (RP_SKYVIEW)))
		{
			// before process of tr.draw_entities
			// we can add muzzleflashes here
			R_RunViewmodelEvents();

			// brush faces not added here!
			// only marks as visible in RI->view.visfaces array
			for( i = 0; i < tr.num_draw_entities; i++ )
			{
				RI->currententity = tr.draw_entities[i];
				RI->currentmodel = RI->currententity->model;

				switch( RI->currentmodel->type )
				{
				case mod_brush:
					R_MarkSubmodelVisibleFaces();
					break;
				case mod_studio:
					R_AddStudioToDrawList( RI->currententity );
					break;
				case mod_sprite:
					R_AddSpriteToDrawList( RI->currententity );
					break;
				default: 
//					HOST_ERROR( "R_SetupViewCache: mod_bad\n" );
					break;
				}
			}

			// add particles to deferred list
			g_pParticleSystems.UpdateSystems();
			g_pParticles.Update();
		}

		// create drawlist for faces, do additional culling for world faces
		for( i = 0; model != NULL && i < world->numsortedfaces; i++ )
		{
			ASSERT( world->sortedfaces != NULL );

			j = world->sortedfaces[i];

			ASSERT( j >= 0 && j < model->numsurfaces );

			if( CHECKVISBIT( RI->view.visfaces, j ))
			{
				surf = model->surfaces + j;
				esrf = surf->info;

				// submodel faces already passed through this
				// operation but world is not
				if( FBitSet( surf->flags, SURF_OF_SUBMODEL ))
				{
					RI->currententity = esrf->parent;
					RI->currentmodel = RI->currententity->model;

					R_AddGrassToDrawList( surf, DRAWLIST_SOLID );
				}
				else
				{
					RI->currententity = GET_ENTITY( 0 );
					RI->currentmodel = RI->currententity->model;

					esrf->parent = RI->currententity; // setup dynamic upcast

					R_AddGrassToDrawList( surf, DRAWLIST_SOLID );

					if( R_CullSurface( surf, GetVieworg(), frustum ))
					{
						CLEARVISBIT( RI->view.visfaces, j ); // not visible
						continue;
					}

					// surface has passed all visibility checks
					// and can be update some data (lightmaps, mirror matrix, etc)
					R_UpdateSurfaceParams( surf );
                                        }

				// store world translucent watery (if transparent water is support)
				if( FBitSet( surf->flags, SURF_DRAWTURB ) && !FBitSet( surf->flags, SURF_OF_SUBMODEL ))
				{
					if( FBitSet( world->features, WORLD_WATERALPHA ))
						R_AddSurfaceToDrawList( surf, DRAWLIST_TRANS );
					else R_AddSurfaceToDrawList( surf, DRAWLIST_SOLID );
				}
				else if( FBitSet( surf->flags, SURF_DRAWSKY ))
				{
					SetBits( RI->view.flags, RF_SKYVISIBLE );
					R_AddSkyBoxSurface( surf );
				}
				else if( R_OpaqueEntity( RI->currententity ))
				{
					R_AddSurfaceToDrawList( surf, DRAWLIST_SOLID );
				}
				else
				{
					R_AddSurfaceToDrawList( surf, DRAWLIST_TRANS );
				}

				// and store faces that required additional pass from another point into separate list
				if (FBitSet( surf->flags, SURF_REFLECT | SURF_REFLECT_PUDDLE | SURF_PORTAL | SURF_SCREEN))
				{
					if( !FBitSet( RI->currentmodel->flags, BIT( 2 )) || tr.waterlevel < 3 )
						R_AddSurfaceToDrawList( surf, DRAWLIST_SUBVIEW );
				}
			}
		}
	}
	else if( !FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
	{
		// before process of tr.draw_entities
		// we can add muzzleflashes here
		R_RunViewmodelEvents();

		// update entity params for every frame
		for( int i = 0; i < tr.num_draw_entities; i++ )
		{
			RI->currententity = tr.draw_entities[i];
			RI->currentmodel = RI->currententity->model;

			switch( RI->currentmodel->type )
			{
			case mod_brush:
				R_UpdateSubmodelParams();
				break;
			case mod_studio:
				R_AddStudioToDrawList( RI->currententity, true );
				break;
			case mod_sprite:
				R_AddSpriteToDrawList( RI->currententity, true );
				break;
			default: HOST_ERROR( "R_SetupViewCache: mod_bad\n" );
			}
		}
	}

	// setup dynamic lights
	if( !FBitSet( RI->params, RP_DEFERREDLIGHT ))
	{
		Mod_InitBSPModelsTexture();
		R_SetupDynamicLights();
	}

	// cache client frame
	RI->view.client_frame = tr.realframecount;		
}

/*
===============
R_SetupGLstate

shared our matrices with gl-machine
===============
*/
void R_SetupGLstate( void )
{
	RI->glstate.viewport.SetAsCurrent();

	if( RP_NORMALPASS() && tr.sunlight != NULL && CVAR_TO_BOOL( r_pssm_show_split ))
	{
		GLfloat debugSplitProjection[16], debugSplitModelview[16];
		int split = bound( 0, (int)r_pssm_show_split->value - 1, NUM_SHADOW_SPLITS );
		tr.sunlight->textureMatrix[split].CopyToArray( debugSplitProjection );
		tr.sunlight->modelviewMatrix.CopyToArray( debugSplitModelview );

		pglMatrixMode( GL_PROJECTION );
		pglLoadMatrixf( debugSplitProjection );

		pglMatrixMode( GL_MODELVIEW );
		pglLoadMatrixf( debugSplitModelview );
	}
	else
	{
		pglMatrixMode( GL_PROJECTION );
		pglLoadMatrixf( RI->glstate.projectionMatrix );

		pglMatrixMode( GL_MODELVIEW );
		pglLoadMatrixf( RI->glstate.modelviewMatrix );
	}

	if( FBitSet( RI->params, RP_CLIPPLANE ))
		GL_ClipPlane( true );
}

/*
===============
R_ResetGLstate

disable frame specifics
===============
*/
void R_ResetGLstate( void )
{
	if( FBitSet( RI->params, RP_CLIPPLANE ))
		GL_ClipPlane( false );

	GL_CleanupDrawState();
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_TRUE );
	GL_Blend( GL_FALSE );
}

/*
=============
R_Clear
=============
*/
void R_Clear( int bitMask )
{
	GL_DEBUG_SCOPE();

	int	bits = GL_DEPTH_BUFFER_BIT;
	// NOTE: mask should be enabled to properly clear buffer
	GL_DepthMask( GL_TRUE );

	if( FBitSet( RI->params, RP_DRAW_OVERVIEW ))
		pglClearColor( 0.0f, 1.0f, 0.0f, 1.0f ); // green background (Valve rules)
	else pglClearColor( 0.5f, 0.5f, 0.5f, 1.0f );

	// force to clearing screen to avoid ugly blur
	if (tr.fClearScreen || CVAR_TO_BOOL(r_clear)) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	bits &= bitMask;

	pglClear( bits );
	pglDepthFunc( GL_LEQUAL );
	GL_Cull( GL_FRONT );

	// change ordering for overview
	if( FBitSet( RI->params, RP_DRAW_OVERVIEW ))
	{
		gldepthmin = 1.0f;
		gldepthmax = 0.0f;
	}
	else
	{ 
		gldepthmin = 0.0f;
		gldepthmax = 1.0f;
	}

	GL_DepthRange( gldepthmin, gldepthmax );
}

/*
=============
R_SetupProjectionMatrix
=============
*/
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m )
{
	GLdouble	xMax, yMax, zFar;

	zFar = Q_max( 256.0, RI->view.farClip );
	xMax = Z_NEAR * tan( fov_x * M_PI / 360.0 );
	yMax = Z_NEAR * tan( fov_y * M_PI / 360.0 );

	m.CreateProjection( xMax, -xMax, yMax, -yMax, Z_NEAR, zFar );
}

/*
=============
R_DrawParticles

NOTE: particles are drawing with engine methods
=============
*/
void R_DrawParticles( qboolean trans )
{
	ref_viewpass_t	rvp;

	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	GL_DEBUG_SCOPE();
	RI->view.port.WriteToArray(rvp.viewport);
	rvp.viewangles = RI->view.angles;
	rvp.vieworigin = RI->view.origin;
	rvp.fov_x = RI->view.fov_x;
	rvp.fov_y = RI->view.fov_y;
	rvp.flags = 0;

	if( FBitSet( RI->params, RP_DRAW_WORLD ))
		SetBits( rvp.flags, RF_DRAW_WORLD );
	if( FBitSet( RI->params, RP_ENVVIEW ))
		SetBits( rvp.flags, RF_DRAW_CUBEMAP );
	if( FBitSet( RI->params, RP_DRAW_OVERVIEW ))
		SetBits( rvp.flags, RF_DRAW_OVERVIEW );

	DRAW_PARTICLES( &rvp, trans, tr.frametime );
}

/*
=================
R_SortTransMeshes

compare translucent meshes
=================
*/
static int R_SortTransMeshes( const CTransEntry *a, const CTransEntry *b )
{
	if( a->m_flViewDist > b->m_flViewDist )
		return -1;
	if( a->m_flViewDist < b->m_flViewDist )
		return 1;

	return 0;
}

/*
===============
R_RenderTransList
===============
*/
void R_RenderTransList( void )
{
	if( !RI->frame.trans_list.Count() )
		return;

	GL_DEBUG_SCOPE();
	GL_Blend( GL_FALSE ); // mixing screencopy with diffuse so we don't need blend
	GL_AlphaTest( GL_FALSE );
	// yes, we rendering translucent objects with enabdled depthwrite
	GL_DepthMask( GL_TRUE );

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	// sorting by distance
	RI->frame.trans_list.Sort( R_SortTransMeshes );

	for( int i = 0; i < RI->frame.trans_list.Count(); i++ )
	{
		CTransEntry *entry = &RI->frame.trans_list[i];

		switch( entry->m_bDrawType )
		{
		case DRAWTYPE_SURFACE:
			R_RenderTransSurface( entry );
			break;
		case DRAWTYPE_MESH:
			R_RenderTransMesh( entry );
			break;
		case DRAWTYPE_QUAD:
			R_RenderQuadPrimitive( entry );
			break;
		}
	}

	R_RenderDecalsTransList( DRAWLIST_SOLID );

	R_RenderDynLightList( false );
	R_RenderLightForTransMeshes();

	R_RenderDecalsTransList( DRAWLIST_TRANS );

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglDisable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
	GL_DepthRange( gldepthmin, gldepthmax );
	GL_CleanupDrawState();
	GL_ClipPlane( true );
	GL_Cull( GL_FRONT );

	DBG_DrawGlassScissors();
}

/*
===============
R_RenderScene
===============
*/
void R_RenderScene( const ref_viewpass_t *rvp, RefParams params )
{
	// now we know about pass specific
	RI->params = params;

	// frametime is valid only for normal pass
	if( RP_NORMALPASS( ))
		tr.frametime = tr.saved_frametime;
	else tr.frametime = 0.0;

	GL_DEBUG_SCOPE();
	R_BuildViewPassHierarchy();
	R_SetupViewCache( rvp );

	R_CheckSkyPortal(tr.sky_camera);
	R_RenderSubview(); // prepare subview frames
	R_RenderShadowmaps(); // draw all the shadowmaps

	R_SetupGLstate();
	R_Clear( ~0 );

	R_DrawSkyBox();
	R_RenderSolidBrushList();
	R_RenderSolidStudioList();
	HUD_DrawNormalTriangles();
	R_RenderSurfOcclusionList();
	R_DrawParticles( false );
	R_RenderDebugStudioList( false );

	GL_CheckForErrors();

	// restore right depthrange here
	GL_DepthRange( gldepthmin, gldepthmax );

	R_RenderTransList();
	R_DrawParticles( true );
	R_DrawWeather();
	HUD_DrawTransparentTriangles();

	GL_CheckForErrors();

	GL_BindShader( NULL );
	R_ResetGLstate();
}

/*
===============
R_RenderDeferredScene
===============
*/
void R_RenderDeferredScene( const ref_viewpass_t *rvp, RefParams params )
{
	// now we know about pass specific
	RI->params = params;
	GL_DEBUG_SCOPE();
	R_SetupViewCache( rvp );

	// draw all the shadowmaps
	if( FBitSet( RI->params, RP_DEFERREDSCENE ))
		R_RenderShadowmaps();

	GL_SetupGBuffer();
	R_SetupGLstate();
	R_Clear( ~0 );

	R_DrawSkyBox();
	R_RenderDeferredBrushList();
	R_RenderDeferredStudioList();
	R_PushRefState();
	RI->params = params;
	R_DrawViewModel();
	R_PopRefState();
	GL_CleanupDrawState();
	GL_ResetGBuffer();

	GL_CheckForErrors();

	GL_DrawDeferredPass();
	R_ResetGLstate();
}

/*
===============
HUD_RenderFrame

A callback that replaces RenderFrame
engine function. Return 1 if you
override ALL the rendering and return 0
if we don't want draw from
the client (e.g. playersetup preview)
===============
*/
int HUD_RenderFrame( const struct ref_viewpass_s *rvp )
{
	GL_DEBUG_SCOPE();

	RefParams refParams = RP_NONE;
	ref_viewpass_t defVP = *rvp;
	bool hdr_rendering = CVAR_TO_BOOL(gl_hdr);
	bool multisampling = CVAR_GET_FLOAT("gl_msaa") > 0.0f;

	// setup some renderer flags
	if( !FBitSet( rvp->flags, RF_DRAW_CUBEMAP ))
	{
		if ( FBitSet( rvp->flags, RF_DRAW_OVERVIEW ))
			SetBits( refParams, RP_DRAW_OVERVIEW );

		if (CL_IsThirdPerson())
			SetBits( refParams, RP_THIRDPERSON );
	}
	else
	{
		if( world->build_default_cubemap )
			SetBits( refParams, RP_SKYVIEW );
		else SetBits( refParams, RP_ENVVIEW );
		tr.fClearScreen = true;

		// now all uber-shaders are invalidate
		// and possible need for recompile
		tr.params_changed = true;
		tr.glsl_valid_sequence++;
	}

	if( FBitSet( rvp->flags, RF_DRAW_WORLD ))
		SetBits( refParams, RP_DRAW_WORLD );

	if (!GL_BackendStartFrame(&defVP, refParams))
	{
		return 0;
	}

	if( CVAR_TO_BOOL( cv_deferred ))
	{
		if( !CVAR_TO_BOOL( cv_deferred_full ))
		{
			defVP.viewport[2] = glState.defWidth;
			defVP.viewport[3] = glState.defHeight;
			R_RenderDeferredScene( &defVP, RP_DEFERREDLIGHT );
			defVP = *rvp;
		}
		R_RenderDeferredScene( &defVP, RP_DEFERREDSCENE );
	}
	else
	{
		if (hdr_rendering) 
		{
			if (multisampling) {
				GL_BindDrawbuffer(tr.screen_multisample_fbo);
			}
			else {
				GL_BindDrawbuffer(tr.screen_hdr_fbo);
			}
		}
		R_RenderScene( &defVP, refParams );
	}

	defVP = *rvp;

	GL_BackendEndFrame( &defVP, refParams );
	return 1;
}

void HUD_ProcessModelData( model_t *mod, qboolean create, const byte *buffer )
{
	if( !g_fRenderInitialized )
	{
		// we needs CRC anyway
		if( mod->type == mod_studio && create )
		{
			// compute model CRC to verify vertexlighting data
			// NOTE: source buffer is not equal to Mod_Extradata!
			studiohdr_t *src = (studiohdr_t *)buffer;
			mod->modelCRC = FILE_CRC32( buffer, src->length );
		}
		return;
	}
	// g-cont. probably this is redundant :-)
	if( RENDER_GET_PARM( PARM_DEDICATED_SERVER, 0 ))
		return;

	if( FBitSet( mod->flags, MODEL_WORLD ))
		R_ProcessWorldData( mod, create, buffer );
	else R_ProcessStudioData( mod, create, buffer );
}

BOOL HUD_SpeedsMessage( char *out, size_t size )
{
	if( !g_fRenderInitialized || !CVAR_TO_BOOL( cv_renderer ))
		return false; // let the engine use built-in counters

	if( r_speeds->value <= 0 ) return false;
	if( !out || !size ) return false;

	Q_strncpy( out, r_speeds_msg, size );

	return true;
}

void HUD_ProcessEntData( qboolean allocate )
{
	if (allocate) 
	{
		Mod_PrepareModelInstances();
		if (strlen(world->name) < 1) 
		{
			// flush world name, cached in GL_InitModelLightCache
			world->ignore_restart_check = true;
		}
	}
	else 
		Mod_ThrowModelInstances();
}

void HUD_BuildLightmaps( void )
{
	if (!g_fRenderInitialized ) 
		return;

	// put the gamma into GLSL-friendly array
	// TODO: get rid of this
	for (int i = 0; i < 256; i++) {
		tr.gamma_table[i / 4][i % 4] = (float)TEXTURE_TO_TEXGAMMA(i) / 255.0f;
	}

	for (int i = 0; i < worldmodel->numsurfaces; i++) {
		SetBits(worldmodel->surfaces[i].flags, SURF_LM_UPDATE | SURF_GRASS_UPDATE);
	}

	R_StudioClearLightCache();
}

//
// Xash3D render interface
//
static render_interface_t gRenderInterface = 
{
	CL_RENDER_INTERFACE_VERSION,
	HUD_RenderFrame,
	HUD_BuildLightmaps,
	Mod_SetOrthoBounds,
	R_CreateStudioDecalList,
	R_ClearStudioDecals,
	HUD_SpeedsMessage,
	HUD_ProcessModelData,
	HUD_ProcessEntData,
	Mod_GetCurrentVis,
	GL_MapChanged,
	R_ClearScene,
	R_UpdateLatchedVars,
};

extern "C" int DLLEXPORT HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback )
{
	if ( !callback || !renderfuncs || version != CL_RENDER_INTERFACE_VERSION )
	{
		return FALSE;
	}

	size_t iImportSize = sizeof( render_interface_t );

	// copy new physics interface
	memcpy( &gRenderfuncs, renderfuncs, sizeof( render_api_t ));

	// fill engine callbacks
	memcpy( callback, &gRenderInterface, iImportSize );

	// get pointer to movevars
	tr.movevars = gEngfuncs.pEventAPI->EV_GetMovevars();

	// check that engine started with ref_gl renderer
	const char *refName = gEngfuncs.pfnGetCvarString("r_refdll");
	if (refName && strcmp(refName, "soft") == 0)
	{
		HOST_ERROR("PrimeXT cannot be started with software renderer. "
			"Please, select \"OpenGL\" renderer in game settings, "
			"otherwise you can use \"ref -gl\" startup parameter, "
			"or remove your \"video.cfg\" configuration file."
		);
		return FALSE;
	}
	else if (refName && strcmp(refName, "gl") != 0)
	{
		ALERT(at_warning, "PrimeXT may not work or work unstable on all other renderers except OpenGL.");
	}

	g_fRenderInitialized = TRUE;
	return TRUE;
}