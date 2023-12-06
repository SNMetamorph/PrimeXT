/*
dofbokeh_fp.glsl - advanced DOF with bokeh effect
Copyright (C) 2014 Martins Uptis

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

uniform float		u_ScreenWidth;
uniform float		u_ScreenHeight;
uniform float		u_FocalDepth;
uniform float		u_FocalLength;
uniform int		u_DofDebug;
uniform float		u_FStop;
uniform float		u_zFar;

uniform sampler2D		u_ScreenMap;
uniform sampler2D		u_DepthMap;

varying vec2		var_TexCoord;

float	width = u_ScreenWidth;	// texture width
float	height = u_ScreenHeight;	// texture height
vec2	texel = vec2( 1.0 / width, 1.0 / height );

// make sure that these two values are the same for your camera, otherwise distances will be wrong.\n

float	znear = Z_NEAR;		// camera clipping start
float	zfar = u_zFar;		// camera clipping end

//------------------------------------------
// user variables

int	samples = 3;		// samples on the first ring
int	rings = 3;		// ring count

// #define MANUALDOF		// manual dof calculation
float	ndofstart = 1.0;		// near dof blur start
float	ndofdist = 2.0;		// near dof blur falloff distance
float	fdofstart = 1.0;		// far dof blur start
float	fdofdist = 3.0;		// far dof blur falloff distance

float	CoC = 0.03;		// circle of confusion size in mm (35mm film = 0.03mm)

// #define VIGNETTING		// use optical lens vignetting?
float	vignout = 1.3;		// vignetting outer border
float	vignin = 0.0;		// vignetting inner border
float	vignfade = 22.0;		// f-stops till vignete fades

// #define AUTOFOCUS		// use autofocus in shader? disable if you use external focalDepth value
vec2	focus = vec2( 0.5, 0.5 );	// autofocus point on screen (0.0,0.0 - left lower corner, 1.0,1.0 - upper right)
float	maxblur = 1.0;		// clamp value of max blur ( 0.0 = no blur, 1.0 default )

float	threshold = 0.0;		// highlight threshold;
float	gain = 0.0;		// highlight gain;

float	bias = 0.5;		// bokeh edge bias
float	fringe = 0.7;		// bokeh chromatic aberration/fringing

#define NOISE			// use noise instead of pattern for sample dithering
float	namount = 0.0001;		// dither amount

// #define DEPTH_BLUR		// blur the depth buffer?
float	dbsize = 1.25;		// depthblursize

/*
next part is experimental
not looking good with small sample and ring count
looks okay starting from samples = 4, rings = 4
*/

// #define PENTAGON			// use pentagon as bokeh shape?
float	feather = 0.4;		// pentagon shape feather

//------------------------------------------

// pentagonal shape
float penta( vec2 coords )
{
	vec4 HS0 = vec4( 1.0,         0.0,         0.0,  1.0 );
	vec4 HS1 = vec4( 0.309016994, 0.951056516, 0.0,  1.0 );
	vec4 HS2 = vec4(-0.809016994, 0.587785252, 0.0,  1.0 );
	vec4 HS3 = vec4(-0.809016994,-0.587785252, 0.0,  1.0 );
	vec4 HS4 = vec4( 0.309016994,-0.951056516, 0.0,  1.0 );
	vec4 HS5 = vec4( 0.0        , 0.0        , 1.0,  1.0 );
	
	vec4 one = vec4( 1.0 );
	float scale = float( rings ) - 1.3;
	vec4 P = vec4( coords, vec2( scale, scale )); 
	
	vec4 dist = vec4( 0.0 );
	float inorout = -4.0;
	
	dist.x = dot( P, HS0 );
	dist.y = dot( P, HS1 );
	dist.z = dot( P, HS2 );
	dist.w = dot( P, HS3 );
	
	dist = smoothstep( -feather, feather, dist );
	
	inorout += dot( dist, one );
	
	dist.x = dot( P, HS4 );
	dist.y = HS5.w - abs( P.z );
	
	dist = smoothstep( -feather, feather, dist );
	inorout += dist.x;
	
	return clamp( inorout, 0.0, 1.0 );
}

// blurring depth
float bdepth( vec2 coords )
{
	float d = 0.0;
	float kernel[9];
	vec2 offset[9];
	
	vec2 wh = texel * dbsize;
	
	offset[0] = vec2(-wh.x,-wh.y );
	offset[1] = vec2( 0.0, -wh.y );
	offset[2] = vec2( wh.x -wh.y );
	
	offset[3] = vec2(-wh.x,  0.0 );
	offset[4] = vec2( 0.0,   0.0 );
	offset[5] = vec2( wh.x,  0.0 );
	
	offset[6] = vec2(-wh.x, wh.y );
	offset[7] = vec2( 0.0,  wh.y );
	offset[8] = vec2( wh.x, wh.y );
	
	kernel[0] = 1.0 / 16.0; kernel[1] = 2.0 / 16.0; kernel[2] = 1.0 / 16.0;
	kernel[3] = 2.0 / 16.0; kernel[4] = 4.0 / 16.0; kernel[5] = 2.0 / 16.0;
	kernel[6] = 1.0 / 16.0; kernel[7] = 2.0 / 16.0; kernel[8] = 1.0 / 16.0;

	for( int i = 0; i < 9; i++ )
	{
		float tmp = texture( u_DepthMap, coords + offset[i] ).r;
		d += tmp * kernel[i];
	}
	
	return d;
}

// processing the sample
vec3 color( vec2 coords, float blur )
{
	vec3 col = vec3( 0.0 );
	
	col.r = texture( u_ScreenMap, coords + vec2(   0.0,  1.0 ) * texel * fringe * blur ).r;
	col.g = texture( u_ScreenMap, coords + vec2(-0.866, -0.5 ) * texel * fringe * blur ).g;
	col.b = texture( u_ScreenMap, coords + vec2( 0.866, -0.5 ) * texel * fringe * blur ).b;
	
	vec3 lumcoeff = vec3( 0.299, 0.587, 0.114 );
	float lum = dot( col.rgb, lumcoeff );
	float thresh = max(( lum - threshold ) * gain, 0.0 );

	return col + mix( vec3( 0.0 ), col, thresh * blur );
}

// generating noise/pattern texture for dithering
vec2 rand( vec2 coord )
{
#ifdef NOISE
	float noiseX = clamp( fract( sin( dot( coord, vec2( 12.9898, 78.233 ))) * 43758.5453 ), 0.0, 1.0 ) * 2.0 - 1.0;
	float noiseY = clamp( fract (sin( dot( coord, vec2( 12.9898, 78.233 ) * 2.0 )) * 43758.5453 ), 0.0, 1.0 ) * 2.0 - 1.0;
#else
	float noiseX = ( (fract( 1.0 - coord.s * (width / 2.0 )) * 0.25 ) + (fract( coord.t * (height / 2.0 )) * 0.75 )) * 2.0 - 1.0;
	float noiseY = (( fract( 1.0 - coord.s * (width / 2.0 )) * 0.75 ) + (fract (coord.t * (height / 2.0 )) * 0.25 )) * 2.0 - 1.0;
#endif
	return vec2( noiseX, noiseY );
}

vec3 debugFocus( vec3 col, float blur, float depth )
{
	float edge = 0.002 * depth; // distance based edge smoothing
	float m = clamp( smoothstep( 0.0, edge, blur ), 0.0, 1.0 );
	float e = clamp( smoothstep( 1.0 - edge, 1.0, blur ), 0.0, 1.0 );
	col = mix( col, vec3( 1.0, 0.5, 0.0 ), ( 1.0 - m ) * 0.6 );
	col = mix( col, vec3( 0.0, 0.5, 1.0 ), (( 1.0 - e ) - ( 1.0 - m )) * 0.2 );

	return col;
}

float linearize( float depth )
{
	return -zfar * znear / ( depth * ( zfar - znear ) - zfar );
}

float vignette( void )
{
	float dist = distance( var_TexCoord, vec2( 0.5, 0.5 ));
	dist = smoothstep( vignout + ( u_FStop / vignfade ), vignin + ( u_FStop / vignfade ), dist );
	return clamp( dist, 0.0, 1.0 );
}

void main( void ) 
{
	// scene depth calculation
#ifdef DEPTH_BLUR
	float depth = linearize( bdepth( var_TexCoord ));
#else
	float depth = linearize( texture( u_DepthMap, var_TexCoord ).x );
#endif
	// focal plane calculation
#ifdef AUTOFOCUS
	float fDepth = linearize( texture( u_DepthMap, focus ).x );
#else
	float fDepth = u_FocalDepth;
#endif
	// dof blur factor calculation
	float blur = 0.0;
	
#ifdef MANUALDOF
	float a = depth - fDepth;		// focal plane
	float b = (a - fdofstart ) / fdofdist;	// far DoF
	float c = (-a - ndofstart ) / ndofdist;	// near Dof
	blur = (a > 0.0) ? b : c;
#else
	float f = u_FocalLength;		// focal length in mm
	float d = fDepth * 1000.0;		// focal plane in mm
	float o = depth * 1000.0;		// depth in mm
		
	float a = (o * f) / (o - f); 
	float b = (d * f) / (d - f); 
	float c = (d - f) / (d * u_FStop * CoC); 
		
	blur = abs(a - b) * c;
#endif
	blur = saturate( blur );
	
	// calculation of pattern for ditering
	vec2 noise = rand( var_TexCoord ) * namount * blur;
	
	// getting blur x and y step factor
	float w = texel.x * blur * maxblur + noise.x;
	float h = texel.y * blur * maxblur + noise.y;
	
	// calculation of final color
	vec3 col = texture( u_ScreenMap, var_TexCoord ).rgb;
	
	if( blur > 0.05 )
	{
		// some optimization thingy
		int ringsamples;
		float s = 1.0;
		
		for( int i = 1; i <= rings; i += 1 )
		{   
			ringsamples = i * samples;
			
			for( int j = 0 ; j < ringsamples; j += 1 )   
			{
				float step = M_PI * 2.0 / float( ringsamples );
				float pw = ( cos( float( j ) * step ) * float( i ));
				float ph = ( sin( float( j ) * step ) * float( i ));
				float p = 1.0;
#ifdef PENTAGON
				p = penta( vec2( pw, ph ));
#endif
				col += color( var_TexCoord + vec2( pw * w, ph * h ), blur ) * mix( 1.0, (float( i )) / (float( rings )), bias ) * p;  
				s += 1.0 * mix( 1.0, (float( i )) / (float( rings )), bias ) * p;   
			}
		}

		col /= s; // divide by sample count
	}
	
	if( bool( u_DofDebug ))
	{
		col = debugFocus( col, blur, depth );
	}
	
#ifdef VIGNETTING
	col *= vignette();
#endif
	gl_FragColor.rgb = col;
	gl_FragColor.a = 1.0;
}