/*
miptex.h - image quantizer, miptex creator
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef MIPTEX_H
#define MIPTEX_H

extern byte	lbmpalette[256*3];

bool MIP_CheckForReplace( dlumpinfo_t *find, rgbdata_t *image, int &width, int &height );
bool MIP_WriteMiptex( const char *lumpname, rgbdata_t *pix );

bool LMP_CheckForReplace( dlumpinfo_t *find, rgbdata_t *image, int &width, int &height );
bool LMP_WriteLmptex( const char *lumpname, rgbdata_t *pix, bool todisk = false );

#endif//MIPTEX_H