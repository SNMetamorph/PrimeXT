/*
BmodelSolid_fp.glsl - fragment uber shader for all solid bmodel surfaces
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
#include "terrain.h"

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_ColorMap;
#else
uniform sampler2D		u_ColorMap;
#endif
uniform sampler2D		u_DetailMap;
uniform sampler2D		u_LightMap;
uniform sampler2D		u_ScreenMap;	// screen copy

#if defined( BMODEL_MULTI_LAYERS )
uniform sampler2DArray	u_HeightMap;
#endif
uniform sampler2D		u_GlowMap;

uniform vec4		u_RenderColor;
uniform vec4		u_FogParams;
uniform vec2		u_ScreenSizeInv;

// shared variables
varying vec2		var_TexDiffuse;
varying vec3		var_TexLight0;
varying vec3		var_TexLight1;
varying vec3		var_TexLight2;
varying vec3		var_TexLight3;
varying vec2		var_TexDetail;

#if defined( BMODEL_REFLECTION_PLANAR )
varying vec4		var_TexMirror;	// mirror coords
#endif

varying vec2		var_TexGlobal;

void main( void )
{
#if defined( BMODEL_MULTI_LAYERS )
	vec4 mask0, mask1 = vec4( 0.0 ), mask2 = vec4( 0.0 ), mask3 = vec4( 0.0 );

	mask0 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 0.0 ));
#if TERRAIN_NUM_LAYERS >= 4
	mask1 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 1.0 ));
#endif
#if TERRAIN_NUM_LAYERS >= 8
	mask2 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 2.0 ));
#endif
#if TERRAIN_NUM_LAYERS >= 12
	mask3 = texture2DArray( u_HeightMap, vec3( var_TexGlobal, 3.0 ));
#endif
#endif
	// compute the diffuse term
#if defined( BMODEL_REFLECTION_PLANAR )
	vec4 diffuse = texture2DProj( u_ColorMap, var_TexMirror );
#elif defined( BMODEL_MULTI_LAYERS )
	vec4 diffuse = TerrainApplyDiffuse( u_ColorMap, var_TexDiffuse, mask0, mask1, mask2, mask3 );
#else
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );
#endif
	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor

#if defined( BMODEL_MONOCHROME )
	diffuse.rgb = vec3( GetLuminance( diffuse.rgb )); 
#endif
	// apply the detail texture
#if defined( BMODEL_HAS_DETAIL )
	diffuse.rgb *= texture2D( u_DetailMap, var_TexGlobal ).rgb * DETAIL_SCALE;
#endif
#if defined( BMODEL_DRAWTURB ) && defined( BMODEL_REFLECTION_PLANAR )
	vec3 screenmap = texture2D( u_ScreenMap, var_TexDiffuse ).rgb;
	diffuse.rgb = Q_mix( screenmap, diffuse.rgb, u_RenderColor.a );
#endif
	vec3 light = vec3( 0.0 ); // completely black if have no lightmaps

	// lighting the world polys
#if defined( BMODEL_APPLY_STYLE0 )
	light += texture2D( u_LightMap, var_TexLight0.xy ).rgb * var_TexLight0.z;
#endif

#if defined( BMODEL_APPLY_STYLE1 )
	light += texture2D( u_LightMap, var_TexLight1.xy ).rgb * var_TexLight1.z;
#endif

#if defined( BMODEL_APPLY_STYLE2 )
	light += texture2D( u_LightMap, var_TexLight2.xy ).rgb * var_TexLight2.z;
#endif

#if defined( BMODEL_APPLY_STYLE3 )
	light += texture2D( u_LightMap, var_TexLight3.xy ).rgb * var_TexLight3.z;
#endif

#if defined( BMODEL_APPLY_STYLE0 ) || defined( BMODEL_APPLY_STYLE1 ) || defined( BMODEL_APPLY_STYLE2 ) || defined( BMODEL_APPLY_STYLE3 )
	light = min(( light * LIGHTMAP_SHIFT ), 1.0 );
#endif

#if !defined( BMODEL_FULLBRIGHT )
	diffuse.rgb *= light;
#endif

#if defined( BMODEL_TRANSLUCENT )
	vec3 screenmap = texture2D( u_ScreenMap, gl_FragCoord.xy * u_ScreenSizeInv ).rgb;
	diffuse.rgb = Q_mix( screenmap, diffuse.rgb, u_RenderColor.a );
#endif

	// apply fullbright pixels
#if defined( BMODEL_HAS_LUMA )
	diffuse.rgb += texture2D( u_GlowMap, var_TexDiffuse ).rgb;
#endif

#if defined( BMODEL_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif//BMODEL_FOG_EXP

	gl_FragColor = vec4( diffuse.rgb, diffuse.a * u_RenderColor.a );
}