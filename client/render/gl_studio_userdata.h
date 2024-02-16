#pragma once
#include "color24.h"
#include "lightlimits.h"
#include "mathlib.h"

/*
===========================

USER-DEFINED DATA

===========================
*/
// this struct may be expaned by user request
typedef struct vbomesh_s
{
	unsigned int	skinref;			// skin reference
	unsigned int	numVerts;			// trifan vertices count
	unsigned int	numElems;			// trifan elements count
	int		lightmapnum;		// each mesh should use only once atlas page!

	unsigned int	vbo, vao, ibo;		// buffer objects
	vec3_t		mins, maxs;		// right transform to get screencopy
	int		parentbone;		// parent bone to transform AABB
	unsigned short	uniqueID;			// to reject decal drawing
	unsigned int	cacheSize;		// debug info: uploaded cache size for this buffer
} vbomesh_t;

typedef struct mstudiosurface_s
{
	int		flags;			// match with msurface_t->flags
	int		texture_step;

	short		lightextents[2];
	unsigned short	light_s[MAXLIGHTMAPS];
	unsigned short	light_t[MAXLIGHTMAPS];
	byte		lights[MAXDYNLIGHTS];// static lights that affected this face (255 = no lights)

	int		lightmaptexturenum;
	byte		styles[MAXLIGHTMAPS];

	color24		*samples;		// note: this is the actual lightmap data for this surface
	color24		*normals;		// note: this is the actual deluxemap data for this surface
	byte		*shadows;		// note: occlusion map for this surface
} mstudiosurface_t;
	
typedef struct
{
	vbomesh_t		*meshes;			// meshes per submodel
	int		nummesh;			// mstudiomodel_t->nummesh
} msubmodel_t;

// triangles
typedef struct mbodypart_s
{
	int		base;			// mstudiobodyparts_t->base
	msubmodel_t	*models[MAXSTUDIOBODYPARTS];	// submodels per body part
	int		nummodels;		// mstudiobodyparts_t->nummodels
} mbodypart_t;

typedef struct mvbocache_s
{
	mstudiosurface_t	*surfaces;
	int		numsurfaces;

	mbodypart_t	*bodyparts;
	int		numbodyparts;

	bool		update_light;		// gamma or brightness was changed so we need to reload lightmaps
} mstudiocache_t;

typedef struct mposebone_s
{
	matrix3x4		posetobone[MAXSTUDIOBONES];
} mposetobone_t;
