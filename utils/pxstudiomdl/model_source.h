/*
model_source.h - common interface for reference mesh/animation sources
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once

#include "studio.h"
#include "studiomdl.h"
#include <memory>

//-----------------------------------------------------------------------------
// Common interface for source model formats (SMD, glTF, ...)
//-----------------------------------------------------------------------------
class IStudioSource
{
public:
	virtual ~IStudioSource( void ) {}
	virtual void GrabStudio( s_model_t *pmodel ) = 0;
	virtual void GrabAnimation( s_animation_t *panim ) = 0;
};

using CSourcePtr = std::unique_ptr<IStudioSource>;

// create an importer matching the source file format (detected by extension)
CSourcePtr CreateStudioSource( const char *name );

//
// shared helpers, used by importers and QC parsing
//
bool IsEnd( char const *pLine );
int LookupTexture( const char *texturename );
s_mesh_t *LookupMesh( s_model_t *pmodel, char *texturename );
s_trianglevert_t *LookupTriangle( s_mesh_t *pmesh, int index );
void Build_Reference( s_model_t *pmodel );
