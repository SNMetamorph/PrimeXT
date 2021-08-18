/*
shaders.h - parsing quake3 shaders for map-compile tools
Copyright (C) 2018 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef SHADERS_H
#define SHADERS_H

#define MAX_SHADER_INFO		8192
#define MAX_SHADERPATH		64

#define DEFAULT_LIGHTMAP_SAMPLE_SIZE	16
#define DEFAULT_LIGHTMAP_SAMPLE_OFFSET	1.0f

#define FSHADER_DEFAULT		0
#define FSHADER_NOCLIP		BIT( 0 )
#define FSHADER_NODRAW		BIT( 1 )
#define FSHADER_DETAIL		BIT( 2 )
#define FSHADER_NOLIGHTMAP		BIT( 3 )
#define FSHADER_SKIP		BIT( 4 )
#define FSHADER_CLIP		BIT( 5 )
#define FSHADER_HINT		BIT( 6 )
#define FSHADER_REMOVE		BIT( 7 )
#define FSHADER_DEFAULTED		BIT( 8 )		// user-shader is not specified
#define FSHADER_TRIGGER		BIT( 9 )

typedef vec_t			matrix3x3[3][3];

typedef struct surfaceParm_s
{
	const char	*name;
	int		contents;
	int		compileFlags;
} surfaceParm_t;

typedef struct shaderInfo_s
{
	char	name[MAX_SHADERPATH];
	int	contents;
	int	flags;
	float	value;				// light value

	int	lightmapSampleSize;			// lightmap sample size
	float	lightmapSampleOffset;		// ydnar: lightmap sample offset (default: 1.0)
	float	shadeAngleDegrees;			// ydnar: breaking angle for smooth shading (degrees)
	matrix3x3	mod;				// texture mod

	char	imagePath[MAX_SHADERPATH];		// path to real image

	char	implicitImagePath[MAX_SHADERPATH];
	char	editorImagePath[MAX_SHADERPATH];	// use this image to generate texture coordinates
	char	lightImagePath[MAX_SHADERPATH];	// use this image to generate color / averageColor
	char	skyParmsImageBase[MAX_SHADERPATH];
	char	*shaderText;

	float	vertexScale;			// vertex light scale
	bool	vertexShadows;			// shadows will be casted at this surface even when vertex lit
	bool	finished;
} shaderInfo_t;

shaderInfo_t *ShaderInfoForShader( const char *shaderName );
void LoadShaderInfo( void );
void FreeShaderInfo( void );

#endif//SHADERS_H