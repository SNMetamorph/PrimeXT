/*
light_decal_bmodel_fp.glsl - fragment uber shader for all dlight types for bmodel decals
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "const.h"
#include "mathlib.h"
#include "texfetch.h"
#include "lightmodel.h"
#include "material.h"

uniform sampler2D	u_DecalMap;
uniform sampler2D	u_ColorMap;	// surface under decal

#if defined( HAS_NORMALMAP )
uniform sampler2D	u_NormalMap;
#endif

#if defined( LIGHT_SPOT )
uniform sampler2D	u_ProjectMap;
#endif

uniform vec4		u_LightDir;
uniform vec3		u_LightDiffuse;
uniform vec4		u_LightOrigin;

varying vec4	var_TexDiffuse;	// xy = decal coords, zw = surface coords
varying vec3	var_LightVec;

#if defined( HAS_NORMALMAP )
varying mat3	var_MatrixTBN;
#else
varying vec3	var_Normal;
#endif

#if defined( LIGHT_SPOT )
varying vec4	var_ProjCoord;
#endif

void main( void )
{
	vec2 vecTexCoord = var_TexDiffuse.xy;
	vec3 L = vec3( 0.0 );
	float atten = 1.0;

#if !defined( LIGHT_PROJ )
	atten = LightAttenuation( var_LightVec, u_LightOrigin.w );
	if( atten <= 0.0 )
		discard; // fast reject
#endif

#if defined( LIGHT_SPOT )
	L = normalize( var_LightVec );

	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos )
		discard;
#elif defined( LIGHT_OMNI )
	L = normalize( var_LightVec );
#elif defined( LIGHT_PROJ )
	L = normalize( u_LightDir.xyz );
#endif

	vec4 decalAlbedo = decalmap2D( u_DecalMap, vecTexCoord );
	vec4 surfAlbedo = colormap2D( u_ColorMap, var_TexDiffuse.zw );

// compute the normal first
#if defined( HAS_NORMALMAP )
	vec3 N = normalmap2D( u_NormalMap, vecTexCoord );
	// transform normal vector to world space
	mat3 tbnBasis = mat3( normalize( var_MatrixTBN[0] ), normalize( var_MatrixTBN[1] ), normalize( var_MatrixTBN[2] ));
	N = normalize( tbnBasis * N );
#else
	vec3 N = normalize( var_Normal );
#endif

	vec3 light = u_LightDiffuse * DLIGHT_SCALE;	// light color
	float NdotL = saturate( dot( N, L ));
	if( NdotL <= 0.0 )
		discard; // fast reject

#if defined( LIGHT_SPOT )
	// texture or procedural spotlight
	light *= textureProj( u_ProjectMap, var_ProjCoord ).rgb;
#endif

	light *= atten;

	// the surface light pass already added the surface's diffuse contribution,
	// so add only the difference the decal introduces on top of the surface
	vec3 delta = ( decalAlbedo.rgb - surfAlbedo.rgb ) * light * NdotL;
	delta *= decalAlbedo.a; // scale by decal coverage

	gl_FragColor = vec4( delta, 1.0 );
}
