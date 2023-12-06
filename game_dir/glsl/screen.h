/*
screen.h - get screen pixel
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

#ifndef SCREEN_H
#define SCREEN_H

uniform sampler2D	u_ScreenMap;
uniform vec2		u_ScreenSizeInv;
uniform float		u_RefractScale;
uniform float		u_AberrationScale;

vec2 GetDistortedTexCoords(in vec3 N, float fade)
{
	vec2 screenCoord = gl_FragCoord.xy * u_ScreenSizeInv;
	screenCoord.x += N.x * u_RefractScale * screenCoord.x * fade;
	screenCoord.y -= N.y * u_RefractScale * screenCoord.y * fade;
	return screenCoord;
}

vec3 GetScreenColor(in vec3 N, float fade)
{
#if defined( APPLY_REFRACTION )
	vec2 screenCoord = GetDistortedTexCoords(N, fade);
#else
	vec2 screenCoord = gl_FragCoord.xy * u_ScreenSizeInv;
#endif
	// clamp here to avoid NaNs on screen
	screenCoord.x = clamp( screenCoord.x, 0.01, 0.99 );
	screenCoord.y = clamp( screenCoord.y, 0.01, 0.99 );

#if defined( APPLY_ABERRATION )
	return chromemap2D(u_ScreenMap, screenCoord, N, u_AberrationScale * fade);
#else
	return texture(u_ScreenMap, screenCoord).rgb;
#endif
}

#endif // SCREEN_H