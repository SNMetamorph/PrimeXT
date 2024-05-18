/*
cl_dlight.h - dynamic lighting description
this code written for Paranoia 2: Savior modification
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef CL_DLIGHT_H
#define CL_DLIGHT_H
#include "enginecallback.h"
#include "vector.h"
#include "mathlib.h"
#include "texture_handle.h"

#define NUM_SHADOW_SPLITS	3 // four splits
#define MAX_SHADOWMAPS	(NUM_SHADOW_SPLITS + 1)

// dlight flags
#define DLF_NOSHADOWS				BIT( 0 )
#define DLF_NOBUMP					BIT( 1 )
#define DLF_LENSFLARE				BIT( 2 )
#define DLF_CULLED					BIT( 3 )		// light culled by scissor
#define DLF_ASPECT3X4				BIT( 4 )
#define DLF_ASPECT4X3				BIT( 5 )
#define DLF_FLIPTEXTURE				BIT( 6 )
#define DLF_PARENTENTITY_NOSHADOW	BIT( 7 )

class CDynLight
{
public:
	Vector		origin;
	Vector		angles;
	float		radius;
	Vector		color;				// ignored for spotlights, they have a texture
	float		die;				// stop lighting after this time
	float		decay;				// drop this each second
	int			key;
	int			type;				// light type
	int			lightstyleIndex;
	bool		update;				// light needs update
	cl_entity_t *parentEntity;

	matrix4x4		viewMatrix;
	matrix4x4		projectionMatrix;			// light projection matrix
	matrix4x4		modelviewMatrix;			// light modelview
	matrix4x4		lightviewProjMatrix;		// lightview projection
	matrix4x4		textureMatrix[MAX_SHADOWMAPS];	// result texture matrix	
	matrix4x4		shadowMatrix[MAX_SHADOWMAPS];		// result texture matrix	
	GLfloat		gl_shadowMatrix[MAX_SHADOWMAPS][16];	// cached matrices

	Vector		mins, maxs;	// local bounds
	Vector		absmin, absmax;	// world bounds
	CFrustum		frustum;		// normal frustum
	CFrustum		splitFrustum[MAX_SHADOWMAPS];

	// scissor data
	float		x, y, w, h;

	// spotlight specific:
	TextureHandle	spotlightTexture; // spotlights only
	TextureHandle	shadowTexture[MAX_SHADOWMAPS]; // shadowmap for this light
	int		cinTexturenum; // not gltexturenum!
	int		lastframe;			// cinematic lastframe
	int		flags;
	float	fov;

	// TODO move it to cl_dlight.cpp
	bool Expired( void )
	{
		if( die < GET_CLIENT_TIME( ))
			return true;
		if( radius <= 0.0f )
			return true;
		return false;
	}

	bool Active( void )
	{
		if( Expired( ))
			return false;
		if( FBitSet( flags, DLF_CULLED ))
			return false;
		return true;
	}
};

#endif//CL_DLIGHT_H