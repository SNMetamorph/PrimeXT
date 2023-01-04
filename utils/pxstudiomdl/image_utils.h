/*
image_utils.h - utility routines for working with images
Copyright (C) 2015 Uncle Mike
Copyright (C) 2022 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H
#include "imagelib.h"
#include <stdint.h>

namespace ImageUtils
{
	rgbdata_t *LoadImageFile(const char *filename);
	rgbdata_t *LoadImageMemory(const char *filename, const byte *buf, size_t fileSize);
	void ApplyPaletteGamma(rgbdata_t *pic);
};

extern float	g_gamma;

#endif // IMAGE_UTILS_H
