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

// don't modify these!
#define MIN_PATCH_EDGE_LENGTH		(1.0 / (vec_t)PATCH_SUBDIVISION)
#define MAX_EXPANDED_AXIS		128
#define MAX_PATCH_SIZE		32
#define PATCH_SUBDIVISION		8
#define PLANAR_EPSILON		0.01
#define STITCH_TOLERANCE		0.01
#define STITCH_ERROR		0.1

typedef struct
{
	int	width;
	int	height;
	trivert_t	*verts;
} patchmesh_t;

/*
================
PlaneFromPoints
================
*/
static bool PlaneFromPoints( plane_t *plane, const vec_t *p0, const vec_t *p1, const vec_t *p2 )
{
	vec3_t	t1, t2;
	vec_t	dist;

	VectorSubtract( p0, p1, t1 );
	VectorSubtract( p2, p1, t2 );
	CrossProduct( t1, t2, plane->normal );

	if( VectorNormalize( plane->normal ))
	{
		dist = DotProduct( plane->normal, p0 );
		plane->dist = dist;

		return true;
	}

	return false;
}

vec_t Det3x3( vec_t a00, vec_t a01, vec_t a02, vec_t a10, vec_t a11, vec_t a12, vec_t a20, vec_t a21, vec_t a22 )
{
	return a00 * ( a11 * a22 - a12 * a21 ) - a01 * ( a10 * a22 - a12 * a20 ) + a02 * ( a10 * a21 - a11 * a20 );
}

bool TexMatDegenerate( const vec3_t texMat[2] )
{
	vec2_t	vec0, vec1;

	Vector2Set( vec0, texMat[0][0], texMat[0][1] );
	Vector2Set( vec1, texMat[1][0], texMat[1][1] );

	return !Vector2Cross( vec0, vec1 ) || IS_NAN( texMat[0][0] );
}

void TexMatFromPoints( vec3_t normal, vec3_t texMat[2], trivert_t *a, trivert_t *b, trivert_t *c )
{
	vec2_t	xyI, xyJ, xyK;
	vec_t	D, D0, D1, D2;
	vec3_t	texX, texY;

	// assume error: make orthogonal
	VectorVectors( normal, texMat[0], texMat[1] );
	TextureAxisFromNormal( normal, texX, texY, true );

	xyI[0] = DotProduct( a->point, texX );
	xyI[1] = DotProduct( a->point, texY );
	xyJ[0] = DotProduct( b->point, texX );
	xyJ[1] = DotProduct( b->point, texY );
	xyK[0] = DotProduct( c->point, texX );
	xyK[1] = DotProduct( c->point, texY );

	//  - solve linear equations:
	//    - (x, y) := xyz . (texX, texY)
	//    - st[i] = texMat[i][0]*x + texMat[i][1]*y + texMat[i][2]
	//      (for three vertices)
	D = Det3x3( xyI[0], xyI[1], 1.0, xyJ[0], xyJ[1], 1.0, xyK[0], xyK[1], 1.0 );

	if( D != 0 )
	{
		for( int i = 0; i < 2; i++ )
		{
			D0 = Det3x3( a->coord[i], xyI[1], 1.0, b->coord[i], xyJ[1], 1.0, c->coord[i], xyK[1], 1.0 );
			D1 = Det3x3( xyI[0], a->coord[i], 1.0, xyJ[0], b->coord[i], 1.0, xyK[0], c->coord[i], 1.0 );
			D2 = Det3x3( xyI[0], xyI[1], a->coord[i], xyJ[0], xyJ[1], b->coord[i], xyK[0], xyK[1], c->coord[i] );
			texMat[i][0] = D0 / D;
			texMat[i][1] = D1 / D;
			texMat[i][2] = fmod( D2 / D, 1.0 );
		}
	}
}

/*
=====================
TexVecsFromPoints

turn the st into texture vectors
=====================
*/
static void TexVecsFromPoints( const char *name, vec_t vecs[2][4], trivert_t *a, trivert_t *b, trivert_t *c )
{
	int	width, height;
	vec3_t	texMat[2];
	vec3_t	axis[2];
	plane_t	plane;

	PlaneFromPoints( &plane, a->point, b->point, c->point );
	TexMatFromPoints( plane.normal, texMat, a, b, c );

	if( TexMatDegenerate( texMat )) // just throw warning. we allow non-orthogonal matrix
		MsgDev( D_REPORT, "non-orthogonal texture vecs: Entity %i, Brush %i\n", g_numparsedentities, g_numparsedbrushes );

	TEX_GetSize( name, &width, &height );
	TextureAxisFromNormal( plane.normal, axis[0], axis[1], true );

	vecs[0][0] = width * ((axis[0][0] * texMat[0][0]) + (axis[1][0] * texMat[0][1]));
	vecs[0][1] = width * ((axis[0][1] * texMat[0][0]) + (axis[1][1] * texMat[0][1]));
	vecs[0][2] = width * ((axis[0][2] * texMat[0][0]) + (axis[1][2] * texMat[0][1]));
	vecs[0][3] = width * texMat[0][2];

	vecs[1][0] = height * ((axis[0][0] * texMat[1][0]) + (axis[1][0] * texMat[1][1]));
	vecs[1][1] = height * ((axis[0][1] * texMat[1][0]) + (axis[1][1] * texMat[1][1]));
	vecs[1][2] = height * ((axis[0][2] * texMat[1][0]) + (axis[1][2] * texMat[1][1]));
	vecs[1][3] = height * texMat[1][2];
}

/*
=================
LerpPatchVert

returns an 50/50 interpolated vert
=================
*/
static void LerpPatchVert( trivert_t *a, trivert_t *b, vec_t factor, trivert_t *out )
{
	out->point[0] = a->point[0] + factor * ( b->point[0] - a->point[0] );
	out->point[1] = a->point[1] + factor * ( b->point[1] - a->point[1] );
	out->point[2] = a->point[2] + factor * ( b->point[2] - a->point[2] );

	out->coord[0] = a->coord[0] + factor * ( b->coord[0] - a->coord[0] );
	out->coord[1] = a->coord[1] + factor * ( b->coord[1] - a->coord[1] );
}

/*
=================
LerpPatchVert

returns a biased interpolated vert
=================
*/
static void LerpPatchVert( trivert_t *a, trivert_t *b, trivert_t *out )
{
	LerpPatchVert( a, b, 0.5, out );
}

/*
=====================
GenerateBoundaryForPoints
=====================
*/
static void GenerateBoundaryForPoints( plane_t *boundary, plane_t *plane, vec3_t a, vec3_t b )
{
	vec3_t	d1;

	// make a perpendicular vector to the edge and the surface
	VectorSubtract( b, a, d1 );
	CrossProduct( plane->normal, d1, boundary->normal );
	VectorNormalize( boundary->normal );
	boundary->dist = DotProduct( a, boundary->normal );
}

/*
=================
PlanePointsFromPlane
=================
*/
static void PlanePointsFromPlane( const plane_t *plane, vec3_t planepts[3] )
{
	vec3_t	org, vright, vup;
	vec_t	max = 0.56;
	int	x = -1;
	vec_t	v;

	// find the major axis
	for( int i = 0; i < 3; i++ )
	{
		v = fabs( plane->normal[i] );

		if( v > max )
		{
			max = v;
			x = i;
		}
	}

	if( x == -1 ) COM_FatalError( "PlanePointsFromPlane: no axis found\n" );

	switch( x )
	{
	case 0:
	case 1:	// fall through to next case.
		vright[0] = -plane->normal[1];
		vright[1] = plane->normal[0];
		vright[2] = 0;
		break;
	case 2:
		vright[0] = 0;
		vright[1] = -plane->normal[2];
		vright[2] = plane->normal[1];
		break;
	}

	VectorScale( vright, (vec_t)WORLD_MAXS * 4.0, vright );
	CrossProduct( plane->normal, vright, vup );
	VectorScale( plane->normal, plane->dist, org );

	VectorSubtract( org, vright, planepts[0] );
	VectorAdd( planepts[0], vup, planepts[0] );
	
	VectorAdd( org, vright, planepts[1] );
	VectorAdd( planepts[1], vup, planepts[1] );
	
	VectorAdd( org, vright, planepts[2] );
	VectorSubtract( planepts[2], vup, planepts[2] );
}

/*
================
ProjectPointOntoVector
================
*/
void ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t	pVec, vec;
	vec_t	dist;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	dist = DotProduct( pVec, vec );

	// project onto the directional vector for this segment
	VectorMA( vStart, dist, vec, vProj );
}

/*
=====================
CreatePatchBevel

create bevel for patch main plane
=====================
*/
void CreatePatchBevel( brush_t *brush, plane_t *plane, vec3_t a, vec3_t b )
{
	side_t	*side = AllocSide( brush );
	plane_t	boundary;

	Q_strncpy( side->name, "SKIP", sizeof( side->name ));
	GenerateBoundaryForPoints( &boundary, plane, a, b );
	PlanePointsFromPlane( &boundary, side->planepts );
	SetBits( side->flags, FSIDE_SKIP );
	side->contents = CONTENTS_EMPTY;
	// no reason to calc texture vectors for disardable face
}

/*
=====================
CreatePatchBevel

create bevel for patch main plane
=====================
*/
void CreatePatchBackplane( brush_t *brush, plane_t *plane )
{
	side_t	*side = AllocSide( brush );
	vec_t	dist = 8192.0; // FIXME: tune this
	plane_t	boundary;

	Q_strncpy( side->name, "SKIP", sizeof( side->name ));
	VectorNegate( plane->normal, boundary.normal );
	boundary.dist = plane->dist;
	PlanePointsFromPlane( &boundary, side->planepts );
	SetBits( side->flags, FSIDE_SKIP );
	side->contents = CONTENTS_EMPTY;
	// no reason to calc texture vectors for disardable face

	// at the finally generate the backplane
	VectorMA( side->planepts[0], dist, boundary.normal, side->planepts[0] );
	VectorMA( side->planepts[1], dist, boundary.normal, side->planepts[1] );
	VectorMA( side->planepts[2], dist, boundary.normal, side->planepts[2] );
}

/*
=====================
MakeBrushFor3Points
=====================
*/
bool MakeBrushFor3Points( mapent_t *mapent, side_t *mainSide, short entindex, trivert_t *a, trivert_t *b, trivert_t *c )
{
	brush_t	*brush;
	side_t	*side;
	plane_t	plane;

	// before createing the brush we should make sure what all points lies on the plane
	if( !PlaneFromPoints( &plane, a->point, b->point, c->point ))
		return false; // bad points ?

	// alloc brush to store the path
	brush = AllocBrush( mapent );
	brush->originalentitynum = g_numparsedentities;
	brush->originalbrushnum = g_numparsedbrushes;
	brush->entitynum = entindex;

	// read ZHLT settings for this brush
	if( BoolForKey( (entity_t *)mapent, "zhlt_noclip" ))
		SetBits( brush->flags, FBRUSH_NOCLIP );
	SetBits( brush->flags, FBRUSH_PATCH );
//	SetBits( brush->flags, FBRUSH_NOCSG );
	brush->detaillevel = 2; // patches are always is detail

	// alloc main side (patch)
	side = AllocSide( brush );
	memcpy( side, mainSide, sizeof( side_t )); 
	TexVecsFromPoints( side->name, side->vecs, a, b, c );
	VectorCopy( a->point, side->planepts[0] );
	VectorCopy( b->point, side->planepts[1] );
	VectorCopy( c->point, side->planepts[2] );

	// make boundaries (side planes)
	CreatePatchBevel( brush, &plane, a->point, b->point );
	CreatePatchBevel( brush, &plane, b->point, c->point );
	CreatePatchBevel( brush, &plane, c->point, a->point );

	// at finally generate the backplane
	CreatePatchBackplane( brush, &plane );

	// determine contents
	brush->contents = BrushContents( mapent, brush );

	return true;
}

/*
=====================
MakeBrushFor4Points

Attempts to use four points as a planar quad
This is optimal case for lightmapping
=====================
*/
bool MakeBrushFor4Points( mapent_t *mapent, side_t *mainSide, short entindex, trivert_t *a, trivert_t *b, trivert_t *c, trivert_t *d )
{
	plane_t	plane, plane2;
	vec3_t	verts[4];
	brush_t	*brush;
	side_t	*side;
	int	i;

	// before createing the brush we should make sure what all points lies on the plane
	if( !PlaneFromPoints( &plane, a->point, b->point, c->point ))
		return false; // bad points ?

	// if the fourth point is also on the plane, we can make a quad facet
	vec_t	dist = PlaneDiff2( d->point, &plane );

	if( fabs( dist ) > PLANAR_EPSILON )
		return false;

	VectorCopy( a->point, verts[0] );
	VectorCopy( b->point, verts[1] );
	VectorCopy( c->point, verts[2] );
	VectorCopy( d->point, verts[3] );

	for( i = 1; i < 4; i++ )
	{
		if( !PlaneFromPoints( &plane2, verts[i], verts[(i+1) % 4], verts[(i+2) % 4] ))
			return false;

		// maximum difference between two planes
		if( DotProduct( plane.normal, plane2.normal ) < 0.9 )
			return false;
	}

	// alloc brush to store the path
	brush = AllocBrush( mapent );
	brush->originalentitynum = g_numparsedentities;
	brush->originalbrushnum = g_numparsedbrushes;
	brush->entitynum = entindex;

	// read ZHLT settings for this brush
	if( BoolForKey( (entity_t *)mapent, "zhlt_noclip" ))
		SetBits( brush->flags, FBRUSH_NOCLIP );
	SetBits( brush->flags, FBRUSH_PATCH );
//	SetBits( brush->flags, FBRUSH_NOCSG );
	brush->detaillevel = 2; // patches are always is detail

	// alloc main side (patch)
	side = AllocSide( brush );
	memcpy( side, mainSide, sizeof( side_t )); 
	TexVecsFromPoints( side->name, side->vecs, a, b, c );
	VectorCopy( a->point, side->planepts[0] );
	VectorCopy( b->point, side->planepts[1] );
	VectorCopy( c->point, side->planepts[2] );

	// make boundaries (side planes)
	CreatePatchBevel( brush, &plane, a->point, b->point );
	CreatePatchBevel( brush, &plane, b->point, c->point );
	CreatePatchBevel( brush, &plane, c->point, d->point );
	CreatePatchBevel( brush, &plane, d->point, a->point );

	// at finally generate the backplane
	CreatePatchBackplane( brush, &plane );

	// determine contents
	brush->contents = BrushContents( mapent, brush );

	return true;
}

/*
=================
AllocPatchMesh

alloc a new patch
=================
*/
patchmesh_t *AllocPatchMesh( int width, int height )
{
	int size = sizeof( patchmesh_t ) + ( width * height * sizeof( trivert_t ));
	patchmesh_t *out = (patchmesh_t *)Mem_Alloc( size, C_PATCH );

	out->verts = (trivert_t *)(out + 1);
	out->width = width;
	out->height = height;

	return out;
}

/*
=================
CopyPatchMesh

make a mesh copy
=================
*/
patchmesh_t *CopyPatchMesh( patchmesh_t *mesh )
{
	int size = sizeof( patchmesh_t ) + ( mesh->width * mesh->height * sizeof( trivert_t ));
	patchmesh_t *out = (patchmesh_t *)Mem_Alloc( size, C_PATCH );

	out->verts = (trivert_t *)(out + 1);
	out->width = mesh->width;
	out->height = mesh->height;
	memcpy( out->verts, mesh->verts, size - sizeof( patchmesh_t ));

	return out;
}

/*
=================
FreePatchMesh

release a mesh
=================
*/
void FreePatchMesh( patchmesh_t *m )
{
	Mem_Free( m, C_PATCH );
}

/*
================
RemoveLinearMeshColumsRows
================
*/
patchmesh_t *RemoveLinearMeshColumnsRows( patchmesh_t *in )
{
	trivert_t		expand[MAX_EXPANDED_AXIS][MAX_EXPANDED_AXIS];
	float		len, maxLength;
	vec3_t		proj, dir;
	int		i, j, k;
	patchmesh_t	out;

	out.width = in->width;
	out.height = in->height;

	for( i = 0; i < in->width; i++ )
	{
		for( j = 0; j < in->height; j++ )
		{
			expand[j][i] = in->verts[j * in->width + i];
		}
	}

	for( j = 1; j < out.width - 1; j++ )
	{
		maxLength = 0.0;

		for( i = 0; i < out.height; i++ )
		{
			ProjectPointOntoVector( expand[i][j].point, expand[i][j - 1].point, expand[i][j + 1].point, proj );
			VectorSubtract( expand[i][j].point, proj, dir );
			len = VectorLength( dir );
			maxLength = Q_max( len, maxLength );
		}

		if( maxLength < PLANAR_EPSILON )
		{
			out.width--;

			for( i = 0; i < out.height; i++ )
			{
				for( k = j; k < out.width; k++ )
				{
					expand[i][k] = expand[i][k + 1];
				}
			}
			j--;
		}
	}

	for( j = 1; j < out.height - 1; j++ )
	{
		maxLength = 0.0;

		for( i = 0; i < out.width; i++ )
		{
			ProjectPointOntoVector( expand[j][i].point, expand[j - 1][i].point, expand[j + 1][i].point, proj );
			VectorSubtract( expand[j][i].point, proj, dir );
			len = VectorLength( dir );
			maxLength = Q_max( len, maxLength );
		}

		if( maxLength < PLANAR_EPSILON )
		{
			out.height--;
			for( i = 0; i < out.width; i++ )
			{
				for( k = j; k < out.height; k++ )
				{
					expand[k][i] = expand[k + 1][i];
				}
			}
			j--;
		}
	}

	// collapse the verts
	out.verts = &expand[0][0];
	for( i = 1; i < out.height; i++ )
		memmove( &out.verts[i * out.width], expand[i], out.width * sizeof( trivert_t ));

	return CopyPatchMesh( &out );
}

/*
=================
PutMeshOnCurve

Drops the aproximating points onto the curve
=================
*/
void PutMeshOnCurve( patchmesh_t *in )
{
	float	prev, next;
	int	i, j, l;

	for( i = 0; i < in->width; i++ )
	{
		for( j = 1; j < in->height; j += 2 )
		{
			for( l = 0; l < 3; l++ )
			{
				prev = ( in->verts[j*in->width+i].point[l] + in->verts[(j+1)*in->width+i].point[l] ) * 0.5;
				next = ( in->verts[j*in->width+i].point[l] + in->verts[(j-1)*in->width+i].point[l] ) * 0.5;
				in->verts[j*in->width+i].point[l] = ( prev + next ) * 0.5;

				if( l >= 2 ) continue;

				prev = ( in->verts[j*in->width+i].coord[l] + in->verts[(j+1)*in->width+i].coord[l] ) * 0.5;
				next = ( in->verts[j*in->width+i].coord[l] + in->verts[(j-1)*in->width+i].coord[l] ) * 0.5;
				in->verts[j*in->width+i].coord[l] = ( prev + next ) * 0.5;
			}
		}
	}

	for( j = 0; j < in->height; j++ )
	{
		for( i = 1; i < in->width; i += 2 )
		{
			for( l = 0 ; l < 3 ; l++ )
			{
				prev = ( in->verts[j*in->width+i].point[l] + in->verts[j*in->width+i+1].point[l] ) * 0.5;
				next = ( in->verts[j*in->width+i].point[l] + in->verts[j*in->width+i-1].point[l] ) * 0.5;
				in->verts[j*in->width+i].point[l] = ( prev + next ) * 0.5;

				if( l >= 2 ) continue;

				prev = ( in->verts[j*in->width+i].coord[l] + in->verts[j*in->width+i+1].coord[l] ) * 0.5;
				next = ( in->verts[j*in->width+i].coord[l] + in->verts[j*in->width+i-1].coord[l] ) * 0.5;
				in->verts[j*in->width+i].coord[l] = ( prev + next ) * 0.5;
			}
		}
	}
}

/*
=================
SubdivideMesh

subdivides each mesh quad a specified number of times
=================
*/
patchmesh_t *SubdivideMesh( patchmesh_t *in, int iterations )
{
	trivert_t		expand[MAX_EXPANDED_AXIS][MAX_EXPANDED_AXIS];
	trivert_t		prev, next, mid;
	int		i, j, k;
	patchmesh_t	out;

	out.width = in->width;
	out.height = in->height;

	for( i = 0; i < in->width; i++ )
	{
		for( j = 0; j < in->height; j++ )
			expand[j][i] = in->verts[j * in->width + i];
	}

	// keep chopping
	for( ; iterations > 0; iterations-- )
	{
		// horizontal subdivisions
		for( j = 0; j + 2 < out.width; j += 4 )
		{
			// check size limit
			if( out.width + 2 >= MAX_EXPANDED_AXIS )
				break;

			out.width += 2;

			// insert two columns and replace the peak
			for( i = 0; i < out.height; i++ )
			{
				LerpPatchVert( &expand[i][j+0], &expand[i][j+1], &prev );
				LerpPatchVert( &expand[i][j+1], &expand[i][j+2], &next );
				LerpPatchVert( &prev, &next, &mid );

				for( k = out.width - 1 ; k > j + 3; k-- )
					expand[i][k] = expand[i][k-2];

				expand[i][j+1] = prev;
				expand[i][j+2] = mid;
				expand[i][j+3] = next;
			}

		}

		// vertical subdivisions
		for( j = 0; j + 2 < out.height; j += 4 )
		{
			// check size limit
			if( out.height + 2 >= MAX_EXPANDED_AXIS )
				break;

			out.height += 2;

			// insert two columns and replace the peak
			for( i = 0; i < out.width; i++ )
			{
				LerpPatchVert( &expand[j+0][i], &expand[j+1][i], &prev );
				LerpPatchVert( &expand[j+1][i], &expand[j+2][i], &next );
				LerpPatchVert( &prev, &next, &mid );

				for( k = out.height - 1; k > j + 3; k-- )
					expand[k][i] = expand[k-2][i];

				expand[j+1][i] = prev;
				expand[j+2][i] = mid;
				expand[j+3][i] = next;
			}
		}
	}

	// collapse the verts
	out.verts = &expand[0][0];
	for( i = 1; i < out.height; i++ )
		memmove( &out.verts[i * out.width], expand[i], out.width * sizeof( trivert_t ));

	return CopyPatchMesh( &out );
}


/*
=================
ExpandLongestCurve

finds length of quadratic curve specified and determines if length is longer than the supplied max
=================
*/
static void ExpandLongestCurve( vec_t *longestCurve, vec3_t a, vec3_t b, vec3_t c )
{
	vec3_t	ab, bc, ac, pt, last, delta;
	vec_t	t, len;
	int	i;
	
	VectorSubtract( b, a, ab );
	if( VectorNormalize( ab ) < MIN_PATCH_EDGE_LENGTH )
		return;

	VectorSubtract( c, b, bc );
	if( VectorNormalize( bc ) < MIN_PATCH_EDGE_LENGTH )
		return;

	VectorSubtract( c, a, ac );
	if( VectorNormalize( ac ) < MIN_PATCH_EDGE_LENGTH )
		return;
	
	// if all 3 vectors are the same direction, then this edge is linear, so we ignore it
	if( DotProduct( ab, bc ) > (1.0 - NORMAL_EPSILON) && DotProduct( ab, ac ) > (1.0 - NORMAL_EPSILON))
		return;
	
	VectorSubtract( b, a, ab );
	VectorSubtract( c, b, bc );
	VectorCopy( a, last );

	for( i = 0, len = 0.0f, t = 0.0f; i < PATCH_SUBDIVISION; i++ )
	{
		delta[0] = ((1.0f - t) * ab[0]) + (t * bc[0]);
		delta[1] = ((1.0f - t) * ab[1]) + (t * bc[1]);
		delta[2] = ((1.0f - t) * ab[2]) + (t * bc[2]);

		// add to first point and calculate pt-pt delta
		VectorAdd( a, delta, pt );
		VectorSubtract( pt, last, delta );
		
		// add it to length and store last point
		t += ( 1.0 / PATCH_SUBDIVISION );
		len += VectorLength( delta );
		VectorCopy( pt, last );
	}
	
	if( len > *longestCurve )
		*longestCurve = len;
}



/*
=================
ExpandMaxIterations

determines how many iterations a quadratic curve needs to be subdivided with to fit the specified error
=================
*/
static void ExpandMaxIterations( int *maxIterations, int maxError, vec3_t a, vec3_t b, vec3_t c )
{
	vec3_t	prev, next, mid, delta, delta2;
	vec3_t	points[MAX_EXPANDED_AXIS];
	int	numPoints, iterations;	
	vec_t	len, len2;
	int	i, j;
	
	VectorCopy( a, points[0] );
	VectorCopy( b, points[1] );
	VectorCopy( c, points[2] );
	numPoints = 3;

	for( i = 0; i + 2 < numPoints; i += 2 )
	{
		if( numPoints + 2 >= MAX_EXPANDED_AXIS )
			break;
		
		for( j = 0; j < 3; j++ )
		{
			prev[j] = points[i + 1][j] - points[i][j]; 
			next[j] = points[i + 2][j] - points[i + 1][j]; 
			mid[j] = (points[i][j] + points[i + 1][j] * 2.0 + points[i + 2][j]) * 0.25;
		}
		
		// see if this midpoint is off far enough to subdivide
		VectorSubtract( points[i + 1], mid, delta );
		len = VectorLength( delta );
		if( len < maxError ) continue;
		
		numPoints += 2;
		
		for( j = 0; j < 3; j++ )
		{
			prev[j] = 0.5 * (points[i][j] + points[i + 1][j]);
			next[j] = 0.5 * (points[ i + 1 ][j] + points[i + 2][j]);
			mid[j] = 0.5 * (prev[j] + next[j]);
		}
		
		for( j = numPoints - 1; j > i + 3; j-- )
			VectorCopy( points[j - 2], points[j] );
		
		VectorCopy( prev, points[i + 1] );
		VectorCopy( mid,  points[i + 2] );
		VectorCopy( next, points[i + 3] );

		// back up and recheck this set again, it may need more subdivision
		i -= 2;
	}
	
	for( i = 1; i < numPoints; i += 2 )
	{
		for( j = 0; j < 3; j++ )
		{
			prev[j] = 0.5 * (points[i][j] + points[i + 1][j] );
			next[j] = 0.5 * (points[i][j] + points[i - 1][j] );
			points[i][j] = 0.5 * (prev[j] + next[j]);
		}
	}
	
	// eliminate linear sections
	for( i = 0; i + 2 < numPoints; i++ )
	{
		VectorSubtract( points[i + 1], points[i], delta );
		len = VectorNormalize( delta );
		VectorSubtract( points[i + 2], points[i + 1], delta2 );
		len2 = VectorNormalize( delta2 );
		
		// if either edge is degenerate, then eliminate it
		if( len < 0.0625 || len2 < 0.0625 || DotProduct( delta, delta2 ) >= 1.0 )
		{
			for( j = i + 1; j + 1 < numPoints; j++ )
				VectorCopy( points[j + 1], points[j] );
			numPoints--;
			continue;
		}
	}
	
	// the number of iterations is 2^(points - 1) - 1
	numPoints >>= 1;
	iterations = 0;

	while( numPoints > 1 )
	{
		numPoints >>= 1;
		iterations++;
	}
	
	if( iterations > *maxIterations )
		*maxIterations = iterations;
}

/*
=================
ExpandLongestCurveAndMaxIterations

combined version
=================
*/
void ExpandLongestCurveAndMaxIterations( vec_t sub, patchmesh_t *m, int i, int j, vec_t *longestCurve, int *maxIterations )
{
	ExpandLongestCurve( longestCurve, m->verts[i*m->width+j].point, m->verts[i*m->width+(j+1)].point, m->verts[i*m->width+(j+2)].point );
	ExpandLongestCurve( longestCurve, m->verts[i*m->width+j].point, m->verts[(i+1)*m->width+j].point, m->verts[(i+2)*m->width+j].point );
	ExpandMaxIterations( maxIterations, sub, m->verts[i*m->width+j].point, m->verts[i*m->width+(j+1)].point, m->verts[i*m->width+(j+2)].point );
	ExpandMaxIterations( maxIterations, sub, m->verts[i*m->width+j].point, m->verts[(i+1)*m->width+j].point, m->verts[(i+2) * m->width+j].point );
}

/*
=================
IterationsForCurve

given a curve of a certain length, return the number of subdivision iterations
note: this is affected by subdivision amount
=================
*/
int IterationsForCurve( vec_t len, int subdivisions )
{
	int	iterations, facets;

	// calculate the number of subdivisions
	for( iterations = 0; iterations < 3; iterations++ )
	{
		facets = subdivisions * 16 * pow( 2, iterations );
		if( facets >= len )
			break;
	}

	return iterations;
}

/*
=================
ParsePatch

creates a mapDrawSurface_t from the patch text
=================
*/
void ParsePatch( mapent_t *mapent, short entindex, short faceinfo, short &brush_type )
{
	vec4_t		delta, delta2, delta3;
	trivert_t	*v1, *v2, *v3, *v4;
	int		maxIterations;
	vec_t		longestCurve;
	bool		degenerate;
	patchmesh_t	*patchMesh;
	side_t		patchSide;
	vec_t		info[5];
	int		count;
	int		i, j;

	CheckToken( "{" );

	if( !GetToken( true ))
		return;

	memset( &patchSide, 0, sizeof( patchSide ));
	Q_strncpy( patchSide.name, token, sizeof( patchSide.name ));
	SetBits( patchSide.flags, FSIDE_PATCH );
	patchSide.contents = CONTENTS_EMPTY;
	patchSide.faceinfo = faceinfo;
	Parse1DMatrix( 5, info );

	if( info[0] < 0 || info[0] > MAX_PATCH_SIZE || info[1] < 0 || info[1] > MAX_PATCH_SIZE )
		COM_FatalError( "patch bad size\n" );

	patchMesh = AllocPatchMesh( (int)info[0], (int)info[1] );

	CheckToken( "(" );

	for( j = 0; j < patchMesh->width ; j++ )
	{
		CheckToken( "(" );

		for( i = 0; i < patchMesh->height; i++ )
			Parse1DMatrix( 5, (vec_t *)patchMesh->verts[i * patchMesh->width + j].point );

		CheckToken( ")" );
	}

	CheckToken( ")" );
	GetToken( true );

	// if brush primitives format, we may have some epairs to ignore here
	if( brush_type == BRUSH_RADIANT && Q_strcmp( token, "}" ))
		InsertLinkBefore( ParseEpair(), (entity_t *)mapent );
	else UnGetToken();

	CheckToken( "}" );
	CheckToken( "}" );

	// delete and warn about degenerate patches
	j = (patchMesh->width * patchMesh->height);
	VectorClear( delta );
	degenerate = true;
	delta[3] = 0;

	// find first valid vector
	for( i = 1; i < j && delta[3] == 0; i++ )
	{
		VectorSubtract( patchMesh->verts[0].point, patchMesh->verts[i].point, delta );
		delta[3] = VectorNormalize( delta );
	}

	// secondary degenerate test
	if( delta[3] != 0 )
	{
		// if all vectors match this or are zero, then this is a degenerate patch
		for( i = 1; i < j && degenerate; i++ )
		{
			VectorSubtract( patchMesh->verts[0].point, patchMesh->verts[i].point, delta2 );
			delta2[3] = VectorNormalize( delta2 );

			if( delta2[3] != 0 )
			{
				VectorCopy( delta2, delta3 );
				delta3[3] = delta2[3];
				VectorNegate( delta3, delta3 );
				
				if( !VectorCompare( delta, delta2 ) && !VectorCompare( delta, delta3 ))
					degenerate = false;
			}
		}
	}

	if( degenerate )
	{
		MsgDev( D_ERROR, "Entity %i, Brush %i: degenerate patch\n", g_numparsedentities, g_numparsedbrushes );
		FreePatchMesh( patchMesh );
		return;
	}

	longestCurve = 0.0f;
	maxIterations = 0;

	// find longest curve on the mesh
	for( j = 0; j + 2 < patchMesh->width; j += 2 )
	{
		for( i = 0; i + 2 < patchMesh->height; i += 2 )
		{
			ExpandLongestCurveAndMaxIterations( PATCH_SUBDIVISION, patchMesh, i, j, &longestCurve, &maxIterations );
		}
	}

	int		iterations = IterationsForCurve( longestCurve, PATCH_SUBDIVISION );
	patchmesh_t	*subdivided = SubdivideMesh( patchMesh, maxIterations /* iterations */ );
	patchmesh_t	*finalMesh;
	int		stitch = 0;

	PutMeshOnCurve( subdivided );
	finalMesh = RemoveLinearMeshColumnsRows( subdivided );
	FreePatchMesh( subdivided );
	count = 0;

	for( i = 0; i < finalMesh->width - 1; i++ )
	{
		for( j = 0; j < finalMesh->height - 1; j++ )
		{
			v1 = finalMesh->verts + j * finalMesh->width + i;
			v2 = v1 + 1;
			v3 = v1 + finalMesh->width + 1;
			v4 = v1 + finalMesh->width;

			if( MakeBrushFor4Points( mapent, &patchSide, entindex, v1, v4, v3, v2 ))
			{
				count++;
			}
			else
			{
				if( MakeBrushFor3Points( mapent, &patchSide, entindex, v1, v4, v3 ))
					count++;

				if( MakeBrushFor3Points( mapent, &patchSide, entindex, v1, v3, v2 ))
					count++;
			}
		}
	}

	MsgDev( D_REPORT, "convert patch cp %d x %d: %d brushes, %i stitchs\n", finalMesh->width, finalMesh->height, count, stitch );
	FreePatchMesh( patchMesh );
	FreePatchMesh( finalMesh );
}