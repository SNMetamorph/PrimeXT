/*
r_decals.cpp - decal surfaces rendering
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
#include "pm_movevars.h"
#include "r_shader.h"
#include "r_world.h"

// gl_decals.c
#define MAX_DECALCLIPVERT		32
static struct decalarray_s
{
	Vector4D texcoord0;
	Vector4D texcoord1;
	Vector4D texcoord2;
	byte lightstyles[4];
	Vector position;
} decalarray[MAX_DECALCLIPVERT];

static GLuint decalvao;

/*
===============
R_ChooseDecalProgram

Select the program for surface (diffuse\puddle)
===============
*/
static word R_ChooseDecalProgram( decal_t *decal )
{
	return GL_UberShaderForBmodelDecal( decal );
}

/*
================
R_DrawProjectDecal
================
*/
void R_DrawProjectDecal( decal_t *decal, float *v, int numVerts )
{
	GL_Bind( GL_TEXTURE0, decal->texture );

	pglBegin( GL_POLYGON );
	for( int i = 0; i < numVerts; i++, v += VERTEXSIZE )
	{
		Vector av = RI->objectMatrix.VectorTransform( Vector( v ));
		pglTexCoord2f( v[3], v[4] );
		pglVertex3fv( av );
	}
	pglEnd();
}

/*
================
R_DrawSingleDecal
================
*/
bool DrawSingleDecal( decal_t *decal, word &hLastShader, bool project )
{
	int	numVerts;
	float	*v, s, t;
	Vector4D	lmcoord0, lmcoord1;

	// check for valid
	if( !decal->psurface )
		return false; // bad decal?

	cl_entity_t *e = GET_ENTITY( decal->entityIndex );
	v = DECAL_SETUP_VERTS( decal, decal->psurface, decal->texture, &numVerts );

	if( numVerts > MAX_DECALCLIPVERT ) // engine limit > renderer limit
		numVerts = MAX_DECALCLIPVERT;

	if( project )
	{
		R_DrawProjectDecal( decal, v, numVerts );
		return true;
	}

	// initialize decal shader
	if( !R_ChooseDecalProgram( decal ))
		return false;

	if( hLastShader != decal->shaderNum )
	{
		GL_BindShader( &glsl_programs[decal->shaderNum] );
		hLastShader = decal->shaderNum;
	}

	mextrasurf_t *es = decal->psurface->info;
	bvert_t *mv = &world->vertexes[es->firstvertex];
	msurface_t *surf = decal->psurface;
	mtexinfo_t *tex = surf->texinfo;
	Vector normal = FBitSet( surf->flags, SURF_PLANEBACK ) ? -surf->plane->normal : surf->plane->normal;
	int decalFlags = 0;

	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	pglUniform1fvARB( RI->currentshader->u_LightStyleValues, MAX_LIGHTSTYLES, &tr.lightstyles[0] );
	pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );

	GL_Bind( GL_TEXTURE0, decal->texture );
	GL_Bind( GL_TEXTURE1, surf->texinfo->texture->gl_texturenum );
	GL_Bind( GL_TEXTURE2, tr.lightmaps[es->lightmaptexturenum].lightmap );

	// update transformation matrix
	gl_state_t *glm = &tr.cached_state[e->hCachedMatrix];
	pglUniformMatrix4fvARB( RI->currentshader->u_ModelMatrix, 1, GL_FALSE, &glm->modelMatrix[0] );
	r_stats.c_total_tris += (numVerts - 2);

	// TODO: sort decal by programs to make this batch
	if( !decalvao )
	{
		pglGenVertexArrays( 1, &decalvao );
		pglBindVertexArray( decalvao );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD0, 4, GL_FLOAT, GL_FALSE, sizeof(struct decalarray_s), &decalarray->texcoord0);
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD0 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD1, 4, GL_FLOAT, GL_FALSE, sizeof(struct decalarray_s),  &decalarray->texcoord1 );
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD1 );
		pglVertexAttribPointerARB( ATTR_INDEX_TEXCOORD2, 4, GL_FLOAT, GL_FALSE, sizeof(struct decalarray_s),  &decalarray->texcoord2 );
		pglEnableVertexAttribArrayARB( ATTR_INDEX_TEXCOORD2 );
		pglVertexAttribPointerARB( ATTR_INDEX_LIGHT_STYLES, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(struct decalarray_s),  &decalarray->lightstyles );
		pglEnableVertexAttribArrayARB( ATTR_INDEX_LIGHT_STYLES );
		pglVertexAttribPointerARB( ATTR_INDEX_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(struct decalarray_s), &decalarray->position );
		pglEnableVertexAttribArrayARB( ATTR_INDEX_POSITION );
	}
	else
		pglBindVertexArray( decalvao );

	for( int i = 0; i < numVerts; i++, v += VERTEXSIZE )
	{
		s = (DotProductA( v, tex->vecs[0] ) + tex->vecs[0][3] ) / tex->texture->width;
		t = (DotProductA( v, tex->vecs[1] ) + tex->vecs[1][3] ) / tex->texture->height;
		R_LightmapCoords( surf, v, lmcoord0, 0 ); // styles 0-1
		R_LightmapCoords( surf, v, lmcoord1, 2 ); // styles 2-3
		decalarray[i].texcoord0.Init(v[3], v[4], s, t);
		decalarray[i].texcoord1 = lmcoord0;
		decalarray[i].texcoord2 = lmcoord1;
		memcpy( decalarray[i].lightstyles, surf->styles, 4 );

		if( !CVAR_TO_BOOL( r_polyoffset ))
			decalarray[i].position = Vector( v ) + normal * 0.05;
		else decalarray[i].position = v;
	}

	pglDrawArrays(GL_POLYGON, 0, numVerts);
	pglBindVertexArray(0);

	return true;
}

void DrawSurfaceDecals( msurface_t *fa, bool reverse, bool project )
{
	word	hLastShader = 0xFFFF;
	decal_t	*p;

	if( !fa->pdecals ) return;

	if( reverse && RI->currententity->curstate.rendermode == kRenderTransTexture )
	{
		decal_t	*list[1024];
		int	i, count;

		for( p = fa->pdecals, count = 0; p && count < 1024; p = p->pnext )
			if( p->texture ) list[count++] = p;

		for( i = count - 1; i >= 0; i-- )
			DrawSingleDecal( list[i], hLastShader, project );
	}
	else
	{
		for( p = fa->pdecals; p; p = p->pnext )
		{
			DrawSingleDecal( p, hLastShader, project );
		}
	}
}

void DrawDecalsBatch( void )
{	
	cl_entity_t	*e;
	int		i;

	if( !tr.num_draw_decals )
		return;

	e = RI->currententity;
	ASSERT( e != NULL );
	pglDisable( GL_ALPHA_TEST );

	if( e->curstate.rendermode != kRenderTransTexture )
	{
		pglEnable( GL_BLEND );
		pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglDepthMask( GL_FALSE );
	}

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		GL_Cull( GL_NONE );

	if( CVAR_TO_BOOL( r_polyoffset ))
	{
		pglEnable( GL_POLYGON_OFFSET_FILL );
		pglPolygonOffset( -1.0f, -r_polyoffset->value );
	}

	for( i = 0; i < tr.num_draw_decals; i++ )
	{
		DrawSurfaceDecals( tr.draw_decals[i], false, false );
	}

	if( R_CountPlights( ))
	{
		RI->objectMatrix = matrix4x4( e->origin, e->angles );	// FIXME

		for( i = 0; i < MAX_PLIGHTS; i++ )
		{
			plight_t *pl = &cl_plights[i];

			if( pl->die < tr.time || !pl->radius || pl->culled )
				continue;

			if( !R_BeginDrawProjectionGLSL( pl, 0.5f ))
				continue;

			for( int k = 0; k < tr.num_draw_decals; k++ )
			{
				DrawSurfaceDecals( tr.draw_decals[k], false, true );
			}

			R_EndDrawProjectionGLSL();
		}
	}

	if( e->curstate.rendermode != kRenderTransTexture )
	{
		pglDepthMask( GL_TRUE );
		pglDisable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
	}

	if( CVAR_TO_BOOL( r_polyoffset ))
		pglDisable( GL_POLYGON_OFFSET_FILL );

	if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
		GL_Cull( GL_FRONT );

	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	GL_BindShader( NULL );

	tr.num_draw_decals = 0;
}

