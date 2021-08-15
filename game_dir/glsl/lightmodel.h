/*
lightmodel.h - BRDF models
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef LIGHTMODEL_H
#define LIGHTMODEL_H

#define FALL_LIGHTMODEL_BLINN		// default lightmodel Blinn\Phong
#define BRDF_LIGHTMODEL_DISNEY	// physical based lightmodel Disney\Oren Nayar

float SmoothnessToRoughness( float smoothness )
{
	return ( 1.0 - smoothness ) * ( 1.0 - smoothness );
}

#if defined( BRDF_LIGHTMODEL_DISNEY )
float DiffuseBRDF( vec3 N, vec3 V, vec3 L, float Gloss, float NdotL )
{
#if defined( APPLY_PBS )
	// TODO: Share computations with Specular BRDF
	float roughness = SmoothnessToRoughness( min( Gloss, 1.0 ));
	float VdotH = saturate( dot( V, normalize( V + L )));
	float NdotV = max( abs( dot( N, V )) + 1e-9, 0.1 );
	
	// Burley BRDF with renormalization to conserve energy
	float energyBias = 0.5 * roughness;
	float energyFactor = mix( 1.0, 1.0 / 1.51, roughness );
	float fd90 = energyBias + 2.0 * VdotH * VdotH * roughness;
	float scatterL = mix( 1.0, fd90, pow( 1.0 - NdotL, 5.0 ));
	float scatterV = mix( 1.0, fd90, pow( 1.0 - NdotV, 5.0 ));

	return scatterL * scatterV * energyFactor * NdotL;
#else
	return NdotL;
#endif
}

vec3 SpecularBRDF( vec3 N, vec3 V, vec3 L, float m, vec3 f0, float NormalizationFactor )
{
	vec3 H = normalize( V + L );
	float m2 = m * m;

	// GGX NDF
	float NdotH = saturate( dot( N, H ));
	float spec = ( NdotH * m2 - NdotH ) * NdotH + 1.0;
	spec = m2 / ( spec * spec ) * NormalizationFactor;
	
	// Correlated Smith Visibility Term (including Cook-Torrance denominator)
	float NdotL = saturate( dot( N, L ));
	float NdotV = abs( dot( N, V )) + 1e-9;
	float Gv = NdotL * sqrt(( -NdotV * m2 + NdotV ) * NdotV + m2 );
	float Gl = NdotV * sqrt(( -NdotL * m2 + NdotL ) * NdotL + m2 );
	spec *= 0.5 / ( Gv + Gl );
		
	// Fresnel (Schlick approximation)
	float f90 = saturate( dot( f0, vec3( 0.33333 )) / 0.02 );  // Assume micro-occlusion when reflectance is below 2%
	vec3 fresnel = mix( f0, vec3( f90 ), pow( 1.0 - saturate( dot( L, H )), 5.0 ));

	return fresnel * spec * 0.5;
}

vec3 SpecularBRDF( vec3 N, vec3 V, vec3 L, float Gloss, vec3 SpecCol )
{
#if defined( APPLY_PBS )
	float m = max( SmoothnessToRoughness( Gloss ), 0.001 );  // Prevent highlights from getting too tiny without area lights
	return SpecularBRDF( N, V, L, m, SpecCol, 1.0 );
#else
#if defined( FALL_LIGHTMODEL_BLINN )
	return SpecCol * pow( max( dot( N, normalize( V + L )), 0.0 ), ( Gloss * Gloss ) * 256.0 ); // Blinn
#else
	return SpecCol * pow( max( dot( reflect( -L, N ), V ), 0.0 ), ( Gloss * Gloss ) * 256.0 ); // Phong
#endif
#endif
}

#else

float SpecularD_GGX( const in float roughness, const in float NH )
{
	float m = roughness * roughness;
	float m2 = m * m;
	float d = (NH * m2 - NH) * NH + 1.0;

	return m2 / (M_PI * d * d);
}

float SpecularG_SmithApprox( const in float roughness, const in float NV, const in float NL )
{
	float a = sqr( roughness );
	float GV = NL * ( NV * ( 1.0 - a ) + a );
	float GL = NV * ( NL * ( 1.0 - a ) + a );

	return 0.5 / (GV + GL);
}

vec3 SpecularF_Schlick( const in vec3 color, const in float VH )
{
	float f = pow( 1.0 - VH, 5.0 );
	
	// clamp reflectance to a minimum of 2% of the "brightest" green channel
	return mix( color, saturate( 50.0 * color.ggg ), f );
}

float Diffuse_OrenNayar( const in float roughness, const in float NV, const in float NL, const in float VH )
{
	float VL = 2.0 * VH - 1.0;
	float m = sqr( roughness );
	float m2 = sqr( m );
	float C1 = 1.0 - 0.5 * m2 / (m2 + 0.33);
	float Cosri = VL - NV * NL;
	float C2 = 0.45 * m2 / (m2 + 0.09) * Cosri * (Cosri >= 0.0 ? min( 1.0, NL / NV ) : NL );

	return INV_M_PI * (NL * C1 + C2);
}

float DiffuseBRDF( vec3 N, vec3 V, vec3 L, float Gloss, float NL )
{
#if defined( APPLY_PBS )
	// TODO: Share computations with Specular BRDF
	float roughness = SmoothnessToRoughness( min( Gloss, 1.0 ));
	float NV = abs( dot( N, V )) + 0.00001;
	vec3 HV = normalize( L + V );
	float NH = saturate( dot( N, HV ));
	float VH = saturate( dot( V, HV ));
	return Diffuse_OrenNayar( roughness, NV, NL, VH ) * 2.5; // FIXME
#else
	return NL;
#endif
}

vec3 SpecularBRDF( vec3 N, vec3 V, vec3 L, float Gloss, vec3 SpecCol )
{
#if defined( APPLY_PBS )
	float roughness = max( SmoothnessToRoughness( Gloss ), 0.001 );  // Prevent highlights from getting too tiny without area lights
	float NL = saturate( dot( N, L ));
	float NV = abs( dot( N, V )) + 0.00001;
	vec3 HV = normalize( L + V );
	float NH = saturate( dot( N, HV ));
	float VH = saturate( dot( V, HV ));

	float D = SpecularD_GGX( roughness, NH );
	float G = SpecularG_SmithApprox( roughness, NV, NL );
	vec3 F = SpecularF_Schlick( SpecCol, VH );
	return ( D * G * F ) * 1.5; // FIXME
#else
#if defined( FALL_LIGHTMODEL_BLINN )
	return SpecCol * pow( max( dot( N, normalize( V + L )), 0.0 ), ( Gloss * Gloss ) * 256.0 ); // Blinn
#else
	return SpecCol * pow( max( dot( reflect( -L, N ), V ), 0.0 ), ( Gloss * Gloss ) * 256.0 ); // Phong
#endif
#endif
}
#endif

vec3 ThinTranslucencyBRDF( vec3 N, vec3 L, vec3 transmittanceColor )
{
	float w = mix( 0, 0.5, GetLuminance( transmittanceColor ));
	float wn = rcp(( 1.0 + w ) * ( 1.0 + w ));
	float NdotL = dot( N, L );
	float transmittance = saturate(( -NdotL + w ) * wn );
	float diffuse = saturate(( NdotL + w ) * wn );
	
	return transmittanceColor * transmittance + diffuse;
}

#endif//LIGHTMODEL_H