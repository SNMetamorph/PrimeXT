/*
worldlight.h - processing worldlight
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

#ifndef WORLDLIGHT_H
#define WORLDLIGHT_H

uniform sampler2DRect	u_BspLightsMap;

#include "deferred\wlfetch.h"

void ProcessWorldLight( const float lnum, const float shadow, const vec4 normal, const vec3 pos, inout vec3 s_light[MAXLIGHTMAPS], inout vec4 styles )
{
	vec3 add = vec3( 0.0 );
	vec4 worldlight[3];	// don't change this name
	vec3 delta;
	float dot1;

	// transform lightnum into texcoords
	int x = int( mod( lnum, 256.0 ));
	int y = int( lnum / 256.0 );

	worldlight[0] = texture2DRect( u_BspLightsMap, ivec2( x, y + 0 ));
	worldlight[1] = texture2DRect( u_BspLightsMap, ivec2( x, y + 1 ));
	worldlight[2] = texture2DRect( u_BspLightsMap, ivec2( x, y + 2 ));

	if( wltype() == EMIT_SKYLIGHT )
	{
		dot1 = -dot( normal.xyz, wlnormal( ));
		dot1 = max( dot1, normal.w );

		if( dot1 <= DISCARD_EPSILON )
			return; // behind light surface

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
		dot1 = max( dot1, normal.w );

		if( dot1 <= DISCARD_EPSILON )
			return; // behind sample surface

		dist = max( 1.0, dist );
		float denominator = ( dist * dist );

		// ZHLT feature "_falloff"
		if( wlfalloff() == falloff_inverse )
			denominator = dist;

		// pointlight
		if( wltype() == EMIT_POINT )
		{
			float ratio = dot1 / denominator;
			add = wlintensity() * ratio;
		}
		else if( wltype() == EMIT_SURFACE )
		{
			float dot2 = -dot( delta, wlnormal());
			if( dot2 <= DISCARD_EPSILON )
				return; // behind light surface
			float ratio = dot1 * dot2 / denominator;
			add = wlintensity() * ratio;
		}
		else if( wltype() == EMIT_SPOTLIGHT )
		{
			float dot2 = -dot( delta, wlnormal());
			if( dot2 <= wlstopdot2())
				return; // outside light cone
			float ratio = dot1 * dot2 / denominator;
			if( dot2 <= wlstopdot())
				ratio *= (dot2 - wlstopdot2()) / (wlstopdot() - wlstopdot2());
			add = wlintensity() * ratio;
		}

		delta = wlpos(); 
	}

	if( VectorMax( add ) > EQUAL_EPSILON )
	{
		add *= shadow;

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

#endif//WORLDLIGHT_H