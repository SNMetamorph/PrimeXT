/*
gl_occlusion.h - occlusion query implementation
this code written for Paranoia 2: Savior modification
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_OCCLUSION_H
#define GL_OCCLUSION_H

void GL_AllocOcclusionQuery( msurface_t *surf );
void GL_DeleteOcclusionQuery( msurface_t *surf );
bool GL_SurfaceOccluded( msurface_t *surf );
void GL_TestSurfaceOcclusion( msurface_t *surf );

#endif//GL_OCCLUSION_H