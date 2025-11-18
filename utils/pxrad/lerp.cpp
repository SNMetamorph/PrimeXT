/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// lerp.c

#include "qrad.h"
#include "utlarray.h"

#define TRIANGLE_SHAPE_THRESHOLD	DEG2RAD( 115.0 )

struct interpolation_t
{
	struct Point
	{
		int	patchnum;
		vec_t	weight;
	};
	
	bool		isbiased;
	vec_t		totalweight;
	CUtlArray<Point>	points;
};

// replace std::pair
struct pair_t
{
	vec_t		first;
	int		second;
};

struct localtrian_t
{
	struct Wedge
	{
		enum eShape
		{
			eTriangular,
			eConvex,
			eConcave,
			eSquareLeft,
			eSquareRight,
		};
		
		eShape		shape;
		int		leftpatchnum;
		vec3_t		leftspot;
		vec3_t		leftdirection;
		// right side equals to the left side of the next wedge
		vec3_t		wedgenormal; // for custom usage
	};

	struct HullPoint
	{
		vec3_t		spot;
		vec3_t		direction;
	};
	
	dplane_t			plane;
	winding_t			*winding;
	vec3_t			center; // center is on the plane

	vec3_t			normal;
	int			patchnum;

	CUtlArray< int >		neighborfaces; // including the face itself

	CUtlArray<Wedge>		sortedwedges; // in clockwise order (same as Winding)
	CUtlArray<HullPoint>	sortedhullpoints; // in clockwise order (same as Winding)
};

struct facetriangulation_t
{
	struct Wall
	{
		vec3_t	points[2];
		vec3_t	direction;
		vec3_t	normal;
	};

	int			facenum;
	CUtlArray< int >		neighbors; // including the face itself
	CUtlArray< Wall >		walls;
	CUtlArray< localtrian_t * >	localtriangulations;
	CUtlArray< int >		usedpatches;
};

// replace std::sort
int __cdecl LessThan( const pair_t *s0, const pair_t *s1 )
{
	if( s0->first > s1->first )
		return 1;
	if( s0->first < s1->first )
		return -1;
	if( s0->second > s1->second )
		return 1;
	if( s0->second < s1->second )
		return -1;
	return 0;
}

facetriangulation_t	*g_facetriangulations[MAX_MAP_FACES];
bool		g_drawlerp = false;

// If the surface formed by the face and its neighbor faces is not flat, the surface should be unfolded onto the face plane
// CalcAdaptedSpot calculates the coordinate of the unfolded spot on the face plane from the original position on the surface
// CalcAdaptedSpot(center) = {0,0,0}
// CalcAdaptedSpot(position on the face plane) = position - center
// Param position: must include g_face_offset
static bool CalcAdaptedSpot( const localtrian_t *lt, const vec3_t position, int surface, vec3_t spot )
{
	vec3_t	surfacespot;
	vec_t	dist, dist2;
	vec3_t	phongnormal;
	vec_t	dot, frac;
	vec3_t	v, middle;
	int	i;

	for( i = 0; i < lt->neighborfaces.Count(); i++ )
	{
		if( lt->neighborfaces[i] == surface )
			break;
	}

	if( i == lt->neighborfaces.Count( ))
	{
		VectorClear( spot );
		return false;
	}

	VectorSubtract( position, lt->center, surfacespot );
	dot = DotProduct( surfacespot, lt->normal );
	VectorMA( surfacespot, -dot, lt->normal, spot );

	// use phong normal instead of face normal, because phong normal is a continuous function
	GetPhongNormal( surface, position, phongnormal );
	dot = DotProduct( spot, phongnormal );

	if( fabs( dot ) > ON_EPSILON )
	{
		frac = DotProduct( surfacespot, phongnormal ) / dot;
		frac = bound( 0, frac, 1 ); // to correct some extreme cases
	}
	else frac = 0;

	VectorScale( spot, frac, middle );
	dist = VectorLength( spot );
	VectorSubtract( surfacespot, middle, v );
	dist2 = VectorLength( middle ) + VectorLength( v );

	if( dist > ON_EPSILON && fabs( dist2 - dist ) > ON_EPSILON )
		VectorScale( spot, dist2 / dist, spot );

	return true;
}

static vec_t GetAngle( const vec3_t leftdirection, const vec3_t rightdirection, const vec3_t normal )
{
	vec_t	angle;
	vec3_t	v;
	
	CrossProduct( rightdirection, leftdirection, v );
	angle = atan2( DotProduct( v, normal ), DotProduct( rightdirection, leftdirection ));

	return angle;
}

static vec_t GetAngleDiff( vec_t angle, vec_t base )
{
	vec_t	diff = angle - base;

	if( diff < 0 )
		diff += 2 * M_PI;
	return diff;
}

static vec_t GetFrac( const vec3_t leftspot, const vec3_t rightspot, const vec3_t direction, const vec3_t normal )
{
	vec_t	dot1;
	vec_t	dot2;
	vec_t	frac;
	vec3_t	v;

	CrossProduct( direction, normal, v );
	dot1 = DotProduct( leftspot, v );
	dot2 = DotProduct( rightspot, v );
	
	// dot1 <= 0 < dot2
	if( dot1 >= -NORMAL_EPSILON )
	{
		if( g_drawlerp && dot1 > ON_EPSILON )
			MsgDev( D_REPORT, "Debug: triangulation: internal error 1.\n" );
		frac = 0.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		if( g_drawlerp && dot2 < -ON_EPSILON )
			MsgDev( D_REPORT, "Debug: triangulation: internal error 2.\n" );
		frac = 1.0;
	}
	else
	{
		frac = dot1 / (dot1 - dot2);
		frac = bound( 0, frac, 1 );
	}

	return frac;
}

static vec_t GetDirection( const vec3_t spot, const vec3_t normal, vec3_t direction_out )
{
	vec_t	dot = DotProduct( spot, normal );
	VectorMA( spot, -dot, normal, direction_out );
	return VectorNormalize( direction_out );
}

// It returns true when the point is inside the hull region (with boundary), even if weight = 0.
static bool CalcWeight( const localtrian_t *lt, const vec3_t spot, vec_t *weight_out )
{
	vec_t			angle, len;
	vec_t			frac, dist;
	vec3_t			direction;
	bool			istoofar;
	CUtlArray< vec_t >		angles;
	vec_t			ratio;
	const localtrian_t::HullPoint	*hp1;
	const localtrian_t::HullPoint	*hp2;
	int			i, j;

	if( GetDirection( spot, lt->normal, direction ) <= 2 * ON_EPSILON )
	{
		*weight_out = 1.0;
		return true;
	}

	if( lt->sortedhullpoints.Count() == 0 )
	{
		*weight_out = 0.0;
		return false;
	}

	angles.SetCount( lt->sortedhullpoints.Count() );

	for( i = 0; i < lt->sortedhullpoints.Count(); i++ )
	{
		angle = GetAngle( lt->sortedhullpoints[i].direction, direction, lt->normal );
		angles[i] = GetAngleDiff( angle, 0 );
	}

	for( i = 1, j = 0; i < lt->sortedhullpoints.Count(); i++ )
	{
		if( angles[i] < angles[j] )
			j = i;
	}

	hp1 = &lt->sortedhullpoints[j];
	hp2 = &lt->sortedhullpoints[(j + 1) % lt->sortedhullpoints.Count()];

	frac = GetFrac( hp1->spot, hp2->spot, direction, lt->normal );
	
	len = (1.0 - frac) * DotProduct( hp1->spot, direction ) + frac * DotProduct( hp2->spot, direction );
	dist = DotProduct( spot, direction );

	if( len <= ON_EPSILON / 4 || dist > len + 2 * ON_EPSILON )
	{
		istoofar = true;
		ratio = 1.0;
	}
	else if( dist >= len - ON_EPSILON )
	{
		istoofar = false; // if we change this "false" to "true", we will see many places turned "green" in "-drawlerp" mode
		ratio = 1.0; // in order to prevent excessively small weight
	}
	else
	{
		istoofar = false;
		ratio = dist / len;
		ratio = bound( 0, ratio, 1 );
	}

	*weight_out = 1 - ratio;
	return !istoofar;
}

static void CalcInterpolation_Square( const localtrian_t *lt, int i, const vec3_t spot, interpolation_t *interp )
{
	vec_t				frac, frac_near, frac_far;
	vec3_t				normal, normal1, normal2;
	vec3_t				test, mid_far, mid_near;
	vec_t				dot, dot1, dot2, ratio;
	vec_t				weights[4];
	const localtrian_t::Wedge	*w1;
	const localtrian_t::Wedge	*w2;
	const localtrian_t::Wedge	*w3;
	
	w1 = &lt->sortedwedges[i];
	w2 = &lt->sortedwedges[(i + 1) % lt->sortedwedges.Count()];
	w3 = &lt->sortedwedges[(i + 2) % lt->sortedwedges.Count()];

	if( w1->shape != localtrian_t::Wedge::eSquareLeft || w2->shape != localtrian_t::Wedge::eSquareRight )
		COM_FatalError( "CalcInterpolation_Square: internal error: not square.\n" );

	weights[0] = 0.0;
	weights[1] = 0.0;
	weights[2] = 0.0;
	weights[3] = 0.0;

	// find mid_near on (o,p3), mid_far on (p1,p2), spot on (mid_near,mid_far)
	CrossProduct( w1->leftdirection, lt->normal, normal1 );
	VectorNormalize( normal1 );
	CrossProduct( w2->wedgenormal, lt->normal, normal2 );
	VectorNormalize( normal2 );
	dot1 = DotProduct( spot, normal1 ) - 0;
	dot2 = DotProduct( spot, normal2 ) - DotProduct( w3->leftspot, normal2 );

	if( dot1 <= NORMAL_EPSILON )
	{
		frac = 0.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		frac = 1.0;
	}
	else
	{
		frac = dot1 / (dot1 + dot2);
		frac = bound( 0, frac, 1 );
	}

	dot1 = DotProduct( w3->leftspot, normal1 ) - 0;
	dot2 = 0 - DotProduct( w3->leftspot, normal2 );

	if( dot1 <= NORMAL_EPSILON )
	{
		frac_near = 1.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		frac_near = 0.0;
	}
	else
	{
		frac_near = (frac * dot2) / ((1.0 - frac) * dot1 + frac * dot2);
	}

	VectorScale( w3->leftspot, frac_near, mid_near );
	dot1 = DotProduct( w2->leftspot, normal1 ) - 0;
	dot2 = DotProduct( w1->leftspot, normal2 ) - DotProduct( w3->leftspot, normal2 );

	if( dot1 <= NORMAL_EPSILON )
	{
		frac_far = 1.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		frac_far = 0.0;
	}
	else
	{
		frac_far = (frac * dot2) / ((1 - frac) * dot1 + frac * dot2);
	}

	VectorScale( w1->leftspot, 1.0 - frac_far, mid_far );
	VectorMA( mid_far, frac_far, w2->leftspot, mid_far );
	CrossProduct( lt->normal, w3->leftdirection, normal );
	VectorNormalize( normal );
	dot = DotProduct( spot, normal ) - 0;
	dot1 = (1.0 - frac_far) * DotProduct( w1->leftspot, normal ) + frac_far * DotProduct( w2->leftspot, normal ) - 0;

	if( dot <= NORMAL_EPSILON )
	{
		ratio = 0.0;
	}
	else if( dot >= dot1 )
	{
		ratio = 1.0;
	}
	else
	{
		ratio = dot / dot1;
		ratio = bound( 0, ratio, 1 );
	}

	VectorScale( mid_near, 1.0 - ratio, test );
	VectorMA( test, ratio, mid_far, test );
	VectorSubtract( test, spot, test );

	if( g_drawlerp && VectorLength( test ) > 4 * ON_EPSILON )
		MsgDev( D_REPORT, "Debug: triangulation: internal error 12.\n" );

	weights[0] += 0.5 * (1.0 - ratio) * (1.0 - frac_near);
	weights[3] += 0.5 * (1.0 - ratio) * frac_near;
	weights[1] += 0.5 * ratio * (1.0 - frac_far);
	weights[2] += 0.5 * ratio * frac_far;
	
	// find mid_near on (o,p1), mid_far on (p2,p3), spot on (mid_near,mid_far)
	CrossProduct( lt->normal, w3->leftdirection, normal1 );
	VectorNormalize( normal1 );
	CrossProduct( w1->wedgenormal, lt->normal, normal2 );
	VectorNormalize( normal2 );
	dot1 = DotProduct( spot, normal1 ) - 0;
	dot2 = DotProduct( spot, normal2 ) - DotProduct( w1->leftspot, normal2 );

	if( dot1 <= NORMAL_EPSILON )
	{
		frac = 0.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		frac = 1.0;
	}
	else
	{
		frac = dot1 / (dot1 + dot2);
		frac = bound( 0, frac, 1 );
	}

	dot1 = DotProduct( w1->leftspot, normal1 ) - 0;
	dot2 = 0 - DotProduct( w1->leftspot, normal2 );

	if( dot1 <= NORMAL_EPSILON )
	{
		frac_near = 1.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		frac_near = 0.0;
	}
	else
	{
		frac_near = (frac * dot2) / ((1.0 - frac) * dot1 + frac * dot2);
	}

	VectorScale( w1->leftspot, frac_near, mid_near );
	dot1 = DotProduct( w2->leftspot, normal1 ) - 0;
	dot2 = DotProduct( w3->leftspot, normal2 ) - DotProduct( w1->leftspot, normal2 );

	if( dot1 <= NORMAL_EPSILON )
	{
		frac_far = 1.0;
	}
	else if( dot2 <= NORMAL_EPSILON )
	{
		frac_far = 0.0;
	}
	else
	{
		frac_far = (frac * dot2) / ((1.0 - frac) * dot1 + frac * dot2);
	}

	VectorScale( w3->leftspot, 1.0 - frac_far, mid_far );
	VectorMA( mid_far, frac_far, w2->leftspot, mid_far );
	CrossProduct( w1->leftdirection, lt->normal, normal );
	VectorNormalize( normal );
	dot = DotProduct( spot, normal ) - 0;
	dot1 = (1.0 - frac_far) * DotProduct( w3->leftspot, normal ) + frac_far * DotProduct( w2->leftspot, normal ) - 0;

	if( dot <= NORMAL_EPSILON )
	{
		ratio = 0.0;
	}
	else if( dot >= dot1 )
	{
		ratio = 1.0;
	}
	else
	{
		ratio = dot / dot1;
		ratio = bound( 0, ratio, 1 );
	}

	VectorScale( mid_near, 1.0 - ratio, test );
	VectorMA( test, ratio, mid_far, test );
	VectorSubtract( test, spot, test );

	if( g_drawlerp && VectorLength( test ) > 4 * ON_EPSILON )
		MsgDev( D_REPORT, "Debug: triangulation: internal error 13.\n" );

	weights[0] += 0.5 * (1 - ratio) * (1 - frac_near);
	weights[1] += 0.5 * (1 - ratio) * frac_near;
	weights[3] += 0.5 * ratio * (1 - frac_far);
	weights[2] += 0.5 * ratio * frac_far;

	interp->isbiased = false;
	interp->totalweight = 1.0;
	interp->points.SetCount( 4 );
	interp->points[0].patchnum = lt->patchnum;
	interp->points[0].weight = weights[0];
	interp->points[1].patchnum = w1->leftpatchnum;
	interp->points[1].weight = weights[1];
	interp->points[2].patchnum = w2->leftpatchnum;
	interp->points[2].weight = weights[2];
	interp->points[3].patchnum = w3->leftpatchnum;
	interp->points[3].weight = weights[3];
}

// The interpolation function is defined over the entire plane, so CalcInterpolation never fails.
static void CalcInterpolation( const localtrian_t *lt, const vec3_t spot, interpolation_t *interp )
{
	vec_t			frac, len, dist;
	vec_t			dot, dot1, dot2;
	vec_t			angle, ratio;
	const localtrian_t::Wedge	*w, *wnext;
	vec3_t			direction;
	bool			istoofar;
	CUtlArray< vec_t >		angles;
	int			i, j;

	if( GetDirection( spot, lt->normal, direction ) <= 2 * ON_EPSILON )
	{
		// spot happens to be at the center
		interp->isbiased = false;
		interp->totalweight = 1.0;
		interp->points.SetCount( 1 );
		interp->points[0].patchnum = lt->patchnum;
		interp->points[0].weight = 1.0;
		return;
	}

	if( lt->sortedwedges.Count() == 0 ) // this local triangulation only has center patch
	{
		interp->isbiased = true;
		interp->totalweight = 1.0;
		interp->points.SetCount( 1 );
		interp->points[0].patchnum = lt->patchnum;
		interp->points[0].weight = 1.0;
		return;
	}
	
	// Find the wedge with minimum non-negative angle (counterclockwise) pass the spot
	angles.SetCount( lt->sortedwedges.Count( ));

	for( i = 0; i < lt->sortedwedges.Count(); i++ )
	{
		angle = GetAngle( lt->sortedwedges[i].leftdirection, direction, lt->normal );
		angles[i] = GetAngleDiff( angle, 0 );
	}

	for( i = 1, j = 0; i < lt->sortedwedges.Count(); i++ )
	{
		if( angles[i] < angles[j] )
			j = i;
	}

	w = &lt->sortedwedges[j];
	wnext = &lt->sortedwedges[(j + 1) % lt->sortedwedges.Count()];

	// Different wedge types have different interpolation methods
	switch( w->shape )
	{
	case localtrian_t::Wedge::eSquareLeft:
	case localtrian_t::Wedge::eSquareRight:
	case localtrian_t::Wedge::eTriangular:
		// w->wedgenormal is undefined
		frac = GetFrac( w->leftspot, wnext->leftspot, direction, lt->normal );
		len = (1.0 - frac) * DotProduct( w->leftspot, direction ) + frac * DotProduct( wnext->leftspot, direction );
		dist = DotProduct( spot, direction );

		if( len <= ON_EPSILON / 4 || dist > len + 2 * ON_EPSILON )
		{
			istoofar = true;
			ratio = 1.0;
		}
		else if( dist >= len - ON_EPSILON )
		{
			istoofar = false;
			ratio = 1.0;
		}
		else
		{
			istoofar = false;
			ratio = dist / len;
			ratio = bound( 0, ratio, 1 );
		}

		if( istoofar )
		{
			interp->isbiased = true;
			interp->totalweight = 1.0;
			interp->points.SetCount( 2 );
			interp->points[0].patchnum = w->leftpatchnum;
			interp->points[0].weight = 1.0 - frac;
			interp->points[1].patchnum = wnext->leftpatchnum;
			interp->points[1].weight = frac;
		}
		else if( w->shape == localtrian_t::Wedge::eSquareLeft )
		{
			i = w - &lt->sortedwedges[0];
			CalcInterpolation_Square( lt, i, spot, interp );
		}
		else if( w->shape == localtrian_t::Wedge::eSquareRight )
		{
			i = w - &lt->sortedwedges[0];
			i = (i - 1 + lt->sortedwedges.Count()) % lt->sortedwedges.Count();
			CalcInterpolation_Square( lt, i, spot, interp );
		}
		else
		{
			interp->isbiased = false;
			interp->totalweight = 1.0;
			interp->points.SetCount( 3 );
			interp->points[0].patchnum = lt->patchnum;
			interp->points[0].weight = 1.0 - ratio;
			interp->points[1].patchnum = w->leftpatchnum;
			interp->points[1].weight = ratio * (1.0 - frac);
			interp->points[2].patchnum = wnext->leftpatchnum;
			interp->points[2].weight = ratio * frac;
		}
		break;
	case localtrian_t::Wedge::eConvex:
		// w->wedgenormal is the unit vector pointing from w->leftspot to wnext->leftspot
		dot1 = DotProduct( w->leftspot, w->wedgenormal ) - DotProduct( spot, w->wedgenormal );
		dot2 = DotProduct( wnext->leftspot, w->wedgenormal ) - DotProduct( spot, w->wedgenormal );
		dot = 0 - DotProduct( spot, w->wedgenormal );
		// for eConvex type: dot1 < dot < dot2

		if( g_drawlerp && ( dot1 > dot || dot > dot2 ))
		{
			MsgDev( D_REPORT, "Debug: triangulation: internal error 3.\n" );
		}

		if( dot1 >= -NORMAL_EPSILON ) // 0 <= dot1 < dot < dot2
		{
			interp->isbiased = true;
			interp->totalweight = 1.0;
			interp->points.SetCount( 1 );
			interp->points[0].patchnum = w->leftpatchnum;
			interp->points[0].weight = 1.0;
		}
		else if( dot2 <= NORMAL_EPSILON ) // dot1 < dot < dot2 <= 0
		{
			interp->isbiased = true;
			interp->totalweight = 1.0;
			interp->points.SetCount( 1 );
			interp->points[0].patchnum = wnext->leftpatchnum;
			interp->points[0].weight = 1.0;
		}
		else if( dot > 0 ) // dot1 < 0 < dot < dot2
		{
			frac = dot1 / (dot1 - dot);
			frac = bound( 0, frac, 1 );

			interp->isbiased = true;
			interp->totalweight = 1.0;
			interp->points.SetCount( 2 );
			interp->points[0].patchnum = w->leftpatchnum;
			interp->points[0].weight = 1 - frac;
			interp->points[1].patchnum = lt->patchnum;
			interp->points[1].weight = frac;
		}
		else // dot1 < dot <= 0 < dot2
		{
			frac = dot / (dot - dot2);
			frac = bound( 0, frac, 1 );
			
			interp->isbiased = true;
			interp->totalweight = 1.0;
			interp->points.SetCount( 2 );
			interp->points[0].patchnum = lt->patchnum;
			interp->points[0].weight = 1 - frac;
			interp->points[1].patchnum = wnext->leftpatchnum;
			interp->points[1].weight = frac;
		}
		break;
	case localtrian_t::Wedge::eConcave:
		if( DotProduct( spot, w->wedgenormal ) < 0 ) // the spot is closer to the left edge than the right edge
		{
			len = DotProduct( w->leftspot, w->leftdirection );
			dist = DotProduct( spot, w->leftdirection );

			if( g_drawlerp && len <= ON_EPSILON )
			{
				MsgDev( D_REPORT, "Debug: triangulation: internal error 4.\n" );
			}

			if( dist <= NORMAL_EPSILON )
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.SetCount( 1 );
				interp->points[0].patchnum = lt->patchnum;
				interp->points[0].weight = 1.0;
			}
			else if( dist >= len )
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.SetCount( 1 );
				interp->points[0].patchnum = w->leftpatchnum;
				interp->points[0].weight = 1.0;
			}
			else
			{
				ratio = dist / len;
				ratio = bound( 0, ratio, 1 );

				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.SetCount( 2 );
				interp->points[0].patchnum = lt->patchnum;
				interp->points[0].weight = 1.0 - ratio;
				interp->points[1].patchnum = w->leftpatchnum;
				interp->points[1].weight = ratio;
			}
		}
		else // the spot is closer to the right edge than the left edge
		{
			len = DotProduct( wnext->leftspot, wnext->leftdirection );
			dist = DotProduct( spot, wnext->leftdirection );

			if( g_drawlerp && len <= ON_EPSILON )
			{
				MsgDev( D_REPORT, "Debug: triangulation: internal error 5.\n" );
			}

			if( dist <= NORMAL_EPSILON )
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.SetCount( 1 );
				interp->points[0].patchnum = lt->patchnum;
				interp->points[0].weight = 1.0;
			}
			else if( dist >= len )
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.SetCount( 1 );
				interp->points[0].patchnum = wnext->leftpatchnum;
				interp->points[0].weight = 1.0;
			}
			else
			{
				ratio = dist / len;
				ratio = bound( 0, ratio, 1 );

				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.SetCount( 2 );
				interp->points[0].patchnum = lt->patchnum;
				interp->points[0].weight = 1 - ratio;
				interp->points[1].patchnum = wnext->leftpatchnum;
				interp->points[1].weight = ratio;
			}
		}
		break;
	default:
		COM_FatalError( "CalcInterpolation: internal error: invalid wedge type\n" );
		break;
	}
}

static void ApplyInterpolation( const interpolation_t *interp, int numstyles, const int *styles, vec3_t *outs, vec3_t *outs_dir = NULL )
{
	int	i, j;

	for( j = 0; j < numstyles; j++ )
	{
		if( outs_dir != NULL )
			VectorClear( outs_dir[j] );
		VectorClear( outs[j] );
	}

	if( interp->totalweight <= 0 )
		return;

	for( i = 0; i < interp->points.Count(); i++ )
	{
		vec_t	lerp = interp->points[i].weight / interp->totalweight;

		for( j = 0; j < numstyles; j++ )
		{
			const vec_t *b = GetTotalLight( &g_patches[interp->points[i].patchnum], styles[j] );
			VectorMA( outs[j], lerp, b, outs[j] );
			if( !outs_dir ) continue;
			const vec_t *d = GetTotalDirection( &g_patches[interp->points[i].patchnum], styles[j] );
			VectorMA( outs_dir[j], lerp, d, outs_dir[j] );
		}
	}
}

// =====================================================================================
//  InterpolateSampleLight
// =====================================================================================
void InterpolateSampleLight( const vec3_t position, int surface, int numstyles, const int *styles, vec3_t *outs, vec3_t *outs_dir )
{
	vec_t			dot, bestdist;
	vec_t			weight, dist;
	CUtlArray< vec_t >		localweights;
	CUtlArray< interpolation_t*>	localinterps;
	interpolation_t		*maininterp;
	interpolation_t		*interp;
	vec3_t			v, spot;
	const localtrian_t		*best;
	const facetriangulation_t	*ft1;
	const facetriangulation_t	*ft2;
	const localtrian_t		*lt;
	int			i, j, n;

	if( surface < 0 || surface >= g_numfaces )
		COM_FatalError( "InterpolateSampleLight: surface number out of range.\n" );

	ft1 = g_facetriangulations[surface];
	maininterp = new interpolation_t;
	maininterp->points.EnsureCount( 64 );

	// Calculate local interpolations and their weights
	localweights.Purge();
	localinterps.Purge();

	if( g_lerp_enabled )
	{
		for( i = 0; i < ft1->neighbors.Count(); i++ ) // for this face and each of its neighbors
		{
			ft2 = g_facetriangulations[ft1->neighbors[i]];

			for( j = 0; j < ft2->localtriangulations.Count(); j++ ) // for each patch on that face
			{
				lt = ft2->localtriangulations[j];

				if( !CalcAdaptedSpot( lt, position, surface, spot ))
				{
					if( g_drawlerp && ft2 == ft1 )
					{
						MsgDev( D_REPORT, "Debug: triangulation: internal error 6.\n" );
					}
					continue;
				}

				if( !CalcWeight( lt, spot, &weight ))
					continue;

				interp = new interpolation_t;
				interp->points.EnsureCount( 4 );

				CalcInterpolation( lt, spot, interp );

				localweights.AddToTail( weight );
				localinterps.AddToTail( interp );
			}
		}
	}
	
	// combine into one interpolation
	maininterp->isbiased = false;
	maininterp->totalweight = 0;
	maininterp->points.Purge();

	for( i = 0; i < localinterps.Count(); i++ )
	{
		if( localinterps[i]->isbiased )
		{
			maininterp->isbiased = true;
		}

		for( j = 0; j < localinterps[i]->points.Count(); j++ )
		{
			weight = localinterps[i]->points[j].weight * localweights[i];

			if( FBitSet( g_patches[localinterps[i]->points[j].patchnum].flags, PATCH_OUTSIDE ))
			{
				weight *= 0.01;
			}

			n = maininterp->points.AddToTail();
			maininterp->points[n].patchnum = localinterps[i]->points[j].patchnum;
			maininterp->points[n].weight = weight;
			maininterp->totalweight += weight;
		}
	}

	if( maininterp->totalweight > 0 )
	{
		ApplyInterpolation( maininterp, numstyles, styles, outs, outs_dir );

		if( g_drawlerp )
		{
			for( j = 0; j < numstyles; j++ )
			{
				// white or yellow
				outs[j][0] = 100;
				outs[j][1] = 100;
				outs[j][2] = (maininterp->isbiased ? 0 : 100);
			}
		}
	}
	else
	{
		// try again, don't multiply localweights[i] (which equals to 0)
		maininterp->isbiased = false;
		maininterp->totalweight = 0;
		maininterp->points.Purge();

		for( i = 0; i < localinterps.Count(); i++ )
		{
			if( localinterps[i]->isbiased )
			{
				maininterp->isbiased = true;
			}

			for( j = 0; j < localinterps[i]->points.Count(); j++ )
			{
				weight = localinterps[i]->points[j].weight;

				if( FBitSet( g_patches[localinterps[i]->points[j].patchnum].flags, PATCH_OUTSIDE ))
				{
					weight *= 0.01;
				}

				n = maininterp->points.AddToTail();
				maininterp->points[n].patchnum = localinterps[i]->points[j].patchnum;
				maininterp->points[n].weight = weight;
				maininterp->totalweight += weight;
			}
		}

		if( maininterp->totalweight > 0 )
		{
			ApplyInterpolation( maininterp, numstyles, styles, outs, outs_dir );

			if( g_drawlerp )
			{
				for( j = 0; j < numstyles; j++ )
				{
					// red
					outs[j][0] = 100;
					outs[j][1] = 0;
					outs[j][2] = (maininterp->isbiased ? 0 : 100);
				}
			}
		}
		else
		{
			// worst case, simply use the nearest patch
			best = NULL;

			for( i = 0; i < ft1->localtriangulations.Count(); i++ )
			{
				lt = ft1->localtriangulations[i];
				VectorCopy( position, v );
				WindingSnapPoint( lt->winding, lt->plane.normal, v );
				VectorSubtract( v, position, v );
				dist = VectorLength( v );

				if( best == NULL || dist < bestdist - ON_EPSILON )
				{
					best = lt;
					bestdist = dist;
				}
			}

			if( best )
			{
				lt = best;
				VectorSubtract( position, lt->center, spot );
				dot = DotProduct( spot, lt->normal );
				VectorMA( spot, -dot, lt->normal, spot );
				CalcInterpolation( lt, spot, maininterp );

				maininterp->totalweight = 0;

				for( j = 0; j < maininterp->points.Count(); j++ )
				{
					if( FBitSet( g_patches[maininterp->points[j].patchnum].flags, PATCH_OUTSIDE ))
					{
						maininterp->points[j].weight *= 0.01;
					}
					maininterp->totalweight += maininterp->points[j].weight;
				}

				ApplyInterpolation( maininterp, numstyles, styles, outs, outs_dir );

				if( g_drawlerp )
				{
					for( j = 0; j < numstyles; j++ )
					{
						// green
						outs[j][0] = 0;
						outs[j][1] = 100;
						outs[j][2] = (maininterp->isbiased ? 0 : 100);
					}
				}
			}
			else
			{
				maininterp->isbiased = true;
				maininterp->totalweight = 0;
				maininterp->points.Purge();
				ApplyInterpolation( maininterp, numstyles, styles, outs, outs_dir );

				if( g_drawlerp )
				{
					for( j = 0; j < numstyles; j++ )
					{
						// black
						outs[j][0] = 0;
						outs[j][1] = 0;
						outs[j][2] = 0;
					}
				}
			}
		}
	}

	delete maininterp;

	for( i = 0; i < localinterps.Count(); i++ )
		delete localinterps[i];

}

static bool TestLineSegmentIntersectWall( const facetriangulation_t *facetrian, const vec3_t p1, const vec3_t p2 )
{
	vec_t				frac, top, bottom;
	vec_t				dot, dot1, dot2;
	vec_t				front, back;
	const facetriangulation_t::Wall	*wall;

	for( int i = 0; i < facetrian->walls.Count(); i++ )
	{
		wall = &facetrian->walls[i];
		bottom = DotProduct( wall->points[0], wall->direction );
		top = DotProduct( wall->points[1], wall->direction );
		front = DotProduct( p1, wall->normal ) - DotProduct( wall->points[0], wall->normal );
		back = DotProduct( p2, wall->normal ) - DotProduct( wall->points[0], wall->normal );

		if( front > ON_EPSILON && back > ON_EPSILON || front < -ON_EPSILON && back < -ON_EPSILON )
			continue;

		dot1 = DotProduct( p1, wall->direction );
		dot2 = DotProduct( p2, wall->direction );

		if( fabs( front ) <= 2 * ON_EPSILON && fabs( back ) <= 2 * ON_EPSILON )
		{
			top = Q_min( top, Q_max( dot1, dot2 ));
			bottom = Q_max( bottom, Q_min( dot1, dot2 ));
		}
		else
		{
			frac = front / (front - back);
			frac = bound( 0, frac, 1 );
			dot = dot1 + frac * (dot2 - dot1);
			top = Q_min( top, dot );
			bottom = Q_max( bottom, dot );
		}

		if( top - bottom >= -ON_EPSILON )
			return true;
	}

	return false;
}

static bool TestFarPatch( const localtrian_t *lt, const vec3_t p2, const winding_t *p2winding )
{
	vec_t	size1, size2;
	vec_t	dist;
	vec3_t	v;
	int	i;

	for( i = 0, size1 = 0; i < lt->winding->numpoints; i++ )
	{
		VectorSubtract( lt->winding->p[i], lt->center, v );
		dist = VectorLength( v );

		if( dist > size1 )
			size1 = dist;
	}

	for( i = 0, size2 = 0; i < p2winding->numpoints; i++ )
	{
		VectorSubtract( p2winding->p[i], p2, v );
		dist = VectorLength( v );

		if( dist > size2 )
			size2 = dist;
	}

	VectorSubtract( p2, lt->center, v );
	dist = VectorLength( v );

	return dist > 1.4 * (size1 + size2);
}

// if one of the angles in a triangle exceeds this threshold, the most distant point will be removed or the triangle will break into convex-type wedge.
static void GatherPatches( localtrian_t *lt, const facetriangulation_t *facetrian )
{
	int				patchnum2;
	int				facenum2;
	const patch_t			*patch2;
	CUtlArray<localtrian_t::Wedge>	points;
	CUtlArray<pair_t>			angles;
	localtrian_t::Wedge			point;
	vec_t				angle;
	const dplane_t			*dp2;
	vec3_t				v;
	int				i;

	if ( g_nolerp )
	{
		lt->sortedwedges.Purge();
		return;
	}

	points.Purge();

	for( i = 0; i < lt->neighborfaces.Count(); i++ )
	{
		facenum2 = lt->neighborfaces[i];
		dp2 = GetPlaneFromFace( facenum2 );

		for( patch2 = g_face_patches[facenum2]; patch2 != NULL; patch2 = patch2->next )
		{
			patchnum2 = patch2 - g_patches;
			
			point.leftpatchnum = patchnum2;
			VectorMA( patch2->origin, -DEFAULT_HUNT_OFFSET, dp2->normal, v );

			// Do permission tests using the original position of the patch
			if( patchnum2 == lt->patchnum || PointInWindingEpsilon( lt->winding, lt->plane.normal, v ))
				continue;

			if( facenum2 != facetrian->facenum && TestLineSegmentIntersectWall( facetrian, lt->center, v ))
				continue;

			if( TestFarPatch( lt, v, patch2->winding ))
				continue;

			// Store the adapted position of the patch
			if( !CalcAdaptedSpot( lt, v, facenum2, point.leftspot ))
				continue;

			if( GetDirection( point.leftspot, lt->normal, point.leftdirection ) <= 2 * ON_EPSILON )
				continue;

			points.AddToTail( point );
		}
	}

	// Sort the patches into clockwise order
	angles.SetCount( points.Count( ));

	for( i = 0; i < points.Count(); i++ )
	{
		angle = GetAngle( points[0].leftdirection, points[i].leftdirection, lt->normal );

		if( i == 0 )
		{
			if( g_drawlerp && fabs( angle ) > NORMAL_EPSILON )
			{
				MsgDev( D_REPORT, "Debug: triangulation: internal error 7.\n" );
			}
			angle = 0.0;
		}

		angles[i].first = GetAngleDiff( angle, 0 );
		angles[i].second = i;
	}

	angles.Sort( LessThan );
	lt->sortedwedges.SetCount( points.Count( ));

	for( i = 0; i < points.Count(); i++ )
	{
		lt->sortedwedges[i] = points[angles[i].second];
	}
}

static void PurgePatches( localtrian_t *lt )
{
	CUtlArray< localtrian_t::Wedge >	points;
	int				i, cur;
	CUtlArray< int >			next;
	CUtlArray< int >			prev;
	CUtlArray< int >			valid;
	CUtlArray< pair_t >			dists;
	vec_t				angle;
	vec3_t				normal;
	vec3_t				v;

	points.Swap( lt->sortedwedges );
	lt->sortedwedges.Purge();

	next.SetCount( points.Count( ));
	prev.SetCount( points.Count( ));
	valid.SetCount( points.Count( ));
	dists.SetCount( points.Count( ));

	for( i = 0; i < points.Count(); i++ )
	{
		next[i] = (i + 1) % points.Count();
		prev[i] = (i - 1 + points.Count( )) % points.Count();
		valid[i] = 1;
		dists[i].first = DotProduct( points[i].leftspot, points[i].leftdirection );
		dists[i].second = i;
	}

	dists.Sort( LessThan );

	for( i = 0; i < points.Count(); i++ )
	{
		float	sangle, cangle;

		cur = dists[i].second;
		if( valid[cur] == 0 )
			continue;
		valid[cur] = 2; // mark current patch as final

		SinCos( TRIANGLE_SHAPE_THRESHOLD, &sangle, &cangle );
		CrossProduct( points[cur].leftdirection, lt->normal, normal );
		VectorNormalize( normal );
		VectorScale( normal, cangle, v );
		VectorMA( v, sangle, points[cur].leftdirection, v );

		while( next[cur] != cur && valid[next[cur]] != 2 )
		{
			angle = GetAngle( points[cur].leftdirection, points[next[cur]].leftdirection, lt->normal );
			if( fabs( angle ) <= DEG2RAD( 1.0 ) || GetAngleDiff( angle, 0 ) <= M_PI + NORMAL_EPSILON
				&& DotProduct( points[next[cur]].leftspot, v ) >= DotProduct( points[cur].leftspot, v ) - ON_EPSILON / 2 )
			{
				// remove next patch
				valid[next[cur]] = 0;
				next[cur] = next[next[cur]];
				prev[next[cur]] = cur;
				continue;
			}
			// the triangle is good
			break;
		}
		
		CrossProduct( lt->normal, points[cur].leftdirection, normal );
		VectorNormalize( normal );
		VectorScale( normal, cangle, v );
		VectorMA( v, sangle, points[cur].leftdirection, v );

		while( prev[cur] != cur && valid[prev[cur]] != 2 )
		{
			angle = GetAngle( points[prev[cur]].leftdirection, points[cur].leftdirection, lt->normal );
			if( fabs( angle ) <= DEG2RAD( 1.0 ) || GetAngleDiff( angle, 0 ) <= M_PI + NORMAL_EPSILON
				&& DotProduct (points[prev[cur]].leftspot, v) >= DotProduct (points[cur].leftspot, v ) - ON_EPSILON / 2 )
			{
				// remove previous patch
				valid[prev[cur]] = 0;
				prev[cur] = prev[prev[cur]];
				next[prev[cur]] = cur;
				continue;
			}
			// the triangle is good
			break;
		}
	}

	for( i = 0; i < points.Count(); i++ )
	{
		if( valid[i] == 2 )
		{
			lt->sortedwedges.AddToTail( points[i] );
		}
	}
}

static void PlaceHullPoints( localtrian_t *lt )
{
	int				i, j, n;
	vec_t				dot, angle;
	localtrian_t::HullPoint		hp;
	CUtlArray< localtrian_t::HullPoint >	spots;
	CUtlArray< pair_t >			angles;
	const localtrian_t::Wedge		*w;
	const localtrian_t::Wedge		*wnext;
	CUtlArray< localtrian_t::HullPoint >	arc_spots;
	CUtlArray< vec_t >			arc_angles;
	CUtlArray< int >			next;
	CUtlArray< int >			prev;
	vec_t				frac;
	vec_t				len;
	vec_t				dist;
	vec3_t				v;

//	spots.reserve( lt->winding->numpoints );
	spots.Purge();

	for( i = 0; i < lt->winding->numpoints; i++ )
	{
		VectorSubtract( lt->winding->p[i], lt->center, v );
		dot = DotProduct( v, lt->normal );
		VectorMA( v, -dot, lt->normal, hp.spot );

		if( !GetDirection( hp.spot, lt->normal, hp.direction ))
			continue;
		spots.AddToTail( hp );
	}

	if( lt->sortedwedges.Count() == 0 )
	{
		angles.SetCount( spots.Count( ));

		for( i = 0; i < spots.Count(); i++ )
		{
			angle = GetAngle( spots[0].direction, spots[i].direction, lt->normal );
			if( i == 0 ) angle = 0.0;
			angles[i].first = GetAngleDiff( angle, 0 );
			angles[i].second = i;
		}

		angles.Sort( LessThan );
		lt->sortedhullpoints.Purge();

		for( i = 0; i < spots.Count(); i++ )
		{
			if( g_drawlerp && angles[i].second != i )
				MsgDev( D_REPORT, "Debug: triangulation: internal error 8.\n");
			lt->sortedhullpoints.AddToTail( spots[angles[i].second] );
		}
		return;
	}

	lt->sortedhullpoints.Purge();

	for( i = 0; i < lt->sortedwedges.Count(); i++ )
	{
		w = &lt->sortedwedges[i];
		wnext = &lt->sortedwedges[(i + 1) % lt->sortedwedges.Count()];

		angles.SetCount( spots.Count( ));

		for( j = 0; j < spots.Count(); j++ )
		{
			angle = GetAngle( w->leftdirection, spots[j].direction, lt->normal );
			angles[j].first = GetAngleDiff( angle, 0 );
			angles[j].second = j;
		}

		angles.Sort( LessThan );
		angle = GetAngle( w->leftdirection, wnext->leftdirection, lt->normal );

		if( lt->sortedwedges.Count() == 1 )
		{
			angle = M_PI2;
		}
		else
		{
			angle = GetAngleDiff( angle, 0 );
		}

		arc_spots.SetCount( spots.Count() + 2 );
		arc_angles.SetCount( spots.Count() + 2 );
		next.SetCount( spots.Count() + 2 );
		prev.SetCount( spots.Count() + 2 );

		VectorCopy( w->leftspot, arc_spots[0].spot );
		VectorCopy( w->leftdirection, arc_spots[0].direction );
		arc_angles[0] = 0;
		next[0] = 1;
		prev[0] = -1;
		n = 1;

		for( j = 0; j < spots.Count(); j++ )
		{
			if( NORMAL_EPSILON <= angles[j].first && angles[j].first <= angle - NORMAL_EPSILON )
			{
				arc_spots[n] = spots[angles[j].second];
				arc_angles[n] = angles[j].first;
				next[n] = n + 1;
				prev[n] = n - 1;
				n++;
			}
		}

		VectorCopy( wnext->leftspot, arc_spots[n].spot );
		VectorCopy( wnext->leftdirection, arc_spots[n].direction );
		arc_angles[n] = angle;
		next[n] = -1;
		prev[n] = n - 1;
		n++;

		for( j = 1; next[j] != -1; j = next[j] )
		{
			while( prev[j] != -1 )
			{
				if( arc_angles[next[j]] - arc_angles[prev[j]] <= M_PI + NORMAL_EPSILON )
				{
					frac = GetFrac( arc_spots[prev[j]].spot, arc_spots[next[j]].spot, arc_spots[j].direction, lt->normal);
					len = ( 1.0 - frac ) * DotProduct( arc_spots[prev[j]].spot, arc_spots[j].direction )
						+ frac * DotProduct( arc_spots[next[j]].spot, arc_spots[j].direction );
					dist = DotProduct( arc_spots[j].spot, arc_spots[j].direction );

					if( dist <= len + NORMAL_EPSILON )
					{
						j = prev[j];
						next[j] = next[next[j]];
						prev[next[j]] = j;
						continue;
					}
				}
				break;
			}
		}

		for( j = 0; next[j] != -1; j = next[j] )
		{
			lt->sortedhullpoints.AddToTail( arc_spots[j] );
		}
	}
}

static bool TryMakeSquare( localtrian_t *lt, int i )
{
	localtrian_t::Wedge	*w1;
	localtrian_t::Wedge	*w2;
	localtrian_t::Wedge	*w3;
	vec3_t			dir1;
	vec3_t			dir2;
	vec_t			angle;
	vec3_t			v;

	w1 = &lt->sortedwedges[i];
	w2 = &lt->sortedwedges[(i + 1) % lt->sortedwedges.Count()];
	w3 = &lt->sortedwedges[(i + 2) % lt->sortedwedges.Count()];

	// (o, p1, p2) and (o, p2, p3) must be triangles and not in a square
	if( w1->shape != localtrian_t::Wedge::eTriangular || w2->shape != localtrian_t::Wedge::eTriangular )
		return false;

	// (o, p1, p3) must be a triangle
	angle = GetAngle( w1->leftdirection, w3->leftdirection, lt->normal );
	angle = GetAngleDiff( angle, 0 );

	if( angle >= TRIANGLE_SHAPE_THRESHOLD )
		return false;

	// (p2, p1, p3) must be a triangle
	VectorSubtract( w1->leftspot, w2->leftspot, v );
	if( !GetDirection( v, lt->normal, dir1 ))
		return false;

	VectorSubtract( w3->leftspot, w2->leftspot, v );
	if( !GetDirection( v, lt->normal, dir2 ))
		return false;

	angle = GetAngle( dir2, dir1, lt->normal );
	angle = GetAngleDiff( angle, 0 );
	if( angle >= TRIANGLE_SHAPE_THRESHOLD )
		return false;

	w1->shape = localtrian_t::Wedge::eSquareLeft;
	VectorNegate( dir1, w1->wedgenormal );
	w2->shape = localtrian_t::Wedge::eSquareRight;
	VectorCopy( dir2, w2->wedgenormal );

	return true;
}

static void FindSquares( localtrian_t *lt )
{
	CUtlArray< pair_t >	dists;
	localtrian_t::Wedge	*w;
	int		i;

	if(lt->sortedwedges.Count() <= 2 )
		return;

	dists.SetCount( lt->sortedwedges.Count( ));

	for( i = 0; i < lt->sortedwedges.Count(); i++ )
	{
		w = &lt->sortedwedges[i];
		dists[i].first = DotProduct( w->leftspot, w->leftdirection );
		dists[i].second = i;
	}

	dists.Sort( LessThan );

	for( i = 0; i < lt->sortedwedges.Count(); i++ )
	{
		TryMakeSquare( lt, dists[i].second );
		TryMakeSquare( lt, (dists[i].second - 2 + lt->sortedwedges.Count( )) % lt->sortedwedges.Count( ));
	}
}

static localtrian_t *CreateLocalTriangulation( const facetriangulation_t *facetrian, int patchnum )
{
	localtrian_t	*lt;
	const patch_t		*patch;
	int			facenum;
	localtrian_t::Wedge	*w;
	localtrian_t::Wedge	*wnext;
	vec_t			angle;
	vec_t			total;
	vec3_t			normal;
	vec_t			dot;
	vec3_t			v;

	facenum = facetrian->facenum;
	patch = &g_patches[patchnum];
	lt = new localtrian_t;

	// Fill basic information for this local triangulation
	lt->plane = *GetPlaneFromFace( facenum );
	lt->plane.dist += DotProduct( g_face_offset[facenum], lt->plane.normal );
	lt->winding = patch->winding;
	VectorMA( patch->origin, -DEFAULT_HUNT_OFFSET, lt->plane.normal, lt->center );
	dot = DotProduct (lt->center, lt->plane.normal) - lt->plane.dist;
	VectorMA( lt->center, -dot, lt->plane.normal, lt->center );

	if (!PointInWindingEpsilon(lt->winding, lt->plane.normal, lt->center, ON_EPSILON, false))
	{
		WindingSnapPointEpsilon(lt->winding, lt->plane.normal, lt->center, DEFAULT_EDGE_WIDTH, 4 * DEFAULT_EDGE_WIDTH);
	}

	VectorCopy( lt->plane.normal, lt->normal );
	lt->patchnum = patchnum;
	lt->neighborfaces = facetrian->neighbors;

	// Gather all patches from nearby faces
	GatherPatches( lt, facetrian );
	
	// Remove distant patches
	PurgePatches( lt );

	// Calculate wedge types
	total = 0.0;
	for( int i = 0; i < lt->sortedwedges.Count(); i++ )
	{
		w = &lt->sortedwedges[i];
		wnext = &lt->sortedwedges[(i + 1) % lt->sortedwedges.Count()];

		angle = GetAngle (w->leftdirection, wnext->leftdirection, lt->normal );
		if( g_drawlerp && (lt->sortedwedges.Count() >= 2 && fabs( angle ) <= DEG2RAD( 0.9 )))
		{
			MsgDev( D_REPORT, "Debug: triangulation: internal error 9.\n");
		}
		angle = GetAngleDiff( angle, 0 );
		if(lt->sortedwedges.Count() == 1 )
		{
			angle = M_PI2;
		}
		total += angle;

		if( angle <= M_PI + NORMAL_EPSILON )
		{
			if( angle < TRIANGLE_SHAPE_THRESHOLD )
			{
				w->shape = localtrian_t::Wedge::eTriangular;
				VectorClear( w->wedgenormal );
			}
			else
			{
				w->shape = localtrian_t::Wedge::eConvex;
				VectorSubtract( wnext->leftspot, w->leftspot, v );
				GetDirection( v, lt->normal, w->wedgenormal );
			}
		}
		else
		{
			w->shape = localtrian_t::Wedge::eConcave;
			VectorAdd( wnext->leftdirection, w->leftdirection, v );
			CrossProduct( lt->normal, v, normal );
			VectorSubtract( wnext->leftdirection, w->leftdirection, v );
			VectorAdd( normal, v, normal );
			GetDirection( normal, lt->normal, w->wedgenormal );
			if( g_drawlerp && VectorLength( w->wedgenormal ) == 0 )
			{
				MsgDev( D_REPORT, "Debug: triangulation: internal error 10.\n");
			}
		}
	}

	if( g_drawlerp && (lt->sortedwedges.Count() > 0 && fabs( total - M_PI2 ) > 10 * NORMAL_EPSILON ))
	{
		MsgDev( D_REPORT, "Debug: triangulation: internal error 11.\n" );
	}

	FindSquares( lt );

	// Calculate hull points
	PlaceHullPoints( lt );

	return lt;
}

static void FindNeighbors( facetriangulation_t *facetrian )
{
	int		i, j, e;
	int		facenum1;
	int		facenum2;
	const dplane_t	*dp1;
	const dplane_t	*dp2;
	const edgeshare_t	*es;
	const dface_t	*f1;
	const dface_t	*f2;

	facenum1 = facetrian->facenum;
	f1 = &g_dfaces[facenum1];
	dp1 = GetPlaneFromFace( f1 );

	facetrian->neighbors.Purge();
	facetrian->neighbors.AddToTail( facenum1 );

	for( i = 0; i < f1->numedges; i++ )
	{
		e = g_dsurfedges[f1->firstedge + i];
		es = &g_edgeshare[abs( e )];
		if( !es->smooth ) continue;

		f2 = es->faces[e > 0 ? 1 : 0];
		facenum2 = f2 - g_dfaces;
		dp2 = GetPlaneFromFace( f2 );

		if( DotProduct( dp1->normal, dp2->normal ) < -NORMAL_EPSILON )
			continue;

		for( j = 0; j < facetrian->neighbors.Count(); j++ )
		{
			if( facetrian->neighbors[j] == facenum2 )
				break;
		}

		if( j == facetrian->neighbors.Count( ))
		{
			facetrian->neighbors.AddToTail( facenum2 );
		}
	}
}

static void BuildWalls( facetriangulation_t *facetrian )
{
	int		i, j, e;
	int		facenum;
	int		facenum2;
	const dface_t	*f;
	const dface_t	*f2;
	const dplane_t	*dp;
	const dplane_t	*dp2;
	const edgeshare_t	*es;
	vec_t		dot;

	facenum = facetrian->facenum;
	f = &g_dfaces[facenum];
	dp = GetPlaneFromFace( f );

	facetrian->walls.Purge();

	for( i = 0; i < facetrian->neighbors.Count(); i++ )
	{
		facenum2 = facetrian->neighbors[i];
		f2 = &g_dfaces[facenum2];
		dp2 = GetPlaneFromFace( f2 );

		if( DotProduct( dp->normal, dp2->normal ) <= 0.1 )
			continue;

		for( j = 0; j < f2->numedges; j++ )
		{
			e = g_dsurfedges[f2->firstedge + j];
			es = &g_edgeshare[abs(e)];

			if( !es->smooth )
			{
				facetriangulation_t::Wall	wall;

				VectorAdd( g_dvertexes[g_dedges[abs(e)].v[0]].point, g_face_offset[facenum], wall.points[0] );
				VectorAdd( g_dvertexes[g_dedges[abs(e)].v[1]].point, g_face_offset[facenum], wall.points[1] );
				VectorSubtract( wall.points[1], wall.points[0], wall.direction );
				dot = DotProduct( wall.direction, dp->normal );
				VectorMA( wall.direction, -dot, dp->normal, wall.direction );

				if( VectorNormalize( wall.direction ))
				{
					CrossProduct( wall.direction, dp->normal, wall.normal );
					VectorNormalize( wall.normal );
					facetrian->walls.AddToTail( wall );
				}
			}
		}
	}
}

static void CollectUsedPatches( facetriangulation_t *facetrian )
{
	int				i, j, k;
	int				patchnum;
	const localtrian_t		*lt;
	const localtrian_t::Wedge	*w;

	facetrian->usedpatches.Purge();

	for( i = 0; i < facetrian->localtriangulations.Count(); i++ )
	{
		lt = facetrian->localtriangulations[i];
		patchnum = lt->patchnum;

		for( k = 0; k < facetrian->usedpatches.Count(); k++ )
		{
			if( facetrian->usedpatches[k] == patchnum )
				break;
		}

		if( k == facetrian->usedpatches.Count( ))
			facetrian->usedpatches.AddToTail( patchnum );

		for( j = 0; j < lt->sortedwedges.Count(); j++ )
		{
			w = &lt->sortedwedges[j];
			patchnum = w->leftpatchnum;

			for( k = 0; k < facetrian->usedpatches.Count(); k++ )
			{
				if( facetrian->usedpatches[k] == patchnum )
					break;
			}

			if( k == facetrian->usedpatches.Count( ))
				facetrian->usedpatches.AddToTail( patchnum );
		}
	}
}

// =====================================================================================
//  CreateTriangulations
// =====================================================================================
void CreateTriangulations( int facenum, int threadnum )
{
	facetriangulation_t		*facetrian;
	int			patchnum;
	const patch_t		*patch;
	localtrian_t	*lt;

	g_facetriangulations[facenum] = new facetriangulation_t;
	facetrian = g_facetriangulations[facenum];

	facetrian->facenum = facenum;

	// find neighbors
	FindNeighbors( facetrian );

	// Build walls
	BuildWalls( facetrian );

	// Create local triangulation around each patch
	facetrian->localtriangulations.Purge();

	for( patch = g_face_patches[facenum]; patch; patch = patch->next )
	{
		patchnum = patch - g_patches;
		lt = CreateLocalTriangulation( facetrian, patchnum );
		facetrian->localtriangulations.AddToTail( lt );
	}

	// Collect used patches
	CollectUsedPatches( facetrian );
}

// =====================================================================================
//  GetTriangulationPatches
// =====================================================================================
void GetTriangulationPatches( int facenum, int *numpatches, const int **patches )
{
	const facetriangulation_t	*facetrian;

	if( !g_usingpatches )
	{
		*numpatches = 0;
		*patches = NULL;
	}
	else
	{
		facetrian = g_facetriangulations[facenum];
		*numpatches = facetrian->usedpatches.Count();
		*patches = facetrian->usedpatches.Base();
	}
}

// =====================================================================================
//  FreeTriangulations
// =====================================================================================
void FreeTriangulations( void )
{
	facetriangulation_t	*facetrian;

	for( int i = 0; i < g_numfaces; i++ )
	{
		facetrian = g_facetriangulations[i];

		for( int j = 0; j < facetrian->localtriangulations.Count(); j++ )
		{
			delete facetrian->localtriangulations[j];
		}

		g_facetriangulations[i] = NULL;
		delete facetrian;
	}
}
