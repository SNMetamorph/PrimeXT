/*
ddstex.h - image dds encoder. Use squish library
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

#ifndef DDSTEX_H
#define DDSTEX_H

rgbdata_t *DDSToBuffer( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *DDSToRGBA( const char *name, const byte *buffer, size_t filesize );
rgbdata_t *BufferToDDS( rgbdata_t *pix, int saveformat );
int DDS_GetSaveFormatForHint( int hint, rgbdata_t *pix );

#endif//DDSTEX_H