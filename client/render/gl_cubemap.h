#pragma once
#include "vector.h"
#include "mathlib.h"
#include "gl_framebuffer.h"

// rebuilding cubemap states
enum CubemapBuildState
{
	CMREBUILD_INACTIVE,
	CMREBUILD_CHECKING,
	CMREBUILD_WAITING // waiting when engine finish cubemap baking
};

typedef struct mcubemap_s
{
	char		name[64];		// for envshots
	int		texture;		// gl texturenum
	vec3_t		mins, maxs;
	bool		valid;		// don't need to rebuild
	vec3_t		origin;
	short		size;		// cubemap size
	byte		numMips;		// cubemap mipcount
} mcubemap_t;
