/*
waddesc.h - extended printf function that allows
colored printing scheme from Quake3
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef WADDESC_H
#define WADDESC_H

#include "../../common/wadfile.h"

typedef struct
{
	int		ident;		// should be IWAD, WAD2 or WAD3
	int		numlumps;		// num files
	int		infotableofs;
} dwadinfo_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size;		// uncompressed
	char		type;
	char		compression;	// probably not used
	char		pad1;
	char		pad2;
	char		name[16];		// must be null terminated
} dlumpinfo_t;

#endif//WADDESC_H
