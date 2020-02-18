/*
r_studiodecal.cpp - project and rendering studio decals
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
#include "com_model.h"
#include "r_studioint.h"
#include "pm_movevars.h"
#include "r_studio.h"
#include "r_sprite.h"
#include "event_api.h"
#include <mathlib.h>
#include "pm_defs.h"
#include "triangleapi.h"
#include "entity_types.h"
#include "r_shader.h"
#include "r_world.h"

//-----------------------------------------------------------------------------
// Create, destroy list of decals for a particular model
//-----------------------------------------------------------------------------
word CStudioModelRenderer :: CreateDecalList( void )
{
	word handle = m_DecalList.AddToTail();

	m_DecalList[handle].m_FirstMaterial = m_DecalMaterial.InvalidIndex();

	return handle;
}

void CStudioModelRenderer :: DestroyDecalList( word handle )
{
	if( handle == INVALID_HANDLE )
		return;

	// blat out all geometry associated with all materials
	word mat = m_DecalList[handle].m_FirstMaterial;

	while( mat != m_DecalMaterial.InvalidIndex( ))
	{
		word next = m_DecalMaterial.Next( mat );
		m_DecalMaterial.Remove( mat );
		mat = next;
	}

	m_DecalList.Remove( handle );
}

void CStudioModelRenderer :: StudioClearDecals( void )
{
	// NOTE: should destroy in reverse-oreder because it's linked list not array!
	for( int i = m_ModelInstances.Count(); --i >= 0; )
          {
		ModelInstance_t& inst = m_ModelInstances[i];

		if( inst.m_DecalHandle == INVALID_HANDLE )
			continue;	// not initialized?

		DestroyDecalList( inst.m_DecalHandle );
		inst.m_DecalHandle = INVALID_HANDLE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes all the decals on a model instance
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: RemoveAllDecals( int entityIndex )
{
	if( !g_fRenderInitialized ) return;

	cl_entity_t *ent = gEngfuncs.GetEntityByIndex( entityIndex );

	if( !ent || ent->modelhandle == INVALID_HANDLE )
		return;

	ModelInstance_t *inst = &m_ModelInstances[ent->modelhandle];

	if( !IsModelInstanceValid( inst ))
		return;	// no instance data

	if( inst->m_DecalHandle == INVALID_HANDLE )
		return;	// no decals data

	DestroyDecalList( inst->m_DecalHandle );
	inst->m_DecalHandle = INVALID_HANDLE;
}

//-----------------------------------------------------------------------------
// Build the list of decals by save\restore system request
//-----------------------------------------------------------------------------
int CStudioModelRenderer :: StudioDecalList( decallist_t *pBaseList, int count )
{
	if( !g_fRenderInitialized )
		return 0;

	int maxStudioDecals = MAX_STUDIO_DECALS + (MAX_BMODEL_DECALS - count);
	decallist_t *pList = pBaseList + count;	// shift list to first free slot
	cl_entity_t *pEntity = NULL;
	char decalname[64];
	int total = 0;

	for( int i = 0; i < m_ModelInstances.Count(); i++ )
          {
		word decalHandle = m_ModelInstances[i].m_DecalHandle;

		if( decalHandle == INVALID_HANDLE )
			continue;	// no decals for this entity

		DecalModelList_t const& list = m_DecalList[decalHandle];
		word decalMaterial = m_DecalList[decalHandle].m_FirstMaterial;

		// setup the decal entity
		pEntity = m_ModelInstances[i].m_pEntity;

		for( word mat = list.m_FirstMaterial; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next( mat ))
		{
			DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];
			word decal = decalMaterial.m_Decals.Head();

			// setup the decal texture
			Q_strncpy( decalname, GET_TEXTURE_NAME( decalMaterial.decalTexture ), sizeof( decalname ));

			while( decal != decalMaterial.m_Decals.InvalidIndex( ))
			{
				Decal_t *pdecal = &decalMaterial.m_Decals[decal];

				if( FBitSet( pdecal->flags, FDECAL_DONTSAVE ))
				{	
					decal = decalMaterial.m_Decals.Next( decal ); 
					continue;
				}

				pList[total].depth = pdecal->depth;
				pList[total].flags = pdecal->flags|FDECAL_STUDIO;
				pList[total].entityIndex = pEntity->index;
				pList[total].studio_state = pdecal->state;
				pList[total].position = pdecal->vecLocalEnd;
				pList[total].impactPlaneNormal = pdecal->vecLocalStart;
				COM_FileBase( decalname, pList[total].name );
				total++;

				// check for list overflow
				if( total >= maxStudioDecals )
				{
					ALERT( at_error, "StudioDecalList: too many studio decals on save\restore\n" );
					goto end_serialize;
				}

				decal = decalMaterial.m_Decals.Next( decal ); 
			}
		}
	}
end_serialize:

	return total;
}

//-----------------------------------------------------------------------------
// Computes the pose to decal plane transform 
//-----------------------------------------------------------------------------
bool CStudioModelRenderer :: ComputePoseToDecal( const Vector &vecStart, const Vector &vecEnd )
{
	// Create a transform that projects world coordinates into a basis for the decal
	Vector decalU, decalV, decalN, vecDelta;
	matrix3x4	worldToDecal;

	// Bloat a little bit so we get the intersection
	vecDelta = (vecEnd - vecStart) * 1.1f;

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

inline bool CStudioModelRenderer :: IsFrontFacing( const Vector &norm, mstudioboneweight_t *pboneweight )
{
	float z = 0.0f;
	Vector decalN;

	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to decal
	if( pboneweight->weight[0] == 255 )
	{
		decalN.x = m_pdecaltransform[pboneweight->bone[0]][0][2];
		decalN.y = m_pdecaltransform[pboneweight->bone[0]][1][2];
		decalN.z = m_pdecaltransform[pboneweight->bone[0]][2][2];

		z = DotProduct( norm, decalN );
	}
	else
	{
		for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
		{
			if( pboneweight->bone[i] == -1 )
				break;
			decalN.x = m_pdecaltransform[pboneweight->bone[i]][0][2];
			decalN.y = m_pdecaltransform[pboneweight->bone[i]][1][2];
			decalN.z = m_pdecaltransform[pboneweight->bone[i]][2][2];
			z += DotProduct( norm, decalN ) * (pboneweight->weight[i] / 255.0f);
		}
	}

	return ( z >= 0.1f );
}

matrix3x3 CStudioModelRenderer :: GetDecalRotateTransform( byte vertexBone )
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

bool CStudioModelRenderer :: TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, mstudioboneweight_t *pboneweight, Vector2D& uv )
{
	// NOTE: This only works to rotate normals if there's no scale in the
	// pose to world transforms. If we ever add scale, we'll need to
	// multiply by the inverse transpose of the pose to world
	matrix3x3 decalMat;
	float z;

	if( pboneweight->weight[0] == 255 )
	{
		byte vertexBone = pboneweight->bone[0];

		decalMat = GetDecalRotateTransform( vertexBone );
		uv.x = (DotProduct( pos, decalMat[0] ) + m_pdecaltransform[vertexBone][3][0]);
		uv.y = -(DotProduct( pos, decalMat[1] ) + m_pdecaltransform[vertexBone][3][1]);
		z = DotProduct( pos, decalMat[2] ) + m_pdecaltransform[vertexBone][3][2];
	}
	else
	{
		float	weight;

		uv.x = uv.y = z = 0.0f;

		for( int i = 0; i < MAXSTUDIOBONEWEIGHTS; i++ )
		{
			byte vertexBone = pboneweight->bone[i];

			if( vertexBone == -1 )
				break;

			decalMat = GetDecalRotateTransform( vertexBone );
			weight = (pboneweight->weight[i] / 255.0f);

			uv.x += (DotProduct( pos, decalMat[0] ) + m_pdecaltransform[vertexBone][3][0]) * weight;
			uv.y += -(DotProduct( pos, decalMat[1] ) + m_pdecaltransform[vertexBone][3][1]) * weight;
			z += (DotProduct( pos, decalMat[2] ) + m_pdecaltransform[vertexBone][3][2]) * weight;
		}
	}

	return ( fabs( z ) < build.m_Radius );
}

void CStudioModelRenderer :: ProjectDecalOntoMesh( DecalBuildInfo_t& build )
{
	float invRadius = (build.m_Radius != 0.0f) ? 1.0f / build.m_Radius : 1.0f;

	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// For this to work, the plane and intercept must have been transformed
	// into pose space. Also, we'll not be bothering with flexes.
	for( int j = 0; j < build.m_pDecalMesh->numvertices; j++ )
	{
		DecalVertex_t *vert = &m_decalverts[m_arrayelems[build.m_pDecalMesh->firstvertex + j]];

		// No decal vertex yet...
		pVertexInfo[j].m_VertexIndex = 0xFFFF;

		// We need to know if the normal is pointing in the negative direction
		// if so, blow off all triangles connected to that vertex.
		pVertexInfo[j].m_FrontFacing = IsFrontFacing( vert->m_Normal, &vert->m_BoneWeights );

		if( !pVertexInfo[j].m_FrontFacing )
			continue;

		bool inValidArea = TransformToDecalSpace( build, vert->m_Position, &vert->m_BoneWeights, pVertexInfo[j].m_UV );
		pVertexInfo[j].m_InValidArea = inValidArea;

		pVertexInfo[j].m_UV *= invRadius * 0.5f;
		pVertexInfo[j].m_UV[0] += 0.5f;
		pVertexInfo[j].m_UV[1] += 0.5f;
	}
}

int CStudioModelRenderer :: ComputeClipFlags( Vector2D const& uv )
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

//-----------------------------------------------------------------------------
// Converts a mesh index to a DecalVertex_t
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int meshIndex, DecalVertex_t& decalVertex )
{
	// Copy over the data;
	decalVertex = m_decalverts[m_arrayelems[build.m_pDecalMesh->firstvertex + meshIndex]];

	// get the texture coords from the decal planar projection
	decalVertex.m_TexCoord0 = build.m_pVertexInfo[meshIndex].m_UV;

	// if meshIndex is valid this vertex was created from original
	decalVertex.m_MeshVertexIndex = meshIndex;
}

//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices for this material
//-----------------------------------------------------------------------------
word CStudioModelRenderer :: AddVertexToDecal( DecalBuildInfo_t& build, int meshIndex )
{
	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// If we've never seen this vertex before, we need to add a new decal vert
	if( pVertexInfo[meshIndex].m_VertexIndex == 0xFFFF )
	{
		DecalVertexList_t& decalVertexList = build.m_pDecalMaterial->m_Vertices;
		int v = decalVertexList.AddToTail();

		// Copy over the data;
		ConvertMeshVertexToDecalVertex( build, meshIndex, build.m_pDecalMaterial->m_Vertices[v] );

#ifdef _DEBUG
		// Make sure clipped vertices are in the right range...
		if( build.m_UseClipVert )
		{
			ASSERT(( decalVertexList[v].m_TexCoord0[0] >= -1e-3 ) && ( decalVertexList[v].m_TexCoord0[0] - 1.0f < 1e-3 ));
			ASSERT(( decalVertexList[v].m_TexCoord0[1] >= -1e-3 ) && ( decalVertexList[v].m_TexCoord0[1] - 1.0f < 1e-3 ));
		}
#endif

		// Store off the index of this vertex so we can reference it again
		pVertexInfo[meshIndex].m_VertexIndex = build.m_VertexCount;
		++build.m_VertexCount;
		if( build.m_FirstVertex == decalVertexList.InvalidIndex( ))
			build.m_FirstVertex = v;
	}

	return pVertexInfo[meshIndex].m_VertexIndex;
}

//-----------------------------------------------------------------------------
// Adds a vertex to the list of vertices for this material
//-----------------------------------------------------------------------------
word CStudioModelRenderer :: AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert )
{
	// This creates a unique vertex
	DecalVertexList_t& decalVertexList = build.m_pDecalMaterial->m_Vertices;

	// Try to see if the clipped vertex already exists in our decal list...
	// Only search for matches with verts appearing in the current decal
	word vertexCount = 0;

	for( word i = build.m_FirstVertex; i != decalVertexList.InvalidIndex(); i = decalVertexList.Next(i), ++vertexCount )
	{
		// Only bother to check against clipped vertices
		if( decalVertexList[i].m_MeshVertexIndex != INVALID_HANDLE )
			continue;

		// They must have the same position, and normal
		// texcoord will fall right out if the positions match
		Vector temp = decalVertexList[i].m_Position - vert.m_Position;
		if(( fabs( temp[0] ) > 1e-3 ) || (fabs( temp[1] ) > 1e-3 ) || ( fabs( temp[2] ) > 1e-3 ))
			continue;

		temp = decalVertexList[i].m_Normal - vert.m_Normal;
		if(( fabs( temp[0] ) > 1e-3 ) || ( fabs( temp[1] ) > 1e-3 ) || ( fabs( temp[2] ) > 1e-3 ))
			continue;

		return vertexCount;
	}

	// This path is the path taken by clipped vertices
	ASSERT( (vert.m_TexCoord0[0] >= -1e-3) && (vert.m_TexCoord0[0] - 1.0f < 1e-3) );
	ASSERT( (vert.m_TexCoord0[1] >= -1e-3) && (vert.m_TexCoord0[1] - 1.0f < 1e-3) );

	// Must create a new vertex...
	word idx = decalVertexList.AddToTail( vert );
	if( build.m_FirstVertex == decalVertexList.InvalidIndex( ))
		build.m_FirstVertex = idx;

	ASSERT( vertexCount == build.m_VertexCount );

	return build.m_VertexCount++;
}

//-----------------------------------------------------------------------------
// Adds the clipped triangle to the decal
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState )
{
	ASSERT( clipState.m_VertCount <= 7 );

	// Yeah baby yeah!!	Add this sucka
	int i;
	word indices[7];
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
			indices[i] = AddVertexToDecal( build, clipState.m_ClipVerts[vertIdx] );
		}
	}

	// Add a trifan worth of triangles
	for( i = 1; i < clipState.m_VertCount - 1; i++ )
	{
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[0] );
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[i] );
		build.m_pDecalMaterial->m_Indices.AddToTail( indices[i+1] );
	}
}

//-----------------------------------------------------------------------------
// Creates a new vertex where the edge intersects the plane
//-----------------------------------------------------------------------------
int CStudioModelRenderer :: IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val )
{
	DecalVertex_t& startVert = state.m_ClipVerts[start];
	DecalVertex_t& endVert = state.m_ClipVerts[end];

	Vector2D dir = endVert.m_TexCoord0 - startVert.m_TexCoord0;

	ASSERT( dir[normalInd] != 0.0f );
	float t = (val - startVert.m_TexCoord0[normalInd]) / dir[normalInd];
				 
	// Allocate a clipped vertex
	DecalVertex_t& out = state.m_ClipVerts[state.m_ClipVertCount];
	int newVert = state.m_ClipVertCount++;

	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// the clipped vertex has no analogue in the original mesh
	out.m_MeshVertexIndex = INVALID_HANDLE;

	// just select bone by interpolation factor
	if( t <= 0.5f ) out.m_BoneWeights = startVert.m_BoneWeights;
	else out.m_BoneWeights = endVert.m_BoneWeights;

	// Interpolate position
	out.m_Position = startVert.m_Position * (1.0f - t) + endVert.m_Position * t;

	// Interpolate normal
	out.m_Normal = startVert.m_Normal * (1.0f - t) + endVert.m_Normal * t;
	out.m_Normal = out.m_Normal.Normalize();

	// Interpolate texture coord
	out.m_TexCoord0 = startVert.m_TexCoord0 + (endVert.m_TexCoord0 - startVert.m_TexCoord0) * t;
	out.m_TexCoord1 = startVert.m_TexCoord1 + (endVert.m_TexCoord1 - startVert.m_TexCoord1) * t;

	// Interpolate lighting
	for( int i = 0; i < LM_STYLES; i++ )
	{
		for( int j = 0; j < 3; j++ )
			out.m_Light[i][j] = startVert.m_Light[i][j] + (endVert.m_Light[i][j] - startVert.m_Light[i][j]) * t;
	}

	// Compute the clip flags baby...
	state.m_ClipFlags[newVert] = ComputeClipFlags( out.m_TexCoord0 );

	return newVert;
}

//-----------------------------------------------------------------------------
// Clips a triangle against a plane, use clip flags to speed it up
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val )
{
	// Ye Olde Sutherland-Hodgman clipping algorithm
	int outVertCount = 0;
	int start = state.m_Indices[state.m_Pass][state.m_VertCount - 1];
	bool startInside = (state.m_ClipFlags[start] & flag) == 0;
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

//-----------------------------------------------------------------------------
// Clips the triangle to +/- radius
//-----------------------------------------------------------------------------
bool CStudioModelRenderer :: ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags )
{
	DecalClipState_t clipState;
	clipState.m_VertCount = 3;

	ConvertMeshVertexToDecalVertex( build, i1, clipState.m_ClipVerts[0] );
	ConvertMeshVertexToDecalVertex( build, i2, clipState.m_ClipVerts[1] );
	ConvertMeshVertexToDecalVertex( build, i3, clipState.m_ClipVerts[2] );

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

//-----------------------------------------------------------------------------
// Adds a decal to a triangle, but only if it should
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 )
{
	DecalVertexInfo_t* pVertexInfo = build.m_pVertexInfo;

	// All must be front-facing for a decal to be added
	if(( !pVertexInfo[i1].m_FrontFacing ) || ( !pVertexInfo[i2].m_FrontFacing ) || ( !pVertexInfo[i3].m_FrontFacing ))
	{
		return;
	}

	// This is used to prevent poke through; if the points are too far away
	// from the contact point, then don't add the decal
	if(( !pVertexInfo[i1].m_InValidArea ) && ( !pVertexInfo[i2].m_InValidArea ) && ( !pVertexInfo[i3].m_InValidArea ))
	{
		return;
	}

	// Clip to +/- radius
	int clipFlags[3];

	clipFlags[0] = ComputeClipFlags( pVertexInfo[i1].m_UV );
	clipFlags[1] = ComputeClipFlags( pVertexInfo[i2].m_UV );
	clipFlags[2] = ComputeClipFlags( pVertexInfo[i3].m_UV );

	// Cull... The result is non-zero if they're all outside the same plane
	if(( clipFlags[0] & ( clipFlags[1] & clipFlags[2] )) != 0 )
		return;

	bool doClip = true;
	
	// Trivial accept for skinned polys... if even one vert is inside
	// the draw region, accept
	if(( !build.m_UseClipVert ) && ( !clipFlags[0] || !clipFlags[1] || !clipFlags[2] ))
		doClip = false;

	// Trivial accept... no clip flags set means all in
	// Don't clip if we have more than one bone... we'll need to do skinning
	// and we can't clip the bone indices
	// We *do* want to clip in the one bone case though; useful for large
	// static props.
	if( doClip && ( clipFlags[0] || clipFlags[1] || clipFlags[2] ))
	{
		bool validTri = ClipDecal( build, i1, i2, i3, clipFlags );

		// Don't add the triangle if we culled the triangle or if 
		// we had one or less bones
		if( build.m_UseClipVert || ( !validTri ))
			return;
	}

	// Add the vertices to the decal since there was no clipping
	build.m_pDecalMaterial->m_Indices.AddToTail( AddVertexToDecal( build, i1 ));
	build.m_pDecalMaterial->m_Indices.AddToTail( AddVertexToDecal( build, i2 ));
	build.m_pDecalMaterial->m_Indices.AddToTail( AddVertexToDecal( build, i3 ));
}

void CStudioModelRenderer :: AddDecalToMesh( DecalBuildInfo_t& build )
{
	build.m_pVertexInfo = (DecalVertexInfo_t *)stackalloc( build.m_pDecalMesh->numvertices * sizeof( DecalVertexInfo_t ));

	// project all vertices for this group into decal space
	// Note we do this work at a mesh level instead of a model level
	// because vertices are not shared across mesh boundaries
	ProjectDecalOntoMesh( build );

	for( int j = 0; j < build.m_pDecalMesh->numvertices; j += 3 )
	{
		AddTriangleToDecal( build, j + 0, j + 1, j + 2 );
	}
}

void CStudioModelRenderer :: AddDecalToModel( DecalBuildInfo_t& buildInfo )
{
	Vector *pstudioverts = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	Vector *pstudionorms = (Vector *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	byte *pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	DecalModelList_t& list = m_DecalList[m_pModelInstance->m_DecalHandle];
	mstudioboneweight_t	defaultBoneWeight;
	dvertlight_t	*vl = NULL;
	short		*pskinref;
	int 		i, numVerts;

	// if weights was missed their offsets just equal to 0
	mstudioboneweight_t	*pvertweight = (mstudioboneweight_t *)((byte *)m_pStudioHeader + m_pSubModel->blendvertinfoindex);
	DecalMesh_t *pDecalMesh = (DecalMesh_t *)stackalloc( m_pSubModel->nummesh * sizeof( DecalMesh_t ));
	pskinref = (short *)((byte *)m_pStudioHeader + m_pStudioHeader->skinindex);
	if( !pDecalMesh ) return; // empty mesh?

	// initialize default boneweight
	defaultBoneWeight.bone[0] = 0;
	defaultBoneWeight.bone[1] = -1;
	defaultBoneWeight.bone[2] = -1;
	defaultBoneWeight.bone[3] = -1;
	defaultBoneWeight.weight[0] = 255;
	defaultBoneWeight.weight[1] = 0;
	defaultBoneWeight.weight[2] = 0;
	defaultBoneWeight.weight[3] = 0;

	if( buildInfo.modelLight != NULL )
	{
		int	offset = (byte *)m_pSubModel - (byte *)m_pStudioHeader; // search for submodel offset

		for( i = 0; i < MAXSTUDIOMODELS; i++ )
		{
			if( buildInfo.modelLight->submodels[i].submodel_offset == offset )
				break;			
		}

		// has vertexlighting for this submodel
		if( i != MAXSTUDIOMODELS )
			vl = &buildInfo.modelLight->verts[buildInfo.modelLight->submodels[i].vertex_offset];
	}

	m_nNumArrayVerts = m_nNumArrayElems = 0;

	// build all the data for current submodel
	for( i = 0; i < m_pSubModel->nummesh; i++ ) 
	{
		mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + i;
		short *ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
		DecalMesh_t *pCurMesh = pDecalMesh + i;

		pCurMesh->pmaterial = &m_pRenderModel->materials[pskinref[pmesh->skinref]];
		pCurMesh->firstvertex = m_nNumArrayElems;
		pCurMesh->numvertices = m_nNumArrayElems;

		float s = 1.0f / (float)pCurMesh->pmaterial->pSource->width;
		float t = 1.0f / (float)pCurMesh->pmaterial->pSource->height;

		while( ( numVerts = *( ptricmds++ ) ))
		{
			int	vertexState = 0;
			qboolean	tri_strip = true;

			if( numVerts < 0 )
			{
				tri_strip = false;
				numVerts = -numVerts;
			}

			for( ; numVerts > 0; numVerts--, ptricmds += 4 )
			{
				// build in indices
				if( vertexState++ < 3 )
				{
					m_arrayelems[m_nNumArrayElems++] = m_nNumArrayVerts;
				}
				else if( tri_strip )
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

				DecalVertex_t *out = &m_decalverts[m_nNumArrayVerts];

				out->m_Position = pstudioverts[ptricmds[0]];
				out->m_Normal = pstudionorms[ptricmds[1]];

				if( m_pRenderModel->poseToBone != NULL )
				{
					out->m_BoneWeights = pvertweight[ptricmds[0]];
				}
				else
				{
					out->m_BoneWeights = defaultBoneWeight;
					out->m_BoneWeights.bone[0] = pvertbone[ptricmds[0]];
				}

				if( pCurMesh->pmaterial->pSource->flags & STUDIO_NF_CHROME )
				{
					// probably always equal 64 (see studiomdl.c for details)
					out->m_TexCoord1[0] = s;
					out->m_TexCoord1[1] = t;
				}
				else if( pCurMesh->pmaterial->pSource->flags & STUDIO_NF_UV_COORDS )
				{
					out->m_TexCoord1[0] = HalfToFloat( ptricmds[2] );
					out->m_TexCoord1[1] = HalfToFloat( ptricmds[3] );
				}
				else
				{
					out->m_TexCoord1[0] = ptricmds[2] * s;
					out->m_TexCoord1[1] = ptricmds[3] * t;
				}

				// accumulate vertex lighting too
				if( vl != NULL )
					memcpy( &out->m_Light, vl[m_nNumArrayVerts].light, LM_STYLES * 3 );
				m_nNumArrayVerts++;
			}
		}
		pCurMesh->numvertices = m_nNumArrayElems - pCurMesh->firstvertex;
	}

	for( i = 0; i < m_pSubModel->nummesh; i++ )
	{
		buildInfo.m_Mesh = i;
		buildInfo.m_pDecalMesh = pDecalMesh + i;
		buildInfo.m_pMesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + i;
		int materialIdx = GetDecalMaterial( list, buildInfo.m_iDecalTexture, buildInfo.m_pDecalMesh->pmaterial );

		// if material was changed or not init
		if(( buildInfo.m_iDecalMaterial == -1 ) || ( buildInfo.m_iDecalMaterial != materialIdx ))
		{
			// time to dump accumulated vertices from previous material
			CreateDecalForMaterial( buildInfo );

			materialIdx = GetDecalMaterial( list, buildInfo.m_iDecalTexture, buildInfo.m_pDecalMesh->pmaterial, true );
			buildInfo.m_pDecalMaterial = &m_DecalMaterial[materialIdx];

			// Check to see if we should retire the decal
			while( ShouldRetireDecal( buildInfo.m_pDecalMaterial, list.m_DecalHistory ))
				RetireDecal( list.m_DecalHistory );

			buildInfo.m_FirstVertex = buildInfo.m_pDecalMaterial->m_Vertices.InvalidIndex();
			buildInfo.m_PrevIndexCount = buildInfo.m_pDecalMaterial->m_Indices.Count();
			buildInfo.m_iDecalMaterial = materialIdx;
			buildInfo.m_VertexCount = 0;
		}

		AddDecalToMesh( buildInfo );
	}
}

void CStudioModelRenderer :: CreateDecalForMaterial( DecalBuildInfo_t& buildInfo )
{
	// add this to the list of decals in this material
	if( buildInfo.m_VertexCount && buildInfo.m_iDecalMaterial != -1 && buildInfo.m_pDecalMaterial != NULL )
	{
		int decalIndexCount = buildInfo.m_pDecalMaterial->m_Indices.Count() - buildInfo.m_PrevIndexCount;
		DecalModelList_t& list = m_DecalList[m_pModelInstance->m_DecalHandle];
		ASSERT( decalIndexCount > 0 );

		int decalIndex = AddDecalToMaterialList( buildInfo.m_pDecalMaterial );
		Decal_t& decal = buildInfo.m_pDecalMaterial->m_Decals[decalIndex];
		decal.m_VertexCount = buildInfo.m_VertexCount;
		decal.m_IndexCount = decalIndexCount;

		decal.state = *buildInfo.modelState;
		decal.depth = buildInfo.decalDepth;
		decal.vecLocalStart = buildInfo.vecLocalStart;
		decal.vecLocalEnd = buildInfo.vecLocalEnd;
		decal.flags = buildInfo.decalFlags;

		// Add this decal to the history...
		int h = list.m_DecalHistory.AddToTail();
		list.m_DecalHistory[h].m_Material = buildInfo.m_iDecalMaterial;
		list.m_DecalHistory[h].m_Decal = decalIndex;
	}
}

//-----------------------------------------------------------------------------
// Gets the list of triangles for a particular material and lod
//-----------------------------------------------------------------------------
int CStudioModelRenderer :: GetDecalMaterial( DecalModelList_t& decalList, int decalTexture, mstudiomaterial_t *mat, bool create )
{
	// first we need to determine model texture, because we only require textures for alpha and solid modes.
	// otherwise we can use stub like '*whiteTexture' to prevent produce multiple materials
	int modelTexture = tr.whiteTexture;
	word j;

	if( FBitSet( mat->flags, STUDIO_NF_MASKED|STUDIO_NF_HAS_ALPHA ))
		modelTexture = mat->gl_diffuse_id;

	word hProgram = GL_UberShaderForStudioDecal( mat );

	if( !create && !hProgram )
		return 0xFFFF; // create a new material with invalid shader

	for( j = decalList.m_FirstMaterial; j != m_DecalMaterial.InvalidIndex(); j = m_DecalMaterial.Next( j ))
	{
		DecalMaterial_t *pdecal = &m_DecalMaterial[j];

		if( pdecal->decalTexture == decalTexture && pdecal->modelTexture == modelTexture && pdecal->hProgram == hProgram )
		{
			return j;
		}
	}

	if( !create ) return 0xFFFF; // material was not found

	// If we got here, this must be the first time we saw this material
	j = m_DecalMaterial.Alloc( true );
	
	// Link it into the list of data
	if( decalList.m_FirstMaterial != m_DecalMaterial.InvalidIndex( ))
		m_DecalMaterial.LinkBefore( decalList.m_FirstMaterial, j );
	decalList.m_FirstMaterial = j;

	m_DecalMaterial[j].decalTexture = decalTexture;
	m_DecalMaterial[j].modelTexture = modelTexture;
	m_DecalMaterial[j].hProgram = hProgram;
	m_DecalMaterial[j].flags = mat->flags;

	if( !hProgram ) // it's invalid material, disable shader
		m_DecalMaterial[j].flags |= STUDIO_NF_NODRAW;

	return j;
}

//-----------------------------------------------------------------------------
// Removes a decal and associated vertices + indices from the history list
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: RetireDecal( DecalHistoryList_t& historyList )
{
	assert( historyList.Count() );
	DecalHistory_t& decalHistory = historyList[ historyList.Head() ];

	// Find the decal material for the decal to remove
	DecalMaterial_t& material = m_DecalMaterial[decalHistory.m_Material];

	DecalVertexList_t& vertices = material.m_Vertices;
	Decal_t& decalToRemove = material.m_Decals[decalHistory.m_Decal];
	
	// Now clear out the vertices referenced by the indices....
	word next, vert = vertices.Head();
	assert( vertices.Count() >= decalToRemove.m_VertexCount );

	int vertsToRemove = decalToRemove.m_VertexCount;

	while( vertsToRemove > 0 )
	{
		// blat out the vertices
		next = vertices.Next( vert );
		vertices.Remove( vert );
		vertsToRemove--;
		vert = next;
	}

	material.m_Indices.RemoveMultiple( 0, decalToRemove.m_IndexCount );

	// Remove the decal
	material.m_Decals.Remove( decalHistory.m_Decal );

	// Clear the decal out of the history
	historyList.Remove( historyList.Head() );
}

bool CStudioModelRenderer :: ShouldRetireDecal( DecalMaterial_t* pDecalMaterial, DecalHistoryList_t const& decalHistory )
{
	// Check to see if we should retire the decal
	return ( decalHistory.Count() >= 50 ) || ( pDecalMaterial->m_Indices.Count() > 2048 );
}

int CStudioModelRenderer :: AddDecalToMaterialList( DecalMaterial_t* pMaterial )
{
	DecalList_t& decalList = pMaterial->m_Decals;
	return decalList.AddToTail();
}
	
/*
====================
StudioDecalShoot

NOTE: decalTexture is gl texture index
====================
*/
void CStudioModelRenderer :: StudioDecalShoot( const Vector &vecSrc, const Vector &vecEnd, int decalTexture, cl_entity_t *ent, int flags, modelstate_t *state )
{
	Vector	vecLocalSrc;
	Vector	vecLocalEnd;

	if( !g_fRenderInitialized )
		return;

	if( !ent || ent == gEngfuncs.GetViewModel( ))
		return; // no decals for viewmodel

	if( !StudioSetEntity( ent ))
		return;

	if( m_pStudioHeader->numbodyparts == 0 )
		return;

	tr.time = GET_CLIENT_TIME();
	tr.oldtime = GET_CLIENT_OLDTIME();

	// allocate new decallist
	if( m_pModelInstance->m_DecalHandle == INVALID_HANDLE )
		m_pModelInstance->m_DecalHandle = CreateDecalList();

	m_fShootDecal = true;
	m_fDoInterp = false;

	if( FBitSet( flags, FDECAL_LOCAL_SPACE ))
	{
		// make sure what model is in local space
		m_pCurrentEntity->origin = g_vecZero;
		m_pCurrentEntity->angles = g_vecZero;
	}

	// setup bones
	StudioSetUpTransform ( );
	StudioSetupBones ( );

	// setup worldtransform array
	if( m_pRenderModel->poseToBone != NULL )
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i].ConcatTransforms( m_pRenderModel->poseToBone->posetobone[i] );
	}
	else
	{
		// no pose to bone just copy the bones
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i];
	}

	for( int i = 0; i < m_pStudioHeader->numbones; i++ )
		m_pworldtransform[i] = m_pModelInstance->m_protationmatrix.ConcatTransforms( m_pworldtransform[i] );

	if( FBitSet( flags, FDECAL_LOCAL_SPACE ))
	{
		vecLocalSrc = vecSrc;
		vecLocalEnd = vecEnd;
	}
	else
	{
		vecLocalSrc = m_pModelInstance->m_protationmatrix.VectorITransform( vecSrc );
		vecLocalEnd = m_pModelInstance->m_protationmatrix.VectorITransform( vecEnd );
		SetBits( flags, FDECAL_LOCAL_SPACE ); // now it's in local space
	}

	if( !ComputePoseToDecal( vecSrc, vecEnd ))
	{
		m_fShootDecal = false;
		m_fDoInterp = true;
		return;
	}

	float w = (float)RENDER_GET_PARM( PARM_TEX_SRC_WIDTH, decalTexture ) * 0.5f;
	float h = (float)RENDER_GET_PARM( PARM_TEX_SRC_HEIGHT, decalTexture ) * 0.5f;
 	float radius = Q_max( w, h ) * 0.5f; // scale decals so the look same as on world geometry

	// This sucker is state needed only when building decals
	DecalBuildInfo_t buildInfo;
	memset( &buildInfo, 0, sizeof( buildInfo ));
	buildInfo.vecLocalStart = vecLocalSrc;
	buildInfo.vecLocalEnd = vecLocalEnd;
	buildInfo.m_UseClipVert = ( m_pStudioHeader->numbones <= 1 ); // produce clipped verts only for static props
	buildInfo.decalDepth = m_pModelInstance->m_DecalCount++;
	buildInfo.m_iDecalTexture = decalTexture;
	buildInfo.decalFlags = (byte)flags;
	buildInfo.m_iDecalMaterial = -1;
	buildInfo.modelState = state;
	buildInfo.m_Radius = radius;

	// special check for per-vertex lighting
	if(( ent->curstate.iuser3 > 0 ) && world->vertex_lighting != NULL )
	{
		int		cacheID = m_pCurrentEntity->curstate.iuser3 - 1;
		dvlightlump_t	*dvl = world->vertex_lighting;

		if( cacheID < dvl->nummodels && dvl->dataofs[cacheID] != -1 )
			buildInfo.modelLight = (dmodellight_t *)((byte *)world->vertex_lighting + dvl->dataofs[cacheID]);
	}

	// step over all body parts + add decals to them all!
	for( int k = 0; k < m_pStudioHeader->numbodyparts; k++ ) 
	{
		// grab the model for this body part
		buildInfo.m_Model = StudioSetupModel( k, (void **)&m_pBodyPart, (void **)&m_pSubModel );
		buildInfo.m_Body = k;
		AddDecalToModel( buildInfo );
	}

	// dump remainig vertices
	CreateDecalForMaterial( buildInfo );

	m_fShootDecal = false;
	m_fDoInterp = true;
}

void CStudioModelRenderer :: ComputeDecalTransform( DecalMaterial_t& decalMaterial, const matrix3x4 bones[] )
{
	DecalVertexList_t& verts = decalMaterial.m_Vertices; 
	m_nNumArrayVerts = m_nNumArrayElems = 0;
	matrix3x4	skinMat;
	byte alpha = 255;

	if( !R_ModelOpaque( m_pCurrentEntity->curstate.rendermode ))
		alpha = m_pCurrentEntity->curstate.renderamt;

	for( word i = verts.Head(); i != verts.InvalidIndex(); i = verts.Next( i ))
	{
		DecalVertex_t& vertex = verts[i];
		int bone = vertex.m_BoneWeights.bone[0];

		if( m_pRenderModel->poseToBone != NULL )
		{
			ComputeSkinMatrix( &vertex.m_BoneWeights, m_pworldtransform, skinMat );
			m_arrayverts[m_nNumArrayVerts] = skinMat.VectorTransform( vertex.m_Position );
		}
		else
		{
			m_arrayverts[m_nNumArrayVerts] = bones[bone].VectorTransform( vertex.m_Position );
		}

		if( FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
		{
			unsigned int	bl[3], scale;

			bl[0] = bl[1] = bl[2] = 0;

			// add all the lightmaps
			for( int map = 0; map < MAXLIGHTMAPS && m_pModelInstance->styles[map] != 255; map++ )
			{
				scale = tr.lightstylevalue[m_pModelInstance->styles[map]];
				bl[0] += TEXTURE_TO_TEXGAMMA( vertex.m_Light[map][0] ) * scale;
				bl[1] += TEXTURE_TO_TEXGAMMA( vertex.m_Light[map][1] ) * scale;
				bl[2] += TEXTURE_TO_TEXGAMMA( vertex.m_Light[map][2] ) * scale;
			}

			m_arraycolor[m_nNumArrayVerts][0] = Q_min(( bl[0] >> 7 ), 255 );
			m_arraycolor[m_nNumArrayVerts][1] = Q_min(( bl[1] >> 7 ), 255 );
			m_arraycolor[m_nNumArrayVerts][2] = Q_min(( bl[2] >> 7 ), 255 );
		}
		else if( m_pRenderModel->poseToBone != NULL )
		{
			Vector vertexLight, vecNormal;
			float lv;

			vecNormal = skinMat.VectorRotate( vertex.m_Normal );
			StudioLighting( &lv, -1, decalMaterial.flags, vecNormal );
			vertexLight = m_pModelInstance->lighting.color * lv;

			m_arraycolor[m_nNumArrayVerts][0] = vertexLight.x * 255;
			m_arraycolor[m_nNumArrayVerts][1] = vertexLight.y * 255;
			m_arraycolor[m_nNumArrayVerts][2] = vertexLight.z * 255;
		}
		else
		{
			Vector vertexLight;
			float lv;

			// NOTE: studiolights used local coordinates for bonelighting so we pass untransformed normal
			StudioLighting( &lv, bone, decalMaterial.flags, vertex.m_Normal );
			vertexLight = m_pModelInstance->lighting.color * lv;

			m_arraycolor[m_nNumArrayVerts][0] = vertexLight.x * 255;
			m_arraycolor[m_nNumArrayVerts][1] = vertexLight.y * 255;
			m_arraycolor[m_nNumArrayVerts][2] = vertexLight.z * 255;
		}

		m_arraycoord[m_nNumArrayVerts].x = vertex.m_TexCoord0.x;
		m_arraycoord[m_nNumArrayVerts].y = vertex.m_TexCoord0.y;
		m_arraycoord[m_nNumArrayVerts].z = vertex.m_TexCoord1.x;
		m_arraycoord[m_nNumArrayVerts].w = vertex.m_TexCoord1.y;
		m_arraycolor[m_nNumArrayVerts][3] = alpha; // fade decals when fade corpse 
		m_nNumArrayVerts++;	
	}
}

//-----------------------------------------------------------------------------
// Draws all the decals using a particular material
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: DrawDecalMaterial( DecalMaterial_t& decalMaterial, const matrix3x4 bones[] )
{
	// It's possible for the index count to become zero due to decal retirement
	int indexCount = decalMaterial.m_Indices.Count();
	if( indexCount == 0 ) return;

	int vertexCount = decalMaterial.m_Vertices.Count();

	ComputeDecalTransform( decalMaterial, bones );

	// Set the indices
	// This is a little tricky. Because we can retire decals, the indices
	// for each decal start at 0. We output all the vertices in order of
	// each decal, and then fix up the indices based on how many vertices
	// we wrote out for the decals
	word decal = decalMaterial.m_Decals.Head();
	int indicesRemaining = decalMaterial.m_Decals[decal].m_IndexCount;
	int vertexOffset = 0;

	for( int i = 0; i < indexCount; i++ )
	{
		m_arrayelems[m_nNumArrayElems] = decalMaterial.m_Indices[i] + vertexOffset; 
		m_nNumArrayElems++;

		if( --indicesRemaining <= 0 )
		{
			vertexOffset += decalMaterial.m_Decals[decal].m_VertexCount;
			decal = decalMaterial.m_Decals.Next( decal ); 

			if( decal != decalMaterial.m_Decals.InvalidIndex( ))
				indicesRemaining = decalMaterial.m_Decals[decal].m_IndexCount;
		}
	}

	if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		pglDrawRangeElementsEXT( GL_TRIANGLES, 0, m_nNumArrayVerts, m_nNumArrayElems, GL_UNSIGNED_INT, m_arrayelems );
	else pglDrawElements( GL_TRIANGLES, m_nNumArrayElems, GL_UNSIGNED_INT, m_arrayelems );

	r_stats.c_total_tris += (m_nNumArrayElems / 3);
	r_stats.num_flushes++;
}

//-----------------------------------------------------------------------------
// Draws all the decals on a particular model
//-----------------------------------------------------------------------------
void CStudioModelRenderer :: DrawDecal( cl_entity_t *e )
{
	if( e->modelhandle == INVALID_HANDLE )
		return;

	ModelInstance_t *inst = &m_ModelInstances[e->modelhandle];

	if( inst->visframe != tr.realframecount )
		return;	// model is culled

	if( inst->m_DecalHandle == INVALID_HANDLE )
		return;	// no decals for this model

	// All decal vertex data is are stored in pose space
	// So as long as the pose-to-world transforms are set, we're all ready!

	// Get the decal list for this lod
	DecalModelList_t const& list = m_DecalList[inst->m_DecalHandle];

	mposetobone_t *m = m_pRenderModel->poseToBone;

	// setup bone transform
	if( m != NULL )
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i].ConcatTransforms( m->posetobone[i] );
	}
	else
	{
		// no pose to bone just copy the bones
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_pworldtransform[i] = m_pModelInstance->m_pbones[i];
	}

	// setup bone lighting
	if( !m_pRenderModel->poseToBone && !FBitSet( m_pModelInstance->info_flags, MF_VERTEX_LIGHTING ))
	{
		for( int i = 0; i < m_pStudioHeader->numbones; i++ )
			m_bonelightvecs[i] = m_pworldtransform[i].VectorIRotate( inst->lighting.plightvec );
	}

	GL_Blend( GL_TRUE );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	if( CVAR_TO_BOOL( r_polyoffset ))
	{
		pglEnable( GL_POLYGON_OFFSET_FILL );
		pglPolygonOffset( -1.0f, -r_polyoffset->value );
	}

	GL_DepthMask( GL_FALSE );
	GL_AlphaTest( GL_FALSE );

	GL_SelectTexture( GL_TEXTURE0 ); // keep texcoords at 0-th unit
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 4, GL_FLOAT, 16, m_arraycoord );

	pglEnableClientState( GL_COLOR_ARRAY );
	pglColorPointer( 4, GL_UNSIGNED_BYTE, 0, m_arraycolor );

	pglEnableClientState( GL_VERTEX_ARRAY );
	pglVertexPointer( 3, GL_FLOAT, 12, m_arrayverts );

	int decalTexture = -1;
	int modelTexture = -1;
	word hLastShader = -1;

	// Draw each set of decals using a particular material
	for( word mat = list.m_FirstMaterial; mat != m_DecalMaterial.InvalidIndex(); mat = m_DecalMaterial.Next( mat ))
	{
		DecalMaterial_t& decalMaterial = m_DecalMaterial[mat];
		bool valid = FBitSet( decalMaterial.flags, STUDIO_NF_NODRAW ) ? false : true;

		// NOTE: even if shader was failed we can draw the decals with some limitations
		if( valid && (( mat == list.m_FirstMaterial ) || ( RI->currentshader != &glsl_programs[decalMaterial.hProgram] )))
		{
			GL_BindShader( &glsl_programs[decalMaterial.hProgram] );			
			ASSERT( RI->currentshader != NULL );
			pglUniform4fARB( RI->currentshader->u_FogParams, tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
		}

		if(( decalTexture != decalMaterial.decalTexture ) || ( modelTexture != decalMaterial.modelTexture ))
		{
			// bind the decal texture
			GL_Bind( GL_TEXTURE0, decalMaterial.decalTexture );
			if( valid ) GL_Bind( GL_TEXTURE1, decalMaterial.modelTexture );
			decalTexture = decalMaterial.decalTexture;
			modelTexture = decalMaterial.modelTexture;
		}

		// adjust the cull state
		if( valid && FBitSet( decalMaterial.flags, STUDIO_NF_HAS_ALPHA ))
			GL_Cull( GL_NONE );
		else GL_Cull( GL_FRONT );

		DrawDecalMaterial( decalMaterial, m_pworldtransform );
	}

	pglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	pglDisableClientState( GL_VERTEX_ARRAY );
	pglDisableClientState( GL_COLOR_ARRAY );
	if( CVAR_TO_BOOL( r_polyoffset ))
		pglDisable( GL_POLYGON_OFFSET_FILL );
	GL_SelectTexture( glConfig.max_texture_units - 1 ); // force to cleanup all the units
	GL_CleanUpTextureUnits( 0 );
	GL_Cull( GL_FRONT );
}
