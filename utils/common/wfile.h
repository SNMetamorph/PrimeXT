/*
wfile.h - simple version of game engine filesystem for tools
Copyright (C) 2018 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef WFILE_H
#define WFILE_H

// wad intermediate struct
struct wfile_s
{
	char		filename[256];
	int		infotableofs;
	int		numlumps;
	int		mode;
#ifdef ALLOW_WADS_IN_PACKS
	file_t		*handle;
#else
	int		handle;
#endif
	time_t		filetime;	
	dlumpinfo_t	*lumps;
};

#endif