/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// studio.c

#include "qrad.h"
#include "../../engine/studio.h"
#include "model_trace.h"
#include "imagelib.h"
#include "crclib.h"

typedef struct
{
	char		name[64];
	mstudiomodel_t	*pmodel;
	bool		shadow;
} TmpModel_t;

static void ExtractAnimValue( int frame, mstudioanim_t *panim, int dof, float scale, float &v1 )
{
	if( !panim || panim->offset[dof] == 0 )
	{
		v1 = 0.0f;
		return;
	}

	const mstudioanimvalue_t *panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[dof]);
	int k = frame;

	while( panimvalue->num.total <= k )
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;

		if( panimvalue->num.total == 0 )
		{
			v1 = 0.0f;
			return;
		}
	}

	// Bah, missing blend!
	if( panimvalue->num.valid > k )
	{
		v1 = panimvalue[k+1].value * scale;
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
	}
}

static void StudioCalcBoneTransform( int frame, mstudiobone_t *pbone, mstudioanim_t *panim, vec3_t pos, vec4_t q )
{
	vec3_t	origin;
	vec3_t	angles;

	ExtractAnimValue( frame, panim, 0, pbone->scale[0], origin[0] );
	ExtractAnimValue( frame, panim, 1, pbone->scale[1], origin[1] );
	ExtractAnimValue( frame, panim, 2, pbone->scale[2], origin[2] );
	ExtractAnimValue( frame, panim, 3, pbone->scale[3], angles[0] );
	ExtractAnimValue( frame, panim, 4, pbone->scale[4], angles[1] );
	ExtractAnimValue( frame, panim, 5, pbone->scale[5], angles[2] );

	for( int j = 0; j < 3; j++ )
	{
		origin[j] = pbone->value[j+0] + origin[j];
		angles[j] = pbone->value[j+3] + angles[j];
	}

	AngleQuaternion( angles, q );
	VectorCopy( origin, pos );
}

static void StudioSetupTriangle( tmesh_t *mesh, int index )
{
	tface_t	*face = &mesh->faces[index];

	// calculate mins & maxs
	ClearBounds( face->absmin, face->absmax );
	face->contents = CONTENTS_SOLID;

	assert( face->a < mesh->numverts );
	assert( face->b < mesh->numverts );
	assert( face->c < mesh->numverts );

	// calc bounds
	AddPointToBounds( mesh->verts[face->a].point, face->absmin, face->absmax );
	AddPointToBounds( mesh->verts[face->a].point, mesh->absmin, mesh->absmax );
	AddPointToBounds( mesh->verts[face->b].point, face->absmin, face->absmax );
	AddPointToBounds( mesh->verts[face->b].point, mesh->absmin, mesh->absmax );
	AddPointToBounds( mesh->verts[face->c].point, face->absmin, face->absmax );
	AddPointToBounds( mesh->verts[face->c].point, mesh->absmin, mesh->absmax );
	ExpandBounds( face->absmin, face->absmax, 1.0 );

	// convert skinref to texture pointer
	face->texture = &mesh->textures[face->skinref];
	// setup efficiency intersection data
	face->PrepareIntersectionData( mesh->verts );
}

static void StudioRelinkFace( aabb_tree_t *tree, tface_t *face )
{
	// only shadow faces must be linked
	if( !face->shadow ) return;

	// find the first node that the facet box crosses
	areanode_t *node = tree->areanodes;

	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( face->absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( face->absmax[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	InsertLinkBefore( &face->area, &node->solid_edicts );
}

static int StudioCreateMeshFromTriangles( entity_t *ent, studiohdr_t *phdr, const char *modname, int body, int skin, int flags, matrix3x4 transform[] )
{
	TmpModel_t	submodel[MAXSTUDIOMODELS];	// list of unique models
	int		i, j, k, totalVertSize = 0;
	int		num_submodels = 0;
	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t	*psubmodel;

	memset( submodel, 0, sizeof( submodel ));

	// build list of unique submodels (by name)
	for( i = 0; i < phdr->numbodyparts; i++ )
	{
		pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;

		for( j = 0; j < pbodypart->nummodels; j++ )
		{
			psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + j;
			if( !psubmodel->nummesh ) continue; // blank submodel, ignore it

			for( k = 0; k < num_submodels; k++ )
			{
				if( !Q_stricmp( submodel[k].name, psubmodel->name ))
					break;
			}

			// add new one
			if( k == num_submodels )
			{
				Q_strncpy( submodel[k].name, psubmodel->name, sizeof( submodel[k].name ));
				submodel[k].pmodel = psubmodel;
				num_submodels++;
			}
		}
	}

	// find which parts are used by current body
	for( i = 0; i < phdr->numbodyparts; i++ )
	{
		pbodypart = (mstudiobodyparts_t *)((byte *)phdr + phdr->bodypartindex) + i;

		int index = body / pbodypart->base;
		index = index % pbodypart->nummodels;
		psubmodel = (mstudiomodel_t *)((byte *)phdr + pbodypart->modelindex) + index;

		for( j = 0; j < num_submodels; j++ )
		{
			if( submodel[j].pmodel == psubmodel )
			{
				submodel[j].shadow = true;
				break;
			}
		}
	}

	// count unique model vertices
	for( i = 0; i < num_submodels; i++ )
	{
		psubmodel = submodel[i].pmodel;
		totalVertSize += psubmodel->numverts;
	}

	tface_t		*faces = (tface_t *)Mem_Alloc( sizeof( tface_t ) * totalVertSize * 8 ); // allocate face lighting array
	tvert_t		*verts = (tvert_t *)Mem_Alloc( sizeof( tvert_t ) * totalVertSize * 8 ); // allocate vertex array
	mstudiotexture_t	*ptexture = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
	dvlightofs_t	vsubmodels[MAXSTUDIOMODELS];
	dflightofs_t	fsubmodels[MAXSTUDIOMODELS];
	int		numFaces = 0, numVerts = 0;

	memset( vsubmodels, 0, sizeof( vsubmodels ));
	memset( fsubmodels, 0, sizeof( fsubmodels ));

	for( k = 0; k < num_submodels; k++ )
	{
		mstudiomodel_t	*psubmodel = submodel[k].pmodel;

		if( psubmodel->numverts <= 0 )
			continue; // a blank submodel

		vec3_t		*pstudioverts = (vec3_t *)((byte *)phdr + psubmodel->vertindex);
		vec3_t		*pstudionorms = (vec3_t *)((byte *)phdr + psubmodel->normindex);
		vec3_t		*m_verts = (vec3_t *)Mem_Alloc( sizeof( vec3_t ) * psubmodel->numverts );
		vec3_t		*m_norms = (vec3_t *)Mem_Alloc( sizeof( vec3_t ) * psubmodel->numnorms );
		byte		*pvertbone = ((byte *)phdr + psubmodel->vertinfoindex);
		byte		*pnormbone = ((byte *)phdr + psubmodel->norminfoindex);
		short		*pskinref = (short *)((byte *)phdr + phdr->skinindex);
		int		m_skinnum = bound( 0, skin, MAXSTUDIOSKINS - 1 );

		// setup skin
		if( m_skinnum != 0 && m_skinnum < phdr->numskinfamilies )
			pskinref += (m_skinnum * phdr->numskinref);

		// setup all the vertices
		for( i = 0; i < psubmodel->numverts; i++ )
			Matrix3x4_VectorTransform( transform[pvertbone[i]], pstudioverts[i], m_verts[i] );

		// setup all the normals
		for( i = 0; i < psubmodel->numnorms; i++ )
			Matrix3x4_VectorRotate( transform[pnormbone[i]], pstudionorms[i], m_norms[i] );

		vsubmodels[k].submodel_offset = (byte *)psubmodel - (byte *)phdr;
		vsubmodels[k].vertex_offset = numVerts;
		fsubmodels[k].submodel_offset = (byte *)psubmodel - (byte *)phdr;
		fsubmodels[k].surface_offset = numFaces;

		// build all the light vertices because we should include all the bodies into the buffer
		for( j = 0; j < psubmodel->nummesh; j++ ) 
		{
			mstudiomesh_t	*pmesh = (mstudiomesh_t *)((byte *)phdr + psubmodel->meshindex) + j;
			float	s = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].width;
			float	t = 1.0f / (float)ptexture[pskinref[pmesh->skinref]].height;
			short	*ptricmds = (short *)((byte *)phdr + pmesh->triindex);
			int		flags = ptexture[pskinref[pmesh->skinref]].flags;

			while ((i = *(ptricmds++)) != 0)
			{
				int	vertexState = 0;
				bool	tri_strip;

				if( i < 0 )
				{
					tri_strip = false;
					i = -i;
				}
				else tri_strip = true;

				for( ; i > 0; i--, ptricmds += 4 )
				{
					tvert_t	*tv = &verts[numVerts];
					tface_t	*tf = &faces[numFaces];

					if( submodel[k].shadow )
						tf->shadow = true;
					tf->skinref = pskinref[pmesh->skinref];

					// build in indices
					if( vertexState++ < 3 )
					{
						switch( vertexState )
						{
						case 1:
							tf->a = numVerts;
							break;
						case 2:
							tf->b = numVerts;
							break;						
						case 3:
							tf->c = numVerts;
							numFaces++;
							break;
						}
					}
					else if( tri_strip )
					{
						// flip triangles between clockwise and counter clockwise
						if( vertexState & 1 )
						{
							// draw triangle [n-2 n-1 n]
							tf->a = numVerts - 2;
							tf->b = numVerts - 1;
							tf->c = numVerts;
						}
						else
						{
							// draw triangle [n-1 n-2 n]
							tf->a = numVerts - 1;
							tf->b = numVerts - 2;
							tf->c = numVerts;
						}
						numFaces++;
					}
					else
					{
						// draw triangle fan [0 n-1 n]
						tf->a = numVerts - ( vertexState - 1 );
						tf->b = numVerts - 1;
						tf->c = numVerts;
						numFaces++;
					}

					if( FBitSet( flags, STUDIO_NF_CHROME ))
					{
						// probably always equal 64 (see studiomdl.c for details)
						tv->st[0] = float( s );
						tv->st[1] = float( t );
					}
					else if( FBitSet( flags, STUDIO_NF_UV_COORDS ))
					{
						tv->st[0] = HalfToFloat( ptricmds[2] );
						tv->st[1] = HalfToFloat( ptricmds[3] );
					}
					else
					{
						tv->st[0] = float( ptricmds[2] * s );
						tv->st[1] = float( ptricmds[3] * t );
					}

					VectorCopy( m_verts[ptricmds[0]], tv->point );
					VectorCopy( m_norms[ptricmds[1]], tv->normal );
					VectorNormalize2( tv->normal );
					numVerts++;
				}
			}
		}

		// don't keep this because different submodels may have difference count of normals
		Mem_Free( m_norms );
		Mem_Free( m_verts );
	}

	// perfomance warning
	if( numFaces >= MAX_TRIANGLES )
	{
		MsgDev( D_ERROR, "%s have too many triangles (%i). Ignored\n", modname, numFaces );
		Mem_Free( verts );
		Mem_Free( faces );
		return 0; // failed to build (too many triangles)
	}
	else if( numFaces >= (MAX_TRIANGLES>>1))
		MsgDev( D_WARN, "%s have too many triangles (%i)\n", modname, numFaces );

	size_t	texdata_size = sizeof( timage_t ) * phdr->numtextures;
	char	diffuse[128], texname[128], mdlname[64];
	rgbdata_t	*external_textures[256];

	COM_FileBase( modname, mdlname );
	memset( external_textures, 0 , sizeof( external_textures ));

	// store only texdata where we has alpha-testing
	for( i = 0; i < phdr->numtextures; i++ )
	{
		mstudiotexture_t	*tex = &ptexture[i];

		COM_FileBase( tex->name, texname );
		Q_snprintf( diffuse, sizeof( diffuse ), "textures/%s/%s", mdlname, texname );

#ifdef HLRAD_EXTERNAL_TEXTURES
		//we have to load all textures for colored gi, but only alpha will be kept for tracing
		rgbdata_t *test = COM_LoadImage(diffuse, true, FS_LoadFile);
		if(test)
		{
			MsgDev(D_REPORT, "Load external texture %s\n", diffuse);
			external_textures[i] = test;

			if (FBitSet(tex->flags, STUDIO_NF_ADDITIVE | STUDIO_NF_MASKED))	
				if (FBitSet(test->flags, IMAGE_HAS_ALPHA))
					texdata_size += test->width * test->height;			
		}

#endif
		if( external_textures[i] ) continue; // already loaded

		// NOTE: store the only pixels, we doesn't need a palette
		if( FBitSet( tex->flags, STUDIO_NF_MASKED ))
			texdata_size += tex->width * tex->height;
	}

	size_t	memsize = sizeof( tmesh_t ) + ( sizeof( tface_t ) * numFaces ) + ( sizeof( tvert_t ) * numVerts ) + texdata_size;

	// alloc vislight matrix
	if( FBitSet( flags, FMESH_MODEL_LIGHTMAPS|FMESH_VERTEX_LIGHTING ))
		memsize += (g_numworldlights + 7) / 8;

	// alloc lighting faces
	if( FBitSet( flags, FMESH_MODEL_LIGHTMAPS ))
		memsize += sizeof( lface_t ) * numFaces;

	// alloc lighting verts
	if( FBitSet( flags, FMESH_VERTEX_LIGHTING ))
		memsize += sizeof( lvert_t ) * numVerts;

//	Msg( "%s alloc %s\n", modname, Q_memprint( memsize ));

	byte	*meshdata = (byte *)Mem_Alloc( memsize );
	byte	*meshend = meshdata + memsize; // bounds checking
	tmesh_t	*mesh = (tmesh_t *)meshdata;

	if( ent->cache ) Mem_Free( ent->cache ); // throw previous instance
	ent->cache = meshdata; // FreeEntities will be automatically free that

	// setup pointers
	meshdata += sizeof( tmesh_t );
	mesh->textures = (timage_t *)meshdata;
	meshdata += sizeof( timage_t ) * phdr->numtextures;
	mesh->faces = (tface_t *)meshdata;
	meshdata += sizeof( tface_t ) * numFaces;
	mesh->numfaces = numFaces;

	// store faces
	memcpy( mesh->faces, faces, sizeof( tface_t ) * mesh->numfaces );
	Mem_Free( faces );

	// setup additional lightdata if present
	if( FBitSet( flags, FMESH_MODEL_LIGHTMAPS ))
	{
		for( int i = 0; i < numFaces; i++ )
		{
			mesh->faces[i].light = (lface_t *)meshdata;
			meshdata += sizeof( lface_t );

			// clearing lightdata
			for( int j = 0; j < MAXLIGHTMAPS; j++ )
				mesh->faces[i].light->styles[j] = 255;
			mesh->faces[i].light->lightofs = -1;
		}
	}

	mesh->verts = (tvert_t *)meshdata;
	meshdata += sizeof( tvert_t ) * numVerts;
	mesh->numverts = numVerts;

	// store vertexes
	memcpy( mesh->verts, verts, sizeof( tvert_t ) * mesh->numverts );
	Mem_Free( verts );

	// setup additional lightdata if present
	if( FBitSet( flags, FMESH_VERTEX_LIGHTING ))
	{
		for( int i = 0; i < numVerts; i++ )
		{
			mesh->verts[i].light = (lvert_t *)meshdata;
			meshdata += sizeof( lvert_t );
			VectorCopy( mesh->verts[i].point, mesh->verts[i].light->pos );

			VectorClear( mesh->verts[i].light->pos );
		}
	}

	//move every vertex lighting pos closer to the face center to prevent selfshadowing in the corners
	if( FBitSet( flags, FMESH_VERTEX_LIGHTING ))	
	{
		for( i = 0; i < mesh->numfaces; i++ )
		{
			vec3_t	origin, delta;
			vec_t	dist, dot;
			tvert_t	*tv[3];

			tv[0] = &mesh->verts[mesh->faces[i].a];
			tv[1] = &mesh->verts[mesh->faces[i].b];
			tv[2] = &mesh->verts[mesh->faces[i].c];
/*
			VectorCopy( tv[0]->point, origin );
			VectorAdd( origin, tv[1]->point, origin );
			VectorAdd( origin, tv[2]->point, origin );
			VectorScale( origin, 0.333333333f, origin );
*/
			//incenter seems to be better than centroid
			TriangleIncenter( tv[0]->point, tv[1]->point, tv[2]->point, origin );

			for( j = 0; j < 3; j++ )
			{
				VectorSubtract( origin, tv[j]->point, delta );

				dist = VectorNormalize( delta );

				dot = DotProduct( delta, tv[j]->normal );

				if( dot < 0.0f )
				{
					VectorMA( delta, -dot, tv[j]->normal, delta );	//project offset vector to the tangent plane
				}
				
				dist = Q_min( dist, 1.0f );
				VectorMA( tv[j]->light->pos, dist, delta, tv[j]->light->pos );
			}
		}

		for( i = 0; i < numVerts; i++ )
		{
			vec_t	dist;
			dist = VectorNormalize( mesh->verts[i].light->pos );
			dist = Q_min( dist, 1.0f );

			VectorMA( mesh->verts[i].point, dist, mesh->verts[i].light->pos, mesh->verts[i].light->pos );
		}
	}

	memcpy( mesh->vsubmodels, vsubmodels, sizeof( mesh->vsubmodels ));
	memcpy( mesh->fsubmodels, fsubmodels, sizeof( mesh->fsubmodels ));

	if( FBitSet( flags, FMESH_MODEL_LIGHTMAPS|FMESH_VERTEX_LIGHTING ))
	{
		mesh->vislight = (byte *)meshdata;
		meshdata += (g_numworldlights + 7) / 8;
	}

	for( int l = 0; l < MAXLIGHTMAPS; l++ )
		mesh->styles[l] = 255;


	//diffuse color for faces, also mark two-sided vertices
	for( i = 0; i < mesh->numfaces; i++ )
	{
		tface_t				*face = &mesh->faces[i];
		mstudiotexture_t	*tex = &ptexture[face->skinref];
		rgbdata_t 			*extex = external_textures[face->skinref];
		tvert_t	*tv[3];
		byte	color_id;
		vec3_t	temp_color = {0,0,0};
		byte	weight_sum = 0;

		tv[0] = &mesh->verts[face->a];
		tv[1] = &mesh->verts[face->b];
		tv[2] = &mesh->verts[face->c];		

		for( j = 0; j < 3; j++ )
			tv[j]->twosided = FBitSet( tex->flags, STUDIO_NF_TWOSIDE );

		vec_t	face_s = ( tv[0]->st[0] + tv[1]->st[0] + tv[2]->st[0] ) * 0.3333333f;
		vec_t	face_t = ( tv[0]->st[1] + tv[1]->st[1] + tv[2]->st[1] ) * 0.3333333f;

		//sample texture 4 times - in the centroid and halfway to vertices, hopefully that's enough
		for( j = 0; j < 4; j++ )
		{
			vec_t s, t;
			if ( j < 3 )
			{
				s = tv[j]->st[0]; t = tv[j]->st[1];
			}
			else
			{
				s = face_s; t = face_t;
			}

			s = (s + face_s) * 0.5f;
			t = (t + face_t) * 0.5f;

			if( extex )
			{
				int	x = fix_coord( s * extex->width, extex->width );
				int	y = fix_coord( t * extex->height, extex->height );
				
				weight_sum++;
				for( k = 0; k < 3; k++ )
					temp_color[k] += TextureToLinear( extex->buffer[(y * extex->width + x) * 4 + k] );					
			}
			else
			{
				int	x = fix_coord( s * tex->width, tex->width );
				int	y = fix_coord( t * tex->height, tex->height );	

				color_id = *((byte *)phdr + tex->index + tex->width * y + x);			

				if( color_id != 255 )	//skip transparent alpha 
				{	
					weight_sum++;
					for( k = 0; k < 3; k++ )
						temp_color[k] += TextureToLinear( *((byte *)phdr + tex->index + tex->width * tex->height + color_id * 3 + k) );	
				}
			}
		}

		if( weight_sum > 0 )
			VectorScale( temp_color, 1.0f / (float)weight_sum, temp_color );

		for( k = 0; k < 3; k++ )
			face->color[k] = LinearToTexture( temp_color[k] );
		//Msg( "face color %d %d %d\n", face->color[0],face->color[1],face->color[2]);
	}
	

	// store only texdata where we have alpha-testing
	for( i = 0; i < phdr->numtextures; i++ )
	{
		mstudiotexture_t	*src = &ptexture[i];
		timage_t		*dst = &mesh->textures[i];

		dst->width = src->width;
		dst->height = src->height;
		dst->flags = src->flags;

		if( external_textures[i] )
		{
			rgbdata_t *test = external_textures[i];

			if (FBitSet(src->flags, STUDIO_NF_ADDITIVE | STUDIO_NF_MASKED))
				if (FBitSet(test->flags, IMAGE_HAS_ALPHA))
				{
					dst->data = meshdata;
					dst->width = test->width;
					dst->height = test->height;

					// copy alpha channel from external texture
					for( j = 0; j < test->width * test->height; j++ )
						dst->data[j] = test->buffer[j*4+3] < 128 ? 255 : 0; // because we used palette index, not alpha value
					meshdata += test->width * test->height; // move pointer
				}

			external_textures[i] = NULL;
			Mem_Free( test );
		}
		else if( FBitSet( src->flags, STUDIO_NF_MASKED ))
		{
			// NOTE: store the only pixels, we don't need a palette
			dst->data = meshdata;
			memcpy( dst->data, (byte *)phdr + src->index, src->width * src->height );
			meshdata += src->width * src->height; // move pointer
		}
	}

	if( meshdata != meshend )
		Msg( "%s memory corrupted\n", modname );

	ClearBounds( mesh->absmin, mesh->absmax );

	// do some post-initialization
	for( i = 0; i < numFaces; i++ )
		StudioSetupTriangle( mesh, i );

	return numFaces;
}

bool StudioConstructMesh( entity_t *ent, void *extradata, const char *modname, uint modelCRC, int flags )
{
	studiohdr_t	*phdr = (studiohdr_t *)extradata;
	double		start = I_FloatTime();
	vec3_t		origin, angles;
	int		i;
	int		body, skin;
	int		numFaces;
	vec3_t		xform;
	float		scale;
	
	if( phdr->numbones < 1 )
		return false;

	// get model properties
	GetVectorForKey( ent, "origin", origin );
	GetVectorForKey( ent, "angles", angles );

	if (g_compatibility_mode == CompatibilityMode::GoldSrc) {
		angles[0] = -angles[0]; // Stupid quake bug workaround
	}

	scale = FloatForKey( ent, "scale" );
	body = IntForKey( ent, "body" );
	skin = IntForKey( ent, "skin" );

	GetVectorForKey( ent, "xform", xform );

	if( VectorIsNull( xform ))
		VectorFill( xform, scale );

	// check xform values
	if( xform[0] < 0.01f ) xform[0] = 1.0f;
	if( xform[1] < 0.01f ) xform[1] = 1.0f;
	if( xform[2] < 0.01f ) xform[2] = 1.0f;
	if( xform[0] > 16.0f ) xform[0] = 16.0f;
	if( xform[1] > 16.0f ) xform[1] = 16.0f;
	if( xform[2] > 16.0f ) xform[2] = 16.0f;

	// compute default pose for building mesh from
	mstudioseqdesc_t	*pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	mstudioseqgroup_t	*pseqgroup = (mstudioseqgroup_t *)((byte *)phdr + phdr->seqgroupindex);
	mstudioanim_t	*panim = (mstudioanim_t *)((byte *)phdr + pseqdesc->animindex);
	mstudiobone_t	*pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	static vec3_t	pos[MAXSTUDIOBONES];
	static vec4_t	q[MAXSTUDIOBONES];

	for( i = 0; i < phdr->numbones; i++, pbone++, panim++ ) 
		StudioCalcBoneTransform( 0, pbone, panim, pos[i], q[i] );
	pbone = (mstudiobone_t *)((byte *)phdr + phdr->boneindex);
	matrix3x4	transform, bonematrix, bonetransform[MAXSTUDIOBONES];
	Matrix3x4_CreateFromEntityScale3f( transform, angles, origin, xform );

	// compute bones for default anim
	for( i = 0; i < phdr->numbones; i++ ) 
	{
		// initialize bonematrix
		Matrix3x4_FromOriginQuat( bonematrix, q[i], pos[i] );

		if( pbone[i].parent == -1 ) 
			Matrix3x4_ConcatTransforms( bonetransform[i], transform, bonematrix );
		else Matrix3x4_ConcatTransforms( bonetransform[i], bonetransform[pbone[i].parent], bonematrix );
	}

	if(( numFaces = StudioCreateMeshFromTriangles( ent, phdr, modname, body, skin, flags, bonetransform )) == 0 )
		return false;

	tmesh_t	*mesh = (tmesh_t *)ent->cache;
	size_t	memsize = Mem_Size( ent->cache );
	ent->modtype = mod_studio; // now our mesh is valid and ready to trace
	mesh->modelCRC = modelCRC;
	VectorCopy( origin, mesh->origin );
	VectorCopy( angles, mesh->angles );
	VectorCopy( xform, mesh->scale );
	mesh->backfrac = FloatForKey( ent, "zhlt_backfrac" );
	mesh->flags = flags; // copy settings

	MsgDev( D_REPORT, "%s: build time %g secs, size %s\n", modname, I_FloatTime() - start, Q_memprint( memsize ));

	return true;
}

void LoadStudio( entity_t *ent, void *extradata, int fileLength, int flags )
{
	const char *modname = ValueForKey( ent, "model" );
	uint32_t modelCRC = 0;
	studiohdr_t	*phdr;

	if( !extradata )
	{
		MsgDev( D_WARN, "LoadStudio: couldn't load %s\n", modname );
		return;
	}

	phdr = (studiohdr_t *)extradata;

	if( phdr->ident != IDSTUDIOHEADER || phdr->version != STUDIO_VERSION )
	{
		if( phdr->ident != IDSTUDIOHEADER )
			MsgDev( D_WARN, "LoadStudio: %s not a studio model\n", modname );
		else if( phdr->version != STUDIO_VERSION )
			MsgDev( D_WARN, "LoadStudio: %s has wrong version number (%i should be %i)\n", modname, phdr->version, STUDIO_VERSION );
		Mem_Free( extradata, C_FILESYSTEM );
		return;
	}

#ifndef HLRAD_VERTEXLIGHTING
	ClearBits( flags, FMESH_VERTEX_LIGHTING );
#endif

#ifndef HLRAD_LIGHTMAPMODELS
	ClearBits( flags, FMESH_MODEL_LIGHTMAPS );
#endif

#if defined( HLRAD_VERTEXLIGHTING ) || defined( HLRAD_LIGHTMAPMODELS )
	CRC32_Init( &modelCRC );
	CRC32_ProcessBuffer( &modelCRC, extradata, phdr->length );
	modelCRC = CRC32_Final( modelCRC );
#endif
	// well the textures place in separate file (very stupid case)
	if( phdr->numtextures == 0 )
	{
		char		texname[128], texpath[128];
		byte		*texdata, *moddata;
		studiohdr_t	*thdr, *newhdr;

		Q_strncpy( texname, modname, sizeof( texname ));
		COM_StripExtension( texname );

		Q_snprintf( texpath, sizeof( texpath ), "%sT.mdl", texname );
		MsgDev( D_REPORT, "loading %s\n", texpath );
		texdata = FS_LoadFile( texpath, NULL, false );

		if( !texdata )
		{
			MsgDev( D_WARN, "LoadStudioModel: couldn't load %s\n", texpath );
			Mem_Free( extradata, C_FILESYSTEM );
			return;
		}

		moddata = (byte *)extradata;
		phdr = (studiohdr_t *)moddata;
		thdr = (studiohdr_t *)texdata;

		// merge textures with main model buffer
		extradata = Mem_Alloc( phdr->length + thdr->length - sizeof( studiohdr_t ), C_FILESYSTEM ); // we don't need two headers
		memcpy( extradata, moddata, phdr->length );
		memcpy((byte *)extradata + phdr->length, texdata + sizeof( studiohdr_t ), thdr->length - sizeof( studiohdr_t ));

		// merge header
		newhdr = (studiohdr_t *)extradata;

		newhdr->numskinfamilies = thdr->numskinfamilies;
		newhdr->numtextures = thdr->numtextures;
		newhdr->numskinref = thdr->numskinref;
		newhdr->textureindex = phdr->length;
		newhdr->skinindex = newhdr->textureindex + ( newhdr->numtextures * sizeof( mstudiotexture_t ));
		newhdr->texturedataindex = newhdr->skinindex + (newhdr->numskinfamilies * newhdr->numskinref * sizeof( short ));
		newhdr->length = phdr->length + thdr->length - sizeof( studiohdr_t );

		// and finally merge datapointers for textures
		for( int i = 0; i < newhdr->numtextures; i++ )
		{
			mstudiotexture_t *ptexture = (mstudiotexture_t *)(((byte *)newhdr) + newhdr->textureindex);
			ptexture[i].index += ( phdr->length - sizeof( studiohdr_t ));
		}

		Mem_Free( moddata, C_FILESYSTEM );
		Mem_Free( texdata, C_FILESYSTEM );
	}

	StudioConstructMesh( ent, extradata, modname, modelCRC, flags );
	Mem_Free( extradata, C_FILESYSTEM );
}

void StudioGetBounds( entity_t *ent, vec3_t mins, vec3_t maxs )
{
	tmesh_t	*mesh = (tmesh_t *)ent->cache;

	// assume point hull
	VectorClear( mins );
	VectorClear( maxs );

	if( !mesh || ent->modtype != mod_studio )
		return;

	VectorSubtract( mesh->absmin, ent->origin, mins );
	VectorSubtract( mesh->absmax, ent->origin, maxs );
}
