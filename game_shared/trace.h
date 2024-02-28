/*
trace.h - trace triangle meshes
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef TRACEMESH_H
#define TRACEMESH_H

#include "vector.h"
#include "meshdesc.h"
#include "material.h"

#define FRAC_EPSILON	(1.0f / 32.0f)
#define BARY_EPSILON	0.01f
#define ASLF_EPSILON	0.0001f
#define COPLANAR_EPSILON	0.25f

class TraceMesh
{
private:
	Vector		m_vecSrcStart, m_vecSrcEnd;
	Vector		m_vecAbsMins, m_vecAbsMaxs;
	Vector		m_vecStart, m_vecEnd;
	vec3_t		m_vecTraceDirection;// ray direction
	Vector		m_vecOffsets[8];	// for fast signbits tests
	float		m_flSphereRadius;	// capsule
	Vector		m_flSphereOffset;
	float		m_flTraceDistance;
	float		m_flRealFraction;
	bool		m_bHitTriangle;	// now we hit triangle
	bool		bIsTestPosition;
	bool		bIsTraceLine;	// more accurate than ClipBoxToFacet
	bool		bUseCapsule;	// use capsule instead of bbox
	areanode_t	*areanodes;	// AABB for static meshes
	mmesh_t		*mesh;		// mesh to trace
	trace_t  	*trace;		// output
	matdesc_t	*material;	// pointer to texture for special effects
	int			checkcount;	// debug
	Vector		m_vecOrigin;
	Vector		m_vecAngles;
	Vector		m_vecScale;
	int32_t		m_bodyNumber;
	int32_t		m_skinNumber;
	matrix4x4	m_transform;
	model_t		*m_pModel;

public:
	TraceMesh() { mesh = NULL; }
	~TraceMesh() {}

	// trace stuff
	void SetTraceMesh( mmesh_t *cached_mesh, areanode_t *tree, int modelindex, int32_t body, int32_t skin );
	void SetMeshOrientation( const Vector &pos, const Vector &ang, const Vector &xform )
	{
		m_vecOrigin = pos, m_vecAngles = ang, m_vecScale = xform;
	}

	void SetupTrace( const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *trace ); 
	matdesc_t *GetLastHitSurface( void ) { return material; }
	matdesc_t *GetMaterialForFacet( const mfacet_t *facet );
	mstudiotexture_t *GetTextureForFacet( const mfacet_t *facet );
	bool ClipRayToFacet( const mfacet_t *facet );
	void ClipBoxToFacet( mfacet_t	*facet );
	void TestBoxInFacet( mfacet_t	*facet );
	bool IsTrans( const mfacet_t *facet );
	void ClipToLinks( areanode_t *node );
	bool DoTrace( void );
};

#endif//TRACEMESH_H