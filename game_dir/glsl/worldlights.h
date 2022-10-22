/*
worldlights.h - handle wordlights
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

#ifndef WORLDLIGHTS_H
#define WORLDLIGHTS_H

uniform sampler2DRect	u_BspLightsMap;

uniform float	u_LightStyleValues[MAX_LIGHTSTYLES];
uniform vec4	u_LightNums0;
uniform vec4	u_LightNums1;
uniform float	u_LightThreshold;
uniform float	u_LightGamma;
uniform float	u_LightScale;

#include "deferred\bsptrace.h"

vec3 ProcessWorldLights( const vec3 pos, const vec3 normal )
{
	vec3 lightmap = vec3( 0.0 );
	vec4 lights = u_LightNums0;
	vec3 s_light[MAXLIGHTMAPS];
	vec4 styles;

	s_light[0] = vec3( 0.0 );
	s_light[1] = vec3( 0.0 );
	s_light[2] = vec3( 0.0 );
	s_light[3] = vec3( 0.0 );
	styles = vec4( 255.0 );

	// do lighting for current pixel
	for( int i = 0; i < MAXDYNLIGHTS; i++ )
	{
		if( i == MAXLIGHTMAPS )
		{
			if( lights == u_LightNums0 )
			{
				lights = u_LightNums1;
				i = 0;
			}
			else
			{
				break;
			}
		}

		if( lights[i] == 255.0 )
			break;

		vec3 add = vec3( 0.0 );
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
			dot1 = max( dot1, 0.0 );

			if( dot1 <= DISCARD_EPSILON )
				continue; // behind light surface

			add = wlintensity() * dot1;

			// search back to see if we can hit a sky brush
			delta = pos + wlnormal() * -65536.0;
		}
		else
		{
			delta = wlpos() - pos;
			float dist = length( delta );
			delta = normalize( delta );
			dot1 = dot( delta, normal.xyz );
			dot1 = max( dot1, 0.0 );

			if( dot1 <= DISCARD_EPSILON )
				continue; // behind sample surface

			dist = max( 1.0, dist );

			// pointlight
			if( wltype() == EMIT_POINT )
			{
				float ratio = dot1 / ( dist * dist );
				add = wlintensity() * ratio;
			}
			else if( wltype() == EMIT_SURFACE )
			{
				float dot2 = -dot( delta, wlnormal());
				if( dot2 <= DISCARD_EPSILON )
					continue; // behind light surface
				float ratio = dot1 * dot2 / (dist * dist);
				add = wlintensity() * ratio;
			}
			else if( wltype() == EMIT_SPOTLIGHT )
			{
				float dot2 = -dot( delta, wlnormal());
				if( dot2 <= wlstopdot2())
					continue; // outside light cone
				float ratio = dot1 * dot2 / (dist * dist);
				if( dot2 <= wlstopdot())
					ratio *= (dot2 - wlstopdot2()) / (wlstopdot() - wlstopdot2());
				add = wlintensity() * ratio;
			}

			delta = wlpos(); 
		}

		if( VectorMax( add ) > EQUAL_EPSILON )
		{
			add *= traceRay( pos, delta, 0.0 );

			// unroll styles
			if( styles[0] == 255.0 )
				styles[0] = wlstyle();

			if( styles[0] != wlstyle( ))
			{
				if( styles[1] == 255.0 )
					styles[1] = wlstyle();

				if( styles[1] != wlstyle( ))
				{
					if( styles[2] == 255.0 )
						styles[2] = wlstyle();

					if( styles[2] != wlstyle( ))
					{
						if( styles[3] == 255.0 )
							styles[3] = wlstyle();

						if( styles[3] == wlstyle( ))
							s_light[3] += add;
					}
					else s_light[2] += add;
				}
				else s_light[1] += add;
			}
			else s_light[0] += add;
		}
	}

	for( int i = 0; i < MAXLIGHTMAPS && styles[i] != 255.0; i++ )
	{
		vec3 lb = ColorNormalize( s_light[i] * u_LightScale, u_LightThreshold );
		s_light[i] = ApplyGamma( lb / 256.0, u_LightGamma, 1.0 );
		lightmap += s_light[i] * u_LightStyleValues[int( styles[i] )];
	}

	// convert values back to normal range and clamp it
	lightmap = min(( lightmap * LIGHTMAP_SHIFT ), 1.0 );
#if 0
	// apply screen gamma
	gammaIndex = (lightmap.r * 255.0);
	lightmap.r = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (lightmap.g * 255.0);
	lightmap.g = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
	gammaIndex = (lightmap.b * 255.0);
	lightmap.b = u_GammaTable[int(gammaIndex/4.0)][int(mod(gammaIndex, 4.0 ))];
#endif
	return lightmap;
}

#endif//WORLDLIGHTS_H