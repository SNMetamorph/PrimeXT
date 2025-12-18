//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "utils.h"
#include "const.h"
#include "cdll_int.h"
#include "com_model.h"
#include "entity_types.h"
#include "r_efx.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include <utlarray.h>
#include "gl_local.h"
#include "gl_decals.h"
#include <stringlib.h>
#include "gl_shader.h"
#include "gl_world.h"
#include "gl_studio.h"
#include "gl_occlusion.h"
#include "gl_cvars.h"
#include "brush_material.h"

#define MAX_CLIPVERTS		64	// don't change this
#define MAX_GROUPENTRIES		512
#define MAX_BRUSH_DECALS		4096
#define MAX_DECAL_VERTICES		32	// per one fragment, enough in most cases
#define MAX_DECAL_INDICES		(MAX_DECAL_VERTICES * 3)
#define DECAL_VERTICES_HASH_SIZE	(MAX_DECAL_VERTICES >> 2)

#define MAX_DECAL_VERTS		(MAX_DECAL_VERTICES * MAX_BRUSH_DECALS)
#define MAX_DECAL_ELEMS		(MAX_DECAL_INDICES * MAX_BRUSH_DECALS)

typedef CUtlArray<int> CIntVector;

typedef struct decalVertex_s
{
	Vector			point;
	int			index;
	decalVertex_s*		nextHash;
} decalVertex_t;

// used for build new decals
typedef struct
{
	// decal baseinfo
	short			entityIndex;
	const DecalGroupEntry	*decalDesc;
	model_t			*model;
	byte			flags;
	float			angle;
	brushdecal_t		*current;
	Vector			origin;
	Vector			axis[3];	// up, right, normal

	// decal clipinfo
	Vector			mins, maxs;

	mplane_t			planes[6];
	mplane_t			splitPlanes[2];

	Vector			textureVecs[2];

	word			indices[MAX_DECAL_INDICES];
	word			numIndices;

	// clipped vertices
	decalVertex_t		vertices[MAX_DECAL_VERTICES];
	decalVertex_t		*verticesHashTable[DECAL_VERTICES_HASH_SIZE];
	byte			numVertices;
} decalClip_t;

void DecalGroupEntry :: PreloadTextures( void )
{
	char	path[256];
	char	name[64];

	if( m_init ) return;

	if( m_DecalName[0] == '#' )
		Q_strncpy( name, m_DecalName + 1, sizeof( name ));
	else Q_strncpy( name, m_DecalName, sizeof( name ));

	Q_snprintf( path, sizeof( path ), "gfx/decals/%s", name );
	TextureHandle id = LOAD_TEXTURE( path, NULL, 0, TF_CLAMP );
	if (!id.Initialized())
	{
		// decal was completely missed
		m_init = true;
		return;
	}

	gl_diffuse_id = id;
	if (FBitSet(gl_diffuse_id.GetFlags(), TF_HAS_ALPHA))
		has_alpha = true;

	Q_snprintf( path, sizeof( path ), "gfx/decals/%s_norm", name );
	if( IMAGE_EXISTS( path ))
		gl_normalmap_id = LOAD_TEXTURE( path, NULL, 0, TF_NORMALMAP );
	else gl_normalmap_id = tr.normalmapTexture;

	Q_snprintf( path, sizeof( path ), "gfx/decals/%s_gloss", name );
	if( IMAGE_EXISTS( path ))
		gl_specular_id = LOAD_TEXTURE( path, NULL, 0, TF_CLAMP );
	else gl_specular_id = tr.blackTexture;

	Q_snprintf( path, sizeof( path ), "gfx/decals/%s_hmap", name );
	if( IMAGE_EXISTS( path ))
		gl_heightmap_id = LOAD_TEXTURE( path, NULL, 0, TF_CLAMP );
	else gl_heightmap_id = tr.whiteTexture;

	m_init = true;
}

DecalGroup *pDecalGroupList = NULL;

DecalGroup :: DecalGroup( const char *name, int numelems, DecalGroupEntry *source )
{
	Q_strncpy( m_chGroupName, name, sizeof( m_chGroupName ));

	pEntryArray = new DecalGroupEntry[numelems];
	memcpy( pEntryArray, source, sizeof( DecalGroupEntry ) * numelems );
	size = numelems;

	// setup upcast
	for( int i = 0; i < size; i++ )
		pEntryArray[i].group = this;

	pnext = pDecalGroupList;
	pDecalGroupList = this;
}

DecalGroup :: ~DecalGroup( void )
{
	for( int i = 0; i < size; i++ )
	{
		if( pEntryArray[i].gl_diffuse_id.Initialized() )
			FREE_TEXTURE( pEntryArray[i].gl_diffuse_id );
		if( pEntryArray[i].gl_normalmap_id != tr.normalmapTexture )
			FREE_TEXTURE( pEntryArray[i].gl_normalmap_id );
		if( pEntryArray[i].gl_specular_id != tr.blackTexture )
			FREE_TEXTURE( pEntryArray[i].gl_specular_id );
		if( pEntryArray[i].gl_heightmap_id != tr.whiteTexture )
			FREE_TEXTURE( pEntryArray[i].gl_heightmap_id );
	}

	delete[] pEntryArray;
}

DecalGroupEntry *DecalGroup :: GetEntry( int num )
{
	if( num < 0 || num >= size )
		return NULL;
	return &pEntryArray[num];
}

DecalGroupEntry *DecalGroup :: GetEntry( const char *name, int flags )
{
	if( FBitSet( flags, FDECAL_NORANDOM ) && Q_strchr( name, '@' ))
	{
		// NOTE: restored decal contain name same as 'group@name'
		// so we need separate them before search group
		char *sep = Q_strchr( name, '@' );
		if( sep != NULL ) *sep = '\0';
		char *decalname = sep + 1;

		DecalGroup *groupDesc = FindGroup( name );
		if( !groupDesc )
		{
			ALERT( at_warning, "RestoreDecal: group %s is not exist\n", name );
			return NULL;
                    }

		return groupDesc->FindEntry( decalname );
	}
	else
	{
		DecalGroup *groupDesc = DecalGroup::FindGroup( name );
		if( !groupDesc )
		{
			ALERT( at_warning, "CreateDecal: group %s is not exist\n", name );
			return NULL;
                    }

		return groupDesc->GetRandomDecal();
	}
}

DecalGroupEntry *DecalGroup :: FindEntry( const char *name )
{
	for( int i = 0; i < size; i++ )
	{
		if( !Q_strcmp( pEntryArray[i].m_DecalName, name ))
			return &pEntryArray[i];
	}

	return NULL;
}

DecalGroupEntry *DecalGroup :: GetRandomDecal( void )
{
	return &pEntryArray[RANDOM_LONG( 0, size - 1 )];
}

DecalGroup *DecalGroup :: FindGroup( const char *name )
{
	DecalGroup *plist = pDecalGroupList;

	while( plist )
	{
		if( !Q_strcmp( plist->m_chGroupName, name ))
			return plist;
		plist = plist->pnext;
	}

	return NULL; // nothing found
}

static dvert_t		g_decalVertexCache[MAX_DECAL_VERTS];	// 4.00 mbytes here if max decals count is 4096
static word		g_decalIndexCache[MAX_DECAL_ELEMS];	// 1.5 mbytes here if max decals count is 4096
static unsigned int		g_decalVertUsed;
static unsigned int		g_decalElemUsed;

static brushdecal_t		gDecalPool[MAX_BRUSH_DECALS];
static int		gDecalCycle;
static int		gDecalCount;

// ===========================
// Decals creation
// ===========================
// unlink pdecal from any surface it's attached to
static void R_UnlinkDecal( brushdecal_t *pdecal )
{
	brushdecal_t *tmp;

	if( pdecal->surface )
	{
		mextrasurf_t *es = pdecal->surface;

		ASSERT( es != NULL );

		if( es->pdecals == pdecal )
		{
			es->pdecals = pdecal->pnext;
		}
		else 
		{
			tmp = es->pdecals;
			if( !tmp ) HOST_ERROR( "R_UnlinkDecal: bad decal list\n" );

			while( tmp->pnext ) 
			{
				if( tmp->pnext == pdecal ) 
				{
					tmp->pnext = pdecal->pnext;
					break;
				}
				tmp = tmp->pnext;
			}
		}

		if( FBitSet( es->surf->flags, SURF_REFLECT_PUDDLE ))
		{
			int puddleCount = 0;

			tmp = es->pdecals;
			while( tmp ) 
			{
				if( FBitSet( tmp->flags, FDECAL_PUDDLE ))
					puddleCount++;
				tmp = tmp->pnext;
			}

			if( !puddleCount )
			{
				ClearBits( es->surf->flags, SURF_REFLECT_PUDDLE );
				GL_DeleteOcclusionQuery( es->surf );
			}
		}

		if( !es->pdecals )
			ClearBits( es->surf->flags, SURF_HAS_DECALS );
	}

	// decals are not used dynamic memory, just clear it
	pdecal->surface = NULL;
}

static brushdecal_t *R_AllocDecal( brushdecal_t *pdecal = NULL )
{
	int limit = MAX_BRUSH_DECALS;

	if( r_decals->value < limit )
		limit = r_decals->value;
	
	if( !limit ) return NULL;

	if( !pdecal ) 
	{
		int	count = 0;

		// check for the odd possiblity of infinte loop
		do 
		{
			if( gDecalCycle >= limit )
				gDecalCycle = 0;

			pdecal = &gDecalPool[gDecalCycle]; // reuse next decal
			gDecalCycle++;
			count++;
		} while( FBitSet( pdecal->flags, FDECAL_PERMANENT ) && count < limit );

		// decal allocated
		if( gDecalCount < limit )
			gDecalCount++;
	}

	// if decal is already linked to a surface, unlink it.
	R_UnlinkDecal( pdecal );

	return pdecal;	
}

/*
===============
R_ShaderDecalForward

Select the program for surface (diffuse\puddle)
===============
*/
static word R_ShaderDecalForward( brushdecal_t *decal )
{
	char glname[64];
	char options[MAX_OPTIONS_LENGTH];
	const DecalGroupEntry *texinfo = decal->texinfo;
	mextrasurf_t *es= decal->surface;
	bool mirrorSurface = false;
	bool have_parallax = false;
	bool have_bumpmap = false;
	msurface_t *s = es->surf;

	ASSERT( worldmodel != NULL );

	if( decal->forwardScene.IsValid( ))
		return decal->forwardScene.GetHandle(); // valid

	Q_strncpy( glname, "forward/decal_bmodel", sizeof( glname ));
	memset( options, 0, sizeof( options ));

	if( FBitSet( decal->flags, FDECAL_PUDDLE ))
		GL_AddShaderDirective( options, "DECAL_PUDDLE" );

	if( texinfo->gl_heightmap_id != tr.whiteTexture && texinfo->matdesc->reliefScale > 0.0f )
	{
		if( cv_parallax->value == 1.0f )
			GL_AddShaderDirective( options, "PARALLAX_SIMPLE" );
		else if( cv_parallax->value >= 2.0f )
			GL_AddShaderDirective( options, "PARALLAX_OCCLUSION" );
		have_parallax = true;
	}

	material_t *mat = R_TextureAnimation( s )->material;

	// process lightstyles
	for( int i = 0; i < MAXLIGHTMAPS && s->styles[i] != LS_NONE; i++ )
	{
		if (!R_UseSkyLightstyle(s->styles[i]))
			continue;	// skip the sunlight due realtime sun is enabled
		GL_AddShaderDirective( options, va( "APPLY_STYLE%i", i ));
	}

	if( CVAR_TO_BOOL( cv_brdf ))
		GL_AddShaderDirective( options, "APPLY_PBS" );

	// NOTE: deluxemap and normalmap are separate because some modes may using
	// normalmap directly e.g. for mirror distorsion
	if( es->normals ) GL_AddShaderDirective( options, "HAS_DELUXEMAP" );

	if( CVAR_TO_BOOL( cv_specular ) && ( texinfo->gl_specular_id != tr.blackTexture ))
		GL_AddShaderDirective( options, "HAS_GLOSSMAP" );

	if( !texinfo->has_alpha )
	{
		// GL_DST_COLOR, GL_SRC_COLOR
		GL_AddShaderDirective( options, "APPLY_COLORBLEND" );
	}

	if(( texinfo->gl_normalmap_id != tr.normalmapTexture ) && CVAR_TO_BOOL( cv_bump ))
	{
		GL_AddShaderDirective( options, "HAS_NORMALMAP" );
		GL_EncodeNormal( options, texinfo->gl_normalmap_id );

		if( FBitSet( decal->flags, FDECAL_PUDDLE ))
			GL_AddShaderDirective( options, "APPLY_REFRACTION" );
		have_bumpmap = true;
	}

	if( have_parallax || have_bumpmap )
		GL_AddShaderDirective( options, "COMPUTE_TBN" );

	if( FBitSet( mat->flags, BRUSH_TRANSPARENT ))
		GL_AddShaderDirective( options, "ALPHA_TEST" );

	if( FBitSet( decal->flags, FDECAL_PUDDLE ))
	{
		if( CVAR_TO_BOOL( cv_realtime_puddles ) && CVAR_TO_BOOL( r_allow_mirrors ))
			GL_AddShaderDirective( options, "PLANAR_REFLECTION" );
		else if (!RP_CUBEPASS())
			GL_AddShaderDirective( options, "REFLECTION_CUBEMAP" );		 
	}

	if (tr.fogEnabled && !RP_CUBEPASS())
		GL_AddShaderDirective( options, "APPLY_FOG_EXP" );

	word shaderNum = GL_FindUberShader( glname, options );
	if( !shaderNum ) return 0; // something bad happens

	decal->forwardScene.SetShader( shaderNum );

	return shaderNum;
}

/*
==============================================================================

 DECAL CLIPPING

==============================================================================
*/
/*
==================
R_DecalPointHashKey
==================
*/
static uint R_DecalPointHashKey( const Vector &point, uint hashSize )
{
	uint	hashKey = 0;

	hashKey ^= int( fabs( point.x ));
	hashKey ^= int( fabs( point.y ));
	hashKey ^= int( fabs( point.z ));

	hashKey &= (hashSize - 1);

	return hashKey;
}

/*
==================
R_DecalPointCull
==================
*/
static void R_DecalPointCull( const mplane_t *planes, int numVertices, const bvert_t *vertices, byte *cullBits )
{
	float	d0, d1, d2, d3, d4, d5;
	int	bits;

	for( int i = 0; i < numVertices; i++ )
	{
		bits = 0;

		d0 = PlaneDiff( vertices[i].vertex, &planes[0] );
		d1 = PlaneDiff( vertices[i].vertex, &planes[1] );
		d2 = PlaneDiff( vertices[i].vertex, &planes[2] );
		d3 = PlaneDiff( vertices[i].vertex, &planes[3] );
		d4 = PlaneDiff( vertices[i].vertex, &planes[4] );
		d5 = PlaneDiff( vertices[i].vertex, &planes[5] );

		bits |= FLOATSIGNBITSET( d0 ) << 0;
		bits |= FLOATSIGNBITSET( d1 ) << 1;
		bits |= FLOATSIGNBITSET( d2 ) << 2;
		bits |= FLOATSIGNBITSET( d3 ) << 3;
		bits |= FLOATSIGNBITSET( d4 ) << 4;
		bits |= FLOATSIGNBITSET( d5 ) << 5;

		cullBits[i] = bits;
	}
}

/*
==================
R_ClearDecalClip
==================
*/
static void R_ClearDecalClip( decalClip_t *clip )
{
	if( !clip->numIndices && !clip->numVertices )
		return;

	// clear the hash table
	memset( clip->verticesHashTable, 0, sizeof( clip->verticesHashTable ));

	// clear the arrays
	clip->numVertices = clip->numIndices = 0;
}

/*
==================
R_DecalIntersect

check if this decal is intersected with others
==================
*/
static brushdecal_t *R_DecalIntersect( decalClip_t *clip, msurface_t *surf )
{
	float texc_orig_x = DotProduct( clip->textureVecs[0], clip->origin );
	float texc_orig_y = DotProduct( clip->textureVecs[1], clip->origin );
	float xsize = (float)clip->decalDesc->ysize;
	float ysize = (float)clip->decalDesc->xsize;
	brushdecal_t *pDecal = surf->info->pdecals;
	float overlay = clip->decalDesc->overlay;

	while( pDecal ) 
	{
		if( !FBitSet( pDecal->flags, FDECAL_PERMANENT ))
		{
			if(( pDecal->texinfo->xsize == xsize ) && ( pDecal->texinfo->ysize == ysize ))
			{
				float texc_x = fabs( DotProduct( pDecal->position, clip->textureVecs[0] ) - texc_orig_x );
				float texc_y = fabs( DotProduct( pDecal->position, clip->textureVecs[1] ) - texc_orig_y );

				// replace existed decal
				if( texc_x < ( xsize * overlay ) && texc_y < ( ysize * overlay ))
					return pDecal;
			}
		}

		pDecal = pDecal->pnext;
	}

	return NULL;
}

static void R_DecalComputeTBN( brushdecal_t *decal )
{
	if( decal->texinfo->gl_normalmap_id == tr.normalmapTexture && decal->texinfo->gl_heightmap_id == tr.whiteTexture )
		return; // not needs

	// build a map from vertex to a list of triangles that share the vert.
	CUtlArray<CIntVector> vertToTriMap;

	vertToTriMap.AddMultipleToTail( decal->numVerts );
	int triID;

	for( triID = 0; triID < (decal->numElems / 3); triID++ )
	{
		vertToTriMap[decal->elems[triID*3+0]].AddToTail( triID );
		vertToTriMap[decal->elems[triID*3+1]].AddToTail( triID );
		vertToTriMap[decal->elems[triID*3+2]].AddToTail( triID );
	}

	// calculate the tangent space for each triangle.
	CUtlArray<Vector> triSVect, triTVect;
	triSVect.AddMultipleToTail( (decal->numElems / 3) );
	triTVect.AddMultipleToTail( (decal->numElems / 3) );

	float	*v[3], *tc[3];

	for( triID = 0; triID < (decal->numElems / 3); triID++ )
	{
		for( int i = 0; i < 3; i++ )
		{
			v[i] = (float *)&decal->verts[decal->elems[triID*3+i]].vertex;
			tc[i] = (float *)&decal->verts[decal->elems[triID*3+i]].stcoord0;
		}

		CalcTBN( v[0], v[1], v[2], tc[0], tc[1], tc[2], triSVect[triID], triTVect[triID] );
	}	

	// calculate an average tangent space for each vertex.
	for( int vertID = 0; vertID < decal->numVerts; vertID++ )
	{
		dvert_t *v = &decal->verts[vertID];
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
	
/*
==================
R_AddDecal
==================
*/
static void R_AddDecal( decalClip_t *clip, msurface_t *surf )
{
	if( !clip->numIndices || !clip->numVertices )
		return;

	DecalGroupEntry *entry = (DecalGroupEntry *)clip->decalDesc;

	entry->PreloadTextures(); // time to cache decal textures

	if( !entry->gl_diffuse_id.Initialized() )
	{
		// decal texture was missed?
		R_ClearDecalClip( clip );
		return;
	}

	brushdecal_t *newdecal = NULL;
	brushdecal_t *olddecal = NULL;

	if( FBitSet( clip->flags, FDECAL_PERMANENT ))
	{
		newdecal = R_AllocDecal();
	}
	else
	{
		if( !clip->current ) // don't intersect with self-fragments!
			olddecal = R_DecalIntersect( clip, surf );
		newdecal = R_AllocDecal( olddecal );
	}

	if( !newdecal )
	{
		ALERT( at_error, "MAX_BRUSH_DECALS limit exceeded!\n" );
		R_ClearDecalClip( clip );
		return;
	}

	// puddles required reflections
	if( FBitSet( clip->flags, FDECAL_PUDDLE ) && clip->decalDesc->matdesc->reflectScale > 0.0f )
	{
		SetBits( surf->flags, SURF_REFLECT_PUDDLE );
		GL_AllocOcclusionQuery( surf );
	}

	// now our surface has decals
	SetBits( surf->flags, SURF_HAS_DECALS );

	newdecal->pnext = NULL;
	olddecal = surf->info->pdecals;

	// place decal at tail of list
	if( olddecal ) 
	{
		while( olddecal->pnext ) 
			olddecal = olddecal->pnext;
		olddecal->pnext = newdecal;
	}
	else
	{
		// first decal in chain
		surf->info->pdecals = newdecal;
	}

	// tag surface
	newdecal->surface = surf->info;
	newdecal->position = clip->origin;
	newdecal->impactPlaneNormal = clip->axis[2];
	newdecal->model = clip->model;
	newdecal->entityIndex = clip->entityIndex;
	newdecal->texinfo = clip->decalDesc;
	newdecal->flags = clip->flags;
	newdecal->angle = clip->angle;

	// NOTE: any decal may potentially fragmented to multiple parts
	// don't save all the fragments because we don't need them to restore this right
	// only once (any) fragment needs to be saved to full restore all the fragments
	if( clip->current )
		SetBits( newdecal->flags, FDECAL_DONTSAVE );
	else clip->current = newdecal;

	// init vertex cache
	if( newdecal->numVerts < clip->numVertices )
	{
		// FIXME: cache realloc provoked 'leaks' in cache array
		if( newdecal->numVerts )
			ALERT( at_aiconsole, "R_AddDecal: vertex cache was reallocated (%i < %i)\n", newdecal->numVerts, clip->numVertices );
		newdecal->verts = &g_decalVertexCache[g_decalVertUsed];
		g_decalVertUsed += clip->numVertices;
		newdecal->numVerts = clip->numVertices;
	}

	// init index cache
	if( newdecal->numElems < clip->numIndices )
	{
		// FIXME: cache realloc provoked 'leaks' in cache array
		if( newdecal->numElems )
			ALERT( at_aiconsole, "R_AddDecal: index cache was reallocated (%i < %i)\n", newdecal->numElems, clip->numIndices );
		newdecal->elems = &g_decalIndexCache[g_decalElemUsed];
		g_decalElemUsed += clip->numIndices;
		newdecal->numElems = clip->numIndices;
	}

	// Copy the indices
	memcpy( newdecal->elems, clip->indices, clip->numIndices * sizeof( word ));

	mtexinfo_t *tex = surf->texinfo;

	// set up the vertices
	for( int i = 0; i < clip->numVertices; i++ )
	{
		dvert_t *v = &newdecal->verts[i];

		Vector point = clip->vertices[i].point;
		Vector delta = point - clip->origin;

		v->stcoord0[0] = DotProduct( clip->textureVecs[0], delta ) + 0.5f;
		v->stcoord0[1] = DotProduct( clip->textureVecs[1], delta ) + 0.5f;
		v->stcoord0[2] = (( DotProduct( point, tex->vecs[0] ) + tex->vecs[0][3] ) / tex->texture->width );
		v->stcoord0[3] = (( DotProduct( point, tex->vecs[1] ) + tex->vecs[1][3] ) / tex->texture->height );
		R_LightmapCoords( surf, point, newdecal->verts[i].lmcoord0, 0 ); // styles 0-1
		R_LightmapCoords( surf, point, newdecal->verts[i].lmcoord1, 2 ); // styles 2-3
		// NOTE: i can to place styles is outside but i'm leave it here to keep vertex aligned
		memcpy( v->styles, surf->styles, sizeof( surf->styles ));
		v->vertex = point;
		if( FBitSet( surf->flags, SURF_PLANEBACK ))
			v->normal = -surf->plane->normal;
		else v->normal = surf->plane->normal;
	}

	R_DecalComputeTBN( newdecal );
	R_ClearDecalClip( clip );
}

/*
==================
R_AddDecalFragment
==================
*/
static void R_AddDecalFragment( decalClip_t *clip, msurface_t *fa, const bvert_t *verts, int numPoints, const Vector *points )
{
	int		index, indices[3];
	decalVertex_t	*vertex;
	uint		hashKey;

	// for each triangle in the fragment
	for( int i = 0; i < numPoints - 2; i++ )
	{
		indices[0] = 0;
		indices[1] = i + 1;
		indices[2] = i + 2;

		// If we're going to overflow, add all the previous triangles to a separate decal
		if(( clip->numIndices + 3 ) > MAX_DECAL_INDICES || ( clip->numVertices + 3 ) > MAX_DECAL_VERTICES )
			R_AddDecal( clip, fa );

		// add the triangle
		for( int j = 0; j < 3; j++ )
		{
			index = indices[j];

			// Check if this vertex already exists
			hashKey = R_DecalPointHashKey( points[index], DECAL_VERTICES_HASH_SIZE );

			for( vertex = clip->verticesHashTable[hashKey]; vertex; vertex = vertex->nextHash )
			{
				if( vertex->point.IsEqual( points[index], 0.01f ))
					break;
			}

			// reuse an existing vertex or add a new one
			if( !vertex )
			{
				clip->indices[clip->numIndices++] = clip->numVertices;

				// add a new vertex
				clip->vertices[clip->numVertices].point = points[index];
				clip->vertices[clip->numVertices].index = clip->numVertices;

				clip->vertices[clip->numVertices].nextHash = clip->verticesHashTable[hashKey];
				clip->verticesHashTable[hashKey] = &clip->vertices[clip->numVertices];

				clip->numVertices++;
			}
			else clip->indices[clip->numIndices++] = vertex->index;
		}
	}
}

/*
==================
R_ClipTriangleToDecal
==================
*/
static void R_ClipTriangleToDecal( decalClip_t *clip, msurface_t *fa, int v0, int v1, int v2, const bvert_t *verts, int planeBits )
{
	Vector	points[2][MAX_CLIPVERTS];
	Vector	front[MAX_CLIPVERTS];
	int	numFront, pingPong = 0;
	int	numPoints;

	// clip the triangle to the decal
	numPoints = 3;

	points[pingPong][0] = verts[v0].vertex;
	points[pingPong][1] = verts[v1].vertex;
	points[pingPong][2] = verts[v2].vertex;

	for( int i = 0; i < 6; i++ )
	{
		if( !FBitSet( planeBits, BIT( i )))
			continue;

		if( !R_ClipPolygon( numPoints, points[pingPong], clip->planes + i, &numPoints, points[!pingPong] ))
			return;

		pingPong ^= 1;
	}

	if( numPoints < 3 ) return;

	// add the fragment at the front of the first split plane
	R_SplitPolygon( numPoints, points[pingPong], &clip->splitPlanes[0], &numFront, front, &numPoints, points[!pingPong] );

	R_AddDecalFragment( clip, fa, verts, numFront, front );

	// add the fragment at the front of the second split plane
	R_SplitPolygon( numPoints, points[!pingPong], &clip->splitPlanes[1], &numFront, front, &numPoints, points[pingPong] );

	R_AddDecalFragment( clip, fa, verts, numFront, front );

	// add the fragment at the back of both split planes
	R_AddDecalFragment( clip, fa, verts, numPoints, points[pingPong] );
}

/*
==================
R_ClipSurfaceToDecal
==================
*/
static void R_ClipSurfaceToDecal( decalClip_t *clip, msurface_t *fa )
{
	mextrasurf_t	*esrf = fa->info;
	bvert_t		*verts = &world->vertexes[esrf->firstvertex];
	byte		cullBits[MAX_CLIPVERTS];
	int		v0, v1, v2;

	ASSERT( esrf->numverts < MAX_CLIPVERTS );

	if( FBitSet( fa->flags, SURF_PLANEBACK ))
	{
		if( DotProduct( clip->axis[2], fa->plane->normal ) > 0.0f )
			return; // facing away
	}
	else
	{
		if( DotProduct( clip->axis[2], fa->plane->normal ) < 0.0f )
			return; // facing away
	}

	// Categorize all points by the planes
	R_DecalPointCull( clip->planes, esrf->numverts, verts, cullBits );

	// clip the surface
	for( int i = 0; i < esrf->numverts - 2; i++ )
	{
		v0 = 0;
		v1 = i + 1;
		v2 = i + 2;

		if( cullBits[v0] & cullBits[v1] & cullBits[v2] )
			continue;	// completely off one side

		// calculate two mostly perpendicular edge directions
		Vector dir1 = verts[v0].vertex - verts[v1].vertex;
		Vector dir2 = verts[v2].vertex - verts[v1].vertex;

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the triangle normal
		Vector snorm = CrossProduct( dir1, dir2 ).Normalize();

		// we multiply 0.5 by length of snorm to avoid normalizing
		if( DotProduct( clip->axis[2], snorm ) < 0.0 )
			continue; // greater than 90 degrees

		// clip the triangle to the decal
		R_ClipTriangleToDecal( clip, fa, v0, v1, v2, verts, cullBits[v0]|cullBits[v1]|cullBits[v2] );
	}

	// add a new decal if needed
	R_AddDecal( clip, fa );
}

static void R_DecalNodeSurfaces( mnode_t *node, decalClip_t *clip )
{
	// iterate over all surfaces in the node
	msurface_t *surf = worldmodel->surfaces + node->firstsurface;

	for( int i = 0; i < node->numsurfaces; i++, surf++ ) 
	{
		mextrasurf_t *esrf = surf->info;

		// never apply decals on the water or sky surfaces
		if( FBitSet( surf->flags, ( SURF_DRAWTURB|SURF_DRAWSKY|SURF_CONVEYOR|SURF_DRAWTILED )))
			continue;

		// no puddles on transparent surfaces, mirrors, screens, portals
		if( FBitSet( clip->flags, FDECAL_PUDDLE ) && FBitSet( surf->flags, ( SURF_TRANSPARENT | SURF_REFLECT | SURF_SCREEN | SURF_PORTAL )))
			continue;

		if( !BoundsIntersect( esrf->mins, esrf->maxs, clip->mins, clip->maxs ))
			continue;

		R_ClipSurfaceToDecal( clip, surf );
	}
}

static void R_DecalNode( mnode_t *node, decalClip_t *clip )
{
	ASSERT( node != NULL );

	// hit a leaf
	if( node->contents < 0 )
		return;

	int s = BOX_ON_PLANE_SIDE( clip->mins, clip->maxs, node->plane );

	if( s == 3 ) R_DecalNodeSurfaces( node, clip );
	if( s & 1 ) R_DecalNode( node->children[0], clip );
	if( s & 2 ) R_DecalNode( node->children[1], clip );
}

void CreateDecal(const Vector &vecEndPos, const Vector &vecPlaneNormal, float angle, const char *name, int flags, int entityIndex, int modelIndex, bool source)
{
	int	srcFlags = flags;

	if( !pDecalGroupList )
		return;

	// probably this never happens
	if(( g_decalVertUsed >= MAX_DECAL_VERTS ) || ( g_decalElemUsed >= MAX_DECAL_ELEMS ))
	{
		ALERT( at_error, "CreateDecal: cache decal is overflow!\n" );
		return;
	}

	decalClip_t decalClip; // intermediate struct that used only for build new decals
	cl_entity_t *ent = NULL;

	decalClip.decalDesc = DecalGroup::GetEntry( name, flags );
	if( !decalClip.decalDesc ) return;

	// g-cont. allow more groups that starts from 'puddle'
	if( !Q_strnicmp( name, "puddle", 6 ))
		SetBits( flags, FDECAL_PUDDLE );

	// puddles allowed only at floor surfaces
	if( FBitSet( flags, FDECAL_PUDDLE ) && vecPlaneNormal != Vector( 0.0f, 0.0f, 1.0f ))
		return;

	decalClip.model = NULL;

	if( entityIndex > 0 )
	{
		ent = GET_ENTITY( entityIndex );

		if( modelIndex > 0 )
			decalClip.model = MODEL_HANDLE( modelIndex );
		else if( ent != NULL )
			decalClip.model = MODEL_HANDLE( ent->curstate.modelindex );
		else return;
	}
	else if( modelIndex > 0 )
		decalClip.model = MODEL_HANDLE( modelIndex );
	else decalClip.model = worldmodel;

	if( !decalClip.model ) return;
	
	if( decalClip.model->type != mod_brush )
	{
		ALERT( at_error, "Decals must hit mod_brush!\n" );
		return;
	}

	if( ent && !FBitSet( flags, FDECAL_LOCAL_SPACE ))
	{
		// transform decal position in local bmodel space
		if( ent->angles != g_vecZero )
		{
			matrix4x4	matrix( ent->origin, ent->angles );

			// transfrom decal position into local space
			decalClip.origin = matrix.VectorITransform( vecEndPos );
			decalClip.axis[2] = matrix.VectorIRotate( vecPlaneNormal );
		}
		else
		{
			decalClip.origin = vecEndPos - ent->origin;
			decalClip.axis[2] = vecPlaneNormal;
		}

		flags |= FDECAL_LOCAL_SPACE; // decal position moved into local space
	}
	else
	{
		// pass position in global
		decalClip.origin = vecEndPos;
		decalClip.axis[2] = vecPlaneNormal;
	}

	// this decal must use landmark for correct transition
	// a models with origin brush just use local space
	if( !FBitSet( decalClip.model->flags, MODEL_HAS_ORIGIN ))
		flags |= FDECAL_USE_LANDMARK;

	// just for consistency
	RI->currententity = GET_ENTITY( entityIndex );
	RI->currentmodel = RI->currententity->model;

	// don't allow random decal select on a next save\restore
	SetBits( flags, FDECAL_NORANDOM );

	int xsize = decalClip.decalDesc->xsize;
	int ysize = decalClip.decalDesc->ysize;
	float s, c, depth = 1.0f;
	matrix3x4	transform;
	Vector up, right;

	// Compute orientation
	SinCos( DEG2RAD( anglemod( angle )), &s, &c );
	VectorMatrix( decalClip.axis[2], right, up );
	right = -right;

	decalClip.axis[0] = (up * c) + (right * s);
	decalClip.axis[1] = (right * c) - (up * s);

	decalClip.mins.x = -xsize;
	decalClip.mins.y = -ysize;
	decalClip.mins.z = -depth;
	decalClip.maxs.x = xsize;
	decalClip.maxs.y = ysize;
	decalClip.maxs.z = depth;
	decalClip.current = NULL;
	decalClip.angle = angle;

	// need to properly transform decal bbox
	transform.SetForward( decalClip.axis[0] );
	transform.SetRight( decalClip.axis[1] );
	transform.SetUp( decalClip.axis[2] );
	transform.SetOrigin( decalClip.origin );
	TransformAABB( transform, decalClip.mins, decalClip.maxs, decalClip.mins, decalClip.maxs );

	// set up the clip planes
	SetPlane( &decalClip.planes[0], decalClip.axis[0], DotProduct( decalClip.origin, decalClip.axis[0] ) - xsize );
	SetPlane( &decalClip.planes[1],-decalClip.axis[0],-DotProduct( decalClip.origin, decalClip.axis[0] ) - xsize );
	SetPlane( &decalClip.planes[2], decalClip.axis[1], DotProduct( decalClip.origin, decalClip.axis[1] ) - ysize );
	SetPlane( &decalClip.planes[3],-decalClip.axis[1],-DotProduct( decalClip.origin, decalClip.axis[1] ) - ysize );
	SetPlane( &decalClip.planes[4], decalClip.axis[2], DotProduct( decalClip.origin, decalClip.axis[2] ) - depth );
	SetPlane( &decalClip.planes[5],-decalClip.axis[2],-DotProduct( decalClip.origin, decalClip.axis[2] ) - depth );

	// set up the split planes
	SetPlane( &decalClip.splitPlanes[0], decalClip.axis[2],  DotProduct( decalClip.origin, decalClip.axis[2] ) + ( depth * 0.5f ));
	SetPlane( &decalClip.splitPlanes[1],-decalClip.axis[2], -DotProduct( decalClip.origin, decalClip.axis[2] ) - ( depth * 0.5f ) + depth );

	// compute the texture vectors
	decalClip.textureVecs[0] = decalClip.axis[1] * ( 0.5f / ysize );
	decalClip.textureVecs[1] = decalClip.axis[0] * ( 0.5f / xsize );

	// Clear the arrays
	decalClip.numIndices = 0;
	decalClip.numVertices = 0;
	decalClip.entityIndex = entityIndex;
	decalClip.flags = flags;

	// clear the hash table
	memset( decalClip.verticesHashTable, 0, sizeof( decalClip.verticesHashTable ));

	// g-cont. now using walking on bsp-tree instead of stupid linear search
	R_DecalNode( &decalClip.model->nodes[decalClip.model->hulls[0].firstclipnode], &decalClip );
	if( !source ) return; // to avoid recursion

	// trying to place decals on contacted submodels too
	// FIXME: this is doesn't working
	for( int i = 0; i < 1024; i++ )
	{
		model_t *mod = MODEL_HANDLE( i );

		if( !mod || mod->type != mod_brush || mod == decalClip.model )
			continue;

		if( mod->firstmodelsurface >= worldmodel->numsurfaces || !mod->nummodelsurfaces )
			continue;	// skip weird models

		int entityIndex = 0;

		if( mod->name[0] == '*' )
		{
			msurface_t *surf = mod->surfaces + mod->firstmodelsurface;
			cl_entity_t *e = surf->info->parent;
			if( !e ) continue; // entity invisible on the client

			gl_state_t *glm = GL_GetCache( e->hCachedMatrix );
			Vector absmin, absmax;

			TransformAABB( glm->transform, mod->mins, mod->maxs, absmin, absmax );

			if( !BoundsIntersect( absmin, absmax, vecEndPos, vecEndPos ))
				continue;	// no intersection with this model
			entityIndex = e->index;
		}

		// trying to place decal on neighbored bmodel
		CreateDecal( vecEndPos, vecPlaneNormal, angle, name, srcFlags, entityIndex, 0, false );
	}
}

void CreateDecal( pmtrace_t *tr, const char *name, float angle, bool visent )
{
	if( !g_fRenderInitialized )
		return;

	if( tr->allsolid || tr->fraction == 1.0 || tr->ent < 0 )
		return;

	physent_t *pe = NULL;

	if( visent ) pe = gEngfuncs.pEventAPI->EV_GetVisent( tr->ent );
 	else pe = gEngfuncs.pEventAPI->EV_GetPhysent( tr->ent );
	if( !pe ) return;
 
	int entityIndex = pe->info;
	int modelIndex = 0;

	// modelindex is needs for properly save\restore
	cl_entity_t *ent = GET_ENTITY( entityIndex );
	if( ent ) modelIndex = ent->curstate.modelindex;

	CreateDecal( tr->endpos, tr->plane.normal, angle, name, 0, entityIndex, modelIndex );
}

// debugging feature
void PasteViewDecal( void )
{
	const char *name = CMD_ARGV( 1 );
	float angle = 0.0f;

	if( CMD_ARGC() <= 1 )
	{
		Msg( "usage: pastedecal <decal name>\n" );
		return;
	}

	pmtrace_t *tr = gEngfuncs.pEventAPI->EV_VisTraceLine( GetVieworg(), (GetVieworg() + (GetVForward() * 1024.0f)), PM_NORMAL );
	if( fabs( tr->plane.normal.z ) > 0.7f )
		angle = RI->view.angles.y + 90.0f;
	physent_t *pe = gEngfuncs.pEventAPI->EV_GetVisent( tr->ent );

	// now handle both types: studio and brushes
	if( pe && pe->studiomodel )
		g_StudioRenderer.StudioDecalShoot( tr, name, true );
	else CreateDecal( tr, name, angle, true );
}

int SaveDecalList( decallist_t *pBaseList, int count )
{
	int maxBrushDecals = MAX_BRUSH_DECALS + (MAX_BRUSH_DECALS - count);
	decallist_t *pList = pBaseList + count;	// shift list to first free slot
	brushdecal_t *pdecal, *pdecals;
	int total = 0, depth;

	for( int i = 0; i < gDecalCount; i++ )
	{
		pdecal = &gDecalPool[i];
		const DecalGroupEntry *tex = pdecal->texinfo;

		if( !pdecal->surface || !tex || !tex->group )
			continue; // unused

		if( FBitSet( pdecal->flags, FDECAL_DONTSAVE ))	
			continue;

		// compute depth
		depth = 0;
		pdecals = pdecal->surface->pdecals;

		while( pdecals && pdecals != pdecal )
		{
			depth++;
			pdecals = pdecals->pnext;
		}

		pList[total].depth = depth;
		pList[total].flags = pdecal->flags;
		pList[total].entityIndex = pdecal->entityIndex;
		pList[total].position = pdecal->position;
		pList[total].impactPlaneNormal = pdecal->impactPlaneNormal;
		pList[total].scale = pdecal->angle;
		Q_snprintf( pList[total].name, sizeof( pList[total].name ), "%s@%s", tex->group->GetName(), tex->m_DecalName );
		total++;

		// check for list overflow
		if( total >= maxBrushDecals )
		{
			ALERT( at_error, "SaveDecalList: too many brush decals on save\restore\n" );
			goto end_serialize;
		}
	}
end_serialize:
	return total;
}

inline void R_DecalSetupVertex( dvert_t *vert )
{
	pglVertexAttrib4fvARB( ATTR_INDEX_TANGENT, vert->tangent );
	pglVertexAttrib4fvARB( ATTR_INDEX_BINORMAL, vert->binormal );
	pglVertexAttrib4fvARB( ATTR_INDEX_NORMAL, vert->normal );
	pglVertexAttrib4fvARB( ATTR_INDEX_TEXCOORD0, vert->stcoord0 );
	pglVertexAttrib4fvARB( ATTR_INDEX_TEXCOORD1, vert->lmcoord0 );
	pglVertexAttrib4fvARB( ATTR_INDEX_TEXCOORD2, vert->lmcoord1 );
	pglVertexAttrib4ubvARB( ATTR_INDEX_LIGHT_STYLES, vert->styles );
	pglVertexAttrib3fvARB( ATTR_INDEX_POSITION, vert->vertex );
}

/*
================
R_SetDecalUniforms

================
*/
void R_SetDecalUniforms( brushdecal_t *decal )
{
	mextrasurf_t *es = decal->surface;
	cl_entity_t *e = es->parent;
	msurface_t *s = es->surf;
	int map, width, height;
	Vector4D lightstyles;

	// begin draw the sorted list
	if( RI->currentshader != decal->forwardScene.GetShader() )
	{
		// force to bind new shader
		GL_BindShader( decal->forwardScene.GetShader() );
	}

	material_t *mat = R_TextureAnimation( es->surf )->material;
	gl_state_t *glm = GL_GetCache( e->hCachedMatrix );
	glsl_program_t *shader = RI->currentshader;
	matdesc_t *desc = decal->texinfo->matdesc;
	GLfloat viewMatrix[16];

	tr.modelorg = glm->GetModelOrigin();

	// setup specified uniforms (and texture bindings)
	for( int i = 0; i < shader->numUniforms; i++ )
	{
		uniform_t *u = &shader->uniforms[i];

		switch( u->type )
		{
		case UT_COLORMAP:
			if( Surf_CheckSubview( es, true ) && FBitSet( decal->flags, FDECAL_PUDDLE ))
				u->SetValue( Surf_GetSubview( es )->texturenum.ToInt() );
			else u->SetValue( mat->impl->gl_diffuse_id.ToInt() );
			break;
		case UT_NORMALMAP:
			u->SetValue( decal->texinfo->gl_normalmap_id.ToInt() );
			break;
		case UT_GLOSSMAP:
			u->SetValue( decal->texinfo->gl_specular_id.ToInt() );
			break;
		case UT_LIGHTMAP:
			if( R_FullBright( )) 
				u->SetValue( tr.deluxemapTexture.ToInt() );
			else 
				u->SetValue( tr.lightmaps[es->lightmaptexturenum].lightmap.ToInt() );
			break;
		case UT_DELUXEMAP:
			if( R_FullBright( )) 
				u->SetValue( tr.grayTexture.ToInt() );
			else 
				u->SetValue( tr.lightmaps[es->lightmaptexturenum].deluxmap.ToInt() );
			break;
		case UT_DECALMAP:
			u->SetValue( decal->texinfo->gl_diffuse_id.ToInt() );
			break;
		case UT_SCREENMAP:
			u->SetValue( tr.screen_color.ToInt() );
			break;
		case UT_DEPTHMAP:
			u->SetValue( tr.screen_depth.ToInt() );
			break;
		case UT_HEIGHTMAP:
			u->SetValue( decal->texinfo->gl_heightmap_id.ToInt() );
			break;
		case UT_ENVMAP0:
		case UT_ENVMAP:
			if (!RP_CUBEPASS() && es->cubemap[0] != NULL)
			{
				u->SetValue(es->cubemap[0]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_ENVMAP1:
			if (!RP_CUBEPASS() && es->cubemap[1] != NULL) {
				u->SetValue(es->cubemap[1]->texture.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.texture.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL0:
			if (!RP_CUBEPASS() && es->cubemap[0] != NULL) {
				u->SetValue(es->cubemap[0]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_SPECULARMAPIBL1:
			if (!RP_CUBEPASS() && es->cubemap[1] != NULL) {
				u->SetValue(es->cubemap[1]->textureSpecularIBL.ToInt());
			}
			else {
				u->SetValue(world->defaultCubemap.textureSpecularIBL.ToInt());
			}
			break;
		case UT_BRDFAPPROXMAP:
			u->SetValue( tr.brdfApproxTexture.ToInt() );
			break;
		case UT_FITNORMALMAP:
			u->SetValue( tr.normalsFitting.ToInt() );
			break;
		case UT_MODELMATRIX:
			u->SetValue( &glm->modelMatrix[0] );
			break;
		case UT_REFLECTMATRIX:
			if( Surf_CheckSubview( es, true ))
				Surf_GetSubview( es )->matrix.CopyToArray( viewMatrix );
			else memcpy( viewMatrix, glState.identityMatrix, sizeof( float ) * 16 );
			u->SetValue( &viewMatrix[0] );
			break;
		case UT_SCREENSIZEINV:
			u->SetValue( 1.0f / (float)glState.width, 1.0f / (float)glState.height );
			break;
		case UT_ZFAR:
			u->SetValue( -tr.farclip * 1.74f );
			break;
		case UT_LIGHTGAMMA:
			u->SetValue( tr.light_gamma );
			break;
		case UT_LIGHTSTYLES:
			for( map = 0; map < MAXLIGHTMAPS; map++ )
			{
				if( s->styles[map] != 255 )
					lightstyles[map] = tr.lightstyle[s->styles[map]];
				else lightstyles[map] = 0.0f;
			}
			u->SetValue( lightstyles.x, lightstyles.y, lightstyles.z, lightstyles.w );
			break;
		case UT_LIGHTSTYLEVALUES:
			u->SetValue( &tr.lightstyle[0], MAX_LIGHTSTYLES );
			break;
		case UT_REALTIME:
			u->SetValue( (float)tr.time );
			break;
		case UT_FOGPARAMS:
			u->SetValue( tr.fogColor[0], tr.fogColor[1], tr.fogColor[2], tr.fogDensity );
			break;
		case UT_TEXOFFSET:
			u->SetValue( es->texofs[0], es->texofs[1] );
			break;
		case UT_VIEWORIGIN:
			u->SetValue( tr.modelorg.x, tr.modelorg.y, tr.modelorg.z, e->hCachedMatrix ? 1.0f : 0.0f );
			break;
		case UT_SMOOTHNESS:
			u->SetValue( desc->smoothness );
			break;
		case UT_AMBIENTFACTOR:
			u->SetValue( tr.ambientFactor );
			break;
		case UT_REFRACTSCALE:
			u->SetValue( bound( 0.0f, desc->refractScale, 1.0f ));
			break;
		case UT_REFLECTSCALE:
			u->SetValue( bound( 0.0f, desc->reflectScale, 1.0f ));
			break;
		case UT_ABERRATIONSCALE:
			u->SetValue( bound( 0.0f, desc->aberrationScale, 1.0f ));
			break;
		case UT_LERPFACTOR:
			u->SetValue( es->lerpFactor );
			break;
		case UT_BOXMINS:
			if( world->num_cubemaps > 0 )
			{
				Vector mins[2];
				mins[0] = es->cubemap[0]->mins;
				mins[1] = es->cubemap[1]->mins;
				u->SetValue( &mins[0][0], 2 );
			}
			break;
		case UT_BOXMAXS:
			if( world->num_cubemaps > 0 )
			{
				Vector maxs[2];
				maxs[0] = es->cubemap[0]->maxs;
				maxs[1] = es->cubemap[1]->maxs;
				u->SetValue( &maxs[0][0], 2 );
			}
			break;
		case UT_CUBEORIGIN:
			if( world->num_cubemaps > 0 )
			{
				Vector origin[2];
				origin[0] = es->cubemap[0]->origin;
				origin[1] = es->cubemap[1]->origin;
				u->SetValue( &origin[0][0], 2 );
			}
			break;
		case UT_CUBEMIPCOUNT:
			if( world->num_cubemaps > 0 )
			{
				float r = Q_max( 1, es->cubemap[0]->numMips - cv_cube_lod_bias->value );
				float g = Q_max( 1, es->cubemap[1]->numMips - cv_cube_lod_bias->value );
				u->SetValue( r, g );
			}
			break;
		case UT_LIGHTNUMS0:
			u->SetValue( (float)es->lights[0], (float)es->lights[1], (float)es->lights[2], (float)es->lights[3] );
			break;
		case UT_LIGHTNUMS1:
			u->SetValue( (float)es->lights[4], (float)es->lights[5], (float)es->lights[6], (float)es->lights[7] );
			break;
		case UT_RELIEFPARAMS:
			width = decal->texinfo->gl_heightmap_id.GetWidth();
			height = decal->texinfo->gl_heightmap_id.GetHeight();
			u->SetValue( (float)width, (float)height, desc->reliefScale, cv_shadow_offset->value );
			break;
		case UT_SWAYHEIGHT:
			u->SetValue(desc->swayHeight);
			break;
		default:
			ALERT( at_error, "%s: unhandled uniform %s\n", RI->currentshader->name, u->name );
			break;
		}
	}
}

static void DrawWireDecal( brushdecal_t *decal )
{
	mextrasurf_t *es = decal->surface;
	cl_entity_t *e = es->parent;

	pglEnable( GL_BLEND );
	pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	if( FBitSet( decal->flags, FDECAL_DONTSAVE ))
		pglColor4f( 1.0f, 0.5f, 0.36f, 0.99f ); 
	else pglColor4f( 0.5f, 1.0f, 0.36f, 0.99f ); 
	pglLineWidth( 4.0f );

	pglDisable( GL_DEPTH_TEST );
	pglEnable( GL_LINE_SMOOTH );
	pglEnable( GL_POLYGON_SMOOTH );
	pglHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
	pglHint( GL_POLYGON_SMOOTH_HINT, GL_NICEST );

	gl_state_t *glm = GL_GetCache( e->hCachedMatrix );

	// transform decal verts to actual position
	for( int k = 0; k < decal->numElems; k += 3 )
	{
		pglBegin( GL_TRIANGLES );
			pglVertex3fv( glm->transform.VectorTransform( decal->verts[decal->elems[k+0]].vertex ));
			pglVertex3fv( glm->transform.VectorTransform( decal->verts[decal->elems[k+1]].vertex ));
			pglVertex3fv( glm->transform.VectorTransform( decal->verts[decal->elems[k+2]].vertex ));
		pglEnd();
	}

	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	pglDisable( GL_POLYGON_SMOOTH );
	pglDisable( GL_LINE_SMOOTH );
	pglEnable( GL_DEPTH_TEST );
	pglDisable( GL_BLEND );
	pglLineWidth( 1.0f );
}

static void R_DrawSurfaceDecalsDebug( msurface_t *surf, drawlist_t drawlist_type )
{
	mextrasurf_t *fa = surf->info;
	brushdecal_t *p;

	for( p = fa->pdecals; p; p = p->pnext )
	{
		if( !p->surface || !p->texinfo )
			continue; // bad decal?

		if( drawlist_type == DRAWLIST_SOLID && !p->texinfo->has_alpha )
			continue;

		if( drawlist_type == DRAWLIST_TRANS && p->texinfo->has_alpha )
			continue;

		DrawWireDecal( p );
	}
}

static void R_RenderSurfaceDecals( msurface_t *surf, drawlist_t drawlist_type )
{
	mextrasurf_t *fa = surf->info;
	brushdecal_t *p;

	for( p = fa->pdecals; p; p = p->pnext )
	{
		if( !p->surface || !p->texinfo )
			continue; // bad decal?

		if( drawlist_type == DRAWLIST_SOLID && !p->texinfo->has_alpha )
			continue;

		if( drawlist_type == DRAWLIST_TRANS && p->texinfo->has_alpha )
			continue;

		// initialize decal shader
		if( !R_ShaderDecalForward( p ))
			continue;

		R_SetDecalUniforms( p );

		if( p->texinfo->has_alpha )
			pglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		else pglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );

		r_stats.c_total_tris += (p->numElems / 3);

		// draw decal from cache
		for( int k = 0; k < p->numElems; k += 3 )
		{
			pglBegin( GL_TRIANGLES );
				R_DecalSetupVertex( &p->verts[p->elems[k+0]] );
				R_DecalSetupVertex( &p->verts[p->elems[k+1]] );
				R_DecalSetupVertex( &p->verts[p->elems[k+2]] );
			pglEnd();
		}
	}
}

static void R_RenderSolidListDecalsDebug( drawlist_t drawlist_type )
{
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_BindShader( NULL );
	GL_Blend( GL_FALSE );

	for( int i = 0; i < RI->frame.solid_faces.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.solid_faces[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		msurface_t *s = entry->m_pSurf;

		if( !FBitSet( s->flags, SURF_HAS_DECALS ))
			continue;

		R_DrawSurfaceDecalsDebug( s, drawlist_type );
	}
}

static void R_RenderTransListDecalsDebug( drawlist_t drawlist_type )
{
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_BindShader( NULL );
	GL_Blend( GL_FALSE );

	for( int i = 0; i < RI->frame.trans_list.Count(); i++ )
	{
		CTransEntry *entry = &RI->frame.trans_list[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		msurface_t *s = entry->m_pSurf;

		if( !FBitSet( s->flags, SURF_HAS_DECALS ))
			continue;

		R_DrawSurfaceDecalsDebug( s, drawlist_type );
	}
}

void R_RenderDecalsSolidList( drawlist_t drawlist_type )
{
	ZoneScoped;

	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	if( !gDecalCount || !CVAR_TO_BOOL( cv_decals ))
		return;

	if( !RI->frame.solid_faces.Count() )
		return;

	if( CVAR_TO_BOOL( cv_decalsdebug ))
	{
		R_RenderSolidListDecalsDebug( drawlist_type );
		return;
	}

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	GL_Blend( GL_TRUE );

	GL_DEBUG_SCOPE();
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );

	if( CVAR_TO_BOOL( r_polyoffset ))
	{
		pglEnable( GL_POLYGON_OFFSET_FILL );
		pglPolygonOffset( -1.0f, -r_polyoffset->value );
	}

	for( int i = 0; i < RI->frame.solid_faces.Count(); i++ )
	{
		CSolidEntry *entry = &RI->frame.solid_faces[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		msurface_t *s = entry->m_pSurf;

		if( !FBitSet( s->flags, SURF_HAS_DECALS ))
			continue;

		R_RenderSurfaceDecals( s, drawlist_type );
	}

	if( GL_Support( R_SEAMLESS_CUBEMAP ))
		pglDisable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
	if( CVAR_TO_BOOL( r_polyoffset ))
		pglDisable( GL_POLYGON_OFFSET_FILL );
	GL_CleanupAllTextureUnits();
	GL_BindShader( NULL );
	GL_Cull( GL_FRONT );
}

void R_RenderDecalsTransList( drawlist_t drawlist_type )
{
	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	if( !gDecalCount || !CVAR_TO_BOOL( cv_decals ))
		return;

	if( !RI->frame.trans_list.Count() )
		return;

	if( CVAR_TO_BOOL( cv_decalsdebug ))
	{
		R_RenderTransListDecalsDebug( drawlist_type );
		return;
	}

	GL_DEBUG_SCOPE();
	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_Cull( GL_NONE );

	pglPolygonOffset( -1, -1 );
	pglEnable( GL_POLYGON_OFFSET_FILL );

	for( int i = 0; i < RI->frame.trans_list.Count(); i++ )
	{
		CTransEntry *entry = &RI->frame.trans_list[i];

		if( entry->m_bDrawType != DRAWTYPE_SURFACE )
			continue;

		msurface_t *s = entry->m_pSurf;

		if( !FBitSet( s->flags, SURF_HAS_DECALS ))
			continue;

		R_RenderSurfaceDecals( s, drawlist_type );
	}

	pglDisable( GL_POLYGON_OFFSET_FILL );
	GL_CleanupAllTextureUnits();
	GL_BindShader( NULL );
	GL_Cull( GL_FRONT );
}

void R_RenderDecalsTransEntry( CTransEntry *entry, drawlist_t drawlist_type )
{
	if( FBitSet( RI->params, ( RP_ENVVIEW|RP_SKYVIEW )))
		return;

	if( !gDecalCount || !CVAR_TO_BOOL( cv_decals ))
		return;

	if( entry->m_bDrawType != DRAWTYPE_SURFACE )
		return;

	if( !FBitSet( entry->m_pSurf->flags, SURF_HAS_DECALS ))
		return;

	if( CVAR_TO_BOOL( cv_decalsdebug ))
	{
		R_DrawSurfaceDecalsDebug( entry->m_pSurf, drawlist_type );
		return;
	}

	GL_Blend( GL_TRUE );
	GL_AlphaTest( GL_FALSE );
	GL_DepthMask( GL_FALSE );
	GL_Cull( GL_NONE );

	pglPolygonOffset( -1, -1 );
	pglEnable( GL_POLYGON_OFFSET_FILL );

	R_RenderSurfaceDecals( entry->m_pSurf, drawlist_type );

	pglDisable( GL_POLYGON_OFFSET_FILL );
	GL_CleanupAllTextureUnits();
	GL_Cull( GL_FRONT );
}

void ClearDecals( void )
{
	for( int i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];
		ClearBits( surf->flags, SURF_HAS_DECALS|SURF_REFLECT_PUDDLE );
		surf->info->pdecals = NULL;
	}

	memset( gDecalPool, 0, sizeof( gDecalPool ));
	g_decalVertUsed = g_decalElemUsed = 0;
	gDecalCount = gDecalCycle = 0;
}

// ===========================
// Decals loading
// ===========================
void DecalsInit( void )
{
	ADD_COMMAND( "pastedecal", PasteViewDecal );
	ADD_COMMAND( "cleardecals", ClearDecals );

	ALERT( at_aiconsole, "Loading decals\n" );

	char *pfile = (char *)gEngfuncs.COM_LoadFile( "gfx/decals/decalinfo.txt", 5, NULL );

	if( !pfile )
	{
		ALERT( at_error, "Cannot open file \"gfx/decals/decalinfo.txt\"\n" );
		return;
	}

	ALERT( at_aiconsole, "Decal vertex cache %s, index cache %s\n", Q_memprint( sizeof( g_decalVertexCache )), Q_memprint( sizeof( g_decalIndexCache )));

	char *ptext = pfile;
	int counter = 0;

	while( 1 )
	{
		// store position where group names recorded
		char *groupnames = ptext;

		// loop until we'll find decal names
		char path[256], token[256];
		int numgroups = 0;

		while( 1 )
		{
			ptext = COM_ParseFile( ptext, token );
			if( !ptext ) goto getout;

			if( token[0] == '{' && !token[1] )
				break;
			numgroups++;
		}

		DecalGroupEntry	tempentries[MAX_GROUPENTRIES];
		int		numtemp = 0;

		while( 1 )
		{
			char sz_xsize[64];
			char sz_ysize[64];
			char sz_overlay[64];

			ptext = COM_ParseFile( ptext, token );
			if( !ptext ) goto getout;
			if( token[0] == '}' )
				break;

			if( numtemp >= MAX_GROUPENTRIES )
			{
				ALERT( at_error, "Too many decals in group (%d max) - skipping %s\n", MAX_GROUPENTRIES, token );
				continue;
			}

			ptext = COM_ParseFile( ptext, sz_xsize );
			if( !ptext ) goto getout;
			ptext = COM_ParseFile( ptext, sz_ysize );
			if( !ptext ) goto getout;
			ptext = COM_ParseFile( ptext, sz_overlay );
			if( !ptext ) goto getout;

			if( Q_strlen( token ) > 16 )
			{
				ALERT( at_error, "%s - got too large token when parsing decal info file\n", token );
				continue;
			}

			if( token[0] == '$' )
			{
				Q_strncpy( token, token + 1, sizeof( token ));
				tempentries[numtemp].has_alpha = true;// force solid
			}
			else tempentries[numtemp].has_alpha = false;
			Q_strncpy( tempentries[numtemp].m_DecalName, token, sizeof( tempentries[numtemp].m_DecalName ));
			tempentries[numtemp].xsize = Q_atof( sz_xsize ) / 2.0f;
			tempentries[numtemp].ysize = Q_atof( sz_ysize ) / 2.0f;
			tempentries[numtemp].overlay = Q_atof( sz_overlay ) * 2.0f;
			Q_snprintf( path, sizeof( path ), "decals/%s", token );
			tempentries[numtemp].matdesc = CL_FindMaterial( path ); // get description for decal texture
			tempentries[numtemp].m_init = false; // need to preload
			tempentries[numtemp].gl_normalmap_id = tr.normalmapTexture;
			tempentries[numtemp].gl_specular_id = tr.blackTexture;
			tempentries[numtemp].gl_heightmap_id = tr.whiteTexture;
			tempentries[numtemp].gl_diffuse_id = TextureHandle::Null(); // assume no texture
			numtemp++;
		}

		// get back to group names
		for( int i = 0; i < numgroups; i++ )
		{
			groupnames = COM_ParseFile( groupnames, token );
			if( !numtemp )
			{
				ALERT( at_warning, "got empty decal group: %s\n", token );
				continue;
			}

			new DecalGroup( token, numtemp, tempentries );
			counter++;
		}
	}

getout:
	gEngfuncs.COM_FreeFile( pfile );
	ALERT( at_console, "%d decal groups created\n", counter );
}

void DecalsShutdown( void )
{
	if( !pDecalGroupList )
		return;

	DecalGroup **prev = &pDecalGroupList;
	DecalGroup *item;

	while( 1 )
	{
		item = *prev;
		if( !item ) break;

		*prev = item->GetNext();
		delete item;
	}
}