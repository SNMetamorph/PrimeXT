/*
r_world.h - local world data for rendering
this code written for Paranoia 2: Savior modification
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

#ifndef R_WORLD_H
#define R_WORLD_H

// world features
#define WORLD_HAS_MOVIES	BIT( 0 )
#define WORLD_HAS_MIRRORS	BIT( 1 )
#define WORLD_HAS_PORTALS	BIT( 2 )
#define WORLD_HAS_SCREENS	BIT( 3 )
#define WORLD_HAS_GRASS	BIT( 4 )
#define WORLD_HAS_DELUXEMAP	BIT( 5 )
#define WORLD_HAS_SKYBOX	BIT( 6 )
#define WORLD_WATERALPHA	BIT( 7 )

#define NORMALS_UNKNOWN	0
#define NORMALS_VERTEX_BASED	1		// old wrong method
#define NORMALS_INDEXED	2		// new right method

typedef struct
{
	dlightcube_t	cube;
	vec3_t		origin;
	struct mworldleaf_s	*leaf;		// ambient linked into this leaf
} mlightprobe_t;

// this is custom world nodes and leaf to store various data
typedef struct mworldnode_s
{
	// common with leaf
	int		contents;			// 0, to differentiate from leafs
	int		visframe[MAX_REF_STACK];	// node needs to be traversed if current

	Vector		mins, maxs;		// for bounding box culling
	struct mworldnode_s	*parent;

// node specific
	mplane_t		*plane;
	struct mworldnode_s	*children[2];	

	unsigned short	firstsurface;
	unsigned short	numsurfaces;
} mworldnode_t;

typedef struct mworldleaf_s
{
	// common with node
	int		contents;			// 0, to differentiate from leafs
	int		visframe[MAX_REF_STACK];	// node needs to be traversed if current

	Vector		mins, maxs;		// for bounding box culling
	struct mworldnode_s	*parent;

// leaf specific
	struct efrag_s	**efrags;			// for custom efrag storing

	msurface_t	**firstmarksurface;
	int		nummarksurfaces;
	int		cluster;			// helper to acess to uncompressed visdata

	mlightprobe_t	*ambient_light;
	byte		num_lightprobes;
	byte		fogDensity;
} mworldleaf_t;

struct BmodelInstance_t
{
	cl_entity_t	*m_pEntity;

	// bounds info
	Vector		bbox[8];
	Vector		absmin;
	Vector		absmax;
	float		radius;

	lightinfo_t	light;		// cached light values
	matrix4x4		m_transform;
	GLfloat		m_gltransform;
};
	
typedef struct
{
	char		name[64];		// to avoid reloading on same

	word		features;		// world features

	mworldleaf_t	*leafs;		// [worldmodel->numleafs]
	int		numleafs;		// [submodels[0].visleafs + 1]
	mworldnode_t	*nodes;		// [worldmodel->numodes]
	int		numnodes;

	dvertnorm_t	*surfnormals;	// is not NULL here a indexed normals
	dnormal_t		*normals;
	int		numnormals;

	int		numleaflights;
	mlightprobe_t	*leaflights;

	terrain_t		*terrains;
	int		num_terrains;

	bvert_t		*vertexes;
	int		numvertexes;

	dvlightlump_t	*vertex_lighting;	// used for env_statics

	// single buffer for all the models
	unsigned int	vertex_buffer_object;
	unsigned int	vertex_array_object;

	unsigned short	*sortedfaces;	// surfaces sorted through all models
	unsigned short	numsortedfaces;

	// misc info
	int		grasscount;	// number of bushes per world (used to determine total VBO size)
	int		grassmem;		// total video memory that used by grass 
	Vector2D		orthocenter;	// overview stuff
	Vector2D		orthohalf;
} gl_world_t;

extern gl_world_t	*world;

#endif//GL_WORLD_H
