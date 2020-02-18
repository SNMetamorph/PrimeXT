/*
terrain.h - terrain texturing
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

#ifndef TERRAIN_H
#define TERRAIN_H

#define colormap2DArray( tex, tc, layer )	texture2DArray( tex, vec3( tc, layer ))

#if defined( GLSL_ALLOW_TEXTURE_ARRAY )
vec4 TerrainApplyDiffuse( sampler2DArray tex, vec2 tc, vec4 mask0, vec4 mask1, vec4 mask2, vec4 mask3 )
{
	vec4 diffuse = vec4( 0.0 );
#if TERRAIN_NUM_LAYERS >= 1
	if( mask0.r > 0.0 ) diffuse += colormap2DArray( tex, tc, 0.0 ) * mask0.r;
#endif
#if TERRAIN_NUM_LAYERS >= 2
	if( mask0.g > 0.0 ) diffuse += colormap2DArray( tex, tc, 1.0 ) * mask0.g;
#endif
#if TERRAIN_NUM_LAYERS >= 3
	if( mask0.b > 0.0 ) diffuse += colormap2DArray( tex, tc, 2.0 ) * mask0.b;
#endif
#if TERRAIN_NUM_LAYERS >= 4
	if( mask0.a > 0.0 ) diffuse += colormap2DArray( tex, tc, 3.0 ) * mask0.a;
#endif
#if TERRAIN_NUM_LAYERS >= 5
	if( mask1.r > 0.0 ) diffuse += colormap2DArray( tex, tc, 4.0 ) * mask1.r;
#endif
#if TERRAIN_NUM_LAYERS >= 6
	if( mask1.g > 0.0 ) diffuse += colormap2DArray( tex, tc, 5.0 ) * mask1.g;
#endif
#if TERRAIN_NUM_LAYERS >= 7
	if( mask1.b > 0.0 ) diffuse += colormap2DArray( tex, tc, 6.0 ) * mask1.b;
#endif
#if TERRAIN_NUM_LAYERS >= 8
	if( mask1.a > 0.0 ) diffuse += colormap2DArray( tex, tc, 7.0 ) * mask1.a;
#endif
#if TERRAIN_NUM_LAYERS >= 9
	if( mask1.r > 0.0 ) diffuse += colormap2DArray( tex, tc, 8.0 ) * mask2.r;
#endif
#if TERRAIN_NUM_LAYERS >= 10
	if( mask1.g > 0.0 ) diffuse += colormap2DArray( tex, tc, 9.0 ) * mask2.g;
#endif
#if TERRAIN_NUM_LAYERS >= 11
	if( mask1.b > 0.0 ) diffuse += colormap2DArray( tex, tc, 10.0 ) * mask2.b;
#endif
#if TERRAIN_NUM_LAYERS >= 12
	if( mask1.a > 0.0 ) diffuse += colormap2DArray( tex, tc, 11.0 ) * mask2.a;
#endif
#if TERRAIN_NUM_LAYERS >= 13
	if( mask1.r > 0.0 ) diffuse += colormap2DArray( tex, tc, 12.0 ) * mask3.r;
#endif
#if TERRAIN_NUM_LAYERS >= 14
	if( mask1.g > 0.0 ) diffuse += colormap2DArray( tex, tc, 13.0 ) * mask3.g;
#endif
#if TERRAIN_NUM_LAYERS >= 15
	if( mask1.b > 0.0 ) diffuse += colormap2DArray( tex, tc, 14.0 ) * mask3.b;
#endif
#if TERRAIN_NUM_LAYERS >= 16
	if( mask1.a > 0.0 ) diffuse += colormap2DArray( tex, tc, 15.0 ) * mask3.a;
#endif
	return diffuse;
}
#endif

// this is for debug
#if defined( GLSL_ALLOW_TEXTURE_ARRAY )
vec4 TerrainColorDebug( vec4 mask0, vec4 mask1, vec4 mask2, vec4 mask3 )
{
	vec4 diffuse = vec4( vec3( 0.0 ), 1.0 );	// black as default

#if TERRAIN_NUM_LAYERS >= 1
	diffuse.rgb += vec3( 1.0, 0.0, 0.0 ) * mask0.r;	// layer0: red
#endif

#if TERRAIN_NUM_LAYERS >= 2
	diffuse.rgb += vec3( 0.0, 1.0, 0.0 ) * mask0.g;	// layer1: green
#endif

#if TERRAIN_NUM_LAYERS >= 3
	diffuse.rgb += vec3( 0.0, 0.0, 1.0 ) * mask0.b;	// layer2: blue
#endif

#if TERRAIN_NUM_LAYERS >= 4
	diffuse.rgb += vec3( 1.0, 1.0, 0.0 ) * mask0.a;	// layer3: yellow
#endif

#if TERRAIN_NUM_LAYERS >= 5
	diffuse.rgb += vec3( 0.0, 1.0, 1.0 ) * mask1.r;	// layer4: cyan
#endif

#if TERRAIN_NUM_LAYERS >= 6
	diffuse.rgb += vec3( 1.0, 0.0, 1.0 ) * mask1.g;	// layer5: magenta
#endif

#if TERRAIN_NUM_LAYERS >= 7
	diffuse.rgb += vec3( 1.0, 0.5, 0.0 ) * mask1.b;	// layer6: orange
#endif

#if TERRAIN_NUM_LAYERS >= 8
	diffuse.rgb += vec3( 0.5, 0.5, 0.5 ) * mask1.a;	// layer7: gray
#endif

#if TERRAIN_NUM_LAYERS >= 9
	diffuse.rgb += vec3( 0.5, 0.0, 0.0 ) * mask2.r;	// layer8: red
#endif

#if TERRAIN_NUM_LAYERS >= 10
	diffuse.rgb += vec3( 0.0, 0.5, 0.0 ) * mask2.g;	// layer9: green
#endif

#if TERRAIN_NUM_LAYERS >= 11
	diffuse.rgb += vec3( 0.0, 0.0, 0.5 ) * mask2.b;	// layer10: blue
#endif

#if TERRAIN_NUM_LAYERS >= 12
	diffuse.rgb += vec3( 0.5, 0.5, 0.0 ) * mask2.a;	// layer11: yellow
#endif

#if TERRAIN_NUM_LAYERS >= 13
	diffuse.rgb += vec3( 0.0, 0.5, 0.5 ) * mask3.r;	// layer12: cyan
#endif

#if TERRAIN_NUM_LAYERS >= 14
	diffuse.rgb += vec3( 0.5, 0.0, 0.5 ) * mask3.g;	// layer13: magenta
#endif

#if TERRAIN_NUM_LAYERS >= 15
	diffuse.rgb += vec3( 0.5, 0.25, 0.0 ) * mask3.b;	// layer14: orange
#endif

#if TERRAIN_NUM_LAYERS >= 16
	diffuse.rgb += vec3( 0.25, 0.25, 0.25 ) * mask3.a;// layer15: gray
#endif
	return diffuse;
}
#endif
#endif