/*
gltf_source.h - glTF reference mesh/animation importer
Copyright (C) 2026 SNMetamorph

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

#include "model_source.h"
#include <cgltf.h>
#include <memory>
#include <string>

class CGltfSource : public IStudioSource
{
public:
	explicit CGltfSource( const char *name );
	~CGltfSource( void ) {}

	void GrabStudio( s_model_t *pmodel ) override;
	void GrabAnimation( s_animation_t *panim ) override;

private:
	using CGlTFData = std::unique_ptr<cgltf_data, void(*)(cgltf_data*)>;

	CGlTFData	m_data;
	std::string	m_filename;
};
