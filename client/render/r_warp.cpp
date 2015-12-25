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
#include "pm_movevars.h"
#include <mathlib.h>

#define MAX_CLIP_VERTS	64 // skybox clip vertices
static float		speedscale;
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

// speed up sin calculations
float r_turbsin[] =
{
	#include "warpsin.h"
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

		if( s < RI.skyMins[0][axis] ) RI.skyMins[0][axis] = s;
		if( t < RI.skyMins[1][axis] ) RI.skyMins[1][axis] = t;
		if( s > RI.skyMaxs[0][axis] ) RI.skyMaxs[0][axis] = s;
		if( t > RI.skyMaxs[1][axis] ) RI.skyMaxs[1][axis] = t;
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

	farclip = RI.farClip;

	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	for( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k-1] : b[k-1];
		v[j] += RI.vieworg[j];
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
		RI.skyMins[0][i] = RI.skyMins[1][i] =  99999.0f;
		RI.skyMaxs[0][i] = RI.skyMaxs[1][i] = -99999.0f;
	}
}

/*
=================
R_AddSkyBoxSurface
=================
*/
void R_AddSkyBoxSurface( msurface_t *fa )
{
	if( r_fastsky->value )
		return;

	if( RI.refdef.movevars->skyangle )
	{
		// HACK: force full sky to draw when it has angle
		for( int i = 0; i < 6; i++ )
		{
			RI.skyMins[0][i] = RI.skyMins[1][i] = -1;
			RI.skyMaxs[0][i] = RI.skyMaxs[1][i] = 1;
		}
	}

	Vector verts[MAX_CLIP_VERTS];

	// calculate vertex values for sky box
	for( glpoly_t *p = fa->polys; p; p = p->next )
	{
		if( p->numverts >= MAX_CLIP_VERTS )
			HOST_ERROR( "R_AddSkyBoxSurface: numverts >= MAX_CLIP_VERTS\n" );

		for( int i = 0; i < p->numverts; i++ )
		{
			verts[i][0] = p->verts[i][0] - RI.vieworg[0];
			verts[i][1] = p->verts[i][1] - RI.vieworg[1];
			verts[i][2] = p->verts[i][2] - RI.vieworg[2];
                    }

		ClipSkyPolygon( p->numverts, verts, 0 );
	}
}

/*
==============
R_DrawSkyPortal
==============
*/
void R_DrawSkyPortal( cl_entity_t *skyPortal )
{
	ref_instance_t	oldRI;

	oldRI = RI;

	int oldframecount = tr.framecount;
	RI.params = ( RI.params|RP_SKYPORTALVIEW ) & ~(RP_OLDVIEWLEAF|RP_CLIPPLANE);
	RI.pvsorigin = skyPortal->curstate.origin;

	RI.clipFlags = 15;

	if( skyPortal->curstate.scale )
	{
		Vector	centre, diff;
		float	scale;

		scale = skyPortal->curstate.scale / 100.0f;
		centre = (worldmodel->mins + worldmodel->maxs) * 0.5f;
		diff = centre - RI.vieworg;

		RI.refdef.vieworg = skyPortal->curstate.origin + (-scale * diff);
	}
	else
	{
		RI.refdef.vieworg = skyPortal->curstate.origin;
	}

	// angle offset
	RI.refdef.viewangles += skyPortal->curstate.angles;

	if( skyPortal->curstate.fuser2 )
	{
		RI.refdef.fov_x = skyPortal->curstate.fuser2;
		RI.refdef.fov_y = V_CalcFov( RI.refdef.fov_x, RI.refdef.viewport[2], RI.refdef.viewport[3] );

		if( RENDER_GET_PARM( PARM_WIDESCREEN, 0 ) && r_adjust_fov->value )
			V_AdjustFov( RI.refdef.fov_x, RI.refdef.fov_y, RI.refdef.viewport[2], RI.refdef.viewport[3], false );
	}

	r_viewleaf = r_viewleaf2 = NULL;	// force markleafs next frame

	R_RenderScene( &RI.refdef );

	r_viewleaf = r_viewleaf2 = NULL;	// force markleafs next frame
	tr.framecount = oldframecount;	// restore real framecount
	r_stats.c_sky_passes++;
	RI = oldRI;
}

/*
================
R_RecursiveSkyNode
================
*/
void R_RecursiveSkyNode( mnode_t *node, unsigned int clipflags )
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
		for( i = 0, clipplane = RI.frustum; i < 6; i++, clipplane++ )
		{
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
	R_RecursiveSkyNode( node->children[side], clipflags );

	// draw stuff
	for( c = node->numsurfaces, surf = worldmodel->surfaces + node->firstsurface; c; c--, surf++ )
	{
		if( R_CullSurface( surf, clipflags ))
			continue;

		if( surf->flags & SURF_DRAWSKY )
		{
			// sky is visible, break
			RI.isSkyVisible = true;
			return;
		}
	}

	// recurse down the back side
	R_RecursiveSkyNode( node->children[!side], clipflags );
}

/*
================
R_CheckSkyPortal

Build mirror chains for this frame
================
*/
void R_CheckSkyPortal( cl_entity_t *skyPortal )
{
	if( RENDER_GET_PARM( PARM_SKY_SPHERE, 0 ) || !worldmodel || r_fastsky->value )
		return;

	// build the transformation matrix for the given view angles
	RI.vieworg = RI.pvsorigin; // grab the PVS origin in case of mirror and the portal origin may be out of world bounds
	AngleVectors( RI.refdef.viewangles, RI.vforward, RI.vright, RI.vup );

	R_FindViewLeaf();

	R_SetupFrustum();
	R_MarkLeaves ();

	tr.modelorg = RI.pvsorigin;
	RI.currententity = GET_ENTITY( 0 );
	RI.currentmodel = RI.currententity->model;
	RI.isSkyVisible = false;

	R_RecursiveSkyNode( worldmodel->nodes, RI.clipFlags );

	if( RI.isSkyVisible )
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
	vec3_t	fogColor;
	int	i;

	if( RI.refdef.movevars->skyangle )
	{	
		// check for no sky at all
		for( i = 0; i < 6; i++ )
		{
			if( RI.skyMins[0][i] < RI.skyMaxs[0][i] && RI.skyMins[1][i] < RI.skyMaxs[1][i] )
				break;
		}

		if( i == 6 ) return; // nothing visible
	}

	RI.isSkyVisible = true;

	if( r_skyfog->value && ( RI.fogCustom || RI.fogEnabled ))
	{
		pglEnable( GL_FOG );

		if( RI.fogCustom )
		{
			pglFogi( GL_FOG_MODE, GL_LINEAR );
			pglFogf( GL_FOG_START, RI.fogStart );
			pglFogf( GL_FOG_END, RI.fogEnd );
                    }
                    else
                    {
			pglFogi( GL_FOG_MODE, GL_EXP );
			pglFogf( GL_FOG_DENSITY, RI.fogDensity );
                    }

		pglFogfv( GL_FOG_COLOR, RI.fogColor );
		pglHint( GL_FOG_HINT, GL_NICEST );
	}

	pglDisable( GL_BLEND );
	pglDisable( GL_ALPHA_TEST );
	pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	Vector skyDir( RI.refdef.movevars->skydir_x, RI.refdef.movevars->skydir_y, RI.refdef.movevars->skydir_z );

	if( RI.refdef.movevars->skyangle && skyDir != g_vecZero )
	{
		RI.modelviewMatrix = RI.worldviewMatrix;
		RI.modelviewMatrix.ConcatRotate( RI.refdef.movevars->skyangle, skyDir.x, skyDir.y, skyDir.z );
		RI.modelviewMatrix.ConcatTranslate( -RI.vieworg[0], -RI.vieworg[1], -RI.vieworg[2] );

		GL_LoadMatrix( RI.modelviewMatrix );
		tr.modelviewIdentity = false;
	}

	for( i = 0; i < 6; i++ )
	{
		if( RI.skyMins[0][i] >= RI.skyMaxs[0][i] || RI.skyMins[1][i] >= RI.skyMaxs[1][i] )
			continue;

		GL_Bind( GL_TEXTURE0, tr.skyboxTextures[r_skyTexOrder[i]] );

		pglBegin( GL_QUADS );
		MakeSkyVec( RI.skyMins[0][i], RI.skyMins[1][i], i );
		MakeSkyVec( RI.skyMins[0][i], RI.skyMaxs[1][i], i );
		MakeSkyVec( RI.skyMaxs[0][i], RI.skyMaxs[1][i], i );
		MakeSkyVec( RI.skyMaxs[0][i], RI.skyMins[1][i], i );
		pglEnd();
	}

	R_LoadIdentity();

	if( r_skyfog->value && ( RI.fogCustom || RI.fogEnabled ))
		pglDisable( GL_FOG );
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys( glpoly_t *polys, qboolean noCull, qboolean reflection )
{
	glpoly_t	*p;
	float	*v, nv, waveHeight;
	float	s, t, os, ot;
	int	i;

	if( noCull ) pglDisable( GL_CULL_FACE );

	// set the current waveheight
	waveHeight = RI.currentWaveHeight;

	for( p = polys; p; p = p->next )
	{
		pglBegin( GL_POLYGON );

		for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
		{
			if( reflection )
			{
				Vector stw = v;
				stw = (stw.Normalize()) * ( 6.0f + sin( RI.refdef.time * 0.1f ));
				stw.y += RI.refdef.time * 0.1f;

				pglMultiTexCoord3f( GL_TEXTURE1_ARB, stw.x, stw.y, stw.z );
				pglVertex3f( v[0], v[1], v[2] );
				continue;
			}

			if( waveHeight )
			{
				nv = v[2] + waveHeight + ( waveHeight * sin(v[0] * 0.02 + gEngfuncs.GetClientTime())
					* sin(v[1] * 0.02 + gEngfuncs.GetClientTime()) * sin(v[2] * 0.02 + gEngfuncs.GetClientTime()));
				nv -= waveHeight;
			}
			else nv = v[2];

			os = v[3];
			ot = v[4];

			s = os + r_turbsin[(int)((ot * 0.125f + gEngfuncs.GetClientTime() ) * TURBSCALE) & 255];
			s *= ( 1.0f / SUBDIVIDE_SIZE );

			t = ot + r_turbsin[(int)((os * 0.125f + gEngfuncs.GetClientTime() ) * TURBSCALE) & 255];
			t *= ( 1.0f / SUBDIVIDE_SIZE );

			pglTexCoord2f( s, t );
			pglVertex3f( v[0], v[1], nv );
		}
		pglEnd();
	}

	// restore culling
	if( noCull ) pglEnable( GL_CULL_FACE );
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolysReflection( msurface_t *fa )
{
	mextrasurf_t *es = SURF_INFO( fa, RI.currentmodel );

	if( !es || !es->mesh )
	{
		EmitWaterPolys( fa->polys, ( fa->flags & SURF_NOCULL ), ( fa->flags & SURF_REFLECT ));
		return;
	}

	// set the current waveheight
	float waveHeight = RI.currentWaveHeight;

	msurfmesh_t *mesh = es->mesh;
	Vector dir;

	pglBegin( GL_POLYGON );
	pglMultiTexCoord3f( GL_TEXTURE3_ARB, fa->plane->normal.x, fa->plane->normal.y, fa->plane->normal.z );

	for( int i = 0; i < mesh->numVerts; i++ )
	{
		dir = mesh->verts[i].vertex - RI.refdef.vieworg;
		Vector stw = mesh->verts[i].vertex.Normalize();
		stw *= ( waveHeight + sin( RI.refdef.time * 0.1f ));
		stw.x -= RI.refdef.time * 0.2f;
		stw.y += RI.refdef.time * 0.1f;

		pglMultiTexCoord3f( GL_TEXTURE1_ARB, stw.x, stw.y, mesh->verts[i].vertex.z );
		pglMultiTexCoord3f( GL_TEXTURE2_ARB, dir.x, dir.y, dir.z );
		pglVertex3fv( mesh->verts[i].vertex );
          }
	pglEnd();
}

/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys( msurface_t *fa )
{
	glpoly_t	*p;
	float	*v;
	int	i;
	float	s, t;
	Vector	dir;
	float	length;

	for( p = fa->polys; p; p = p->next )
	{
		pglBegin( GL_POLYGON );

		for( i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE )
		{
			dir[0] = v[0] - RI.vieworg[0];
			dir[1] = v[1] - RI.vieworg[1];
			dir[2] = (v[2] - RI.vieworg[2]) * 3; // flatten the sphere

			length = dir.Length();
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			s = ( speedscale + dir[0] ) * (1.0f / 128);
			t = ( speedscale + dir[1] ) * (1.0f / 128);

			pglTexCoord2f( s, t );
			pglVertex3fv( v );
		}
		pglEnd ();
	}
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain( msurface_t *s )
{
	msurface_t	*fa;

	GL_Bind( GL_TEXTURE0, tr.solidskyTexture );

	speedscale = gEngfuncs.GetClientTime() * 8;
	speedscale -= (int)speedscale & ~127;

	for( fa = s; fa; fa = fa->texturechain )
		EmitSkyPolys( fa );

	pglEnable( GL_BLEND );
	GL_Bind( GL_TEXTURE0, tr.alphaskyTexture );

	speedscale = gEngfuncs.GetClientTime() * 16;
	speedscale -= (int)speedscale & ~127;

	for( fa = s; fa; fa = fa->texturechain )
		EmitSkyPolys( fa );

	pglDisable( GL_BLEND );
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitSkyLayers( msurface_t *fa )
{
	GL_Bind( GL_TEXTURE0, tr.solidskyTexture );

	speedscale = gEngfuncs.GetClientTime() * 8;
	speedscale -= (int)speedscale & ~127;

	EmitSkyPolys( fa );

	pglEnable( GL_BLEND );
	GL_Bind( GL_TEXTURE0, tr.alphaskyTexture );

	speedscale = gEngfuncs.GetClientTime() * 16;
	speedscale -= (int)speedscale & ~127;

	EmitSkyPolys( fa );

	pglDisable( GL_BLEND );
}
