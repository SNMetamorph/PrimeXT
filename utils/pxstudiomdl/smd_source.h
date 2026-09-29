/*
smd_source.h - SMD reference mesh/animation importer
Copyright (C) 2017 Uncle Mike
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
#include <array>
#include <cstdio>
#include <memory>
#include <string>

class CSmdSource : public IStudioSource
{
public:
	CSmdSource( void );
	~CSmdSource( void ) {}

	void GrabStudio( s_model_t *pmodel ) override;
	void GrabAnimation( s_animation_t *panim ) override;

private:
	bool GetLine( void );

	int  Grab_Nodes( s_node_t *pnodes );
	void Grab_Skeleton( s_model_t *pmodel );
	void Grab_Triangles( s_model_t *pmodel );
	bool Grab_AnimFrames( s_animation_t *panim );

	std::unique_ptr<FILE, int(*)(FILE*)> m_input;
	std::array<char, 1024>		m_line;
	int				m_linecount;
	std::string			m_filename;
};
