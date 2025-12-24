/*
gl_shader.h - shader parsing and handling
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

#ifndef GL_SHADER_H
#define GL_SHADER_H
#include "custom.h"
#include "gl_export.h"
#include <vector>
#include <string>

#define MAX_OPTIONS_LENGTH		512
#define MAX_GLSL_PROGRAMS		4096

#define SHADER_VERTEX_COMPILED		BIT( 0 )
#define SHADER_FRAGMENT_COMPILED	BIT( 1 )
#define SHADER_PROGRAM_LINKED		BIT( 2 )
#define SHADER_UBERSHADER			BIT( 3 )
#define SHADER_USE_CUBEMAPS			BIT( 4 )
#define SHADER_USE_SCREENCOPY		BIT( 5 )
#define SHADER_ADDITIVE				BIT( 6 )

#define SHADER_STATUS_OK		( SHADER_PROGRAM_LINKED|SHADER_VERTEX_COMPILED|SHADER_FRAGMENT_COMPILED )
#define CheckShader( shader )		( shader && shader->status == SHADER_STATUS_OK )

enum
{
	ATTR_INDEX_POSITION = 0,
	ATTR_INDEX_TANGENT,
	ATTR_INDEX_BINORMAL,
	ATTR_INDEX_NORMAL,
	ATTR_INDEX_TEXCOORD0,	// texture coord
	ATTR_INDEX_TEXCOORD1,	// lightmap coord (styles0-1)
	ATTR_INDEX_TEXCOORD2,	// lightmap coord (styles2-3)
	ATTR_INDEX_BONE_INDEXES,	// studiomodels only
	ATTR_INDEX_BONE_WEIGHTS,	// studiomodels only
	ATTR_INDEX_LIGHT_STYLES,	// brushmodels only
	ATTR_INDEX_LIGHT_COLOR,	// studio & grass
	ATTR_INDEX_LIGHT_VECS,	// studio & grass
	ATTR_INDEX_LIGHT_NUMS0,	// brushmodels only
	ATTR_INDEX_LIGHT_NUMS1,	// brushmodels only
};

// shader->attribs
#define FATTR_POSITION		BIT( 0 )
#define FATTR_TANGENT		BIT( 1 )
#define FATTR_BINORMAL		BIT( 2 )
#define FATTR_NORMAL		BIT( 3 )
#define FATTR_TEXCOORD0		BIT( 4 )
#define FATTR_TEXCOORD1		BIT( 5 )
#define FATTR_TEXCOORD2		BIT( 6 )
#define FATTR_BONE_INDEXES		BIT( 7 )
#define FATTR_BONE_WEIGHTS		BIT( 8 )
#define FATTR_LIGHT_STYLES		BIT( 9 )
#define FATTR_LIGHT_COLOR		BIT( 10 )
#define FATTR_LIGHT_VECS		BIT( 11 )
#define FATTR_LIGHT_NUMS0		BIT( 12 )
#define FATTR_LIGHT_NUMS1		BIT( 13 )

// uniform->flags
#define UFL_GLOBAL_PARM		BIT( 0 )
#define UFL_TEXTURE_UNIT		BIT( 1 )

// uniform->type
typedef enum
{
	UT_COLORMAP = 0,
	UT_DEPTHMAP,
	UT_NORMALMAP,
	UT_GLOSSMAP,
	UT_DETAILMAP,
	UT_PROJECTMAP,		// spotlight texture
	UT_SHADOWMAP0,
	UT_SHADOWMAP1,
	UT_SHADOWMAP2,
	UT_SHADOWMAP3,
	UT_SHADOWMAP,
	UT_LIGHTMAP,
	UT_DELUXEMAP,
	UT_DECALMAP,
	UT_SCREENMAP,
	UT_VISLIGHTMAP0,
	UT_VISLIGHTMAP1,
	UT_ENVMAP0,
	UT_ENVMAP1,
	UT_ENVMAP,
	UT_SPECULARMAPIBL0,
	UT_SPECULARMAPIBL1,
	UT_BRDFAPPROXMAP,
	UT_GLOWMAP,
	UT_HEIGHTMAP,
	UT_LAYERMAP,
	UT_FITNORMALMAP,
	UT_MODELMATRIX,
	UT_REFLECTMATRIX,
	UT_BONESARRAY,
	UT_BONEQUATERNION,
	UT_BONEPOSITION,
	UT_SCREENSIZEINV,
	UT_ZFAR,
	UT_LIGHTSTYLEVALUES,
	UT_LIGHTSTYLES,
	UT_REALTIME,
	UT_TIMEDELTA,
	UT_DETAILSCALE,
	UT_FOGPARAMS,
	UT_SHADOWPARMS,
	UT_TEXOFFSET,
	UT_VIEWORIGIN,
	UT_VIEWRIGHT,
	UT_RENDERCOLOR,
	UT_RENDERALPHA,
	UT_SMOOTHNESS,
	UT_SHADOWMATRIX,
	UT_SHADOWSPLITDIST,
	UT_TEXELSIZE,
	UT_GAMMATABLE,
	UT_LIGHTDIR,
	UT_LIGHTDIFFUSE,
	UT_LIGHTSHADE,
	UT_LIGHTORIGIN,
	UT_LIGHTVIEWPROJMATRIX,
	UT_AMBIENTFACTOR,
	UT_AMBIENTCUBE,
	UT_SUNREFRACT,
	UT_LERPFACTOR,
	UT_REFRACTSCALE,
	UT_REFLECTSCALE,
	UT_SWAYHEIGHT,
	UT_ABERRATIONSCALE,
	UT_BOXMINS,
	UT_BOXMAXS,
	UT_CUBEORIGIN,
	UT_CUBEMIPCOUNT,
	UT_LIGHTNUMS0,
	UT_LIGHTNUMS1,
	UT_GRASSPARAMS,
	UT_RELIEFPARAMS,
	UT_BLURFACTOR,
	UT_MIPLOD,
	UT_TEXCOORDCLAMP,
	UT_BLOOMFIRSTPASS,
	UT_SCREENWIDTH,
	UT_SCREENHEIGHT,
	UT_FOCALDEPTH,
	UT_FOCALLENGTH,
	UT_DOFDEBUG,
	UT_FSTOP,
	UT_BRIGHTNESS,
	UT_SATURATION,
	UT_CONTRAST,
	UT_COLORLEVELS,
	UT_VIGNETTESCALE,
	UT_FILMGRAINSCALE,
	UT_ACCENTCOLOR,
	UT_LIGHTGAMMA,
	UT_LIGHTSCALE,
	UT_LIGHTTHRESHOLD,
	UT_NUMVISIBLEMODELS,
	UT_UNDEFINED,
} uniformType_t;

union unicache_t
{
	unicache_t( int v0 ) { iValue[0] = v0; iValue[1] = iValue[2] = iValue[3] = 0; }
	unicache_t( int v0, int v1 ) { iValue[0] = v0; iValue[1] = v1; iValue[2] = iValue[3] = 0; }
	unicache_t( int v0, int v1, int v2 ) { iValue[0] = v0; iValue[1] = v1; iValue[2] = v2; iValue[3] = 0; }
	unicache_t( int v0, int v1, int v2, int v3 ) { iValue[0] = v0; iValue[1] = v1; iValue[2] = v2; iValue[3] = v3; }
	unicache_t( float v0 ) { fValue[0] = v0; fValue[1] = fValue[2] = fValue[3] = 0.0f; }
	unicache_t( float v0, float v1 ) { fValue[0] = v0; fValue[1] = v1; fValue[2] = fValue[3] = 0.0f; }
	unicache_t( float v0, float v1, float v2 ) { fValue[0] = v0; fValue[1] = v1; fValue[2] = v2; fValue[3] = 0.0f; }
	unicache_t( float v0, float v1, float v2, float v3 ) { fValue[0] = v0; fValue[1] = v1; fValue[2] = v2; fValue[3] = v3; }

	float	fValue[4];
	int	iValue[4];
};

class uniform_t
{
public:
	char		name[MAX_QPATH];
	uniformType_t	type;
	int		size;
	uint		format;
	int		location;
	int		unit;	// texture unit
	int		flags;	// hints
	unicache_t	cache;

	// helpers
	void SetValue( float v0 )
	{
		unicache_t pack( v0 );
		SetValue( &pack );
	}

	void SetValue( float v0, float v1 )
	{
		unicache_t pack( v0, v1 );
		SetValue( &pack );
	}

	void SetValue( float v0, float v1, float v2 )
	{
		unicache_t pack( v0, v1, v2 );
		SetValue( &pack );
	}

	void SetValue( float v0, float v1, float v2, float v3 )
	{
		unicache_t pack( v0, v1, v2, v3 );
		SetValue( &pack );
	}

	void SetValue( int v0 )
	{
		unicache_t pack( v0 );
		SetValue( &pack );
	}

	void SetValue( int v0, int v1 )
	{
		unicache_t pack( v0, v1 );
		SetValue( &pack );
	}

	void SetValue( int v0, int v1, int v2 )
	{
		unicache_t pack( v0, v1, v2 );
		SetValue( &pack );
	}

	void SetValue( int v0, int v1, int v2, GLint v3 )
	{
		unicache_t pack( v0, v1, v2, v3 );
		SetValue( &pack );
	}

	// passed any data here
	void SetValue( const void *pdata, int count = -1 );
	int GetSizeInBytes( void );
};

struct glsl_prog_include
{
	glsl_prog_include() { 
		line = 0;
		rebasedLine = 0;
		lineCount = 0;
		name.clear();
		siblings.clear();
	};
	std::string name;
	int line;
	int rebasedLine;
	int lineCount;
	std::vector<glsl_prog_include> siblings;
};

typedef struct glsl_prog_s
{
	glsl_prog_s()
	{
		memset(name, 0, sizeof(name));
		memset(options, 0, sizeof(options));
		memset(vp_name, 0, sizeof(vp_name));
		memset(fp_name, 0, sizeof(fp_name));
		initialized = false;
		handle = 0;
		status = 0;
		attribs = 0;
		numUniforms = 0;
		uniforms = nullptr;
		nextHash = nullptr;
		sourceUnits = {};
	};
	char		name[64];
	char		options[MAX_OPTIONS_LENGTH];	// UberShader preprocess agrs
	char		vp_name[64];
	char		fp_name[64];
	bool		initialized;
	GLhandleARB	handle;
	int			status;
	int			attribs;
	uniform_t	*uniforms;
	int			numUniforms;
	std::vector<glsl_prog_include> sourceUnits;
	struct glsl_prog_s *nextHash;
} glsl_program_t;

extern glsl_program_t	glsl_programs[MAX_GLSL_PROGRAMS];
extern int		num_glsl_programs;

#endif//GL_SHADER_H