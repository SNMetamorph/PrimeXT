/*
gl_decals.h - decal project & rendering
this code written for Paranoia 2: Savior modification
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_DECALS_H
#define GL_DECALS_H
#include "texture_handle.h"

class DecalGroup;
class DecalGroupEntry
{
public:
	char		m_DecalName[64];
	TextureHandle	gl_diffuse_id;
	TextureHandle	gl_normalmap_id;
	TextureHandle	gl_heightmap_id;
	TextureHandle	gl_specular_id;		// specular
	int		xsize, ysize;
	matdesc_t		*matdesc;	// pointer to settings
	float		overlay;
	bool		has_alpha;	// solid decal doesn't use blend
	const DecalGroup	*group;	// get group name
	bool		m_init;

	void PreloadTextures( void );
};

class DecalGroup
{
public:
	DecalGroup( const char *name, int numelems, DecalGroupEntry *source );
	~DecalGroup();

	DecalGroupEntry *GetRandomDecal( void );
	DecalGroup *GetNext( void ) { return pnext; }
	const char *GetName( void ) { return m_chGroupName; }
	const char *GetName( void ) const { return m_chGroupName; }
	static DecalGroup *FindGroup( const char *name );
	DecalGroupEntry *FindEntry( const char *name );
	DecalGroupEntry *GetEntry( int num );
	static DecalGroupEntry *GetEntry( const char *name, int flags );
private:
	char m_chGroupName[16];
	DecalGroupEntry *pEntryArray;
	DecalGroup *pnext;
	int size;
};

typedef struct dvert_s
{
	Vector		vertex;		// position
	Vector		tangent;		// tangent
	Vector		binormal;		// binormal
	Vector		normal;		// normal
	float		stcoord0[4];	// ST texture coords + Decal coords
	float		lmcoord0[4];	// LM texture coords for styles 0-1
	float		lmcoord1[4];	// LM texture coords for styles 2-3
	byte		styles[4];	// lightstyles
} dvert_t;

// decal entry
typedef struct brushdecal_s
{
	// this part is goes to savelist
	byte			flags;
	short			entityIndex;
	Vector			position;
	Vector			impactPlaneNormal;
	float			angle; // goes into scale
	const DecalGroupEntry	*texinfo;

	// verts & elems
	dvert_t			*verts;	// pointer to cache array
	word			*elems;	// pointer to index array
	byte			numVerts;
	byte			numElems;

	// shader cache
	shader_t			forwardScene;
	mextrasurf_t		*surface;
	struct brushdecal_s		*pnext;	// linked list for each surface
	model_t			*model;
} brushdecal_t;

void DecalsInit( void );
void ClearDecals( void );
void DecalsShutdown( void );
void R_RenderDecalsSolidList( drawlist_t drawlist_type );
void R_RenderDecalsTransList( drawlist_t drawlist_type );
void R_RenderDecalsTransEntry( CTransEntry *entry, drawlist_t drawlist_type );
int SaveDecalList( decallist_t *pBaseList, int count );

#endif//GL_DECALS_H