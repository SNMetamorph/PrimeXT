#pragma once
#include "color24.h"
#include "lightlimits.h"
#include "mathlib.h"
#include "bounding_box.h"
#include <unordered_map>

/*
===========================

USER-DEFINED DATA

===========================
*/
// this struct may be expaned by user request
struct vbomesh_t
{
	vbomesh_t() :
		skinref(0), 
		numVerts(0), 
		numElems(0),
		lightmapnum(0),
		vbo(0), 
		vao(0), 
		ibo(0),
		parentbone(0),
		uniqueID(0),
		cacheSize(0) {};

	unsigned int	skinref;			// skin reference
	unsigned int	numVerts;			// trifan vertices count
	unsigned int	numElems;			// trifan elements count
	int		lightmapnum;				// each mesh should use only once atlas page!

	unsigned int	vbo, vao, ibo;		// buffer objects
	std::unordered_map<int32_t, CBoundingBox> boneBounds; // right transform to get screencopy
	int		parentbone;					// parent bone to transform AABB
	unsigned short	uniqueID;			// to reject decal drawing
	unsigned int	cacheSize;			// debug info: uploaded cache size for this buffer
};

struct mstudiosurface_t
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
};
	
struct msubmodel_t
{
	vbomesh_t		*meshes;			// meshes per submodel
	int		nummesh;			// mstudiomodel_t->nummesh
};

// triangles
struct mbodypart_t
{
	int		base;			// mstudiobodyparts_t->base
	msubmodel_t	*models[MAXSTUDIOBODYPARTS];	// submodels per body part
	int		nummodels;		// mstudiobodyparts_t->nummodels
};

struct mstudiocache_t
{
	mstudiosurface_t	*surfaces;
	int		numsurfaces;

	mbodypart_t	*bodyparts;
	int		numbodyparts;

	bool		update_light;		// gamma or brightness was changed so we need to reload lightmaps
};

struct mposetobone_t
{
	matrix3x4		posetobone[MAXSTUDIOBONES];
};
