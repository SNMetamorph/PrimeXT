/*
r_warp.cpp - sky and water polygons
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
#include "r_world.h"
#include "r_shader.h"

#define MAX_CLIP_VERTS	64 // skybox clip vertices
static const int		r_skyTexOrder[6] = { 0, 2, 1, 3, 4, 5 };

static const Vector	skyclip[6] = 
{
Vector(  1,  1,  0 ),
Vector(  1, -1,  0 ),
Vector(  0, -1,  1 ),
Vector(  0,  1,  1 ),
Vector(  1,  0,  1 ),
Vector( -1,  0,  1 ) 
};

// 1 = s, 2 = t, 3 = 2048
static const int st_to_vec[6][3] =
{
{  3, -1,  2 },
{ -3,  1,  2 },
{  1,  3,  2 },
{ -1, -3,  2 },
{ -2, -1,  3 },  // 0 degrees yaw, look straight up
{  2, -1, -3 }   // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3] =
{
{ -2,  3,  1 },
{  2,  3, -1 },
{  1,  3,  2 },
{ -1,  3, -2 },
{ -2, -1,  3 },
{ -2,  1, -3 }
};

void DrawSkyPolygon( int nump, Vector vecs[] )
{
	int	i, j, axis;
	float	s, t, dv;
	Vector	v, av;

	// decide which face it maps to
	v = g_vecZero;

	for( i = 0; i < nump; i++ )
	{
		v += vecs[i];
	}

	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );

	if( av[0] > av[1] && av[0] > av[2] )
		axis = (v[0] < 0) ? 1 : 0;
	else if( av[1] > av[2] && av[1] > av[0] )
		axis = (v[1] < 0) ? 3 : 2;
	else axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for( i = 0; i < nump; i++ )
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? (vecs[i])[j-1] : -(vecs[i])[-j-1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[i][-j-1] / dv : (vecs[i])[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -(vecs[i])[-j-1] / dv : (vecs[i])[j-1] / dv;

		if( s < RI->skyMins[0][axis] ) RI->skyMins[0][axis] = s;
		if( t < RI->skyMins[1][axis] ) RI->skyMins[1][axis] = t;
		if( s > RI->skyMaxs[0][axis] ) RI->skyMaxs[0][axis] = s;
		if( t > RI->skyMaxs[1][axis] ) RI->skyMaxs[1][axis] = t;
	}
}

/*
==============
ClipSkyPolygon
==============
*/
void ClipSkyPolygon( int nump, Vector vecs[], int stage )
{
	qboolean		front, back;
	float		dists[MAX_CLIP_VERTS + 1];
	int		sides[MAX_CLIP_VERTS + 1];
	Vector		newv[2][MAX_CLIP_VERTS + 1];
	int		newc[2];
	float		d, e;
	int		i;

	if( nump > MAX_CLIP_VERTS )
		HOST_ERROR( "ClipSkyPolygon: MAX_CLIP_VERTS\n" );
loc1:
	if( stage == 6 )
	{	
		// fully clipped, so draw it
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = false;
	const Vector &norm = skyclip[stage];

	for( i = 0; i < nump; i++ )
	{
		d = DotProduct( vecs[i], norm );
		if( d > ON_EPSILON )
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if( d < -ON_EPSILON )
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if( !front || !back )
	{	
		// not clipped
		stage++;
		goto loc1;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	vecs[i] = vecs[0];
	newc[0] = newc[1] = 0;

	for( i = 0; i < nump; i++ )
	{
		switch( sides[i] )
		{
		case SIDE_FRONT:
			newv[0][newc[0]] = vecs[i];
			newc[0]++;
			break;
		case SIDE_BACK:
			newv[1][newc[1]] = vecs[i];
			newc[1]++;
			break;
		case SIDE_ON:
			newv[0][newc[0]] = vecs[i];
			newc[0]++;
			newv[1][newc[1]] = vecs[i];
			newc[1]++;
			break;
		}

		if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		d = dists[i] / ( dists[i] - dists[i+1] );

		for( int j = 0; j < 3; j++ )
		{
			e = (vecs[i])[j] + d * ( (vecs[i+1])[j] - (vecs[i])[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1], stage + 1 );
}

static void MakeSkyVec( float s, float t, int axis )
{
	int	j, k, farclip;
	Vector	v, b;

	farclip = RI->farClip;

	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	for( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k-1] : b[k-1];
		v[j] += RI->vieworg[j];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5f;
	t = (t + 1) * 0.5f;

	if( s < 1.0f / 512.0f )
		s = 1.0f / 512.0f;
	else if( s > 511.0f / 512.0f )
		s = 511.0f / 512.0f;
	if( t < 1.0f / 512.0f )
		t = 1.0f / 512.0f;
	else if( t > 511.0f / 512.0f )
		t = 511.0f / 512.0f;

	t = 1.0f - t;

	pglTexCoord2f( s, t );
	pglVertex3fv( v );
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox( void )
{
	for( int i = 0; i < 6; i++ )
	{
		RI->skyMins[0][i] = RI->skyMins[1][i] =  99999.0f;
		RI->skyMaxs[0][i] = RI->skyMaxs[1][i] = -99999.0f;
	}

	ClearBits( RI->params, RP_SKYVISIBLE ); // now sky is invisible
}

/*
=================
R_AddSkyBoxSurface
=================
*/
void R_AddSkyBoxSurface( msurface_t *fa )
{
	Vector verts[MAX_CLIP_VERTS];

	// calculate vertex values for sky box
	for( glpoly_t *p = fa->polys; p; p = p->next )
	{
		if( p->numverts >= MAX_CLIP_VERTS )
			HOST_ERROR( "R_AddSkyBoxSurface: numverts >= MAX_CLIP_VERTS\n" );

		for( int i = 0; i < p->numverts; i++ )
		{
			verts[i][0] = p->verts[i][0] - RI->vieworg[0];
			verts[i][1] = p->verts[i][1] - RI->vieworg[1];
			verts[i][2] = p->verts[i][2] - RI->vieworg[2];
                    }

		ClipSkyPolygon( p->numverts, verts, 0 );
	}

	SetBits( RI->params, RP_SKYVISIBLE ); // now sky is visible
}

/*
==============
R_DrawSkyPortal
==============
*/
void R_DrawSkyPortal( cl_entity_t *skyPortal )
{
	R_PushRefState();

	RI->params = ( RI->params|RP_SKYPORTALVIEW ) & ~(RP_OLDVIEWLEAF|RP_CLIPPLANE);
	RI->pvsorigin = skyPortal->curstate.origin;
	RI->frustum.DisablePlane( FRUSTUM_FAR );
	RI->frustum.DisablePlane( FRUSTUM_NEAR );

	if( skyPortal->curstate.scale )
	{
		Vector	centre, diff;
		float	scale;

		// TODO: use world->mins and world->maxs instead ?
		centre = (worldmodel->mins + worldmodel->maxs) * 0.5f;
		scale = skyPortal->curstate.scale / 100.0f;
		diff = centre - RI->vieworg;

		RI->vieworg = skyPortal->curstate.origin + (-scale * diff);
	}
	else
	{
		RI->vieworg = skyPortal->curstate.origin;
	}

	// angle offset
	RI->viewangles += skyPortal->curstate.angles;

	if( skyPortal->curstate.fuser2 )
	{
		RI->fov_x = skyPortal->curstate.fuser2;
		RI->fov_y = V_CalcFov( RI->fov_x, RI->viewport[2], RI->viewport[3] );

		if( RENDER_GET_PARM( PARM_WIDESCREEN, 0 ) && CVAR_TO_BOOL( r_adjust_fov ))
			V_AdjustFov( RI->fov_x, RI->fov_y, RI->viewport[2], RI->viewport[3], false );
	}

	r_stats.c_sky_passes++;
	R_RenderScene();
	R_PopRefState();
}

void R_WorldFindSky( void )
{
	CFrustum		*frustum = &RI->frustum;
	msurface_t	*surf, **mark;
	mworldleaf_t	*leaf;
	int		i, j;

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

		if( !FBitSet( surf->flags, SURF_DRAWSKY ))
			continue;	// only skyfaces interesting us

		if( CHECKVISBIT( RI->visfaces, j ))
		{
			RI->currententity = GET_ENTITY( 0 );
			RI->currentmodel = RI->currententity->model;

			if( R_CullSurfaceExt( surf, frustum ))
				continue;

			SetBits( RI->params, RP_SKYVISIBLE ); // now sky is visible
			return;
		}
	}
}

/*
================
R_CheckSkyPortal

Build mirror chains for this frame
================
*/
void R_CheckSkyPortal( cl_entity_t *skyPortal )
{
	tr.fIgnoreSkybox = false;

	if( tr.sky_camera == NULL )
		return;

	if( !CVAR_TO_BOOL( r_allow_3dsky ))
		return;

	if( FBitSet( RI->params, RP_OVERVIEW ))
		return;

	// don't allow recursive 3dsky
	if( FBitSet( RI->params, RP_SKYPORTALVIEW ))
		return;

	if( !FBitSet( RI->params, RP_OLDVIEWLEAF ))
		R_FindViewLeaf();
	R_SetupFrustum();
	R_MarkLeaves();

	if( FBitSet( RI->params, RP_MIRRORVIEW ))
		tr.modelorg = RI->pvsorigin;
	else tr.modelorg = RI->vieworg;
	RI->currententity = GET_ENTITY( 0 );
	RI->currentmodel = RI->currententity->model;
	R_WorldFindSky();

	if( FBitSet( RI->params, RP_SKYVISIBLE ))
	{
		R_DrawSkyPortal( skyPortal );
		tr.fIgnoreSkybox = true;
	}
}

/*
==============
R_DrawSkybox
==============
*/
void R_DrawSkyBox( void )
{
	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	// make sure what light_environment is present
	if( tr.sky_normal != g_vecZero && CheckShader( glsl.skyboxEnv ))
	{
		Vector sky_color = tr.sky_ambient * (1.0f / 128.0f) * r_lighting_modulate->value;
		Vector sky_vec = tr.sky_normal.Normalize();

		GL_BindShader( glsl.skyboxEnv );

		ColorNormalize( sky_color, sky_color );
		pglUniform3fARB( RI->currentshader->u_LightDir, sky_vec.x, sky_vec.y, sky_vec.z );
		pglUniform3fARB( RI->currentshader->u_LightDiffuse, sky_color.x, sky_color.y, sky_color.z );
		pglUniform3fARB( RI->currentshader->u_ViewOrigin, RI->vieworg.x, RI->vieworg.y, RI->vieworg.z );
		pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity * 0.25f );
	}

	for( int i = 0; i < 6; i++ )
	{
		if( RI->skyMins[0][i] >= RI->skyMaxs[0][i] || RI->skyMins[1][i] >= RI->skyMaxs[1][i] )
			continue;

		GL_Bind( GL_TEXTURE0, tr.skyboxTextures[r_skyTexOrder[i]] );

		pglBegin( GL_QUADS );
		MakeSkyVec( RI->skyMins[0][i], RI->skyMins[1][i], i );
		MakeSkyVec( RI->skyMins[0][i], RI->skyMaxs[1][i], i );
		MakeSkyVec( RI->skyMaxs[0][i], RI->skyMaxs[1][i], i );
		MakeSkyVec( RI->skyMaxs[0][i], RI->skyMins[1][i], i );
		pglEnd();
	}

	GL_BindShader( NULL );
	R_LoadIdentity();
}
