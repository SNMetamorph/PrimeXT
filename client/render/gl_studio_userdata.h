#pragma once
#include "color24.h"
#include "lightlimits.h"
#include "mathlib.h"
#include "bounding_box.h"
#include <vector>
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
	~vbomesh_t();

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
	mstudiosurface_t() 
	{
		// remove this in case some non-PoD members added to this struct
		memset(this, 0, sizeof(mstudiosurface_t)); 
	};

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
	std::vector<vbomesh_t> meshes;		// meshes per submodel
};

// triangles
struct mbodypart_t
{
	mbodypart_t() : base(0) {};

	int	base;							// mstudiobodyparts_t->base
	std::vector<msubmodel_t*> models;	// submodels per body part 
};

struct mstudiocache_t
{
	mstudiocache_t() : update_light(false) {};

	std::vector<mstudiosurface_t> surfaces;
	std::vector<mbodypart_t> bodyparts;
	std::vector<msubmodel_t> submodels;
	bool update_light;		// gamma or brightness was changed so we need to reload lightmaps
};

struct mposetobone_t
{
	matrix3x4		posetobone[MAXSTUDIOBONES];
};
