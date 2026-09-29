/*
gltf_source.cpp - glTF reference mesh/animation importer
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"

// compile cgltf implementation exactly once; the implementation section lives
// outside cgltf.h's include guard, so undef the macro before any other include
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#undef CGLTF_IMPLEMENTATION

#include "cmdlib.h"
#include "mathlib.h"
#include "stringlib.h"
#include "file_system.h"
#include "gltf_source.h"
#include <vector>

#define GLTF_ROOT_BONE_NAME "static_prop"

//-----------------------------------------------------------------------------
// Purpose: pick a texture name for a primitive: material name, then mesh name,
//          then the base color texture image name
//-----------------------------------------------------------------------------
static void GetTextureName( const cgltf_mesh *mesh, const cgltf_primitive *prim, char *out, size_t outSize )
{
	out[0] = '\0';

	if( prim->material && prim->material->name && prim->material->name[0] )
	{
		Q_strncpy( out, prim->material->name, outSize );
		COM_DefaultExtension( out, ".tga" );
	}
	else if( mesh->name && mesh->name[0] )
	{
		Q_strncpy( out, mesh->name, outSize );
		COM_DefaultExtension( out, ".tga" );
	}
	else if( prim->material && prim->material->has_pbr_metallic_roughness )
	{
		const cgltf_texture *tex = prim->material->pbr_metallic_roughness.base_color_texture.texture;

		if( tex && tex->image && tex->image->uri && tex->image->uri[0] )
		{
			const char *uri = tex->image->uri;
			const char *base = uri;

			// strip directory part
			for( const char *p = uri; *p; p++ )
			{
				if( *p == '/' || *p == '\\' )
					base = p + 1;
			}

			Q_strncpy( out, base, outSize );
			COM_StripExtension( out );
			COM_DefaultExtension( out, ".tga" );
		}
	}

	if( out[0] == '\0' )
	{
		Q_strncpy( out, "default", outSize );
		COM_DefaultExtension( out, ".tga" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: convert glTF node world matrix (Y-up, column-major) to GoldSrc matrix3x4 (Z-up)
//-----------------------------------------------------------------------------
static matrix3x4 GlTFNodeWorldGold( const cgltf_node *node )
{
	cgltf_float m[16];

	cgltf_node_transform_world( node, m );

	// glTF world matrix W is column-major; GoldSrc space is M = C * W with C: (x,y,z)->(x,-z,y)
	return matrix3x4(
		m[0],  -m[2],  m[1],
		m[4],  -m[6],  m[5],
		m[8],  -m[10], m[9],
		m[12], -m[14], m[13] );
}

//-----------------------------------------------------------------------------
// Purpose: build bind-pose bones (names, hierarchy and local transforms) from a glTF skin
//-----------------------------------------------------------------------------
static bool BuildSkinSkeleton( const cgltf_skin *skin, s_node_t *localBone, s_bone_t *skeleton, int &numbones )
{
	if( skin == nullptr || skin->joints_count == 0 || skin->joints_count > MAXSTUDIOSRCBONES )
		return false;

	int nj = (int)skin->joints_count;

	for( int i = 0; i < nj; i++ )
	{
		const cgltf_node *n = skin->joints[i];
		const char *nm = ( n->name && n->name[0] ) ? n->name : "bone";

		Q_strncpy( localBone[i].name, nm, sizeof( localBone[i].name ));
		localBone[i].parent = -1;
	}

	for( int i = 0; i < nj; i++ )
	{
		const cgltf_node *n = skin->joints[i];

		if( n->parent )
		{
			for( int k = 0; k < nj; k++ )
			{
				if( skin->joints[k] == n->parent )
				{
					localBone[i].parent = k;
					break;
				}
			}
		}
	}

	std::vector<matrix3x4> world( nj );

	for( int i = 0; i < nj; i++ )
		world[i] = GlTFNodeWorldGold( skin->joints[i] );

	for( int i = 0; i < nj; i++ )
	{
		int p = localBone[i].parent;
		matrix3x4 local = ( p == -1 ) ? world[i] : world[p].Invert().ConcatTransforms( world[i] );

		local.GetStudioTransform( skeleton[i].pos, skeleton[i].rot );
	}

	numbones = nj;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: collapse duplicate/small bone weights and normalize them
//-----------------------------------------------------------------------------
static int NormalizeBoneWeights( int count, int bones[MAXSTUDIOBONEWEIGHTS], float weights[MAXSTUDIOBONEWEIGHTS] )
{
	for( int i = 0; i < count - 1; i++ )
	{
		for( int j = i + 1; j < count; j++ )
		{
			if( bones[i] == bones[j] )
			{
				weights[i] += weights[j];
				weights[j] = 0.0f;
			}
		}
	}

	for( int i = 0; i < count - 1; i++ )
	{
		for( int j = i + 1; j < count; j++ )
		{
			if( weights[j] > weights[i] )
			{
				float w = weights[i]; weights[i] = weights[j]; weights[j] = w;
				int b = bones[i]; bones[i] = bones[j]; bones[j] = b;
			}
		}
	}

	while( count > 1 && weights[count-1] < 0.005f )
		count--;

	float sum = 0.0f;
	for( int i = 0; i < count; i++ )
		sum += weights[i];

	if( sum <= 0.0f )
	{
		for( int i = 0; i < count; i++ )
			weights[i] = 1.0f / count;
	}
	else
	{
		for( int i = 0; i < count; i++ )
			weights[i] /= sum;
	}

	return count;
}

CGltfSource::CGltfSource( const char *name )
	: m_data( nullptr, &cgltf_free )
{
	char		path[1024];
	cgltf_options	options = {};
	cgltf_data	*data = nullptr;

	Q_snprintf( path, sizeof( path ), "%s/%s", cddir[numdirs], name );
	COM_DefaultExtension( path, ".gltf" );
	m_filename = path;

	cgltf_result result = cgltf_parse_file( &options, path, &data );

	if( result != cgltf_result_success || data == nullptr )
		COM_FatalError( "failed to parse glTF file %s (error %d)\n", path, (int)result );

	result = cgltf_load_buffers( &options, data, path );

	if( result != cgltf_result_success )
	{
		cgltf_free( data );
		COM_FatalError( "failed to load glTF buffers %s (error %d)\n", path, (int)result );
	}

	m_data.reset( data );
}

void CGltfSource::GrabStudio( s_model_t *pmodel )
{
	if( !m_data )
		COM_FatalError( "glTF file %s was not loaded\n", m_filename.c_str());

	cgltf_data *data = m_data.get();
	const cgltf_skin *skin = ( data->skins_count > 0 ) ? &data->skins[0] : nullptr;

	if( !BuildSkinSkeleton( skin, pmodel->localBone, pmodel->skeleton, pmodel->numbones ))
	{
		// no skeleton: bind everything to a single root bone
		pmodel->numbones = 1;
		Q_strncpy( pmodel->localBone[0].name, GLTF_ROOT_BONE_NAME, sizeof( pmodel->localBone[0].name ));
		pmodel->localBone[0].parent = -1;
		pmodel->skeleton[0].pos = Vector( 0, 0, 0 );
		pmodel->skeleton[0].rot = Radian( 0, 0, 0 );
	}

	Build_Reference( pmodel );

	// glTF is Y-up while GoldSrc is Z-up: rotate all geometry 90 degrees around X
	matrix3x4 convert = matrix3x4( g_vecZero, Radian( M_PI / 2.0f, 0.0f, 0.0f ));

	for( cgltf_size mi = 0; mi < data->meshes_count; mi++ )
	{
		const cgltf_mesh *mesh = &data->meshes[mi];

		for( cgltf_size pi = 0; pi < mesh->primitives_count; pi++ )
		{
			const cgltf_primitive *prim = &mesh->primitives[pi];
			const cgltf_accessor *posAcc = nullptr;
			const cgltf_accessor *nrmAcc = nullptr;
			const cgltf_accessor *uvAcc = nullptr;
			const cgltf_accessor *jointAcc = nullptr;
			const cgltf_accessor *weightAcc = nullptr;

			for( cgltf_size ai = 0; ai < prim->attributes_count; ai++ )
			{
				const cgltf_attribute *attr = &prim->attributes[ai];

				if( attr->type == cgltf_attribute_type_position )
					posAcc = attr->data;
				else if( attr->type == cgltf_attribute_type_normal )
					nrmAcc = attr->data;
				else if( attr->type == cgltf_attribute_type_texcoord && attr->index == 0 )
					uvAcc = attr->data;
				else if( attr->type == cgltf_attribute_type_joints )
					jointAcc = attr->data;
				else if( attr->type == cgltf_attribute_type_weights )
					weightAcc = attr->data;
			}

			if( posAcc == nullptr )
				continue;

			bool skinned = ( skin != nullptr && jointAcc != nullptr && weightAcc != nullptr );

			char		texturename[64];
			s_mesh_t	*pmesh;

			GetTextureName( mesh, prim, texturename, sizeof( texturename ));
			pmesh = LookupMesh( pmodel, texturename );

			cgltf_size indexCount = prim->indices ? prim->indices->count : posAcc->count;

			for( cgltf_size t = 0; t + 2 < indexCount; t += 3 )
			{
				s_trianglevert_t	*ptri = LookupTriangle( pmesh, pmesh->numtris );
				cgltf_size		idx[3];
				Vector			vpos[3];
				Vector			vnorm[3];
				float			uv[3][2];
				int			bones[3][MAXSTUDIOBONEWEIGHTS];
				float			weights[3][MAXSTUDIOBONEWEIGHTS];
				int			numb[3] = { 1, 1, 1 };

				for( int j = 0; j < 3; j++ )
				{
					idx[j] = prim->indices ? cgltf_accessor_read_index( prim->indices, t + j ) : ( t + j );

					float p[3] = { 0, 0, 0 };
					float n[3] = { 0, 0, 0 };

					cgltf_accessor_read_float( posAcc, idx[j], p, 3 );
					if( nrmAcc ) cgltf_accessor_read_float( nrmAcc, idx[j], n, 3 );
					if( uvAcc )
					{
						cgltf_accessor_read_float( uvAcc, idx[j], uv[j], 2 );
						// glTF V origin is top-left, SMD expects bottom-left
						uv[j][1] = 1.0f - uv[j][1];
					}
					else { uv[j][0] = 0.0f; uv[j][1] = 0.0f; }

					vpos[j] = convert.VectorTransform( Vector( p[0], p[1], p[2] ));
					vnorm[j] = convert.VectorRotate( Vector( n[0], n[1], n[2] ));

					bones[j][0] = 0;
					weights[j][0] = 1.0f;

					if( skinned )
					{
						float jf[4] = { 0, 0, 0, 0 };
						float wf[4] = { 0, 0, 0, 0 };

						cgltf_accessor_read_float( jointAcc, idx[j], jf, 4 );
						cgltf_accessor_read_float( weightAcc, idx[j], wf, 4 );

						for( int k = 0; k < MAXSTUDIOBONEWEIGHTS; k++ )
						{
							bones[j][k] = (int)( jf[k] + 0.5f );
							weights[j][k] = wf[k];
						}

						numb[j] = NormalizeBoneWeights( MAXSTUDIOBONEWEIGHTS, bones[j], weights[j] );
					}
				}

				// generate a face normal if the mesh doesn't provide one
				if( nrmAcc == nullptr )
				{
					Vector edge1 = vpos[1] - vpos[0];
					Vector edge2 = vpos[2] - vpos[0];
					Vector facenorm = CrossProduct( edge1, edge2 ).Normalize();

					vnorm[0] = vnorm[1] = vnorm[2] = facenorm;
				}

				for( int j = 0; j < 3; j++ )
				{
					s_trianglevert_t *ptriv = pmodel->flip_triangles ? ( ptri + 2 - j ) : ( ptri + j );
					s_srcvertex_t *srcv;

					pmodel->srcvert.AddToTail();
					srcv = &pmodel->srcvert[pmodel->srcvert.Count() - 1];
					memset( srcv, 0, sizeof( *srcv ));

					srcv->vert = vpos[j] * pmodel->scale;
					srcv->norm = vnorm[j];
					srcv->skinref = pmesh->skinref;
					srcv->localWeight.numbones = numb[j];

					for( int k = 0; k < numb[j]; k++ )
					{
						srcv->localWeight.bone[k] = bones[j][k];
						srcv->localWeight.weight[k] = weights[j][k];
					}

					if( skinned )
						has_boneweights = true;

					ptriv->u = uv[j][0];
					ptriv->v = uv[j][1];
					ptriv->vertindex = ptriv->normindex = pmodel->srcvert.Count() - 1;
				}

				pmesh->numtris++;
			}
		}
	}
}

void CGltfSource::GrabAnimation( s_animation_t *panim )
{
	if( !m_data )
		COM_FatalError( "glTF file %s was not loaded\n", m_filename.c_str());

	cgltf_data *data = m_data.get();
	const cgltf_skin *skin = ( data->skins_count > 0 ) ? &data->skins[0] : nullptr;
	std::vector<s_bone_t> skeleton( MAXSTUDIOSRCBONES );
	int numbones = 0;

	panim->source.startframe = 0;
	panim->source.endframe = 0;
	panim->source.numframes = 1;

	if( BuildSkinSkeleton( skin, panim->localBone, skeleton.data(), numbones ))
	{
		panim->numbones = numbones;
		panim->rawanim[0] = (s_bone_t *)Mem_Alloc( sizeof( s_bone_t ) * numbones );

		for( int i = 0; i < numbones; i++ )
			panim->rawanim[0][i] = skeleton[i];
	}
	else
	{
		// static-only: emit a single identity frame for the root bone
		panim->numbones = 1;
		Q_strncpy( panim->localBone[0].name, GLTF_ROOT_BONE_NAME, sizeof( panim->localBone[0].name ));
		panim->localBone[0].parent = -1;

		panim->rawanim[0] = (s_bone_t *)Mem_Alloc( sizeof( s_bone_t ));
		panim->rawanim[0][0].pos = Vector( 0, 0, 0 );
		panim->rawanim[0][0].rot = Radian( 0, 0, 0 );
	}
}
