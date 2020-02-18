/*
BmodelDecal_fp.glsl - fragment uber shader for bmodel decals
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

uniform sampler2D		u_DecalMap;
uniform sampler2D		u_ColorMap;	// surface under decal
uniform sampler2D		u_LightMap;

uniform vec4		u_FogParams;

varying vec4		var_TexDiffuse;
varying vec3		var_TexLight0;
varying vec3		var_TexLight1;
varying vec3		var_TexLight2;
varying vec3		var_TexLight3;

void main( void )
{
	vec3 light = vec3( 0.0 ); // completely black if have no lightmaps

#if defined( DECAL_ALPHATEST )
	if( texture2D( u_ColorMap, var_TexDiffuse.zw ).a <= BMODEL_ALPHA_THRESHOLD )
		discard;
#endif
	vec4 diffuse = texture2D( u_DecalMap, var_TexDiffuse.xy );

	// lighting the world polys
#if defined( DECAL_APPLY_STYLE0 )
	light += texture2D( u_LightMap, var_TexLight0.xy ).rgb * var_TexLight0.z;
#endif

#if defined( DECAL_APPLY_STYLE1 )
	light += texture2D( u_LightMap, var_TexLight1.xy ).rgb * var_TexLight1.z;
#endif

#if defined( DECAL_APPLY_STYLE2 )
	light += texture2D( u_LightMap, var_TexLight2.xy ).rgb * var_TexLight2.z;
#endif

#if defined( DECAL_APPLY_STYLE3 )
	light += texture2D( u_LightMap, var_TexLight3.xy ).rgb * var_TexLight3.z;
#endif

#if defined( DECAL_APPLY_STYLE0 ) || defined( DECAL_APPLY_STYLE1 ) || defined( DECAL_APPLY_STYLE2 ) || defined( DECAL_APPLY_STYLE3 )
	light = min(( light * LIGHTMAP_SHIFT ), 1.0 );
#endif

#if !defined( DECAL_FULLBRIGHT )
	diffuse.rgb *= light;
#endif

#if defined( DECAL_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif
	gl_FragColor = diffuse;
}