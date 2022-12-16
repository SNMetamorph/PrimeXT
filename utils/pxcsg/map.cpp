/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "csg.h"
#include "build_info.h"

CUtlArray<mapent_t>	g_mapentities;
brush_t		g_mapbrushes[MAX_MAP_BRUSHES];
int		g_nummapbrushes;
int		g_numparsedentities;
int		g_numparsedbrushes;

static short	g_groupid = 0;
static short	g_world_faceinfo = -1;	// shared faceinfo in case level-desginer apply it to the world but except landscapes
int		g_world_luxels = 0;		// alternative lightmap matrix will be used (luxels per world units instead of luxels per texels)
static int	g_brushtype = BRUSH_UNKNOWN;

const char *g_sMapType[BRUSH_COUNT] =
{
"Unknown format",
"Worldcraft 2.1",
"Valve Hammer 3.4",
"Radiant",
"QuArK"
};

/*
=================
AllocSide

allocate a new side
=================
*/
side_t *AllocSide( brush_t *brush )
{
	int	sideidx = brush->sides.AddToTail();
	side_t	*news = &brush->sides[sideidx];

	memset( news, 0, sizeof( side_t ));

	return news;
}

/*
=================
AllocBrush

allocate a new local brush
=================
*/
brush_t *AllocBrush( mapent_t *entity )
{
	int	brushidx = entity->brushes.AddToTail();
	brush_t	*newb = &entity->brushes[brushidx];

	memset( newb, 0, sizeof( brush_t ));
	newb->sides.Purge();

	return newb;
}

/*
=================
AllocEntity

allocate a new entity
=================
*/
int AllocEntity( CUtlArray<mapent_t> *entities )
{
	int	index = entities->AddToTail();
	mapent_t	*mapent = &entities->Element( index );

	memset( mapent, 0, sizeof( mapent_t ));
	ClearBounds( mapent->absmin, mapent->absmax );

	return index;
}

/*
=================
CopyBrush

copy specified brush into new one
=================
*/
static brush_t *CopyBrush( mapent_t *entity, const brush_t *brush )
{
	int	brushidx = brush - entity->brushes.Base();
	brush_t	*newb = AllocBrush( entity );

	// maybe array is changed so we need to refresh pointer to source brush
	brush = (const brush_t *)&entity->brushes[brushidx];
 	memcpy( newb, brush, sizeof( brush_t ));
	memset( &newb->sides, 0, sizeof( newb->sides ));
	newb->sides.AddMultipleToTail( brush->sides.Count() );

	// duplicate brush sides into newbrush
	for( int i = 0; i < brush->sides.Count(); i++ )
		newb->sides[i] = brush->sides[i];

	return newb;
}

/*
=================
DeleteBrush

delete brush from local array
=================
*/
static void DeleteBrush( mapent_t *entity, const brush_t *brush )
{
	int	brushidx = brush - entity->brushes.Base();

	if( brushidx < 0 || brushidx >= entity->brushes.Count())
		COM_FatalError( "DeleteBrush: invalid brush index %d\n", brushidx );

	brush_t	*b = &entity->brushes[brushidx];

	b->sides.Purge(); // remove brush sides
	entity->brushes.Remove( brushidx );
}

/*
================
MoveBrushesToEntity

copy all the brushes into another entity
================
*/
void MoveBrushesToEntity( CUtlArray<mapent_t> *entities, mapent_t *dst, mapent_t *src )
{
	int oldcount = dst->brushes.Count();
	// add the brushes to the tail of local array
	dst->brushes.AddMultipleToTail( src->brushes.Count() );
	int entnum = dst - entities->Base();

	for( int i = 0; i < src->brushes.Count(); i++ )
	{
		brush_t	*srcb = &src->brushes[i];
		brush_t	*dstb = &dst->brushes[oldcount+i];

		memcpy( dstb, srcb, sizeof( brush_t ));
		memset( &dstb->sides, 0, sizeof( dstb->sides ));
		dstb->sides.AddMultipleToTail( srcb->sides.Count() );
		dstb->entitynum = entnum; // need to setup entitynum

		// copy brush sides into newbrush
		for( int j = 0; j < srcb->sides.Count(); j++ )
			dstb->sides[j] = srcb->sides[j];
	}
}

/*
=================
EntityApplyTransform

hierarchical transform brushes by entity settings
=================
*/
static void EntityApplyTransform( mapent_t *src, mapent_t *dst, bool brushentity, bool external_map, vec_t yaw_offset = 0.0 )
{
	vec3_t	origin, angles, dscale, iscale;
	matrix3x4	ptransform, itransform;
	vec_t	temp;

	GetVectorForKey( (entity_t *)src, "origin", origin );
	VectorSet( dscale, 1.0, 1.0, 1.0 );
	VectorClear( angles );

	if( external_map )
	{
		// TyrTtils external map
		if( CheckKey( (entity_t *)src, "_external_map_angles" ))
			GetVectorForKey( (entity_t *)src, "_external_map_angles", angles );

		if( CheckKey( (entity_t *)src, "_external_map_angle" ))
			angles[1] = FloatForKey( (entity_t *)src, "_external_map_angle" );

		if( CheckKey( (entity_t *)src, "_external_map_scale" ))
		{
			switch( GetVectorForKey( (entity_t *)src, "_external_map_scale", iscale ))
			{
			case 1:
				VectorSet( dscale, iscale[0], iscale[0], iscale[0] );
				break;
			case 3:
				VectorCopy( iscale, dscale );
				break;
			}
		}
	}
	else
	{
		temp = FloatForKey( (entity_t *)src, "modelscale" );
		if( temp != 0.0f ) VectorSet( dscale, temp, temp, temp );

		if( CheckKey( (entity_t *)src, "modelscale_vec" ))
			GetVectorForKey( (entity_t *)src, "modelscale_vec", dscale );

		if( CheckKey( (entity_t *)src, "angles" ))
			GetVectorForKey( (entity_t *)src, "angles", angles );

		if( CheckKey( (entity_t *)src, "angle" ))
			angles[1] = FloatForKey( (entity_t *)src, "angle" );
	}

	angles[1] += yaw_offset;
	COM_NormalizeAngles( angles );
	VectorSet( iscale, 1.0 / dscale[0], 1.0 / dscale[1], 1.0 / dscale[2] );
	Matrix3x4_CreateFromEntityScale3f( ptransform, angles, origin, dscale );
	Matrix3x4_CreateFromEntityScale3f( itransform, angles, origin, iscale );

	if( brushentity )
	{
		for( int i = 0; i < dst->brushes.Count(); i++ )
		{
			brush_t *b = &dst->brushes[i];

			for( int j = 0; j < b->sides.Count(); j++ )
			{
				side_t *s = &b->sides[j];

				// transform plane points
				Matrix3x4_VectorTransform( ptransform, s->planepts[0], s->planepts[0] );
				Matrix3x4_VectorTransform( ptransform, s->planepts[1], s->planepts[1] );
				Matrix3x4_VectorTransform( ptransform, s->planepts[2], s->planepts[2] );

				// transform texture vectors
				Matrix3x4_VectorRotate( itransform, s->vecs[0], s->vecs[0] );
				Matrix3x4_VectorRotate( itransform, s->vecs[1], s->vecs[1] );
				s->vecs[0][3] -= DotProduct( origin, s->vecs[0] );
				s->vecs[1][3] -= DotProduct( origin, s->vecs[1] );
			}
		}
	}
	else
	{
		vec3_t	dst_origin, dst_angles, dst_scale;
		matrix3x4	dst_transform, out_matrix;
		vec3_t	default_scale, dummy;

		VectorSet( default_scale, 1.0, 1.0, 1.0 );

		// a point entity
		GetVectorForKey( (entity_t *)dst, "origin", dst_origin );
		GetVectorForKey( (entity_t *)dst, "angles", dst_angles );
		VectorSet( dst_scale, 1.0, 1.0, 1.0 );

		if( CheckKey( (entity_t *)dst, "xform" ))
			GetVectorForKey( (entity_t *)dst, "xform", dst_scale );
		else if( CheckKey( (entity_t *)dst, "scale" ))
		{
			temp = FloatForKey( (entity_t *)src, "scale" );
			if( temp != 0.0f ) VectorSet( dst_scale, temp, temp, temp );
		}

		// re-create source matrix with default scale
		Matrix3x4_CreateFromEntityScale3f( ptransform, angles, origin, default_scale );
		Matrix3x4_CreateFromEntityScale3f( dst_transform, dst_angles, dst_origin, default_scale );
		Matrix3x4_ConcatTransforms( out_matrix, ptransform, dst_transform );
		Matrix3x4_MatrixToEntityScale3f( out_matrix, dst_origin, dst_angles, dummy );

		// NOTE: point entities ignore scale for transformation
		dst_scale[0] *= dscale[0];
		dst_scale[1] *= dscale[1];
		dst_scale[2] *= dscale[2];

		// set transformed values back
		SetVectorForKey( (entity_t *)dst, "origin", dst_origin, true );
		SetVectorForKey( (entity_t *)dst, "angles", dst_angles, true );
		SetVectorForKey( (entity_t *)dst, "xform", dst_scale, true );
	}
}

/*
=================
SetupSideContents

side contents must be set as soon as possible
=================
*/
static void SetupSideContents( brush_t *brush, side_t *side )
{
	if( !Q_strnicmp( side->name, "sky", 3 ))
	{
		SetBits( side->flags, FSIDE_SKY );
		side->contents = CONTENTS_SKY;
		return;
	}

	if( side->name[0] == '!' || side->name[0] == '*' )
	{
		if( !Q_strnicmp( side->name + 1, "lava", 4 ))
			side->contents = CONTENTS_LAVA;
		else if( !Q_strnicmp( side->name + 1, "slime", 5 ))
			side->contents = CONTENTS_SLIME;
		else side->contents = CONTENTS_WATER; // otherwise it's water

		return;
	}

	if( !Q_strnicmp( side->name, "water", 5 ))
	{
		side->contents = CONTENTS_WATER;
		return;
	}

	// special case for origin-brush
	if( !Q_strnicmp( side->name, "origin", 6 ))
	{
		SetBits( brush->flags, FBRUSH_REMOVE );
		side->contents = CONTENTS_ORIGIN;
		return;
	}

	// this is needs only at build brush stage
	if( !Q_strnicmp( side->name, "skip", 4 ) || !Q_strnicmp( side->name, "hintskip", 8 ))
	{
		SetBits( side->flags, FSIDE_SKIP );
		side->contents = CONTENTS_EMPTY;
		return;
	}

	if( side->name[0] == '@' || !Q_strnicmp( side->name, "translucent", 11 ))
	{
		side->contents = CONTENTS_TRANSLUCENT;
		return;
	}

	if( !Q_strnicmp( side->name, "fog", 3 ))
	{
		side->contents = CONTENTS_FOG;
		return;
	}

	if( !Q_strnicmp( side->name, "hint", 4 ))
	{
		SetBits( side->flags, FSIDE_HINT );
		side->contents = CONTENTS_EMPTY;
		return;
	}

	if( !Q_strnicmp( side->name, "solidhint", 9 ))
		SetBits( side->flags, FSIDE_SOLIDHINT|FSIDE_NODRAW );

	if( !Q_strnicmp( side->name, "null", 4 ))	// structural clip
		SetBits( side->flags, FSIDE_NODRAW );

	if( !Q_strnicmp( side->name, "clip", 4 ))
		SetBits( brush->flags, FBRUSH_CLIPONLY );

	// all other it's a solid contents
	side->contents = CONTENTS_SOLID;
}

/*
=================
SetupSideParams

load shader, apply side flags
=================
*/
static void SetupSideParams( mapent_t *mapent, brush_t *brush, side_t *side )
{
	const char *classname = ValueForKey( (entity_t *)mapent, "classname" );

	// check for Quake1 issues
	if( side->name[0] == '*' )
		side->name[0] = '!';

	SetupSideContents( brush, side );

	// try to find shader for this side
	side->shader = ShaderInfoForShader( side->name );

	// no user shader specified, ignore it
	if( !FBitSet( side->shader->flags, FSHADER_DEFAULTED ))
	{
		// update contents from shader
		if( side->shader->contents )
			side->contents = side->shader->contents;

		if( side->shader->contents == CONTENTS_SKY )
			SetBits( side->flags, FSIDE_SKY );

		if( FBitSet( side->shader->flags, FSHADER_NOCLIP ))
			SetBits( brush->flags, FBRUSH_NOCLIP );

		if( FBitSet( side->shader->flags, FSHADER_TRIGGER ))
			SetBits( brush->flags, FBRUSH_CLIPONLY );

		if( FBitSet( side->shader->flags, FSHADER_NODRAW ))
			SetBits( side->flags, FSIDE_NODRAW );

		if( FBitSet( side->shader->flags, FSHADER_DETAIL ))
			brush->detaillevel = 1;

		if( FBitSet( side->shader->flags, FSHADER_NOLIGHTMAP ))
			SetBits( side->flags, FSIDE_NOLIGHTMAP );

		if( FBitSet( side->shader->flags, FSHADER_SKIP ))
			SetBits( side->flags, FSIDE_SKIP );

		if( FBitSet( side->shader->flags, FSHADER_CLIP ))
			SetBits( brush->flags, FBRUSH_CLIPONLY );

		if( FBitSet( side->shader->flags, FSHADER_HINT ))
		{
			Q_strncpy( side->name, "HINT", sizeof( side->name ));
			SetBits( side->flags, FSIDE_HINT );
		}

		if( FBitSet( side->shader->flags, FSHADER_REMOVE ))
			SetBits( brush->flags, FBRUSH_REMOVE );
	}

	// don't compute lightmaps for sky and triggers
	if( side->contents == CONTENTS_SKY )
		SetBits( side->flags, FSIDE_NOLIGHTMAP );

	if( !Q_strnicmp( side->name, "trigger", 7 ) || !Q_strnicmp( side->name, "aaatrigger", 10 ))
	{
		SetBits( side->flags, FSIDE_NOLIGHTMAP );
		SetBits( brush->flags, FBRUSH_NOCSG );
	}

	// for each face of each brush of this entity
	if( BoolForKey( (entity_t *)mapent, "zhlt_invisible" ) && side->contents != CONTENTS_ORIGIN )
		SetBits( side->flags, FSIDE_NODRAW );

	// disable lightmapping for brushes with special flag and for textures "black"
	if (BoolForKey((entity_t *)mapent, "zhlt_nolightmap") || !Q_strnicmp(side->name, "black", 5))
	{
		SetBits(side->flags, FSIDE_NOLIGHTMAP);
	}

	const int shadow = IntForKey( (entity_t *)mapent, "_shadow" );

	if( IntForKey( (entity_t *)mapent, "_dirt" ) == -1 )
		SetBits( side->flags, FSIDE_NODIRT );

	if( shadow == -1 )
		SetBits( side->flags, FSIDE_NOSHADOW );

	if( !Q_stricmp( classname, "func_detail_illusionary" ))
	{
		// mark these entities as TEX_NOSHADOW unless the mapper set "_shadow" "1"
		if( shadow != 1 ) SetBits( side->flags, FSIDE_NOSHADOW );
	}
}

/*
=================
SetupTextureVectors

parse and setup tex->vecs
=================
*/
static void SetupTextureVectors( mapent_t *mapent, brush_t *brush, side_t *side, short &brush_type )
{
	int	side_flags = 0;
	bool	read_flags;
	texvecs_t	tex_vects;

	if( brush_type == BRUSH_RADIANT )
		Parse2DMatrix( 2, 3, (vec_t *)tex_vects.matrix );

	// read the texturedef
	GetToken( false );

	Q_strncpy( side->name, token, sizeof( side->name ));
	SetupSideParams( mapent, brush, side );

	// continue determine brush type
	if( brush_type != BRUSH_RADIANT )
		GetToken( false );

	if( brush_type == BRUSH_WORLDCRAFT_22 || !Q_strcmp( token, "[" )) // Worldcraft 2.2+
	{
		brush_type = BRUSH_WORLDCRAFT_22;

		// texture U axis
		if( Q_strcmp( token, "[" ))
			COM_FatalError( "missing '[' in texturedef (U) at line %i\n", scriptline );

		GetToken( false );
		tex_vects.UAxis[0] = atof( token );
		GetToken( false );
		tex_vects.UAxis[1] = atof( token );
		GetToken( false );
		tex_vects.UAxis[2] = atof( token );
		GetToken( false );
		tex_vects.shift[0] = atof( token );
		GetToken( false );

		if( Q_strcmp( token, "]" ))
			COM_FatalError( "missing ']' in texturedef (U) at line %i\n", scriptline );

		// texture V axis
		GetToken( false );

		if( Q_strcmp( token, "[" ))
			COM_FatalError( "missing '[' in texturedef (V) at line %i\n", scriptline );

		GetToken( false );
		tex_vects.VAxis[0] = atof( token );
		GetToken( false );
		tex_vects.VAxis[1] = atof( token );
		GetToken( false );
		tex_vects.VAxis[2] = atof( token );
		GetToken( false );
		tex_vects.shift[1] = atof( token );
		GetToken( false );

		if( Q_strcmp( token, "]" ))
			COM_FatalError( "missing ']' in texturedef (V) at line %i\n", scriptline );

		// texture rotation is implicit in U/V axes.
		GetToken( false );
		tex_vects.rotate = 0.0;

		// texure scale
		GetToken( false );
		tex_vects.scale[0] = atof( token );
		GetToken( false );
		tex_vects.scale[1] = atof( token );
	}
	else if( brush_type == BRUSH_WORLDCRAFT_21 || brush_type == BRUSH_QUARK )
	{
		// worldcraft 2.1-, old Radiant, QuArK
		tex_vects.shift[0] = atof( token );
		GetToken( false );
		tex_vects.shift[1] = atof( token );
		GetToken( false );
		tex_vects.rotate = atof( token );	
		GetToken( false );
		tex_vects.scale[0] = atof( token );
		GetToken( false );
		tex_vects.scale[1] = atof( token );
	}

	// can't read it here because we're unfinished detecting map type
	read_flags = TryToken( ) ? true : false;
	if( read_flags ) side_flags = atoi( token );

	if( g_TXcommand == '1' || g_TXcommand == '2' )
	{
		vec_t	a, b, c, d, determinant;
		vec3_t	vecs[2];

		brush_type = BRUSH_QUARK;

		// we are QuArK mode and need to translate some numbers to align textures it's way
		// from QuArK, the texture vectors are given directly from the three points
		switch( g_TXcommand - '0' )
		{
		case 1:
			VectorSubtract( side->planepts[2], side->planepts[0], vecs[0] );
			VectorSubtract( side->planepts[1], side->planepts[0], vecs[1] );
			break;
		case 2:
			VectorSubtract( side->planepts[1], side->planepts[0], vecs[0] );
			VectorSubtract( side->planepts[2], side->planepts[0], vecs[1] );
			break;
		default:
			COM_FatalError( "QuArK: bad TX%d type\n", g_TXcommand - '0' );
		}

		// default QuArK scale correction
		VectorScale( vecs[0], 1.0 / 128.0, vecs[0] );
		VectorScale( vecs[1], 1.0 / 128.0, vecs[1] );

		a = DotProduct( vecs[0], vecs[0] );
		b = DotProduct( vecs[0], vecs[1] );
		c = DotProduct( vecs[1], vecs[0] );	// b == c
		d = DotProduct( vecs[1], vecs[1] );
		determinant = a * d - b * c;

		if( fabs( determinant ) > 1e-6 )
		{
			for( int i = 0; i < 3; i++ )
			{
				side->vecs[0][i] = (d * vecs[0][i] - b * vecs[1][i]) / determinant;
				side->vecs[1][i] =-(a * vecs[1][i] - c * vecs[0][i]) / determinant;
			}

			side->vecs[0][3] = -DotProduct( side->vecs[0], side->planepts[0] );
			side->vecs[1][3] = -DotProduct( side->vecs[1], side->planepts[0] );
		}
		else
		{
			MsgDev( D_WARN, "Degenerate QuArK-style brush texture: Entity %i, Brush %i\n",
				brush->originalentitynum, brush->originalbrushnum );
		}
	}
	else if( brush_type == BRUSH_WORLDCRAFT_21 || brush_type == BRUSH_WORLDCRAFT_22 )
	{
		vec3_t	axis[2];
		int	i, j;

		if( brush_type == BRUSH_WORLDCRAFT_21 )
			TextureAxisFromSide( side, axis[0], axis[1], false );

		if( !tex_vects.scale[0] ) tex_vects.scale[0] = 1.0;
		if( !tex_vects.scale[1] ) tex_vects.scale[1] = 1.0;

		if( brush_type == BRUSH_WORLDCRAFT_21 )
		{
			vec_t	sinv, cosv;
			vec_t	ns, nt;
			int	sv, tv;

			// rotate axis
			if( tex_vects.rotate == 0.0 )
			{
				sinv = 0.0;
				cosv = 1.0;
			}
			else if( tex_vects.rotate == 90.0 )
			{
				sinv = 1.0;
				cosv = 0.0;
			}
			else if( tex_vects.rotate == 180.0 )
			{
				sinv = 0.0;
				cosv = -1.0;
			}
			else if( tex_vects.rotate == 270.0 )
			{
				sinv = -1.0;
				cosv = 0.0;
			}
			else
			{
				vec_t	ang = DEG2RAD( tex_vects.rotate );

				sinv = sin( ang );
				cosv = cos( ang );
			}

			if( axis[0][0] )
				sv = 0;
			else if( axis[0][1] )
				sv = 1;
			else sv = 2;

			if( axis[1][0] )
				tv = 0;
			else if( axis[1][1] )
				tv = 1;
			else tv = 2;
			
			for( i = 0; i < 2; i++ )
			{
				ns = cosv * axis[i][sv] - sinv * axis[i][tv];
				nt = sinv * axis[i][sv] + cosv * axis[i][tv];
				axis[i][sv] = ns;
				axis[i][tv] = nt;
			}

			for( i = 0; i < 2; i++ )
			{
				for( j = 0; j < 3; j++ )
				{
					side->vecs[i][j] = axis[i][j] / tex_vects.scale[i];
				}
			}
		}
		else if( brush_type == BRUSH_WORLDCRAFT_22 )
		{
			for( j = 0; j < 3; j++ )
				side->vecs[0][j] = tex_vects.UAxis[j] / tex_vects.scale[0];
			for( j = 0; j < 3; j++ )
				side->vecs[1][j] = tex_vects.VAxis[j] / tex_vects.scale[1];
		}

		side->vecs[0][3] = tex_vects.shift[0];
		side->vecs[1][3] = tex_vects.shift[1];
	}
	else if( brush_type == BRUSH_RADIANT )
	{
		int	width, height;
		vec3_t	axis[2];

		TEX_GetSize( side->shader->imagePath, &width, &height );
		TextureAxisFromSide( side, axis[0], axis[1], true );

		side->vecs[0][0] = width * ((axis[0][0] * tex_vects.matrix[0][0]) + (axis[1][0] * tex_vects.matrix[0][1]));
		side->vecs[0][1] = width * ((axis[0][1] * tex_vects.matrix[0][0]) + (axis[1][1] * tex_vects.matrix[0][1]));
		side->vecs[0][2] = width * ((axis[0][2] * tex_vects.matrix[0][0]) + (axis[1][2] * tex_vects.matrix[0][1]));
		side->vecs[0][3] = width * tex_vects.matrix[0][2];

		side->vecs[1][0] = height * ((axis[0][0] * tex_vects.matrix[1][0]) + (axis[1][0] * tex_vects.matrix[1][1]));
		side->vecs[1][1] = height * ((axis[0][1] * tex_vects.matrix[1][0]) + (axis[1][1] * tex_vects.matrix[1][1]));
		side->vecs[1][2] = height * ((axis[0][2] * tex_vects.matrix[1][0]) + (axis[1][2] * tex_vects.matrix[1][1]));
		side->vecs[1][3] = height * tex_vects.matrix[1][2];
	}

	// Quake3 detail brushes or Volatile3D detail brushes
	if( FBitSet( side_flags, 0x8000000 ) || FBitSet( side_flags, 0x400000 ))
	{
		// enable detaillevel feature on solid brushes
		if( side->contents == CONTENTS_SOLID )
			brush->detaillevel = 1;
//		SetBits( brush->flags, FBRUSH_NOCSG );
	}

	// Doom2Gold extrainfo
	if( g_DXspecial != 0 )
	{
		dfaceinfo_t	*fi = NULL;
		short		doomlight;

		if( FBitSet( g_DXspecial, (1<<16) ))
		{
			SetBits( side->flags, FSIDE_SCROLL );
			ClearBits( g_DXspecial, (1<<16));
		}

		doomlight = (short)g_DXspecial;

		// check for already exist faceinfo
		if( side->faceinfo != -1 )
		{
			fi = &g_dfaceinfo[side->faceinfo];
			side->faceinfo = FaceinfoForTexinfo( fi->landname, fi->texture_step, fi->max_extent, doomlight );
		}
		else
		{
			// build from scratch
			side->faceinfo = FaceinfoForTexinfo( "", TEXTURE_STEP, MAX_SURFACE_EXTENT, doomlight );
		}
	}

	// first token it's already waiting
	if( read_flags )
	{
		TryToken(); // unused
		TryToken(); // unused
	}
}

/*
=================
ParseBrush
=================
*/
static void ParseBrush( mapent_t *mapent, short entindex, short faceinfo, short &brush_type )
{
	brush_t	*brush;
	side_t	*side;
	int	i;

	if( brush_type == BRUSH_RADIANT )
		CheckToken( "{" );

	brush = AllocBrush( mapent );
	brush->originalentitynum = g_numparsedentities;
	brush->originalbrushnum = g_numparsedbrushes;
	brush->entitynum = entindex;

	// read ZHLT settings for this brush
	brush->detaillevel = Q_max( IntForKey( (entity_t *)mapent, "zhlt_detaillevel" ), 0 );

	if( BoolForKey( (entity_t *)mapent, "zhlt_noclip" ))
		SetBits( brush->flags, FBRUSH_NOCLIP );
	if( BoolForKey( (entity_t *)mapent, "zhlt_nocsg" ))
		SetBits( brush->flags, FBRUSH_NOCSG );

	while( 1 )
	{
		g_TXcommand = 0;
		g_DXspecial = 0;

		if( !GetToken( true ))
			break;

		if( !Q_strcmp( token, "}" ))
			break;

		if( brush_type == BRUSH_RADIANT )
		{
			while( 1 )
			{
				if( Q_strcmp( token, "(" ))
					GetToken( false );
				else break;
				GetToken( true );
			}
		}

		side = AllocSide( brush );
		UnGetToken();

		// read the three point plane definition
		Parse1DMatrix( 3, side->planepts[0] );
		Parse1DMatrix( 3, side->planepts[1] );
		Parse1DMatrix( 3, side->planepts[2] );

		// store faceinfo number for group of brushes. Otherwise write -1
		side->faceinfo = faceinfo;

		SetupTextureVectors( mapent, brush, side, brush_type );
	}

	if( brush_type == BRUSH_RADIANT )
	{
		UnGetToken();
		CheckToken( "}" );
		CheckToken( "}" );
	}

	brush->contents = BrushContents( mapent, brush );

	// check for faces that should be a nullify
	for( i = 0; i < brush->sides.Count(); i++ )
	{
		side = &brush->sides[i];

		// remove aaatrigger from the world and brush entities
		if(( g_nullifytrigger || brush->entitynum == 0 ) && ( !Q_strnicmp( side->name, "AAATRIGGER", 10 ) || !Q_strnicmp( side->name, "trigger", 7 )))
			SetBits( side->flags, FSIDE_NODRAW );
	}

	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity
	if( brush->contents == CONTENTS_ORIGIN )
	{
		char	string[32];
		vec3_t	origin;

		CreateBrushFaces( brush ); // to get sizes
		DeleteBrushFaces( brush );

		if( brush->entitynum != 0 )
		{
			// ignore for WORLD (code elsewhere enforces no ORIGIN in world message)
			VectorAverage( brush->hull[0].mins, brush->hull[0].maxs, origin );
			Q_snprintf( string, sizeof( string ), "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2] );
			SetKeyValue( (entity_t *)mapent, "origin", string );
		}
	}

	// brush will no longer used and should be removed
	if( FBitSet( brush->flags, FBRUSH_REMOVE ))
	{
		DeleteBrush( mapent, brush );
		return;
	}

	// apply clip to the sky (NOTE: after call CopyBrush brush ptr is possibly invalidate)
	if( brush->contents == CONTENTS_SKY && !FBitSet( brush->flags, FBRUSH_NOCLIP ))
	{
		brush_t *newb = CopyBrush( mapent, brush );
		SetBits( newb->flags, FBRUSH_CLIPONLY );
		newb->contents = CONTENTS_SOLID;
	}
}

/*
================
ParseMapEntity
================
*/
short GetFaceInfoForEntity( mapent_t *mapent )
{
	if( g_onlyents ) return -1; // don't modify g_dfaceinfo!

	// NOTE: we guranteed what this function called only for non-point entities
	const char *landname = ValueForKey( (entity_t *)mapent, "zhlt_landscape" );
	int	texture_step = g_compatibility_goldsrc ? TEXTURE_STEP : Q_max( 0, IntForKey( (entity_t *)mapent, "zhlt_texturestep" ));
	int	max_extent = g_compatibility_goldsrc ? MAX_SURFACE_EXTENT : Q_max( 0, IntForKey( (entity_t *)mapent, "zhlt_maxextent" ));
	int	global_texture_step = TEXTURE_STEP, global_max_extent = MAX_SURFACE_EXTENT;
	int	lmscale = TEXTURE_STEP * Q_max( 0, FloatForKey( (entity_t *)mapent, "_lmscale" ));
	short faceinfo = -1;

	if (!g_compatibility_goldsrc)
	{
		// TyrUtils: handle _lmscale feature
		if (lmscale && lmscale != TEXTURE_STEP)
			texture_step = lmscale;
	}

	// update global settings
	if( g_world_faceinfo != -1 )
	{
		global_texture_step = g_dfaceinfo[g_world_faceinfo].texture_step;
		global_max_extent = g_dfaceinfo[g_world_faceinfo].max_extent;
	}

	// setup the global parms for world entity
	if( g_mapentities.Count() == 1 )
	{
		// get defaults
		if( !max_extent ) max_extent = MAX_SURFACE_EXTENT;
		if( !texture_step ) texture_step = TEXTURE_STEP;

		// if user not specified landscape but desire to change lightmap resolution\subdiv size globally
		if( !ValueForKey( (entity_t *)mapent, "zhlt_landscape", true ))
		{
			// check for default values
			if( max_extent == MAX_SURFACE_EXTENT && texture_step == TEXTURE_STEP )
				return -1; // nothing changed

			// store a global settings for lightmap resoltion\subdiv size
			g_world_faceinfo = FaceinfoForTexinfo( landname, texture_step, max_extent, 0 );
			return g_world_faceinfo;
		}
		else // user specified landscape for world-entity, so all other entities will be ingnore them
		{
			g_groupid++; // increase group number because landscape was specified

			// now we specified a global landscape for all the world brushes but except all other entities (include func_group etc)
			faceinfo = FaceinfoForTexinfo( landname, texture_step, max_extent, g_groupid );
			return faceinfo;
		}
	}
	else // all other non-point ents
	{
		// all the fields are completely missed, so we can use world setttings
		if( !CheckKey( (entity_t *)mapent, "zhlt_landscape" ) && !CheckKey( (entity_t *)mapent, "zhlt_texturestep" ) && !CheckKey( (entity_t *)mapent, "zhlt_maxextent" ))
		{
			return g_world_faceinfo; // nothing changed
        }

		// get defaults
		if( !max_extent ) max_extent = global_max_extent;
		if( !texture_step ) texture_step = global_texture_step;

		// if user not specified landscape but desire to change lightmap resolution\subdiv size
		if( !ValueForKey( (entity_t *)mapent, "zhlt_landscape", true ))
		{
			// check for default values
			if( max_extent == global_max_extent && texture_step == global_texture_step )
				return g_world_faceinfo; // nothing changed

			// store a global settings for lightmap resoltion\subdiv size
			faceinfo = FaceinfoForTexinfo( landname, texture_step, max_extent, 0 );
			return faceinfo;
		}
		else // user specified landscape for world-entity, so all other entities will be ingnore them
		{
			g_groupid++; // increase group number because landscape was specified

			// now we specified a global landscape for all the world brushes but except all other entities (include func_group etc)
			faceinfo = FaceinfoForTexinfo( landname, texture_step, max_extent, g_groupid );
			return faceinfo;
		}
	}
}

/*
================
ParseMapEntity
================
*/
bool ParseMapEntity( CUtlArray<mapent_t> *entities, bool external = false )
{
	short	brush_type;
	short	faceinfo = -1;
	char	str[128];
	mapent_t	*mapent;
	int	index;
	epair_t	*e;

	brush_type = BRUSH_UNKNOWN;
	g_numparsedbrushes = 0;

	if( !GetToken( true ))
		return false;

	if( Q_strcmp( token, "{" ))
		COM_FatalError( "ParseEntity: { not found at line %i\n", scriptline );

	index = AllocEntity( entities );
	mapent = &entities->Element( index );

	while( 1 )
	{
		if( !GetToken( true ))
			COM_FatalError( "ParseEntity: EOF without closing brace at line %i\n", scriptline );

		if( !Q_strcmp( token, "}" ))
		{
			// in case worldspawn doesn't contain any brushes
			if( entities->Count() == 1 && !mapent->brushes.Count( ))
				TEX_LoadTextures( entities, external );
			break;
		}

		if( !Q_strcmp( token, "{" ))
		{
			// parse a brush or patch
			if( !GetToken( true ))
				break;

			// "wad" field is now valid and can be parsed
			if( entities->Count() == 1 && !mapent->brushes.Count( ))
				TEX_LoadTextures( entities, external );

			if( !mapent->brushes.Count( ))
				faceinfo = GetFaceInfoForEntity( mapent );

			if( !Q_strcmp( token, "patchDef2" ))
			{
				// NOTE: patchDef may be combined with old brush descripton
				ParsePatch( mapent, index, faceinfo, brush_type );
				g_numparsedbrushes++;
			}
			else if( !Q_strcmp( token, "terrainDef" ))
			{
				MsgDev( D_REPORT, "terrains not supported, skipping terrain on line %d\n", scriptline );
				SkipBracedSection( 0 );
			}
			else if( !Q_strcmp( token, "brushDef" ))
			{
				brush_type = BRUSH_RADIANT;
				ParseBrush( mapent, index, faceinfo, brush_type ); // parse brush primitive
				g_numparsedbrushes++;
			}
			else
			{
				// predict state
				if( brush_type == BRUSH_UNKNOWN )
					brush_type = BRUSH_WORLDCRAFT_21;

				// QuArK or WorldCraft map
				UnGetToken();
				ParseBrush( mapent, index, faceinfo, brush_type );
				g_numparsedbrushes++;
			}
		}
		else
		{
			e = ParseEpair ();

			if( mapent->brushes.Count() > 0 )
				MsgDev( D_WARN, "ParseMapEntity: keyvalue comes after brushes\n" ); //--vluzacn

			if( !external && entities->Count() == 1 && !Q_strcmp( e->key, "zhlt_worldluxels" ))
				g_world_luxels = atoi( e->value );

			InsertLinkBefore( e, (entity_t *)mapent );
		}
	}

	if (!external && entities->Count() == 1)
	{
		// let the map tell which version of the compiler it comes from, to help tracing compiler bugs.
		Q_snprintf(str, sizeof(str), "%s %s (%s / %s / %s / %s)",
			COMPILERSTRING,
			VERSIONSTRING,
			BuildInfo::GetDate(),
			BuildInfo::GetCommitHash(),
			BuildInfo::GetArchitecture(),
			BuildInfo::GetPlatform()
		);
		SetKeyValue((entity_t *)mapent, "_compiler", str); // g-cont. don't pass this field into game dlls
	}

	// save off to displaying the map format
	if( g_brushtype == BRUSH_UNKNOWN )
		g_brushtype = brush_type;

	GetVectorForKey( (entity_t *)mapent, "origin", mapent->origin );

	const char *classname = ValueForKey( (entity_t *)mapent, "classname" );

	// group entities are just for editor convenience toss all brushes into the world entity
	if( !Q_strcmp( "func_group", classname ) || !Q_strcmp( "func_landscape", classname ) || !Q_strncmp( "func_detail", classname, 11 ))
	{
		MoveBrushesToEntity( entities, &entities->Element( 0 ), mapent );
		FreeMapEntity( mapent ); // throw all key-value pairs
		entities->Remove( index );

		return true;
	}

	// processing the external mapfile
	if( !Q_strcmp( classname, "misc_model" ))
	{
		char	modelpath[64];

		Q_strncpy( modelpath, ValueForKey((entity_t *)mapent, "model" ), sizeof( modelpath ));
		COM_ReplaceExtension( modelpath, ".mdl" );

		if( FS_FileExists( modelpath, false ))
		{
			// keep this entity and convert into env_static
			SetKeyValue((entity_t *)mapent, "classname", "env_static" );
			SetKeyValue((entity_t *)mapent, "model", modelpath );
			SetKeyValue((entity_t *)mapent, "spawnflags", "1" );	// TESTTEST
			classname = ValueForKey( (entity_t *)mapent, "classname" );	// refresh the pointer

			// rename some keys
			if( CheckKey( (entity_t *)mapent, "modelscale_vec" ))
				RenameKey( (entity_t *)mapent, "modelscale_vec", "xform" );
			if( CheckKey( (entity_t *)mapent, "modelscale" ))
				RenameKey( (entity_t *)mapent, "modelscale_vec", "scale" );
		}
		else
		{
			Q_strncpy( modelpath, ValueForKey((entity_t *)mapent, "model" ), sizeof( modelpath ));
			IncludeMapFile( modelpath, entities, index, false );
			FreeMapEntity( mapent ); // throw all key-value pairs
			entities->Remove( index );
		}

		return true;
	}

	// processing the external mapfile Tyr-Utils (not recursive)
	if( !external && !Q_strcmp( classname, "misc_external_map" ))
	{
		char	modelpath[64];

		Q_strncpy( modelpath, ValueForKey((entity_t *)mapent, "_external_map" ), sizeof( modelpath ));

		if( FS_FileExists( modelpath, false ))
		{
			// keep this entity and convert into specified classname
			SetKeyValue((entity_t *)mapent, "classname", ValueForKey((entity_t *)mapent, "_external_map_classname" ));
			RemoveKey( (entity_t *)mapent, "_external_map_classname" );
			RemoveKey( (entity_t *)mapent, "_external_map" );
			classname = ValueForKey( (entity_t *)mapent, "classname" );	// refresh the pointer

			IncludeMapFile( modelpath, entities, index, true );

			// remove source keys
			RemoveKey( (entity_t *)mapent, "_external_map_angles" );
			RemoveKey( (entity_t *)mapent, "_external_map_angle" );
			RemoveKey( (entity_t *)mapent, "_external_map_scale" );
			RemoveKey( (entity_t *)mapent, "origin" );
		}
		else
		{
			// failed to loading external map
			entities->Remove( index );
		}

		return true;
	}

	if( !external && !Q_strcmp( classname, "env_cubemap" ))
	{
		if( g_numcubemaps == MAX_MAP_CUBEMAPS )
			COM_FatalError( "MAX_MAP_CUBEMAPS limit exceeded\n" );

		// save off cubemap positions
		dcubemap_t *pCubemap = &g_dcubemaps[g_numcubemaps];
		pCubemap->origin[0] = (short)mapent->origin[0];	
		pCubemap->origin[1] = (short)mapent->origin[1];	
		pCubemap->origin[2] = (short)mapent->origin[2];	
		pCubemap->size = (short)IntForKey( (entity_t *)mapent, "cubemapsize" );
		FreeMapEntity( mapent ); // throw all key-value pairs
		entities->Remove( index );
		g_numcubemaps++;

		return true;
	}

	// auto-enum targets
	if( !Q_strcmp( classname, "multi_switcher" ))
	{
		char	string[32];
		int	count = 0;

		for( e = mapent->epairs; e; e = e->next )
		{
			if( !Q_strcmp( e->key, "mode" )) 
				continue;	// ignore mode

			// only change if value is 0
			if( Q_isdigit( e->value ) && !atoi( e->value ))
			{
				Q_snprintf( string, sizeof( string ), "%d", count );
				freestring( e->value );
				e->value = copystring( string );
				count++;
			}
		}
	}

	if( fabs( mapent->origin[0] ) > WORLD_MAXS || fabs( mapent->origin[1] ) > WORLD_MAXS || fabs( mapent->origin[2] ) > WORLD_MAXS )
	{
		if( Q_strncmp( classname, "light", 5 ))
		{
			MsgDev( D_WARN, "Entity %i (classname \"%s\"): origin outside +/-%.0f: (%.0f,%.0f,%.0f)", 
			g_numparsedentities, classname, (double)WORLD_MAXS, mapent->origin[0], mapent->origin[1], mapent->origin[2] );
		}
	}

	return true;
}

/*
================
IncludeMapFile
================
*/
bool IncludeMapFile( const char *filename, CUtlArray<mapent_t> *entities, int index, bool external_map )
{		
	mapent_t		*mapent = &entities->Element( index );
	const char	*ext = COM_FileExtension( filename );
	int		oldentitynum = g_numparsedentities;
	int		oldbrushtype = g_brushtype;
	mapent_t		*localworld, *target;
	CUtlArray<mapent_t>	localents;
	char		path[256];
	int		i;

	// only .ase and .map is allowed
	if( Q_stricmp( ext, "ase" ) && Q_stricmp( ext, "map" ))
		return false;

	int spawnflags = IntForKey((entity_t *)mapent, "spawnflags" );

	if( InsertASEModel( filename, mapent, index, -1 ))
	{
		MsgDev( D_INFO, "include: %s\n", filename );

		for( int i = 0; i < mapent->brushes.Count(); i++ )
		{
			brush_t	*b = &mapent->brushes[i];

			if( !FBitSet( spawnflags, 2 ))
				SetBits( b->flags, FBRUSH_NOCLIP );
			SetBits( b->flags, FBRUSH_NOCSG );
		}

		// all the remaining brushes it our included model. transform it
		EntityApplyTransform( mapent, mapent, true, false, -90.0 );

		// find the corresponding entity which we send brushes from this model
		target = FindTargetMapEntity( entities, ValueForKey((entity_t *)mapent, "target" ));
		if( !target ) target = &entities->Element( 0 ); // move to world as default

		// moving our transformed brushes into supposed entity
		MoveBrushesToEntity( entities, target, mapent );

		return true;
	}

	Q_strncpy( path, filename, sizeof( path ));
	COM_ReplaceExtension( path, ".map" );

	if( !FS_FileExists( path, false ))
		return false;

	// parse sub-map with models
	IncludeScriptFile( path );
	g_numparsedentities = g_numparsedbrushes = 0;
	g_brushtype = BRUSH_UNKNOWN;
	localents.Purge();

	while( ParseMapEntity( &localents, true ))
	{
		g_numparsedentities++;
	}

	// deal only with world entity (all the misc_models should be recursive include into the worldentity)
	localworld = &localents[0];

	MsgDev( D_INFO, "include: %s ^2%s^7\n", path, g_sMapType[g_brushtype] );
	MsgDev( D_REPORT, "\n" );
	MsgDev( D_REPORT, "%5i brushes\n", localworld->brushes.Count());
	MsgDev( D_REPORT, "%5i entities\n", localents.Count() );
restart:
	if( !external_map )
	{
		// Quake3 style recursive-included prefabs (keep only detail brushes)
		for( i = 0; i < localworld->brushes.Count(); i++ )
		{
			brush_t	*b = &localworld->brushes[i];

			// remove non-detail brushes, this is a debug stuff
			if( b->detaillevel ) continue;

			DeleteBrush( localworld, b );
			goto restart; // array is changed, restart
		}

		for( i = 0; i < localworld->brushes.Count(); i++ )
		{
			brush_t	*b = &localworld->brushes[i];

			if( !FBitSet( spawnflags, 2 ))
				SetBits( b->flags, FBRUSH_NOCLIP );
//			SetBits( b->flags, FBRUSH_NOCSG );
		}
	}

	mapent = &entities->Element( index ); // refresh the source pointer
	// all the remaining brushes it our included model. transform it
	EntityApplyTransform( mapent, localworld, true, external_map );

	if( !external_map )
	{
		// find the corresponding entity which we send brushes from this model
		target = FindTargetMapEntity( entities, ValueForKey((entity_t *)mapent, "target" ));
		if( !target ) target = &entities->Element( 0 ); // move to world as default
	}
	else
	{
		// target is himself
		target = mapent;
	}

	// moving our transformed brushes into supposed entity
	MoveBrushesToEntity( &localents, target, localworld );

	// move env_static from sub-levels
	for( i = 0; i < localents.Count(); i++ )
	{
		mapent_t *ent = &localents[i];

		if( Q_strcmp( ValueForKey( (entity_t *)ent, "classname" ), "env_static" ))
			continue;

		mapent = &entities->Element( index ); // refresh the source pointer
		EntityApplyTransform( mapent, ent, false, false );

		// copy point entity into the parent pool
		int newidx = AllocEntity( entities );
		mapent_t *newent = &entities->Element( newidx );
		memcpy( newent, ent, sizeof( mapent_t ));
		newent->epairs = newent->tail = NULL;
		CopyEpairs( (entity_t *)newent, (entity_t *)ent );
	}

	// free any remaining ents
	for( i = 0; i < localents.Count(); i++ )
		FreeMapEntity( &localents[i] );
	localents.Purge();

	// all done, restore oldcount
	g_numparsedentities = oldentitynum;
	g_brushtype = oldbrushtype;

	return true;
}

/*
================
FilterBrushesIntoArray
================
*/
void FilterBrushesIntoArray( mapent_t *mapent, int entnum, int contents )
{
	// simple version of clip-nazi
	const char *classname = ValueForKey( (entity_t *)mapent, "classname" );
	bool noclip = !Q_stricmp( classname, "func_illusionary" );

	for( int j = 0; j < mapent->brushes.Count(); j++ )
	{
		brush_t	*dst = &g_mapbrushes[g_nummapbrushes];
		brush_t	*src = &mapent->brushes[j];

		if( noclip ) SetBits( src->flags, FBRUSH_NOCLIP );

		// update contents for nonclipped brushes
		if( FBitSet( src->flags, FBRUSH_NOCLIP ))
		{
			for( int k = 0; k < src->sides.Count(); k++ )
			{
				side_t	*s = &src->sides[k];
				if( FBitSet( s->flags, FSIDE_NODRAW ))
				{
					Q_strncpy( s->name, "SOLIDHINT", sizeof( s->name ));
					SetBits( s->flags, FSIDE_SOLIDHINT );
				}
			}
// g-cont. This causes problems. Disabled since 13.03.2019
//			src->contents = CONTENTS_EMPTY;
		}

		if( src->contents != contents )
			continue;

		memcpy( dst, src, sizeof( brush_t ));
		memset( src, 0, sizeof( brush_t ));
		dst->entitynum = entnum;
		mapent->numbrushes++;
		g_nummapbrushes++;

		// make sure what we not overflowed
		if( g_nummapbrushes == MAX_MAP_BRUSHES )
			COM_FatalError( "MAX_MAP_BRUSHES limit exceeded\n" );
	}
}

/*
================
PrepareMapBrushes
================
*/
void PrepareMapBrushes( void )
{
	g_nummapbrushes = 0;

	// move local brushes into single solid array
	for( int i = 0; i < g_mapentities.Count(); i++ )
	{
		mapent_t	*mapent = &g_mapentities[i];

		mapent->firstbrush = g_nummapbrushes;

		FilterBrushesIntoArray( mapent, i, CONTENTS_EMPTY );
		FilterBrushesIntoArray( mapent, i, CONTENTS_FOG );
		FilterBrushesIntoArray( mapent, i, CONTENTS_WATER );
		FilterBrushesIntoArray( mapent, i, CONTENTS_SLIME );
		FilterBrushesIntoArray( mapent, i, CONTENTS_LAVA );
		FilterBrushesIntoArray( mapent, i, CONTENTS_TRANSLUCENT );
		FilterBrushesIntoArray( mapent, i, CONTENTS_SKY );
		FilterBrushesIntoArray( mapent, i, CONTENTS_SOLID ); // must be last

		// release localcopy we doesn't need them
		mapent->brushes.Purge();
	}
}

/*
================
LoadMapFile
================
*/
void LoadMapFile( const char *filename )
{		
	LoadScriptFile( filename );
	g_numparsedentities = 0;
	g_mapentities.Purge();

	while( ParseMapEntity( &g_mapentities ))
	{
		g_numparsedentities++;
	}

	Msg( "LoadMapFile: ^2%s^7\n\n", g_sMapType[g_brushtype] );

	// now copy all the brushes into solid array
	PrepareMapBrushes();

	// at this point all the brushes stored into global array
	MsgDev( D_INFO, "%5i brushes\n", g_nummapbrushes );
	MsgDev( D_INFO, "%5i entities\n", g_mapentities.Count() );
}

/*
================
MapEntityForModel

returns entity addy for given modelnum
================
*/
mapent_t *MapEntityForModel( const int modnum )
{
	char	name[16];

	Q_snprintf( name, sizeof( name ), "*%i", modnum );

	// search the entities for one using modnum
	for( int i = 0; i < g_mapentities.Count(); i++ )
	{
		const char *s = ValueForKey((entity_t *)&g_mapentities[i], "model" );
		if( !Q_strcmp( s, name )) return &g_mapentities[i];
	}

	return &g_mapentities[0];
}

/*
==================
FindTargetEntity
==================
*/
mapent_t *FindTargetMapEntity( CUtlArray<mapent_t> *entities, const char *target )
{
	for( int i = 0; i < entities->Count(); i++ )
	{
		char *n = ValueForKey( (entity_t *)&entities->Element( i ), "targetname" );
		if( !Q_strcmp( n, target ))
			return &entities->Element( i );
	}
	return NULL;
}

/*
================
UnparseMapEntities

Generates the dentdata string from all the entities
================
*/
void UnparseMapEntities( void )
{
	char	*buf, *end;
	char	line[2048];
	char	key[1024], value[1024];
	epair_t	*ep;

	buf = g_dentdata;
	end = buf;
	*end = 0;

	for( int i = 0; i < g_mapentities.Count(); i++ )
	{
		mapent_t *ent = &g_mapentities[i];

		if( !ent->epairs )
			continue;	// ent got removed

		Q_strcat( end, "{\n" );
		end += 2;

		// ugly hack to put spawnorigin after origin pt.1
		for( ep = ent->epairs; ep; ep = ep->next )
		{
			if( !Q_stricmp( ep->key, "spawnorigin" ))
				continue;

			Q_strncpy( key, ep->key, sizeof( key ));
			StripTrailing( key );
			Q_strncpy( value, ep->value, sizeof( value ));
			StripTrailing( value );

			Q_snprintf( line, sizeof( line ), "\"%s\" \"%s\"\n", key, value );
			Q_strcat( end, line );
			end += Q_strlen( line );
		}

		// ugly hack to put spawnorigin after origin pt.2
		for( ep = ent->epairs; ep; ep = ep->next )
		{
			if( Q_stricmp( ep->key, "spawnorigin" ))
				continue;

			Q_strncpy( key, ep->key, sizeof( key ));
			StripTrailing( key );
			Q_strncpy( value, ep->value, sizeof( value ));
			StripTrailing( value );

			Q_snprintf( line, sizeof( line ), "\"%s\" \"%s\"\n", key, value );
			Q_strcat( end, line );
			end += Q_strlen( line );
		}

		Q_strcat( end, "}\n" );
		end += 2;

		if( end > ( buf + MAX_MAP_ENTSTRING ))
			COM_FatalError( "Entity text too long\n" );
	}

	g_entdatasize = end - buf + 1; // write term
}

/*
================
FreeMapEntity

release all the entity data
================
*/
void FreeMapEntity( mapent_t *mapent )
{
	epair_t	*ep, *next;

	for( ep = mapent->epairs; ep != NULL; ep = next )
	{
		next = ep->next;
		freestring( ep->key );
		freestring( ep->value );
		Mem_Free( ep, C_EPAIR );
	}

	mapent->epairs = mapent->tail = NULL;

	// release brushsides
 	for( int i = 0; i < mapent->brushes.Count(); i++ )
		mapent->brushes[i].sides.Purge();

	// release brushes
	mapent->brushes.Purge();
}

/*
================
FreeMapEntities

release all the dynamically allocated data
================
*/
void FreeMapEntities( void )
{
	for( int i = 0; i < g_mapentities.Count(); i++ )
	{
		FreeMapEntity( &g_mapentities[i] );
	}

	// remove all the entities
	g_mapentities.Purge();
}