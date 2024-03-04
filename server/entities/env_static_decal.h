/*
env_static_decal.h - analog for infodecal that works with new decals system
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

#pragma once
#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CStaticDecal : public CPointEntity
{
public:
	void KeyValue(KeyValueData *pkvd);
	int PasteDecal(int dir);
	void Spawn(void);
};
