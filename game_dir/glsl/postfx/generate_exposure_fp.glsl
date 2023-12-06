/*
generate_exposure_fp.glsl - Calculates & interpolate exposure from average frame luminance
Copyright (C) 2021 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "mathlib.h"

uniform sampler2D	u_ScreenMap;	// average luminance
uniform sampler2D	u_NormalMap;	// last frame exposure storage
uniform float		u_MipLod;
uniform float 		u_TimeDelta;

varying vec2		var_TexCoord;

// Source: http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr.pdf
float ComputeEV100FromAvgLuminance(float avgLum) 
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt (2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    return log2(avgLum * 100.0 / 12.5);
}

float ConvertEV100ToExposure(float ev100) 
{
    // Compute the maximum luminance possible with H_sbs sensitivity
    // maxLum = 78 / ( S * q ) * N^2 / t
    // = 78 / ( S * q ) * 2^ EV_100
    // = 78 / (100 * 0.65) * 2^ EV_100
    // = 1.2 * 2^ EV
    float max_luminance = 1.2 * pow(2.0, ev100);
    return 1.0 / (max_luminance);
}

void main()
{
	const float exposureMin = 0.01;
	const float exposureMax = 1.0;
	const float exposureScale = 1.0;
	const float adaptRateToDark = 0.6;
	const float adaptRateToBright = 1.6;

	float currentAdaptRate;
	float avgLuminanceLog = textureLod(u_ScreenMap, vec2(0.5), u_MipLod).r;
	float prevExposure = texture(u_NormalMap, vec2(0.5)).r;
	float avgLuminance = avgLuminanceLog / (1.0 - avgLuminanceLog);
	float ev100 = ComputeEV100FromAvgLuminance(avgLuminance);
	float currentExposure = clamp(ConvertEV100ToExposure(ev100) * exposureScale, exposureMin, exposureMax);

	if (currentExposure > prevExposure)
		currentAdaptRate = adaptRateToDark;
	else
		currentAdaptRate = adaptRateToBright;

	float t = clamp(u_TimeDelta * currentAdaptRate, 0.0, 1.0);
	float exposure = mix(prevExposure, currentExposure, t);
	gl_FragColor = vec4(exposure, 0.0, 0.0, 0.0);
}
