/*
wlfetch.h - fetch worldlight params
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

#ifndef WLFETCH_H
#define WLFETCH_H

// helper macroces
#define wltype()		( worldlight[0].a )
#define wlnormal()		( UnpackNormal( worldlight[0].x ))
#define wlpos()		( worldlight[1].xyz )
#define wlintensity()	( worldlight[2].rgb )
#define wlstopdot()		( worldlight[0].y )
#define wlstopdot2()	( worldlight[0].z )
#define wlstyle()		( worldlight[2].a )
#define wlfalloff()		( worldlight[1].a )

#endif//WLFETCH_H