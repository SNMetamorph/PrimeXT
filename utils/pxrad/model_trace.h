/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "raytracer.h"

#define MAX_TRIANGLES		524288		// studio triangles

#define FMESH_VERTEX_LIGHTING		BIT( 0 )
#define FMESH_MODEL_LIGHTMAPS		BIT( 1 )
#define FMESH_CAST_SHADOW		BIT( 2 )
#define FMESH_SELF_SHADOW		BIT( 3 )
#define FMESH_DONT_SMOOTH		BIT( 4 )
#define TEX_ALPHATEST		BIT( 31 )	// virtual flag

#define PLANECHECK_POSITIVE		1
#define PLANECHECK_NEGATIVE		-1
#define PLANECHECK_STRADDLING		0

typedef struct
{
	int		flags;		// STUDIO_NF_???
	int		width;
	int		height;
	byte		*data;		// may be NULL
} timage_t;

typedef struct lvert_s
{
	vec3_t		pos;			// adjusted position
#ifdef HLRAD_SHRINK_MEMORY
	hvec3_t		light[MAXLIGHTMAPS];	// lightvalue
	hvec3_t		deluxe[MAXLIGHTMAPS];	// deluxe vectors
	half		shadow[MAXLIGHTMAPS];	// shadow values
#else
	vec3_t		light[MAXLIGHTMAPS];	// lightvalue
	vec3_t		deluxe[MAXLIGHTMAPS];	// deluxe vectors
	vec_t		shadow[MAXLIGHTMAPS];	// shadow values
	hvec3_t		gi[MAXLIGHTMAPS];
	hvec3_t		gi_prev[MAXLIGHTMAPS];
	hvec3_t		gi_dlx[MAXLIGHTMAPS];
	int			face_counter;			// need for blurring
#endif
} lvert_t;

typedef struct tvert_s
{
	vec3_t		point;
	half		st[2];			// for alpha-texture test
#ifdef HLRAD_SHRINK_MEMORY
	hvec3_t		normal;			// smoothed normal
#else
	vec3_t		normal;			// smoothed normal
#endif
	lvert_t		*light;			// vertex lighting only (may be NULL)
	bool		twosided;
} tvert_t;

typedef struct lface_s
{
#ifdef HLRAD_SHRINK_MEMORY
	hvec3_t		normal;
#else
	vec3_t		normal;
#endif
	byte		styles[MAXLIGHTMAPS];	// each face has individual styles
	short		texture_step;
	short		extents[2];
	facelight_t	facelight;		// will be alloced later
	vec3_t		lmvecs[2];		// worldluxels face matrix
	vec3_t		origin;			// lightmap origin
	int		lightofs;			// offset into global lighmaps array
} lface_t;

struct tface_t
{
	vec3_t		absmin, absmax;	// an individual size of each facet
	vec3_t		edge1, edge2;	// new trace stuff
	char		contents;		// surface contents
	byte		pcoord0;		// coords selection
	byte		pcoord1;
	bool		shadow;		// this face is used for traceline
	int			a, b, c;		// face indices
	vec3_t		normal;		// triangle unsmoothed normal
	float		NdotP1;

	byte		color[3];	// diffuse color for gi

	union
	{
		timage_t	*texture;		// texture->data valid only for alpha-testing surfaces
		int	skinref;		// index during loading
	};

	lface_t		*light;		// may be NULL
	link_t		area;		// linked to a division node or leaf

	void GetEdgeEquation( const vec3_t p1, const vec3_t p2, vec3_t InsidePoint, vec3_t out )
	{
		float nx = p1[pcoord1] - p2[pcoord1];
		float ny = p2[pcoord0] - p1[pcoord0];
		float d = -(nx * p1[pcoord0] + ny * p1[pcoord1]);

		// use the convention that negative is "outside"
		float trial_dist = InsidePoint[pcoord0] * nx + InsidePoint[pcoord1] * ny + d;
		if( trial_dist == 0 ) trial_dist = FLT_EPSILON;

		if( trial_dist < 0 )
		{
			trial_dist = -trial_dist;
			nx = -nx;
			ny = -ny;
			d = -d;
		}

		// scale so that it will be =1.0 at the oppositve vertex
		nx /= trial_dist;
		ny /= trial_dist;
		d /= trial_dist;

		VectorSet( out, nx, ny, d );
	}

	void PrepareIntersectionData( tvert_t *triangle )
	{
		VectorSubtract( triangle[a].point, triangle[b].point, edge1 );
		VectorSubtract( triangle[c].point, triangle[b].point, edge2 );
		CrossProduct( edge1, edge2, normal );
		VectorNormalize( normal );
		int drop_axis = 0;

		// now, determine which axis to drop
		for( int i = 1; i < 3; i++ )
		{
			if( fabs( normal[i] ) > fabs( normal[drop_axis] ))
				drop_axis = i;
		}

		NdotP1 = DotProduct( normal, triangle[a].point );

		// decide which axes to keep
		pcoord0 = ( drop_axis + 1 ) % 3;
		pcoord1 = ( drop_axis + 2 ) % 3;

		GetEdgeEquation( triangle[a].point, triangle[b].point, triangle[c].point, edge1 );
		GetEdgeEquation( triangle[b].point, triangle[c].point, triangle[a].point, edge2 );
		if( light != NULL ) VectorCopy( normal, light->normal );
	}

#if defined( HLRAD_RAYTRACE )
	// NEW TRACE STUFF BEGIN HERE!!!
	char		pcheck0;		// KD-tree intermediate data
	char		pcheck1;

	int ClassifyAgainstAxisSplit( tvert_t *triangle, int split_plane, float split_value )
	{
		// classify a triangle against an axis-aligned plane
		float minc = triangle[a].point[split_plane];
		float maxc = minc;

		minc = Q_min( minc, triangle[b].point[split_plane] );
		maxc = Q_max( maxc, triangle[b].point[split_plane] );
		minc = Q_min( minc, triangle[c].point[split_plane] );
		maxc = Q_max( maxc, triangle[c].point[split_plane] );

		if (minc > split_value)
			return PLANECHECK_POSITIVE;

		if (maxc < split_value)
			return PLANECHECK_NEGATIVE;

		return PLANECHECK_STRADDLING;
	}
#endif
};

struct tmesh_t
{
	vec3_t		absmin, absmax;
	timage_t	*textures;
	tface_t		*faces;
	int			numfaces;
	tvert_t		*verts;
	int			numverts;

	int			flags;
	float		backfrac;		// 0.0 by default
	uint32_t	modelCRC;
	uint8_t		styles[MAXLIGHTMAPS];
	int			texture_step;	// lightmap resolution
	dvlightofs_t	vsubmodels[32];	// MAXSTUDIOMODELS
	dflightofs_t	fsubmodels[32];
	uint8_t		*vislight;	//[numworldlights/8]
	float		origin[3];
	float		angles[3];
	float		scale[3];

	aabb_tree_t	face_tree;
#ifdef HLRAD_RAYTRACE
	CWorldRayTrace	ray;
	CWorldRayTraceBVH	rayBVH;
#endif
};

typedef struct
{
	dplane_t		*edges;
	int		numedges;
	vec3_t		origin;		// face origin
	vec_t		radius;		// for culling tests
	const dface_t	*original;
	int		contents;		// sky or solid
	int		flags;		// texinfo->flags
} twface_t;