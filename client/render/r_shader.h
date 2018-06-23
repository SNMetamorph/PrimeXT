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

#define MAX_OPTIONS_LENGTH		512
#define MAX_GLSL_PROGRAMS		512

#define SHADER_VERTEX_COMPILED	BIT( 0 )
#define SHADER_FRAGMENT_COMPILED	BIT( 1 )
#define SHADER_PROGRAM_LINKED		BIT( 2 )
#define SHADER_UBERSHADER		BIT( 3 )
#define SHADER_USE_SCREENCOPY		BIT( 4 )

#define SHADER_STATUS_OK		( SHADER_PROGRAM_LINKED|SHADER_VERTEX_COMPILED|SHADER_FRAGMENT_COMPILED )
#define CheckShader( shader )		( shader && shader->status == SHADER_STATUS_OK )
#define ScreenCopyRequired( x )	( x && FBitSet( x->status, SHADER_USE_SCREENCOPY ))

enum
{
	ATTR_INDEX_POSITION = 0,
	ATTR_INDEX_NORMAL,
	ATTR_INDEX_TEXCOORD0,	// texture coord
	ATTR_INDEX_TEXCOORD1,	// lightmap coord (styles0-1)
	ATTR_INDEX_TEXCOORD2,	// lightmap coord (styles2-3)
	ATTR_INDEX_BONE_INDEXES,	// studiomodels only
	ATTR_INDEX_BONE_WEIGHTS,	// studiomodels only
	ATTR_INDEX_LIGHT_STYLES,	// brushmodels only
	ATTR_INDEX_LIGHT_COLOR,	// studio & grass
	ATTR_INDEX_LIGHT_VECS,	// studio & grass
};

typedef struct glsl_prog_s
{
	char		name[64];
	char		options[MAX_OPTIONS_LENGTH];	// UberShader preprocess agrs
	GLhandleARB	handle;
	unsigned short	status;

	struct glsl_prog_s	*nextHash;

	// sampler parameters
	GLint		u_ColorMap;	// 0-th unit
	GLint		u_DepthMap;	// 1-th unit
	GLint		u_NormalMap;	// 1-th unit
	GLint		u_GlossMap;	// 2-th unit
	GLint		u_DetailMap;	// 6-th unit
	GLint		u_ProjectMap;	// 0-th unit
	GLint		u_ShadowMap;	// 2-th unit
	GLint		u_AttnZMap;	// 1-th unit
	GLint		u_LightMap;	// 1-th unit
	GLint		u_DeluxeMap;	// 3-th unit (lightvectors)
	GLint		u_DecalMap;	// 0-th unit
	GLint		u_ScreenMap;	// 2-th unit
	GLint		u_EnvCube0;	// env_cubemap
	GLint		u_EnvCube1;	// env_cubemap
	GLint		u_GlowMap;	// luma

	// uniform parameters
	GLint		u_BonesArray;	// bones compacted into vec4 arrays
	GLint		u_BoneQuaternion;
	GLint		u_BonePosition;
	GLint		u_GammaTable;
	GLint		u_ViewMatrix;
	GLint		u_ModelMatrix;
	GLint		u_ModelViewMatrix;
	GLint		u_LightViewProjectionMatrix;
	GLint		u_ModelViewProjectionMatrix;
	GLint		u_GenericCondition;	// generic parm
	GLint		u_GenericCondition2;// generic parm
	GLint		u_BlurFactor;	// blur factor
	GLint		u_ScreenWidth;
	GLint		u_ScreenHeight;
	GLint		u_ScreenSizeInv;
	GLint		u_zFar;		// global z-far value
	GLint		u_FocalDepth;
	GLint		u_FocalLength;
	GLint		u_FStop;
	GLint		u_LightDir;
	GLint		u_LightColor;
	GLint		u_LightAmbient;
	GLint		u_LightShade;
	GLint		u_LightDiffuse;
	GLint		u_LightSamples;	// sample count
	GLint		u_LightStyleValues;	// array
	GLint		u_AmbientCube;
	GLint		u_AmbientFactor;
	GLint		u_DiffuseFactor;
	GLint		u_Smoothness;
	GLint		u_FogParams;
	GLint		u_LightOrigin;
	GLint		u_LightScale;
	GLint		u_ViewOrigin;
	GLint		u_ViewRight;
	GLint		u_FaceFlags;
	GLint		u_LerpFactor;
	GLint		u_FaceTangent;
	GLint		u_FaceBinormal;
	GLint		u_FaceNormal;
	GLint		u_AberrationScale;
	GLint		u_ShadowMode;
	GLint		u_ShadowParams;
	GLint		u_DetailScale;
	GLint		u_RenderAlpha;
	GLint		u_RefractScale;
	GLint		u_ReflectScale;
	GLint		u_TexOffset;	// e.g. conveyor belt
	GLint		u_RenderColor;	// color + alpha
	GLint		u_RealTime;	// tr.time
	GLint		u_BoxMins;	// cubemaps stuff
	GLint		u_BoxMaxs;	// cubemaps stuff
	GLint		u_CubeOrigin;	// cubemaps stuff
	GLint		u_CubeMipCount;	// cubemaps stuff
	GLint		u_GrassFadeStart;
	GLint		u_GrassFadeDist;
	GLint		u_GrassFadeEnd;
} glsl_program_t;

typedef struct
{
	// post-process shaders
	glsl_program_t	*genSunShafts;		// sunshafts effect
	glsl_program_t	*blurShader[2];		// e.g. underwater blur
	glsl_program_t	*drawSunShafts;		// sunshafts effect
	glsl_program_t	*skyboxEnv;		// skybox & sun
	glsl_program_t	*genericFog;		// apply fog

	glsl_program_t	*bmodelDepthFill;		// shadow pass for world
	glsl_program_t	*studioDepthFill[2];	// shadow pass for studio (unweighted, weighted)
	glsl_program_t	*grassDepthFill;		// shadow pass for grass
} ref_shaders_t;

extern glsl_program_t	glsl_programs[MAX_GLSL_PROGRAMS];
extern unsigned int		num_glsl_programs;
extern ref_shaders_t	glsl;

#endif//GL_SHADER_H
