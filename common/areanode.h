/*
areanode.h - arenode description
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

#ifndef AREANODE_H
#define AREANODE_H

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
	link_t		portal_edicts;
} areanode_t;

#define STRUCT_FROM_LINK( l, t, m )		((t *)((byte *)l - (int)&(((t *)0)->m)))
#define EDICT_FROM_AREA( l )			STRUCT_FROM_LINK( l, edict_t, area )
#define FACET_FROM_AREA( l )			STRUCT_FROM_LINK( l, mfacet_t, area )

#endif//AREANODE_H