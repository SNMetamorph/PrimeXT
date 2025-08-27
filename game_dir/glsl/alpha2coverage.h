/*
alpha2coverage.h - utilities to provide better visual quality for alpha to coverage
Copyright (C) 2023 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

//#extension GL_ARB_texture_query_lod : enable

float CalcMipLevel(sampler2D tex, in vec2 texCoords)
{
    // TODO don't use it until figuring out how the fuck it works
//#ifdef GL_ARB_texture_query_lod
//    return textureQueryLOD(tex, texCoords).x;
//#else
    // fallback method for case when textureQueryLOD() is unavailable
    vec2 dx = dFdx(texCoords);
    vec2 dy = dFdy(texCoords);
    float deltaMaxSqr = max(dot(dx, dx), dot(dy, dy));
    return max(0.0, 0.5 * log2(deltaMaxSqr));
//#endif
}

float AlphaSharpening(in float alpha, in float cutoff)
{
    return clamp((alpha - cutoff) / max(abs(dFdx(alpha)) + abs(dFdy(alpha)), 0.0001) + 0.5, 0.0, 1.0);
}

float AlphaRescaling(sampler2D tex, in vec2 texCoords, in float alpha)
{
    const float mipScale = 0.25;
    return alpha * (1.0 + CalcMipLevel(tex, texCoords * textureSize(tex, 0)) * mipScale);
}
