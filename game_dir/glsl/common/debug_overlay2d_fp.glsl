/*
debug_overlay2d_fp.glsl - 2D screen-space debug overlay
Copyright (C) 2025

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

uniform sampler2D u_ColorMap;
uniform vec4      u_Color;       // tint / outright color
uniform float     u_UseTexture;  // 1.0 = sample texture, 0.0 = u_Color only

varying vec2      var_TexCoord;

void main()
{
    if (u_UseTexture > 0.5)
        gl_FragColor = texture2D(u_ColorMap, var_TexCoord) * u_Color;
    else
        gl_FragColor = u_Color;
}
