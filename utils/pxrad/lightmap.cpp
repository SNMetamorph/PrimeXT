/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "qrad.h"
#include "imagelib.h"

typedef struct facelist_s
{
	word		facenum;
	facelist_s	*next;
} facelist_t;

static int			g_numnormals;
static dnormal_t	g_dnormals[MAX_MAP_NORMS];
static int			g_numvertnormals;
static dvertnorm_t	g_vertnormals[MAX_MAP_VERTNORMS];
faceneighbor_t		g_faceneighbor[MAX_MAP_FACES];
vec3_t				g_face_centroids[MAX_MAP_FACES];
static facelist_t	*g_dependentfacelights[MAX_MAP_FACES];
static float		g_normal_blend = cos( DEG2RAD( 2.0 ));
edgeshare_t			*g_edgeshare;

vec_t *GetTotalLight( patch_t *patch, int style )
{
	for( int i = 0; i < MAXLIGHTMAPS && patch->totalstyle[i] != 255; i++ )
	{
		if( patch->totalstyle[i] == style )
			return patch->totallight[i];
	}
	return vec3_origin;
}

vec_t *GetDirectLight( patch_t *patch, int style )
{
	for( int i = 0; i < MAXLIGHTMAPS && patch->totalstyle[i] != 255; i++ )
	{
		if( patch->totalstyle[i] == style )
			return patch->directlight[i];
	}
	return vec3_origin;
}

vec_t *GetTotalDirection( patch_t *patch, int style )
{
#ifdef HLRAD_DELUXEMAPPING
	for( int i = 0; i < MAXLIGHTMAPS && patch->totalstyle[i] != 255; i++ )
	{
		if( patch->totalstyle[i] == style )
			return patch->totallight_dir[i];
	}
#endif
	return vec3_origin;
}

/*
===================
StoreNormal
===================
*/
unsigned short StoreNormal( const vec3_t normal )
{
	int i;
	static bool	showerror = false;

	// search if already added
	for( i = 1; i < g_numnormals; i++ )
	{
		dnormal_t	*dv = &g_dnormals[i];

		if( DotProduct( dv->normal, normal ) > g_normal_blend )
			return i;		
	}

	// normals limit don't stop the compilation, just show non-fatal error
	// we can use unsmoothed normals in this case...
	if( g_numnormals == MAX_MAP_NORMS )
	{
		if( !showerror )
		{
			MsgDev( D_ERROR, "MAX_MAP_NORMS limit exceeded\n" );
			showerror = true;
		}
		return 0;
	}

	// put new elem
	VectorCopy( normal, g_dnormals[i].normal );

	return g_numnormals++;
}

// Must guarantee these faces will form a loop or a chain, otherwise will result in endless loop.
//
//   e[end]/enext[endnext]
//  *
//  |\.
//  |a\ fnext
//  |  \,
//  | f \.
//  |    \.
//  e   enext
//
int AddFaceForVertexNormal( const int edgeabs, int &edgeabsnext, const int edgeend, int &edgeendnext, const dface_t *f, dface_t *&fnext, vec_t &angle )
{
	int	vnum = g_dedges[edgeabs].v[edgeend];
	int	iedge, iedgenext, edge, edgenext;
	int	i, e, count1, count2;
	vec_t	dot;

	for( count1 = count2 = 0, i = 0; i < f->numedges; i++ )
	{
		e = g_dsurfedges[f->firstedge + i];

		if( g_dedges[abs(e)].v[0] == g_dedges[abs(e)].v[1] )
			continue;

		if( abs(e) == edgeabs )
		{
			iedge = i;
			edge = e;
			count1++;
		}
		else if( g_dedges[abs(e)].v[0] == vnum || g_dedges[abs(e)].v[1] == vnum )
		{
			iedgenext = i;
			edgenext = e;
			count2++;
		}
	}

	if( count1 != 1 || count2 != 1 )
		return -1;

	int	vnum11, vnum12, vnum21, vnum22;
	vec3_t	vec1, vec2;

	vnum11 = g_dedges[abs(edge)].v[edge > 0 ? 0 : 1];
	vnum12 = g_dedges[abs(edge)].v[edge > 0 ? 1 : 0];
	vnum21 = g_dedges[abs(edgenext)].v[edgenext > 0 ? 0 : 1];
	vnum22 = g_dedges[abs(edgenext)].v[edgenext > 0 ? 1 : 0];

	if( vnum == vnum12 && vnum == vnum21 && vnum != vnum11 && vnum != vnum22 )
	{
		VectorSubtract( g_dvertexes[vnum11].point, g_dvertexes[vnum].point, vec1 );
		VectorSubtract( g_dvertexes[vnum22].point, g_dvertexes[vnum].point, vec2 );
		edgeabsnext = abs( edgenext );
		edgeendnext = edgenext > 0 ? 0 : 1;
	}
	else if( vnum == vnum11 && vnum == vnum22 && vnum != vnum12 && vnum != vnum21 )
	{
		VectorSubtract( g_dvertexes[vnum12].point, g_dvertexes[vnum].point, vec1 );
		VectorSubtract( g_dvertexes[vnum21].point, g_dvertexes[vnum].point, vec2 );
		edgeabsnext = abs( edgenext );
		edgeendnext = edgenext > 0 ? 1 : 0;
	}
	else
	{
		return -1;
	}

	VectorNormalize( vec1 );
	VectorNormalize( vec2 );
	dot = DotProduct( vec1, vec2 );
	dot = dot > 1 ? 1 : dot < -1 ? -1 : dot;
	angle = acos( dot );

	edgeshare_t	*es = &g_edgeshare[edgeabsnext];

	if(!( es->faces[0] && es->faces[1] ))
		return 1;

	if( es->faces[0] == f && es->faces[1] != f )
		fnext = es->faces[1];
	else if( es->faces[1] == f && es->faces[0] != f )
		fnext = es->faces[0];
	else return -1;

	return 0;
}

void TranslateWorldToTex( int facenum, matrix3x4 out )
{
	vec_t		lmvecs[2][4];
	dtexinfo_t	*tex;
	const dplane_t	*fp;
	dface_t		*f;

	f = &g_dfaces[facenum];
	tex = &g_texinfo[f->texinfo];
	fp = GetPlaneFromFace( f );

	LightMatrixFromTexMatrix( tex, lmvecs );

	for( int i = 0; i < 3; i++ )
	{
		out[i][0] = lmvecs[0][i];
		out[i][1] = lmvecs[1][i];
		out[i][2] = fp->normal[i];
	}

	out[3][0] = lmvecs[0][3];
	out[3][1] = lmvecs[1][3];
	out[3][2] = -fp->dist;
}

/*
============
TranslateTexToTex

This function creates a matrix that can translate texture coords in face1 into texture coords in face2.
It keeps all points in the common edge invariant. For example, if there is a point in the edge,
and in the texture of face1, its (s,t) = (16,0), and in face2, its (s,t) = (128,64), then we must let matrix * (16,0,0) = (128,64,0)
============
*/
static bool TranslateTexToTex( int facenum1, int edgenum, int facenum2, matrix3x4 out1, matrix3x4 out2 )
{
	dvertex_t		*vert[2];
	vec3_t		face1_vert[2];
	vec3_t		face2_vert[2];
	vec3_t		face1_axis[2];
	vec3_t		face2_axis[2];
	const vec3_t	v_up = { 0, 0, 1 };
	matrix3x4		edgetotex1, edgetotex2;
	matrix3x4		worldtotex1, worldtotex2;
	matrix3x4		inv1, inv2;
	vec_t		len1, len2;
	dedge_t		*e;

	TranslateWorldToTex( facenum1, worldtotex1 );
	TranslateWorldToTex( facenum2, worldtotex2 );

	e = &g_dedges[edgenum];

	for( int i = 0; i < 2; i++ )
	{
		vert[i] = &g_dvertexes[e->v[i]];
		Matrix3x4_VectorTransform( worldtotex1, vert[i]->point, face1_vert[i] );
		face1_vert[i][2] = 0.0; // don't need we are in texture space
		Matrix3x4_VectorTransform( worldtotex2, vert[i]->point, face2_vert[i] );
		face2_vert[i][2] = 0.0; // don't need we are in texture space
	}

	VectorSubtract( face1_vert[1], face1_vert[0], face1_axis[0] );
	len1 = VectorLength( face1_axis[0] );
	CrossProduct( v_up, face1_axis[0], face1_axis[1] );

	if( Matrix3x4_CalcSign( worldtotex1 ) < 0.0 )
	{
		// the three vectors s, t, facenormal are in reverse order
		VectorNegate( face1_axis[1], face1_axis[1] );
	}

	VectorSubtract( face2_vert[1], face2_vert[0], face2_axis[0] );
	len2 = VectorLength( face2_axis[0] );
	CrossProduct( v_up, face2_axis[0], face2_axis[1] );

	if( Matrix3x4_CalcSign( worldtotex2 ) < 0.0 )
	{
		// the three vectors s, t, facenormal are in reverse order
		VectorNegate( face2_axis[1], face2_axis[1] );
	}

	VectorCopy( face1_axis[0], edgetotex1[0] );	// / v[0][0] v[1][0] \ is a rotation (possibly with a reflection by the edge)
	VectorCopy( face1_axis[1], edgetotex1[1] );	// \ v[0][1] v[1][1] / 
	VectorScale( v_up, len1, edgetotex1[2] );	// encode the length into the 3rd value of the matrix
	VectorCopy( face1_vert[0], edgetotex1[3] );	// map (0,0) into the origin point

	VectorCopy( face2_axis[0], edgetotex2[0] );
	VectorCopy( face2_axis[1], edgetotex2[1] );
	VectorScale( v_up, len2, edgetotex2[2] );
	VectorCopy( face2_vert[0], edgetotex2[3] );

	// for some reasons matrix can't be inverted
	if( !Matrix3x4_Invert_Full( inv1, edgetotex1 ) || !Matrix3x4_Invert_Full( inv2, edgetotex2 ))
		return false;

	Matrix3x4_ConcatTransforms( out1, edgetotex2, inv1 );
	Matrix3x4_ConcatTransforms( out2, edgetotex1, inv2 );

	return true;
}

/*
============
BuildFaceNeighbors
============
*/
void BuildFaceNeighbors( void )
{
	int		m, j, e;
	int		facenum1;
	int		facenum2;
	int		tmpneighbor[64];
	int		numneighbors;
	vec_t		lmmins[2], lmmaxs[2];
	vec_t		mins[2], maxs[2], val;
	float		lmvecs[2][4];
	dtexinfo_t	*tex;
	const dplane_t	*dp1;
	const dplane_t	*dp2;
	const edgeshare_t	*es;
	const dface_t	*f1;
	const dface_t	*f2;
	faceneighbor_t	*fn;
	dvertex_t		*v;

	// store a list of every face that uses a particular vertex
	for( facenum1 = 0; facenum1 < g_numfaces; facenum1++ )
	{
		fn = &g_faceneighbor[facenum1];
		numneighbors = 0;

		f1 = &g_dfaces[facenum1];
		dp1 = GetPlaneFromFace( f1 );
		tex = &g_texinfo[f1->texinfo];
		VectorCopy( dp1->normal, fn->facenormal );
		mins[0] = lmmins[0] = mins[1] = lmmins[1] = 999999;
		maxs[0] = lmmaxs[0] = maxs[1] = lmmaxs[1] =-999999;

		LightMatrixFromTexMatrix( tex, lmvecs );

		for( j = 0; j < f1->numedges; j++ )
		{
			e = g_dsurfedges[f1->firstedge + j];

			if( e >= 0 ) v = &g_dvertexes[g_dedges[e].v[0]];
			else v = &g_dvertexes[g_dedges[-e].v[1]];

			for( m = 0; m < 2; m++ )
			{
				/* The following calculation is sensitive to floating-point
			 	* precision.  It needs to produce the same result that the
			 	* light compiler does, because R_BuildLightMap uses surf->
			 	* extents to know the width/height of a surface's lightmap,
			 	* and incorrect rounding here manifests itself as patches
			 	* of "corrupted" looking lightmaps.
			 	* Most light compilers are win32 executables, so they use
			 	* x87 floating point.  This means the multiplies and adds
			 	* are done at 80-bit precision, and the result is rounded
			 	* down to 32-bits and stored in val.
			 	* Adding the casts to double seems to be good enough to fix
			 	* lighting glitches when Quakespasm is compiled as x86_64
			 	* and using SSE2 floating-point.  A potential trouble spot
			 	* is the hallway at the beginning of mfxsp17.  -- ericw
			 	*/
				val =	((double)v->point[0] * (double)tex->vecs[m][0]) +
					((double)v->point[1] * (double)tex->vecs[m][1]) +
					((double)v->point[2] * (double)tex->vecs[m][2]) +
					(double)tex->vecs[m][3];
				mins[m] = Q_min( val, mins[m] );
				maxs[m] = Q_max( val, maxs[m] );

				val =	((double)v->point[0] * (double)lmvecs[m][0]) +
					((double)v->point[1] * (double)lmvecs[m][1]) +
					((double)v->point[2] * (double)lmvecs[m][2]) +
					(double)lmvecs[m][3];
				lmmins[m] = Q_min( val, lmmins[m] );
				lmmaxs[m] = Q_max( val, lmmaxs[m] );
			}

			es = &g_edgeshare[abs( e )];
			if( !es->smooth ) continue;

			f2 = es->faces[e > 0 ? 1 : 0];
			facenum2 = f2 - g_dfaces;
			dp2 = GetPlaneFromFace( f2 );

			if( DotProduct( dp1->normal, dp2->normal ) < -NORMAL_EPSILON )
				continue;

			// look to see if we've already added this one
			for( m = 0; m < numneighbors; m++ )
			{
				if( tmpneighbor[m] == facenum2 )
					break;
			}
				
			if( m >= numneighbors )
			{
				// add to neighbor list
				tmpneighbor[m] = facenum2;
				numneighbors++;

				if( numneighbors > ARRAYSIZE( tmpneighbor ))
					COM_FatalError( "BuildFaceNeighbors: stack overflow\n" );
			}
		}

		// calc face extents for traceline and lightmap extents for LightForPoint
		for( j = 0; j < 2; j++ )
		{
			mins[j] = floor( mins[j] );
			maxs[j] = ceil( maxs[j] );

			fn->texmins[j] = mins[j];
			fn->extents[j] = (maxs[j] - mins[j]);

			if( FBitSet( tex->flags, TEX_WORLD_LUXELS ))
			{
				lmmins[j] = floor( lmmins[j] );
				lmmaxs[j] = ceil( lmmaxs[j] );

				fn->lightmapmins[j] = lmmins[j];
				fn->lightextents[j] = (lmmaxs[j] - lmmins[j]);
			}
			else
			{
				// just copy texturemins
				fn->lightmapmins[j] = fn->texmins[j];
				fn->lightextents[j] = fn->extents[j];
			}
		}

		if( numneighbors )
		{
			// copy over neighbor list
			fn->neighbor = (int *)Mem_Alloc( numneighbors * sizeof( fn->neighbor[0] ));
			fn->numneighbors = numneighbors;

			for( m = 0; m < numneighbors; m++ )
			{
				fn->neighbor[m] = tmpneighbor[m];
			}
		}
	}
}

void FreeFaceNeighbors( void )
{
	faceneighbor_t	*fn;

	for( int i = 0; i < g_numfaces; i++ )
	{
		fn = &g_faceneighbor[i];
		Mem_Free( fn->neighbor );
		fn->neighbor = NULL;
	}
}

void GetPhongNormal( int facenum, const vec3_t spot, vec3_t phongnormal )
{
	int		j, s;
	const dface_t	*f = g_dfaces + facenum;
	const dplane_t	*p = GetPlaneFromFace( f );
	vec3_t		facenormal;

	VectorCopy( p->normal, facenormal );
	VectorCopy( facenormal, phongnormal );

	if( g_smoothing_threshold != 0 )
	{
		// Calculate modified point normal for surface
		// Use the edge normals iff they are defined.  Bend the surface towards the edge normal(s)
		// Crude first attempt: find nearest edge normal and do a simple interpolation with facenormal.
		// Second attempt: find edge points+center that bound the point and do a three-point triangulation(baricentric)
		// Better third attempt: generate the point normals for all vertices and do baricentric triangulation.

		for( j = 0; j < f->numedges; j++ )
		{
			vec3_t		p1, p2, v1, v2, vspot;
			uint		prev_edge, next_edge;
			vec_t		a1, a2, aa, bb, ab;
			edgeshare_t	*es0, *es1, *es2;
			int		e0, e1, e2;
			vec3_t		n1, n2;
			dface_t		*f2;

			if( j != 0 ) prev_edge = f->firstedge + ((j + f->numedges - 1) % f->numedges);
			else prev_edge = f->firstedge + f->numedges - 1;

			if(( j + 1 ) != f->numedges ) next_edge = f->firstedge + ((j + 1) % f->numedges);
			else next_edge = f->firstedge;

			e0 = g_dsurfedges[f->firstedge + j];
			e1 = g_dsurfedges[prev_edge];
			e2 = g_dsurfedges[next_edge];

			es0 = &g_edgeshare[abs(e0)];
			es1 = &g_edgeshare[abs(e1)];
			es2 = &g_edgeshare[abs(e2)];

			if(( !es0->smooth || es0->coplanar ) && ( !es1->smooth || es1->coplanar ) && ( !es2->smooth || es2->coplanar ))
				continue;

			if( e0 > 0 )
			{
				f2 = es0->faces[1];
				VectorCopy( g_dvertexes[g_dedges[e0].v[0]].point, p1 );
				VectorCopy( g_dvertexes[g_dedges[e0].v[1]].point, p2 );
			}
			else
			{
				f2 = es0->faces[0];
				VectorCopy( g_dvertexes[g_dedges[-e0].v[1]].point, p1 );
				VectorCopy( g_dvertexes[g_dedges[-e0].v[0]].point, p2 );
			}

			// adjust for origin-based models
			VectorAdd( p1, g_face_offset[facenum], p1 );
			VectorAdd( p2, g_face_offset[facenum], p2 );

			for( s = 0; s < 2; s++ )
			{
				vec3_t	s1, s2;

				if( s == 0 ) VectorCopy( p1, s1 );
				else VectorCopy( p2, s1 );
				VectorAdd( p1, p2, s2 ); // edge center
				VectorScale( s2, 0.5, s2 );

				VectorSubtract( s1, g_face_centroids[facenum], v1 );
				VectorSubtract( s2, g_face_centroids[facenum], v2 );

				VectorSubtract( spot, g_face_centroids[facenum], vspot );

				aa = DotProduct( v1, v1 );
				bb = DotProduct( v2, v2 );
				ab = DotProduct( v1, v2 );
				a1 = (bb * DotProduct( v1, vspot ) - ab * DotProduct( vspot, v2 )) / (aa * bb - ab * ab);
				a2 = (DotProduct( vspot, v2 ) - a1 * ab) / bb;

				// test center to sample vector for inclusion between center to vertex vectors (Use dot product of vectors)
				if( a1 >= -0.01 && a2 >= -0.01 )
				{
					// calculate distance from edge to pos
					vec3_t	temp;

					if( es0->smooth )
					{
						if( s == 0 ) VectorCopy( es0->vertex_normal[e0 > 0 ? 0 : 1], n1 );
						else VectorCopy( es0->vertex_normal[e0 > 0 ? 1 : 0], n1 );
					}
					else if( s == 0 && es1->smooth )
					{
						VectorCopy( es1->vertex_normal[e1 > 0 ? 1 : 0], n1 );
					}
					else if( s == 1 && es2->smooth )
					{
						VectorCopy( es2->vertex_normal[e2 > 0 ? 0 : 1], n1 );
					}
					else
					{
						VectorCopy( facenormal, n1 );
					}

					if( es0->smooth )
						VectorCopy( es0->interface_normal, n2 );
					else VectorCopy( facenormal, n2 );

					// Interpolate between the center and edge normals based on sample position
					VectorScale( facenormal, 1.0 - a1 - a2, phongnormal );
					VectorScale( n1, a1, temp );
					VectorAdd( phongnormal, temp, phongnormal );
					VectorScale( n2, a2, temp );
					VectorAdd( phongnormal, temp, phongnormal );
					VectorNormalize( phongnormal );
					break;
				}
			}
		}
	}
}

/*
============
PairEdges
============
*/
void PairEdges( void )
{
	int		edgeabs, absnext;
	int		edgeend, endnext;
	vec3_t		normal, normals, edgenormal;
	int		i, j, k, d, r, count;
	dface_t		*f, *fcur, *fnext;
	vec_t		angle, angles;
	edgeshare_t	*e;

	g_edgeshare = (edgeshare_t *)Mem_Alloc( sizeof( edgeshare_t ) * g_numedges );

	for( i = 0, f = g_dfaces; i < g_numfaces; i++, f++ )
	{
		const dtexinfo_t	*tex = &g_texinfo[f->texinfo];
		const dplane_t	*fp = GetPlaneFromFace( f );

		if( FBitSet( tex->flags, TEX_SPECIAL ))
		{
			// special textures don't have lightmaps
			continue;
		}

		for( j = 0; j < f->numedges; j++ )
		{
			k = g_dsurfedges[f->firstedge + j];

			if( k < 0 )
			{
				e = &g_edgeshare[-k];
				e->faces[1] = f;
			}
			else
			{
				e = &g_edgeshare[k];
				e->faces[0] = f;
			}

			if( e->faces[0] && e->faces[1] )
			{
				// determine if coplanar
				if( e->faces[0]->planenum == e->faces[1]->planenum && e->faces[0]->side == e->faces[1]->side )
				{
					VectorCopy( GetPlaneFromFace( e->faces[0] )->normal, e->interface_normal );
					e->cos_normals_angle = 1.0;
					e->coplanar = true;
				}
				else
				{
					// see if they fall into a "smoothing group" based on angle of the normals
					vec3_t          normals[2];

					VectorCopy( GetPlaneFromFace( e->faces[0] )->normal, normals[0] );
					VectorCopy( GetPlaneFromFace( e->faces[1] )->normal, normals[1] );
					e->cos_normals_angle = DotProduct( normals[0], normals[1] );

                    			if( e->cos_normals_angle > ( 1.0 - NORMAL_EPSILON ))
					{
						VectorCopy( GetPlaneFromFace( e->faces[0] )->normal, e->interface_normal );
						e->cos_normals_angle = 1.0;
						e->coplanar = true;
					}
					else if( g_smoothing_threshold > 0.0 )
					{
						if( e->cos_normals_angle >= g_smoothing_threshold )
						{
							VectorAdd( normals[0], normals[1], e->interface_normal );
							VectorNormalize( e->interface_normal );
						}
					}
				}

				if( !VectorIsNull( e->interface_normal ))
					e->smooth = true;

				if( e->smooth )
				{
					int	facenum1 = e->faces[0] - g_dfaces;
					int	facenum2 = e->faces[1] - g_dfaces;

					// compute the matrix in advance
					if( !TranslateTexToTex( facenum1, abs( k ), facenum2, e->textotex[0], e->textotex[1] ))
					{
						VectorClear( e->interface_normal );
						e->coplanar = false;
						e->smooth = false;
						MsgDev( D_WARN, "TranslateTexToTex failed on face %d and %d\n", facenum1, facenum2 );
					}
				}
			}
		}
	}

	for( edgeabs = 0; edgeabs < g_numedges; edgeabs++ )
	{
		e = &g_edgeshare[edgeabs];
		if( !e->smooth ) continue;

		VectorCopy( e->interface_normal, edgenormal );

		if( g_dedges[edgeabs].v[0] != g_dedges[edgeabs].v[1] )
		{
			const dplane_t	*p0 = GetPlaneFromFace( e->faces[0] );
			const dplane_t	*p1 = GetPlaneFromFace( e->faces[1] );

			for( edgeend = 0; edgeend < 2; edgeend++ )
			{
				VectorClear( normals );
				angles = 0;

				for( d = 0; d < 2; d++ )
				{
					f = e->faces[d];
					absnext = edgeabs;
					endnext = edgeend;
					count = 0;
					fnext = f;

					while( 1 )
					{
						fcur = fnext;
						r = AddFaceForVertexNormal( absnext, absnext, endnext, endnext, fcur, fnext, angle );
						count++;

						if( r == -1 || count > 128 )
						{
							MsgDev( D_WARN, "PairEdges: face edges mislink\n" );
							break;
						}

						VectorCopy( GetPlaneFromFace( fcur )->normal, normal );

						if( DotProduct( normal, p0->normal ) <= NORMAL_EPSILON )
							break;

						if( DotProduct( normal, p1->normal ) <= NORMAL_EPSILON )
							break;

						if( DotProduct( edgenormal, normal ) + NORMAL_EPSILON < g_smoothing_threshold )
							break;

						VectorMA( normals, angle, normal, normals );
						angles += angle;

						if( r != 0 || fnext == f )
							break;
					}
				}

				if( angles < NORMAL_EPSILON )
				{
					VectorCopy( edgenormal, e->vertex_normal[edgeend] );
					MsgDev( D_WARN, "PairEdges: no valid faces\n" );
				}
				else
				{
					VectorNormalize( normals );
					VectorCopy( normals, e->vertex_normal[edgeend] );
				}
			}
		}
		else
		{
			MsgDev( D_WARN, "PairEdges: invalid edge\n" );
			VectorCopy( edgenormal, e->vertex_normal[0] );
			VectorCopy( edgenormal, e->vertex_normal[1] );
		}

		if( e->coplanar )
		{
			if( !VectorCompare( e->vertex_normal[0], e->interface_normal ))
				e->coplanar = false;

			if( !VectorCompare( e->vertex_normal[1], e->interface_normal ))
				e->coplanar = false;
		}
	}
}

void FreeSharedEdges( void )
{
	Mem_Free( g_edgeshare );
	g_edgeshare = NULL;
}

/*
=================================================================

  LIGHTMAP SAMPLE GENERATION

=================================================================
*/
/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
void CalcFaceExtents( lightinfo_t *l )
{
	vec_t		mins[2];
	vec_t		maxs[2];
	float		lmvecs[2][4];
	int		i, j, e;
	dtexinfo_t	*tex;
	vec_t		val;
	dvertex_t		*v;
	dface_t		*s;
	
	s = l->face;

	mins[0] = mins[1] =  999999;
	maxs[0] = maxs[1] = -999999;

	tex = &g_texinfo[s->texinfo];

	int max_surface_extent = GetSurfaceExtent( tex );
	int texture_step = GetTextureStep( tex );

	LightMatrixFromTexMatrix( tex, lmvecs );
	
	for( i = 0; i < s->numedges; i++ )
	{
		e = g_dsurfedges[s->firstedge+i];
		if( e >= 0 ) v = g_dvertexes + g_dedges[e].v[0];
		else v = g_dvertexes + g_dedges[-e].v[1];
		
		for( j = 0; j < 2; j++ )
		{
			/* The following calculation is sensitive to floating-point
			 * precision.  It needs to produce the same result that the
			 * light compiler does, because R_BuildLightMap uses surf->
			 * extents to know the width/height of a surface's lightmap,
			 * and incorrect rounding here manifests itself as patches
			 * of "corrupted" looking lightmaps.
			 * Most light compilers are win32 executables, so they use
			 * x87 floating point.  This means the multiplies and adds
			 * are done at 80-bit precision, and the result is rounded
			 * down to 32-bits and stored in val.
			 * Adding the casts to double seems to be good enough to fix
			 * lighting glitches when Quakespasm is compiled as x86_64
			 * and using SSE2 floating-point.  A potential trouble spot
			 * is the hallway at the beginning of mfxsp17.  -- ericw
			 */
			val =	((double)v->point[0] * (double)lmvecs[j][0]) +
				((double)v->point[1] * (double)lmvecs[j][1]) +
				((double)v->point[2] * (double)lmvecs[j][2]) +
				(double)lmvecs[j][3];
			mins[j] = Q_min( val, mins[j] );
			maxs[j] = Q_max( val, maxs[j] );
		}
	}

	for( i = 0; i < 2; i++ )
	{	
		l->exactmins[i] = mins[i];
		l->exactmaxs[i] = maxs[i];

		mins[i] = floor( mins[i] / texture_step );
		maxs[i] = ceil( maxs[i] / texture_step );

		l->texmins[i] = mins[i];
		l->texsize[i] = (maxs[i] - mins[i]);
	}

	if( !FBitSet( tex->flags, TEX_SPECIAL ))
	{
		if( l->texsize[0] * l->texsize[1] > ( MAX_SINGLEMAP / 3 ))
			COM_FatalError( "surface to large to map\n" );

		if( l->texsize[0] > max_surface_extent )
			MsgDev( D_ERROR, "bad surface extents %d > %d\n", l->texsize[0], max_surface_extent );

		if( l->texsize[1] > max_surface_extent )
			MsgDev( D_ERROR, "bad surface extents %d > %d\n", l->texsize[1], max_surface_extent );

		if( l->texsize[0] < 0 || l->texsize[1] < 0 )
			COM_FatalError( "negative extents\n" );
	}

	l->lmcache_density = 1;

	l->lmcache_side = (int)ceil(( 0.5 * g_blur * l->lmcache_density - 0.5 ) * ( 1.0 - NORMAL_EPSILON ));
	l->lmcache_offset = l->lmcache_side;
	l->lmcachewidth = l->texsize[0] * l->lmcache_density + 1 + 2 * l->lmcache_side;
	l->lmcacheheight = l->texsize[1] * l->lmcache_density + 1 + 2 * l->lmcache_side;

	l->light = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec3_t[MAXLIGHTMAPS] ));
#ifdef HLRAD_DELUXEMAPPING
	l->deluxe = (vec3_t (*)[MAXLIGHTMAPS])Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec3_t[MAXLIGHTMAPS] ));
	l->normals = (vec3_t *)Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec3_t ));
#ifdef HLRAD_SHADOWMAPPING
	l->shadow = (vec_t (*)[MAXLIGHTMAPS])Mem_Alloc( l->lmcachewidth * l->lmcacheheight * sizeof( vec_t[MAXLIGHTMAPS] ));
#endif
#endif
}

/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
void CalcFaceVectors( lightinfo_t *l )
{
	dtexinfo_t	*tex;
	vec_t		distscale;
	vec_t		dist, len;
	float		lmvecs[2][4];
	vec3_t		texnormal;
	int		i, j;

	tex = &g_texinfo[l->face->texinfo];

	LightMatrixFromTexMatrix( tex, lmvecs );
	
	// convert from float to double
	for( i = 0; i < 2; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			l->worldtotex[i][j] = lmvecs[i][j];
		}
	}

	// calculate a normal to the texture axis.  points can be moved along this
	// without changing their S/T
	CrossProduct( lmvecs[1], lmvecs[0], texnormal );
	VectorNormalize( texnormal );

	// flip it towards plane normal
	distscale = DotProduct( texnormal, l->plane->normal );
	if( !distscale )
	{
		MsgDev( D_WARN, "Texture axis perpendicular to face\n" );
		distscale = 1.0;
	}

	if( distscale < 0 )
	{
		VectorNegate( texnormal, texnormal );
		distscale = -distscale;
	}	

	// distscale is the ratio of the distance along the texture normal to
	// the distance along the plane normal
	distscale = 1.0 / distscale;

	for( i = 0; i < 2; i++ )
	{
		CrossProduct( l->worldtotex[!i], l->plane->normal, l->textoworld[i] );
		len = DotProduct( l->textoworld[i], l->worldtotex[i] );
		VectorScale( l->textoworld[i], ( 1.0 / len ), l->textoworld[i] );
	}

	// calculate texorg on the texture plane
	VectorMA( l->texorg, -lmvecs[0][3], l->textoworld[0], l->texorg );
	VectorMA( l->texorg, -lmvecs[1][3], l->textoworld[1], l->texorg );

	// project back to the face plane
	dist = ( DotProduct( l->texorg, l->plane->normal ) - l->plane->dist ) * distscale;
	VectorMA( l->texorg, -dist, texnormal, l->texorg );

	// compensate for org'd bmodels
	VectorAdd( l->texorg, g_face_offset[l->surfnum], l->texorg );	
}

/*
=================
TexelSpaceToWorld

translate texel coord into world coord
=================
*/
void TexelSpaceToWorld( const lightinfo_t *l, vec3_t world, const vec_t s, const vec_t t )
{
	VectorMA( l->texorg, s, l->textoworld[0], world );
	VectorMA( world, t, l->textoworld[1], world );
}

/*
=================
WorldToTexelSpace

translate world coord into texel coord
=================
*/
void WorldToTexelSpace( const lightinfo_t *l, const vec3_t world, vec_t &s, vec_t &t )
{
	vec3_t	pos;

	VectorSubtract( world, l->texorg, pos );
	s = DotProduct( pos, l->worldtotex[0] );
	t = DotProduct( pos, l->worldtotex[1] );
}

typedef struct
{
	int	edgenum;		// g_dedges index
	int	edgeside;
	int	nextfacenum;	// where to grow
	bool	tried;

	vec3_t	point1;		// start point
	vec3_t	point2;		// end point
	vec3_t	direction;	// normalized; from point1 to point2
	
	bool	noseam;
	vec_t	distance;		// distance from origin
	vec_t	distancereduction;
	vec_t	flippedangle;

	matrix3x4	prevtonext;
	matrix3x4	nexttoprev;
	vec_t	ratio;		// if ratio != 1, seam is unavoidable
} samplefragedge_t;

typedef struct
{
	dplane_t	planes[4];
} samplefragrect_t;

typedef struct samplefrag_s
{
	samplefrag_s	*next;		// since this is a node in a list
	samplefrag_s	*parentfrag;	// where it grew from
	samplefragedge_t	*parentedge;
	int		facenum;		// facenum

	vec_t		flippedangle;	// copied from parent edge
	bool		noseam;		// copied from parent edge

	matrix3x4		coordtomycoord;	// v[2][2] > 0, v[2][0] = v[2][1] = v[0][2] = v[1][2] = 0.0
	matrix3x4		mycoordtocoord;

	vec3_t		origin;		// original s,t
	vec3_t		myorigin;		// relative to the texture coordinate on that face
	samplefragrect_t	rect;		// original rectangle that forms the boundary
	samplefragrect_t	myrect;		// relative to the texture coordinate on that face

	winding_t		*winding;		// a fragment of the original rectangle in the texture coordinate plane;
					// windings of different frags should not overlap
	dplane_t		windingplane;	// normal = (0,0,1) or (0,0,-1); if this normal is wrong, PointInWinding() will never return true
	winding_t		*mywinding;	// relative to the texture coordinate on that face
	dplane_t		mywindingplane;

	int		numedges;		// # of candicates for the next growth
	samplefragedge_t	*edges;		// candicates for the next growth
} samplefrag_t;

typedef struct
{
	int		maxsize;
	int		size;
	samplefrag_t	*head;
} samplefraginfo_t;

/*
=================
ChopFrag

fill winding, windingplane, mywinding,
mywindingplane, numedges, edges
=================
*/
void ChopFrag( samplefrag_t *frag )
{
	// get the shape of the fragment by clipping the face using the boundaries
	const vec3_t	v_up = { 0, 0, 1 };
	winding_t		*facewinding;
	matrix3x4		worldtotex;
	dface_t		*f;
	int		x;

	f = &g_dfaces[frag->facenum];
	facewinding = WindingFromFace( f );

	TranslateWorldToTex( frag->facenum, worldtotex );
	frag->mywinding = AllocWinding( facewinding->numpoints );
	frag->mywinding->numpoints = facewinding->numpoints;

	// transform facewinding into texture space
	for( x = 0; x < facewinding->numpoints; x++ )
	{
		Matrix3x4_VectorTransform( worldtotex, facewinding->p[x], frag->mywinding->p[x] );
		frag->mywinding->p[x][2] = 0.0;
	}

	RemoveColinearPointsEpsilon( frag->mywinding, ON_EPSILON );
	VectorCopy( v_up, frag->mywindingplane.normal ); // this is the same as applying the worldtotex matrix to the faceplane

	if( Matrix3x4_CalcSign( worldtotex ) < 0.0 )
		frag->mywindingplane.normal[2] *= -1.0;
	frag->mywindingplane.dist = 0.0;

	for( x = 0; x < 4 && frag->mywinding != NULL; x++ )
		ChopWindingInPlace( &frag->mywinding, frag->myrect.planes[x].normal, frag->myrect.planes[x].dist, ON_EPSILON, false );

	if( !frag->mywinding )
	{
		FreeWinding( facewinding );
		return; // failed to chop
	}

	ASSERT( frag->mywinding != NULL );

	frag->winding = AllocWinding( frag->mywinding->numpoints );
	frag->winding->numpoints = frag->mywinding->numpoints;

	// translate coords from current surface to another
	for( x = 0; x < frag->mywinding->numpoints; x++ )
		Matrix3x4_VectorTransform( frag->mycoordtocoord, frag->mywinding->p[x], frag->winding->p[x] );

	RemoveColinearPointsEpsilon( frag->winding, ON_EPSILON );
	VectorCopy( frag->mywindingplane.normal, frag->windingplane.normal );

	if( Matrix3x4_CalcSign( frag->mycoordtocoord ) < 0.0 )
		frag->windingplane.normal[2] *= -1.0;
	frag->windingplane.dist = 0.0;

	FreeWinding( facewinding );

	// find the edges where the fragment can grow in the future
	frag->edges = (samplefragedge_t *)Mem_Alloc( f->numedges * sizeof( samplefragedge_t ));
	frag->numedges = 0;

	for( int i = 0; i < f->numedges; i++ )
	{
		samplefragedge_t	*e;
		edgeshare_t	*es;
		dedge_t		*de;
		dvertex_t		*dv1;
		dvertex_t		*dv2;
		matrix3x4		*m1;
		matrix3x4		*m2;
		vec_t		frac1, frac2;
		vec_t		dot0, dot1, dot2;
		vec3_t		tmp, v, normal;
		vec_t		edgelen;
		
		e = &frag->edges[frag->numedges];

		// some basic info
		e->edgenum = abs( g_dsurfedges[f->firstedge + i] );
		e->edgeside = (g_dsurfedges[f->firstedge + i] < 0 ? 1 : 0);
		es = &g_edgeshare[e->edgenum];
		if( !es->smooth ) continue;

		if(( es->faces[e->edgeside] - g_dfaces ) != frag->facenum )
			COM_FatalError( "ChopFrag: internal error\n" );

		e->nextfacenum = (es->faces[!e->edgeside] - g_dfaces);
		m1 = (matrix3x4 *)&es->textotex[e->edgeside];
		m2 = (matrix3x4 *)&es->textotex[!e->edgeside];

		if( e->nextfacenum == frag->facenum )
			continue;	// an invalid edge (usually very short)
		e->tried = false;	// because the frag hasn't been linked into the list yet

		// translate the edge points from world to the texture plane of the original frag
		// so the distances are able to be compared among edges from different frags
		de = &g_dedges[e->edgenum];
		dv1 = &g_dvertexes[de->v[e->edgeside]];
		dv2 = &g_dvertexes[de->v[!e->edgeside]];

		// translate to another frag
		Matrix3x4_VectorTransform( worldtotex, dv1->point, tmp );
		Matrix3x4_VectorTransform( frag->mycoordtocoord, tmp, e->point1 );
		e->point1[2] = 0.0;

		Matrix3x4_VectorTransform( worldtotex, dv2->point, tmp );
		Matrix3x4_VectorTransform( frag->mycoordtocoord, tmp, e->point2 );
		e->point2[2] = 0.0;

		VectorSubtract( e->point2, e->point1, e->direction );
		edgelen = VectorNormalize( e->direction );

		if( edgelen <= ON_EPSILON )
			continue;

		// clip the edge
		frac1 = 0;
		frac2 = 1;

		for( int x = 0; x < 4; x++ )
		{
			vec_t	dot1 = DotProduct( e->point1, frag->rect.planes[x].normal ) - frag->rect.planes[x].dist;
			vec_t	dot2 = DotProduct( e->point2, frag->rect.planes[x].normal ) - frag->rect.planes[x].dist;

			if( dot1 <= ON_EPSILON && dot2 <= ON_EPSILON )
			{
				frac1 = 1.0;
				frac2 = 0.0;
			}
			else if( dot1 < 0.0 )
			{
				frac1 = Q_max( frac1, dot1 / ( dot1 - dot2 ));
			}
			else if( dot2 < 0.0 )
			{
				frac2 = Q_min( frac2, dot1 / ( dot1 - dot2 ));
			}
		}

		if( edgelen * ( frac2 - frac1 ) <= ON_EPSILON )
			continue;

		VectorMA( e->point1, edgelen * frac2, e->direction, e->point2 );
		VectorMA( e->point1, edgelen * frac1, e->direction, e->point1 );

		// calculate the distance, etc., which are used to determine its priority
		dot0 = DotProduct( frag->origin, e->direction );
		dot1 = DotProduct( e->point1, e->direction );
		dot2 = DotProduct( e->point2, e->direction );
		e->noseam = frag->noseam;

		dot0 = Q_max( dot1, Q_min( dot0, dot2 ));
		VectorMA( e->point1, dot0 - dot1, e->direction, v );
		VectorSubtract( v, frag->origin, v );
		e->distance = VectorLength( v );
		CrossProduct( e->direction, frag->windingplane.normal, normal );
		VectorNormalize( normal ); // points inward
		e->distancereduction = DotProduct( v, normal );
		e->flippedangle = frag->flippedangle + acos( Q_min( es->cos_normals_angle, 1.0 ));

		// calculate the matrix
		e->ratio = (*m2)[2][2];

		if( e->ratio <= NORMAL_EPSILON || ( 1.0 / e->ratio ) <= NORMAL_EPSILON )
		{
			MsgDev( D_INFO, "TranslateTexToTex failed on face %d and %d\n", frag->facenum, e->nextfacenum );
			continue;
		}

		Matrix3x4_Copy( e->prevtonext, *m1 );
		Matrix3x4_Copy( e->nexttoprev, *m2 );

		if( fabs( e->ratio - 1.0 ) >= 0.005 )
			e->noseam = false;
		frag->numedges++;
	}
}

static samplefrag_t *GrowSingleFrag( const samplefraginfo_t *info, samplefrag_t *parent, samplefragedge_t *edge )
{
	dplane_t		clipplanes[MAX_POINTS_ON_WINDING];
	int		x, numclipplanes;
	bool		overlap;
	samplefrag_t	*frag;

	frag = (samplefrag_t *)Mem_Alloc( sizeof( samplefrag_t ));

	// some basic info
	frag->next = NULL;
	frag->parentfrag = parent;
	frag->parentedge = edge;
	frag->facenum = edge->nextfacenum;
	frag->flippedangle = edge->flippedangle;
	frag->noseam = edge->noseam;

	// calculate the matrix
	Matrix3x4_ConcatTransforms( frag->coordtomycoord, edge->prevtonext, parent->coordtomycoord );
	Matrix3x4_ConcatTransforms( frag->mycoordtocoord, parent->mycoordtocoord, edge->nexttoprev );

	// fill in origin
	VectorCopy( parent->origin, frag->origin );
	Matrix3x4_VectorTransform( frag->coordtomycoord, frag->origin, frag->myorigin );

	// fill in boundaries
	frag->rect = parent->rect;

	for( x = 0; x < 4; x++ )
	{
		// since a plane's parameters are in the dual coordinate space,
		// we translate the original absolute plane into this relative plane by multiplying the inverse matrix
		Matrix3x4_TransformStandardPlane( frag->mycoordtocoord, frag->rect.planes[x].normal, frag->rect.planes[x].dist,
		frag->myrect.planes[x].normal, &frag->myrect.planes[x].dist );
		vec_t	len = VectorLength( frag->myrect.planes[x].normal );

		if( !len )
		{
			MsgDev( D_INFO, "couldn't translate sample boundaries on face %d\n", frag->facenum );
			Mem_Free( frag );
			return NULL;
		}

		VectorScale( frag->myrect.planes[x].normal, (1.0 / len), frag->myrect.planes[x].normal );
		frag->myrect.planes[x].dist /= len;
	}

	// chop windings and edges
	ChopFrag( frag );

	if( !frag->winding || !frag->mywinding )
	{
		// empty
		if( frag->mywinding )
			FreeWinding( frag->mywinding );
		if( frag->winding )
			FreeWinding( frag->winding );
		Mem_Free( frag->edges );
		Mem_Free( frag );

		return NULL;
	}

	// do overlap test
	numclipplanes = 0;	
	overlap = false;

	for( x = 0; x < frag->winding->numpoints; x++ )
	{
		vec3_t	v;

		VectorSubtract( frag->winding->p[(x + 1) % frag->winding->numpoints], frag->winding->p[x], v );
		CrossProduct( v, frag->windingplane.normal, clipplanes[numclipplanes].normal );

		if( !VectorNormalize( clipplanes[numclipplanes].normal ))
			continue;

		clipplanes[numclipplanes].dist = DotProduct( frag->winding->p[x], clipplanes[numclipplanes].normal );
		numclipplanes++;
	}

	for( samplefrag_t *f2 = info->head; f2 && !overlap; f2 = f2->next )
	{
		winding_t	*w = CopyWinding( f2->winding );

		for( x = 0; x < numclipplanes && w != NULL; x++ )
			ChopWindingInPlace( &w, clipplanes[x].normal, clipplanes[x].dist, ON_EPSILON * 4, false );
		overlap = (w) ? true : false;
		if( w ) FreeWinding( w );
	}

	if( overlap )
	{
		// in the original texture plane, this fragment overlaps with some existing fragments
		if( frag->mywinding )
			FreeWinding( frag->mywinding );
		if( frag->winding )
			FreeWinding( frag->winding );
		Mem_Free( frag->edges );
		Mem_Free( frag );

		return NULL;
	}

	return frag;
}

static bool FindBestEdge( samplefraginfo_t *info, samplefrag_t *&bestfrag, samplefragedge_t *&bestedge )
{
	bool		found = false;
	bool		better;
	samplefrag_t	*f;
	samplefragedge_t	*e;

	for( f = info->head; f != NULL; f = f->next )
	{
		for( e = f->edges; e < f->edges + f->numedges; e++ )
		{
			if( e->tried ) continue;

			if( !found )
				better = true;
			else if(( e->flippedangle < M_PI + NORMAL_EPSILON ) != ( bestedge->flippedangle < M_PI + NORMAL_EPSILON ))
				better = (( e->flippedangle < M_PI + NORMAL_EPSILON ) && !( bestedge->flippedangle < M_PI + NORMAL_EPSILON ));
			else if( e->noseam != bestedge->noseam )
				better = (e->noseam && !bestedge->noseam);
			else if( fabs( e->distance - bestedge->distance ) > ON_EPSILON )
				better = ( e->distance < bestedge->distance );
			else if( fabs( e->distancereduction - bestedge->distancereduction ) > ON_EPSILON )
				better = ( e->distancereduction > bestedge->distancereduction );
			else better = e->edgenum < bestedge->edgenum;

			if( better )
			{
				found = true;
				bestfrag = f;
				bestedge = e;
			}
		}
	}

	return found;
}

static samplefraginfo_t *CreateSampleFrag( int facenum, vec_t s, vec_t t, const vec_t square[2][2], int maxsize )
{
	const vec3_t	v_s = { 1, 0, 0 };
	const vec3_t	v_t = { 0, 1, 0 };
	samplefraginfo_t	*info;

	info = (samplefraginfo_t *)Mem_Alloc( sizeof( samplefraginfo_t ));
	info->maxsize = maxsize;
	info->size = 1;
	info->head = (samplefrag_t *)Mem_Alloc( sizeof( samplefrag_t ));
	info->head->next = NULL;
	info->head->parentfrag = NULL;
	info->head->parentedge = NULL;
	info->head->facenum = facenum;
	info->head->flippedangle = 0.0;
	info->head->noseam = true;

	Matrix3x4_LoadIdentity( info->head->coordtomycoord );
	Matrix3x4_LoadIdentity( info->head->mycoordtocoord );

	VectorSet( info->head->origin, s, t, 0 );
	VectorCopy( info->head->origin, info->head->myorigin );

	VectorScale( v_s,  1.0, info->head->rect.planes[0].normal );
	info->head->rect.planes[0].dist =  square[0][0]; // smin
	VectorScale( v_s, -1.0, info->head->rect.planes[1].normal );
	info->head->rect.planes[1].dist = -square[1][0]; // smax
	VectorScale( v_t,  1.0, info->head->rect.planes[2].normal );
	info->head->rect.planes[2].dist =  square[0][1]; // tmin
	VectorScale( v_t, -1.0, info->head->rect.planes[3].normal );
	info->head->rect.planes[3].dist = -square[1][1]; // tmax

	info->head->myrect = info->head->rect;
	ChopFrag( info->head );

	if( !info->head->winding || !info->head->mywinding )
	{
		// empty
		if( info->head->mywinding )
			FreeWinding( info->head->mywinding );
		if( info->head->winding )
			FreeWinding( info->head->winding );
		Mem_Free( info->head->edges );
		Mem_Free( info->head );
		info->head = NULL;
		info->size = 0;
	}
	else
	{
		// prune edges
		for( samplefragedge_t *e = info->head->edges; e < info->head->edges + info->head->numedges; e++ )
		{
			if( e->nextfacenum == info->head->facenum )
				e->tried = true;
		}
	}

	while( info->size < info->maxsize )
	{
		samplefrag_t	*bestfrag;
		samplefragedge_t	*e, *bestedge;
		samplefrag_t	*f, *newfrag;

		if( !FindBestEdge( info, bestfrag, bestedge ))
			break;

		newfrag = GrowSingleFrag( info, bestfrag, bestedge );
		bestedge->tried = true;

		if( newfrag )
		{
			newfrag->next = info->head;
			info->head = newfrag;
			info->size++;


			for( f = info->head; f != NULL; f = f->next )
			{
				for( e = newfrag->edges; e < newfrag->edges + newfrag->numedges; e++ )
				{
					if( e->nextfacenum == f->facenum )
						e->tried = true;
				}
			}

			for( f = info->head; f != NULL; f = f->next )
			{
				for( e = f->edges; e < f->edges + f->numedges; e++ )
				{
					if( e->nextfacenum == newfrag->facenum )
						e->tried = true;
				}
			}
		}
	}

	return info;
}

static bool IsFragEmpty( samplefraginfo_t *fraginfo )
{
	return (fraginfo->size == 0);
}

static void DeleteSampleFrag( samplefraginfo_t *fraginfo )
{
	while( fraginfo->head )
	{
		samplefrag_t	*f;

		f = fraginfo->head;
		fraginfo->head = f->next;

		if( f->mywinding ) FreeWinding( f->mywinding );
		if( f->winding ) FreeWinding( f->winding );
		Mem_Free( f->edges );
		Mem_Free( f );
	}
	Mem_Free( fraginfo );
}

static bool CalcSamplePoint( vec3_t point, vec3_t position, int *surface, const lightinfo_t *l, vec_t orig_s, vec_t orig_t, const vec_t square[2][2] )
{
	bool		found = false;
	vec_t		bests, bestt;
	const dplane_t	*faceplane;
	samplefraginfo_t	*fraginfo;
	samplefrag_t	*bestfrag;
	vec_t		best_dist;
	vec3_t		bestpos;
	int			facenum;
	bool		outside;
	dface_t		*face;
	samplefrag_t	*f;

	facenum = l->surfnum;
	face = l->face;
	faceplane = GetPlaneFromFace( face );
	fraginfo = CreateSampleFrag( facenum, orig_s, orig_t, square, 100 );

	for( f = fraginfo->head; f != NULL; f = f->next )
	{
		vec_t	s, t, dist;
		bool	better;
		vec3_t	pos;

		if( !FindNearestPosition( f->facenum, f->mywinding, f->myorigin[0], f->myorigin[1], pos, &s, &t, &dist ))
			continue;

		if( !found )
			better = true;
		else if( fabs( dist - best_dist ) > ON_EPSILON * 2 )
			better = ( dist < best_dist );
		else if( f->noseam != bestfrag->noseam )
			better = (f->noseam && !bestfrag->noseam);
		else better = (f->facenum < bestfrag->facenum);

		if( better )
		{
			VectorCopy( pos, bestpos );
			best_dist = dist;
			bestfrag = f;
			found = true;
			bests = s;
			bestt = t;
		}
	}
	
	if( found )
	{
		matrix3x4	worldtotex, textoworld;
		vec3_t	tex;

		TranslateWorldToTex( bestfrag->facenum, worldtotex );

		if( !Matrix3x4_Invert_Full( textoworld, worldtotex ))
			MsgDev( D_WARN, "Texture axis perpendicular to face\n" );

		// adjust source point and store valid position
		VectorSet( tex, bests, bestt, 0.0 );
		Matrix3x4_VectorTransform( textoworld, tex, point );
		VectorAdd( point, g_face_offset[bestfrag->facenum], point );
		VectorCopy( bestpos, position );
		*surface = bestfrag->facenum;
		outside = false;
	}
	else
	{
		TexelSpaceToWorld( l, point, orig_s, orig_t );
		VectorMA( point, DEFAULT_HUNT_OFFSET, faceplane->normal, position );
		*surface = facenum;
		outside = true;
	}

	DeleteSampleFrag( fraginfo );
	
	return outside;
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
void CalcPoints( lightinfo_t *l )
{
	vec_t		starts, startt, us, ut;
	int		h = l->texsize[1] + 1;
	int		w = l->texsize[0] + 1;
	int		s, t, texture_step;
	vec_t		square[2][2];
	vec_t		step;

	l->surfpt = (surfpt_t *)Mem_Alloc( w * h * sizeof( surfpt_t ));
          texture_step = GetTextureStep( l->face );
	starts = (float)l->texmins[0] * texture_step;
	startt = (float)l->texmins[1] * texture_step;
	step = (float)texture_step;
	l->numsurfpt = w * h;

	for( t = 0; t < h; t++ )
	{
		for( s = 0; s < w; s++ )
		{
			surfpt_t	*surf = &l->surfpt[s+w*t];
			us = starts + s * step;
			ut = startt + t * step;
			square[0][0] = us - step;
			square[0][1] = ut - step;
			square[1][0] = us + step;
			square[1][1] = ut + step;
			surf->occluded = CalcSamplePoint( surf->point, surf->position, &surf->surface, l, us, ut, square );
		}
	}
}


//==============================================================
facelight_t	g_facelight[MAX_MAP_FACES];
directlight_t	*g_directlights;
int		g_numdlights;


directlight_t	g_fakeskylight;	//fake light for sky pvs culling
directlight_t	*g_skylights[256];
int				g_numskylights;


/*
=============
GetLightType

trying to determine light type by settings
=============
*/
emittype_t GetLightType( entity_t *e )
{
	const char *name = ValueForKey( e, "classname" );
	const char *target = ValueForKey( e, "target" );
	int style = IntForKey( e, "style" );

	if( Q_strncmp( name, "light", 5 ) && !CheckKey( e, "_sunlight" ))
		return emit_ignored; // not a light entity

#ifdef HLRAD_PARANOIA_BUMP
	if( style != 0 && style != LS_SKY )
		return emit_ignored;
#endif
	if( CheckKey( e, "_surface" ))
		return emit_ignored; // already handled

	// check for skylight or sunlight
	if( BoolForKey( e, "_sky" ) || CheckKey( e, "_sunlight" ) || !Q_strcmp( name, "light_environment" )) 
		return emit_skylight;

	// check for spotlight
	if( !Q_strcmp( name, "light_spot" ))
		return emit_spotlight;
	else if( target[0] || CheckKey( e, "mangle" ))
		return emit_spotlight;

	// otherwise it's pointlight
	return emit_point;
}

/*
=============
ParseLightIntensity

get light intensity
=============
*/
int ParseLightIntensity( const char *pLight, vec3_t intensity )
{
	double	r, g, b, scaler;
	int	argCnt;

	// scanf into doubles, then assign, so it is vec_t size independent
	r = g = b = scaler = 0;
	argCnt = sscanf( pLight, "%lf %lf %lf %lf", &r, &g, &b, &scaler );
	intensity[0] = (float)r;

	if( argCnt == 1 )
	{
		// the R, G, B values are all equal.
		intensity[1] = intensity[2] = (float)r;
	}
	else if( argCnt == 3 || argCnt == 4 )
	{
		// save the other two G,B values.
		intensity[1] = (float)g;
		intensity[2] = (float)b;

		intensity[0] = powf( intensity[0] / 255.0f, 2.2f );
		intensity[1] = powf( intensity[1] / 255.0f, 2.2f );
		intensity[2] = powf( intensity[2] / 255.0f, 2.2f );
		scaler /= 255.0;

		if( argCnt == 3 )
			scaler = 1.0;

		// Scale the normalized 0-255 R,G,B values by the intensity scaler
		VectorScale( intensity, scaler, intensity );
	}
	else
	{
		// quake default lightvalue
		VectorFill( intensity, 300.0f );
	}

	return argCnt;
}

/*
=============
ParseLightIntensity

get light intensity and color
=============
*/
void ParseLightIntensity( entity_t *e, directlight_t *dl )
{
	double	r, g, b, scaler;
	char	*pLight = NULL;
	char	*pColor = NULL;
	int	argCnt;

	if( CheckKey( e, "_sunlight" ))
		pLight = ValueForKey( e, "_sunlight" );
	else if( CheckKey( e, "_light" ))
		pLight = ValueForKey( e, "_light" );
	else pLight = ValueForKey( e, "light" );

	argCnt = ParseLightIntensity( pLight, dl->intensity );

	switch( argCnt )
	{
	case 3:
	case 4:
		dl->falloff = falloff_valve;
		break;
	default:
		dl->falloff = falloff_quake;
		break;
	}

	// quake light
	if( argCnt <= 1 )
	{
		scaler = dl->intensity[0];
		r = g = b = 0;

		if( CheckKey( e, "_sunlight_color" ))
		{
			pColor = ValueForKey( e, "_sunlight_color" );
			dl->radius = BOGUS_RANGE; // sunlight has infinite radius
		}
		else if( CheckKey( e, "_color" ))
		{
			pColor = ValueForKey( e, "_color" );
			dl->radius = scaler;
		}
		else
		{
			pColor = ValueForKey( e, "color" );
			dl->radius = scaler;
		}

		if( pColor[0] && sscanf( pColor, "%lf %lf %lf", &r, &g, &b ) == 3 )
		{
			dl->intensity[0] = r * (float)scaler;
			dl->intensity[1] = g * (float)scaler;
			dl->intensity[2] = b * (float)scaler;
		}
	}
}

/*
=============
ParseLightDirection

determine light direction
=============
*/
void ParseLightDirection( entity_t *e, directlight_t *dl )
{
	const char *target = ValueForKey( e, "target" );
	vec3_t	vAngles;

	if( target[0] )
	{	
		// point towards target
		entity_t	*e2 = FindTargetEntity( target );
		vec3_t	dest;

		if( e2 )
		{
			GetVectorForKey( e2, "origin", dest );
			VectorSubtract( dest, dl->origin, dl->normal );
			VectorNormalize( dl->normal );
		}
		else MsgDev( D_WARN, "%s[%i] has missing target %s\n", ValueForKey( e, "classname" ), e - g_entities, target );
	}

	// try angles
	if( VectorIsNull( dl->normal ))
	{
		if( CheckKey( e, "mangle" ))
		{
			// Quake spotlight angles
			GetVectorForKey( e, "mangle", vAngles );

			dl->normal[0] = cos( DEG2RAD( vAngles[0] )) * cos( DEG2RAD( vAngles[1] ));
			dl->normal[1] = sin( DEG2RAD( vAngles[0] )) * cos( DEG2RAD( vAngles[1] ));
			dl->normal[2] = sin( DEG2RAD( vAngles[1] ));
		}
		else if( CheckKey( e, "_sunlight_mangle" ))
		{
			// Quake sunlight angles
			GetVectorForKey( e, "_sunlight_mangle", vAngles );

			dl->normal[0] = cos( DEG2RAD( vAngles[0] )) * cos( DEG2RAD( vAngles[1] ));
			dl->normal[1] = sin( DEG2RAD( vAngles[0] )) * cos( DEG2RAD( vAngles[1] ));
			dl->normal[2] = sin( DEG2RAD( vAngles[1] ));
		}
		else if( CheckKey( e, "_sun_mangle" ))
		{
			// Quake sunlight angles
			GetVectorForKey( e, "_sun_mangle", vAngles );

			dl->normal[0] = cos( DEG2RAD( vAngles[0] )) * cos( DEG2RAD( vAngles[1] ));
			dl->normal[1] = sin( DEG2RAD( vAngles[0] )) * cos( DEG2RAD( vAngles[1] ));
			dl->normal[2] = sin( DEG2RAD( vAngles[1] ));
		}
		else
		{
			vec_t	angle = (float)FloatForKey( e, "angle" );

			// Half-Life spotlight or sunlight angles
			GetVectorForKey( e, "angles", vAngles );

			if( angle == ANGLE_UP )
			{
				VectorSet( dl->normal, 0.0, 0.0, 1.0 );
			}
			else if( angle == ANGLE_DOWN )
			{
				VectorSet( dl->normal, 0.0, 0.0, -1.0 );
			}
			else
			{
				// if we don't have a specific "angle" use the "angles" YAW
				if( !angle ) angle = vAngles[1];
				dl->normal[0] = (float)cos( DEG2RAD( angle ));
				dl->normal[1] = (float)sin( DEG2RAD( angle ));
				dl->normal[2] = 0;
			}

			angle = FloatForKey( e, "pitch" );
			// if we don't have a specific "pitch" use the "angles" PITCH
			if( !angle ) angle = vAngles[0];

			dl->normal[0] *= (float)cos( DEG2RAD( angle ));
			dl->normal[1] *= (float)cos( DEG2RAD( angle ));
			dl->normal[2] = (float)sin( DEG2RAD( angle ));
		}
	}
}

/*
=============
ParseLightAttenuation

get attenuation value
=============
*/
void ParseLightAttenuation( entity_t *e, directlight_t *dl )
{
	if( CheckKey( e, "_fade" ))
		dl->fade = FloatForKey( e, "_fade" );
	else if( CheckKey( e, "wait" ))
		dl->fade = FloatForKey( e, "wait" );

	// to prevent division by zero
	if( dl->fade <= 0.0 ) dl->fade = 1.0;
}

/*
=============
ParseLightFalloff

get falloff formula
=============
*/
void ParseLightFalloff( entity_t *e, directlight_t *dl )
{

	if( CheckKey( e, "_falloff" ))
	{
		// strange behavior...
		dl->falloff = IntForKey( e, "_falloff" );
		if( dl->falloff == 1 )
			dl->falloff = falloff_inverse;
		else dl->falloff = falloff_valve;
	}
	else if( CheckKey( e, "delay" ))
	{
		dl->falloff = IntForKey( e, "delay" );
		dl->falloff = bound( falloff_quake, dl->falloff, falloff_inverse2a );
		dl->lf_scale = LF_SCALE;
	}

	// oterwise it's already set in ParseLightIntensity
}

/*
=============
ParsePointLight
=============
*/
void ParsePointLight( entity_t *e, directlight_t *dl )
{
	ParseLightIntensity( e, dl );
	ParseLightAttenuation( e, dl );
	ParseLightFalloff( e, dl );
}

/*
=============
ParseSpotLight
=============
*/
void ParseSpotLight( entity_t *e, directlight_t *dl )
{
	ParseLightIntensity( e, dl );
	ParseLightDirection( e, dl );
	ParseLightAttenuation( e, dl );
	ParseLightFalloff( e, dl );

	if( CheckKey( e, "mangle" ))
	{
		// Quake spotlights
		dl->stopdot = FloatForKey( e, "_softangle" );
		dl->stopdot2 = FloatForKey( e, "angle" );
		if( !dl->stopdot ) dl->stopdot = 40;
		if( !dl->stopdot2 ) dl->stopdot2 = dl->stopdot;
		dl->stopdot2 = Q_max( dl->stopdot, dl->stopdot2 );
		dl->stopdot = (float)cos( DEG2RAD( dl->stopdot * 0.5 ));
		dl->stopdot2 = (float)cos( DEG2RAD( dl->stopdot2 * 0.5 ));
	}	
	else
	{
		// Half-Life spotlights
		dl->stopdot = FloatForKey( e, "_cone" );
		dl->stopdot2 = FloatForKey( e, "_cone2" );
		if( !dl->stopdot ) dl->stopdot = 10;
		if( !dl->stopdot2 ) dl->stopdot2 = dl->stopdot;
		dl->stopdot2 = Q_max( dl->stopdot, dl->stopdot2 );
		dl->stopdot = (float)cos( DEG2RAD( dl->stopdot ));
		dl->stopdot2 = (float)cos( DEG2RAD( dl->stopdot2 ));
	}
}

/*
=============
ParseSkyLight
=============
*/
void ParseSkyLight( entity_t *e, directlight_t *dl )
{
	char	*pLight = NULL;

	ParseLightIntensity( e, dl );
	ParseLightDirection( e, dl );

	// check for sky diffuse light
	if( CheckKey( e, "_diffuse_light" ))
	{
		ParseLightIntensity( ValueForKey( e, "_diffuse_light" ), dl->diffuse_intensity );
	}
	else if( CheckKey( e, "_sunlight2" ))
	{
		if( CheckKey( e, "_sunlight2_color" )) 
			pLight = va( "%s %s", ValueForKey( e, "_sunlight2_color" ), ValueForKey( e, "_sunlight2" ));
		else pLight = ValueForKey( e, "_sunlight2" );
		ParseLightIntensity( pLight, dl->diffuse_intensity );
	}

	// FIXME: get support for diffuse_intensity2 and _sunlight3?
	if( CheckKey( e, "_sunlight3" ))
	{
	}

	if( CheckKey( e, "_spread" ))
		dl->sunspreadangle = FloatForKey( e, "_spread" );
	else if( CheckKey( e, "_sunlight_penumbra" ))
		dl->sunspreadangle = FloatForKey( e, "_sunlight_penumbra" );

	if( dl->sunspreadangle < 0.0 || dl->sunspreadangle > 180 )
	{
		MsgDev( D_ERROR, "invalid spread angle '%s'. Defaulting to 0\n", ValueForKey( e, "_spread" ));
		dl->sunspreadangle = 0.0;
	}

	if( dl->sunspreadangle > 0.0 )
	{
		vec_t	testangle = dl->sunspreadangle;
		vec_t	totalw = 0, testdot;
		int	i, count;

		// we will later centralize all the normals we have collected.
		if( dl->sunspreadangle < SUNSPREAD_THRESHOLD )
			testangle = SUNSPREAD_THRESHOLD;

		testdot = cos( DEG2RAD( testangle ));

		for( count = 0, i = 0; i < g_numskynormals[SUNSPREAD_SKYLEVEL]; i++ )
		{
			vec_t	*testnormal = g_skynormals[SUNSPREAD_SKYLEVEL][i];
			vec_t	dot = -DotProduct( dl->normal, testnormal );

			if( dot > testdot )
			{
				// this is not the right formula when dl->sunspreadangle < SUNSPREAD_THRESHOLD,
				// but it gives almost the same result as the right one.
				totalw += Q_max( 0, dot - testdot );
				count++;
			}
		}

		Msg( "sun normals %d\n", count );

		if( count == 0 )
			COM_FatalError( "CreateDirectLights: internal error\n" );

		dl->sunnormals = (vec3_t *)Mem_Alloc( count * sizeof( vec3_t ));
		dl->sunnormalweights = (vec_t *)Mem_Alloc( count * sizeof( vec_t ));
		dl->numsunnormals = count;

		for( count = 0, i = 0; i < g_numskynormals[SUNSPREAD_SKYLEVEL]; i++ )
		{
			vec_t	*testnormal = g_skynormals[SUNSPREAD_SKYLEVEL][i];
			vec_t	dot = -DotProduct( dl->normal, testnormal );

			if( dot > testdot )
			{
				if( count >= dl->numsunnormals )
					COM_FatalError( "CreateDirectLights: internal error\n" );

				VectorCopy( testnormal, dl->sunnormals[count] );
				dl->sunnormalweights[count] = Q_max( 0, dot - testdot );
				dl->sunnormalweights[count] *= 1.0f / totalw;
				count++;
			}
		}

		if( count != dl->numsunnormals )
			COM_FatalError( "CreateDirectLights: internal error\n" );

		if( dl->sunspreadangle < SUNSPREAD_THRESHOLD )
		{
			for( i = 0; i < dl->numsunnormals; i++ )
			{
				vec_t	iscale = -1.0f / DotProduct( dl->sunnormals[i], dl->normal );
				vec_t	angle = dl->sunspreadangle / SUNSPREAD_THRESHOLD;
				vec3_t	tmp;

				VectorScale( dl->sunnormals[i], iscale, tmp );
				VectorAdd( tmp, dl->normal, tmp );
				VectorNegate( dl->normal, dl->sunnormals[i] );
				VectorMA( dl->sunnormals[i], angle, tmp, dl->sunnormals[i] );


				VectorNormalize( dl->sunnormals[i] );
			}
		}
	}
	else
	{
		dl->sunnormals = (vec3_t *)Mem_Alloc( sizeof( vec3_t ));
		dl->sunnormalweights = (vec_t *)Mem_Alloc( sizeof( vec_t ));
		VectorNegate( dl->normal, dl->sunnormals[0] );
		dl->sunnormalweights[0] = 1.0;
		dl->numsunnormals = 1;
	}
}

/*
=============
BuildVisForDLight

create visibility cache for dlight
=============
*/
int BuildVisForDLight( directlight_t *dl )
{
	int	leafnum;

	if( dl->type == emit_skylight )
	{
		// all leafs that contain skyface should be added to sun visibility
		for( leafnum = 0; leafnum < g_numvisleafs; leafnum++ )
		{
			uint	firstmarkface = g_dleafs[leafnum + 1].firstmarksurface;

			for( int markface = 0; markface < g_dleafs[leafnum + 1].nummarksurfaces; markface++ )
			{
				dface_t		*surf = &g_dfaces[g_dmarksurfaces[firstmarkface + markface]];
				dtexinfo_t	*tex = g_texinfo + surf->texinfo;

				if( FBitSet( tex->flags, TEX_SPECIAL ))
				{
					if( !Q_strnicmp( GetTextureByTexinfo( surf->texinfo ), "sky", 3 ))			
					{
						MergeDLightVis( dl, leafnum + 1 );
						break; // no reason to check all faces, go to next leaf
					}
				}
			}
		}

		// technically light_environment is outside of world
		return -1;
	}
	else
	{
		leafnum = PointInLeaf( dl->origin ) - g_dleafs;
		SetDLightVis( dl, leafnum );

		return leafnum;
	}
}

/*
=============
ParseLightEntity
=============
*/
bool ParseLightEntity( entity_t *e, directlight_t *dl )
{
	int	leafnum;
	vec_t	l1;

	// all the lights has the origin
	GetVectorForKey( e, "origin", dl->origin );

	switch( dl->type )
	{
	case emit_point:
		ParsePointLight( e, dl );
		break;
	case emit_spotlight:
		ParseSpotLight( e, dl );
		break;
	case emit_skylight:
		ParseSkyLight( e, dl );
		break;
	default:
		// unknown or unsupported light type
		Mem_Free( dl );
		return false;
	}

	if( VectorMax( dl->intensity ) <= 0.0f )
	{
		Mem_Free( dl );
		return false; // bad light value?
	}

	if( dl->type != emit_skylight && dl->falloff == falloff_valve )
	{
		dl->radius = BOGUS_RANGE;		
		VectorScale( dl->intensity, 255.0f * LF_SCALE / M_PI2 , dl->intensity );
	}

	dl->facenum = -1; // no texinfo for point and spotlights
	dl->modelnum = 0; // worldmodel for pointlights
#ifdef HLRAD_PARANOIA_BUMP
	dl->flags = IntForKey( e, "spawnflags" ); // buz
#else
	dl->style = abs( IntForKey( e, "style" ));
#endif
	leafnum = BuildVisForDLight( dl );
	dl->topatch = g_fastmode;
	dl->next = g_directlights;
	g_directlights = dl;
	g_numdlights++;

	if( dl->type == emit_skylight )
	{		
		g_skylights[g_numskylights] = dl;
		g_numskylights++;
	}

	// copy worldlight params and set unique number
	InitWorldLightFromDlight( dl, leafnum );

	return true;
}

/*
=============
CreateDirectLights
=============
*/
void CreateDirectLights( void )
{
	directlight_t	*dl;
	patch_t		*p;
	int		i;

	g_numdlights = 0;
	g_numskylights = 0;
	g_fakeskylight.type = emit_skylight;
	BuildVisForDLight( &g_fakeskylight );


	// surfaces
	for( i = 0, p = g_patches; i < g_num_patches; i++, p++ )
	{
		vec_t	value;

		if( !VectorIsNull( p->reflectivity )) 
			value = DotProduct( p->baselight, p->reflectivity ) / 3.0;
		else value = VectorAvg( p->baselight );

		if( value > 0.0 )
		{
			dl = (directlight_t *)Mem_Alloc( sizeof( directlight_t ));
			g_numdlights++;

			VectorCopy( p->origin, dl->origin );
			dl->type = emit_surface;
			dl->style = p->emitstyle;
			dl->lf_scale = 1.0f;

			VectorCopy( GetPlaneFromFace( p->faceNumber )->normal, dl->normal );
			VectorCopy( p->baselight, dl->intensity );
			VectorScale( dl->intensity, p->area, dl->intensity );
			VectorScale( dl->intensity, p->exposure, dl->intensity );
			dl->falloff = falloff_valve;
			dl->radius = p->area * 1.74;
			dl->facenum = p->faceNumber;
			dl->modelnum = p->modelnum;
			dl->lightnum = p->lightnum;	// already set
			dl->next = g_directlights;
			g_directlights = dl;

			if( !VectorIsNull( p->reflectivity ))
				VectorMultiply( dl->intensity, p->reflectivity, dl->intensity );

			if( !FBitSet( p->flags, PATCH_EMITLIGHT ) || g_fastmode )
				dl->topatch = true;
			dl->patch_emitter_range = p->emitter_range;
			dl->patch_area = p->area;
			BuildVisForDLight( dl );
			dl->fade = p->fade;
			dl->patch = p;
		}
	}

	// entities
	for( i = 0; i < g_numentities; i++ )
	{
		emittype_t	type;
		entity_t		*e;

		e = &g_entities[i];
		type = GetLightType( e );
		if( type == emit_ignored )
			continue;	// not a light entity

		dl = (directlight_t *)Mem_Alloc( sizeof( directlight_t ));
		dl->lf_scale = 1.0f;
		dl->type = type;

		if( !ParseLightEntity( e, dl ))
			continue;
	}

	if( g_numdlights <= 0 && !g_envsky )
		COM_FatalError( "map %s without any lights\n", source );
	MsgDev( D_INFO, "%i direct lights\n", g_numdlights );

	if( g_onlylights ) return;

#ifdef HLRAD_COMPUTE_VISLIGHTMATRIX
	// rows: facenum -> visible light bits like a normal vis info
	g_vislightdatasize = g_numfaces * ((g_numworldlights + 7) / 8);
	if( g_dvislightdata ) Mem_Free( g_dvislightdata );
	g_dvislightdata = (byte *)Mem_Alloc( g_vislightdatasize );
#endif
}

/*
=============
DeleteDirectLights
=============
*/
void DeleteDirectLights( void )
{
	directlight_t	*dl, *next;

	for( dl = g_directlights; dl; dl = next )
	{
		next = dl->next;
		Mem_Free( dl->pvs );
		Mem_Free( dl );
	}
}

void LoadEnvSkyTextures( void )
{
	const char*	skyBoxSuffix[6] = { "rt", "lf", "bk", "ft", "up", "dn" };
	char		*pszSkyName;
	char		pszSkySideFileName[64];
	rgbdata_t	*temp_image;

	pszSkyName = ValueForKey( g_entities, "skyname" );
	
	if( !*pszSkyName )
	{
		Msg( "^3Warning:^7 no skyname specified, -envsky parameter will be ignored\n" );
		g_envsky = false;
		return;		
	}

	Msg( "sky name %s\n", pszSkyName );

	for( int i = 0; i < 6; i++ )
	{
		Q_snprintf( pszSkySideFileName, 64, "gfx/env/%s%s", pszSkyName, skyBoxSuffix[i] );

		temp_image = COM_LoadImage( pszSkySideFileName, false, FS_LoadFile ); 
		if( temp_image )
		{
			while( (temp_image->width > 8)||(temp_image->height > 8) )
			{
				Image_BuildMipMap( temp_image->buffer, temp_image->width, temp_image->height, false, true );
				temp_image->width >>= 1;
				temp_image->height >>= 1;
			}
			temp_image->size = temp_image->width * temp_image->height * 4;
			g_skytextures[i] = Image_Copy( temp_image );
			Image_Free( temp_image );
		}
		else
		{
			Msg( "%s - failed to load\n", pszSkySideFileName);
			Msg( "^3Warning:^7 failed to load skybox textures, light_environment will be used to get the sky color (if present)\n" );
			FreeEnvSkyTextures();
			g_envsky = false;
			return;
		}
	}
}

void FreeEnvSkyTextures( void )
{
	for( int i = 0; i < 6; i++ )
	{
		Image_Free( g_skytextures[i] );
		g_skytextures[i] = NULL;
	}
}

void GetEnvSkyColor( const vec3_t dir, vec3_t color )
{	
	int sky_main_axis;	
	int face_index;
	vec_t u, v;
	int pix_pos;

	sky_main_axis = fabsf(dir[1]) > fabsf(dir[0]) ? 1 : 0;
	sky_main_axis = fabsf(dir[2]) > fabsf(dir[sky_main_axis]) ? 2 : sky_main_axis;
	face_index = sky_main_axis * 2 + ( dir[sky_main_axis] < 0.0f );
	
	u = sky_main_axis == 0 ? dir[1] : dir[0];
	v = sky_main_axis == 1 ? dir[2] : dir[1];

	switch( face_index )
	{
	case 0:
		u = -dir[1];
		v = -dir[2];
		break;
	case 1:
		u = -dir[1];
		v = dir[2];
		break;
	case 2:
		u = dir[0];
		v = -dir[2];
		break;
	case 3:
		u = dir[0];
		v = dir[2];
		break;		
	case 4:
		u = -dir[1];
		v = dir[0];
		break;
	case 5:
		u = dir[1];
		v = dir[0];
		break;
	}
	u = u / dir[sky_main_axis] * 0.5f + 0.5f;
	v = v / dir[sky_main_axis] * 0.5f + 0.5f;
	if( g_skytextures[face_index] )
	{
		u *= g_skytextures[face_index]->width;
		v *= g_skytextures[face_index]->height;
		pix_pos = (int)v * g_skytextures[face_index]->width + int(u);
		pix_pos *= 4;	//rgba
		color[0] = TextureToLinear( g_skytextures[face_index]->buffer[pix_pos] );
		color[1] = TextureToLinear( g_skytextures[face_index]->buffer[pix_pos+1] );
		color[2] = TextureToLinear( g_skytextures[face_index]->buffer[pix_pos+2] ); 
	}
}

void GetSkyColor( const vec3_t dir, vec3_t color )
{
	directlight_t *dl;

	if( g_envsky ) 
		GetEnvSkyColor( dir, color );
	else
	{
		VectorClear( color );
		for( int j = 0; j < g_numskylights; j++ )
		{	
			dl = g_skylights[j];

			if( g_solidsky )
				VectorAdd( color, dl->diffuse_intensity, color );
			else
			{
				// how far this piece of sky has deviated from the sun
				vec_t	factor = DotProduct( dl->normal, dir ) * 0.5f + 0.5f;
				vec3_t	temp_color;
				factor = bound( 0.0f, factor, 1.0f );
				VectorLerp( dl->intensity, factor, dl->diffuse_intensity, temp_color );
				VectorAdd( color, temp_color, color );
			}					
		}		
	}
}

/*
=============
GetLightDenominator

calc fallof denominator
=============
*/
vec_t GetLightDenominator( const directlight_t *dl, vec_t dist )
{
	vec_t	lf_scale = Q_max( 1.0, dl->lf_scale );
	vec_t	value = dist * dl->fade;

	switch( dl->falloff )
	{
	case falloff_quake:
		// Quake attenuation.
		if( dl->radius - value > 0.0f )
			return 1.0 / (1.0 - value * ( 1.0 / dl->radius ));
		return 0.0f;
	case falloff_inverse:
		return value / lf_scale;
		break;
	case falloff_inverse2a:
		value += lf_scale;
	case falloff_inverse2:
		return value * value / (lf_scale * lf_scale);
	case falloff_valve:
		// Valve attenuation
		return value * value;
	default:
		// No attenuation
		return 1.0;
	}
}

//this was a part of GatherSampleLight
_forceinline void AddSampleLight( int threadnum, bool topatch, vec3_t add, vec3_t add_direction, int lightstyle, vec3_t *s_light, vec3_t *s_dir, vec_t *s_occ, byte *styles )
{
	int		style_index = 0;
#ifdef HLRAD_PARANOIA_BUMP
	// hardcoded style representation
	if( !FBitSet( dl->flags, LIGHTFLAG_NOT_NORMAL ))
	{
		VectorAdd( s_light[STYLE_ORIGINAL_LIGHT], add, s_light[STYLE_ORIGINAL_LIGHT] );
		styles[0] = STYLE_ORIGINAL_LIGHT; // used
	}

	if( !FBitSet( dl->flags, LIGHTFLAG_NOT_RENDERER ))
	{
		VectorAdd( s_light[STYLE_BUMPED_LIGHT], add, s_light[STYLE_BUMPED_LIGHT] );
		styles[1] = STYLE_BUMPED_LIGHT; // used
	}
#else
	for( style_index = 0; style_index < MAXLIGHTMAPS; style_index++ )
	{
		if( styles[style_index] == lightstyle || styles[style_index] == 255 )
			break;
	}

	if( style_index == MAXLIGHTMAPS )
	{
		if( topatch ) g_overflowed_styles_onpatch[threadnum]++;
		else g_overflowed_styles_onface[threadnum]++;
		return;
	}

	// allocate a new one					
	if( styles[style_index] == 255 )
		styles[style_index] = lightstyle;

	VectorAdd( s_light[style_index], add, s_light[style_index] );
#endif
	if( topatch == false )
		g_lighted_luxels[threadnum]++;

	if( s_dir )
	{
#ifdef HLRAD_PARANOIA_BUMP
		// buz: add intensity to lightdir vector
		// delta must contain direction to light
		if( !FBitSet( dl->flags, LIGHTFLAG_NOT_RENDERER ))
		{
			if( dl->type != emit_skylight )
			{
				vec_t maxlight = VectorMaximum( add );
				VectorScale( add_direction, maxlight, add_direction );
			}
			VectorAdd( s_dir[STYLE_BUMPED_LIGHT], add_direction, s_dir[STYLE_BUMPED_LIGHT] );
		}
#else
		VectorAdd( s_dir[style_index], add_direction, s_dir[style_index] );
#endif
	}
#ifdef HLRAD_SHADOWMAPPING
	if( s_occ ) s_occ[style_index] = 1.0f;
#endif
}



/*
=============
GatherSampleLight
=============
*/
void GatherSampleLight( int threadnum, int fn, vec3_t *pos, int leafnum, const vec3_t n,
vec3_t *s_light, vec3_t *s_dir, vec_t *s_occ, byte *styles, byte *vislight, bool topatch, entity_t *ignoreent )
{
	vec3_t	add, delta, add_one;
	vec3_t	testline_origin;
	vec3_t	add_direction;
	float	dist, ratio;
	float	dot, dot2;
	vec3_t	direction;
	vec3_t	trace_pos;
	vec_t	avg;
	int		aa_samples = 0;
	vec_t	aa_divider;
	directlight_t *dl;
	
	bool	this_sample_uses_aa = g_aa && (fn >= 0) && (!topatch);
	bool	need_gi_bounce_from_studiomodels = topatch && (g_numbounce > 0);

	VectorCopy( *pos, trace_pos );	//trace_pos keeps constant for non two-sided surfaces

	if( this_sample_uses_aa )
	{
		for( int i = 0; i < 9; i++ )
			if( pos[i][0] != FLT_MAX )
				aa_samples++;
			else
				break;
		if( aa_samples == 0 )
			return;
		aa_divider = 1.0f / (float)aa_samples;
	}

	if( (topatch != g_perpixelsky ) && (g_indirect_sun > 0.0) && (g_numskylights > 0 || g_envsky) && (fn >= 0) )
	{	
		// check light visibility
		if( leafnum && g_fakeskylight.pvs && CHECKVISBIT( g_fakeskylight.pvs, leafnum - 1 ) )
		{
			vec3_t	*skynormals = g_skynormals_random;
			int		count = 0;
			float	mean = 0.0f;
			float	meanDistSquared = 0.0f;

			trace_t	trace;
			vec_t	scale = 4.0f / (float)SKYNORMALS_PER_PASS;
			
			VectorClear( add_direction );
			VectorClear( add );		
		
			// loop over the normals		
			for( int k = 0; k < SKYNORMALS_PASSES; k++ )
			{		
				for( int i = 0; i < SKYNORMALS_PER_PASS; i++, skynormals++ )
				{				
					// make sure the angle is okay				
					dot = DotProduct( n, *skynormals );	
					if( dot <= NORMAL_EPSILON )
						continue;

					// search back to see if we can hit a sky brush
					VectorScale( *skynormals, BOGUS_RANGE, delta );
					VectorAdd( *pos, delta, delta );
					
					if( topatch )
						VectorCopy( pos[i % PATCH_MAX_TRACE_ORIGINS ], trace_pos );

					TestLine( threadnum, trace_pos, delta, &trace, false, ignoreent );

					if( trace.contents == CONTENTS_SKY )
					{
						GetSkyColor( *skynormals, add_one );

						VectorScale( add_one, dot, add_one );
						VectorAdd( add, add_one, add );
						avg = VectorAvg( add_one );
						VectorMA( add_direction, avg, *skynormals, add_direction );
					}
					else
						avg = 0.0f;

					count++;

					if( g_fastsky )
					{				
						vec_t delta2 = avg - mean;
						mean += delta2 / (float)count;
						meanDistSquared += delta2 * (avg - mean);
					}
				}

				if( g_fastsky )
				{	
					vec_t variance = meanDistSquared / (float)(count - 1);

					// Accept 5% error using 95% confidence interval
					vec_t threshold = 0.05f / 1.96f;
					vec_t standardError = sqrtf( variance / (float)count );
					if( standardError < (mean * threshold) )	//<= causes artifacts in the dark but why?
						break;
				}
			}	

			VectorScale( add, 2.0f * g_indirect_sun / (float)count, add );
			VectorScale( add_direction, 2.0f * g_indirect_sun / (float)count, add_direction );

			AddSampleLight( threadnum, topatch, add, add_direction, g_skystyle, s_light, s_dir, s_occ, styles );		
		}
	}

	//single gi bounce from studiomodels on patches
	if( need_gi_bounce_from_studiomodels )
	{
		vec3_t	*skynormals = g_skynormals_random;
		trace_t	trace;
		vec_t	scale = 4.0f / 1024.0f;

		for( int i = 0; i < 1024; i++, skynormals++ )
		{
			dot = DotProduct( n, *skynormals );	
			if( dot <= NORMAL_EPSILON ) continue;

			VectorScale( *skynormals, BOGUS_RANGE, delta );
			VectorAdd( *pos, delta, delta );

			VectorCopy( pos[i % PATCH_MAX_TRACE_ORIGINS ], trace_pos );

			TestLine( threadnum, trace_pos, delta, &trace, false );

			if( trace.surface == STUDIO_SURFACE_HIT )
			{
				for (int j = 0; j < MAXLIGHTMAPS; j++ )
				{	
					if( trace.styles[j] == 255 )
						break;	

					VectorScale( trace.light[j], dot * scale, add );
					VectorScale( *skynormals, VectorAvg(add), add_direction );

					AddSampleLight( threadnum, topatch, add, add_direction, trace.styles[j], s_light, s_dir, s_occ, styles );	
				}
			}
		}
	}


	VectorCopy( *pos, trace_pos );
	

	for( dl = g_directlights; dl != NULL; dl = dl->next )
	{
		// check light visibility
		if( !leafnum || !dl->pvs || !CHECKVISBIT( dl->pvs, leafnum - 1 ))
			continue;

		// skylights work fundamentally differently than normal lights
		if( dl->type == emit_skylight )
		{
			VectorClear( add_direction );
			VectorClear( add );

			// add sun light
			if( topatch == dl->topatch )
			{
				// loop over the normals
				for( int i = 0; i < dl->numsunnormals; i++ )
				{
					// make sure the angle is okay
					dot = DotProduct( n, dl->sunnormals[i] );

					if( fn == -2 )	//two-sided meshes
					{
						VectorMA( *pos, dot > 0.0 ? DEFAULT_HUNT_OFFSET : -DEFAULT_HUNT_OFFSET, n, trace_pos );
						dot = fabsf(dot) * 0.5f;
						dot = 1.0f;
					}						

					if( dot <= NORMAL_EPSILON )
						continue;

					// search back to see if we can hit a sky brush
					VectorScale( dl->sunnormals[i], BOGUS_RANGE, delta );
					VectorAdd( *pos, delta, delta );


					if( this_sample_uses_aa )
					{
						int lit = 0;
						for( int k = 0; k < aa_samples; k++ )
							lit += ( TestLine( threadnum, pos[k], delta, dl->topatch, ignoreent ) == CONTENTS_SKY );
						
						if( lit == 0 )
							continue;

						dot *= (float)lit * aa_divider;
					}
					else
						if( TestLine( threadnum, trace_pos, delta, dl->topatch, ignoreent ) != CONTENTS_SKY )
							continue; // occluded

					VectorCopy( dl->sunnormals[i], direction );
					VectorScale( dl->intensity, dot * dl->sunnormalweights[i], add_one );
					// add to the contribution of this light
					VectorAdd( add, add_one, add );
					avg = VectorAvg( add_one );
					VectorMA( add_direction, avg, direction, add_direction );
				}
			}
		}
		else
		{
			bool	light_behind_surface = false;
			vec_t	range;

			if( topatch != dl->topatch )
				continue;

			VectorCopy( dl->origin, testline_origin );
			VectorSubtract( dl->origin, *pos, delta );

			if( dl->type == emit_surface )	// move emitter back to its plane
				VectorMA( delta, -DEFAULT_HUNT_OFFSET * 0.5, dl->normal, delta );

			dist = VectorNormalize( delta );
			dot = DotProduct( delta, n );
			dist = Q_max( dist, 1.0f );

			// save some compile time
			if( dl->type == emit_point && dl->falloff == falloff_quake && dist > dl->radius && topatch == false )
				continue;	// don't bother with light too far away

			// variable power falloff (1 = inverse linear, 2 = inverse square)
			vec_t denominator = GetLightDenominator( dl, dist );
			if( denominator <= 0.0 ) continue;

			VectorCopy( delta, direction );

			if( fn == -2 )	//two-sided meshes
			{
				VectorMA( *pos, dot > 0.0 ? DEFAULT_HUNT_OFFSET : -DEFAULT_HUNT_OFFSET, n, trace_pos );
				dot = fabsf(dot) * 0.5f;
				dot = 1.0f;
			}			

			if(( -dot ) > 0 )
			{
				// reflect the direction back (this is not ideal!)
				VectorMA( direction, -(-dot) * 2.0f, n, direction );
			}

			switch( dl->type )
			{
			case emit_point:
				if( dot <= NORMAL_EPSILON )
					continue;
				ratio = dot / denominator;
				break;
			case emit_surface:
				if( dot <= NORMAL_EPSILON )
					light_behind_surface = true;
				dot2 = -DotProduct( delta, dl->normal );
				if( dot2 * dist <= MINIMUM_PATCH_DISTANCE )
					continue;
				range = dl->patch_emitter_range;
				ratio = dot * dot2 / denominator;

				// analogous to the one in MakeScales
				// 0.4f is tested to be able to fully eliminate bright spots
				if( ratio * dl->patch_area > 0.4f )
					ratio = 0.4f / dl->patch_area;

				if( dist < range - ON_EPSILON )
				{
					vec_t	sightarea;
					vec_t	ratio2, frac;

					// do things slow
					if( light_behind_surface )
					{
						ratio = 0.0;
						dot = 0.0;
					}

					GetAlternateOrigin( *pos, n, dl->patch, testline_origin );
					sightarea = CalcSightArea( *pos, n, dl->patch->winding, dl->patch->emitter_skylevel );

					frac = dist / range;
					frac = ( frac - 0.5 ) * 2.0; // make a smooth transition between the two methods
					frac = bound( 0.0, frac, 1.0 );
					// because dl->patch_area has been multiplied into dl->intensity
					ratio2 = (sightarea / dl->patch_area);
					ratio = frac * ratio + (1.0 - frac) * ratio2;
				}
				else if( light_behind_surface )
				{
					continue;
				}
				break;
			case emit_spotlight:
				if( dot <= NORMAL_EPSILON )
					continue;
				dot2 = -DotProduct( delta, dl->normal );
				if( dot2 <= dl->stopdot2 )
					continue; // outside light cone
				ratio = dot * dot2 / denominator;
				if( dot2 <= dl->stopdot )
					ratio *= (dot2 - dl->stopdot2) / (dl->stopdot - dl->stopdot2);
				break;
			default:
				COM_FatalError( "bad dl->type\n" );
				break;
			}


			if( this_sample_uses_aa )
			{
				int lit = 0;
				for( int k = 0; k < aa_samples; k++ )
					lit += ( TestLine( threadnum, pos[k], testline_origin, dl->topatch, ignoreent ) == CONTENTS_EMPTY );
				
				if( lit == 0 )
					continue;

				ratio *= (float)lit * aa_divider;
			}
			else
				if( TestLine( threadnum, trace_pos, testline_origin, dl->topatch, ignoreent ) != CONTENTS_EMPTY )
					continue;	// occluded

			VectorScale( dl->intensity, ratio, add );
			avg = VectorAvg( add );
			VectorScale( direction, avg, add_direction );
		}

		AddSampleLight( threadnum, topatch, add, add_direction, dl->style, s_light, s_dir, s_occ, styles );

#ifdef HLRAD_COMPUTE_VISLIGHTMATRIX
		// no reason to set it again
		if( vislight != NULL && !CHECKVISBIT( vislight, dl->lightnum ))
		{
			ThreadLock();
			SETVISBIT( vislight, dl->lightnum );
			ThreadUnlock();
		}
#endif

	}
}

// =====================================================================================
//  AddSampleToPatch
//      Take the sample's collected light and add it back into the apropriate patch for the radiosity pass.
// =====================================================================================
static void AddSamplesToPatches( int threadnum, const sample_t *samples, const byte *styles, int facenum, const lightinfo_t *l )
{
	int	numtexwindings = 0;
	winding_t	**texwindings;
	int	texture_step;
	int	i, j, m, k;
	patch_t	*p;

	for( p = g_face_patches[facenum]; p != NULL; p = p->next )
		numtexwindings++;

	// create array of pointers
	texwindings = (winding_t **)Mem_Alloc( numtexwindings * sizeof( winding_t* ));
	texture_step = GetTextureStep( &g_dfaces[facenum] );

	// translate world winding into winding in s,t plane
	for( j = 0, p = g_face_patches[facenum]; j < numtexwindings && p != NULL; j++, p = p->next )
	{
		winding_t	*w = AllocWinding( p->winding->numpoints );

		w->numpoints = p->winding->numpoints;

		for( int x = 0; x < w->numpoints; x++ )
		{
			vec_t s, t;
			WorldToTexelSpace( l, p->winding->p[x], s, t );
			VectorSet( w->p[x], s, t, 0.0 );
		}

		RemoveColinearPointsEpsilon( w, ON_EPSILON );
		texwindings[j] = w;
	}

	for( i = 0; i < l->numsurfpt; i++ )
	{
		dplane_t	clipplanes[4];
		vec_t	s_vec, t_vec;

		s_vec = l->texmins[0] * texture_step + ( i % (l->texsize[0] + 1 )) * texture_step;
		t_vec = l->texmins[1] * texture_step + ( i / (l->texsize[0] + 1 )) * texture_step;

		// prepare clip planes (in 2D)
		VectorSet( clipplanes[0].normal, 1.0, 0.0, 0.0 );
		clipplanes[0].dist =  (s_vec - 0.5 * texture_step);
		VectorSet( clipplanes[1].normal,-1.0, 0.0, 0.0 );
		clipplanes[1].dist = -(s_vec + 0.5 * texture_step);
		VectorSet( clipplanes[2].normal, 0.0, 1.0, 0.0 );
		clipplanes[2].dist =  (t_vec - 0.5 * texture_step);
		VectorSet( clipplanes[3].normal, 0.0,-1.0, 0.0 );
		clipplanes[3].dist = -(t_vec + 0.5 * texture_step);

		// clip each patch
		for( j = 0, p = g_face_patches[facenum]; j < numtexwindings && p != NULL; j++, p = p->next )
		{
			winding_t	*w = CopyWinding( texwindings[j] );

			for( k = 0; k < 4 && w != NULL; k++ )
			{
				ChopWindingInPlace( &w, clipplanes[k].normal, clipplanes[k].dist, ON_EPSILON, false );
			}

			if( w != NULL )
			{
				// add sample to patch
				vec_t		area = WindingArea( w ) / (texture_step * texture_step);
				const sample_t	*s = &samples[i];

				for( m = 0; m < MAXLIGHTMAPS && ( styles[m] != 255 ); m++ )
				{
					for( k = 0; k < MAXLIGHTMAPS && p->totalstyle[k] != 255; k++ )
					{
						if( p->totalstyle[k] == styles[m] )
							break;
					}

					if( k < MAXLIGHTMAPS )
					{
						if( p->totalstyle[k] == 255 )
							p->totalstyle[k] = styles[m];

						// add the sample to the patch
						VectorMA( p->samplelight[k], area, s->light[m], p->samplelight[k] );
#ifdef HLRAD_DELUXEMAPPING
						VectorMA( p->samplelight_dir[k], area, s->deluxe[m], p->samplelight_dir[k] );
#endif
						p->samples[k] += area;
					}
					else
					{
						g_overflowed_styles_onpatch[threadnum]++;
					}
				}

				FreeWinding( w );
			}
		}
	}

	for( j = 0; j < numtexwindings; j++ )
		FreeWinding( texwindings[j] );
	Mem_Free( texwindings );
}

static void CalcLightmap( int thread, lightinfo_t *l, facelight_t *fl )
{
	vec_t	texture_step = GetTextureStep( &g_dfaces[l->surfnum] );
	vec_t	density = (vec_t)l->lmcache_density;
	int	w = l->texsize[0] + 1;
	int	h = l->texsize[1] + 1;
	byte	*vislight = NULL;
	dface_t	*f = l->face;
	vec_t	square[2][2];
	int	i, j;

	// allocate light samples
	fl->samples = (sample_t *)Mem_Alloc( l->numsurfpt * sizeof( sample_t ));
	fl->numsamples = l->numsurfpt;
	fl->width = w;

	// stats
	g_direct_luxels[thread] += fl->numsamples;

#ifdef HLRAD_COMPUTE_VISLIGHTMATRIX
	vislight = g_dvislightdata + l->surfnum * ((g_numworldlights + 7) / 8);
#endif
	// copy surf points from lightinfo with offset 0,0
	for( i = 0; i < fl->numsamples; i++ )
	{
		VectorCopy( l->surfpt[i].point, fl->samples[i].pos );
		fl->samples[i].occluded = l->surfpt[i].occluded; 
		fl->samples[i].surface = l->surfpt[i].surface;
	}

	// for each sample whose light we need to calculate
	for( i = 0; i < l->lmcachewidth * l->lmcacheheight; i++ )
	{
		vec_t	s, t, s_vec, t_vec;
		int	nearest_s, nearest_t;
		vec3_t	spot, surfpt;
		vec3_t	pointnormal;
		int	surface;
		bool	blocked;

		// prepare input parameter and output parameter
		s = ((i % l->lmcachewidth) - l->lmcache_offset) / (vec_t)l->lmcache_density;
		t = ((i / l->lmcachewidth) - l->lmcache_offset) / (vec_t)l->lmcache_density;
		s_vec = l->texmins[0] * texture_step + s * texture_step;
		t_vec = l->texmins[1] * texture_step + t * texture_step;
		nearest_s = Q_max( 0, Q_min((int)floor( s + 0.5 ), l->texsize[0] ));
		nearest_t = Q_max( 0, Q_min((int)floor( t + 0.5 ), l->texsize[1] ));
		square[0][0] = l->texmins[0] * texture_step + ceil( s - (l->lmcache_side + 0.5) / density) * texture_step - texture_step;
		square[0][1] = l->texmins[1] * texture_step + ceil( t - (l->lmcache_side + 0.5) / density) * texture_step - texture_step;
		square[1][0] = l->texmins[0] * texture_step + floor( s + (l->lmcache_side + 0.5) / density) * texture_step + texture_step;
		square[1][1] = l->texmins[1] * texture_step + floor( t + (l->lmcache_side + 0.5) / density) * texture_step + texture_step;

		// find world's position for the sample
		blocked = false;

		if( CalcSamplePoint( surfpt, spot, &surface, l, s_vec, t_vec, square ))
		{
			j = nearest_s + w * nearest_t;

			if( l->surfpt[j].occluded )
			{
				blocked = true;
			}
			else
			{
				// the area this light sample has effect on is completely covered by solid, so take whatever valid position.
				VectorCopy( l->surfpt[j].point, surfpt );
				VectorCopy( l->surfpt[j].position, spot );
				surface = l->surfpt[j].surface;
			}
		}

		// calculate normal for the sample
		GetPhongNormal( surface, surfpt, pointnormal );
#ifdef HLRAD_DELUXEMAPPING
		VectorCopy( pointnormal, l->normals[i] );
#endif
		if( blocked ) continue;

		// calculate visibility for the sample
		int leaf = PointInLeaf( spot ) - g_dleafs;

		// gather light
#if defined( HLRAD_DELUXEMAPPING ) && defined( HLRAD_SHADOWMAPPING )
		GatherSampleLight( thread, l->surfnum, &spot, leaf, pointnormal, l->light[i], l->deluxe[i], l->shadow[i], f->styles, vislight, 0 );
#elif defined( HLRAD_DELUXEMAPPING )
		GatherSampleLight( thread, l->surfnum, &spot, leaf, pointnormal, l->light[i], l->deluxe[i], NULL, f->styles, vislight, 0 );
#else
		GatherSampleLight( thread, l->surfnum, &spot, leaf, pointnormal, l->light[i], NULL, NULL, f->styles, vislight, 0 );
#endif
	}

	for( i = 0; i < fl->numsamples; i++ )
	{
#ifdef HLRAD_DELUXEMAPPING
		vec_t	weighting_correction;
		vec3_t	centernormal;
#endif
		int	s_center, t_center;
		vec_t	weighting, subsamples;
		int	s, t, pos;
		vec_t	sizehalf;

		s_center = (i % w) * l->lmcache_density + l->lmcache_offset;
		t_center = (i / w) * l->lmcache_density + l->lmcache_offset;
		sizehalf = 0.5 * g_blur * l->lmcache_density;
		subsamples = 0.0;
#ifdef HLRAD_DELUXEMAPPING
		VectorCopy( l->normals[s_center + l->lmcachewidth * t_center], centernormal );
#endif
		for( s = s_center - l->lmcache_side; s <= s_center + l->lmcache_side; s++ )
		{
			for( t = t_center - l->lmcache_side; t <= t_center + l->lmcache_side; t++ )
			{
				weighting = (Q_min( 0.5, sizehalf - ( s - s_center )) - Q_max( -0.5, -sizehalf - ( s - s_center )));
				weighting	*=(Q_min( 0.5, sizehalf - ( t - t_center )) - Q_max( -0.5, -sizehalf - ( t - t_center )));
				pos = s + l->lmcachewidth * t;
#ifdef HLRAD_DELUXEMAPPING
				// when blur distance (g_blur) is large, the subsample can be very far from the original lightmap sample
				// in some cases such as a thin cylinder, the subsample can even grow into the opposite side
				// as a result, when exposed to a directional light, the light on the cylinder may "leak" into
				// the opposite dark side this correction limits the effect of blur distance when the normal changes very fast
				// this correction will not break the smoothness that HLRAD_GROWSAMPLE ensures
				weighting_correction = DotProduct( l->normals[pos], centernormal );
				weighting_correction = (weighting_correction > 0) ? weighting_correction * weighting_correction : 0;
				weighting = weighting * weighting_correction;
#endif
				for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
				{
					VectorMA( fl->samples[i].light[j], weighting, l->light[pos][j], fl->samples[i].light[j] );
#ifdef HLRAD_DELUXEMAPPING
					VectorMA( fl->samples[i].deluxe[j], weighting, l->deluxe[pos][j], fl->samples[i].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
					fl->samples[i].shadow[j] += l->shadow[pos][j] * weighting;
#endif
#endif
				}
				subsamples += weighting;
			}
		}

		if( subsamples > NORMAL_EPSILON )
		{
#ifdef HLRAD_DELUXEMAPPING
			VectorCopy( centernormal, fl->samples[i].normal );
#endif
			for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
			{
				VectorScale( fl->samples[i].light[j], (1.0 / subsamples), fl->samples[i].light[j] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( fl->samples[i].deluxe[j], (1.0 / subsamples), fl->samples[i].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
				fl->samples[i].shadow[j] *= (1.0 / subsamples);
#endif
#endif
			}
		}
	}

#ifdef HLRAD_DELUXEMAPPING
#ifdef HLRAD_SHADOWMAPPING
	// multiply light by shadow to prevent blur artifacts
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
//			VectorScale( fl->samples[i].light[j], fl->samples[i].shadow[j], fl->samples[i].light[j] );
//			VectorScale( fl->samples[i].deluxe[j], fl->samples[i].shadow[j], fl->samples[i].deluxe[j] );
		}
	}

	// output occlusion shouldn't be blured
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			int	s = (i % w) + l->lmcache_side;
			int	t = (i / w) + l->lmcache_side;
			int	pos = s + l->lmcachewidth * t;
			fl->samples[i].shadow[j] = l->shadow[pos][j];
		}
	}
#endif
#endif

	Mem_Free( l->light );
#ifdef HLRAD_DELUXEMAPPING
	Mem_Free( l->normals );
	Mem_Free( l->deluxe );
#ifdef HLRAD_SHADOWMAPPING
	Mem_Free( l->shadow );
#endif
#endif
}

static void CalcLightmapAA( int thread, lightinfo_t *l, facelight_t *fl )
{
	vec_t	texture_step = GetTextureStep( &g_dfaces[l->surfnum] );
	vec_t	density = (vec_t)l->lmcache_density;
	int	w = l->texsize[0] + 1;
	int	h = l->texsize[1] + 1;
	byte	*vislight = NULL;
	dface_t	*f = l->face;
	vec_t	square[2][2];
	int	i, j;
	const dplane_t	*plane;

	// allocate light samples
	fl->samples = (sample_t *)Mem_Alloc( l->numsurfpt * sizeof( sample_t ));
	fl->numsamples = l->numsurfpt;
	fl->width = w;

	plane = GetPlaneFromFace( f );	

	// stats
	g_direct_luxels[thread] += fl->numsamples;

#ifdef HLRAD_COMPUTE_VISLIGHTMATRIX
	vislight = g_dvislightdata + l->surfnum * ((g_numworldlights + 7) / 8);
#endif
	// copy surf points from lightinfo with offset 0,0
	for( i = 0; i < fl->numsamples; i++ )
	{
		VectorCopy( l->surfpt[i].point, fl->samples[i].pos );
		fl->samples[i].occluded = l->surfpt[i].occluded; 
		fl->samples[i].surface = l->surfpt[i].surface;
	}
	
	

	// for each sample whose light we need to calculate	
	for( i = 0; i < l->lmcachewidth * l->lmcacheheight; i++ )
	{
		vec_t	s, t, s_vec, t_vec;
		int		nearest_s, nearest_t;
		vec3_t	spot, surfpt;
		vec3_t	pointnormal;
		int		surface;
		bool	blocked = false;
		vec_t	off_x, off_y;
		vec3_t	orig_spot, orig_surfpt;
		int		orig_surface;
		bool	point_outside = false;
		int		spot_count = 0;
		vec3_t	spots_aa[9];
		int		leaf;


		// prepare input parameter and output parameter
		s = ((i % l->lmcachewidth) - l->lmcache_offset) / (vec_t)l->lmcache_density;
		t = ((i / l->lmcachewidth) - l->lmcache_offset) / (vec_t)l->lmcache_density;
		nearest_s = Q_max( 0, Q_min((int)floor( s + 0.5 ), l->texsize[0] ));
		nearest_t = Q_max( 0, Q_min((int)floor( t + 0.5 ), l->texsize[1] ));
		square[0][0] = l->texmins[0] * texture_step + ceil( s - (l->lmcache_side + 0.5) / density) * texture_step - texture_step;
		square[0][1] = l->texmins[1] * texture_step + ceil( t - (l->lmcache_side + 0.5) / density) * texture_step - texture_step;
		square[1][0] = l->texmins[0] * texture_step + floor( s + (l->lmcache_side + 0.5) / density) * texture_step + texture_step;
		square[1][1] = l->texmins[1] * texture_step + floor( t + (l->lmcache_side + 0.5) / density) * texture_step + texture_step;

		s_vec = ((float)l->texmins[0] + s ) * texture_step; 
		t_vec = ((float)l->texmins[1] + t ) * texture_step;	

		if( CalcSamplePoint( orig_surfpt, orig_spot, &orig_surface, l, s_vec, t_vec, square ))
		{
			j = nearest_s + w * nearest_t;

			if( l->surfpt[j].occluded )
			{
				blocked = true;
			}
			else
			{
				// the area this light sample has effect on is completely covered by solid, so take whatever valid position.
				VectorCopy( l->surfpt[j].point, orig_surfpt );
				VectorCopy( l->surfpt[j].position, orig_spot );
				orig_surface = l->surfpt[j].surface;
			}				
		}

		// calculate normal for the sample
		GetPhongNormal( orig_surface, orig_surfpt, pointnormal );


		matrix3x4	worldtotex, textoworld;
		vec3_t	tex;
		TranslateWorldToTex( l->surfnum, worldtotex );
		if( !Matrix3x4_Invert_Full( textoworld, worldtotex ))
			MsgDev( D_WARN, "Texture axis perpendicular to face\n" );


		//do not apply aa to points outside the visible part
		point_outside = ((i % l->lmcachewidth) < (l->lmcache_offset - 1));
		point_outside = point_outside || ((i / l->lmcachewidth) < (l->lmcache_offset - 1));		
		point_outside = point_outside || ((l->lmcachewidth - (i % l->lmcachewidth)) < l->lmcache_offset);
		point_outside = point_outside || ((l->lmcacheheight - (i / l->lmcachewidth)) < l->lmcache_offset);


		for( int k = 0; k < 9; k++ )
		{			
			if( point_outside && k != 4 )
				continue;

			off_x = (float)(k % 3 - 1) * 0.333333f;
			off_y = (float)(k / 3 - 1) * 0.333333f;

			s_vec = s + off_x;
			t_vec = t + off_y;

			s_vec = ((float)l->texmins[0] + s_vec ) * texture_step; 
			t_vec = ((float)l->texmins[1] + t_vec ) * texture_step;	
			
			if( k != 4 )
			{
				TexelSpaceToWorld( l, surfpt, s_vec, t_vec );
				VectorMA( surfpt, DEFAULT_HUNT_OFFSET, plane->normal, spot );
				surface = l->surfpt[i].surface;
				
				if( !blocked )
				{
					if( TestLine(thread, spot, orig_spot, true, 0) != CONTENTS_EMPTY )
						continue;
				}
			}
			else
			{
				if( blocked ) continue;

				VectorCopy( orig_surfpt, surfpt );
				VectorCopy( orig_spot, spot );
				surface = orig_surface;
			}	

			// calculate visibility for the sample			
			leaf = PointInLeaf( spot ) - g_dleafs;

			if(( g_dleafs[leaf].contents == CONTENTS_SKY ) || ( g_dleafs[leaf].contents == CONTENTS_SOLID ))
				continue;

			
			if( k == 4 )	//center sample first
			{
				VectorCopy( spots_aa[0], spots_aa[spot_count] );
				VectorCopy( spot, spots_aa[0] );
			}
			else
				VectorCopy( spot, spots_aa[spot_count] );
			spot_count++;
		}


	#ifdef HLRAD_DELUXEMAPPING
		VectorAdd( pointnormal, l->normals[i], l->normals[i] );
	#endif		

		if( spot_count == 0 )
			continue;

		//fill invalid spots with flt_max
		for( int k = spot_count; k < 9; k++ )
			spots_aa[k][0] = FLT_MAX;

		leaf = PointInLeaf( spots_aa[0] ) - g_dleafs;

			// gather light
	#if defined( HLRAD_DELUXEMAPPING ) && defined( HLRAD_SHADOWMAPPING )
		GatherSampleLight( thread, l->surfnum, spots_aa, leaf, pointnormal, l->light[i], l->deluxe[i], l->shadow[i], f->styles, vislight, 0 );
	#elif defined( HLRAD_DELUXEMAPPING )
		GatherSampleLight( thread, l->surfnum, spots_aa, leaf, pointnormal, l->light[i], l->deluxe[i], NULL, f->styles, vislight, 0 );
	#else
		GatherSampleLight( thread, l->surfnum, spots_aa, leaf, pointnormal, l->light[i], NULL, NULL, f->styles, vislight, 0 );
	#endif
	}

	for( i = 0; i < fl->numsamples; i++ )
	{
#ifdef HLRAD_DELUXEMAPPING
		vec_t	weighting_correction;
		vec3_t	centernormal;
#endif
		int	s_center, t_center;
		vec_t	weighting, subsamples;
		int	s, t, pos;
		vec_t	sizehalf;

		s_center = (i % w) * l->lmcache_density + l->lmcache_offset;
		t_center = (i / w) * l->lmcache_density + l->lmcache_offset;
		sizehalf = 0.5 * g_blur * l->lmcache_density;
		subsamples = 0.0;
#ifdef HLRAD_DELUXEMAPPING
		VectorCopy( l->normals[s_center + l->lmcachewidth * t_center], centernormal );
#endif
		for( s = s_center - l->lmcache_side; s <= s_center + l->lmcache_side; s++ )
		{
			for( t = t_center - l->lmcache_side; t <= t_center + l->lmcache_side; t++ )
			{
				weighting = (Q_min( 0.5, sizehalf - ( s - s_center )) - Q_max( -0.5, -sizehalf - ( s - s_center )));
				weighting	*=(Q_min( 0.5, sizehalf - ( t - t_center )) - Q_max( -0.5, -sizehalf - ( t - t_center )));
				pos = s + l->lmcachewidth * t;
#ifdef HLRAD_DELUXEMAPPING
				// when blur distance (g_blur) is large, the subsample can be very far from the original lightmap sample
				// in some cases such as a thin cylinder, the subsample can even grow into the opposite side
				// as a result, when exposed to a directional light, the light on the cylinder may "leak" into
				// the opposite dark side this correction limits the effect of blur distance when the normal changes very fast
				// this correction will not break the smoothness that HLRAD_GROWSAMPLE ensures
				weighting_correction = DotProduct( l->normals[pos], centernormal );
				weighting_correction = (weighting_correction > 0) ? weighting_correction * weighting_correction : 0;
				weighting = weighting * weighting_correction;
#endif
				for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
				{
					VectorMA( fl->samples[i].light[j], weighting, l->light[pos][j], fl->samples[i].light[j] );
#ifdef HLRAD_DELUXEMAPPING
					VectorMA( fl->samples[i].deluxe[j], weighting, l->deluxe[pos][j], fl->samples[i].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
					fl->samples[i].shadow[j] += l->shadow[pos][j] * weighting;
#endif
#endif
				}
				subsamples += weighting;
			}
		}

		if( subsamples > NORMAL_EPSILON )
		{
#ifdef HLRAD_DELUXEMAPPING
			VectorCopy( centernormal, fl->samples[i].normal );
#endif
			for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
			{
				VectorScale( fl->samples[i].light[j], (1.0 / subsamples), fl->samples[i].light[j] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( fl->samples[i].deluxe[j], (1.0 / subsamples), fl->samples[i].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
				fl->samples[i].shadow[j] *= (1.0 / subsamples);
#endif
#endif
			}
		}
	}

#ifdef HLRAD_DELUXEMAPPING
#ifdef HLRAD_SHADOWMAPPING
	// multiply light by shadow to prevent blur artifacts
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
//			VectorScale( fl->samples[i].light[j], fl->samples[i].shadow[j], fl->samples[i].light[j] );
//			VectorScale( fl->samples[i].deluxe[j], fl->samples[i].shadow[j], fl->samples[i].deluxe[j] );
		}
	}

	// output occlusion shouldn't be blured
	for( i = 0; i < fl->numsamples; i++ )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			int	s = (i % w) + l->lmcache_side;
			int	t = (i / w) + l->lmcache_side;
			int	pos = s + l->lmcachewidth * t;
			fl->samples[i].shadow[j] = l->shadow[pos][j];
		}
	}
#endif
#endif

	Mem_Free( l->light );
#ifdef HLRAD_DELUXEMAPPING
	Mem_Free( l->normals );
	Mem_Free( l->deluxe );
#ifdef HLRAD_SHADOWMAPPING
	Mem_Free( l->shadow );
#endif
#endif
}

/*
=============
InitLightinfo
=============
*/
void InitLightinfo( lightinfo_t *l, int facenum )
{
	dface_t	*f;

	f = &g_dfaces[facenum];

	memset( l, 0, sizeof( *l ));
	l->plane = GetPlaneFromFace( f );
	l->surfnum = facenum;
	l->face = f;

	CalcFaceVectors( l );
	CalcFaceExtents( l );
}

/*
=============
BuildFaceLights
=============
*/
void BuildFaceLights( int facenum, int thread )
{
	facelight_t	*fl = &g_facelight[facenum];
	vec3_t		normal;
	int		i, j;
	patch_t		*p;
	dface_t		*f;
	sample_t		*s;
	lightinfo_t	l;
	vec3_t		v;

	f = &g_dfaces[facenum];

	// some surfaces don't need lightmaps
	f->lightofs = -1;
	for( j = 0; j < MAXLIGHTMAPS; j++ )
		f->styles[j] = 255;

	if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		return; // non-lit texture

	if( g_face_patches[facenum] && g_face_patches[facenum]->emitstyle )
		f->styles[0] = g_face_patches[facenum]->emitstyle;

	InitLightinfo( &l, facenum );
	CalcPoints( &l );

	//lazy
	if( g_aa )
		CalcLightmapAA( thread, &l, fl );
	else
		CalcLightmap( thread, &l, fl );

	VectorCopy( l.plane->normal, normal );

	AddSamplesToPatches( thread, fl->samples, f->styles, facenum, &l );

	// average up the direct light on each patch for radiosity
	for( p = g_face_patches[facenum]; p != NULL; p = p->next )
	{
		ASSERT( p->faceNumber == facenum );

		for( int i = 0; i < MAXLIGHTMAPS && p->totalstyle[i] != 255; i++ )
		{
			if( p->samples[i] != 0.0 )
			{
				vec_t	iscale = ( 1.0f / p->samples[i] );

				VectorScale( p->samplelight[i], iscale, v );
				VectorAdd( p->directlight[i], v, p->directlight[i] );
				VectorAdd( p->totallight[i], v, p->totallight[i] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( p->samplelight_dir[i], iscale, v );
				VectorAdd( p->directlight_dir[i], v, p->directlight_dir[i] );
				VectorAdd( p->totallight_dir[i], v, p->totallight_dir[i] );
#endif
			}
		}
	}

	for( p = g_face_patches[facenum]; p != NULL; p = p->next )
	{
		int	leafnum = p->leafnum;
#ifdef HLRAD_DELUXEMAPPING
		GatherSampleLight( thread, l.surfnum, p->trace_origins, leafnum, normal, p->totallight, p->totallight_dir, NULL, p->totalstyle, NULL, 1, NULL );
#else
		GatherSampleLight( thread, l.surfnum, p->origin, leafnum, normal, p->totallight, NULL, NULL, p->totalstyle, NULL, 1 );
#endif
	}

	Mem_Free( l.surfpt );

	// add an ambient term if desired
	if( g_ambient[0] || g_ambient[1] || g_ambient[2] )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] == 255; j++ );
		if( j == MAXLIGHTMAPS ) f->styles[0] = 0; // adding style

		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			if( f->styles[j] == 0 )
			{
				s = fl->samples;
				for( i = 0; i < fl->numsamples; i++, s++ )
				{
					VectorAdd( s->light[j], g_ambient, s->light[j] );
#ifdef HLRAD_DELUXEMAPPING
					vec_t avg = VectorAvg( g_ambient );
					VectorMA( s->deluxe[j], DIFFUSE_DIRECTION_SCALE * avg, s->normal, s->deluxe[j] );
#endif
				}
				break;
			}
		}

	}

	for( j = 0; j < MAXLIGHTMAPS; j++ )
		VectorClear( fl->texlight[j] );

	// light from dlight_threshold and above is sent out, but the
	// texture itself should still be full bright
	if( g_face_patches[facenum] && (!g_noemissive) )
	{
		for( j = 0; j < MAXLIGHTMAPS && f->styles[j] != 255; j++ )
		{
			if( f->styles[j] == g_face_patches[facenum]->emitstyle )
				break;
		}

		if( j == MAXLIGHTMAPS )
		{
			g_overflowed_styles_onpatch[thread]++;
		}
		else
		{
			if( f->styles[j] == 255 )
				f->styles[j] = g_face_patches[facenum]->emitstyle;

			s = fl->samples;
			VectorCopy( g_face_patches[facenum]->baselight, fl->texlight[j] );
			for( i = 0; i < fl->numsamples; i++, s++ )
			{
				VectorAdd( s->light[j], g_face_patches[facenum]->baselight, s->light[j] );
#ifdef HLRAD_DELUXEMAPPING
				vec_t avg = VectorAvg( g_face_patches[facenum]->baselight );
				VectorMA( s->deluxe[j], -DIFFUSE_DIRECTION_SCALE * avg, s->normal, s->deluxe[j] );
#endif
			}
		}
	}
}

/*
=============
PrecompLightmapOffsets
=============
*/
#ifdef HLRAD_PARANOIA_BUMP
void PrecompLightmapOffsets( void )
{
	int		lightstyles;
	facelight_t	*fl;
	dface_t		*f;

	g_shadowdatasize = 0; // unused
	g_normaldatasize = 0; // unused
	g_numvertnormals = 0; // unused
	g_lightdatasize = 0;
	lightstyles = 4;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		f = &g_dfaces[facenum];
		fl = &g_facelight[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue; // non-lit texture

		f->lightofs = g_lightdatasize;
		g_lightdatasize += fl->numsamples * 3 * lightstyles;
	}

	g_dlightdata = (byte *)Mem_Realloc( g_dlightdata, g_lightdatasize );
}
#else
void PrecompLightmapOffsets( void )
{
	int		overflow_styles_onpatch = 0;
	int		overflow_styles_onface = 0;
	int		i, e, facenum;
	int		lightstyles;
	patch_t		*patch;
	dvertex_t		*vert;
	facelight_t	*fl;
	dface_t		*f;

	g_lightdatasize = g_normaldatasize = 0;
	g_numvertnormals = g_numsurfedges; // indexes count is always matched surfedge count
	g_numnormals = 1; // leave first normal empty as error

	for( facenum = 0; facenum < g_numfaces; facenum++ )
	{
		vec3_t	maxlights1[MAXSTYLES];
		vec3_t	maxlights2[MAXSTYLES];
		vec_t	maxlights[MAXSTYLES];
		vec3_t	phongNormal;
		int	j, k;

		f = &g_dfaces[facenum];
		fl = &g_facelight[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		{
			// NOTE: for non-lit faces all the vertex-normals have index0 (empty normal)
			continue; // non-lit texture
		}

		// write smoothed normals for face
		for( i = 0; i < f->numedges; i++ )
		{
			e = g_dsurfedges[f->firstedge+i];

			if( e >= 0 ) vert = g_dvertexes + g_dedges[e].v[0];
			else vert = g_dvertexes + g_dedges[-e].v[1];

			vec3_t temp_point;
			VectorAdd( vert->point, g_face_offset[facenum], temp_point );
			GetPhongNormal( facenum, temp_point, phongNormal );
			
			g_vertnormals[f->firstedge+i] = StoreNormal( phongNormal );
		}

		for( j = 0; j < MAXSTYLES; j++ )
		{
			VectorClear( maxlights1[j] );
			VectorClear( maxlights2[j] );
		}

		for( k = 0; k < MAXLIGHTMAPS && f->styles[k] != 255; k++ )
		{
			for( i = 0; i < fl->numsamples; i++ )
			{
				VectorCompareMax( maxlights1[f->styles[k]], fl->samples[i].light[k], maxlights1[f->styles[k]] );
			}
		}

		int	numpatches;
		const int	*patches;

		GetTriangulationPatches( facenum, &numpatches, &patches ); // collect patches and their neighbors

		for( i = 0; i < numpatches; i++ )
		{
			patch = &g_patches[patches[i]];

			for( k = 0; k < MAXLIGHTMAPS && patch->totalstyle[k] != 255; k++ )
			{
				VectorCompareMax( maxlights2[patch->totalstyle[k]], patch->totallight[k], maxlights2[patch->totalstyle[k]] );
			}
		}

		for( j = 0; j < MAXSTYLES; j++ )
		{
			vec3_t	v;

			VectorAdd( maxlights1[j], maxlights2[j], v );
			maxlights[j] = VectorMaximum( v );
		}

		byte	oldstyles[MAXLIGHTMAPS];
		sample_t	*oldsamples = (sample_t *)Mem_Alloc( sizeof( sample_t ) * fl->numsamples );
		vec3_t		oldtexlight[MAXLIGHTMAPS];

		for( k = 0; k < MAXLIGHTMAPS; k++ )
		{
			oldstyles[k] = f->styles[k];
			VectorCopy( fl->texlight[k], oldtexlight[k] );
			VectorClear( fl->texlight[k] );
		}

		// make backup and clear the source
		for( k = 0; k < fl->numsamples; k++ )
		{
			for( j = 0; j < MAXLIGHTMAPS; j++ )
			{
				VectorCopy( fl->samples[k].light[j], oldsamples[k].light[j] );
				VectorClear( fl->samples[k].light[j] );
#ifdef HLRAD_DELUXEMAPPING
				VectorCopy( fl->samples[k].deluxe[j], oldsamples[k].deluxe[j] );
				VectorClear( fl->samples[k].deluxe[j] );
#ifdef HLRAD_SHADOWMAPPING
				oldsamples[k].shadow[j] = fl->samples[k].shadow[j];
				fl->samples[k].shadow[j] = 0.0f;
#endif
#endif
			}
		}

		for( k = 0; k < MAXLIGHTMAPS; k++ )
		{
			byte	beststyle = 255;

			if( k == 0 )
			{
				beststyle = 0;
			}
			else
			{
				vec_t	bestmaxlight = 0;

				for( j = 1; j < MAXSTYLES; j++ )
				{
					if( maxlights[j] > bestmaxlight + NORMAL_EPSILON )
					{
						bestmaxlight = maxlights[j];
						beststyle = j;
					}
				}
			}

			if( beststyle != 255 )
			{
				maxlights[beststyle] = 0;
				f->styles[k] = beststyle;

				for( i = 0; i < MAXLIGHTMAPS && oldstyles[i] != 255; i++ )
				{
					if( oldstyles[i] == f->styles[k] )
						break;
				}

				VectorCopy( oldtexlight[i], fl->texlight[k] );

				if( i < MAXLIGHTMAPS && oldstyles[i] != 255 )
				{
					for( j = 0; j < fl->numsamples; j++ )
					{
						VectorCopy( oldsamples[j].light[i], fl->samples[j].light[k] );
#ifdef HLRAD_DELUXEMAPPING
						VectorCopy( oldsamples[j].deluxe[i], fl->samples[j].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
						fl->samples[j].shadow[k] = oldsamples[j].shadow[i];
#endif
#endif
					}
				}
				else
				{
					for( j = 0; j < fl->numsamples; j++ )
					{
						VectorClear( fl->samples[j].light[k] );
#ifdef HLRAD_DELUXEMAPPING
						VectorClear( fl->samples[j].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
						fl->samples[j].shadow[k] = 0.0f;
#endif
#endif
					}
				}
			}
			else
			{
				f->styles[k] = 255;

				for( j = 0; j < fl->numsamples; j++ )
				{
					VectorClear( fl->samples[j].light[k] );
#ifdef HLRAD_DELUXEMAPPING
					VectorClear( fl->samples[j].deluxe[k] );
#ifdef HLRAD_SHADOWMAPPING
					fl->samples[j].shadow[k] = 0.0f;
#endif
#endif
				}
			}
		}

		Mem_Free( oldsamples );

		for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
		{
			if( f->styles[lightstyles] == 255 )
				break; // end if styles
		}

		if( !lightstyles ) continue;

		f->lightofs = g_lightdatasize;
		g_lightdatasize += fl->numsamples * 3 * lightstyles;
	}
#ifdef HLRAD_LIGHTMAPMODELS
	PrecompModelLightmapOffsets();
#endif
	// now we have lightdata size and alloc the arrays
	// g-cont. we use realloc in case we already have valid lighting data and do it again
	if( g_found_extradata )
	{
#ifdef HLRAD_DELUXEMAPPING
		g_ddeluxdata = (byte *)Mem_Realloc( g_ddeluxdata, g_lightdatasize );
		g_deluxdatasize = g_lightdatasize;
#ifdef HLRAD_SHADOWMAPPING
		g_dshadowdata = (byte *)Mem_Realloc( g_dshadowdata, g_lightdatasize / 3 );
		g_shadowdatasize = g_lightdatasize / 3;
#endif
#endif
	}
	g_dlightdata = (byte *)Mem_Realloc( g_dlightdata, g_lightdatasize );

	// calc normal datasize
	g_normaldatasize = sizeof( dnormallump_t ) + ( g_numvertnormals * sizeof( dvertnorm_t )) + (g_numnormals * sizeof( dnormal_t ));
	g_dnormaldata = (byte *)Mem_Realloc( g_dnormaldata, g_normaldatasize );

	// write indexed normals into memory
	byte *buffer = g_dnormaldata;
	dnormallump_t *normhdr = (dnormallump_t *)buffer;

	normhdr->ident = NORMIDENT;
	normhdr->numnormals = g_numnormals; // this is trivialy to compute in engine but i'm leave this count for bounds checking
	buffer += sizeof( dnormallump_t );

	// store normal indexes
	memcpy( buffer, g_vertnormals, g_numvertnormals * sizeof( dvertnorm_t ));
	buffer += g_numvertnormals * sizeof( dvertnorm_t );

	// store unique normals
	memcpy( buffer, g_dnormals, g_numnormals * sizeof( dnormal_t ));
	buffer += g_numnormals * sizeof( dnormal_t );

	if(( buffer - g_dnormaldata ) != g_normaldatasize )
		COM_FatalError( "WriteVertexNormals: memory corrupted\n" );

	// now count how many styles was overflowed
	for( i = 0; i < MAX_THREADS; i++ )
	{
		overflow_styles_onpatch += g_overflowed_styles_onpatch[i];
		overflow_styles_onface += g_overflowed_styles_onface[i];
	}

	if( overflow_styles_onface > 0 )
		Msg( "^3Warning:^7 too many light styles on a face (%i faces overflowed)\n", overflow_styles_onface ); 

	if( overflow_styles_onpatch > 0 )
		Msg( "^3Warning:^7 too many light styles on a patch (%i patches overflowed)\n", overflow_styles_onpatch ); 
}
#endif

/*
============
CreateFacelightDependencyList
============
*/
void CreateFacelightDependencyList( void )
{
	facelist_t	*item;
	facelight_t	*fl;
	dface_t		*f;

	for( int i = 0; i < MAX_MAP_FACES; i++ )
		g_dependentfacelights[i] = NULL;

	// for each face
	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		f = &g_dfaces[facenum];
		fl = &g_facelight[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue;

		for( int i = 0; i < fl->numsamples; i++ )
		{
			int	surface = fl->samples[i].surface; // that surface contains at least one sample from this face

			if( surface >= 0 && surface < g_numfaces )
			{
				// insert this face into the dependency list of that surface
				for( item = g_dependentfacelights[surface]; item != NULL; item = item->next )
				{
					if( item->facenum == facenum )
						break;
				}

				if( item ) continue; // already added?

				item = (facelist_t *)Mem_Alloc( sizeof( facelist_t ));

				item->next = g_dependentfacelights[surface];
				g_dependentfacelights[surface] = item;
				item->facenum = facenum;
			}
		}
	}
}

/*
============
FreeFacelightDependencyList
============
*/
void FreeFacelightDependencyList( void )
{
	facelist_t	*item;

	for( int i = 0; i < MAX_MAP_FACES; i++ )
	{
		while( g_dependentfacelights[i] )
		{
			item = g_dependentfacelights[i];
			g_dependentfacelights[i] = item->next;
			Mem_Free( item );
		}
	}
}

/*
============
CalcSampleSize
============
*/
void CalcSampleSize( void )
{
	facelight_t	*fl;
	dface_t		*f;
	size_t		samples_total_size = 0;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		f = &g_dfaces[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue;

		fl = &g_facelight[facenum];
		samples_total_size += fl->numsamples * sizeof( sample_t );
	}

	Msg( "total facelight data: %s\n", Q_memprint( samples_total_size ));
}

/*
============
CalcLuxelsCount
============
*/
void CalcLuxelsCount( void )
{
	size_t	total_luxels = 0;
	size_t	lighted_luxels = 0;

	for( int i = 0; i < MAX_THREADS; i++ )
	{
		total_luxels += g_direct_luxels[i];
		lighted_luxels += g_lighted_luxels[i];
	}
	Msg( "%d luxels affected by direct light\n", total_luxels );
	Msg( "%d luxels reached by direct light\n", lighted_luxels );
}

/*
============
ScaleDirectLights
============
*/
void ScaleDirectLights( void )
{
#ifndef HLRAD_PARANOIA_BUMP
	sample_t		*samp;
	facelight_t	*fl;
	dface_t		*f;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		f = &g_dfaces[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue;

		fl = &g_facelight[facenum];

		for( int k = 0; k < MAXLIGHTMAPS && f->styles[k] != 255; k++ )
		{
			for( int i = 0; i < fl->numsamples; i++ )
			{
				samp = &fl->samples[i];
				VectorScale( samp->light[k], g_direct_scale, samp->light[k] );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( samp->deluxe[k], g_direct_scale, samp->deluxe[k] );
#endif
			}
		}
	}
#endif
#ifdef HLRAD_LIGHTMAPMODELS
	ScaleModelDirectLights();
#endif
}

/*
============
FacePatchLights

This function is run multithreaded
============
*/
void FacePatchLights( int facenum, int threadnum )
{
	dface_t		*f_other;
	facelight_t	*fl_other;
	vec3_t		v, v_dir;
	facelist_t	*item;
	sample_t	*samp;
	dface_t		*f;

	f = &g_dfaces[facenum];

	if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		return;

	for( item = g_dependentfacelights[facenum]; item != NULL; item = item->next )
	{
		f_other = &g_dfaces[item->facenum];
		fl_other = &g_facelight[item->facenum];

		for( int k = 0; k < MAXLIGHTMAPS && f_other->styles[k] != 255; k++ )
		{
			for( int i = 0; i < fl_other->numsamples; i++ )
			{
				samp = &fl_other->samples[i];

				if( samp->surface != facenum )
				{
					// the sample is not in this surface
					continue;
				}

				int style = f_other->styles[k];
				InterpolateSampleLight( samp->pos, samp->surface, 1, &style, &v, &v_dir );
#ifdef HLRAD_PARANOIA_BUMP
				if( f_other->styles[k] == STYLE_ORIGINAL_LIGHT )
				{
					VectorAdd( samp->light[STYLE_ORIGINAL_LIGHT], v, samp->light[STYLE_ORIGINAL_LIGHT] );
					VectorAdd( samp->light[STYLE_INDIRECT_LIGHT], v, samp->light[STYLE_INDIRECT_LIGHT] );
				}
				else if( f_other->styles[k] == STYLE_BUMPED_LIGHT )
				{
					// store indirect light into separate style
					VectorAdd( samp->deluxe[STYLE_BUMPED_LIGHT], v_dir, samp->deluxe[STYLE_BUMPED_LIGHT] );
				}
				// only STYLE_ORIGINAL_LIGHT and STYLE_BUMPED_LIGHT are handled
#else
				VectorScale( v, g_indirect_scale, v );
				VectorAdd( samp->light[k], v, v );
#ifdef HLRAD_DELUXEMAPPING
				VectorScale( v_dir, g_indirect_scale, v_dir );
				VectorAdd( samp->deluxe[k], v_dir, v_dir );
#endif

				VectorCopy( v, samp->light[k] );
#ifdef HLRAD_DELUXEMAPPING
				VectorCopy( v_dir, samp->deluxe[k] );
#endif

#endif
			}
		}
	}
}

/*
=============
ApplyLightPostprocess
=============
*/
void ApplyLightPostprocess( vec3_t lb, const float minlight )
{
	int i;
	for( i = 0; i < 3; i++ )
		lb[i] *= g_scale;

	if( g_tonemap )
	{
		vec_t luminance = lb[0] * 0.2126f + lb[1] * 0.7152f + lb[2] * 0.0722f;
		for( i = 0; i < 3; i++ )
		{
			vec_t tv = lb[i] / sqrtf( 1.0f + lb[i] * lb[i] );
			lb[i] = lb[i] / (1.0f + luminance) * (1.0f - tv ) + tv * tv;
		}
	}	


	for( i = 0; i < 3; i++ )
	{
		lb[i] = powf( lb[i], g_gamma );

		if( g_hdrcompresslog )
		{
			lb[i] = 1.0f - expf( -lb[i] );						//ln
			//lb[i] = 1.0f - powf( 4.0f, -lb[i] );				//log4
			//lb[i] = lb[i] / sqrtf( 1.0f + lb[i] * lb[i] );	//inverse sqrt
			lb[i] = Q_min( lb[i], 254.1f / 255.0f );
		}

		lb[i] *= 255.0f;
		
		lb[i] = Q_max( lb[i], minlight );
	}

	if( !g_tonemap && !g_hdrcompresslog )
	{
		if( lb[0] > g_maxlight || lb[1] > g_maxlight || lb[2] > g_maxlight )	// clip from the top
		{
			// find max value and scale the whole color down;
			vec_t	max = VectorMax( lb );

			for( i = 0; i < 3; i++ )
				lb[i] = ( lb[i] * g_maxlight ) / max;
		}
	}
}

/*
=============
FinalLightFace

Add the indirect lighting on top of the direct
lighting and save into final map format
=============
*/
#ifdef HLRAD_PARANOIA_BUMP
void ApplyLightGamma( vec3_t lb )
{
	// clip from the top
	if( lb[0] > g_maxlight || lb[1] > g_maxlight || lb[2] > g_maxlight )
	{
		// find max value and scale the whole color down;
		float	max = VectorMax( lb );

		for( int i = 0; i < 3; i++ )
			lb[i] = ( lb[i] * g_maxlight ) / max;
	}

	// do gamma adjust
	if( g_gammamode == 0 )
	{
		lb[0] = (float)pow( lb[0] / 255.0f, g_gamma ) * 255.0f;
		lb[1] = (float)pow( lb[1] / 255.0f, g_gamma ) * 255.0f;
		lb[2] = (float)pow( lb[2] / 255.0f, g_gamma ) * 255.0f;
	}
	else
	{
		float	avg, avgbefore;

		if( g_gammamode == 2 )
			avg = VectorMaximum( lb );
		else avg = VectorAvg( lb );
		avgbefore = avg;

		if( avgbefore )
		{
			avg = (float)pow( avg / 255.0f, g_gamma ) * 255.0f;
			avg = avg / avgbefore;
			VectorScale( lb, avg, lb );
		}
	}
}

void PackColor( vec3_t lb, int facenum, int style, int sample )
{
	dface_t		*f = &g_dfaces[facenum];
	facelight_t	*fl = &g_facelight[facenum];

	// check
	for( int i = 0; i < 3; i++ )
	{
		if( lb[i] < 0.0f ) MsgDev( D_WARN, "component %d on style %d is too low: %f\n", i, style, lb[i] );
		if( lb[i] > 255.0f ) MsgDev( D_WARN, "component %d on style %d is too high: %f\n", i, style, lb[i] );
	}

	g_dlightdata[f->lightofs + style * fl->numsamples * 3 + sample * 3 + 0] = (byte)lb[0];
	g_dlightdata[f->lightofs + style * fl->numsamples * 3 + sample * 3 + 1] = (byte)lb[1];
	g_dlightdata[f->lightofs + style * fl->numsamples * 3 + sample * 3 + 2] = (byte)lb[2];
}

void FinalLightFace( int facenum, int threadnum )
{
	vec_t		lmvecs[2][4];
	sample_t		*samp;
	dtexinfo_t	*tx;
	facelight_t	*fl;
	dface_t		*f;

	f = &g_dfaces[facenum];
	fl = &g_facelight[facenum];
	tx = &g_texinfo[f->texinfo];
	samp = fl->samples;

	if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		return; // non-lit texture

	f->styles[0] = 0; // everyone gets the style zero map.
	f->styles[BUMP_LIGHTVECS_MAP] = BUMP_LIGHTVECS_STYLE;
	f->styles[BUMP_BASELIGHT_MAP] = BUMP_BASELIGHT_STYLE;
	f->styles[BUMP_ADDLIGHT_MAP] = BUMP_ADDLIGHT_STYLE;

	// we assume what all the four styles is always present
	LightMatrixFromTexMatrix( tx, lmvecs );

	for( int j = 0; j < fl->numsamples; j++, samp++ )
	{
		const vec3_t	&facenormal = GetPlaneFromFace( f )->normal;
		vec3_t		directionnormals[3];
		vec3_t		texdirections[2];
		vec3_t		original_light;	// direct + indirect
		vec3_t		light_deluxe;	// deluxemap
		vec3_t		light_diffuse;	// direct light
		vec3_t		light_indirect;	// radiosity
		vec3_t		direction;
		int		side;

		VectorCopy( samp->normal, directionnormals[2] );

		for( side = 0; side < 2; side++ )
		{
			CrossProduct( facenormal, lmvecs[!side], texdirections[side] );
			VectorNormalize( texdirections[side] );
			if( DotProduct( texdirections[side], lmvecs[side]) < 0.0f )
				VectorNegate( texdirections[side], texdirections[side] );
		}

		for( side = 0; side < 2; side++ )
		{
			vec_t	dot = DotProduct( texdirections[side], samp->normal );
			VectorMA( texdirections[side], -dot, samp->normal, directionnormals[side] );
			VectorNormalize( directionnormals[side] );
		}

//		VectorNegate( directionnormals[1], directionnormals[1] );

		// prepare four styles
		VectorScale( samp->light[STYLE_ORIGINAL_LIGHT], g_direct_scale, original_light ); 
		VectorScale( samp->light[STYLE_BUMPED_LIGHT], g_direct_scale, light_diffuse ); 
		VectorScale( samp->light[STYLE_INDIRECT_LIGHT], g_direct_scale, light_indirect ); 
		VectorScale( samp->deluxe[STYLE_BUMPED_LIGHT], g_direct_scale, direction );
		VectorNegate( direction, direction ); // let the direction point from face sample to light source
		VectorNormalize( direction );

		ApplyLightGamma( original_light ); // apply standard gamma rules
		ApplyLightGamma( light_indirect ); // apply standard gamma rules
		ApplyLightGamma( light_diffuse ); // apply standard gamma rules

		// buz - divide by two bump styles.
		// this decreases precision, but increases range
		VectorScale( light_indirect, 0.5f, light_indirect );
		VectorScale( light_diffuse, 0.5f, light_diffuse );

		// turn to tangent space
		for( int x = 0; x < 3; x++ )
		{
			light_deluxe[x] = DotProduct( direction, directionnormals[x] ) * 127.0f + 128.0f;
			light_deluxe[x] = bound( 0, light_deluxe[x], 255.0 );
		}

		PackColor( original_light, facenum, 0, j );
		PackColor( light_indirect, facenum, 1, j );
		PackColor( light_deluxe, facenum, 2, j );
		PackColor( light_diffuse,  facenum, 3, j );
	}
}
#else
void FinalLightFace( int facenum, int threadnum )
{
	vec_t		lmvecs[2][4];
	int		lightstyles;
	float		minlight;
	int		i, j, k;
	sample_t	*samp;
	dtexinfo_t	*tx;
	facelight_t	*fl;
	dface_t		*f;
	vec3_t		lb;

	f = &g_dfaces[facenum];
	fl = &g_facelight[facenum];
	tx = &g_texinfo[f->texinfo];

	if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
		return; // non-lit texture

	for( lightstyles = 0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
	{
		if( f->styles[lightstyles] == 255 )
			break;
	}

	if( !lightstyles ) return;

	LightMatrixFromTexMatrix( tx, lmvecs );

	minlight = FloatForKey( g_face_entity[facenum], "_minlight" );
	if( minlight < 1.0 ) minlight *= 128.0f; // GoldSrc
	else minlight *= 0.5f; // Quake

	if( g_lightbalance )
		minlight *= g_direct_scale;
	if( g_numbounce > 0 ) minlight = 0.0f; // ignore for radiosity

	for( k = 0; k < lightstyles; k++ )
	{
		samp = fl->samples;

		VectorClear( fl->average[k] );

		for( j = 0; j < fl->numsamples; j++, samp++ )
		{
#ifdef HLRAD_DELUXEMAPPING
			const vec3_t	&facenormal = GetPlaneFromFace( f )->normal;
			vec3_t		directionnormals[3];
			vec3_t		texdirections[2];
			vec3_t		direction;
			int			side;
			vec3_t		v;

			VectorCopy( samp->normal, directionnormals[2] );

			for( side = 0; side < 2; side++ )
			{
				CrossProduct( facenormal, lmvecs[!side], texdirections[side] );
				VectorNormalize( texdirections[side] );
				if( DotProduct( texdirections[side], lmvecs[side]) < 0.0f )
					VectorNegate( texdirections[side], texdirections[side] );
			}

			for( side = 0; side < 2; side++ )
			{
				vec_t	dot = DotProduct( texdirections[side], samp->normal );
				VectorMA( texdirections[side], -dot, samp->normal, directionnormals[side] );
				VectorNormalize( directionnormals[side] );
			}
			VectorNegate( directionnormals[1], directionnormals[1] );
#endif
			VectorCopy( samp->light[k], lb ); 
			VectorAdd( fl->average[k], samp->light[k], fl->average[k] );
#ifdef HLRAD_DELUXEMAPPING
			VectorCopy( samp->deluxe[k], direction );
			vec_t avg = VectorAvg( lb );

			if (avg > 0.0f)
				VectorScale( direction, 1.0f / avg, direction );

			if( g_delambert )
			{
				VectorCopy( direction, v );
				VectorNormalize( v );
				vec_t lambert = DotProduct( v, directionnormals[2] );

				if( lambert > NORMAL_EPSILON )
					VectorScale( lb, 1.0f / lambert, lb );						
			}
#endif


			ApplyLightPostprocess( lb, minlight );
	

#ifdef HLRAD_RIGHTROUND	// when you go down, when you go down down!
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = Q_rint( lb[0] );
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = Q_rint( lb[1] );
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = Q_rint( lb[2] );
#else
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = (byte)lb[0];
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = (byte)lb[1];
			g_dlightdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = (byte)lb[2];
#endif

#ifdef HLRAD_DELUXEMAPPING
			if( g_deluxdatasize )
			{
				VectorCopy( direction, v );

				if( DotProduct( v, v ) > 1.0f )
					VectorNormalize( v );

				if( VectorIsNull( v ) )
					VectorCopy( directionnormals[2], v );

				for( int x = 0; x < 3; x++ )
				{
					if( g_worldspace )
						lb[x] = v[x] * 127.0f + 128.0f;
					else
						lb[x] = DotProduct( v, directionnormals[x] ) * 127.0f + 128.0f;
					
					lb[x] = bound( 0.0f, lb[x], 255.0f );
				}

				g_ddeluxdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 0] = (byte)lb[0];
				g_ddeluxdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 1] = (byte)lb[1];
				g_ddeluxdata[f->lightofs + k * fl->numsamples * 3 + j * 3 + 2] = (byte)lb[2];
			}
#ifdef HLRAD_SHADOWMAPPING
			if( g_shadowdatasize )
				g_dshadowdata[(f->lightofs / 3) + k * fl->numsamples + j] = (byte)samp->shadow[k] * 255;
#endif
#endif
		}

		VectorScale( fl->average[k], 1.0f / (float)fl->numsamples, fl->average[k] );
	}
}
#endif

void FreeFaceLights( void )
{
	for( int i = 0; i < g_numfaces; i++ )
	{
		Mem_Free( g_facelight[i].samples );
	}
#ifdef HLRAD_LIGHTMAPMODELS
	FreeModelFaceLights();
#endif
}

void ReduceLightmap( void )
{
	// store compiler settings to help matching with dynamic lighting
	SetKeyValue( &g_entities[0], "_lightgamma", va( "%.2f", g_gamma ));
	SetKeyValue( &g_entities[0], "_dscale", va( "%.2f", g_direct_scale ));
	SetKeyValue( &g_entities[0], "_ambient", va( "%.2f %.2f %.2f", g_ambient[0], g_ambient[1], g_ambient[2] ));
	SetKeyValue( &g_entities[0], "_maxlight", va( "%.2f", g_maxlight ));
	SetKeyValue( &g_entities[0], "_smooth", va( "%.2f", g_smoothvalue ));
	if( g_hdrcompresslog )
		SetKeyValue( &g_entities[0], "_hdr", "compress_log" );
	if( g_skystyle )
		SetKeyValue( &g_entities[0], "_skystyle", va( "%d", g_skystyle ));	

	int	oldsize = g_lightdatasize;

	byte	*oldlightdata = (byte *)Mem_Alloc( g_lightdatasize, C_SAFEALLOC );

	if( !oldlightdata )
	{
		MsgDev( D_WARN, "failed to reduce lightmap due to failed to allocate memory for lightdata\n" );
		return;
	}
	memcpy( oldlightdata, g_dlightdata, g_lightdatasize );

#ifdef HLRAD_DELUXEMAPPING
	byte	*olddeluxdata = (byte *)Mem_Alloc( g_deluxdatasize, C_SAFEALLOC );

	if( !olddeluxdata )
	{
		MsgDev( D_WARN, "failed to reduce lightmap due to failed to allocate memory for deluxdata\n" );
		Mem_Free( oldlightdata, C_SAFEALLOC );
		return;
	}
	memcpy( olddeluxdata, g_ddeluxdata, g_deluxdatasize );
#ifdef HLRAD_SHADOWMAPPING
	byte	*oldshadowdata = (byte *)Mem_Alloc( g_shadowdatasize, C_SAFEALLOC );

	if( !oldshadowdata )
	{
		MsgDev( D_WARN, "failed to reduce lightmap due to failed to allocate memory for shadowdata\n" );
		Mem_Free( oldlightdata, C_SAFEALLOC );
		Mem_Free( olddeluxdata, C_SAFEALLOC );
		return;
	}
	memcpy( oldshadowdata, g_dshadowdata, g_shadowdatasize );
#endif
#endif
	g_lightdatasize = 0;
	g_deluxdatasize = 0;
	g_shadowdatasize = 0;

	for( int facenum = 0; facenum < g_numfaces; facenum++ )
	{
		dface_t		*f = &g_dfaces[facenum];
		facelight_t	*fl = &g_facelight[facenum];

		if( FBitSet( g_texinfo[f->texinfo].flags, TEX_SPECIAL ))
			continue; // non-lit texture

		if( f->lightofs == -1 )
			continue;

		byte	oldstyles[MAXLIGHTMAPS];
		int	numstyles = 0;
		int	i, k, oldofs;

		oldofs = f->lightofs;
		f->lightofs = g_lightdatasize;

		for( k = 0; k < MAXLIGHTMAPS; k++ )
		{
			oldstyles[k] = f->styles[k];
			f->styles[k] = 255;
		}

		for( k = 0; k < MAXLIGHTMAPS && oldstyles[k] != 255; k++ )
		{
			int	count = fl->numsamples;
			byte	maxb = 0;

			for( i = 0; i < count; i++ )
			{
				byte *v = &oldlightdata[oldofs + count * 3 * k + i * 3];
				maxb = Q_max( maxb, VectorMaximum( v ));
			}

			if( maxb <= 0 ) // black
				continue;

			f->styles[numstyles] = oldstyles[k];
			memcpy( &g_dlightdata[f->lightofs + count * 3 * numstyles], &oldlightdata[oldofs + count * 3 * k], count * 3 );
#ifdef HLRAD_DELUXEMAPPING
			if( g_ddeluxdata != NULL )
				memcpy( &g_ddeluxdata[f->lightofs + count * 3 * numstyles], &olddeluxdata[oldofs + count * 3 * k], count * 3 );
#ifdef HLRAD_SHADOWMAPPING
			if( g_dshadowdata != NULL )
				memcpy( &g_dshadowdata[(f->lightofs/3) + count * numstyles], &oldshadowdata[(oldofs / 3) + count * k], count );
#endif
#endif
			numstyles++;
		}
		g_lightdatasize += fl->numsamples * 3 * numstyles;
#ifdef HLRAD_DELUXEMAPPING
		if( g_ddeluxdata != NULL )
			g_deluxdatasize += fl->numsamples * 3 * numstyles;
#ifdef HLRAD_SHADOWMAPPING
		if( g_dshadowdata != NULL )
			g_shadowdatasize += fl->numsamples * numstyles;
#endif
#endif
	}

#ifdef HLRAD_LIGHTMAPMODELS
#ifdef HLRAD_DELUXEMAPPING
#ifdef HLRAD_SHADOWMAPPING
	ReduceModelLightmap( oldlightdata, olddeluxdata, oldshadowdata );
#else
	ReduceModelLightmap( oldlightdata, olddeluxdata, NULL );
#endif
#else
	ReduceModelLightmap( oldlightdata, NULL, NULL );
#endif
#endif
	Mem_Free( oldlightdata, C_SAFEALLOC );
#ifdef HLRAD_DELUXEMAPPING
	Mem_Free( olddeluxdata, C_SAFEALLOC );
#ifdef HLRAD_SHADOWMAPPING
	Mem_Free( oldshadowdata, C_SAFEALLOC );
#endif
#endif
	int	newsize = g_lightdatasize;

	if( oldsize != newsize )
	{
#ifdef HLRAD_DELUXEMAPPING
		Msg( "Reduce lightmap from %s to %s\n", Q_memprint( oldsize * 2 + ( oldsize / 3 )), Q_memprint( newsize * 2 + ( newsize / 3 )));
#else
		Msg( "Reduce lightmap from %s to %s\n", Q_memprint( oldsize + ( oldsize / 3 )), Q_memprint( newsize + ( newsize / 3 )));
#endif
	}
}
