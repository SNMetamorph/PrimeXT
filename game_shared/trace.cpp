/*
trace.cpp - trace triangle meshes
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

#include <alert.h>
#include "vector.h"
#include "matrix.h"
#include "const.h"
#include "com_model.h"
#include "trace.h"
#include "mathlib.h"
#include "stringlib.h"

#ifdef CLIENT_DLL
#include "utils.h"
#include "gl_local.h"
#include "render_api.h"
#else
#include "edict.h"
#include "eiface.h"
#include "physcallback.h"

extern globalvars_t *gpGlobals;
#endif

#include "enginecallback.h"

// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define	SURFACE_CLIP_EPSILON	(0.125)

void TraceMesh :: SetTraceMesh( mmesh_t *cached_mesh, areanode_t *tree, int modelindex, int32_t body, int32_t skin )
{
	m_pModel = (model_t *)MODEL_HANDLE( modelindex );
	mesh = cached_mesh;
	areanodes = tree;
	m_bodyNumber = body;
	m_skinNumber = skin;
}

matdesc_t *TraceMesh :: GetMaterialForFacet( const mfacet_t *facet )
{
	mstudiotexture_t *texture = GetTextureForFacet(facet);
	if (texture)
	{
		char materialName[64];
		char modelName[64];
		char textureName[64];
		COM_FileBase(m_pModel->name, modelName);
		COM_FileBase(texture->name, textureName);
		snprintf(materialName, sizeof(materialName) - 1, "%s/%s", modelName, textureName);
		return COM_FindMaterial(materialName);
	}
	return nullptr;
}

mstudiotexture_t *TraceMesh :: GetTextureForFacet( const mfacet_t *facet )
{
	if (!m_pModel || facet->skinref < 0)
		return nullptr;

	studiohdr_t *phdr = (studiohdr_t *)m_pModel->cache.data;
	mstudiotexture_t *textures = (mstudiotexture_t *)((byte *)phdr + phdr->textureindex);
	if (phdr && textures)
	{
		if (phdr->numtextures != 0 && phdr->textureindex != 0)
		{
			short *pskinref = (short *)((byte *)phdr + phdr->skinindex);
			if (m_skinNumber > 0 && m_skinNumber < phdr->numskinfamilies)
				pskinref += (m_skinNumber * phdr->numskinref);

			return &textures[pskinref[facet->skinref]];
		}
	}
	return nullptr;
}

bool TraceMesh :: IsTrans( const mfacet_t *facet )
{
	mstudiotexture_t *texture = GetTextureForFacet( facet );

	return (texture && FBitSet( texture->flags, STUDIO_NF_MASKED ));
}

void CheckAngles( Vector &angles )
{
	// blackmagic to avoid invalid signbits state
	if( angles.y != 0.0f && fmod( angles.y, 45.0f ) == 0.0f )
		angles.y += 0.01f;
}

void TraceMesh :: SetupTrace( const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	trace = tr;
	memset( trace, 0, sizeof( *trace ));
	trace->fraction = m_flRealFraction = 1.0f;
	Vector lmins = mins, lmaxs = maxs, offset;
	Vector adjustedStart, adjustedEnd;
	float t, halfwidth, halfheight;
	int i, total_signbits = 0;

	m_vecSrcStart = start;
 	m_vecSrcEnd = end;

	// adjust so that mins and maxs are always symetric, which
	// avoids some complications with plane expanding of rotated
	// bmodels
	for( i = 0; i < 3; i++ )
	{
		offset[i] = ( mins[i] + maxs[i] ) * 0.5f;
		lmins[i] = mins[i] - offset[i];
		lmaxs[i] = maxs[i] - offset[i];
		adjustedStart[i] = start[i] + offset[i];
		adjustedEnd[i] = end[i] + offset[i];
	}

	if (mins != maxs) 
	{
		// HACKHACK: shrink bbox a bit...
		ExpandBounds(lmins, lmaxs, -(1.0f / 8.0f));
	}
	else
	{
		// just reset offsets
		memset(m_vecOffsets, 0, sizeof(m_vecOffsets));
	}

	halfwidth = lmaxs[0];
	halfheight = lmaxs[2];

	bUseCapsule = (offset == g_vecZero) ? false : true;
	m_flSphereRadius = ( halfwidth > halfheight ) ? halfheight : halfwidth;
	t = halfheight - m_flSphereRadius;

	CheckAngles( m_vecAngles );

	m_transform = matrix4x4( m_vecOrigin, Vector( m_vecAngles.x, m_vecAngles.y, m_vecAngles.z ), m_vecScale ).InvertFull();
	m_vecStart = m_transform.VectorTransform( adjustedStart );
	m_vecEnd = m_transform.VectorTransform( adjustedEnd );

	if( mins != maxs )
	{
		// compute a full bounding box
		for( i = 0; i < 8; i++ )
		{
			Vector p1, p2;
			p1.x = ( i & 1 ) ? lmins[0] : lmaxs[0];
			p1.y = ( i & 2 ) ? lmins[1] : lmaxs[1];
			p1.z = ( i & 4 ) ? lmins[2] : lmaxs[2];

			p2 = m_transform.VectorRotate( p1 );
			// NOTE: this is looks silly but it works for some reasons:
			// bbox are symetric and stored in local space
			// signbits are detected normals for bbox side tests
			int j = SignbitsForPlane( -p2 );
			m_vecOffsets[j] = p2;
			total_signbits += j;
		}

		if( total_signbits != 28 )
		{
			ALERT( at_error, "total signbits %d != 28 (mins %g %g %g) maxs( %g %g %g)\n",
			total_signbits, lmins.x, lmins.y, lmins.z, lmaxs.x, lmaxs.y, lmaxs.z );
			ALERT( at_error, "rotated angles %g %g %g\n", m_vecAngles.x, m_vecAngles.y, m_vecAngles.z );

			for( i = 0; i < 8; i++ )
			{
				Vector p1, p2;
				p1.x = ( i & 1 ) ? lmins[0] : lmaxs[0];
				p1.y = ( i & 2 ) ? lmins[1] : lmaxs[1];
				p1.z = ( i & 4 ) ? lmins[2] : lmaxs[2];

				p2 = m_transform.VectorRotate( p1 );
				// NOTE: this is looks silly but it works for some reasons:
				// bbox are symetric and stored in local space
				// signbits are detected normals for bbox side tests
				int j = SignbitsForPlane( -p2 );
				Msg( "#%i (%g %g %g) -> (%g %g %g), signbits %d\n", i, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, j );
			}
		}
	}
	else
	{
		// just reset offsets
		memset( m_vecOffsets, 0, sizeof( m_vecOffsets ));
	}

	// rotated sphere offset for capsule
	m_flSphereOffset[0] = m_transform[2][0] * t;
	m_flSphereOffset[1] = -m_transform[2][1] * t;
	m_flSphereOffset[2] = m_transform[2][2] * t;

	// now calculate the local bbox
	ClearBounds( lmins, lmaxs );
	for( i = 0; i < 8; i++ )
		AddPointToBounds( m_vecOffsets[i], lmins, lmaxs );

	material = NULL;
	m_vecTraceDirection = m_vecEnd - m_vecStart;
	m_flTraceDistance = m_vecTraceDirection.Length();
	m_vecTraceDirection = m_vecTraceDirection.Normalize();

	// build a bounding box of the entire move
	ClearBounds( m_vecAbsMins, m_vecAbsMaxs );
	AddPointToBounds( m_vecStart + lmins, m_vecAbsMins, m_vecAbsMaxs );
	AddPointToBounds( m_vecStart + lmaxs, m_vecAbsMins, m_vecAbsMaxs );
	AddPointToBounds( m_vecEnd + lmins, m_vecAbsMins, m_vecAbsMaxs );
	AddPointToBounds( m_vecEnd + lmaxs, m_vecAbsMins, m_vecAbsMaxs );

	// spread min\max by a pixel
	for( i = 0; i < 3; i++ )
	{
		m_vecAbsMins[i] -= 1.0f;
		m_vecAbsMaxs[i] += 1.0f;
	}

	// use untransformed values to avoid FP rounding errors
	if( mins == maxs )
		bIsTraceLine = true;
	else bIsTraceLine = false;

	if( start == end )
		bIsTestPosition = true;
	else bIsTestPosition = false;
}

void TraceMesh :: ClipBoxToFacet( mfacet_t *facet )
{
	mplane_t	*p, *clipplane;
	float	enterfrac, leavefrac;
	mstudiotexture_t *ptexture;
	bool	getout, startout;
	Vector	startp, endp;
	float	d, d1, d2, f;

	if( !facet->numplanes )
		return;

	ptexture = GetTextureForFacet( facet );

	if( !bIsTraceLine && ptexture )
	{
		if( FBitSet( ptexture->flags, STUDIO_NF_MASKED ) && !FBitSet( ptexture->flags, STUDIO_NF_SOLIDGEOM ))
			return;
	}

	enterfrac = -1.0f;
	leavefrac = 1.0f;
	clipplane = NULL;
	checkcount++;

	getout = false;
	startout = false;

	for( int i = 0; i < facet->numplanes; i++ )
	{
		p = &mesh->planes[facet->indices[i]];

		if( bUseCapsule )
		{
			// adjust the plane distance apropriately for radius
			float dist = p->dist + m_flSphereRadius;
			// find the closest point on the capsule to the plane
			float t = DotProduct( p->normal, m_flSphereOffset );

			if( t > 0.0f )
			{
				startp = m_vecStart - m_flSphereOffset;
				endp = m_vecEnd - m_flSphereOffset;
			}
			else
			{
				startp = m_vecStart + m_flSphereOffset;
				endp = m_vecEnd + m_flSphereOffset;
			}

			d1 = DotProduct( startp, p->normal ) - dist;
			d2 = DotProduct( endp, p->normal ) - dist;
		}
		else
		{
			// adjust the plane distance apropriately for mins/maxs
			float dist = p->dist - DotProduct( m_vecOffsets[p->signbits], p->normal );

			d1 = DotProduct( m_vecStart, p->normal ) - dist;
			d2 = DotProduct( m_vecEnd, p->normal ) - dist;
		}

		if( d2 > 0.0f ) getout = true;	// endpoint is not in solid
		if( d1 > 0.0f ) startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1))
			return;

		// if it doesn't cross the plane, the plane isn't relevent
		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		if (d1 > d2) {	// enter
			f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
			if (f < 0) {
				f = 0;
			}
			if (f > enterfrac) {
				enterfrac = f;
				clipplane = p;
			}
		}
		else {	// leave
			f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
			if (f > 1) {
				f = 1;
			}
			if (f < leavefrac) {
				leavefrac = f;
			}
		}
	}

	if( !startout )
	{
		// original point was inside brush
		trace->startsolid = true;
		if( !getout ) trace->allsolid = true;
		return;
	}

	if( enterfrac - FRAC_EPSILON <= leavefrac )
	{
		if( enterfrac > -1.0f && enterfrac < m_flRealFraction )
		{
			if( enterfrac < 0 ) enterfrac = 0;
			m_flRealFraction = enterfrac;
			trace->plane.normal = clipplane->normal;
			trace->plane.dist = clipplane->dist;
			trace->fraction = enterfrac;
			material = GetMaterialForFacet( facet ); // material was hit
		}
	}
}

void TraceMesh :: TestBoxInFacet( mfacet_t *facet )
{
	mstudiotexture_t *ptexture;
	Vector	startp;
	float	d1;
	mplane_t	*p;

	if( !facet->numplanes )
		return;

	ptexture = GetTextureForFacet( facet );

	if( !bIsTraceLine && ptexture )
	{
		if( FBitSet( ptexture->flags, STUDIO_NF_MASKED ) && !FBitSet( ptexture->flags, STUDIO_NF_SOLIDGEOM ))
			return;
	}

	checkcount++;

	for( int i = 0; i < facet->numplanes; i++ )
	{
		p = &mesh->planes[facet->indices[i]];

		if( bUseCapsule )
		{
			// adjust the plane distance apropriately for radius
			float dist = p->dist + m_flSphereRadius;
			// find the closest point on the capsule to the plane
			float t = DotProduct( p->normal, m_flSphereOffset );
			if( t > 0.0f ) startp = m_vecStart - m_flSphereOffset;
			else startp = m_vecStart + m_flSphereOffset;
			d1 = DotProduct( startp, p->normal ) - dist;
		}
		else
		{
			// adjust the plane distance apropriately for mins/maxs
			float dist = p->dist - DotProduct( m_vecOffsets[p->signbits], p->normal );
			d1 = DotProduct( m_vecStart, p->normal ) - dist;
		}

		// if completely in front of face, no intersection
		if( d1 > 0 ) return;
	}

	// inside this brush
	m_flRealFraction = 0.0f;
	trace->startsolid = true;
	trace->allsolid = true;
}

bool TraceMesh :: ClipRayToFacet( const mfacet_t *facet )
{
	// begin calculating determinant - also used to calculate u parameter
	Vector pvec = CrossProduct( m_vecTraceDirection, facet->edge2 );

	// if determinant is near zero, trace lies in plane of triangle
	float det = DotProduct( facet->edge1, pvec );

	// the non-culling branch
	if( fabs( det ) < COPLANAR_EPSILON )
		return false;

	checkcount++;

	Vector n = CrossProduct( facet->edge2, facet->edge1 );
	Vector p = m_vecEnd - m_vecStart;

	// calculate distance from first vertex to ray origin
	Vector tvec = m_vecStart - facet->triangle[0].point;

	float d1 = -DotProduct( n, tvec );
	float d2 = DotProduct( n, p );

	if( fabs( d2 ) < 0.0001 )
		return false;

	// get intersect point of ray with triangle plane
	float frac = d1 / d2;

	if( frac <= 0.0f ) return false;
	if( frac > m_flRealFraction )
		return false; // we have hit something earlier

	float invDet = 1.0f / det;
	bool force_solid = false;

	// calculate u parameter and test bounds
	float u = DotProduct( tvec, pvec ) * invDet;
	if( u < -BARY_EPSILON || u > ( 1.0f + BARY_EPSILON ))
		return false;

	// prepare to test v parameter
	Vector qvec = CrossProduct( tvec, facet->edge1 );

	// calculate v parameter and test bounds
	float v = DotProduct( m_vecTraceDirection, qvec ) * invDet;
	if( v < -BARY_EPSILON || ( u + v ) > ( 1.0f + BARY_EPSILON ))
		return false;

	// calculate t (depth)
	float depth = DotProduct( facet->edge2, qvec ) * invDet;
	if( depth <= 0.001f || depth >= m_flTraceDistance )
		return false;

	n = n.Normalize();

	mstudiotexture_t *ptexture = GetTextureForFacet( facet );

	if( ptexture && !FBitSet( ptexture->flags, STUDIO_NF_MASKED ))
		force_solid = true;

#ifdef CLIENT_DLL
	// FIXME: how to handle trace flags on the client?
#else
	if( FBitSet( gpGlobals->trace_flags, FTRACE_IGNORE_ALPHATEST ))
		force_solid = true;
#endif
	// most surfaces are completely opaque
	if( !ptexture || !ptexture->index || force_solid )
	{
		trace->fraction = m_flRealFraction = frac;
		material = GetMaterialForFacet( facet ); // material was hit

		if( DotProduct( m_vecTraceDirection, n ) >= 0.0f )
			trace->plane.normal = -n;
		else trace->plane.normal = n;

		return true;
	}

	// try to avoid double shadows near triangle seams
	if( u < -ASLF_EPSILON || u > ( 1.0f + ASLF_EPSILON ) || v < -ASLF_EPSILON || ( u + v ) > ( 1.0f + ASLF_EPSILON ))
		return false;

	// calculate w parameter
	float w = 1.0f - ( u + v );

	// calculate st from uvw (barycentric) coordinates
	float s = w * facet->triangle[0].st[0] + u * facet->triangle[1].st[0] + v * facet->triangle[2].st[0];
	float t = w * facet->triangle[0].st[1] + u * facet->triangle[1].st[1] + v * facet->triangle[2].st[1];
	s = s - floor( s );
	t = t - floor( t );

	int is = s * ptexture->width;
	int it = t * ptexture->height;

	if( is < 0 ) is = 0;
	if( it < 0 ) it = 0;

	if( is > ptexture->width - 1 )
		is = ptexture->width - 1;
	if( it > ptexture->height - 1 )
		it = ptexture->height - 1;

	// TODO this should be refactored to use texture pixels DIRECTLY from studiomodel 
	// structure, without relying on physics/client APIs since it's not reliable
	// also correct implementation will work fully similar, both on client and server
	byte *pixels = nullptr;

	// test pixel
	if( pixels && pixels[it * ptexture->width + is] == 0xFF )
		return false; // last color in palette is indicated alpha-pixel

	trace->fraction = m_flRealFraction = frac;
	material = GetMaterialForFacet( facet ); // material was hit

	if( DotProduct( m_vecTraceDirection, n ) >= 0.0f )
		trace->plane.normal = -n;
	else trace->plane.normal = n;

	return true;
}

void TraceMesh :: ClipToLinks( areanode_t *node )
{
	link_t	*l, *next = nullptr;
	mfacet_t	*facet;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		facet = FACET_FROM_AREA( l );

		if( !BoundsIntersect( m_vecAbsMins, m_vecAbsMaxs, facet->mins, facet->maxs ))
			continue;

		// might intersect, so do an exact clip
		if( !m_flRealFraction ) return;

		if( bIsTestPosition )
			TestBoxInFacet( facet );
		else if( bIsTraceLine )
			ClipRayToFacet( facet );
		else ClipBoxToFacet( facet );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( m_vecAbsMaxs[node->axis] > node->dist )
		ClipToLinks( node->children[0] );
	if( m_vecAbsMins[node->axis] < node->dist )
		ClipToLinks( node->children[1] );
}

bool TraceMesh :: DoTrace( void )
{
	if( !mesh || !BoundsIntersect( mesh->mins, mesh->maxs, m_vecAbsMins, m_vecAbsMaxs ))
		return false; // invalid mesh or no intersection

	checkcount = 0;

	if( areanodes )
	{
		ClipToLinks( areanodes );
	}
	else
	{
		mfacet_t *facet = mesh->facets;
		for( int i = 0; i < mesh->numfacets; i++, facet++ )
		{
			if( bIsTestPosition )
				TestBoxInFacet( facet );
			else if( bIsTraceLine )
				ClipRayToFacet( facet );
			else ClipBoxToFacet( facet );

			if( !m_flRealFraction )
				break;
		}
	}

//	ALERT( at_aiconsole, "total %i checks for %s\n", checkcount, areanodes ? "tree" : "brute force" );

	trace->plane.normal = m_transform.VectorIRotate( trace->plane.normal ).Normalize();
	trace->fraction = bound( 0.0f, trace->fraction, 1.0f );
	if( trace->fraction == 1.0f ) trace->endpos = m_vecSrcEnd;
	else VectorLerp( m_vecSrcStart, trace->fraction, m_vecSrcEnd, trace->endpos );
	trace->plane.dist = DotProduct( trace->endpos, trace->plane.normal );

	return (trace->fraction != 1.0f);
}