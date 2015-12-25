/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef PLANE_H
#define PLANE_H

//=========================================================
// Plane
//=========================================================
typedef int SideType;

#define VP_EPSILON	0.01f


class VPlane
{
public:

	VPlane();
	VPlane(const Vector &vNormal, float dist);

	void Init(const Vector &vNormal, float dist);
          void Init(const Vector &vNormal, const Vector &vecPoint);
	
	// Return the distance from the point to the plane.
	float	DistTo(const Vector &vVec) const;

	// Flip the plane.
	VPlane		Flip();

	// Get a point on the plane (normal*dist).
	Vector		GetPointOnPlane() const;

	// Copy.
	VPlane&		operator=(const VPlane &thePlane);

	// Snap the specified point to the plane (along the plane's normal).
	Vector		SnapPointToPlane(const Vector &vPoint) const;

	// Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK.
	// The epsilon for SIDE_ON can be passed in.
	SideType	GetPointSide(const Vector &vPoint, float sideEpsilon=VP_EPSILON) const;

	// Returns SIDE_FRONT or SIDE_BACK.
	SideType	GetPointSideExact(const Vector &vPoint) const;

	// Classify the box with respect to the plane.
	// Returns SIDE_ON, SIDE_FRONT, or SIDE_BACK
	SideType	BoxOnPlaneSide(const Vector &vMin, const Vector &vMax) const;


public:

	Vector		m_Normal;
	float		m_Dist;
};


// ------------------------------------------------------------------------------------------- //
// Inlines.
// ------------------------------------------------------------------------------------------- //

inline VPlane::VPlane()
{
}

inline VPlane::VPlane(const Vector &vNormal, float dist)
{
	m_Normal = vNormal;
	m_Dist = dist;
}

inline void VPlane::Init(const Vector &vNormal, float dist)
{
	m_Normal = vNormal;
	m_Dist = dist;
}

inline void VPlane::Init(const Vector &vNormal, const Vector &vecPoint)
{
	m_Normal = vNormal;
	m_Dist = vNormal.Dot( vecPoint );
}

inline float VPlane::DistTo(const Vector &vVec) const
{	
	return DotProduct( vVec, m_Normal ) - m_Dist;
}

inline VPlane VPlane::Flip()
{
	return VPlane(-m_Normal, -m_Dist);
}

inline Vector VPlane::GetPointOnPlane() const
{
	return m_Normal * m_Dist;
}

inline VPlane& VPlane::operator=(const VPlane &thePlane)
{
	m_Normal = thePlane.m_Normal;
	m_Dist = thePlane.m_Dist;
	return *this;
}

inline Vector VPlane::SnapPointToPlane(const Vector &vPoint) const
{
	return vPoint - m_Normal * DistTo(vPoint);
}

inline SideType VPlane::GetPointSide(const Vector &vPoint, float sideEpsilon) const
{
	float fDist;

	fDist = DistTo(vPoint);
	if(fDist >= sideEpsilon)
		return SIDE_FRONT;
	else if(fDist <= -sideEpsilon)
		return SIDE_BACK;
	else	return SIDE_ON;
}

inline SideType VPlane::GetPointSideExact(const Vector &vPoint) const
{
	return DistTo(vPoint) > 0.0f ? SIDE_FRONT : SIDE_BACK;
}

inline SideType VPlane::BoxOnPlaneSide(const Vector &vMin, const Vector &vMax) const
{
	int i, firstSide, side;
	Vector vPoints[8] = 
	{
		Vector(vMin.x, vMin.y, vMin.z),
		Vector(vMin.x, vMin.y, vMax.z),
		Vector(vMin.x, vMax.y, vMax.z),
		Vector(vMin.x, vMax.y, vMin.z),

		Vector(vMax.x, vMin.y, vMin.z),
		Vector(vMax.x, vMin.y, vMax.z),
		Vector(vMax.x, vMax.y, vMax.z),
		Vector(vMax.x, vMax.y, vMin.z),
	};

	firstSide = GetPointSideExact(vPoints[0]);
	for(i=1; i < 8; i++)
	{
		side = GetPointSideExact(vPoints[i]);

		// Does the box cross the plane?
		if(side != firstSide)
			return SIDE_ON;
	}

	// Ok, they're all on the same side, return that.
	return firstSide;
}


#endif // PLANE_H

