/*
gl_studiodecal_new.cpp - project and rendering studio decals
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
#include "gl_local.h"
#include "com_model.h"
#include "r_studioint.h"
#include "pm_movevars.h"
#include "gl_studio.h"
#include "gl_sprite.h"
#include "event_api.h"
#include <mathlib.h>
#include "pm_defs.h"
#include "stringlib.h"
#include "triangleapi.h"
#include "entity_types.h"
#include "gl_shader.h"
#include "gl_world.h"
#include "gl_cvars.h"

/*
=============================================================

	COMMON ROUTINES

=============================================================
*/
/*
====================
PushEntityState

====================
*/
void CStudioModelRenderer :: PushEntityState( cl_entity_t *ent )
{
	ASSERT( ent != NULL );

	m_savestate = ent->curstate;
	m_saveorigin = ent->origin;
	m_saveangles = ent->angles;
}

/*
====================
PopEntityState

====================
*/
void CStudioModelRenderer :: PopEntityState( cl_entity_t *ent )
{
	ent->curstate = m_savestate;
	ent->origin = m_saveorigin;
	ent->angles = m_saveangles;
}

/*
====================
EntityToModelState

create snapshot of current model state
====================
*/
void CStudioModelRenderer :: EntityToModelState( modelstate_t *state, const cl_entity_t *ent )
{
	state->sequence = ent->curstate.sequence;
	state->frame = (int)( ent->curstate.frame * 8.0f );
	state->blending[0] = ent->curstate.blending[0];
	state->blending[1] = ent->curstate.blending[1];
	state->controller[0] = ent->curstate.controller[0];
	state->controller[1] = ent->curstate.controller[1];
	state->controller[2] = ent->curstate.controller[2];
	state->controller[3] = ent->curstate.controller[3];
	state->body = ent->curstate.body;
	state->skin = ent->curstate.skin;
}

/*
====================
ModelStateToEntity

setup entity pose for decal shooting
====================
*/
void CStudioModelRenderer :: ModelStateToEntity( cl_entity_t *ent, const modelstate_t *state )
{
	ent->curstate.sequence = state->sequence;
	ent->curstate.frame = (float)state->frame * (1.0f / 8.0f);
	ent->curstate.blending[0] = state->blending[0];
	ent->curstate.blending[1] = state->blending[1];
	ent->curstate.controller[0] = state->controller[0];
	ent->curstate.controller[1] = state->controller[1];
	ent->curstate.controller[2] = state->controller[2];
	ent->curstate.controller[3] = state->controller[3];
	ent->curstate.body = state->body;
	ent->curstate.skin = state->skin;
}

/*
=============================================================

	CLEANUP ROUTINES

=============================================================
*/
void CStudioModelRenderer :: PurgeDecals( ModelInstance_t *inst )
{
	for( int i = 0; i < inst->m_DecalList.Count(); i++ )
	{
		studiodecal_t *pDecal = &inst->m_DecalList[i];
	}

	// release himself
	inst->m_DecalList.Purge();
}

void CStudioModelRenderer :: PurgeDecals( cl_entity_t *e )
{
	if( !e || e->modelhandle == INVALID_HANDLE )
		return;

	PurgeDecals( &m_ModelInstances[e->modelhandle] );
}

float CStudioModelRenderer::StudioSequenceDuration(const cl_entity_t *ent, int sequence)
{
	if (!StudioSetEntity((cl_entity_t *)ent))
		return 0.0f;
	return m_boneSetup.LocalDuration(sequence);
}

void CStudioModelRenderer :: StudioClearDecals( void )
{
	if( !g_fRenderInitialized ) return;

	for( int i = 0; i < m_ModelInstances.Count(); i++ )
		PurgeDecals( &m_ModelInstances[i] );

	// clear BSP-decals too
	ClearDecals();
}

//-----------------------------------------------------------------------------
// Purpose: Removes all the decals on a model instance
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: RemoveAllDecals( int entityIndex )
{
	if( !g_fRenderInitialized ) return;

	PurgeDecals( GET_ENTITY( entityIndex ));
}

/*
=============================================================

	SERIALIZE

=============================================================
*/
int CStudioModelRenderer :: StudioDecalList( decallist_t *pBaseList, int count )
{
	if( !g_fRenderInitialized )
		return 0;

	int maxStudioDecals = MAX_STUDIO_DECALS + (MAX_BMODEL_DECALS - count);
	decallist_t *pList = pBaseList + count;	// shift list to first free slot
	cl_entity_t *pEntity = NULL;
	int total = 0;

	for( int i = 0; i < m_ModelInstances.Count(); i++ )
          {
		ModelInstance_t *inst = &m_ModelInstances[i];

		if( !inst->m_DecalList.Count( ))
			continue;	// no decals?

		// setup the decal entity
		pEntity = inst->m_pEntity;

		for( int i = 0; i < inst->m_DecalList.Count(); i++ )
		{
			studiodecal_t *pdecal = &inst->m_DecalList[i];
			const DecalGroupEntry *tex = pdecal->texinfo;

			if( !tex || !tex->group || FBitSet( pdecal->flags, FDECAL_DONTSAVE ))
				continue; // ???

			pList[total].depth = pdecal->depth; // FIXME: calc depth properly
			pList[total].flags = pdecal->flags|FDECAL_STUDIO;
			pList[total].entityIndex = pEntity->index;
			pList[total].position = pdecal->position;
			pList[total].impactPlaneNormal = pdecal->normal;
			pList[total].studio_state = inst->pose_stamps[pdecal->modelpose];
			Q_snprintf( pList[total].name, sizeof( pList[0].name ), "%s@%s", tex->group->GetName(), tex->m_DecalName );
			pList[total].scale = 1.0f; // unused
			total++;

			// check for list overflow
			if( total >= maxStudioDecals )
			{
				ALERT( at_error, "StudioDecalList: too many studio decals on save\restore\n" );
				goto end_serialize;
			}
		}
	}
end_serialize:
	total += SaveDecalList( pBaseList, count + total );

	return total;
}

/*
=============================================================

	DECAL CLIPPING & PLACING

=============================================================
*/
/*
====================
ComputePoseToDecal

Create a transform that projects world coordinates
into a basis for the decal
====================
*/
bool CStudioModelRenderer :: ComputePoseToDecal( const Vector &vecNormal, const Vector &vecEnd )
{
	Vector decalU, decalV, decalN, vecDelta;
	Vector vecSrc = vecEnd + vecNormal.Normalize() * 5.0f; // magic Valve constant
	matrix3x4	worldToDecal;

	// Bloat a little bit so we get the intersection
	vecDelta = (vecEnd - vecSrc) * 1.1f;

	// Get the z axis
	decalN = vecDelta * -1.0f;
	if( decalN.NormalizeLength() == 0.0f )
		return false;

	// Deal with the u axis
	decalU = CrossProduct( Vector( 0.0f, 0.0f, 1.0f ), decalN );

	if( decalU.NormalizeLength() < 1e-3 )
	{
		// if up parallel or antiparallel to ray, deal...
		Vector fixup( 0.0f, 1.0f, 0.0f );
		decalU = CrossProduct( fixup, decalN );

		if( decalU.NormalizeLength() < 1e-3 )
			return false;
	}

	decalV = CrossProduct( decalN, decalU );

	// Since I want world-to-decal, I gotta take the inverse of the decal
	// to world. Assuming post-multiplying column vectors, the decal to world = 
	// [ Ux Vx Nx | vecEnd[0] ]
	// [ Uy Vy Ny | vecEnd[1] ]
	// [ Uz Vz Nz | vecEnd[2] ]

	worldToDecal[0][0] = decalU.x;
	worldToDecal[1][0] = decalU.y;
	worldToDecal[2][0] = decalU.z;

	worldToDecal[0][1] = decalV.x;
	worldToDecal[1][1] = decalV.y;
	worldToDecal[2][1] = decalV.z;

	worldToDecal[0][2] = decalN.x;
	worldToDecal[1][2] = decalN.y;
	worldToDecal[2][2] = decalN.z;

	// g-cont. just invert matrix here?
	worldToDecal[3][0] = -DotProduct( vecEnd, decalU );
	worldToDecal[3][1] = -DotProduct( vecEnd, decalV );
	worldToDecal[3][2] = -DotProduct( vecEnd, decalN );

	// Compute transforms from pose space to decal plane space
	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		m_pdecaltransform[i] = worldToDecal.ConcatTransforms( m_pworldtransform[i] );
	}

	return true;
}

/*
====================
IsFrontFacing

side checking
====================
*/
_forceinline bool CStudioModelRenderer :: IsFrontFacing( const svert_t *vert )
{
	float z = 0.0f;
	Vector decalN;

	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to decal
	if( vert->weight[0] == 255 )
	{
		decalN.x = m_pdecaltransform[vert->boneid[0]][0][2];
		decalN.y = m_pdecaltransform[vert->boneid[0]][1][2];
		decalN.z = m_pdecaltransform[vert->boneid[0]][2][2];
		z = DotProduct( vert->normal, decalN );
	}
	else
	{
		for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
		{
			if( vert->boneid[i] == -1 )
				break;
			decalN.x = m_pdecaltransform[vert->boneid[i]][0][2];
			decalN.y = m_pdecaltransform[vert->boneid[i]][1][2];
			decalN.z = m_pdecaltransform[vert->boneid[i]][2][2];
			z += DotProduct( vert->normal, decalN ) * (vert->weight[i] / 255.0f);
		}
	}

	return ( z >= 0.1f );
}

/*
====================
GetDecalRotateTransform

====================
*/
_forceinline matrix3x3 CStudioModelRenderer :: GetDecalRotateTransform( byte vertexBone )
{
	matrix3x3	transform;

	transform[0][0] = m_pdecaltransform[vertexBone][0][0];
	transform[0][1] = m_pdecaltransform[vertexBone][1][0];
	transform[0][2] = m_pdecaltransform[vertexBone][2][0];

	transform[1][0] = m_pdecaltransform[vertexBone][0][1];
	transform[1][1] = m_pdecaltransform[vertexBone][1][1];
	transform[1][2] = m_pdecaltransform[vertexBone][2][1];

	transform[2][0] = m_pdecaltransform[vertexBone][0][2];
	transform[2][1] = m_pdecaltransform[vertexBone][1][2];
	transform[2][2] = m_pdecaltransform[vertexBone][2][2];

	return transform;
}

/*
====================
TransformToDecalSpace

====================
*/
_forceinline bool CStudioModelRenderer :: TransformToDecalSpace( DecalBuildInfo_t& build, const svert_t *vert, Vector2D& uv )
{
	cl_entity_t *e = RI->currententity;

	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to world
	float z;

	if( vert->weight[0] == 255 )
	{
		matrix3x3 decalMat = GetDecalRotateTransform( vert->boneid[0] );
		uv.x = (DotProduct( vert->vertex, decalMat[0] ) + m_pdecaltransform[vert->boneid[0]][3][0]);
		uv.y = -(DotProduct( vert->vertex, decalMat[1] ) + m_pdecaltransform[vert->boneid[0]][3][1]);
		z = DotProduct( vert->vertex, decalMat[2] ) + m_pdecaltransform[vert->boneid[0]][3][2];
	}
	else
	{
		uv.x = uv.y = z = 0.0f;

		for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
		{
			if( vert->boneid[i] == -1 )
				break;

			matrix3x3 decalMat = GetDecalRotateTransform( vert->boneid[i] );
			float weight = (vert->weight[i] / 255.0f);

			uv.x += (DotProduct( vert->vertex, decalMat[0] ) + m_pdecaltransform[vert->boneid[i]][3][0]) * weight;
			uv.y += -(DotProduct( vert->vertex, decalMat[1] ) + m_pdecaltransform[vert->boneid[i]][3][1]) * weight;
			z += (DotProduct( vert->vertex, decalMat[2] ) + m_pdecaltransform[vert->boneid[i]][3][2]) * weight;
		}
	}

	// poke thru
	if( FBitSet( e->curstate.iuser1, CF_STATIC_ENTITY ) && !e->efrag )
		return ( fabs( z ) < build.m_Radius );
	return true;
}

/*
====================
ProjectDecalOntoMesh

====================
*/
void CStudioModelRenderer :: ProjectDecalOntoMesh( DecalBuildInfo_t& build )
{
	float invRadius = (build.m_Radius != 0.0f) ? 1.0f / build.m_Radius : 1.0f;

	// for this to work, the plane and intercept must have been transformed
	// into pose space. Also, we'll not be bothering with flexes.
	for( int j = 0; j < m_nNumArrayVerts; j++ )
	{
		// just walk by unique verts
		svert_t *vert = &m_arrayxvert[j];

		// No decal vertex yet...
		build.m_pVertexInfo[j].m_VertexIndex = INVALID_VERTEX_INDEX;

		// We need to know if the normal is pointing in the negative direction
		// if so, blow off all triangles connected to that vertex.
		build.m_pVertexInfo[j].m_FrontFacing = IsFrontFacing( vert );

		if( !build.m_pVertexInfo[j].m_FrontFacing )
			continue;

		build.m_pVertexInfo[j].m_InValidArea = TransformToDecalSpace( build, vert, build.m_pVertexInfo[j].m_UV );
		build.m_pVertexInfo[j].m_UV *= build.vecDecalScale * 0.5f;
		build.m_pVertexInfo[j].m_UV[0] += 0.5f;
		build.m_pVertexInfo[j].m_UV[1] += 0.5f;
	}
}

/*
====================
ComputeClipFlags

====================
*/
int CStudioModelRenderer :: ComputeClipFlags( const Vector2D &uv )
{
	// Otherwise we gotta do the test
	int flags = 0;

	if( uv.x < 0.0f )
		SetBits( flags, DECAL_CLIP_MINUSU );
	else if( uv.x > 1.0f )
		SetBits( flags, DECAL_CLIP_PLUSU );

	if( uv.y < 0.0f )
		SetBits( flags, DECAL_CLIP_MINUSV );
	else if( uv.y > 1.0f )
		SetBits( flags, DECAL_CLIP_PLUSV );

	return flags;
}

/*
====================
ConvertMeshVertexToDecalVertex

Converts a mesh index to a svert_t
====================
*/
void CStudioModelRenderer :: ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int vertIndex, svert_t *out )
{
	// copy over the data (we use through access for all submodel verts)
	*out = m_arrayxvert[vertIndex];

	// get the texture coords from the decal planar projection
	out->stcoord[0] = build.m_pVertexInfo[vertIndex].m_UV.x;
	out->stcoord[1] = build.m_pVertexInfo[vertIndex].m_UV.y;

	// if meshIndex is valid this vertex was created from original
	out->m_MeshVertexIndex = vertIndex;
}

/*
====================
AddVertexToDecal

Adds a vertex to the list of vertices
for this studiomesh
====================
*/
int32_t CStudioModelRenderer :: AddVertexToDecal( DecalBuildInfo_t& build, int vertIndex )
{
	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// If we've never seen this vertex before, we need to add a new decal vert
	if( pVertexInfo[vertIndex].m_VertexIndex == INVALID_VERTEX_INDEX )
	{
		int v = build.m_Vertices.AddToTail();

		// Copy over the data;
		ConvertMeshVertexToDecalVertex( build, vertIndex, &build.m_Vertices[v] );
#ifdef _DEBUG
		// Make sure clipped vertices are in the right range...
		if( build.m_UseClipVert )
		{
			ASSERT(( build.m_Vertices[v].stcoord[0] >= -1e-3 ) && ( build.m_Vertices[v].stcoord[0] - 1.0f < 1e-3 ));
			ASSERT(( build.m_Vertices[v].stcoord[1] >= -1e-3 ) && ( build.m_Vertices[v].stcoord[1] - 1.0f < 1e-3 ));
		}
#endif
		// store off the index of this vertex so we can reference it again
		pVertexInfo[vertIndex].m_VertexIndex = v;
	}

	return pVertexInfo[vertIndex].m_VertexIndex;
}

/*
====================
AddVertexToDecal

This creates a unique vertex
====================
*/
int32_t CStudioModelRenderer :: AddVertexToDecal( DecalBuildInfo_t& build, svert_t *vert )
{
	// Try to see if the clipped vertex already exists in our decal list...
	// Only search for matches with verts appearing in the current decal
	for( int i = 0; i < build.m_Vertices.Count(); i++ )
	{
		svert_t *test = &build.m_Vertices[i];

		// Only bother to check against clipped vertices
		if( test->m_MeshVertexIndex != INVALID_VERTEX_INDEX )
			continue;

		if( !VectorCompareEpsilon( test->vertex, vert->vertex, 1e-3 ))
			continue;

		if( !VectorCompareEpsilon( test->normal, vert->normal, 1e-3 ))
			continue;

		return i;
	}

	// this path is the path taken by clipped vertices
	ASSERT(( vert->stcoord[0] >= -1e-3 ) && ( vert->stcoord[0] - 1.0f < 1e-3 ));
	ASSERT(( vert->stcoord[1] >= -1e-3 ) && ( vert->stcoord[1] - 1.0f < 1e-3 ));

	// must create a new vertex...
	return build.m_Vertices.AddToTail( *vert );
}

//-----------------------------------------------------------------------------
// Creates a new vertex where the edge intersects the plane
//-----------------------------------------------------------------------------
int CStudioModelRenderer :: IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val )
{
	svert_t *startVert = &state.m_ClipVerts[start];
	svert_t *endVert = &state.m_ClipVerts[end];

	Vector2D dir = Vector2D ( endVert->stcoord[0] - startVert->stcoord[0], endVert->stcoord[1] - startVert->stcoord[1] );
	ASSERT( dir[normalInd] != 0.0f );

	float t = (val - startVert->stcoord[normalInd]) / dir[normalInd];
				 
	// Allocate a clipped vertex
	svert_t *out = &state.m_ClipVerts[state.m_ClipVertCount];
	int newVert = state.m_ClipVertCount++;

	// the clipped vertex has no analogue in the original mesh
	out->m_MeshVertexIndex = INVALID_VERTEX_INDEX;

	// just select bone by interpolation factor
	if( t <= 0.5f )
	{
		memcpy( out->boneid, startVert->boneid, 4 );
		memcpy( out->weight, startVert->weight, 4 );
	}
	else
	{
		memcpy( out->boneid, endVert->boneid, 4 );
		memcpy( out->weight, endVert->weight, 4 );
	}

	// interpolate position
	out->vertex = startVert->vertex * (1.0f - t) + endVert->vertex * t;

	// interpolate tangent
	out->tangent = startVert->tangent * (1.0f - t) + endVert->tangent * t;
	out->tangent = out->tangent.Normalize();

	// interpolate binormal
	out->binormal = startVert->binormal * (1.0f - t) + endVert->binormal * t;
	out->binormal = out->binormal.Normalize();

	// interpolate normal
	out->normal = startVert->normal * (1.0f - t) + endVert->normal * t;
	out->normal = out->normal.Normalize();

	// interpolate texture coord
	out->stcoord[0] = startVert->stcoord[0] + (endVert->stcoord[0] - startVert->stcoord[0]) * t;
	out->stcoord[1] = startVert->stcoord[1] + (endVert->stcoord[1] - startVert->stcoord[1]) * t;
	out->stcoord[2] = startVert->stcoord[2] + (endVert->stcoord[2] - startVert->stcoord[2]) * t;
	out->stcoord[3] = startVert->stcoord[3] + (endVert->stcoord[3] - startVert->stcoord[3]) * t;

	ASSERT( !IS_NAN( out->stcoord[0] ) && !IS_NAN( out->stcoord[1] ));

	// interpolate lighting (we need unpack, interpolate and pack again)
	for( int map = 0; map < MAXLIGHTMAPS; map++ )
	{
		color24 startCol = UnpackColor( startVert->light[map] );
		color24 startNrm = UnpackColor( startVert->deluxe[map] );
		color24 endCol = UnpackColor( endVert->light[map] );
		color24 endNrm = UnpackColor( endVert->deluxe[map] );
		out->light[map] = PackColor( startCol * (1.0f - t) + endCol * t );
		out->deluxe[map] = PackColor( startNrm * (1.0f - t) + endNrm * t );
	}

	// Compute the clip flags baby...
	state.m_ClipFlags[newVert] = ComputeClipFlags( Vector2D( out->stcoord ));

	return newVert;
}

/*
====================
ClipTriangleAgainstPlane

Clips a triangle against a plane, 
use clip flags to speed it up
====================
*/
void CStudioModelRenderer :: ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val )
{
	// Ye Olde Sutherland-Hodgman clipping algorithm
	int start = state.m_Indices[state.m_Pass][state.m_VertCount - 1];
	bool startInside = FBitSet( state.m_ClipFlags[start], flag ) == 0;
	int outVertCount = 0;

	for( int i = 0; i < state.m_VertCount; i++ )
	{
		int end = state.m_Indices[state.m_Pass][i];

		bool endInside = (state.m_ClipFlags[end] & flag) == 0;

		if( endInside )
		{
			if( !startInside )
			{
				int clipVert = IntersectPlane( state, start, end, normalInd, val );
				state.m_Indices[!state.m_Pass][outVertCount++] = clipVert;
			}
			state.m_Indices[!state.m_Pass][outVertCount++] = end;
		}
		else
		{
			if( startInside )
			{
				int clipVert = IntersectPlane( state, start, end, normalInd, val );
				state.m_Indices[!state.m_Pass][outVertCount++] = clipVert;
			}
		}

		start = end;
		startInside = endInside;
	}

	state.m_Pass = !state.m_Pass;
	state.m_VertCount = outVertCount;
}

/*
====================
AddClippedDecalToTriangle

Adds the clipped triangle to the decal
====================
*/
void CStudioModelRenderer :: AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState )
{
	int32_t indices[7];
	int i;

	ASSERT( clipState.m_VertCount <= 7 );

	// Yeah baby yeah!!	Add this sucka
	for( i = 0; i < clipState.m_VertCount; i++ )
	{
		// First add the vertices
		int vertIdx = clipState.m_Indices[clipState.m_Pass][i];

		if( vertIdx < 3 )
		{
			indices[i] = AddVertexToDecal( build, clipState.m_ClipVerts[vertIdx].m_MeshVertexIndex );
		}
		else
		{
			indices[i] = AddVertexToDecal( build, &clipState.m_ClipVerts[vertIdx] );
		}
	}

	// Add a trifan worth of triangles
	for( i = 1; i < clipState.m_VertCount - 1; i++ )
	{
		build.m_Indices.AddToTail( indices[0] );
		build.m_Indices.AddToTail( indices[i] );
		build.m_Indices.AddToTail( indices[i+1] );
	}
}

/*
====================
ClipDecal

Clips the triangle to +/- radius
====================
*/
bool CStudioModelRenderer :: ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags )
{
	DecalClipState_t clipState;
	clipState.m_VertCount = 3;

	ConvertMeshVertexToDecalVertex( build, i1, &clipState.m_ClipVerts[0] );
	ConvertMeshVertexToDecalVertex( build, i2, &clipState.m_ClipVerts[1] );
	ConvertMeshVertexToDecalVertex( build, i3, &clipState.m_ClipVerts[2] );

	clipState.m_ClipVertCount = 3;

	for( int i = 0; i < 3; i++ )
	{
		clipState.m_ClipFlags[i] = pClipFlags[i];
		clipState.m_Indices[0][i] = i;
	}

	clipState.m_Pass = 0;

	// Clip against each plane
	ClipTriangleAgainstPlane( clipState, 0, DECAL_CLIP_MINUSU, 0.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	ClipTriangleAgainstPlane( clipState, 0, DECAL_CLIP_PLUSU, 1.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	ClipTriangleAgainstPlane( clipState, 1, DECAL_CLIP_MINUSV, 0.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	ClipTriangleAgainstPlane( clipState, 1, DECAL_CLIP_PLUSV, 1.0f );
	if( clipState.m_VertCount < 3 )
		return false;

	// Only add the clipped decal to the triangle if it's one bone
	// otherwise just return if it was clipped
	if( build.m_UseClipVert )
	{
		AddClippedDecalToTriangle( build, clipState );
	}

	return true;
}

/*
====================
AddTriangleToDecal

Adds a decal to a triangle,
but only if it should
====================
*/
void CStudioModelRenderer :: AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 )
{
	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// All must be front-facing for a decal to be added
	if(( !pVertexInfo[i1].m_FrontFacing ) || ( !pVertexInfo[i2].m_FrontFacing ) || ( !pVertexInfo[i3].m_FrontFacing ))
		return;

	// This is used to prevent poke through; if the points are too far away
	// from the contact point, then don't add the decal
	if(( !pVertexInfo[i1].m_InValidArea ) && ( !pVertexInfo[i2].m_InValidArea ) && ( !pVertexInfo[i3].m_InValidArea ))
		return;

	// Clip to +/- radius
	int clipFlags[3];

	clipFlags[0] = ComputeClipFlags( pVertexInfo[i1].m_UV );
	clipFlags[1] = ComputeClipFlags( pVertexInfo[i2].m_UV );
	clipFlags[2] = ComputeClipFlags( pVertexInfo[i3].m_UV );

	// Cull... The result is non-zero if they're all outside the same plane
	if(( clipFlags[0] & ( clipFlags[1] & clipFlags[2] )) != 0 )
		return;

	bool doClip = true;
	
	// Trivial accept for skinned polys... if even one vert is inside the draw region, accept
	if(( !build.m_UseClipVert ) && ( !clipFlags[0] || !clipFlags[1] || !clipFlags[2] ))
		doClip = false;

	// Trivial accept... no clip flags set means all in
	// Don't clip if we have more than one bone... we'll need to do skinning
	// and we can't clip the bone indices
	// We *do* want to clip in the one bone case though; useful for static props.
	if( doClip && ( clipFlags[0] || clipFlags[1] || clipFlags[2] ))
	{
		bool validTri = ClipDecal( build, i1, i2, i3, clipFlags );

		// Don't add the triangle if we culled the triangle or if 
		// we had one or less bones
		if( build.m_UseClipVert || ( !validTri ))
			return;
	}

	// Add the vertices to the decal since there was no clipping
	build.m_Indices.AddToTail( AddVertexToDecal( build, i1 ));
	build.m_Indices.AddToTail( AddVertexToDecal( build, i2 ));
	build.m_Indices.AddToTail( AddVertexToDecal( build, i3 ));
}

word CStudioModelRenderer :: ShaderDecalForward( studiodecal_t *pDecal, bool vertex_lighting )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];

	if( pDecal->forwardScene.IsValid( ))
		return pDecal->forwardScene.GetHandle();

	Q_strncpy( glname, "forward/decal_studio", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	const DecalGroupEntry *texinfo = pDecal->texinfo;
	vbomesh_t *pMesh = pDecal->modelmesh;
	short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	mstudiomaterial_t *mat = &RI->currentmodel->materials[pskinref[pMesh->skinref]];
	bool hasNormalmap = texinfo->gl_normalmap_id != tr.normalmapTexture;
	bool hasHeightmap = texinfo->gl_heightmap_id != tr.whiteTexture;

	if (tr.fogEnabled && !RP_CUBEPASS())
		GL_AddShaderDirective( options, "APPLY_FOG_EXP" );

	if( !texinfo->opaque )
	{
		// GL_DST_COLOR, GL_SRC_COLOR
		GL_AddShaderDirective( options, "APPLY_COLORBLEND" );
	}

	if (hasHeightmap && texinfo->matdesc->reliefScale > 0.0f)
	{
		if( cv_parallax->value == 1.0f )
			GL_AddShaderDirective( options, "PARALLAX_SIMPLE" );
		else if( cv_parallax->value >= 2.0f )
			GL_AddShaderDirective( options, "PARALLAX_OCCLUSION" );
	}

	if (!texinfo->opaque || FBitSet(mat->flags, STUDIO_NF_FULLBRIGHT) || R_FullBright())
	{
		GL_AddShaderDirective( options, "LIGHTING_FULLBRIGHT" );
	}
	else
	{
		if (vertex_lighting)
			GL_AddShaderDirective(options, "VERTEX_LIGHTING");
		else if (FBitSet(mat->flags, STUDIO_NF_FLATSHADE))
			GL_AddShaderDirective(options, "LIGHTING_FLATSHADE");
	}

	if (CVAR_TO_BOOL(cv_brdf))
		GL_AddShaderDirective(options, "APPLY_PBS");

	// debug visualization
	if( r_lightmap->value > 0.0f && r_lightmap->value <= 2.0f )
	{
		if( r_lightmap->value == 1.0f && worldmodel->lightdata )
			GL_AddShaderDirective( options, "LIGHTMAP_DEBUG" );
		else if( r_lightmap->value == 2.0f && FBitSet( world->features, WORLD_HAS_DELUXEMAP ))
			GL_AddShaderDirective( options, "LIGHTVEC_DEBUG" );
	}

	if (hasHeightmap || hasNormalmap)
		GL_AddShaderDirective( options, "COMPUTE_TBN" );
 
	// deluxemap required
	if( CVAR_TO_BOOL( cv_bump ) && FBitSet( world->features, WORLD_HAS_DELUXEMAP ) && hasNormalmap)
	{
		GL_AddShaderDirective( options, "HAS_NORMALMAP" );
		GL_EncodeNormal( options, mat->gl_normalmap_id );
	}

	// deluxemap required
	if( CVAR_TO_BOOL( cv_specular ) && FBitSet( world->features, WORLD_HAS_DELUXEMAP ) && texinfo->gl_specular_id != tr.blackTexture )
		GL_AddShaderDirective( options, "HAS_GLOSSMAP" );

	if( m_pStudioHeader->numbones == 1 )
		GL_AddShaderDirective( options, "MAXSTUDIOBONES 1" );

	if( FBitSet( m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS ))
		GL_AddShaderDirective( options, "APPLY_BONE_WEIGHTING" );

	if( FBitSet( mat->flags, STUDIO_NF_MASKED ))
		GL_AddShaderDirective( options, "ALPHA_TEST" );

	word shaderNum = GL_FindUberShader( glname, options );
	if( !shaderNum ) return 0;

	// done
	pDecal->forwardScene.SetShader( shaderNum );
	return shaderNum;
}

void CStudioModelRenderer :: ComputeDecalTBN( DecalBuildInfo_t& build )
{
	if( build.m_pTexInfo->gl_normalmap_id == tr.normalmapTexture && build.m_pTexInfo->gl_heightmap_id == tr.whiteTexture )
		return; // not needs

	// build a map from vertex to a list of triangles that share the vert.
	CUtlArray<CIntVector> vertToTriMap;

	vertToTriMap.AddMultipleToTail( build.m_Vertices.Count() );
	int triID;

	for( triID = 0; triID < (build.m_Indices.Count() / 3); triID++ )
	{
		vertToTriMap[build.m_Indices[triID*3+0]].AddToTail( triID );
		vertToTriMap[build.m_Indices[triID*3+1]].AddToTail( triID );
		vertToTriMap[build.m_Indices[triID*3+2]].AddToTail( triID );
	}

	// calculate the tangent space for each triangle.
	CUtlArray<Vector> triSVect, triTVect;
	triSVect.AddMultipleToTail( (build.m_Indices.Count() / 3) );
	triTVect.AddMultipleToTail( (build.m_Indices.Count() / 3) );

	float	*v[3], *tc[3];

	for( triID = 0; triID < (build.m_Indices.Count() / 3); triID++ )
	{
		for( int i = 0; i < 3; i++ )
		{
			v[i] = (float *)&build.m_Vertices[build.m_Indices[triID*3+i]].vertex;
			tc[i] = (float *)&build.m_Vertices[build.m_Indices[triID*3+i]].stcoord;
		}

		CalcTBN( v[0], v[1], v[2], tc[0], tc[1], tc[2], triSVect[triID], triTVect[triID] );
	}	

	// calculate an average tangent space for each vertex.
	for( int vertID = 0; vertID < build.m_Vertices.Count(); vertID++ )
	{
		svert_t *v = &build.m_Vertices[vertID];
		const Vector &normal = v->normal;
		Vector &finalSVect = v->tangent;
		Vector &finalTVect = v->binormal;
		Vector sVect, tVect;

		sVect = tVect = g_vecZero;

		for( triID = 0; triID < vertToTriMap[vertID].Size(); triID++ )
		{
			sVect += triSVect[vertToTriMap[vertID][triID]];
			tVect += triTVect[vertToTriMap[vertID][triID]];
		}

        sVect = sVect.Normalize();
        tVect = tVect.Normalize();            
        sVect = sVect - normal * DotProduct(normal, sVect);
        tVect = CrossProduct(normal, sVect) * Q_sign(DotProduct(CrossProduct(normal, sVect), tVect));

		finalSVect = sVect.Normalize();
		finalTVect = tVect.Normalize();
	}
}

void CStudioModelRenderer :: DecalCreateBuffer( DecalBuildInfo_t& build, studiodecal_t *pDecal )
{
	vbomesh_t *pMesh = pDecal->modelmesh;
	mstudiomaterial_t *pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials;
	short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	pmaterial = &pmaterial[pskinref[pMesh->skinref]];

	bool has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	bool has_vertexlight = ( build.m_pVertexLight ) ? true : false;
	bool has_lightmap = false;
	bool has_bumpmap = false;

	if( pDecal->texinfo->gl_normalmap_id != tr.normalmapTexture || pDecal->texinfo->gl_heightmap_id != tr.whiteTexture )
		has_bumpmap = true;

	ShaderDecalForward( pDecal, has_vertexlight );

	vbomesh_t *pOut = &pDecal->mesh;

	pOut->numVerts = build.m_Vertices.Count();
	pOut->numElems = build.m_Indices.Count();

	GL_CheckVertexArrayBinding();

	// determine optimal mesh loader
	uint attribs = ComputeAttribFlags( m_pStudioHeader->numbones, has_bumpmap, has_boneweights, has_vertexlight, has_lightmap );
	uint type = SelectMeshLoader( m_pStudioHeader->numbones, has_bumpmap, has_boneweights, has_vertexlight, has_lightmap );

	// move data to video memory
	if( glConfig.version < ACTUAL_GL_VERSION )
		m_pfnMeshLoaderGL21[type].CreateBuffer( pOut, build.m_Vertices.Base() );
	else m_pfnMeshLoaderGL30[type].CreateBuffer( pOut, build.m_Vertices.Base() );
	CreateIndexBuffer( pOut, build.m_Indices.Base() );

	// link it with vertex array object
	pglGenVertexArrays( 1, &pOut->vao );
	pglBindVertexArray( pOut->vao );
	if( glConfig.version < ACTUAL_GL_VERSION )
		m_pfnMeshLoaderGL21[type].BindBuffer( pOut, attribs );
	else m_pfnMeshLoaderGL30[type].BindBuffer( pOut, attribs );
	BindIndexBuffer( pOut );
	pglBindVertexArray( GL_FALSE );

	// update stats
	tr.total_vbo_memory += pOut->cacheSize;
}

void CStudioModelRenderer :: AllocDecalForMesh( DecalBuildInfo_t& build )
{
	if( build.m_Vertices.Count() <= 0 || build.m_Indices.Count() <= 0 )
		return; // something went wrong

	int index = m_pModelInstance->m_DecalList.AddToTail();
	studiodecal_t *pDecal = &m_pModelInstance->m_DecalList[index];

	// copy settings
	pDecal->normal = build.vecLocalNormal;
	pDecal->position = build.vecLocalEnd;
	pDecal->flags = build.decalFlags;

	if( !build.current )
	{
		// move modelstate into static memory
		build.poseState = m_pModelInstance->pose_stamps.AddToTail( *build.modelState );
		build.current = pDecal; // first decal is saved, next ignored
	}
	else SetBits( pDecal->flags, FDECAL_DONTSAVE );

	pDecal->modelmesh = build.m_pModelMesh;
	pDecal->modelpose = build.poseState;
	pDecal->texinfo = build.m_pTexInfo;
	pDecal->depth = build.decalDepth;

	// calc TBN if needs
	ComputeDecalTBN( build );

	// place vertices into VBO
	DecalCreateBuffer( build, pDecal );
}

void CStudioModelRenderer :: AddDecalToMesh( DecalBuildInfo_t& build )
{
	const vbomesh_t *mesh = build.m_pModelMesh;
	CBoundingBox meshBounds = StudioGetMeshBounds(m_pModelInstance, mesh);
	
	if( !BoundsAndSphereIntersect( meshBounds.GetMins(), meshBounds.GetMaxs(), build.vecLocalEnd, build.m_Radius))
		return; // no intersection

	build.m_Vertices.Purge();
	build.m_Indices.Purge();

	for( int j = 0; j < build.m_pDecalMesh->numindices; j += 3 )
	{
		int starttri = build.m_pDecalMesh->firstindex + j;
		AddTriangleToDecal( build, m_arrayelems[starttri+0], m_arrayelems[starttri+1], m_arrayelems[starttri+2] );
	}

	// allocate decal for mesh
	AllocDecalForMesh( build );
}

void CStudioModelRenderer :: AddDecalToModel( DecalBuildInfo_t& buildInfo )
{
	Vector *pstudioverts = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	Vector *pstudionorms = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	byte *pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	bool has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	int i, numVerts;
	
	std::vector<StudioMesh_t> decalMeshBuffer;
	decalMeshBuffer.resize(m_pSubModel->nummesh);
	StudioMesh_t *pDecalMesh = decalMeshBuffer.data();

	if( !pDecalMesh ) 
		return; // empty mesh?

	buildInfo.m_pVertexLight = NULL;

	if( buildInfo.modelLight != NULL )
	{
		int offset = (byte *)m_pSubModel - (byte *)m_pStudioHeader; // search for submodel offset

		for( i = 0; i < MAXSTUDIOMODELS; i++ )
		{
			if( buildInfo.modelLight->submodels[i].submodel_offset == offset )
				break;			
		}

		// has vertexlighting for this submodel
		if( i != MAXSTUDIOMODELS )
			buildInfo.m_pVertexLight = &buildInfo.modelLight->verts[buildInfo.modelLight->submodels[i].vertex_offset];
	}

	m_nNumArrayVerts = m_nNumArrayElems = 0;
	m_nNumLightVerts = 0;

	// if weights was missed their offsets just equal to 0
	mstudioboneweight_t	*pvertweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + m_pSubModel->blendvertinfoindex);

	// build all the data for current submodel
	for( i = 0; i < m_pSubModel->nummesh; i++ ) 
	{
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + i;
		mstudiomaterial_t *pmaterial = &RI->currentmodel->materials[pskinref[pmesh->skinref]];
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
		StudioMesh_t *pCurMesh = pDecalMesh + i;

		pCurMesh->firstvertex = m_nNumArrayVerts;
		pCurMesh->firstindex = m_nNumArrayElems;
		pCurMesh->numvertices = 0;
		pCurMesh->numindices = 0;

		mstudiotexture_t *ptexture = pmaterial->pSource;
		float s = 1.0f / (float)ptexture->width;
		float t = 1.0f / (float)ptexture->height;

		while(( numVerts = *( ptricmds++ )))
		{
			bool	strip = ( numVerts < 0 ) ? false : true;
			int	vertexState = 0;

			numVerts = abs( numVerts );

			for( ; numVerts > 0; numVerts--, ptricmds += 4 )
			{
				// build in indices
				if( vertexState++ < 3 )
				{
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}
				else if( strip )
				{
					// flip triangles between clockwise and counter clockwise
					if( vertexState & 1 )
					{
						// draw triangle [n-2 n-1 n]
						m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 2;
						m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 1;
						m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
					}
					else
					{
						// draw triangle [n-1 n-2 n]
						m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 1;
						m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 2;
						m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
					}
				}
				else
				{
					// draw triangle fan [0 n-1 n]
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - ( vertexState - 1 );
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts - 1;
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}

				// store mesh into uniform vertex for consistency
				svert_t *out = &m_arrayxvert[m_nNumArrayVerts];

				out->vertex = pstudioverts[ptricmds[0]];
				out->normal = pstudionorms[ptricmds[1]].Normalize();
				out->binormal = g_vecZero;
				out->tangent = g_vecZero;
				out->stcoord[0] = 0.0f;
				out->stcoord[1] = 0.0f;

				if (has_boneweights)
				{
					mstudioboneweight_t	*pCurWeight = &pvertweight[ptricmds[0]];
					out->boneid[0] = pCurWeight->bone[0];
					out->boneid[1] = pCurWeight->bone[1];
					out->boneid[2] = pCurWeight->bone[2];
					out->boneid[3] = pCurWeight->bone[3];
					out->weight[0] = pCurWeight->weight[0];
					out->weight[1] = pCurWeight->weight[1];
					out->weight[2] = pCurWeight->weight[2];
					out->weight[3] = pCurWeight->weight[3];
				}
				else
				{
					out->boneid[0] = pvertbone[ptricmds[0]];
					out->boneid[1] = -1;
					out->boneid[2] = -1;
					out->boneid[3] = -1;
					out->weight[0] = 255;
					out->weight[1] = 0;
					out->weight[2] = 0;
					out->weight[3] = 0;
				}

				if( FBitSet( ptexture->flags, STUDIO_NF_CHROME ))
				{
					// probably always equal 64 (see studiomdl.c for details)
					out->stcoord[2] = s;
					out->stcoord[3] = t;
				}
				else if( FBitSet( ptexture->flags, STUDIO_NF_UV_COORDS ))
				{
					out->stcoord[2] = HalfToFloat( ptricmds[2] );
					out->stcoord[3] = HalfToFloat( ptricmds[3] );
				}
				else
				{
					out->stcoord[2] = ptricmds[2] * s;
					out->stcoord[3] = ptricmds[3] * t;
				}

				// accumulate vertex lighting too
				if( buildInfo.m_pVertexLight != NULL )
				{
					dvertlight_t *vl = &buildInfo.m_pVertexLight[m_nNumLightVerts++];

					// now setup light and deluxe vector
					for( int map = 0; map < MAXLIGHTMAPS; map++ )
					{
						out->light[map] = PackColor( vl->light[map] );
						out->deluxe[map] = PackColor( vl->deluxe[map] );
					}
				}
				m_nNumArrayVerts++;
			}
		}

		pCurMesh->numvertices = m_nNumArrayVerts - pCurMesh->firstvertex;
		pCurMesh->numindices = m_nNumArrayElems - pCurMesh->firstindex;
	}

	// should keep all the verts of this submodel, because we use direct access by vertex number
	std::vector<DecalVertexInfo_t> decalVertexInfoBuffer;
	decalVertexInfoBuffer.resize(m_nNumArrayVerts);
	buildInfo.m_pVertexInfo = decalVertexInfoBuffer.data();

	// project all vertices for this group into decal space
	// Note we do this work at a mesh level instead of a model level
	// because vertices are not shared across mesh boundaries
	ProjectDecalOntoMesh( buildInfo );

	// NOTE: we should add the individual decals for each mesh
	// to effectively sorting while renderer translucent meshes
	for( i = 0; i < m_pSubModel->nummesh; i++ )
	{
		// setup mesh pointers
		buildInfo.m_pModelMesh = &m_pVboModel->meshes[i];
		buildInfo.m_pDecalMesh = pDecalMesh + i;
		AddDecalToMesh( buildInfo );
	}
}

/*
====================
StudioDecalShoot

simplified version
====================
*/
void CStudioModelRenderer :: StudioDecalShoot( struct pmtrace_s *tr, const char *name, bool visent )
{
	if( !g_fRenderInitialized )
		return;

	if( tr->allsolid || tr->fraction == 1.0 || tr->ent < 0 )
		return;

	physent_t *pe = NULL;

	if( visent ) pe = gEngfuncs.pEventAPI->EV_GetVisent( tr->ent );
 	else pe = gEngfuncs.pEventAPI->EV_GetPhysent( tr->ent );
	if( !pe ) return;

	Vector scale = Vector( 1.0f, 1.0f, 1.0f );
	int entityIndex = pe->info;
	int flags = FDECAL_STUDIO;
	modelstate_t state;
	int modelIndex = 0;

	// modelindex is needs for properly save\restore
	cl_entity_t *ent = GET_ENTITY( entityIndex );

	if( !ent )
	{
		// something very bad happens...
		ALERT( at_error, "StudioDecal: ent == NULL\n" );
		return;
	}

	modelIndex = ent->curstate.modelindex;

	// restore model in case decalmessage was delivered early than delta-update
	if( !ent->model && modelIndex != 0 )
		ent->model = IEngineStudio.GetModelByIndex( modelIndex );

	if( !ent->model || ent->model->type != mod_studio )
		return;

	Vector vecNormal = tr->plane.normal;
	Vector vecEnd = tr->endpos;

	PushEntityState( ent );
	// create snapshot of current model state
	EntityToModelState( &state, ent );

	if( FBitSet( ent->curstate.iuser1, CF_STATIC_ENTITY ))
	{
		if( ent->curstate.startpos != g_vecZero )
			scale = Vector( 1.0f / ent->curstate.startpos.x, 1.0f / ent->curstate.startpos.y, 1.0f / ent->curstate.startpos.z );
	}

	// studio decals is always converted into local space to avoid troubles with precision and modelscale
	matrix4x4 mat = matrix4x4( ent->origin, ent->angles, scale );
	vecNormal = mat.VectorIRotate( vecNormal );
	vecEnd = mat.VectorITransform( vecEnd );
	SetBits( flags, FDECAL_LOCAL_SPACE ); // now it's in local space

	// simulate network degradation for match results
	vecNormal.x = Q_rint( vecNormal.x * 1000.0f ) * 0.001f;
	vecNormal.y = Q_rint( vecNormal.y * 1000.0f ) * 0.001f;
	vecNormal.z = Q_rint( vecNormal.z * 1000.0f ) * 0.001f;
	vecEnd.x = Q_rint( vecEnd.x );
	vecEnd.y = Q_rint( vecEnd.y );
	vecEnd.z = Q_rint( vecEnd.z );

	StudioDecalShoot( vecNormal, vecEnd, name, ent, flags, &state );
	PopEntityState( ent );
}

/*
====================
StudioDecalShoot

NOTE: name is decalgroup
====================
*/
void CStudioModelRenderer :: StudioDecalShoot( const Vector &vecNormal, const Vector &vecEnd, const char *name, cl_entity_t *ent, int flags, modelstate_t *state )
{
	DecalBuildInfo_t buildInfo;
	float flLocalScale = 1.0f;
	
	if( !g_fRenderInitialized )
		return;

	// setup studio pointers
	if( !StudioSetEntity( ent ))
		return;

	if( m_pStudioHeader->numbodyparts == 0 )
		return; // null model?

	// this sucker is state needed only when building decals
	buildInfo.m_pTexInfo = DecalGroup::GetEntry( name, flags );
	if( !buildInfo.m_pTexInfo ) 
		return;
	DecalGroupEntry *entry = (DecalGroupEntry *)buildInfo.m_pTexInfo;

	// time to cache decal textures
	entry->PreloadTextures();

	if( entry->gl_diffuse_id == TextureHandle::Null() )
	{
		ALERT( at_warning, "StudioDecal: decal texture '%s' was missed\n", entry->m_DecalName );
		return; // decal texture was missed.
	}

	// make sure what time is actual
	tr.time = GET_CLIENT_TIME();
	tr.oldtime = GET_CLIENT_OLDTIME();

	if( FBitSet( flags, FDECAL_LOCAL_SPACE ))
	{
		if( FBitSet( ent->curstate.iuser1, CF_STATIC_ENTITY ))
		{
			if( ent->curstate.startpos != g_vecZero )
			{
				flLocalScale = ent->curstate.startpos.Average();
				flLocalScale = 1.0f / flLocalScale, 1.0f;
			}
		}

		// invalidate bonecache
		m_pModelInstance->bonecache.frame = -999.0f;
		// make sure what model is in local space
		ent->curstate.startpos = g_vecZero;
		ent->origin = g_vecZero;
		ent->angles = g_vecZero;
	}
	else
	{
		ALERT( at_warning, "StudioDecal: '%s' not in local space. Ignored!\n", entry->m_DecalName );
		return;
	}

	// we are in decal-shooting mode
	m_fShootDecal = true;
	// setup bones
	StudioSetUpTransform ( );
	StudioSetupBones ( );
	m_fShootDecal = false;

	if( FBitSet( ent->curstate.iuser1, CF_STATIC_ENTITY ) && ( ent->curstate.colormap > 0 ) && world->vertex_lighting != NULL )
	{
		ModelInstance_t *inst = m_pModelInstance;

		// only do it while restore mode and once only
		if( !RENDER_GET_PARM( PARM_CLIENT_ACTIVE, 0 ) && !inst->m_VlCache && !FBitSet( inst->info_flags, MF_VL_BAD_CACHE ))
		{
			// we need to get right pointers from vertex lighted meshes
			CacheVertexLight( RI->currententity );
		}
	}

	// setup worldtransform array
	bool has_boneweights = FBitSet(m_pStudioHeader->flags, STUDIO_HAS_BONEWEIGHTS) != 0;
	if (has_boneweights)
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i].ConcatTransforms( RI->currentmodel->poseToBone->posetobone[i] );
	}
	else
	{
		// no pose to bone just copy the bones
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i];
	}

	if( !ComputePoseToDecal( vecNormal, vecEnd ))
		return;
check_decals:
	// too many decals on model, remove
	while( m_pModelInstance->m_DecalList.Count() > r_studio_decals->value )
	{
		// decals with this depth level are fragments
		// of single decal and should be removed together
		int depth = m_pModelInstance->m_DecalList[0].depth;

		for( int i = m_pModelInstance->m_DecalList.Count(); --i >= 0; )
		{
			studiodecal_t *pDecal = &m_pModelInstance->m_DecalList[i];

			if( pDecal->depth == depth )
			{
				m_pModelInstance->m_DecalList.Remove( i );
				goto check_decals;
			}
		}
	}

	// local scale it's used to keep decal constant size while model is scaled
	float xsize = buildInfo.m_pTexInfo->xsize * flLocalScale;
	float ysize = buildInfo.m_pTexInfo->ysize * flLocalScale;

	SetBits( flags, FDECAL_NORANDOM );
	buildInfo.m_Radius = sqrt( xsize * xsize + ysize * ysize ) * 0.5f;
	buildInfo.vecDecalScale = Vector2D( 1.0f / ysize, 1.0f / xsize );
	buildInfo.m_UseClipVert = ( m_pStudioHeader->numbones <= 1 ); // produce clipped verts only for static props
	buildInfo.decalDepth = m_pModelInstance->m_DecalCount++;
	buildInfo.decalFlags = (byte)flags;
	buildInfo.vecLocalNormal = vecNormal;
	buildInfo.vecLocalEnd = vecEnd;
	buildInfo.modelState = state;
	buildInfo.modelLight = NULL;
	buildInfo.current = NULL;

	// special check for per-vertex lighting
	if( FBitSet( ent->curstate.iuser1, CF_STATIC_ENTITY ) && ( ent->curstate.colormap > 0 ) && world->vertex_lighting != NULL )
	{
		int cacheID = RI->currententity->curstate.colormap - 1;
		dvlightlump_t *dvl = world->vertex_lighting;

		if( cacheID < dvl->nummodels && dvl->dataofs[cacheID] != -1 )
			buildInfo.modelLight = (dmodelvertlight_t *)((byte *)world->vertex_lighting + dvl->dataofs[cacheID]);
	}

	// step over all body parts + add decals to them all!
	for( int k = 0; k < m_pStudioHeader->numbodyparts; k++ ) 
	{
		// grab the model for this body part
		StudioSetupModel( k, &m_pSubModel, &m_pVboModel );
		AddDecalToModel( buildInfo );
	}
}

/*
=============================================================

	DECALS RENDERING

=============================================================
*/
void CStudioModelRenderer :: SetDecalUniforms( studiodecal_t *pDecal )
{
	cl_entity_t *e = RI->currententity;
	ModelInstance_t *inst = m_pModelInstance = &m_ModelInstances[e->modelhandle];
	bool vertex_lighting = FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ) ? true : false;

	// rebuild shader if changed
	ShaderDecalForward( pDecal, vertex_lighting );

	GL_BindShader( pDecal->forwardScene.GetShader());			

	glsl_program_t *shader = RI->currentshader;
	int num_bones = Q_min( m_pStudioHeader->numbones, glConfig.max_skinning_bones );
	mstudiomaterial_t *pmaterial = (mstudiomaterial_t *)RI->currentmodel->materials;
	vbomesh_t *pMesh = pDecal->modelmesh;
	Vector4D	lightstyles, lightdir;
	int width, height;

	int m_skinnum = bound( 0, e->curstate.skin, m_pStudioHeader->numskinfamilies - 1 );
	short *pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pStudioHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pStudioHeader->numskinref);
	pmaterial = &pmaterial[pskinref[pMesh->skinref]];
	CDynLight *pl = RI->currentlight;	// may be NULL
	mstudiolight_t *light = &inst->light;
	int map;

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			u->SetValue( pmaterial->gl_diffuse_id.ToInt() );
			break;
		case UT_DECALMAP:
			u->SetValue( pDecal->texinfo->gl_diffuse_id.ToInt() );
			break;
		case UT_NORMALMAP:
			u->SetValue( pDecal->texinfo->gl_normalmap_id.ToInt() );
			break;
		case UT_GLOSSMAP:
			u->SetValue( pDecal->texinfo->gl_specular_id.ToInt() );
			break;
		case UT_HEIGHTMAP:
			u->SetValue( pDecal->texinfo->gl_heightmap_id.ToInt() );
			break;
		case UT_PROJECTMAP:
			if( pl && pl->type == LIGHT_SPOT )
				u->SetValue( pl->spotlightTexture.ToInt() );
			else u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SHADOWMAP:
		case UT_SHADOWMAP0:
			if( pl ) u->SetValue( pl->shadowTexture[0].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP1:
			if( pl ) u->SetValue( pl->shadowTexture[1].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP2:
			if( pl ) u->SetValue( pl->shadowTexture[2].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_SHADOWMAP3:
			if( pl ) u->SetValue( pl->shadowTexture[3].ToInt() );
			else u->SetValue( tr.depthTexture.ToInt() );
			break;
		case UT_LIGHTMAP:
		case UT_DELUXEMAP:
			// unacceptable for studiomodels
			u->SetValue( tr.whiteTexture.ToInt() );
			break;
		case UT_SCREENMAP:
			u->SetValue( tr.screen_color.ToInt() );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.screen_depth.ToInt() );
			break;
		case UT_ENVMAP0:
		case UT_ENVMAP:
			if (!RP_CUBEPASS() && inst->cubemap[0] != NULL) {
				u->SetValue(inst->cubemap[0]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_ENVMAP1:
			if (!RP_CUBEPASS() && inst->cubemap[1] != NULL) {
				u->SetValue(inst->cubemap[1]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL0:
			if (!RP_CUBEPASS() && inst->cubemap[0] != NULL) {
				u->SetValue(inst->cubemap[0]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL1:
			if (!RP_CUBEPASS() && inst->cubemap[1] != NULL) {
				u->SetValue(inst->cubemap[1]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_BRDFAPPROXMAP:
			u->SetValue(tr.brdfApproxTexture.ToInt());
			break;
		case UT_RENDERALPHA:
			if( e->curstate.rendermode == kRenderTransTexture || e->curstate.rendermode == kRenderTransAdd )
				u->SetValue( e->curstate.renderamt / 255.0f );
			else u->SetValue( 1.0f );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		case UT_BONESARRAY:
			u->SetValue( &inst->m_glstudiobones[0][0], num_bones * 3 );
			break;
		case UT_BONEQUATERNION:
			u->SetValue( &inst->m_studioquat[0][0], num_bones );
			break;
		case UT_BONEPOSITION:
			u->SetValue( &inst->m_studiopos[0][0], num_bones );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_ZFAR:
			u->SetValue( -tr.farclip * 1.74f );
			break;
		case UT_LIGHTSTYLES:
			for( map = 0; map < MAXLIGHTMAPS; map++ )
			{
				if( inst->styles[map] != 255 )
					lightstyles[map] = tr.lightstyle[inst->styles[map]];
				else lightstyles[map] = 0.0f;
			}
			u->SetValue( lightstyles.x, lightstyles.y, lightstyles.z, lightstyles.w );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( GetVieworg().x, GetVieworg().y, GetVieworg().z );
			break;
		case UT_LIGHTGAMMA:
			u->SetValue(tr.light_gamma);
			break;
		case UT_LIGHTDIR:
			if( pl )
			{
				if( pl->type == LIGHT_DIRECTIONAL ) lightdir = -tr.sky_normal;
				else lightdir = pl->frustum.GetPlane( FRUSTUM_FAR )->normal;
				u->SetValue( lightdir.x, lightdir.y, lightdir.z, pl->fov );
			}
			else u->SetValue( light->normal.x, light->normal.y, light->normal.z );
			break;
		case UT_LIGHTDIFFUSE:
			if( pl ) u->SetValue( pl->color.x, pl->color.y, pl->color.z );
			else u->SetValue( light->diffuse.x, light->diffuse.y, light->diffuse.z );
			break;
		case UT_LIGHTSHADE:
			u->SetValue( (float)light->ambientlight / 255.0f, (float)light->shadelight / 255.0f );
			break;
		case UT_LIGHTORIGIN:
			if( pl ) u->SetValue( pl->origin.x, pl->origin.y, pl->origin.z, ( 1.0f / pl->radius )); 
			break;
		case UT_LIGHTVIEWPROJMATRIX:
			if( pl )
			{
				GLfloat gl_lightViewProjMatrix[16];
				pl->lightviewProjMatrix.CopyToArray( gl_lightViewProjMatrix );
				u->SetValue( &gl_lightViewProjMatrix[0] );
			}
			break;
		case UT_AMBIENTFACTOR:
			if( pl && pl->type == LIGHT_DIRECTIONAL )
				u->SetValue( tr.sun_ambient );
			else u->SetValue( tr.ambientFactor );
			break;
		case UT_SUNREFRACT:
			u->SetValue( tr.sun_refract );
			break;
		case UT_AMBIENTCUBE:
			u->SetValue( &light->ambient[0][0], 6 );
			break;
		case UT_LERPFACTOR:
			u->SetValue( inst->lerpFactor );
			break;
		case UT_SMOOTHNESS:
			u->SetValue( pDecal->texinfo->matdesc->smoothness );
			break;
		case UT_RELIEFPARAMS:
			width = pDecal->texinfo->gl_heightmap_id.GetWidth();
			height = pDecal->texinfo->gl_heightmap_id.GetHeight();
			u->SetValue( (float)width, (float)height, pDecal->texinfo->matdesc->reliefScale, cv_shadow_offset->value );
			break;
		default:
			ALERT( at_error, "Unhandled uniform %s\n", u->name );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Draws all the decals on a particular model
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: DrawDecal( CSolidEntry *entry, GLenum cull_mode )
{
	if( !StudioSetEntity( entry ))
		return;

	if( m_pModelInstance->visframe != tr.realframecount )
		return; // model is culled

	if( !m_pModelInstance->m_DecalList.Count())
		return; // no decals for this model

	if( CVAR_TO_BOOL( r_polyoffset ))
	{
		pglEnable( GL_POLYGON_OFFSET_FILL );
		pglPolygonOffset( -1.0f, -r_polyoffset->value );
	}

	GL_DEBUG_SCOPE();
	GL_Blend( GL_TRUE );
	GL_DepthMask( GL_FALSE );
	GL_AlphaTest( GL_FALSE );
	GL_Cull( cull_mode );

	// Draw decals in history order or reverse order
	if( cull_mode == GL_FRONT )
	{
		// direct order
		for( int i = 0; i < m_pModelInstance->m_DecalList.Count(); i++ )
		{
			studiodecal_t *pDecal = &m_pModelInstance->m_DecalList[i];

			if( entry->m_pMesh->uniqueID != pDecal->modelmesh->uniqueID )
				continue;

			SetDecalUniforms( pDecal );
			if( pDecal->texinfo->opaque )
				pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			else pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			DrawMeshFromBuffer( &pDecal->mesh );
		}
	}
	else
	{
		// reverse order
		for( int i = m_pModelInstance->m_DecalList.Count(); --i >= 0; )
		{
			studiodecal_t *pDecal = &m_pModelInstance->m_DecalList[i];

			if( entry->m_pMesh->uniqueID != pDecal->modelmesh->uniqueID )
				continue;

			SetDecalUniforms( pDecal );
			if( pDecal->texinfo->opaque )
				pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			else pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			DrawMeshFromBuffer( &pDecal->mesh );
		}
	}

	if( CVAR_TO_BOOL( r_polyoffset ))
		pglDisable( GL_POLYGON_OFFSET_FILL );
	GL_CleanupAllTextureUnits();
	GL_DepthMask( GL_TRUE );
	GL_Blend( GL_FALSE );
	GL_Cull( GL_FRONT );
}
