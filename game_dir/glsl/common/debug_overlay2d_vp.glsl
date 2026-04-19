/*
debug_overlay2d_vp.glsl - 2D screen-space debug overlay
Copyright (C) 2025

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

attribute vec3  attr_Position; // (screen_x, screen_y, unused)

uniform vec2    u_ScreenSize;  // (width, height) in pixels

varying vec2    var_TexCoord;

void main()
{
    // Derive texcoord from normalized screen position so we avoid relying
    // on extra vertex-attribute streams that have been unreliable for this
    // simple overlay use case.
    var_TexCoord = vec2(attr_Position.x / u_ScreenSize.x,
                        attr_Position.y / u_ScreenSize.y);
    vec2 clip = vec2(
        (2.0 * attr_Position.x / u_ScreenSize.x) - 1.0,
        1.0 - (2.0 * attr_Position.y / u_ScreenSize.y)
    );
    gl_Position = vec4(clip, 0.0, 1.0);
}
