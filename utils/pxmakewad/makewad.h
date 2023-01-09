/*
makewad.h - convert textures into 8-bit indexed and pack into wad3 file
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

#ifndef MAKEWAD_H
#define MAKEWAD_H

#include "cmdlib.h"
#include "filesystem.h"
#include "miptex.h"

#define S_ERROR "^1error:^7 "

extern wfile_t *source_wad;
extern wfile_t *output_wad;
extern char output_path[256];
extern float resize_percent;

#endif//MAKEWAD_H