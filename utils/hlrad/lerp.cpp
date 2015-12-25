#include "qrad.h"
#ifdef HLRAD_LOCALTRIANGULATION
#include <vector>
#include <algorithm>
#endif

int             g_lerp_enabled = DEFAULT_LERP_ENABLED;
#ifdef HLRAD_LOCALTRIANGULATION

struct interpolation_t
{
	struct Point
	{
		int patchnum;
		vec_t weight;
	};
	
	bool isbiased;
	vec_t totalweight;
	std::vector< Point > points;
};

struct localtriangulation_t
{
	struct Wedge
	{
		enum eShape
		{
			eTriangular,
			eConvex,
			eConcave,
#ifdef HLRAD_BILINEARINTERPOLATION
			eSquareLeft,
			eSquareRight,
#endif
		};
		
		eShape shape;
		int leftpatchnum;
		vec3_t leftspot;
		vec3_t leftdirection;
		// right side equals to the left side of the next wedge

		vec3_t wedgenormal; // for custom usage
	};
	struct HullPoint
	{
		vec3_t spot;
		vec3_t direction;
	};
	
	dplane_t plane;
	Winding winding;
	vec3_t center; // center is on the plane

	vec3_t normal;
	int patchnum;
	std::vector< int > neighborfaces; // including the face itself

	std::vector< Wedge > sortedwedges; // in clockwise order (same as Winding)
	std::vector< HullPoint > sortedhullpoints; // in clockwise order (same as Winding)
};

struct facetriangulation_t
{
	struct Wall
	{
		vec3_t points[2];
		vec3_t direction;
		vec3_t normal;
	};

	int facenum;
	std::vector< int > neighbors; // including the face itself
	std::vector< Wall > walls;
	std::vector< localtriangulation_t * > localtriangulations;
	std::vector< int > usedpatches;
};

facetriangulation_t *g_facetriangulations[MAX_MAP_FACES];

static bool CalcAdaptedSpot (const localtriangulation_t *lt, const vec3_t position, int surface, vec3_t spot)
	// If the surface formed by the face and its neighbor faces is not flat, the surface should be unfolded onto the face plane
	// CalcAdaptedSpot calculates the coordinate of the unfolded spot on the face plane from the original position on the surface
	// CalcAdaptedSpot(center) = {0,0,0}
	// CalcAdaptedSpot(position on the face plane) = position - center
	// Param position: must include g_face_offset
{
	int i;
	vec_t dot;
	vec3_t surfacespot;
	vec_t dist;
	vec_t dist2;
	vec3_t phongnormal;
	vec_t frac;
	vec3_t middle;
	vec3_t v;
	
	for (i = 0; i < (int)lt->neighborfaces.size (); i++)
	{
		if (lt->neighborfaces[i] == surface)
		{
			break;
		}
	}
	if (i == (int)lt->neighborfaces.size ())
	{
		VectorClear (spot);
		return false;
	}

	VectorSubtract (position, lt->center, surfacespot);
	dot = DotProduct (surfacespot, lt->normal);
	VectorMA (surfacespot, -dot, lt->normal, spot);

	// use phong normal instead of face normal, because phong normal is a continuous function
	GetPhongNormal (surface, position, phongnormal);
	dot = DotProduct (spot, phongnormal);
	if (fabs (dot) > ON_EPSILON)
	{
		frac = DotProduct (surfacespot, phongnormal) / dot;
		frac = qmax (0, qmin (frac, 1)); // to correct some extreme cases
	}
	else
	{
		frac = 0;
	}
	VectorScale (spot, frac, middle);

	dist = VectorLength (spot);
	VectorSubtract (surfacespot, middle, v);
	dist2 = VectorLength (middle) + VectorLength (v);

	if (dist > ON_EPSILON && fabs (dist2 - dist) > ON_EPSILON)
	{
		VectorScale (spot, dist2 / dist, spot);
	}
	return true;
}

static vec_t GetAngle (const vec3_t leftdirection, const vec3_t rightdirection, const vec3_t normal)
{
	vec_t angle;
	vec3_t v;
	
	CrossProduct (rightdirection, leftdirection, v);
	angle = atan2 (DotProduct (v, normal), DotProduct (rightdirection, leftdirection));

	return angle;
}

static vec_t GetAngleDiff (vec_t angle, vec_t base)
{
	vec_t diff;

	diff = angle - base;
	if (diff < 0)
	{
		diff += 2 * Q_PI;
	}
	return diff;
}

static vec_t GetFrac (const vec3_t leftspot, const vec3_t rightspot, const vec3_t direction, const vec3_t normal)
{
	vec3_t v;
	vec_t dot1;
	vec_t dot2;
	vec_t frac;

	CrossProduct (direction, normal, v);
	dot1 = DotProduct (leftspot, v);
	dot2 = DotProduct (rightspot, v);
	
	// dot1 <= 0 < dot2
	if (dot1 >= -NORMAL_EPSILON)
	{
		if (g_drawlerp && dot1 > ON_EPSILON)
		{
			Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 1.\n");
		}
		frac = 0.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		if (g_drawlerp && dot2 < -ON_EPSILON)
		{
			Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 2.\n");
		}
		frac = 1.0;
	}
	else
	{
		frac = dot1 / (dot1 - dot2);
		frac = qmax (0, qmin (frac, 1));
	}

	return frac;
}

static vec_t GetDirection (const vec3_t spot, const vec3_t normal, vec3_t direction_out)
{
	vec_t dot;

	dot = DotProduct (spot, normal);
	VectorMA (spot, -dot, normal, direction_out);
	return VectorNormalize (direction_out);
}

static bool CalcWeight (const localtriangulation_t *lt, const vec3_t spot, vec_t *weight_out)
	// It returns true when the point is inside the hull region (with boundary), even if weight = 0.
{
	vec3_t direction;
	const localtriangulation_t::HullPoint *hp1;
	const localtriangulation_t::HullPoint *hp2;
	bool istoofar;
	vec_t ratio;

	int i;
	int j;
	vec_t angle;
	std::vector< vec_t > angles;
	vec_t frac;
	vec_t len;
	vec_t dist;

	if (GetDirection (spot, lt->normal, direction) <= 2 * ON_EPSILON)
	{
		*weight_out = 1.0;
		return true;
	}

	if ((int)lt->sortedhullpoints.size () == 0)
	{
		*weight_out = 0.0;
		return false;
	}

	angles.resize ((int)lt->sortedhullpoints.size ());
	for (i = 0; i < (int)lt->sortedhullpoints.size (); i++)
	{
		angle = GetAngle (lt->sortedhullpoints[i].direction, direction, lt->normal);
		angles[i] = GetAngleDiff (angle, 0);
	}
	j = 0;
	for (i = 1; i < (int)lt->sortedhullpoints.size (); i++)
	{
		if (angles[i] < angles[j])
		{
			j = i;
		}
	}
	hp1 = &lt->sortedhullpoints[j];
	hp2 = &lt->sortedhullpoints[(j + 1) % (int)lt->sortedhullpoints.size ()];

	frac = GetFrac (hp1->spot, hp2->spot, direction, lt->normal);
	
	len = (1 - frac) * DotProduct (hp1->spot, direction) + frac * DotProduct (hp2->spot, direction);
	dist = DotProduct (spot, direction);
	if (len <= ON_EPSILON / 4 || dist > len + 2 * ON_EPSILON)
	{
		istoofar = true;
		ratio = 1.0;
	}
	else if (dist >= len - ON_EPSILON)
	{
		istoofar = false; // if we change this "false" to "true", we will see many places turned "green" in "-drawlerp" mode
		ratio = 1.0; // in order to prevent excessively small weight
	}
	else
	{
		istoofar = false;
		ratio = dist / len;
		ratio = qmax (0, qmin (ratio, 1));
	}

	*weight_out = 1 - ratio;
	return !istoofar;
}

#ifdef HLRAD_BILINEARINTERPOLATION
static void CalcInterpolation_Square (const localtriangulation_t *lt, int i, const vec3_t spot, interpolation_t *interp)
{
	const localtriangulation_t::Wedge *w1;
	const localtriangulation_t::Wedge *w2;
	const localtriangulation_t::Wedge *w3;
	vec_t weights[4];
	vec_t dot1;
	vec_t dot2;
	vec_t dot;
	vec3_t normal1;
	vec3_t normal2;
	vec3_t normal;
	vec_t frac;
	vec_t frac_near;
	vec_t frac_far;
	vec_t ratio;
	vec3_t mid_far;
	vec3_t mid_near;
	vec3_t test;
	
	w1 = &lt->sortedwedges[i];
	w2 = &lt->sortedwedges[(i + 1) % (int)lt->sortedwedges.size ()];
	w3 = &lt->sortedwedges[(i + 2) % (int)lt->sortedwedges.size ()];
	if (w1->shape != localtriangulation_t::Wedge::eSquareLeft || w2->shape != localtriangulation_t::Wedge::eSquareRight)
	{
		Error ("CalcInterpolation_Square: internal error: not square.");
	}

	weights[0] = 0.0;
	weights[1] = 0.0;
	weights[2] = 0.0;
	weights[3] = 0.0;

	// find mid_near on (o,p3), mid_far on (p1,p2), spot on (mid_near,mid_far)
	CrossProduct (w1->leftdirection, lt->normal, normal1);
	VectorNormalize (normal1);
	CrossProduct (w2->wedgenormal, lt->normal, normal2);
	VectorNormalize (normal2);
	dot1 = DotProduct (spot, normal1) - 0;
	dot2 = DotProduct (spot, normal2) - DotProduct (w3->leftspot, normal2);
	if (dot1 <= NORMAL_EPSILON)
	{
		frac = 0.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		frac = 1.0;
	}
	else
	{
		frac = dot1 / (dot1 + dot2);
		frac = qmax (0, qmin (frac, 1));
	}

	dot1 = DotProduct (w3->leftspot, normal1) - 0;
	dot2 = 0 - DotProduct (w3->leftspot, normal2);
	if (dot1 <= NORMAL_EPSILON)
	{
		frac_near = 1.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		frac_near = 0.0;
	}
	else
	{
		frac_near = (frac * dot2) / ((1 - frac) * dot1 + frac * dot2);
	}
	VectorScale (w3->leftspot, frac_near, mid_near);

	dot1 = DotProduct (w2->leftspot, normal1) - 0;
	dot2 = DotProduct (w1->leftspot, normal2) - DotProduct (w3->leftspot, normal2);
	if (dot1 <= NORMAL_EPSILON)
	{
		frac_far = 1.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		frac_far = 0.0;
	}
	else
	{
		frac_far = (frac * dot2) / ((1 - frac) * dot1 + frac * dot2);
	}
	VectorScale (w1->leftspot, 1 - frac_far, mid_far);
	VectorMA (mid_far, frac_far, w2->leftspot, mid_far);

	CrossProduct (lt->normal, w3->leftdirection, normal);
	VectorNormalize (normal);
	dot = DotProduct (spot, normal) - 0;
	dot1 = (1 - frac_far) * DotProduct (w1->leftspot, normal) + frac_far * DotProduct (w2->leftspot, normal) - 0;
	if (dot <= NORMAL_EPSILON)
	{
		ratio = 0.0;
	}
	else if (dot >= dot1)
	{
		ratio = 1.0;
	}
	else
	{
		ratio = dot / dot1;
		ratio = qmax (0, qmin (ratio, 1));
	}

	VectorScale (mid_near, 1 - ratio, test);
	VectorMA (test, ratio, mid_far, test);
	VectorSubtract (test, spot, test);
	if (g_drawlerp && VectorLength (test) > 4 * ON_EPSILON)
	{
		Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 12.\n");
	}

	weights[0] += 0.5 * (1 - ratio) * (1 - frac_near);
	weights[3] += 0.5 * (1 - ratio) * frac_near;
	weights[1] += 0.5 * ratio * (1 - frac_far);
	weights[2] += 0.5 * ratio * frac_far;
	
	// find mid_near on (o,p1), mid_far on (p2,p3), spot on (mid_near,mid_far)
	CrossProduct (lt->normal, w3->leftdirection, normal1);
	VectorNormalize (normal1);
	CrossProduct (w1->wedgenormal, lt->normal, normal2);
	VectorNormalize (normal2);
	dot1 = DotProduct (spot, normal1) - 0;
	dot2 = DotProduct (spot, normal2) - DotProduct (w1->leftspot, normal2);
	if (dot1 <= NORMAL_EPSILON)
	{
		frac = 0.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		frac = 1.0;
	}
	else
	{
		frac = dot1 / (dot1 + dot2);
		frac = qmax (0, qmin (frac, 1));
	}

	dot1 = DotProduct (w1->leftspot, normal1) - 0;
	dot2 = 0 - DotProduct (w1->leftspot, normal2);
	if (dot1 <= NORMAL_EPSILON)
	{
		frac_near = 1.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		frac_near = 0.0;
	}
	else
	{
		frac_near = (frac * dot2) / ((1 - frac) * dot1 + frac * dot2);
	}
	VectorScale (w1->leftspot, frac_near, mid_near);

	dot1 = DotProduct (w2->leftspot, normal1) - 0;
	dot2 = DotProduct (w3->leftspot, normal2) - DotProduct (w1->leftspot, normal2);
	if (dot1 <= NORMAL_EPSILON)
	{
		frac_far = 1.0;
	}
	else if (dot2 <= NORMAL_EPSILON)
	{
		frac_far = 0.0;
	}
	else
	{
		frac_far = (frac * dot2) / ((1 - frac) * dot1 + frac * dot2);
	}
	VectorScale (w3->leftspot, 1 - frac_far, mid_far);
	VectorMA (mid_far, frac_far, w2->leftspot, mid_far);

	CrossProduct (w1->leftdirection, lt->normal, normal);
	VectorNormalize (normal);
	dot = DotProduct (spot, normal) - 0;
	dot1 = (1 - frac_far) * DotProduct (w3->leftspot, normal) + frac_far * DotProduct (w2->leftspot, normal) - 0;
	if (dot <= NORMAL_EPSILON)
	{
		ratio = 0.0;
	}
	else if (dot >= dot1)
	{
		ratio = 1.0;
	}
	else
	{
		ratio = dot / dot1;
		ratio = qmax (0, qmin (ratio, 1));
	}

	VectorScale (mid_near, 1 - ratio, test);
	VectorMA (test, ratio, mid_far, test);
	VectorSubtract (test, spot, test);
	if (g_drawlerp && VectorLength (test) > 4 * ON_EPSILON)
	{
		Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 13.\n");
	}

	weights[0] += 0.5 * (1 - ratio) * (1 - frac_near);
	weights[1] += 0.5 * (1 - ratio) * frac_near;
	weights[3] += 0.5 * ratio * (1 - frac_far);
	weights[2] += 0.5 * ratio * frac_far;

	interp->isbiased = false;
	interp->totalweight = 1.0;
	interp->points.resize (4);
	interp->points[0].patchnum = lt->patchnum;
	interp->points[0].weight = weights[0];
	interp->points[1].patchnum = w1->leftpatchnum;
	interp->points[1].weight = weights[1];
	interp->points[2].patchnum = w2->leftpatchnum;
	interp->points[2].weight = weights[2];
	interp->points[3].patchnum = w3->leftpatchnum;
	interp->points[3].weight = weights[3];
}

#endif
static void CalcInterpolation (const localtriangulation_t *lt, const vec3_t spot, interpolation_t *interp)
	// The interpolation function is defined over the entire plane, so CalcInterpolation never fails.
{
	vec3_t direction;
	const localtriangulation_t::Wedge *w;
	const localtriangulation_t::Wedge *wnext;

	int i;
	int j;
	vec_t angle;
	std::vector< vec_t > angles;

	if (GetDirection (spot, lt->normal, direction) <= 2 * ON_EPSILON)
	{
		// spot happens to be at the center
		interp->isbiased = false;
		interp->totalweight = 1.0;
		interp->points.resize (1);
		interp->points[0].patchnum = lt->patchnum;
		interp->points[0].weight = 1.0;
		return;
	}

	if ((int)lt->sortedwedges.size () == 0) // this local triangulation only has center patch
	{
		interp->isbiased = true;
		interp->totalweight = 1.0;
		interp->points.resize (1);
		interp->points[0].patchnum = lt->patchnum;
		interp->points[0].weight = 1.0;
		return;
	}
	
	// Find the wedge with minimum non-negative angle (counterclockwise) pass the spot
	angles.resize ((int)lt->sortedwedges.size ());
	for (i = 0; i < (int)lt->sortedwedges.size (); i++)
	{
		angle = GetAngle (lt->sortedwedges[i].leftdirection, direction, lt->normal);
		angles[i] = GetAngleDiff (angle, 0);
	}
	j = 0;
	for (i = 1; i < (int)lt->sortedwedges.size (); i++)
	{
		if (angles[i] < angles[j])
		{
			j = i;
		}
	}
	w = &lt->sortedwedges[j];
	wnext = &lt->sortedwedges[(j + 1) % (int)lt->sortedwedges.size ()];

	// Different wedge types have different interpolation methods
	switch (w->shape)
	{
#ifdef HLRAD_BILINEARINTERPOLATION
	case localtriangulation_t::Wedge::eSquareLeft:
	case localtriangulation_t::Wedge::eSquareRight:
#endif
	case localtriangulation_t::Wedge::eTriangular:
		// w->wedgenormal is undefined
		{
			vec_t frac;
			vec_t len;
			vec_t dist;
			bool istoofar;
			vec_t ratio;

			frac = GetFrac (w->leftspot, wnext->leftspot, direction, lt->normal);
			
			len = (1 - frac) * DotProduct (w->leftspot, direction) + frac * DotProduct (wnext->leftspot, direction);
			dist = DotProduct (spot, direction);
			if (len <= ON_EPSILON / 4 || dist > len + 2 * ON_EPSILON)
			{
				istoofar = true;
				ratio = 1.0;
			}
			else if (dist >= len - ON_EPSILON)
			{
				istoofar = false;
				ratio = 1.0;
			}
			else
			{
				istoofar = false;
				ratio = dist / len;
				ratio = qmax (0, qmin (ratio, 1));
			}

			if (istoofar)
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.resize (2);
				interp->points[0].patchnum = w->leftpatchnum;
				interp->points[0].weight = 1 - frac;
				interp->points[1].patchnum = wnext->leftpatchnum;
				interp->points[1].weight = frac;
			}
#ifdef HLRAD_BILINEARINTERPOLATION
			else if (w->shape == localtriangulation_t::Wedge::eSquareLeft)
			{
				i = w - &lt->sortedwedges[0];
				CalcInterpolation_Square (lt, i, spot, interp);
			}
			else if (w->shape == localtriangulation_t::Wedge::eSquareRight)
			{
				i = w - &lt->sortedwedges[0];
				i = (i - 1 + (int)lt->sortedwedges.size ()) % (int)lt->sortedwedges.size ();
				CalcInterpolation_Square (lt, i, spot, interp);
			}
#endif
			else
			{
				interp->isbiased = false;
				interp->totalweight = 1.0;
				interp->points.resize (3);
				interp->points[0].patchnum = lt->patchnum;
				interp->points[0].weight = 1 - ratio;
				interp->points[1].patchnum = w->leftpatchnum;
				interp->points[1].weight = ratio * (1 - frac);
				interp->points[2].patchnum = wnext->leftpatchnum;
				interp->points[2].weight = ratio * frac;
			}
		}
		break;
	case localtriangulation_t::Wedge::eConvex:
		// w->wedgenormal is the unit vector pointing from w->leftspot to wnext->leftspot
		{
			vec_t dot;
			vec_t dot1;
			vec_t dot2;
			vec_t frac;

			dot1 = DotProduct (w->leftspot, w->wedgenormal) - DotProduct (spot, w->wedgenormal);
			dot2 = DotProduct (wnext->leftspot, w->wedgenormal) - DotProduct (spot, w->wedgenormal);
			dot = 0 - DotProduct (spot, w->wedgenormal);
			// for eConvex type: dot1 < dot < dot2

			if (g_drawlerp && (dot1 > dot || dot > dot2))
			{
				Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 3.\n");
			}
			if (dot1 >= -NORMAL_EPSILON) // 0 <= dot1 < dot < dot2
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.resize (1);
				interp->points[0].patchnum = w->leftpatchnum;
				interp->points[0].weight = 1.0;
			}
			else if (dot2 <= NORMAL_EPSILON) // dot1 < dot < dot2 <= 0
			{
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.resize (1);
				interp->points[0].patchnum = wnext->leftpatchnum;
				interp->points[0].weight = 1.0;
			}
			else if (dot > 0) // dot1 < 0 < dot < dot2
			{
				frac = dot1 / (dot1 - dot);
				frac = qmax (0, qmin (frac, 1));

				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.resize (2);
				interp->points[0].patchnum = w->leftpatchnum;
				interp->points[0].weight = 1 - frac;
				interp->points[1].patchnum = lt->patchnum;
				interp->points[1].weight = frac;
			}
			else // dot1 < dot <= 0 < dot2
			{
				frac = dot / (dot - dot2);
				frac = qmax (0, qmin (frac, 1));
			
				interp->isbiased = true;
				interp->totalweight = 1.0;
				interp->points.resize (2);
				interp->points[0].patchnum = lt->patchnum;
				interp->points[0].weight = 1 - frac;
				interp->points[1].patchnum = wnext->leftpatchnum;
				interp->points[1].weight = frac;
			}
		}
		break;
	case localtriangulation_t::Wedge::eConcave:
		{
			vec_t len;
			vec_t dist;
			vec_t ratio;

			if (DotProduct (spot, w->wedgenormal) < 0) // the spot is closer to the left edge than the right edge
			{
				len = DotProduct (w->leftspot, w->leftdirection);
				dist = DotProduct (spot, w->leftdirection);
				if (g_drawlerp && len <= ON_EPSILON)
				{
					Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 4.\n");
				}
				if (dist <= NORMAL_EPSILON)
				{
					interp->isbiased = true;
					interp->totalweight = 1.0;
					interp->points.resize (1);
					interp->points[0].patchnum = lt->patchnum;
					interp->points[0].weight = 1.0;
				}
				else if (dist >= len)
				{
					interp->isbiased = true;
					interp->totalweight = 1.0;
					interp->points.resize (1);
					interp->points[0].patchnum = w->leftpatchnum;
					interp->points[0].weight = 1.0;
				}
				else
				{
					ratio = dist / len;
					ratio = qmax (0, qmin (ratio, 1));

					interp->isbiased = true;
					interp->totalweight = 1.0;
					interp->points.resize (2);
					interp->points[0].patchnum = lt->patchnum;
					interp->points[0].weight = 1 - ratio;
					interp->points[1].patchnum = w->leftpatchnum;
					interp->points[1].weight = ratio;
				}
			}
			else // the spot is closer to the right edge than the left edge
			{
				len = DotProduct (wnext->leftspot, wnext->leftdirection);
				dist = DotProduct (spot, wnext->leftdirection);
				if (g_drawlerp && len <= ON_EPSILON)
				{
					Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 5.\n");
				}
				if (dist <= NORMAL_EPSILON)
				{
					interp->isbiased = true;
					interp->totalweight = 1.0;
					interp->points.resize (1);
					interp->points[0].patchnum = lt->patchnum;
					interp->points[0].weight = 1.0;
				}
				else if (dist >= len)
				{
					interp->isbiased = true;
					interp->totalweight = 1.0;
					interp->points.resize (1);
					interp->points[0].patchnum = wnext->leftpatchnum;
					interp->points[0].weight = 1.0;
				}
				else
				{
					ratio = dist / len;
					ratio = qmax (0, qmin (ratio, 1));

					interp->isbiased = true;
					interp->totalweight = 1.0;
					interp->points.resize (2);
					interp->points[0].patchnum = lt->patchnum;
					interp->points[0].weight = 1 - ratio;
					interp->points[1].patchnum = wnext->leftpatchnum;
					interp->points[1].weight = ratio;
				}
			}
		}
		break;
	default:
		Error ("CalcInterpolation: internal error: invalid wedge type.");
		break;
	}
}

static void ApplyInterpolation (const interpolation_t *interp, int numstyles, const int *styles, vec3_t *outs
#ifdef ZHLT_XASH
								, vec3_t *outs_direction
#endif
								)
{
	int i;
	int j;

	for (j = 0; j < numstyles; j++)
	{
		VectorClear (outs[j]);
#ifdef ZHLT_XASH
		VectorClear (outs_direction[j]);
#endif
	}
	if (interp->totalweight <= 0)
	{
		return;
	}
	for (i = 0; i < (int)interp->points.size (); i++)
	{
		for (j = 0; j < numstyles; j++)
		{
#ifdef ZHLT_XASH
			const vec3_t *b_direction;
#endif
			const vec3_t *b = GetTotalLight (&g_patches[interp->points[i].patchnum], styles[j]
#ifdef ZHLT_XASH
											, b_direction
#endif
											);
			VectorMA (outs[j], interp->points[i].weight / interp->totalweight, *b, outs[j]);
#ifdef ZHLT_XASH
			VectorMA (outs_direction[j], interp->points[i].weight / interp->totalweight, *b_direction, outs_direction[j]);
#endif
		}
	}
}

// =====================================================================================
//  InterpolateSampleLight
// =====================================================================================
void InterpolateSampleLight (const vec3_t position, int surface, int numstyles, const int *styles, vec3_t *outs
#ifdef ZHLT_XASH
							, vec3_t *outs_direction
#endif
							)
{
	try
	{
	
	const facetriangulation_t *ft;
	interpolation_t *maininterp;
	std::vector< vec_t > localweights;
	std::vector< interpolation_t * > localinterps;

	int i;
	int j;
	int n;
	const facetriangulation_t *ft2;
	const localtriangulation_t *lt;
	vec3_t spot;
	vec_t weight;
	interpolation_t *interp;
	const localtriangulation_t *best;
	vec3_t v;
	vec_t dist;
	vec_t bestdist;
	vec_t dot;

	if (surface < 0 || surface >= g_numfaces)
	{
		Error ("InterpolateSampleLight: internal error: surface number out of range.");
	}
	ft = g_facetriangulations[surface];
	maininterp = new interpolation_t;
	maininterp->points.reserve (64);

	// Calculate local interpolations and their weights
	localweights.resize (0);
	localinterps.resize (0);
	if (g_lerp_enabled)
	{
		for (i = 0; i < (int)ft->neighbors.size (); i++) // for this face and each of its neighbors
		{
			ft2 = g_facetriangulations[ft->neighbors[i]];
			for (j = 0; j < (int)ft2->localtriangulations.size (); j++) // for each patch on that face
			{
				lt = ft2->localtriangulations[j];
				if (!CalcAdaptedSpot (lt, position, surface, spot))
				{
					if (g_drawlerp && ft2 == ft)
					{
						Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 6.\n");
					}
					continue;
				}
				if (!CalcWeight (lt, spot, &weight))
				{
					continue;
				}
				interp = new interpolation_t;
#ifdef HLRAD_BILINEARINTERPOLATION
				interp->points.reserve (4);
#else
				interp->points.reserve (3);
#endif
				CalcInterpolation (lt, spot, interp);

				localweights.push_back (weight);
				localinterps.push_back (interp);
			}
		}
	}
	
	// Combine into one interpolation
	maininterp->isbiased = false;
	maininterp->totalweight = 0;
	maininterp->points.resize (0);
	for (i = 0; i < (int)localinterps.size (); i++)
	{
		if (localinterps[i]->isbiased)
		{
			maininterp->isbiased = true;
		}
		for (j = 0; j < (int)localinterps[i]->points.size (); j++)
		{
			weight = localinterps[i]->points[j].weight * localweights[i];
			if (g_patches[localinterps[i]->points[j].patchnum].flags == ePatchFlagOutside)
			{
				weight *= 0.01;
			}
			n = (int)maininterp->points.size ();
			maininterp->points.resize (n + 1);
			maininterp->points[n].patchnum = localinterps[i]->points[j].patchnum;
			maininterp->points[n].weight = weight;
			maininterp->totalweight += weight;
		}
	}
	if (maininterp->totalweight > 0)
	{
		ApplyInterpolation (maininterp, numstyles, styles, outs
#ifdef ZHLT_XASH
							, outs_direction
#endif
							);
		if (g_drawlerp)
		{
			for (j = 0; j < numstyles; j++)
			{
				// white or yellow
				outs[j][0] = 100;
				outs[j][1] = 100;
				outs[j][2] = (maininterp->isbiased? 0: 100);
			}
		}
	}
	else
	{
		// try again, don't multiply localweights[i] (which equals to 0)
		maininterp->isbiased = false;
		maininterp->totalweight = 0;
		maininterp->points.resize (0);
		for (i = 0; i < (int)localinterps.size (); i++)
		{
			if (localinterps[i]->isbiased)
			{
				maininterp->isbiased = true;
			}
			for (j = 0; j < (int)localinterps[i]->points.size (); j++)
			{
				weight = localinterps[i]->points[j].weight;
				if (g_patches[localinterps[i]->points[j].patchnum].flags == ePatchFlagOutside)
				{
					weight *= 0.01;
				}
				n = (int)maininterp->points.size ();
				maininterp->points.resize (n + 1);
				maininterp->points[n].patchnum = localinterps[i]->points[j].patchnum;
				maininterp->points[n].weight = weight;
				maininterp->totalweight += weight;
			}
		}
		if (maininterp->totalweight > 0)
		{
			ApplyInterpolation (maininterp, numstyles, styles, outs
#ifdef ZHLT_XASH
								, outs_direction
#endif
								);
			if (g_drawlerp)
			{
				for (j = 0; j < numstyles; j++)
				{
					// red
					outs[j][0] = 100;
					outs[j][1] = 0;
					outs[j][2] = (maininterp->isbiased? 0: 100);
				}
			}
		}
		else
		{
			// worst case, simply use the nearest patch

			best = NULL;
			for (i = 0; i < (int)ft->localtriangulations.size (); i++)
			{
				lt = ft->localtriangulations[i];
				VectorCopy (position, v);
				snap_to_winding (lt->winding, lt->plane, v);
				VectorSubtract (v, position, v);
				dist = VectorLength (v);
				if (best == NULL || dist < bestdist - ON_EPSILON)
				{
					best = lt;
					bestdist = dist;
				}
			}

			if (best)
			{
				lt = best;
				VectorSubtract (position, lt->center, spot);
				dot = DotProduct (spot, lt->normal);
				VectorMA (spot, -dot, lt->normal, spot);
				CalcInterpolation (lt, spot, maininterp);

				maininterp->totalweight = 0;
				for (j = 0; j < (int)maininterp->points.size (); j++)
				{
					if (g_patches[maininterp->points[j].patchnum].flags == ePatchFlagOutside)
					{
						maininterp->points[j].weight *= 0.01;
					}
					maininterp->totalweight += maininterp->points[j].weight;
				}
				ApplyInterpolation (maininterp, numstyles, styles, outs
#ifdef ZHLT_XASH
									, outs_direction
#endif
									);
				if (g_drawlerp)
				{
					for (j = 0; j < numstyles; j++)
					{
						// green
						outs[j][0] = 0;
						outs[j][1] = 100;
						outs[j][2] = (maininterp->isbiased? 0: 100);
					}
				}
			}
			else
			{
				maininterp->isbiased = true;
				maininterp->totalweight = 0;
				maininterp->points.resize (0);
				ApplyInterpolation (maininterp, numstyles, styles, outs
#ifdef ZHLT_XASH
									, outs_direction
#endif
									);
				if (g_drawlerp)
				{
					for (j = 0; j < numstyles; j++)
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

	for (i = 0; i < (int)localinterps.size (); i++)
	{
		delete localinterps[i];
	}

	}
	catch (std::bad_alloc)
	{
		hlassume (false, assume_NoMemory);
	}
}

static bool TestLineSegmentIntersectWall (const facetriangulation_t *facetrian, const vec3_t p1, const vec3_t p2)
{
	int i;
	const facetriangulation_t::Wall *wall;
	vec_t front;
	vec_t back;
	vec_t dot1;
	vec_t dot2;
	vec_t dot;
	vec_t bottom;
	vec_t top;
	vec_t frac;

	for (i = 0; i < (int)facetrian->walls.size (); i++)
	{
		wall = &facetrian->walls[i];
		bottom = DotProduct (wall->points[0], wall->direction);
		top = DotProduct (wall->points[1], wall->direction);
		front = DotProduct (p1, wall->normal) - DotProduct (wall->points[0], wall->normal);
		back = DotProduct (p2, wall->normal) - DotProduct (wall->points[0], wall->normal);
		if (front > ON_EPSILON && back > ON_EPSILON || front < -ON_EPSILON && back < -ON_EPSILON)
		{
			continue;
		}
		dot1 = DotProduct (p1, wall->direction);
		dot2 = DotProduct (p2, wall->direction);
		if (fabs (front) <= 2 * ON_EPSILON && fabs (back) <= 2 * ON_EPSILON)
		{
			top = qmin (top, qmax (dot1, dot2));
			bottom = qmax (bottom, qmin (dot1, dot2));
		}
		else
		{
			frac = front / (front - back);
			frac = qmax (0, qmin (frac, 1));
			dot = dot1 + frac * (dot2 - dot1);
			top = qmin (top, dot);
			bottom = qmax (bottom, dot);
		}
		if (top - bottom >= -ON_EPSILON)
		{
			return true;
		}
	}

	return false;
}
#ifdef HLRAD_FARPATCH_FIX

static bool TestFarPatch (const localtriangulation_t *lt, const vec3_t p2, const Winding &p2winding)
{
	int i;
	vec3_t v;
	vec_t dist;
	vec_t size1;
	vec_t size2;

	size1 = 0;
	for (i = 0; i < lt->winding.m_NumPoints; i++)
	{
		VectorSubtract (lt->winding.m_Points[i], lt->center, v);
		dist = VectorLength (v);
		if (dist > size1)
		{
			size1 = dist;
		}
	}

	size2 = 0;
	for (i = 0; i < p2winding.m_NumPoints; i++)
	{
		VectorSubtract (p2winding.m_Points[i], p2, v);
		dist = VectorLength (v);
		if (dist > size2)
		{
			size2 = dist;
		}
	}

	VectorSubtract (p2, lt->center, v);
	dist = VectorLength (v);

	return dist > 1.4 * (size1 + size2);
}
#endif

#define TRIANGLE_SHAPE_THRESHOLD (115.0*Q_PI/180)
// If one of the angles in a triangle exceeds this threshold, the most distant point will be removed or the triangle will break into a convex-type wedge.

static void GatherPatches (localtriangulation_t *lt, const facetriangulation_t *facetrian)
{
	int i;
	int facenum2;
	const dplane_t *dp2;
	const patch_t *patch2;
	int patchnum2;
	vec3_t v;
	localtriangulation_t::Wedge point;
	std::vector< localtriangulation_t::Wedge > points;
	std::vector< std::pair< vec_t, int > > angles;
	vec_t angle;

	if (!g_lerp_enabled)
	{
		lt->sortedwedges.resize (0);
		return;
	}

	points.resize (0);
	for (i = 0; i < (int)lt->neighborfaces.size (); i++)
	{
		facenum2 = lt->neighborfaces[i];
		dp2 = getPlaneFromFaceNumber (facenum2);
		for (patch2 = g_face_patches[facenum2]; patch2; patch2 = patch2->next)
		{
			patchnum2 = patch2 - g_patches;
			
			point.leftpatchnum = patchnum2;
			VectorMA (patch2->origin, -PATCH_HUNT_OFFSET, dp2->normal, v);

			// Do permission tests using the original position of the patch
			if (patchnum2 == lt->patchnum || point_in_winding (lt->winding, lt->plane, v))
			{
				continue;
			}
			if (facenum2 != facetrian->facenum && TestLineSegmentIntersectWall (facetrian, lt->center, v))
			{
				continue;
			}
#ifdef HLRAD_FARPATCH_FIX
			if (TestFarPatch (lt, v, *patch2->winding))
			{
				continue;
			}
#endif

			// Store the adapted position of the patch
			if (!CalcAdaptedSpot (lt, v, facenum2, point.leftspot))
			{
				continue;
			}
			if (GetDirection (point.leftspot, lt->normal, point.leftdirection) <= 2 * ON_EPSILON)
			{
				continue;
			}
			points.push_back (point);
		}
	}

	// Sort the patches into clockwise order
	angles.resize ((int)points.size ());
	for (i = 0; i < (int)points.size (); i++)
	{
		angle = GetAngle (points[0].leftdirection, points[i].leftdirection, lt->normal);
		if (i == 0)
		{
			if (g_drawlerp && fabs (angle) > NORMAL_EPSILON)
			{
				Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 7.\n");
			}
			angle = 0.0;
		}
		angles[i].first = GetAngleDiff (angle, 0);
		angles[i].second = i;
	}
	std::sort (angles.begin (), angles.end ());

	lt->sortedwedges.resize ((int)points.size ());
	for (i = 0; i < (int)points.size (); i++)
	{
		lt->sortedwedges[i] = points[angles[i].second];
	}
}

static void PurgePatches (localtriangulation_t *lt)
{
	std::vector< localtriangulation_t::Wedge > points;
	int i;
	int cur;
	std::vector< int > next;
	std::vector< int > prev;
	std::vector< int > valid;
	std::vector< std::pair< vec_t, int > > dists;
	vec_t angle;
	vec3_t normal;
	vec3_t v;

	points.swap (lt->sortedwedges);
	lt->sortedwedges.resize (0);

	next.resize ((int)points.size ());
	prev.resize ((int)points.size ());
	valid.resize ((int)points.size ());
	dists.resize ((int)points.size ());
	for (i = 0; i < (int)points.size (); i++)
	{
		next[i] = (i + 1) % (int)points.size ();
		prev[i] = (i - 1 + (int)points.size ()) % (int)points.size ();
		valid[i] = 1;
		dists[i].first = DotProduct (points[i].leftspot, points[i].leftdirection);
		dists[i].second = i;
	}
	std::sort (dists.begin (), dists.end ());

	for (i = 0; i < (int)points.size (); i++)
	{
		cur = dists[i].second;
		if (valid[cur] == 0)
		{
			continue;
		}
		valid[cur] = 2; // mark current patch as final
		
		CrossProduct (points[cur].leftdirection, lt->normal, normal);
		VectorNormalize (normal);
		VectorScale (normal, cos (TRIANGLE_SHAPE_THRESHOLD), v);
		VectorMA (v, sin (TRIANGLE_SHAPE_THRESHOLD), points[cur].leftdirection, v);
		while (next[cur] != cur && valid[next[cur]] != 2)
		{
			angle = GetAngle (points[cur].leftdirection, points[next[cur]].leftdirection, lt->normal);
			if (fabs (angle) <= (1.0*Q_PI/180) ||
				GetAngleDiff (angle, 0) <= Q_PI + NORMAL_EPSILON
				&& DotProduct (points[next[cur]].leftspot, v) >= DotProduct (points[cur].leftspot, v) - ON_EPSILON / 2)
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
		
		CrossProduct (lt->normal, points[cur].leftdirection, normal);
		VectorNormalize (normal);
		VectorScale (normal, cos (TRIANGLE_SHAPE_THRESHOLD), v);
		VectorMA (v, sin (TRIANGLE_SHAPE_THRESHOLD), points[cur].leftdirection, v);
		while (prev[cur] != cur && valid[prev[cur]] != 2)
		{
			angle = GetAngle (points[prev[cur]].leftdirection, points[cur].leftdirection, lt->normal);
			if (fabs (angle) <= (1.0*Q_PI/180) ||
				GetAngleDiff (angle, 0) <= Q_PI + NORMAL_EPSILON
				&& DotProduct (points[prev[cur]].leftspot, v) >= DotProduct (points[cur].leftspot, v) - ON_EPSILON / 2)
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

	for (i = 0; i < (int)points.size (); i++)
	{
		if (valid[i] == 2)
		{
			lt->sortedwedges.push_back (points[i]);
		}
	}
}

static void PlaceHullPoints (localtriangulation_t *lt)
{
	int i;
	int j;
	int n;
	vec3_t v;
	vec_t dot;
	vec_t angle;
	localtriangulation_t::HullPoint hp;
	std::vector< localtriangulation_t::HullPoint > spots;
	std::vector< std::pair< vec_t, int > > angles;
	const localtriangulation_t::Wedge *w;
	const localtriangulation_t::Wedge *wnext;
	std::vector< localtriangulation_t::HullPoint > arc_spots;
	std::vector< vec_t > arc_angles;
	std::vector< int > next;
	std::vector< int > prev;
	vec_t frac;
	vec_t len;
	vec_t dist;

	spots.reserve (lt->winding.m_NumPoints);
	spots.resize (0);
	for (i = 0; i < (int)lt->winding.m_NumPoints; i++)
	{
		VectorSubtract (lt->winding.m_Points[i], lt->center, v);
		dot = DotProduct (v, lt->normal);
		VectorMA (v, -dot, lt->normal, hp.spot);
		if (!GetDirection (hp.spot, lt->normal, hp.direction))
		{
			continue;
		}
		spots.push_back (hp);
	}

	if ((int)lt->sortedwedges.size () == 0)
	{
		angles.resize ((int)spots.size ());
		for (i = 0; i < (int)spots.size (); i++)
		{
			angle = GetAngle (spots[0].direction, spots[i].direction, lt->normal);
			if (i == 0)
			{
				angle = 0.0;
			}
			angles[i].first = GetAngleDiff (angle, 0);
			angles[i].second = i;
		}
		std::sort (angles.begin (), angles.end ());
		lt->sortedhullpoints.resize (0);
		for (i = 0; i < (int)spots.size (); i++)
		{
			if (g_drawlerp && angles[i].second != i)
			{
				Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 8.\n");
			}
			lt->sortedhullpoints.push_back (spots[angles[i].second]);
		}
		return;
	}

	lt->sortedhullpoints.resize (0);
	for (i = 0; i < (int)lt->sortedwedges.size (); i++)
	{
		w = &lt->sortedwedges[i];
		wnext = &lt->sortedwedges[(i + 1) % (int)lt->sortedwedges.size ()];

		angles.resize ((int)spots.size ());
		for (j = 0; j < (int)spots.size (); j++)
		{
			angle = GetAngle (w->leftdirection, spots[j].direction, lt->normal);
			angles[j].first = GetAngleDiff (angle, 0);
			angles[j].second = j;
		}
		std::sort (angles.begin (), angles.end ());
		angle = GetAngle (w->leftdirection, wnext->leftdirection, lt->normal);
		if ((int)lt->sortedwedges.size () == 1)
		{
			angle = 2 * Q_PI;
		}
		else
		{
			angle = GetAngleDiff (angle, 0);
		}

		arc_spots.resize ((int)spots.size () + 2);
		arc_angles.resize ((int)spots.size () + 2);
		next.resize ((int)spots.size () + 2);
		prev.resize ((int)spots.size () + 2);

		VectorCopy (w->leftspot, arc_spots[0].spot);
		VectorCopy (w->leftdirection, arc_spots[0].direction);
		arc_angles[0] = 0;
		next[0] = 1;
		prev[0] = -1;
		n = 1;
		for (j = 0; j < (int)spots.size (); j++)
		{
			if (NORMAL_EPSILON <= angles[j].first && angles[j].first <= angle - NORMAL_EPSILON)
			{
				arc_spots[n] = spots[angles[j].second];
				arc_angles[n] = angles[j].first;
				next[n] = n + 1;
				prev[n] = n - 1;
				n++;
			}
		}
		VectorCopy (wnext->leftspot, arc_spots[n].spot);
		VectorCopy (wnext->leftdirection, arc_spots[n].direction);
		arc_angles[n] = angle;
		next[n] = -1;
		prev[n] = n - 1;
		n++;

		for (j = 1; next[j] != -1; j = next[j])
		{
			while (prev[j] != -1)
			{
				if (arc_angles[next[j]] - arc_angles[prev[j]] <= Q_PI + NORMAL_EPSILON)
				{
					frac = GetFrac (arc_spots[prev[j]].spot, arc_spots[next[j]].spot, arc_spots[j].direction, lt->normal);
					len = (1 - frac) * DotProduct (arc_spots[prev[j]].spot, arc_spots[j].direction)
						+ frac * DotProduct (arc_spots[next[j]].spot, arc_spots[j].direction);
					dist = DotProduct (arc_spots[j].spot, arc_spots[j].direction);
					if (dist <= len + NORMAL_EPSILON)
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

		for (j = 0; next[j] != -1; j = next[j])
		{
			lt->sortedhullpoints.push_back (arc_spots[j]);
		}
	}
}

#ifdef HLRAD_BILINEARINTERPOLATION
static bool TryMakeSquare (localtriangulation_t *lt, int i)
{
	localtriangulation_t::Wedge *w1;
	localtriangulation_t::Wedge *w2;
	localtriangulation_t::Wedge *w3;
	vec3_t v;
	vec3_t dir1;
	vec3_t dir2;
	vec_t angle;

	w1 = &lt->sortedwedges[i];
	w2 = &lt->sortedwedges[(i + 1) % (int)lt->sortedwedges.size ()];
	w3 = &lt->sortedwedges[(i + 2) % (int)lt->sortedwedges.size ()];

	// (o, p1, p2) and (o, p2, p3) must be triangles and not in a square
	if (w1->shape != localtriangulation_t::Wedge::eTriangular || w2->shape != localtriangulation_t::Wedge::eTriangular)
	{
		return false;
	}
	
	// (o, p1, p3) must be a triangle
	angle = GetAngle (w1->leftdirection, w3->leftdirection, lt->normal);
	angle = GetAngleDiff (angle, 0);
	if (angle >= TRIANGLE_SHAPE_THRESHOLD)
	{
		return false;
	}

	// (p2, p1, p3) must be a triangle
	VectorSubtract (w1->leftspot, w2->leftspot, v);
	if (!GetDirection (v, lt->normal, dir1))
	{
		return false;
	}
	VectorSubtract (w3->leftspot, w2->leftspot, v);
	if (!GetDirection (v, lt->normal, dir2))
	{
		return false;
	}
	angle = GetAngle (dir2, dir1, lt->normal);
	angle = GetAngleDiff (angle, 0);
	if (angle >= TRIANGLE_SHAPE_THRESHOLD)
	{
		return false;
	}

	w1->shape = localtriangulation_t::Wedge::eSquareLeft;
	VectorSubtract (vec3_origin, dir1, w1->wedgenormal);
	w2->shape = localtriangulation_t::Wedge::eSquareRight;
	VectorCopy (dir2, w2->wedgenormal);
	return true;
}

static void FindSquares (localtriangulation_t *lt)
{
	int i;
	localtriangulation_t::Wedge *w;
	std::vector< std::pair< vec_t, int > > dists;

	if ((int)lt->sortedwedges.size () <= 2)
	{
		return;
	}

	dists.resize ((int)lt->sortedwedges.size ());
	for (i = 0; i < (int)lt->sortedwedges.size (); i++)
	{
		w = &lt->sortedwedges[i];
		dists[i].first = DotProduct (w->leftspot, w->leftdirection);
		dists[i].second = i;
	}
	std::sort (dists.begin (), dists.end ());

	for (i = 0; i < (int)lt->sortedwedges.size (); i++)
	{
		TryMakeSquare (lt, dists[i].second);
		TryMakeSquare (lt, (dists[i].second - 2 + (int)lt->sortedwedges.size ()) % (int)lt->sortedwedges.size ());
	}
}

#endif
static localtriangulation_t *CreateLocalTriangulation (const facetriangulation_t *facetrian, int patchnum)
{
	localtriangulation_t *lt;
	int i;
	const patch_t *patch;
	vec_t dot;
	int facenum;
	localtriangulation_t::Wedge *w;
	localtriangulation_t::Wedge *wnext;
	vec_t angle;
	vec_t total;
	vec3_t v;
	vec3_t normal;

	facenum = facetrian->facenum;
	patch = &g_patches[patchnum];
	lt = new localtriangulation_t;

	// Fill basic information for this local triangulation
	lt->plane = *getPlaneFromFaceNumber (facenum);
	lt->plane.dist += DotProduct (g_face_offset[facenum], lt->plane.normal);
	lt->winding = *patch->winding;
	VectorMA (patch->origin, -PATCH_HUNT_OFFSET, lt->plane.normal, lt->center);
	dot = DotProduct (lt->center, lt->plane.normal) - lt->plane.dist;
	VectorMA (lt->center, -dot, lt->plane.normal, lt->center);
	if (!point_in_winding_noedge (lt->winding, lt->plane, lt->center, DEFAULT_EDGE_WIDTH))
	{
		snap_to_winding_noedge (lt->winding, lt->plane, lt->center, DEFAULT_EDGE_WIDTH, 4 * DEFAULT_EDGE_WIDTH);
	}
	VectorCopy (lt->plane.normal, lt->normal);
	lt->patchnum = patchnum;
	lt->neighborfaces = facetrian->neighbors;

	// Gather all patches from nearby faces
	GatherPatches (lt, facetrian);
	
	// Remove distant patches
	PurgePatches (lt);

	// Calculate wedge types
	total = 0.0;
	for (i = 0; i < (int)lt->sortedwedges.size (); i++)
	{
		w = &lt->sortedwedges[i];
		wnext = &lt->sortedwedges[(i + 1) % (int)lt->sortedwedges.size ()];

		angle = GetAngle (w->leftdirection, wnext->leftdirection, lt->normal);
		if (g_drawlerp && ((int)lt->sortedwedges.size () >= 2 && fabs (angle) <= (0.9*Q_PI/180)))
		{
			Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 9.\n");
		}
		angle = GetAngleDiff (angle, 0);
		if ((int)lt->sortedwedges.size () == 1)
		{
			angle = 2 * Q_PI;
		}
		total += angle;

		if (angle <= Q_PI + NORMAL_EPSILON)
		{
			if (angle < TRIANGLE_SHAPE_THRESHOLD)
			{
				w->shape = localtriangulation_t::Wedge::eTriangular;
				VectorClear (w->wedgenormal);
			}
			else
			{
				w->shape = localtriangulation_t::Wedge::eConvex;
				VectorSubtract (wnext->leftspot, w->leftspot, v);
				GetDirection (v, lt->normal, w->wedgenormal);
			}
		}
		else
		{
			w->shape = localtriangulation_t::Wedge::eConcave;
			VectorAdd (wnext->leftdirection, w->leftdirection, v);
			CrossProduct (lt->normal, v, normal);
			VectorSubtract (wnext->leftdirection, w->leftdirection, v);
			VectorAdd (normal, v, normal);
			GetDirection (normal, lt->normal, w->wedgenormal);
			if (g_drawlerp && VectorLength (w->wedgenormal) == 0)
			{
				Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 10.\n");
			}
		}
	}
	if (g_drawlerp && ((int)lt->sortedwedges.size () > 0 && fabs (total - 2 * Q_PI) > 10 * NORMAL_EPSILON))
	{
		Developer (DEVELOPER_LEVEL_SPAM, "Debug: triangulation: internal error 11.\n");
	}
#ifdef HLRAD_BILINEARINTERPOLATION
	FindSquares (lt);
#endif

	// Calculate hull points
	PlaceHullPoints (lt);

	return lt;
}

static void FreeLocalTriangulation (localtriangulation_t *lt)
{
	delete lt;
}

static void FindNeighbors (facetriangulation_t *facetrian)
{
	int i;
	int j;
	int e;
	const edgeshare_t *es;
	int side;
	const facelist_t *fl;
	int facenum;
	int facenum2;
	const dface_t *f;
	const dface_t *f2;
	const dplane_t *dp;
	const dplane_t *dp2;

	facenum = facetrian->facenum;
	f = &g_dfaces[facenum];
	dp = getPlaneFromFace (f);

	facetrian->neighbors.resize (0);

	facetrian->neighbors.push_back (facenum);

	for (i = 0; i < f->numedges; i++)
	{
		e = g_dsurfedges[f->firstedge + i];
		es = &g_edgeshare[abs (e)];
		if (!es->smooth)
		{
			continue;
		}
		f2 = es->faces[e > 0? 1: 0];
		facenum2 = f2 - g_dfaces;
		dp2 = getPlaneFromFace (f2);
		if (DotProduct (dp->normal, dp2->normal) < -NORMAL_EPSILON)
		{
			continue;
		}
		for (j = 0; j < (int)facetrian->neighbors.size (); j++)
		{
			if (facetrian->neighbors[j] == facenum2)
			{
				break;
			}
		}
		if (j == (int)facetrian->neighbors.size ())
		{
			facetrian->neighbors.push_back (facenum2);
		}
	}

	for (i = 0; i < f->numedges; i++)
	{
		e = g_dsurfedges[f->firstedge + i];
		es = &g_edgeshare[abs (e)];
		if (!es->smooth)
		{
			continue;
		}
		for (side = 0; side < 2; side++)
		{
			for (fl = es->vertex_facelist[side]; fl; fl = fl->next)
			{
				f2 = fl->face;
				facenum2 = f2 - g_dfaces;
				dp2 = getPlaneFromFace (f2);
				if (DotProduct (dp->normal, dp2->normal) < -NORMAL_EPSILON)
				{
					continue;
				}
				for (j = 0; j < (int)facetrian->neighbors.size (); j++)
				{
					if (facetrian->neighbors[j] == facenum2)
					{
						break;
					}
				}
				if (j == (int)facetrian->neighbors.size ())
				{
					facetrian->neighbors.push_back (facenum2);
				}
			}
		}
	}
}

static void BuildWalls (facetriangulation_t *facetrian)
{
	int i;
	int j;
	int facenum;
	int facenum2;
	const dface_t *f;
	const dface_t *f2;
	const dplane_t *dp;
	const dplane_t *dp2;
	int e;
	const edgeshare_t *es;
	vec_t dot;

	facenum = facetrian->facenum;
	f = &g_dfaces[facenum];
	dp = getPlaneFromFace (f);

	facetrian->walls.resize (0);

	for (i = 0; i < (int)facetrian->neighbors.size (); i++)
	{
		facenum2 = facetrian->neighbors[i];
		f2 = &g_dfaces[facenum2];
		dp2 = getPlaneFromFace (f2);
		if (DotProduct (dp->normal, dp2->normal) <= 0.1)
		{
			continue;
		}
		for (j = 0; j < f2->numedges; j++)
		{
			e = g_dsurfedges[f2->firstedge + j];
			es = &g_edgeshare[abs (e)];
			if (!es->smooth)
			{
				facetriangulation_t::Wall wall;

				VectorAdd (g_dvertexes[g_dedges[abs(e)].v[0]].point, g_face_offset[facenum], wall.points[0]);
				VectorAdd (g_dvertexes[g_dedges[abs(e)].v[1]].point, g_face_offset[facenum], wall.points[1]);
				VectorSubtract (wall.points[1], wall.points[0], wall.direction);
				dot = DotProduct (wall.direction, dp->normal);
				VectorMA (wall.direction, -dot, dp->normal, wall.direction);
				if (VectorNormalize (wall.direction))
				{
					CrossProduct (wall.direction, dp->normal, wall.normal);
					VectorNormalize (wall.normal);
					facetrian->walls.push_back (wall);
				}
			}
		}
	}
}

static void CollectUsedPatches (facetriangulation_t *facetrian)
{
	int i;
	int j;
	int k;
	int patchnum;
	const localtriangulation_t *lt;
	const localtriangulation_t::Wedge *w;

	facetrian->usedpatches.resize (0);
	for (i = 0; i < (int)facetrian->localtriangulations.size (); i++)
	{
		lt = facetrian->localtriangulations[i];

		patchnum = lt->patchnum;
		for (k = 0; k < (int)facetrian->usedpatches.size (); k++)
		{
			if (facetrian->usedpatches[k] == patchnum)
			{
				break;
			}
		}
		if (k == (int)facetrian->usedpatches.size ())
		{
			facetrian->usedpatches.push_back (patchnum);
		}

		for (j = 0; j < (int)lt->sortedwedges.size (); j++)
		{
			w = &lt->sortedwedges[j];

			patchnum = w->leftpatchnum;
			for (k = 0; k < (int)facetrian->usedpatches.size (); k++)
			{
				if (facetrian->usedpatches[k] == patchnum)
				{
					break;
				}
			}
			if (k == (int)facetrian->usedpatches.size ())
			{
				facetrian->usedpatches.push_back (patchnum);
			}
		}
	}
}


// =====================================================================================
//  CreateTriangulations
// =====================================================================================
void CreateTriangulations (int facenum)
{
	try
	{

	facetriangulation_t *facetrian;
	int patchnum;
	const patch_t *patch;
	localtriangulation_t *lt;

	g_facetriangulations[facenum] = new facetriangulation_t;
	facetrian = g_facetriangulations[facenum];

	facetrian->facenum = facenum;

	// Find neighbors
	FindNeighbors (facetrian);

	// Build walls
	BuildWalls (facetrian);

	// Create local triangulation around each patch
	facetrian->localtriangulations.resize (0);
	for (patch = g_face_patches[facenum]; patch; patch = patch->next)
	{
		patchnum = patch - g_patches;
		lt = CreateLocalTriangulation (facetrian, patchnum);
		facetrian->localtriangulations.push_back (lt);
	}

	// Collect used patches
	CollectUsedPatches (facetrian);

	}
	catch (std::bad_alloc)
	{
		hlassume (false, assume_NoMemory);
	}
}

// =====================================================================================
//  GetTriangulationPatches
// =====================================================================================
void GetTriangulationPatches (int facenum, int *numpatches, const int **patches)
{
	const facetriangulation_t *facetrian;

	facetrian = g_facetriangulations[facenum];
	*numpatches = (int)facetrian->usedpatches.size ();
	*patches = facetrian->usedpatches.begin();
}

// =====================================================================================
//  FreeTriangulations
// =====================================================================================
void FreeTriangulations ()
{
	try
	{

	int i;
	int j;
	facetriangulation_t *facetrian;

	for (i = 0; i < g_numfaces; i++)
	{
		facetrian = g_facetriangulations[i];

		for (j = 0; j < (int)facetrian->localtriangulations.size (); j++)
		{
			FreeLocalTriangulation (facetrian->localtriangulations[j]);
		}

		delete facetrian;
		g_facetriangulations[i] = NULL;
	}

	}
	catch (std::bad_alloc)
	{
		hlassume (false, assume_NoMemory);
	}
}

#else
#ifdef HLRAD_LERP_VL
static bool		LerpTriangle(const lerpTriangulation_t* trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
					vec3_t &result_direction, 
	#endif
					int pt1, int pt2, int pt3, int style);
static bool		LerpEdge(const lerpTriangulation_t* trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
					vec3_t &result_direction, 
	#endif
					int pt1, int pt2, int style);
static bool		LerpNearest(const lerpTriangulation_t* trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
					vec3_t &result_direction, 
	#endif
					int pt1, int style);
#define LERP_EPSILON 0.5
#endif

#ifndef HLRAD_LERP_VL
// =====================================================================================
//  TestWallIntersectTri
//      Returns true if wall polygon intersects patch polygon
// =====================================================================================
static bool     TestWallIntersectTri(const lerpTriangulation_t* const trian, const vec3_t p1, const vec3_t p2, const vec3_t p3)
{
#ifdef HLRAD_LERP_VL
	int x;
    const lerpWall_t* wall;
	const vec_t* normal = trian->plane->normal;
	{
		vec3_t d1, d2, n;
		VectorSubtract (p3, p2, d1);
		VectorSubtract (p1, p2, d2);
		CrossProduct (d1, d2, n);
		if (DotProduct (n, normal) < 0)
		{
			const vec_t* tmp;
			tmp = p2;
			p2 = p3;
			p3 = tmp;
		}
	}
	for (x = 0, wall = trian->walls; x < trian->numwalls; x++, wall++)
	{
		if (point_in_tri (wall->vertex0, trian->plane, p1, p2, p3))
		{
			return true;
		}
		if (point_in_tri (wall->vertex1, trian->plane, p1, p2, p3))
		{
			return true;
		}
	}
	return false;
#else
    int             x;
    const lerpWall_t* wall = trian->walls;
    dplane_t        plane;

    plane_from_points(p1, p2, p3, &plane);

    // Try first 'vertical' side
    // Since we test each of the 3 segments from patch against wall, only one side of wall needs testing inside 
    // patch (since they either dont intersect at all at this point, or both line segments intersect inside)
    for (x = 0; x < trian->numwalls; x++, wall++)
    {
        vec3_t          point;

        // Try side A
        if (intersect_linesegment_plane(&plane, wall->vertex[0], wall->vertex[3], point))
        {
            if (point_in_tri(point, &plane, p1, p2, p3))
            {
#if 0
                Verbose
                    ("Wall side A point @ (%4.3f %4.3f %4.3f) inside patch (%4.3f %4.3f %4.3f) (%4.3f %4.3f %4.3f) (%4.3f %4.3f %4.3f)\n",
                     point[0], point[1], point[2], p1[0], p1[1], p1[2], p2[0], p2[1], p2[2], p3[0], p3[1], p3[2]);
#endif
                return true;
            }
        }
    }
    return false;
#endif
}
#endif

// =====================================================================================
//  TestLineSegmentIntersectWall
//      Returns true if line would hit the 'wall' (to fix light streaking)
// =====================================================================================
static bool     TestLineSegmentIntersectWall(const lerpTriangulation_t* const trian, const vec3_t p1, const vec3_t p2
#ifdef HLRAD_LERP_VL
											, vec_t epsilon = ON_EPSILON
#endif
											)
{
    int             x;
    const lerpWall_t* wall = trian->walls;

    for (x = 0; x < trian->numwalls; x++, wall++)
    {
#ifdef HLRAD_LERP_VL
		vec_t front, back, frac;
		vec3_t mid;
		front = DotProduct (p1, wall->plane.normal) - wall->plane.dist;
		back = DotProduct (p2, wall->plane.normal) - wall->plane.dist;
		if (fabs (front) <= epsilon && fabs (back) <= epsilon)
		{
			if (DotProduct (p1, wall->increment) < DotProduct (wall->vertex0, wall->increment) - epsilon &&
				DotProduct (p2, wall->increment) < DotProduct (wall->vertex0, wall->increment) - epsilon )
			{
				continue;
			}
			if (DotProduct (p1, wall->increment) > DotProduct (wall->vertex1, wall->increment) + epsilon &&
				DotProduct (p2, wall->increment) > DotProduct (wall->vertex1, wall->increment) + epsilon )
			{
				continue;
			}
			return true;
		}
		if (front > 0.9 * epsilon && back > 0.9 * epsilon || front < -0.9 * epsilon && back < -0.9 * epsilon)
		{
			continue;
		}
		frac = front / (front - back);
		frac = qmax (0, qmin (frac, 1));
		mid[0] = p1[0] + (p2[0] - p1[0]) * frac;
		mid[1] = p1[1] + (p2[1] - p1[1]) * frac;
		mid[2] = p1[2] + (p2[2] - p1[2]) * frac;
		if (DotProduct (mid, wall->increment) < DotProduct (wall->vertex0, wall->increment) - epsilon ||
			DotProduct (mid, wall->increment) > DotProduct (wall->vertex1, wall->increment) + epsilon )
		{
			continue;
		}
		return true;
#else
        vec3_t          point;

        if (intersect_linesegment_plane(&wall->plane, p1, p2, point))
        {
            if (point_in_wall(wall, point))
            {
#if 0
                Verbose
                    ("Tested point @ (%4.3f %4.3f %4.3f) blocks segment from (%4.3f %4.3f %4.3f) to (%4.3f %4.3f %4.3f) intersects wall\n",
                     point[0], point[1], point[2], p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);
#endif
                return true;
            }
        }
#endif
    }
    return false;
}

#ifndef HLRAD_LERP_VL
// =====================================================================================
//  TestTriIntersectWall
//      Returns true if line would hit the 'wall' (to fix light streaking)
// =====================================================================================
static bool     TestTriIntersectWall(const lerpTriangulation_t* trian, const vec3_t p1, const vec3_t p2,
                                     const vec3_t p3)
{
    if (TestLineSegmentIntersectWall(trian, p1, p2) || TestLineSegmentIntersectWall(trian, p1, p3)
        || TestLineSegmentIntersectWall(trian, p2, p3))
    {
        return true;
    }
    return false;
}
#endif

// =====================================================================================
//  LerpTriangle
//      pt1 must be closest point
// =====================================================================================
#ifdef HLRAD_LERP_VL
static bool LerpTriangle(const lerpTriangulation_t* trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
				vec3_t &result_direction, 
	#endif
				int pt1, int pt2, int pt3, int style)
{
    patch_t *p1;
    patch_t *p2;
    patch_t *p3;
	const vec_t *o1;
	const vec_t *o2;
	const vec_t *o3;
	vec3_t v;

    p1 = trian->points[pt1];
    p2 = trian->points[pt2];
    p3 = trian->points[pt3];
#ifdef HLRAD_LERP_TEXNORMAL
	o1 = trian->points_pos[pt1];
	o2 = trian->points_pos[pt2];
	o3 = trian->points_pos[pt3];
#else
	o1 = p1->origin;
	o2 = p2->origin;
	o3 = p3->origin;
#endif

	dplane_t edge12, edge13, edge23;

	VectorSubtract (o2, o1, v);
	CrossProduct (trian->plane->normal, v, edge12.normal);
	VectorNormalize (edge12.normal);
	edge12.dist = DotProduct (o1, edge12.normal);

	VectorSubtract (o3, o1, v);
	CrossProduct (trian->plane->normal, v, edge13.normal);
	VectorNormalize (edge13.normal);
	edge13.dist = DotProduct (o1, edge13.normal);

	VectorSubtract (o3, o2, v);
	CrossProduct (trian->plane->normal, v, edge23.normal);
	VectorNormalize (edge23.normal);
	edge23.dist = DotProduct (o2, edge23.normal);

	vec_t dist12 = DotProduct (point, edge12.normal) - edge12.dist;
	vec_t dist13 = DotProduct (point, edge13.normal) - edge13.dist;
	vec_t dist23 = DotProduct (point, edge23.normal) - edge23.dist;

	// the point could be on the edge.
	if (fabs (dist12) < LERP_EPSILON)
	{
		if (LerpEdge (trian, point, result, 
	#ifdef ZHLT_XASH
				result_direction, 
	#endif
				pt1, pt2, style))
		{
			return true;
		}
	}
	if (fabs (dist13) < LERP_EPSILON)
	{
		if (LerpEdge (trian, point, result, 
	#ifdef ZHLT_XASH
				result_direction, 
	#endif
				pt1, pt3, style))
		{
			return true;
		}
	}
	if (fabs (dist23) < LERP_EPSILON)
	{
		if (LerpEdge (trian, point, result, 
	#ifdef ZHLT_XASH
				result_direction, 
	#endif
				pt2, pt3, style))
		{
			return true;
		}
	}

	// now that the point is not on the edge, we should check the shape of the triangle and make sure that the point is precisely inside the triangle.
	vec_t maxdist12 = DotProduct (o3, edge12.normal) - edge12.dist;
	vec_t maxdist13 = DotProduct (o2, edge13.normal) - edge13.dist;
	vec_t maxdist23 = DotProduct (o1, edge23.normal) - edge23.dist;
	if (fabs (maxdist12) <= LERP_EPSILON || fabs (maxdist13) <= LERP_EPSILON || fabs (maxdist23) <= LERP_EPSILON)
	{
		return false;
	}
	if (dist12 / maxdist12 < 0 || dist12 / maxdist12 > 1 ||
		dist13 / maxdist13 < 0 || dist13 / maxdist13 > 1 ||
		dist23 / maxdist23 < 0 || dist23 / maxdist23 > 1 )
	{
		return false;
	}

	vec3_t testpoint;
	VectorScale (p1->origin, 1 - dist13 / maxdist13 - dist12 / maxdist12, testpoint);
	VectorMA (testpoint, dist13 / maxdist13, p2->origin, testpoint);
	VectorMA (testpoint, dist12 / maxdist12, p3->origin, testpoint);
	if (TestLineSegmentIntersectWall (trian, testpoint, p1->origin) ||
		TestLineSegmentIntersectWall (trian, testpoint, p2->origin) ||
		TestLineSegmentIntersectWall (trian, testpoint, p3->origin) ||
		TestLineSegmentIntersectWall (trian, p2->origin, p1->origin) ||
		TestLineSegmentIntersectWall (trian, p3->origin, p2->origin) ||
		TestLineSegmentIntersectWall (trian, p1->origin, p3->origin) )
	{
		return false;
	}

	const vec3_t *l1, *l2, *l3;
	#ifdef ZHLT_XASH
	const vec3_t *l1_d, *l2_d, *l3_d;
	#endif
	l1 = GetTotalLight(p1, style
	#ifdef ZHLT_XASH
			, l1_d
	#endif
			);
	l2 = GetTotalLight(p2, style
	#ifdef ZHLT_XASH
			, l2_d
	#endif
			);
	l3 = GetTotalLight(p3, style
	#ifdef ZHLT_XASH
			, l3_d
	#endif
			);

	VectorScale (*l1, 1 - dist13 / maxdist13 - dist12 / maxdist12, result);
	VectorMA (result, dist13 / maxdist13, *l2, result);
	VectorMA (result, dist12 / maxdist12, *l3, result);
	#ifdef ZHLT_XASH
	VectorScale (*l1_d, 1 - dist13 / maxdist13 - dist12 / maxdist12, result_direction);
	VectorMA (result_direction, dist13 / maxdist13, *l2_d, result_direction);
	VectorMA (result_direction, dist12 / maxdist12, *l3_d, result_direction);
	#endif

	return true;
}
#else
#ifdef ZHLT_TEXLIGHT
static void     LerpTriangle(const lerpTriangulation_t* const trian, const vec3_t point, vec3_t result, const unsigned pt1, const unsigned pt2, const unsigned pt3, int style) //LRC
#else
static void     LerpTriangle(const lerpTriangulation_t* const trian, const vec3_t point, vec3_t result, const unsigned pt1, const unsigned pt2, const unsigned pt3)
#endif
{
    patch_t*        p1;
    patch_t*        p2;
    patch_t*        p3;
    vec3_t          base;
    vec3_t          d1;
    vec3_t          d2;
    vec_t           x;
    vec_t           y;
    vec_t           y1;
    vec_t           x2;
    vec3_t          v;
    dplane_t        ep1;
    dplane_t        ep2;

    p1 = trian->points[pt1];
    p2 = trian->points[pt2];
    p3 = trian->points[pt3];

#ifdef ZHLT_TEXLIGHT
    VectorCopy(*GetTotalLight(p1, style), base); //LRC
    VectorSubtract(*GetTotalLight(p2, style), base, d1); //LRC
    VectorSubtract(*GetTotalLight(p3, style), base, d2); //LRC
#else
    VectorCopy(p1->totallight, base);
    VectorSubtract(p2->totallight, base, d1);
    VectorSubtract(p3->totallight, base, d2);
#endif

    // Get edge normals
    VectorSubtract(p1->origin, p2->origin, v);
    VectorNormalize(v);
    CrossProduct(v, trian->plane->normal, ep1.normal);
    ep1.dist = DotProduct(p1->origin, ep1.normal);

    VectorSubtract(p1->origin, p3->origin, v);
    VectorNormalize(v);
    CrossProduct(v, trian->plane->normal, ep2.normal);
    ep2.dist = DotProduct(p1->origin, ep2.normal);

    x = DotProduct(point, ep1.normal) - ep1.dist;
    y = DotProduct(point, ep2.normal) - ep2.dist;

    y1 = DotProduct(p2->origin, ep2.normal) - ep2.dist;
    x2 = DotProduct(p3->origin, ep1.normal) - ep1.dist;

    VectorCopy(base, result);
    if (fabs(x2) >= ON_EPSILON)
    {
        int             i;

        for (i = 0; i < 3; i++)
        {
            result[i] += x * d2[i] / x2;
        }
    }
    if (fabs(y1) >= ON_EPSILON)
    {
        int             i;

        for (i = 0; i < 3; i++)
        {
            result[i] += y * d1[i] / y1;
        }
    }
}
#endif

// =====================================================================================
//  LerpNearest
// =====================================================================================
#ifdef HLRAD_LERP_VL
static bool LerpNearest(const lerpTriangulation_t* trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
				vec3_t &result_direction, 
	#endif
				int pt1, int style)
{
    patch_t *patch;
	patch = trian->points[pt1];
	const vec3_t *l;
	#ifdef ZHLT_XASH
	const vec3_t *l_d;
	#endif
	l = GetTotalLight(patch, style
	#ifdef ZHLT_XASH
			, l_d
	#endif
			);
	VectorCopy (*l, result);
	#ifdef ZHLT_XASH
	VectorCopy (*l_d, result_direction);
	#endif
	return true;
}
#else
#ifdef ZHLT_TEXLIGHT
static void     LerpNearest(const lerpTriangulation_t* const trian, vec3_t result, int style) //LRC
#else
static void     LerpNearest(const lerpTriangulation_t* const trian, vec3_t result)
#endif
{
    unsigned        x;
    unsigned        numpoints = trian->numpoints;
    patch_t*        patch;

    // Find nearest in original face
    for (x = 0; x < numpoints; x++)
    {
        patch = trian->points[trian->dists[x].patch];

        if (patch->faceNumber == trian->facenum)
        {
	#ifdef ZHLT_TEXLIGHT
            VectorCopy(*GetTotalLight(patch, style), result); //LRC
	#else
            VectorCopy(patch->totallight, result);
	#endif
            return;
        }
    }

    // If none in nearest face, settle for nearest
    if (numpoints)
    {
	#ifdef ZHLT_TEXLIGHT 
        VectorCopy(*GetTotalLight(trian->points[trian->dists[0].patch], style), result); //LRC
	#else
        VectorCopy(trian->points[trian->dists[0].patch]->totallight, result);
	#endif
    }
    else
    {
        VectorClear(result);
    }
}
#endif

// =====================================================================================
//  LerpEdge
// =====================================================================================
#ifdef HLRAD_LERP_VL
static bool		LerpEdge(const lerpTriangulation_t* trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
					vec3_t &result_direction, 
	#endif
					int pt1, int pt2, int style)
{
    patch_t *p1;
    patch_t *p2;
	const vec_t *o1;
	const vec_t *o2;
    vec3_t increment;
	vec3_t normal;
	vec_t x;
    vec_t x1;
	vec3_t base;
	vec3_t d;
	#ifdef ZHLT_XASH
	vec3_t base_direction;
	vec3_t d_direction;
	#endif
	p1 = trian->points[pt1];
	p2 = trian->points[pt2];
#ifdef HLRAD_LERP_TEXNORMAL
	o1 = trian->points_pos[pt1];
	o2 = trian->points_pos[pt2];
#else
	o1 = p1->origin;
	o2 = p2->origin;
#endif
    if (TestLineSegmentIntersectWall(trian, p1->origin, p2->origin))
		return false;
	VectorSubtract (o2, o1, increment);
	CrossProduct (trian->plane->normal, increment, normal);
	CrossProduct (normal, trian->plane->normal, increment);
	VectorNormalize (increment);
	x = DotProduct (o2, increment) - DotProduct (o1, increment);
	x1 = DotProduct (point, increment) - DotProduct (o1, increment);
	if (x1 < -LERP_EPSILON || x1 > x + LERP_EPSILON)
	{
		return false;
	}
	const vec3_t *l;
	#ifdef ZHLT_XASH
	const vec3_t *l_d;
	#endif
	l = GetTotalLight(p1, style
	#ifdef ZHLT_XASH
			, l_d
	#endif
			);
	VectorCopy (*l, base);
	#ifdef ZHLT_XASH
	VectorCopy (*l_d, base_direction);
	#endif
	l = GetTotalLight(p2, style
	#ifdef ZHLT_XASH
			, l_d
	#endif
			);
	VectorSubtract (*l, base, d);
	VectorCopy (base, result);
	#ifdef ZHLT_XASH
	VectorSubtract (*l_d, base_direction, d_direction);
	VectorCopy (base_direction, result_direction);
	#endif
	if (x > LERP_EPSILON)
	{
		vec_t frac = x1 / x;
		if (frac < 0) frac = 0;
		if (frac > 1) frac = 1;
		VectorMA (result, frac, d, result);
	#ifdef ZHLT_XASH
		VectorMA (result_direction, frac, d_direction, result_direction);
	#endif
	}
	return true;
}
#else
#ifdef ZHLT_TEXLIGHT
static bool     LerpEdge(const lerpTriangulation_t* const trian, const vec3_t point, vec3_t result, int style) //LRC
#else
static bool     LerpEdge(const lerpTriangulation_t* const trian, const vec3_t point, vec3_t result)
#endif
{
    patch_t*        p1;
    patch_t*        p2;
#ifndef HLRAD_LERP_VL
    patch_t*        p3;
#endif
    vec3_t          v1;
    vec3_t          v2;
    vec_t           d;

#ifdef HLRAD_LERP_VL
	p1 = trian->points[pt1];
	p2 = trian->points[pt2];
#else
    p1 = trian->points[trian->dists[0].patch];
    p2 = trian->points[trian->dists[1].patch];
    p3 = trian->points[trian->dists[2].patch];
#endif

#ifndef HLRAD_LERP_FIX
    VectorSubtract(point, p1->origin, v2);
    VectorNormalize(v2);
#endif

    // Try nearest and 2
    if (!TestLineSegmentIntersectWall(trian, p1->origin, p2->origin))
    {
#ifdef HLRAD_LERP_FIX
		vec_t total_length, length1, length2;
		VectorSubtract (p2->origin, p1->origin, v1);
		CrossProduct (trian->plane->normal, v1, v2);
		CrossProduct (v2, trian->plane->normal, v1);
		length1 = DotProduct (v1, point) - DotProduct (v1, p1->origin);
		length2 = DotProduct (v1, p2->origin) - DotProduct (v1, point);
		total_length = DotProduct (v1, p2->origin) - DotProduct (v1, p1->origin);
		if (total_length > 0 && length1 >= 0 && length2 >= 0)
		{
            int             i;
#else
        VectorSubtract(p2->origin, p1->origin, v1);
        VectorNormalize(v1);
        d = DotProduct(v2, v1);
        if (d >= ON_EPSILON)
        {
            int             i;
            vec_t           length1;
            vec_t           length2;
            vec3_t          segment;
            vec_t           total_length;

            VectorSubtract(point, p1->origin, segment);
            length1 = VectorLength(segment);
            VectorSubtract(point, p2->origin, segment);
            length2 = VectorLength(segment);
            total_length = length1 + length2;
#endif

            for (i = 0; i < 3; i++)
            {
#ifdef ZHLT_TEXLIGHT
	#ifdef HLRAD_LERP_FIX
                result[i] = (((*GetTotalLight(p1, style))[i] * length2) + ((*GetTotalLight(p2, style))[i] * length1)) / total_length; //LRC
	#else
				result[i] = (((*GetTotalLight(p1, style))[i] * length2) + ((*GetTotalLight(p1, style))[i] * length1)) / total_length; //LRC
	#endif
#else
                result[i] = ((p1->totallight[i] * length2) + (p2->totallight[i] * length1)) / total_length;
#endif
            }
            return true;
        }
    }

#ifndef HLRAD_LERP_VL
    // Try nearest and 3
    if (!TestLineSegmentIntersectWall(trian, p1->origin, p3->origin))
    {
#ifdef HLRAD_LERP_FIX
		vec_t total_length, length1, length2;
		VectorSubtract (p3->origin, p1->origin, v1);
		CrossProduct (trian->plane->normal, v1, v2);
		CrossProduct (v2, trian->plane->normal, v1);
		length1 = DotProduct (v1, point) - DotProduct (v1, p1->origin);
		length2 = DotProduct (v1, p3->origin) - DotProduct (v1, point);
		total_length = DotProduct (v1, p3->origin) - DotProduct (v1, p1->origin);
		if (total_length > 0 && length1 >= 0 && length2 >= 0)
		{
            int             i;
#else
        VectorSubtract(p3->origin, p1->origin, v1);
        VectorNormalize(v1);
        d = DotProduct(v2, v1);
        if (d >= ON_EPSILON)
        {
            int             i;
            vec_t           length1;
            vec_t           length2;
            vec3_t          segment;
            vec_t           total_length;

            VectorSubtract(point, p1->origin, segment);
            length1 = VectorLength(segment);
            VectorSubtract(point, p3->origin, segment);
            length2 = VectorLength(segment);
            total_length = length1 + length2;
#endif

            for (i = 0; i < 3; i++)
            {
	#ifdef ZHLT_TEXLIGHT
                result[i] = (((*GetTotalLight(p1, style))[i] * length2) + ((*GetTotalLight(p3, style))[i] * length1)) / total_length; //LRC
	#else
                result[i] = ((p1->totallight[i] * length2) + (p3->totallight[i] * length1)) / total_length;
	#endif
            }
            return true;
        }
    }
#endif
    return false;
}
#endif


// =====================================================================================
//
//  SampleTriangulation
//
// =====================================================================================

// =====================================================================================
//  dist_sorter
// =====================================================================================
static int CDECL dist_sorter(const void* p1, const void* p2)
{
    lerpDist_t*     dist1 = (lerpDist_t*) p1;
    lerpDist_t*     dist2 = (lerpDist_t*) p2;

#ifdef HLRAD_LERP_VL
	if (dist1->invalid < dist2->invalid)
		return -1;
	if (dist2->invalid < dist1->invalid)
		return 1;
	if (dist1->dist + ON_EPSILON < dist2->dist)
		return -1;
	if (dist2->dist + ON_EPSILON < dist1->dist)
		return 1;
	if (dist1->pos[0] + ON_EPSILON < dist2->pos[0])
		return -1;
	if (dist2->pos[0] + ON_EPSILON < dist1->pos[0])
		return 1;
	if (dist1->pos[1] + ON_EPSILON < dist2->pos[1])
		return -1;
	if (dist2->pos[1] + ON_EPSILON < dist1->pos[1])
		return 1;
	if (dist1->pos[2] + ON_EPSILON < dist2->pos[2])
		return -1;
	if (dist2->pos[2] + ON_EPSILON < dist1->pos[2])
		return 1;
	return 0;
#else
    if (dist1->dist < dist2->dist)
    {
        return -1;
    }
    else if (dist1->dist > dist2->dist)
    {
        return 1;
    }
    else
    {
        return 0;
    }
#endif
}

// =====================================================================================
//  FindDists
// =====================================================================================
static void     FindDists(const lerpTriangulation_t* const trian, const vec3_t point)
{
    unsigned        x;
    unsigned        numpoints = trian->numpoints;
    patch_t**       patch = trian->points;
    lerpDist_t*     dists = trian->dists;
    vec3_t          delta;
#ifdef HLRAD_LERP_VL
	vec3_t testpoint;
	{
		VectorCopy (point, testpoint);
		Winding *w = new Winding (*trian->face);
		int i;
		for (i = 0; i < w->m_NumPoints; i++)
		{
			VectorAdd (w->m_Points[i], g_face_offset[trian->facenum], w->m_Points[i]);
		}
		if (!point_in_winding_noedge (*w, *trian->plane, testpoint, DEFAULT_EDGE_WIDTH))
		{
			snap_to_winding_noedge (*w, *trian->plane, testpoint, DEFAULT_EDGE_WIDTH, 4 * DEFAULT_EDGE_WIDTH);
		}
		delete w;
		if (!TestLineSegmentIntersectWall (trian, point, testpoint, 2 * ON_EPSILON))
		{
			VectorCopy (point, testpoint);
		}
	}
#endif

    for (x = 0; x < numpoints; x++, patch++, dists++)
    {
#ifdef HLRAD_LERP_TEXNORMAL
		VectorSubtract (trian->points_pos[x], point, delta);
#else
        VectorSubtract((*patch)->origin, point, delta);
#endif
#ifdef HLRAD_LERP_VL
		vec3_t normal;
		CrossProduct (trian->plane->normal, delta, normal);
		CrossProduct (normal, trian->plane->normal, delta);
#endif
        dists->dist = VectorLength(delta);
        dists->patch = x;
#ifdef HLRAD_LERP_VL
		dists->invalid = TestLineSegmentIntersectWall (trian, testpoint, (*patch)->origin);
		VectorCopy ((*patch)->origin, dists->pos);
#endif
    }

    qsort((void*)trian->dists, (size_t) numpoints, sizeof(lerpDist_t), dist_sorter);
}

// =====================================================================================
//  SampleTriangulation
// =====================================================================================
#ifdef ZHLT_TEXLIGHT
#ifdef HLRAD_LERP_VL
void            SampleTriangulation(const lerpTriangulation_t* const trian, const vec3_t point, vec3_t result, 
	#ifdef ZHLT_XASH
					vec3_t &result_direction, 
	#endif
					int style)
#else
void            SampleTriangulation(const lerpTriangulation_t* const trian, vec3_t point, vec3_t result, int style) //LRC
#endif
#else
void            SampleTriangulation(const lerpTriangulation_t* const trian, vec3_t point, vec3_t result)
#endif
{
    FindDists(trian, point);

#ifdef HLRAD_LERP_VL
	VectorClear(result);
	#ifdef ZHLT_XASH
	VectorClear (result_direction);
	#endif
	if (trian->numpoints >= 3 && trian->dists[2].invalid <= 0 && g_lerp_enabled)
	{
		int pt1 = trian->dists[0].patch;
		int pt2 = trian->dists[1].patch;
		int pt3 = trian->dists[2].patch;
		if (LerpTriangle (trian, point, result, 
	#ifdef ZHLT_XASH
				result_direction, 
	#endif
				pt1, pt2, pt3, style))
		{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
			if (g_drawlerp)
				result[0] = 100, result[1] = 100, result[2] = 100;
	#endif
			return;
		}
#ifdef HLRAD_LERP_TRY5POINTS
		if (trian->numpoints >= 4 && trian->dists[3].invalid <= 0)
		{
			int pt4 = trian->dists[3].patch;
			if (LerpTriangle (trian, point, result, 
	#ifdef ZHLT_XASH
					result_direction, 
	#endif
					pt1, pt2, pt4, style))
			{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
				if (g_drawlerp)
					result[0] = 100, result[1] = 100, result[2] = 0;
				return;
	#endif
			}
			if (LerpTriangle (trian, point, result, 
	#ifdef ZHLT_XASH
					result_direction, 
	#endif
					pt1, pt3, pt4, style))
			{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
				if (g_drawlerp)
					result[0] = 100, result[1] = 0, result[2] = 100;
	#endif
				return;
			}
			if (trian->numpoints >= 5 && trian->dists[4].invalid <= 0)
			{
				int pt5 = trian->dists[4].patch;
				if (LerpTriangle (trian, point, result, 
	#ifdef ZHLT_XASH
						result_direction, 
	#endif
						pt1, pt2, pt5, style))
				{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
					if (g_drawlerp)
						result[0] = 100, result[1] = 0, result[2] = 0;
	#endif
					return;
				}
				if (LerpTriangle (trian, point, result, 
	#ifdef ZHLT_XASH
						result_direction, 
	#endif
						pt1, pt3, pt5, style))
				{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
					if (g_drawlerp)
						result[0] = 100, result[1] = 0, result[2] = 0;
	#endif
					return;
				}
				if (LerpTriangle (trian, point, result, 
	#ifdef ZHLT_XASH
						result_direction, 
	#endif
						pt1, pt4, pt5, style))
				{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
					if (g_drawlerp)
						result[0] = 100, result[1] = 0, result[2] = 0;
	#endif
					return;
				}
			}
		}
#endif
	}
	if (trian->numpoints >= 2 && trian->dists[1].invalid <= 0  && g_lerp_enabled)
	{
		int pt1 = trian->dists[0].patch;
		int pt2 = trian->dists[1].patch;
		if (LerpEdge (trian, point, result, 
	#ifdef ZHLT_XASH
				result_direction, 
	#endif
				pt1, pt2, style))
		{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
			if (g_drawlerp)
				result[0] = 0, result[1] = 100, result[2] = 100;
	#endif
			return;
		}
		if (trian->numpoints >= 3 && trian->dists[2].invalid <= 0 )
		{
			int pt3 = trian->dists[2].patch;
			if (LerpEdge (trian, point, result, 
	#ifdef ZHLT_XASH
					result_direction, 
	#endif
					pt1, pt3, style))
			{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
				if (g_drawlerp)
					result[0] = 0, result[1] = 100, result[2] = 0;
	#endif
				return;
			}
		}
	}
	if (trian->numpoints >= 1)
	{
		int pt1 = trian->dists[0].patch;
		if (LerpNearest (trian, point, result, 
	#ifdef ZHLT_XASH
				result_direction, 
	#endif
				pt1, style))
		{
	#ifdef HLRAD_DEBUG_DRAWPOINTS
			if (g_drawlerp)
				result[0] = 0, result[1] = 0, result[2] = 100;
	#endif
			return;
		}
	}
#else
    if ((trian->numpoints > 3) && (g_lerp_enabled))
    {
        unsigned        pt1;
        unsigned        pt2;
        unsigned        pt3;
        vec_t*          p1;
        vec_t*          p2;
        vec_t*          p3;
        dplane_t        plane;

        pt1 = trian->dists[0].patch;
        pt2 = trian->dists[1].patch;
        pt3 = trian->dists[2].patch;

        p1 = trian->points[pt1]->origin;
        p2 = trian->points[pt2]->origin;
        p3 = trian->points[pt3]->origin;

        plane_from_points(p1, p2, p3, &plane);
        SnapToPlane(&plane, point, 0.0);
        if (point_in_tri(point, &plane, p1, p2, p3))
        {                                                  // TODO check edges/tri for blocking by wall
            if (!TestWallIntersectTri(trian, p1, p2, p3) && !TestTriIntersectWall(trian, p1, p2, p3))
            {
#ifdef ZHLT_TEXLIGHT
                LerpTriangle(trian, point, result, pt1, pt2, pt3, style); //LRC
#else
                LerpTriangle(trian, point, result, pt1, pt2, pt3);
#endif
                return;
            }
        }
        else
        {
#ifdef ZHLT_TEXLIGHT
            if (LerpEdge(trian, point, result, style)) //LRC
#else
            if (LerpEdge(trian, point, result))
#endif
            {
                return;
            }
        }
    }

#ifdef ZHLT_TEXLIGHT
    LerpNearest(trian, result, style); //LRC
#else
    LerpNearest(trian, result);
#endif
#endif
}

// =====================================================================================
//  AddPatchToTriangulation
// =====================================================================================
static void     AddPatchToTriangulation(lerpTriangulation_t* trian, patch_t* patch)
{
#ifdef HLRAD_PATCHBLACK_FIX
	if (patch->flags != ePatchFlagOutside)
#else
    if (!(patch->flags & ePatchFlagOutside))
#endif
    {
        int             pnum = trian->numpoints;

        if (pnum >= trian->maxpoints)
        {
            trian->points = (patch_t**)realloc(trian->points, sizeof(patch_t*) * (trian->maxpoints + DEFAULT_MAX_LERP_POINTS));

            hlassume(trian->points != NULL, assume_NoMemory);
            memset(trian->points + trian->maxpoints, 0, sizeof(patch_t*) * DEFAULT_MAX_LERP_POINTS);   // clear the new block
#ifdef HLRAD_LERP_TEXNORMAL
			trian->points_pos = (vec3_t *)realloc(trian->points_pos, sizeof (vec3_t) * (trian->maxpoints + DEFAULT_MAX_LERP_POINTS));
			hlassume (trian->points_pos != NULL, assume_NoMemory);
			memset (trian->points_pos + trian->maxpoints, 0, sizeof (vec3_t) * DEFAULT_MAX_LERP_POINTS);
#endif

            trian->maxpoints += DEFAULT_MAX_LERP_POINTS;
        }

        trian->points[pnum] = patch;
#ifdef HLRAD_LERP_TEXNORMAL
		VectorCopy (patch->origin, trian->points_pos[pnum]);
		if (patch->faceNumber != trian->facenum)
		{
			vec3_t snapdir;
			dplane_t p1 = *trian->plane;
			p1.dist += DotProduct (p1.normal, g_face_offset[trian->facenum]);
			dplane_t p2 = *getPlaneFromFaceNumber (patch->faceNumber);
			p2.dist += DotProduct (p2.normal, g_face_offset[patch->faceNumber]);
#ifdef HLRAD_GROWSAMPLE
			// we have abandoned the texnormal approach, since the lighting should not be affected by the texnormal: it should only rely on the s,t coordinate of vertices, which doesn't include texnormal information
			VectorAdd (p1.normal, p2.normal, snapdir);
			if (!VectorNormalize (snapdir)) // normal2 = -normal1
			{
				// skip this patch
				return;
			}
#else
			VectorCopy (p1.normal, snapdir);
			if (!GetIntertexnormal (patch->faceNumber, trian->facenum, snapdir))
			{
				Warning ("AddPatchToTriangulation: internal error 1.");
			}
#endif
			VectorMA (trian->points_pos[pnum], -PATCH_HUNT_OFFSET, p2.normal, trian->points_pos[pnum]);
			vec_t dist = (DotProduct (trian->points_pos[pnum], p1.normal) - p1.dist) / DotProduct (snapdir, p1.normal);
			VectorMA (trian->points_pos[pnum], -dist, snapdir, trian->points_pos[pnum]);
		}
#endif
        trian->numpoints++;
    }
}

// =====================================================================================
//  CreateWalls
// =====================================================================================
#ifdef HLRAD_LERP_VL
static void		AddWall (lerpTriangulation_t *trian, const vec_t *v1, const vec_t *v2) // v1 & v2 should be without offset
{
	int facenum = trian->facenum;
    const dplane_t* p = trian->plane;
	vec3_t delta;
	vec3_t p0;
	vec3_t p1;
	vec3_t normal;
	lerpWall_t *wall;

	if (trian->numwalls >= trian->maxwalls)
	{
		trian->walls =
			(lerpWall_t*)realloc(trian->walls, sizeof(lerpWall_t) * (trian->maxwalls + DEFAULT_MAX_LERP_WALLS));
		hlassume(trian->walls != NULL, assume_NoMemory);
		memset(trian->walls + trian->maxwalls, 0, sizeof(lerpWall_t) * DEFAULT_MAX_LERP_WALLS);     // clear the new block
		trian->maxwalls += DEFAULT_MAX_LERP_WALLS;
	}

	wall = &trian->walls[trian->numwalls];
	trian->numwalls++;
	VectorAdd (v1, g_face_offset[facenum], p0);
	VectorAdd (v2, g_face_offset[facenum], p1);
	VectorSubtract (p1, p0, delta);
	CrossProduct (p->normal, delta, normal);
	if (VectorNormalize (normal) == 0.0)
	{
		trian->numwalls--;
		return;
	}
	VectorCopy (normal, wall->plane.normal);
	wall->plane.dist = DotProduct (normal, p0);
	CrossProduct (normal, p->normal, wall->increment);
	VectorCopy (p0, wall->vertex0);
	VectorCopy (p1, wall->vertex1);
}
#endif
#ifndef HLRAD_LERP_FACELIST
static void     CreateWalls(lerpTriangulation_t* trian, const dface_t* const face)
{
#ifdef HLRAD_LERP_VL
    const dplane_t* p = getPlaneFromFace(face);
    int             facenum = face - g_dfaces;
    int             x;

    for (x = 0; x < face->numedges; x++)
    {
        edgeshare_t*    es;
        dface_t*        f2;
        int             edgenum = g_dsurfedges[face->firstedge + x];

        if (edgenum > 0)
        {
            es = &g_edgeshare[edgenum];
            f2 = es->faces[1];
        }
        else
        {
            es = &g_edgeshare[-edgenum];
            f2 = es->faces[0];
        }
		if (!es->smooth)
		{
			AddWall (trian, g_dvertexes[g_dedges[abs(edgenum)].v[0]].point, g_dvertexes[g_dedges[abs(edgenum)].v[1]].point);
		}
		else
		{
			int facenum2 = f2 - g_dfaces;
			int x2;
			for (x2 = 0; x2 < f2->numedges; x2++)
			{
				edgeshare_t *es2;
				dface_t *f3;
				int edgenum2 = g_dsurfedges[f2->firstedge + x2];
				if (edgenum2 > 0)
				{
					es2 = &g_edgeshare[edgenum2];
					f3 = es2->faces[1];
				}
				else
				{
					es2 = &g_edgeshare[-edgenum2];
					f3 = es2->faces[0];
				}
				if (!es2->smooth)
				{
					AddWall (trian, g_dvertexes[g_dedges[abs(edgenum2)].v[0]].point, g_dvertexes[g_dedges[abs(edgenum2)].v[1]].point);
				}
			}
		}
	}
#else
    const dplane_t* p = getPlaneFromFace(face);
    int             facenum = face - g_dfaces;
    int             x;

    for (x = 0; x < face->numedges; x++)
    {
        edgeshare_t*    es;
        dface_t*        f2;
        int             edgenum = g_dsurfedges[face->firstedge + x];

        if (edgenum > 0)
        {
            es = &g_edgeshare[edgenum];
            f2 = es->faces[1];
        }
        else
        {
            es = &g_edgeshare[-edgenum];
            f2 = es->faces[0];
        }

        // Build Wall for non-coplanar neighbhors
#ifdef HLRAD_GetPhongNormal_VL
        if (f2 && !es->smooth)
#else
        if (f2 && !es->coplanar && VectorCompare(vec3_origin, es->interface_normal))
#endif
        {
            const dplane_t* plane = getPlaneFromFace(f2);

            // if plane isn't facing us, ignore it
	#ifdef HLRAD_DPLANEOFFSET_MISCFIX
            if (DotProduct(plane->normal, g_face_centroids[facenum]) < plane->dist + DotProduct(plane->normal, g_face_offset[facenum]))
	#else
            if (DotProduct(plane->normal, g_face_centroids[facenum]) < plane->dist)
	#endif
            {
                continue;
            }

            {
                vec3_t          delta;
                vec3_t          p0;
                vec3_t          p1;
                lerpWall_t*     wall;

                if (trian->numwalls >= trian->maxwalls)
                {
                    trian->walls =
                        (lerpWall_t*)realloc(trian->walls, sizeof(lerpWall_t) * (trian->maxwalls + DEFAULT_MAX_LERP_WALLS));
                    hlassume(trian->walls != NULL, assume_NoMemory);
                    memset(trian->walls + trian->maxwalls, 0, sizeof(lerpWall_t) * DEFAULT_MAX_LERP_WALLS);     // clear the new block
                    trian->maxwalls += DEFAULT_MAX_LERP_WALLS;
                }

                wall = &trian->walls[trian->numwalls];
                trian->numwalls++;

                VectorScale(p->normal, 10000.0, delta);

                VectorCopy(g_dvertexes[g_dedges[abs(edgenum)].v[0]].point, p0);
                VectorCopy(g_dvertexes[g_dedges[abs(edgenum)].v[1]].point, p1);

                // Adjust for origin-based models
                // technically we should use the other faces g_face_offset entries
                // If they are nonzero, it has to be from the same model with
                // the same offset, so we are cool
                VectorAdd(p0, g_face_offset[facenum], p0);
                VectorAdd(p1, g_face_offset[facenum], p1);

                VectorAdd(p0, delta, wall->vertex[0]);
                VectorAdd(p1, delta, wall->vertex[1]);
                VectorSubtract(p1, delta, wall->vertex[2]);
                VectorSubtract(p0, delta, wall->vertex[3]);

                {
                    vec3_t          delta1;
                    vec3_t          delta2;
                    vec3_t          normal;
                    vec_t           dist;

                    VectorSubtract(wall->vertex[2], wall->vertex[1], delta1);
                    VectorSubtract(wall->vertex[0], wall->vertex[1], delta2);
                    CrossProduct(delta1, delta2, normal);
                    VectorNormalize(normal);
                    dist = DotProduct(normal, p0);

                    VectorCopy(normal, wall->plane.normal);
                    wall->plane.dist = dist;
                }
            }
        }
    }
#endif
}
#endif

// =====================================================================================
//  AllocTriangulation
// =====================================================================================
static lerpTriangulation_t* AllocTriangulation()
{
    lerpTriangulation_t* trian = (lerpTriangulation_t*)calloc(1, sizeof(lerpTriangulation_t));
#ifdef HLRAD_HLASSUMENOMEMORY
	hlassume (trian != NULL, assume_NoMemory);
#endif

    trian->maxpoints = DEFAULT_MAX_LERP_POINTS;
    trian->maxwalls = DEFAULT_MAX_LERP_WALLS;

    trian->points = (patch_t**)calloc(DEFAULT_MAX_LERP_POINTS, sizeof(patch_t*));
#ifdef HLRAD_LERP_TEXNORMAL
	trian->points_pos = (vec3_t *)calloc (DEFAULT_MAX_LERP_POINTS, sizeof(vec3_t));
	hlassume (trian->points_pos != NULL, assume_NoMemory);
#endif

    trian->walls = (lerpWall_t*)calloc(DEFAULT_MAX_LERP_WALLS, sizeof(lerpWall_t));

    hlassume(trian->points != NULL, assume_NoMemory);
    hlassume(trian->walls != NULL, assume_NoMemory);

    return trian;
}

// =====================================================================================
//  FreeTriangulation
// =====================================================================================
void            FreeTriangulation(lerpTriangulation_t* trian)
{
    free(trian->dists);
    free(trian->points);
#ifdef HLRAD_LERP_TEXNORMAL
	free(trian->points_pos);
#endif
    free(trian->walls);
#ifdef HLRAD_LERP_FACELIST
	for (facelist_t *next; trian->allfaces; trian->allfaces = next)
	{
		next = trian->allfaces->next;
		free (trian->allfaces);
	}
#endif
    free(trian);
}

#ifdef HLRAD_LERP_FACELIST
void AddFaceToTrian (facelist_t **faces, dface_t *face)
{
	for (; *faces; faces = &(*faces)->next)
	{
		if ((*faces)->face == face)
		{
			return;
		}
	}
	*faces = (facelist_t *)malloc (sizeof (facelist_t));
	hlassume (*faces != NULL, assume_NoMemory);
	(*faces)->face = face;
	(*faces)->next = NULL;
}
void FindFaces (lerpTriangulation_t *trian)
{
	int i, j;
	AddFaceToTrian (&trian->allfaces, &g_dfaces[trian->facenum]);
	for (j = 0; j < trian->face->numedges; j++)
	{
		int edgenum = g_dsurfedges[trian->face->firstedge + j];
		edgeshare_t *es = &g_edgeshare[abs (edgenum)];
		dface_t *f2;
		if (!es->smooth)
		{
			continue;
		}
		if (edgenum > 0)
		{
			f2 = es->faces[1];
		}
		else
		{
			f2 = es->faces[0];
		}
		AddFaceToTrian (&trian->allfaces, f2);
	}
	for (j = 0; j < trian->face->numedges; j++)
	{
		int edgenum = g_dsurfedges[trian->face->firstedge + j];
		edgeshare_t *es = &g_edgeshare[abs (edgenum)];
		if (!es->smooth)
		{
			continue;
		}
		for (i = 0; i < 2; i++)
		{
			facelist_t *fl;
			for (fl = es->vertex_facelist[i]; fl; fl = fl->next)
			{
				dface_t *f2 = fl->face;
				AddFaceToTrian (&trian->allfaces, f2);
			}
		}
	}
}
#endif
// =====================================================================================
//  CreateTriangulation
// =====================================================================================
lerpTriangulation_t* CreateTriangulation(const unsigned int facenum)
{
    const dface_t*  f = g_dfaces + facenum;
    const dplane_t* p = getPlaneFromFace(f);
    lerpTriangulation_t* trian = AllocTriangulation();
    patch_t*        patch;
    unsigned int    j;
    dface_t*        f2;

    trian->facenum = facenum;
    trian->plane = p;
    trian->face = f;

#ifdef HLRAD_LERP_FACELIST
	FindFaces (trian);
	facelist_t *fl;
	for (fl = trian->allfaces; fl; fl = fl->next)
	{
		f2 = fl->face;
		int facenum2 = fl->face - g_dfaces;
		for (patch = g_face_patches[facenum2]; patch; patch = patch->next)
		{
			AddPatchToTriangulation (trian, patch);
		}
		for (j = 0; j < f2->numedges; j++)
		{
			int edgenum = g_dsurfedges[f2->firstedge + j];
			edgeshare_t *es = &g_edgeshare[abs(edgenum)];
			if (!es->smooth)
			{
				AddWall (trian, g_dvertexes[g_dedges[abs(edgenum)].v[0]].point, g_dvertexes[g_dedges[abs(edgenum)].v[1]].point);
			}
		}
	}
#else
    for (patch = g_face_patches[facenum]; patch; patch = patch->next)
    {
        AddPatchToTriangulation(trian, patch);
    }

    CreateWalls(trian, f);

    for (j = 0; j < f->numedges; j++)
    {
        edgeshare_t*    es;
        int             edgenum = g_dsurfedges[f->firstedge + j];

        if (edgenum > 0)
        {
            es = &g_edgeshare[edgenum];
            f2 = es->faces[1];
        }
        else
        {
            es = &g_edgeshare[-edgenum];
            f2 = es->faces[0];
        }

#ifdef HLRAD_GetPhongNormal_VL
		if (!es->smooth)
#else
        if (!es->coplanar && VectorCompare(vec3_origin, es->interface_normal))
#endif
        {
            continue;
        }

        for (patch = g_face_patches[f2 - g_dfaces]; patch; patch = patch->next)
        {
            AddPatchToTriangulation(trian, patch);
        }
    }
#endif

    trian->dists = (lerpDist_t*)calloc(trian->numpoints, sizeof(lerpDist_t));
#ifdef HLRAD_HULLU
    //Get rid off error that seems to happen with some opaque faces (when opaque face have all edges 'out' of map)
    if(trian->numpoints != 0) // this line should be removed. --vluzacn
#endif
    hlassume(trian->dists != NULL, assume_NoMemory);

    return trian;
}
#endif

