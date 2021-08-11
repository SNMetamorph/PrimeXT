/*
scene_studio_fp.glsl - vertex uber shader for studio meshes with static lighting
Copyright (C) 2019 Uncle Mike

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
#include "deluxemap.h"	// directional lightmap routine
#include "cubemap.h"
#include "screen.h"

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;
uniform sampler2D	u_GlossMap;
uniform sampler2D	u_GlowMap;
uniform sampler2D	u_DetailMap;

uniform vec4	u_RenderColor;
uniform float	u_Smoothness;
uniform float	u_ReflectScale;
uniform vec4	u_FogParams;
uniform vec3	u_ViewOrigin;
uniform vec2	u_LightShade;

varying vec2	var_TexDiffuse;
varying vec2	var_TexDetail;
varying vec3	var_AmbientLight;
varying vec3	var_DiffuseLight;

#if defined (SURFACE_LIGHTING)
varying vec3	var_TexLight0;
varying vec3	var_TexLight1;
varying vec3	var_TexLight2;
varying vec3	var_TexLight3;
#endif

#if defined( REFLECTION_CUBEMAP )
varying vec3	var_WorldNormal;
varying vec3	var_Position;
#endif

varying vec3	var_LightDir;
varying vec3	var_ViewDir;
varying vec3	var_Normal;

void main( void )
{
	// compute the diffuse term
	vec4 diffuse = colormap2D( u_ColorMap, var_TexDiffuse );
	vec3 specular = vec3( 0.0 );
	vec4 glossmap = vec4( 0.0 );
	vec3 light = vec3( 0.0 );
	vec3 gloss = vec3( 0.0 );

#if defined( HAS_DETAIL )
	diffuse.rgb *= detailmap2D( u_DetailMap, var_TexDetail ).rgb * DETAIL_SCALE;
#endif
	// kRenderTransColor support
	diffuse.rgb *= u_RenderColor.rgb;

#if defined( SIGNED_DISTANCE_FIELD )
	diffuse.a *= smoothstep( SOFT_EDGE_MIN, SOFT_EDGE_MAX, diffuse.a ); 
#endif//SIGNED_DISTANCE_FIELD

#if !defined( ALPHA_GLASS )
	diffuse.a *= u_RenderColor.a;
#endif

#if defined( HAS_NORMALMAP )
	vec3 N = normalmap2D( u_NormalMap, var_TexDiffuse );
#else
	vec3 N = normalize( var_Normal );
#endif
	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

#if !defined( LIGHTING_FULLBRIGHT )
	vec3 L = normalize( var_LightDir );
	vec3 V = normalize( var_ViewDir );
#if defined( HAS_GLOSSMAP )
	glossmap = texture2D( u_GlossMap, var_TexDiffuse );
#endif

#if defined (SURFACE_LIGHTING)	// lightmapped surfaces
#if defined( APPLY_STYLE0 )
	ApplyLightStyle( var_TexLight0, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE1 )
	ApplyLightStyle( var_TexLight1, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE2 )
	ApplyLightStyle( var_TexLight2, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE3 )
	ApplyLightStyle( var_TexLight3, N, V, glossmap.rgb, u_Smoothness, light, gloss );
#endif
#if defined( APPLY_STYLE0 ) || defined( APPLY_STYLE1 ) || defined( APPLY_STYLE2 ) || defined( APPLY_STYLE3 )
	// convert values back to normal range and clamp it
	light = min(( light * LIGHTMAP_SHIFT ), 1.0 );
	gloss = min(( gloss * LIGHTMAP_SHIFT ), 1.0 );
#endif
#elif defined (VERTEX_LIGHTING)	// vertexlight surface
#if defined( HAS_NORMALMAP )
	float NdotB = ComputeStaticBump( L, N, u_AmbientFactor );
	float factor = DiffuseBRDF( N, V, L, u_Smoothness, NdotB );
	light = (var_DiffuseLight * factor) + (var_AmbientLight);
#else //!HAS_NORMALMAP
	light = var_AmbientLight + var_DiffuseLight;
#endif
#else				// NPC, etc
#if defined( HAS_NORMALMAP )
	float factor = u_LightShade.y;
	float NdotL = (( dot( N, -L ) + ( SHADE_LAMBERT - 1.0 )) / SHADE_LAMBERT );
	if( NdotL > 0.0 ) factor -= DiffuseBRDF( N, V, L, u_Smoothness, saturate( NdotL )) * u_LightShade.y;
	light = (var_DiffuseLight * factor) + (var_AmbientLight);
#else //!HAS_NORMALMAP
	light = var_AmbientLight + var_DiffuseLight;
#endif
#endif // SURFACE_LIGHTING
	// apply lighting
	diffuse.rgb *= light;

// compute the specular term
#if defined( HAS_GLOSSMAP )
#if defined (SURFACE_LIGHTING)
	// gloss it's already computed
	diffuse.rgb *= saturate( 1.0 - GetLuminance( gloss ));
	diffuse.rgb += gloss;
#else //!SURFACE_LIGHTING
	float NdotL2 = saturate( dot( N, L ));
	specular = SpecularBRDF( N, V, L, u_Smoothness, glossmap.rgb ) * NdotL2;
	diffuse.rgb *= saturate( 1.0 - GetLuminance( specular ));
#if defined (VERTEX_LIGHTING)
	diffuse.rgb += var_DiffuseLight * specular;
#else
	diffuse.rgb += var_DiffuseLight * specular * u_LightShade.y; // prevent parazite lighting
#endif
#endif // SURFACE_LIGHTING
#endif // HAS_GLOSSMAP

#if defined( LIGHTVEC_DEBUG ) && !defined (SURFACE_LIGHTING)
	light = ( normalize( N + L ) + 1.0 ) * 0.5;
#endif//LIGHTVEC_DEBUG

#if defined( REFLECTION_CUBEMAP )
	vec3 reflectance = GetReflectionProbe( var_Position, u_ViewOrigin, var_WorldNormal, glossmap.rgb, u_Smoothness );
	diffuse.rgb += var_DiffuseLight * reflectance * u_ReflectScale;
#endif//defined( REFLECTION_CUBEMAP )

#if defined( LIGHTMAP_DEBUG ) || defined( LIGHTVEC_DEBUG )
	diffuse.rgb = light;
#endif
#endif// LIGHTING_FULLBRIGHT

#if defined( HAS_LUMA )
	diffuse.rgb += texture2D( u_GlowMap, var_TexDiffuse ).rgb;
#endif

#if defined( TRANSLUCENT )
	diffuse.rgb = mix( GetScreenColor( N, 1.0 ), diffuse.rgb, diffuse.a );
#endif

#if defined( APPLY_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
#endif
	// compute final color
	gl_FragColor = diffuse;
}