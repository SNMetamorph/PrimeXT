/*
light_fp.glsl - defered scene lighting
Copyright (C) 2018 Uncle Mike

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
#include "lightmodel.h"
#include "deferred\wlfetch.h"
#include "deferred\bsptrace.h"

uniform sampler2D		u_NormalMap;
uniform sampler2D		u_VisLightMap0;
uniform sampler2D		u_VisLightMap1;
uniform sampler2D		u_DepthMap;
uniform sampler2DRect	u_BspLightsMap;

// reconstruct pos
uniform vec3		u_ViewOrigin;
uniform float		u_zFar;

varying vec2		var_TexCoord;
varying vec3 		var_RayVec;

void main( void )
{
	vec4 normal = texture2D( u_NormalMap, var_TexCoord );

	if( normal.w == 1.0 )
	{
		// something fullbright (e.g. sky)
		gl_FragColor = vec4( 1.0 );
		return;
	}

	float w = texture2D( u_DepthMap, var_TexCoord ).r;
	w = linearizeDepth( u_zFar, RemapVal( w, 0.0, 0.8, 0.0, 1.0 ));
	vec3 pos = u_ViewOrigin + var_RayVec * (w - 1.0); // nudge point to avoid z-fighting
	normal.xyz = normalize( normal.xyz * 2.0 - 1.0 );
	vec4 shadowmap = vec4( 0.0 );
	bool first = true;

	vec4 lights = texture2D( u_VisLightMap0, var_TexCoord ) * 255.0;

	// do lighting for current pixel
	for( int i = 0; i < MAXDYNLIGHTS; i++ )
	{
		if( i == MAXLIGHTMAPS )
		{
			if( first )
			{
				lights = texture2D( u_VisLightMap1, var_TexCoord ) * 255.0;
				first = false;
				i = 0;
			}
			else
			{
				break;
			}
		}

		if( lights[i] == 255.0 )
			break;

		float add = 0.0;
		vec4 worldlight[3];
		vec3 delta;
		float dot1;

		// transform lightnum into texcoords
		int x = int( mod( lights[i], 256.0 ));
		int y = int( lights[i] / 256.0 );

		worldlight[0] = texture2DRect( u_BspLightsMap, ivec2( x, y + 0 ));
		worldlight[1] = texture2DRect( u_BspLightsMap, ivec2( x, y + 1 ));
		worldlight[2] = texture2DRect( u_BspLightsMap, ivec2( x, y + 2 ));

		if( wltype() == EMIT_SKYLIGHT )
		{
			dot1 = -dot( normal.xyz, wlnormal( ));
			dot1 = max( dot1, normal.w );

			if( dot1 <= DISCARD_EPSILON )
				continue; // behind light surface

			add = dot1;

			// search back to see if we can hit a sky brush
			delta = pos + wlnormal() * -65536.0;
		}
		else
		{
			delta = wlpos() - pos;
			delta = normalize( delta );
			dot1 = dot( delta, normal.xyz );
			dot1 = max( dot1, normal.w );

			if( dot1 <= DISCARD_EPSILON )
				continue; // behind sample surface

			// pointlight
			if( wltype() == EMIT_POINT )
			{
				add = dot1;
			}
			else if( wltype() == EMIT_SURFACE )
			{
				float dot2 = -dot( delta, wlnormal());
				if( dot2 <= DISCARD_EPSILON )
					continue; // behind light surface
				add = dot1 * dot2;
			}
			else if( wltype() == EMIT_SPOTLIGHT )
			{
				float dot2 = -dot( delta, wlnormal());
				if( dot2 <= wlstopdot2())
					continue; // outside light cone
				float ratio = dot1 * dot2;
				if( dot2 <= wlstopdot())
					ratio *= (dot2 - wlstopdot2()) / (wlstopdot() - wlstopdot2());
				add = ratio;
			}

			delta = wlpos(); 
		}
#if defined( BSPTRACE_BMODELS )
		if( add > EQUAL_EPSILON && bool( traceRay2( pos, delta )))
#else
		if( add > EQUAL_EPSILON && bool( traceRay( pos, delta )))
#endif
		{
			shadowmap[i] += (first) ? 0.25 : 0.75;
		}
	}

	gl_FragColor = shadowmap;
}