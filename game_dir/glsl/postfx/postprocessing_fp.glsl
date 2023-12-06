/*
postprocessing_fp.glsl - generic postprocessing shader
Copyright (C) 2014 Uncle Mike
Copyright (C) 2022 SNMetamorph

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

uniform sampler2D	u_ScreenMap;
uniform float		u_RealTime;
uniform float		u_Brightness;
uniform float		u_Saturation;
uniform float		u_Contrast;
uniform vec3 		u_ColorLevels;
uniform vec4		u_AccentColor;
uniform float		u_VignetteScale;
uniform float		u_FilmGrainScale;

varying vec2		var_TexCoord;

vec3 AdjustBrightness(vec3 color)
{
	return color + u_Brightness;
}

vec3 AdjustSaturation(vec3 color)
{
	float luminance = GetLuminance(color);
	return mix(vec3(luminance), color, u_Saturation);
}

vec3 AdjustContrast(vec3 color)
{
	return 0.5 + u_Contrast * (color - 0.5);
}

vec3 AdjustColorLevels(vec3 color)
{
	return color * u_ColorLevels;
}

vec3 ApplyVignette(vec3 color)
{
	const float intensity = 19.0;
	const float curvature = 0.25;
	vec2 uv = var_TexCoord * (1.0 - var_TexCoord.yx);
	float vig = uv.x * uv.y * intensity;
	return color * mix(1.0, clamp(pow(vig, curvature), 0.0, 1.0), u_VignetteScale);
}

vec3 ApplyFilmGrain(vec3 color)
{
	float noise = fract(sin(dot(var_TexCoord, vec2(12.9898, 78.233) * u_RealTime)) * 43758.5453);
	return mix(color, vec3(0.5), noise * u_FilmGrainScale);
}

vec3 ApplyColorAccent(vec3 color)
{
	const float threshold = 0.999;
	float scale = u_AccentColor.a;
	vec3 monochrome = vec3(GetLuminance(color));
	vec3 probeColor = normalize(color + vec3(0.001)); // to avoid normalizing null vector
	vec3 desiredColor = normalize(u_AccentColor.rgb + vec3(0.001));
	float t = dot(probeColor, desiredColor);
	float f = smoothstep(threshold - 0.1, threshold, t);
	vec3 b = mix(color, monochrome, scale);
	return mix(b, color, f);
}

void main()
{
	vec3 color = texture(u_ScreenMap, var_TexCoord).rgb;
	color = AdjustColorLevels(color);
	color = ApplyColorAccent(color);
	color = AdjustBrightness(color);
	color = AdjustSaturation(color);
	color = AdjustContrast(color);
	color = ApplyFilmGrain(color);
	color = ApplyVignette(color);
	color = ConvertLinearToSRGB(color); // gamma-correction
	gl_FragColor = vec4(color, 1.0);
}
