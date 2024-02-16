/*
shader.h - shadercache implementation
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

#ifndef SHADER_H
#define SHADER_H

// simple cache (4 bytes here)
class shader_t
{
	unsigned short	shadernum;
	unsigned short	sequence;
public:
	shader_t() : shadernum(0), sequence(0) {};

	struct glsl_prog_s *GetShader( void );
	void SetShader( unsigned short hand );
	unsigned short GetHandle( void ) { return shadernum; }
	void Invalidate( void ) { shadernum = sequence = 0; }
	bool IsValid( void );
};

#endif//SHADER_H