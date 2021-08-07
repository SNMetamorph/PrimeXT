/*
material.h - material description for both client and server
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

#ifndef MATERIAL_H
#define MATERIAL_H
#include "vector.h"
#define MAX_MAT_SOUNDS	8

typedef struct matdef_s
{
	char		name[32];	// material name
	const char	*impact_decal;
	const char	*impact_parts[MAX_MAT_SOUNDS+1];	// for terminator
	const char	*impact_sounds[MAX_MAT_SOUNDS+1];
	const char	*step_sounds[MAX_MAT_SOUNDS+1];
	char		detailName[64];			// shared detail texture for materials
	vec2_t		detailScale;			// detail texture scales x, y
} matdef_t;

#define IMPACT_NONE		0
#define IMPACT_BODY		1
#define IMPACT_MATERIAL	2

#define COM_CopyString( s )	_COM_CopyString( s, __FILE__, __LINE__ )
matdef_t *COM_MatDefFromSurface( struct msurface_s *surf, const Vector &vecPoint );
matdef_t *COM_FindMatdef( const char *name );
matdef_t *COM_DefaultMatdef( void );
void COM_InitMatdef( void );

#endif//MATERIAL_H