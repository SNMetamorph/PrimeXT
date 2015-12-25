/*
r_surf.cpp - dynamic and static lights
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
#include "pm_movevars.h"
#include "pmtrace.h"
#include "mathlib.h"
#include "pm_defs.h"
#include "event_api.h"

/*
=============================================================================

PROJECTED LIGHTS

=============================================================================
*/
plight_t	cl_plights[MAX_PLIGHTS];

/*
================
CL_ClearPlights
================
*/
void CL_ClearPlights( void )
{
	memset( cl_plights, 0, sizeof( cl_plights ));
}

/*
===============
CL_AllocPlight

alloc first 64 slots only
===============
*/
plight_t *CL_AllocPlight( int key )
{
	plight_t *pl;
	int i;

	// first look for an exact key match
	if( key )
	{
		for( i = 0, pl = cl_plights; i < MAX_USER_PLIGHTS; i++, pl++ )
		{
			if( pl->key == key )
			{
				r_stats.c_plights++;
				// reuse this light
				return pl;
			}
		}
	}

	// then look for anything else
	for( i = 0, pl = cl_plights; i < MAX_USER_PLIGHTS; i++, pl++ )
	{
		if( pl->die < GET_CLIENT_TIME() && pl->key == 0 )
		{
			memset( pl, 0, sizeof( *pl ));
			r_stats.c_plights++;
			pl->key = key;

			return pl;
		}
	}

	// otherwise grab first dlight
	pl = &cl_plights[0];
	memset( pl, 0, sizeof( *pl ));
	r_stats.c_plights++;
	pl->key = key;

	return pl;
}

/*
================
R_SetupLightProjection

General setup light projections.
Calling only once per frame
================
*/
void R_GetLightVectors( cl_entity_t *pEnt, Vector &origin, Vector &angles )
{
	// fill default case
	origin = pEnt->origin;
	angles = pEnt->angles; 

	// try to grab position from attachment
	if( pEnt->curstate.aiment > 0 && pEnt->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t *pParent = GET_ENTITY( pEnt->curstate.aiment );
		studiohdr_t *pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata( pParent->model );

		if( pParent && pParent->model && pStudioHeader != NULL )
		{
			// make sure what model really has attachements
			if( pEnt->curstate.body > 0 && ( pStudioHeader && pStudioHeader->numattachments > 0 ))
			{
				int num = bound( 1, pEnt->curstate.body, MAXSTUDIOATTACHMENTS );
				origin = R_StudioAttachmentPos( pParent, num - 1 );
				VectorAngles( R_StudioAttachmentDir( pParent, num - 1 ), angles );
				angles[PITCH] = -angles[PITCH]; // stupid quake bug
			}
			else if( pParent->curstate.movetype == MOVETYPE_STEP )
			{
				float f;
                    
				// don't do it if the goalstarttime hasn't updated in a while.
				// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
				//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
				if(( GET_CLIENT_TIME() < pParent->curstate.animtime + 1.0f ) && ( pParent->curstate.animtime != pParent->latched.prevanimtime ))
				{
					f = (GET_CLIENT_TIME() - pParent->curstate.animtime) / (pParent->curstate.animtime - pParent->latched.prevanimtime);
				}

				if( !( pParent->curstate.effects & EF_NOINTERP ))
				{
					// ugly hack to interpolate angle, position.
					// current is reached 0.1 seconds after being set
					f = f - 1.0f;
				}
				else
				{
					f = 0.0f;
				}

				// start lerping from
				origin = pParent->origin;
				angles = pParent->angles;

				InterpolateOrigin( pParent->latched.prevorigin, pParent->origin, origin, f, true );
				InterpolateAngles( pParent->latched.prevangles, pParent->angles, angles, f, true );

				// add the eye position for monster
				if( pParent->curstate.eflags & EFLAG_SLERP )
					origin += pStudioHeader->eyeposition;
			} 
			else
			{
				origin = pParent->origin;
				angles = pParent->angles;
			}
		}
	}
	// all other parent types will be attached on the server
}

/*
================
R_SetupLightProjection

General setup light projections.
Calling only once per frame
================
*/
void R_SetupLightProjection( plight_t *pl, const Vector &origin, const Vector &angles, float radius, float fov )
{
	// update the frustum only if needs
	if( pl->origin != origin || pl->angles != angles || pl->fov != fov || pl->radius != radius )
	{
		GLdouble	xMin, xMax, yMin, yMax, zNear, zFar;
		GLfloat	fov_x, fov_y;

		zNear = 0.1f;
		zFar = radius * 1.5f; // distance

		pl->origin = origin;
		pl->angles = angles;
		pl->radius = radius;
		pl->fov = fov;

		if( pl->pointlight )
		{
			// 'quake oriented' cubemaps probably starts from Tenebrae
			// may be it was an optimization?
			pl->modelviewMatrix.Identity();
		}
		else
		{
			if( pl->flags & (CF_ASPECT4X3|CF_ASPECT3X4))
			{
				fov_x = fov_y = fov;

				// BUGBUG: we use 5:4 aspect not an 4:3 
				if( pl->flags & CF_ASPECT3X4 )
					fov_y = fov_x * (5.0f / 4.0f); 
				else if( pl->flags & CF_ASPECT4X3 )
					fov_y = fov_x * (4.0f / 5.0f);

				yMax = zNear * tan( fov_y * M_PI / 360.0 );
				yMin = -yMax;

				xMax = zNear * tan( fov_x * M_PI / 360.0 );
				xMin = -xMax;
			}
			else
			{
				xMax = yMax = zNear * tan( fov * M_PI / 360.0 );
				xMin = yMin = -yMax;
				fov_x = fov_y = fov;
			}

			// e.g. for fake cinema projectors
			if( FBitSet( pl->flags, CF_FLIPTEXTURE ))
			{
				xMax = -xMax;
				xMin = -xMin;
			}

			fov = zNear * tan( fov * M_PI / 360.0 );
			pl->projectionMatrix.CreateProjection( xMax, xMin, yMax, yMin, zNear, zFar );
			pl->modelviewMatrix.CreateModelview(); // init quake world orientation
		}

		pl->modelviewMatrix.ConcatRotate( -pl->angles.z, 1, 0, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.x, 0, 1, 0 );
		pl->modelviewMatrix.ConcatRotate( -pl->angles.y, 0, 0, 1 );
		pl->modelviewMatrix.ConcatTranslate( -pl->origin.x, -pl->origin.y, -pl->origin.z );

		matrix4x4 projectionView, m1, s1;

		m1.CreateTranslate( 0.5f, 0.5f, 0.5f );
		s1.CreateTranslate( 0.5f, 0.5f, 0.5f );

		if( pl->pointlight )
		{
			projectionView = pl->modelviewMatrix; // cubemaps not used projection matrix
			m1.ConcatScale( 0.5f, 0.5f, 0.5f );
		}
		else
		{
			projectionView = pl->projectionMatrix.Concat( pl->modelviewMatrix );
			m1.ConcatScale( 0.5f, -0.5f, 0.5f );
		}

		s1.ConcatScale( 0.5f, 0.5f, 0.5f );

		pl->textureMatrix = m1.Concat( projectionView ); // only for world, sprites and studiomodels
		pl->shadowMatrix = s1.Concat( projectionView );

		if( pl->pointlight )
		{
			for( int i = 0; i < 6; i++ )
			{
				pl->frustum[i].type = i>>1;
				pl->frustum[i].normal[i>>1] = (i & 1) ? 1.0f : -1.0f;
				pl->frustum[i].signbits = SignbitsForPlane( pl->frustum[i].normal );
				pl->frustum[i].dist = DotProduct( pl->origin, pl->frustum[i].normal ) - pl->radius * 2;
			}

			// setup base clipping
			pl->clipflags = 63;
		}
		else
		{
			Vector vforward, vright, vup;
			AngleVectors( pl->angles, vforward, vright, vup );

			Vector farPoint = pl->origin + vforward * zFar;

			// rotate vforward right by FOV_X/2 degrees
			RotatePointAroundVector( pl->frustum[0].normal, vup, vforward, -(90 - fov_x / 2));
			// rotate vforward left by FOV_X/2 degrees
			RotatePointAroundVector( pl->frustum[1].normal, vup, vforward, 90 - fov_x / 2 );
			// rotate vforward up by FOV_Y/2 degrees
			RotatePointAroundVector( pl->frustum[2].normal, vright, vforward, 90 - fov_y / 2 );
			// rotate vforward down by FOV_Y/2 degrees
			RotatePointAroundVector( pl->frustum[3].normal, vright, vforward, -(90 - fov_y / 2));

			for( int i = 0; i < 4; i++ )
			{
				pl->frustum[i].type = PLANE_NONAXIAL;
				pl->frustum[i].dist = DotProduct( pl->origin, pl->frustum[i].normal );
				pl->frustum[i].signbits = SignbitsForPlane( pl->frustum[i].normal );
			}

			pl->frustum[4].normal = -vforward;
			pl->frustum[4].type = PLANE_NONAXIAL;
			pl->frustum[4].dist = DotProduct( farPoint, pl->frustum[4].normal );
			pl->frustum[4].signbits = SignbitsForPlane( pl->frustum[4].normal );

			// store vector forward to avoid compute it again
			pl->frustum[5].normal = vforward;

			// setup base clipping
			pl->clipflags = 15;
		}
	}
}

/*
================
R_MergeLightProjection

merge projection for bmodels
================
*/
void R_MergeLightProjection( plight_t *pl )
{
	matrix4x4 modelviewMatrix = pl->modelviewMatrix.ConcatTransforms( RI.objectMatrix );
	matrix4x4 projectionView, m1, s1;

	m1.CreateTranslate( 0.5f, 0.5f, 0.5f );
	s1.CreateTranslate( 0.5f, 0.5f, 0.5f );
	s1.ConcatScale( 0.5f, 0.5f, 0.5f );

	if( pl->pointlight )
	{
		projectionView = modelviewMatrix; // cubemaps not used projection matrix
		m1.ConcatScale( 0.5f, 0.5f, 0.5f );
	}
	else
	{
		projectionView = pl->projectionMatrix.Concat( modelviewMatrix );
		m1.ConcatScale( 0.5f, -0.5f, 0.5f );
	}

	pl->textureMatrix2 = m1.Concat( projectionView );
	pl->shadowMatrix2 = s1.Concat( projectionView );
}

/*
================
R_SetupLightProjectionTexture

select the custom projection texture or movie
================
*/
void R_SetupLightProjectionTexture( plight_t *pl, cl_entity_t *pEnt )
{
	// select the cinematic texture
	if( pl->flags & CF_TEXTURE )
	{
		if( !pl->projectionTexture )
		{
			const char *txname = gEngfuncs.pEventAPI->EV_EventForIndex( pEnt->curstate.sequence );

			if( txname && *txname )
			{
				const char *ext = UTIL_FileExtension( txname );

				if( !Q_stricmp( ext, "tga" ) || !Q_stricmp( ext, "mip" ))
				{
					pl->projectionTexture = LOAD_TEXTURE( txname, NULL, 0, TF_SPOTLIGHT );
				}
				else
				{
					ALERT( at_error, "couldn't find texture %s\n", txname );
					pl->projectionTexture = tr.spotlightTexture;
				}
			}
			else
			{
				ALERT( at_error, "couldn't find texture %s\n", txname );
				pl->projectionTexture = tr.spotlightTexture;
			}
		}
	}
	else if( pl->flags & CF_SPRITE )
	{
		if( !pl->pSprite && !pl->projectionTexture )
		{
			// setup the sprite
			const char *sprname = gEngfuncs.pEventAPI->EV_EventForIndex( pEnt->curstate.sequence );
			HSPRITE handle = SPR_LoadEx( sprname, TF_BORDER );
			pl->pSprite = (model_t *)gEngfuncs.GetSpritePointer( handle );

			if( !pl->pSprite )
			{
				ALERT( at_error, "couldn't find sprite %s\n", sprname );
				pl->projectionTexture = tr.spotlightTexture;
			}
		}

		if( pl->pSprite )
		{
			// animate frames!
			int texture = R_GetSpriteTexture( pl->pSprite, pEnt->curstate.frame );
			if( texture != 0 ) pl->projectionTexture = texture;
		}
	}
	else if( pl->flags & CF_MOVIE )
	{
		if( pl->projectionTexture == tr.spotlightTexture )
			return;	// bad video texture

		// found the corresponding cinstate
		const char *cinname = gEngfuncs.pEventAPI->EV_EventForIndex( pEnt->curstate.sequence );
		int hCin = R_PrecacheCinematic( cinname );

		if( hCin >= 0 && !pl->cinTexturenum )
			pl->cinTexturenum = R_AllocateCinematicTexture( TF_SPOTLIGHT );

		if( hCin == -1 || pl->cinTexturenum <= 0 || !CIN_IS_ACTIVE( tr.cinematics[hCin].state ))
		{
			// cinematic textures limit exceeded or movie not found
			pl->projectionTexture = tr.spotlightTexture;
			return;
		}

		gl_movie_t *cin = &tr.cinematics[hCin];
		float cin_time;

		// advances cinematic time
		cin_time = fmod( pEnt->curstate.fuser2, cin->length );

		// read the next frame
		int cin_frame = CIN_GET_FRAME_NUMBER( cin->state, cin_time );

		if( cin_frame != pl->lastframe )
		{
			// upload the new frame
			byte *raw = CIN_GET_FRAMEDATA( cin->state, cin_frame );
			CIN_UPLOAD_FRAME( tr.cinTextures[pl->cinTexturenum-1],
				cin->xres, cin->yres, cin->xres, cin->yres, raw );
			pl->lastframe = cin_frame;
		}

		// have valid cinematic texture
		pl->projectionTexture = tr.cinTextures[pl->cinTexturenum-1];
	}
	else
	{
		// just use default texture
		pl->projectionTexture = tr.spotlightTexture;
	}

	// check for valid
	if( pl->projectionTexture <= 0 )
		pl->projectionTexture = tr.spotlightTexture;

	// set cubemap flag for easy check
	if( RENDER_GET_PARM( PARM_TEX_FLAGS, pl->projectionTexture ) & TF_CUBEMAP )
	{
		pl->flags |= CF_CUBEMAP;
		pl->pointlight = true; // in case of
	}
}

/*
================
R_SetupLightAttenuationTexture

select the properly attenuation
================
*/
void R_SetupLightAttenuationTexture( plight_t *pl, int falloff )
{
	if( pl->flags & CF_NOATTEN )
	{
		if( pl->pointlight )
			pl->clipflags = 0;
		else pl->clipflags &= ~(1<<4); // clear farplane clipping

		// NOTE: we can't use shadows for projectors without stub texture:
		// because PCF shader expected it from us
		if( pl->pointlight ) pl->attenuationTexture = 0;
		else pl->attenuationTexture = tr.attenuationStubTexture;
		return;
	}

	if( pl->pointlight )
	{
		pl->attenuationTexture = tr.attenuationTexture3D;
		return;
	}

	if( falloff <= 0 )
	{
		if( pl->radius <= 300 )
			pl->attenuationTexture = tr.attenuationTexture3;	// bright
		else if( pl->radius > 300 && pl->radius <= 800 )
			pl->attenuationTexture = tr.attenuationTexture;	// normal
		else if( pl->radius > 800 )
			pl->attenuationTexture = tr.attenuationTexture2;	// dark
	}
	else
	{
		switch( bound( 1, falloff, 3 ))
		{
		case 1:
			pl->attenuationTexture = tr.attenuationTexture2;	// dark
			break;
		case 2:
			pl->attenuationTexture = tr.attenuationTexture;	// normal
			break;
		case 3:
			pl->attenuationTexture = tr.attenuationTexture3;	// bright
			break;
		}
	}
	pl->clipflags |= (1<<4);
}

/*
================
R_RecursiveLightNode
================
*/
void R_RecursiveLightNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags )
{
	const mplane_t	*clipplane;
	int		i, clipped;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	int		c, side;
	float		dot;

	if( node->contents == CONTENTS_SOLID )
		return; // hit a solid leaf

	if( node->visframe != tr.visframecount )
		return;

	if( clipflags )
	{
		for( i = 0; i < 6; i++ )
		{
			clipplane = &frustum[i];

			if(!( clipflags & ( 1<<i )))
				continue;

			clipped = BoxOnPlaneSide( node->minmaxs, node->minmaxs + 3, clipplane );
			if( clipped == 2 ) return;
			if( clipped == 1 ) clipflags &= ~(1<<i);
		}
	}

	// if a leaf node, draw stuff
	if( node->contents < 0 )
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if( c )
		{
			do
			{
				(*mark)->visframe = tr.framecount;
				mark++;
			} while( --c );
		}
		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	dot = PlaneDiff( tr.modelorg, node->plane );
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveLightNode( node->children[side], frustum, clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		// in some cases surface is invisible but grass is visible
		R_AddToGrassChain( surf, frustum, clipflags, true );

		if( R_CullSurfaceExt( surf, frustum, clipflags ))
			continue;

		if( !( surf->flags & (SURF_DRAWTILED|SURF_PORTAL|SURF_REFLECT)))
		{
			// keep light surfaces seperate from world chains
			SURF_INFO( surf, RI.currentmodel )->lightchain = surf->texinfo->texture->lightchain;
			surf->texinfo->texture->lightchain = SURF_INFO( surf, RI.currentmodel );
		}
	}

	// recurse down the back side
	R_RecursiveLightNode( node->children[!side], frustum, clipflags );
}

/*
=================
R_LightStaticModel

Merge static model brushes with world lighted surfaces
=================
*/
void R_LightStaticModel( const plight_t *pl, cl_entity_t *e )
{
	model_t		*clmodel;
	msurface_t	*psurf;
	
	clmodel = e->model;

	if( R_CullBoxExt( pl->frustum, clmodel->mins, clmodel->maxs, pl->clipflags ))
		return;

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for( int i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
		if( R_CullSurfaceExt( psurf, pl->frustum, pl->clipflags ))
			continue;

		if( !( psurf->flags & (SURF_DRAWTILED|SURF_PORTAL|SURF_REFLECT)))
		{
			// keep light surfaces seperate from world chains
			SURF_INFO( psurf, RI.currentmodel )->lightchain = psurf->texinfo->texture->lightchain;
			psurf->texinfo->texture->lightchain = SURF_INFO( psurf, RI.currentmodel );
		}
	}
}

/*
=================
R_LightStaticBrushes

Insert static brushes into world texture chains
=================
*/
void R_LightStaticBrushes( const plight_t *pl )
{
	// draw static entities
	for( int i = 0; i < tr.num_static_entities; i++ )
	{
		RI.currententity = tr.static_entities[i];
		RI.currentmodel = RI.currententity->model;
	
		assert( RI.currententity != NULL );
		assert( RI.currententity->model != NULL );

		switch( RI.currententity->model->type )
		{
		case mod_brush:
			R_LightStaticModel( pl, RI.currententity );
			break;
		default:
			HOST_ERROR( "R_LightStatics: non bsp model in static list!\n" );
			break;
		}
	}

	// restore the world entity
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;
}

/*
=================
R_DrawShadowChains

Shadow pass, zbuffer only
=================
*/
void R_DrawShadowChains( void )
{
	int		j;
	msurface_t	*s;
	mextrasurf_t	*es;
	texture_t		*t;
	float		*v;

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		t = worldmodel->textures[i];
		if( !t ) continue;

		es = t->lightchain;
		if( !es ) continue;

		if( i == tr.skytexturenum )
			continue;

		for( ; es != NULL; es = es->lightchain )
		{
			s = INFO_SURF( es, RI.currentmodel );
			pglBegin( GL_POLYGON );

			for( j = 0, v = s->polys->verts[0]; j < s->polys->numverts; j++, v += VERTEXSIZE )
				pglVertex3fv( v );
			pglEnd();
		}
		t->lightchain = NULL;
	}
}

/*
================
R_DrawLightChains
================
*/
void R_DrawLightChains( void )
{
	R_LoadIdentity();	// set identity matrix

	// restore worldmodel
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;

	pglEnable( GL_BLEND );
	pglDepthMask( GL_FALSE );
	pglDepthFunc( GL_EQUAL );
	pglDisable( GL_ALPHA_TEST );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	R_DrawShadowChains();
	R_DrawGrass( GRASS_PASS_LIGHT );

	pglDisable( GL_BLEND );
	pglDepthMask( GL_TRUE );
	pglDepthFunc( GL_LEQUAL );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
}

/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights( void )
{
	plight_t	*pl;
	int	i;

	for( i = 0, pl = cl_plights; i < MAX_USER_PLIGHTS; i++, pl++ )
	{
		if( !pl->radius ) continue;

		pl->radius -= RI.refdef.frametime * pl->decay;
		if( pl->radius < 0 ) pl->radius = 0;

		if( pl->die < GET_CLIENT_TIME() || !pl->radius ) 
			memset( pl, 0, sizeof( *pl ));
	}
}

/*
=============
R_CountPlights
=============
*/
int R_CountPlights( bool countShadowLights )
{
	int numPlights = 0;

	if( !worldmodel->lightdata || r_fullbright->value )
		return numPlights;

	for( int i = 0; i < MAX_PLIGHTS; i++ )
	{
		plight_t *pl = &cl_plights[i];

		if( pl->die < GET_CLIENT_TIME() || !pl->radius )
			continue;

		// TODO: allow shadows for pointlights
		if(( pl->pointlight || FBitSet( pl->flags, CF_NOSHADOWS )) && countShadowLights )
			continue;

		numPlights++;
	}

	return numPlights;
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/
/*
==================
R_AnimateLight

==================
*/
void R_AnimateLight( void )
{
	int		k, flight, clight;
	float		l, c, lerpfrac, backlerp;
	float		scale;
	lightstyle_t	*ls;

	if( !RI.drawWorld || !worldmodel )
		return;

	if( R_OVERBRIGHT_SILENT() )
		scale = r_lighting_modulate->value / 2.0f;
	else
		scale = r_lighting_modulate->value;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	for( int i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		ls = GET_LIGHTSTYLE( i );

		if( r_fullbright->value || !worldmodel->lightdata )
		{
			RI.lightstylevalue[i] = 256 * 256;
			RI.lightcache[i] = 3.0f;
			continue;
		}

		// local time for each lightstyle is gurantee what each sequence has begin at start of pattern
		if( !RI.refdef.paused && RI.refdef.frametime <= 0.1f )
			ls->time += RI.refdef.frametime; // evaluate local time

		flight = (int)floor( ls->time * 10 );
		clight = (int)ceil( ls->time * 10 );
		lerpfrac = ( ls->time * 10 ) - flight;
		backlerp = 1.0f - lerpfrac;

		if( !ls->length )
		{
			RI.lightstylevalue[i] = 256 * scale;
			RI.lightcache[i] = 3.0f * scale;
			continue;
		}
		else if( ls->length == 1 )
		{
			// single length style so don't bother interpolating
			RI.lightstylevalue[i] = ls->map[0] * 22 * scale;
			RI.lightcache[i] = ( ls->map[0] / 12.0f ) * 3.0f * scale;
			continue;
		}
		else if( !ls->interp || !r_lightstyle_lerping->value )
		{
			RI.lightstylevalue[i] = ls->map[flight%ls->length] * 22 * scale;
			RI.lightcache[i] = ( ls->map[flight%ls->length] / 12.0f ) * 3.0f * scale;
			continue;
		}

		// interpolate animating light
		// frame just gone
		k = ls->map[flight % ls->length];
		l = (float)( k * 22.0f ) * backlerp;
		c = (float)( k / 12.0f ) * backlerp;

		// upcoming frame
		k = ls->map[clight % ls->length];
		l += (float)( k * 22.0f ) * lerpfrac;
		c += (float)( k / 12.0f ) * lerpfrac;

		RI.lightstylevalue[i] = (int)l * scale;
		RI.lightcache[i] = c * 3.0f * scale;
	}
}

/*
=============
R_PushDlights

copy dlights into projected lights
=============
*/
void R_PushDlights( void )
{
	dlight_t	*l;
	plight_t	*pl;

	for( int lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		l = GET_DYNAMIC_LIGHT( lnum );
		pl = &cl_plights[MAX_USER_PLIGHTS+lnum];

		// NOTE: here we copies dlight settings 'as-is'
		// without reallocating by key because key may
		// be set indirectly without call CL_AllocDlight
		if( l->die < GET_CLIENT_TIME() || !l->radius )
		{
			// light is expired. Clear it
			memset( pl, 0, sizeof( *pl ));
			continue;
		}

		pl->key = l->key;
		pl->die = l->die;
		pl->decay = l->decay;
		pl->pointlight = true;
		pl->color.r = l->color.r;
		pl->color.g = l->color.g;
		pl->color.b = l->color.b;	
		pl->projectionTexture = tr.dlightCubeTexture;
			
		R_SetupLightProjection( pl, l->origin, g_vecZero, l->radius, 90.0f );
		R_SetupLightAttenuationTexture( pl );
	}
}

/*
=======================================================================

	AMBIENT LIGHTING

=======================================================================
*/
static unsigned int	r_pointColor[3];
static Vector	r_lightSpot;

/*
=================
R_RecursiveLightPoint
=================
*/
static bool R_RecursiveLightPoint( model_t *model, mnode_t *node, const Vector &start, const Vector &end )
{
	float		front, back, frac;
	int		i, map, side, size, s, t;
	msurface_t	*surf;
	mtexinfo_t	*tex;
	color24		*lm;
	Vector		mid;

	// didn't hit anything
	if( !node || node->contents < 0 )
		return false;

	// calculate mid point
	front = PlaneDiff( start, node->plane );
	back = PlaneDiff( end, node->plane );

	side = front < 0;
	if(( back < 0 ) == side )
		return R_RecursiveLightPoint( model, node->children[side], start, end );

	frac = front / ( front - back );

	VectorLerp( start, frac, end, mid );

	// co down front side	
	if( R_RecursiveLightPoint( model, node->children[side], start, mid ))
		return true; // hit something

	if(( back < 0 ) == side )
		return false;// didn't hit anything

	r_lightSpot = mid;

	// check for impact on this node
	surf = model->surfaces + node->firstsurface;

	for( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
			continue;	// no lightmaps

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

		if(( s < 0 || s > surf->extents[0] ) || ( t < 0 || t > surf->extents[1] ))
			continue;

		s /= LM_SAMPLE_SIZE;
		t /= LM_SAMPLE_SIZE;

		if( !surf->samples )
			return true;

		r_pointColor[0] = r_pointColor[1] = r_pointColor[2] = 0;

		lm = surf->samples + (t * ((surf->extents[0] / LM_SAMPLE_SIZE ) + 1) + s);
		size = ((surf->extents[0] / LM_SAMPLE_SIZE) + 1) * ((surf->extents[1] / LM_SAMPLE_SIZE) + 1);

		for( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
		{
			unsigned int scale = RI.lightstylevalue[surf->styles[map]];

			r_pointColor[0] += TEXTURE_TO_TEXGAMMA( lm->r ) * scale;
			r_pointColor[1] += TEXTURE_TO_TEXGAMMA( lm->g ) * scale;
			r_pointColor[2] += TEXTURE_TO_TEXGAMMA( lm->b ) * scale;

			lm += size; // skip to next lightmap
		}
		return true;
	}

	// go down back side
	return R_RecursiveLightPoint( model, node->children[!side], mid, end );
}

/*
=================
R_LightTraceFilter
=================
*/
int R_LightTraceFilter( physent_t *pe )
{
	if( !pe || pe->solid != SOLID_BSP )
		return 1;
	return 0;
}

/*
=================
R_LightForPoint
=================
*/
void R_LightForPoint( const Vector &point, color24 *ambientLight, bool invLight, bool useAmbient, float radius )
{
	pmtrace_t		trace;
	cl_entity_t	*m_pGround;
	Vector		start, end, dir;
	float		dist, add;
	model_t		*pmodel;
	mnode_t		*pnodes;

	if( !RI.refdef.movevars )
	{
		ambientLight->r = 255;
		ambientLight->g = 255;
		ambientLight->b = 255;
		return;
	}

	// set to full bright if no light data
	if( !worldmodel || !worldmodel->lightdata )
	{
		ambientLight->r = TEXTURE_TO_TEXGAMMA( RI.refdef.movevars->skycolor_r );
		ambientLight->g = TEXTURE_TO_TEXGAMMA( RI.refdef.movevars->skycolor_g );
		ambientLight->b = TEXTURE_TO_TEXGAMMA( RI.refdef.movevars->skycolor_b );
		return;
	}

	// Get lighting at this point
	start = end = point;

	if( invLight )
	{
		start.z = point.z - 64;
		end.z = point.z + 8192;
	}
	else
	{
		start.z = point.z + 64;
		end.z = point.z - 8192;
	}

	// always have valid model
	pmodel = worldmodel;
	pnodes = pmodel->nodes;
	m_pGround = NULL;

	if( r_lighting_extended->value )
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTraceExt( start, end, PM_STUDIO_IGNORE, R_LightTraceFilter, &trace );
		m_pGround = GET_ENTITY( gEngfuncs.pEventAPI->EV_IndexFromTrace( &trace ));
	}

	if( m_pGround && m_pGround->model && m_pGround->model->type == mod_brush )
	{
		hull_t	*hull;
		Vector	start_l, end_l;
		Vector	offset;

		pmodel = m_pGround->model;
		pnodes = &pmodel->nodes[pmodel->hulls[0].firstclipnode];

		hull = &pmodel->hulls[0];
		offset = -hull->clip_mins;
		offset += m_pGround->origin;

		start_l = start - offset;
		end_l = end - offset;

		// rotate start and end into the models frame of reference
		if( m_pGround->angles != g_vecZero )
		{
			matrix4x4	matrix( offset, m_pGround->angles );
			start_l = matrix.VectorITransform( start );
			end_l = matrix.VectorITransform( end );
		}

		// copy transformed pos back
		start = start_l;
		end = end_l;
	}

	r_pointColor[0] = r_pointColor[1] = r_pointColor[2] = 0;

	if( R_RecursiveLightPoint( pmodel, pnodes, start, end ))
	{
		ambientLight->r = min((r_pointColor[0] >> 7), 255 );
		ambientLight->g = min((r_pointColor[1] >> 7), 255 );
		ambientLight->b = min((r_pointColor[2] >> 7), 255 );
	}
	else
	{
		float	ambient;

		// R_RecursiveLightPoint didn't hit anything, so use default value
		ambient = bound( 0.1f, r_lighting_ambient->value, 1.0f );
		if( !useAmbient ) ambient = 0.0f; // clear ambient
		ambientLight->r = 255 * ambient;
		ambientLight->g = 255 * ambient;
		ambientLight->b = 255 * ambient;
	}

	// add dynamic lights
	if( radius && r_dynamic->value )
	{
		int	lnum, total; 
		float	f;

		r_pointColor[0] = r_pointColor[1] = r_pointColor[2] = 0;

		// FIXME: using spotcone for projectors
		for( total = lnum = 0; lnum < MAX_PLIGHTS; lnum++ )
		{
			plight_t *pl = &cl_plights[lnum];

			if( pl->die < GET_CLIENT_TIME() || !pl->radius )
				continue;

			dir = pl->origin - point;
			dist = dir.Length();

			if( !dist || dist > pl->radius + radius )
				continue;

			add = 1.0f - (dist / ( pl->radius + radius ));
			r_pointColor[0] += TEXTURE_TO_TEXGAMMA( pl->color.r ) * add;
			r_pointColor[1] += TEXTURE_TO_TEXGAMMA( pl->color.g ) * add;
			r_pointColor[2] += TEXTURE_TO_TEXGAMMA( pl->color.b ) * add;
			total++;
		}

		if( total != 0 )
		{
			r_pointColor[0] += ambientLight->r;
			r_pointColor[1] += ambientLight->g;
			r_pointColor[2] += ambientLight->b;

			f = max( max( r_pointColor[0], r_pointColor[1] ), r_pointColor[2] );

			if( f > 1.0f )
			{
				r_pointColor[0] *= ( 255.0f / f );
				r_pointColor[1] *= ( 255.0f / f );
				r_pointColor[2] *= ( 255.0f / f );
			}

			ambientLight->r = r_pointColor[0];
			ambientLight->g = r_pointColor[1];
			ambientLight->b = r_pointColor[2];
		}
	}
}
