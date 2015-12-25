/*
conprint.h - extended printf function that allows
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

#ifndef CONPRINT_H
#define CONPRINT_H

void Con_Print( const char *pMsg );
void Con_Printf( const char *pMsg, ... );

#endif//CONRPINT_H
