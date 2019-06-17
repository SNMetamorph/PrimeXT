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

#define FRAC_EPSILON	(1.0f / 32.0f)

class TraceMesh
{
private:
	Vector		m_vecStart, m_vecEnd;
	Vector		m_vecStartMins, m_vecEndMins;
	Vector		m_vecStartMaxs, m_vecEndMaxs;
	Vector		m_vecAbsMins, m_vecAbsMaxs;
	float		m_flRealFraction;
	bool		bIsTestPosition;
	areanode_t	*areanodes;	// AABB for static meshes
	mmesh_t		*mesh;		// mesh to trace
	trace_t  		*trace;		// output
	int		checkcount;	// debug
public:
	TraceMesh() { mesh = NULL; }
	~TraceMesh() {}

	// trace stuff
	void SetTraceMesh( mmesh_t *cached_mesh, areanode_t *tree ) { mesh = cached_mesh; areanodes = tree; }
	void SetupTrace( const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *trace ); 
	void ClipBoxToFacet( mfacet_t	*facet );
	void TestBoxInFacet( mfacet_t	*facet );
	void ClipToLinks( areanode_t *node );
	bool DoTrace( void );
};

#endif//TRACEMESH_H
