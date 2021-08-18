/*
texrep.cpp - replace internal bsp textures with external textures (from specified wad)
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#define EXTERN

#include "gl_export.h"
#include "conprint.h"
#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <stdio.h>
#include <io.h>
#include <stringlib.h>

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define MIN_SHADER_UNIFOMS		1024	// GLSL spec says that at least 1024 uniform is allowed on sm 2.0
#define MAX_RESERVED_UNIFORMS		256	// while MAX_LIGHTSTYLES 64
#define WINDOW_STYLE		(WS_OVERLAPPED|WS_BORDER|WS_SYSMENU|WS_CAPTION)
#define WINDOW_EX_STYLE		(0)
#define WINDOW_NAME			"Xash Window" // Half-Life

typedef HMODULE dllhandle_t;

typedef struct dllfunc_s
{
	const char *name;
	void	**func;
} dllfunc_t;

typedef enum
{
	rserr_ok,
	rserr_invalid_fullscreen,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

void *Sys_GetProcAddress( dllhandle_t handle, const char *name )
{
	return (void *)GetProcAddress( handle, name );
}

void Sys_FreeLibrary( dllhandle_t *handle )
{
	if( !handle || !*handle )
		return;

	FreeLibrary( *handle );
	*handle = NULL;
}

bool Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunc_t *fcts )
{
	const dllfunc_t *gamefunc;
	dllhandle_t dllhandle;

	if( !handle ) return false;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->func = NULL;

	dllhandle = LoadLibrary( dllname );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
	{
		if( !( *gamefunc->func = (void *)Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_FreeLibrary( &dllhandle );
			return false;
		}
	}          

	*handle = dllhandle;

	return true;
}

/*
=======================================================================

 GL STATE MACHINE

=======================================================================
*/
enum
{
	R_OPENGL_110 = 0,		// base
	R_WGL_PROCADDRESS,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_ARB_VERTEX_ARRAY_OBJECT_EXT,
	R_EXT_GPU_SHADER4,		// shaders only
	R_ARB_MULTITEXTURE,
	R_TEXTURECUBEMAP_EXT,
	R_SHADER_GLSL100_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_TEXTURE_3D_EXT,
	R_SHADER_OBJECTS_EXT,
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_ARB_TEXTURE_NPOT_EXT,
	R_TEXTURE_ARRAY_EXT,
	R_DEPTH_TEXTURE,
	R_SHADOW_EXT,
	R_FRAMEBUFFER_OBJECT,
	R_PARANOIA_EXT,		// custom OpenGL32.dll with hacked function glDepthRange
	R_DEBUG_OUTPUT,
	R_ARB_TEXTURE_FLOAT_EXT,
	R_DRAW_BUFFERS_EXT,
	R_ARB_HALF_FLOAT_EXT,
	R_EXTCOUNT,		// must be last
};

typedef struct
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*version_string;
	const char	*vendor_string;

	// list of supported extensions
	const char	*extensions_string;
	bool		extension[R_EXTCOUNT];

	int		block_size;		// lightmap blocksize
	
	int		max_texture_units;
	int		max_texture_coords;
	int		max_teximage_units;
	GLint		max_2d_texture_size;
	GLint		max_2d_texture_layers;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_size;

	int		color_bits;
	int		alpha_bits;
	int		depth_bits;
	int		stencil_bits;

	int		max_vertex_uniforms;
	int		max_vertex_attribs;
	int		max_skinning_bones;		// total bones that can be transformed with GLSL
} glConfig_t;

typedef struct
{
	HWND		hWnd;
	HDC		hDC;		// handle to device context
	HGLRC		hGLRC;		// handle to GL rendering context
	HINSTANCE		hInst;

	int		desktopBitsPixel;
	int		desktopWidth;
	int		desktopHeight;

	bool		software;		// OpenGL software emulation
	bool		initialized;	// OpenGL subsystem started
} glwstate_t;

glwstate_t	glw_state;
glConfig_t	glConfig;

static dllfunc_t opengl_110funcs[] =
{
{ "glClearColor"         	, (void **)&pglClearColor },
{ "glClear"              	, (void **)&pglClear },
{ "glAlphaFunc"          	, (void **)&pglAlphaFunc },
{ "glBlendFunc"          	, (void **)&pglBlendFunc },
{ "glCullFace"           	, (void **)&pglCullFace },
{ "glDrawBuffer"         	, (void **)&pglDrawBuffer },
{ "glReadBuffer"         	, (void **)&pglReadBuffer },
{ "glAccum"         	, (void **)&pglAccum },
{ "glEnable"             	, (void **)&pglEnable },
{ "glDisable"            	, (void **)&pglDisable },
{ "glEnableClientState"  	, (void **)&pglEnableClientState },
{ "glDisableClientState" 	, (void **)&pglDisableClientState },
{ "glGetBooleanv"        	, (void **)&pglGetBooleanv },
{ "glGetDoublev"         	, (void **)&pglGetDoublev },
{ "glGetFloatv"          	, (void **)&pglGetFloatv },
{ "glGetIntegerv"        	, (void **)&pglGetIntegerv },
{ "glGetError"           	, (void **)&pglGetError },
{ "glGetString"          	, (void **)&pglGetString },
{ "glFinish"             	, (void **)&pglFinish },
{ "glFlush"              	, (void **)&pglFlush },
{ "glClearDepth"         	, (void **)&pglClearDepth },
{ "glDepthFunc"          	, (void **)&pglDepthFunc },
{ "glDepthMask"          	, (void **)&pglDepthMask },
{ "glDepthRange"         	, (void **)&pglDepthRange },
{ "glFrontFace"          	, (void **)&pglFrontFace },
{ "glDrawElements"       	, (void **)&pglDrawElements },
{ "glColorMask"          	, (void **)&pglColorMask },
{ "glIndexPointer"       	, (void **)&pglIndexPointer },
{ "glVertexPointer"      	, (void **)&pglVertexPointer },
{ "glNormalPointer"      	, (void **)&pglNormalPointer },
{ "glColorPointer"       	, (void **)&pglColorPointer },
{ "glTexCoordPointer"    	, (void **)&pglTexCoordPointer },
{ "glArrayElement"       	, (void **)&pglArrayElement },
{ "glColor3f"            	, (void **)&pglColor3f },
{ "glColor3fv"           	, (void **)&pglColor3fv },
{ "glColor4f"            	, (void **)&pglColor4f },
{ "glColor4fv"           	, (void **)&pglColor4fv },
{ "glColor3ub"           	, (void **)&pglColor3ub },
{ "glColor4ub"           	, (void **)&pglColor4ub },
{ "glColor4ubv"          	, (void **)&pglColor4ubv },
{ "glTexCoord1f"         	, (void **)&pglTexCoord1f },
{ "glTexCoord2f"         	, (void **)&pglTexCoord2f },
{ "glTexCoord3f"         	, (void **)&pglTexCoord3f },
{ "glTexCoord4f"         	, (void **)&pglTexCoord4f },
{ "glTexCoord1fv"        	, (void **)&pglTexCoord1fv },
{ "glTexCoord2fv"        	, (void **)&pglTexCoord2fv },
{ "glTexCoord3fv"        	, (void **)&pglTexCoord3fv },
{ "glTexCoord4fv"        	, (void **)&pglTexCoord4fv },
{ "glTexGenf"            	, (void **)&pglTexGenf },
{ "glTexGenfv"           	, (void **)&pglTexGenfv },
{ "glTexGeni"            	, (void **)&pglTexGeni },
{ "glVertex2f"           	, (void **)&pglVertex2f },
{ "glVertex3f"           	, (void **)&pglVertex3f },
{ "glVertex3fv"          	, (void **)&pglVertex3fv },
{ "glNormal3f"           	, (void **)&pglNormal3f },
{ "glNormal3fv"          	, (void **)&pglNormal3fv },
{ "glBegin"              	, (void **)&pglBegin },
{ "glEnd"                	, (void **)&pglEnd },
{ "glLineWidth"          	, (void**)&pglLineWidth },
{ "glPointSize"          	, (void**)&pglPointSize },
{ "glMatrixMode"         	, (void **)&pglMatrixMode },
{ "glOrtho"              	, (void **)&pglOrtho },
{ "glRasterPos2f"        	, (void **) &pglRasterPos2f },
{ "glFrustum"            	, (void **)&pglFrustum },
{ "glViewport"           	, (void **)&pglViewport },
{ "glPushMatrix"         	, (void **)&pglPushMatrix },
{ "glPopMatrix"          	, (void **)&pglPopMatrix },
{ "glPushAttrib"         	, (void **)&pglPushAttrib },
{ "glPopAttrib"          	, (void **)&pglPopAttrib },
{ "glLoadIdentity"       	, (void **)&pglLoadIdentity },
{ "glLoadMatrixd"        	, (void **)&pglLoadMatrixd },
{ "glLoadMatrixf"        	, (void **)&pglLoadMatrixf },
{ "glMultMatrixd"        	, (void **)&pglMultMatrixd },
{ "glMultMatrixf"        	, (void **)&pglMultMatrixf },
{ "glRotated"            	, (void **)&pglRotated },
{ "glRotatef"            	, (void **)&pglRotatef },
{ "glScaled"             	, (void **)&pglScaled },
{ "glScalef"             	, (void **)&pglScalef },
{ "glTranslated"         	, (void **)&pglTranslated },
{ "glTranslatef"         	, (void **)&pglTranslatef },
{ "glReadPixels"         	, (void **)&pglReadPixels },
{ "glDrawPixels"         	, (void **)&pglDrawPixels },
{ "glStencilFunc"        	, (void **)&pglStencilFunc },
{ "glStencilMask"        	, (void **)&pglStencilMask },
{ "glStencilOp"          	, (void **)&pglStencilOp },
{ "glClearStencil"       	, (void **)&pglClearStencil },
{ "glIsEnabled"          	, (void **)&pglIsEnabled },
{ "glIsList"             	, (void **)&pglIsList },
{ "glIsTexture"          	, (void **)&pglIsTexture },
{ "glTexEnvf"            	, (void **)&pglTexEnvf },
{ "glTexEnvfv"           	, (void **)&pglTexEnvfv },
{ "glTexEnvi"            	, (void **)&pglTexEnvi },
{ "glTexParameterf"      	, (void **)&pglTexParameterf },
{ "glTexParameterfv"     	, (void **)&pglTexParameterfv },
{ "glTexParameteri"      	, (void **)&pglTexParameteri },
{ "glHint"               	, (void **)&pglHint },
{ "glPixelStoref"        	, (void **)&pglPixelStoref },
{ "glPixelStorei"        	, (void **)&pglPixelStorei },
{ "glGenTextures"        	, (void **)&pglGenTextures },
{ "glDeleteTextures"     	, (void **)&pglDeleteTextures },
{ "glBindTexture"        	, (void **)&pglBindTexture },
{ "glTexImage1D"         	, (void **)&pglTexImage1D },
{ "glTexImage2D"         	, (void **)&pglTexImage2D },
{ "glTexSubImage1D"      	, (void **)&pglTexSubImage1D },
{ "glTexSubImage2D"      	, (void **)&pglTexSubImage2D },
{ "glCopyTexImage1D"     	, (void **)&pglCopyTexImage1D },
{ "glCopyTexImage2D"     	, (void **)&pglCopyTexImage2D },
{ "glCopyTexSubImage1D"  	, (void **)&pglCopyTexSubImage1D },
{ "glCopyTexSubImage2D"  	, (void **)&pglCopyTexSubImage2D },
{ "glScissor"            	, (void **)&pglScissor },
{ "glGetTexImage"		, (void **)&pglGetTexImage },
{ "glGetTexEnviv"        	, (void **)&pglGetTexEnviv },
{ "glPolygonOffset"      	, (void **)&pglPolygonOffset },
{ "glPolygonMode"        	, (void **)&pglPolygonMode },
{ "glPolygonStipple"     	, (void **)&pglPolygonStipple },
{ "glClipPlane"          	, (void **)&pglClipPlane },
{ "glGetClipPlane"       	, (void **)&pglGetClipPlane },
{ "glShadeModel"         	, (void **)&pglShadeModel },
{ "glGetTexLevelParameteriv"	, (void **)&pglGetTexLevelParameteriv },
{ "glGetTexLevelParameterfv"	, (void **)&pglGetTexLevelParameterfv },
{ "glFogfv"              	, (void **)&pglFogfv },
{ "glFogf"               	, (void **)&pglFogf },
{ "glFogi"               	, (void **)&pglFogi },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsfuncs[] =
{
{ "glDrawRangeElements" , (void **)&pglDrawRangeElements },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsextfuncs[] =
{
{ "glDrawRangeElementsEXT" , (void **)&pglDrawRangeElementsEXT },
{ NULL, NULL }
};

static dllfunc_t debugoutputfuncs[] =
{
{ "glDebugMessageControlARB" , (void **)&pglDebugMessageControlARB },
{ "glDebugMessageInsertARB" , (void **)&pglDebugMessageInsertARB },
{ "glDebugMessageCallbackARB" , (void **)&pglDebugMessageCallbackARB },
{ "glGetDebugMessageLogARB" , (void **)&pglGetDebugMessageLogARB },
{ NULL, NULL }
};

static dllfunc_t multitexturefuncs[] =
{
{ "glMultiTexCoord1fARB"     , (void **)&pglMultiTexCoord1f },
{ "glMultiTexCoord2fARB"     , (void **)&pglMultiTexCoord2f },
{ "glMultiTexCoord3fARB"     , (void **)&pglMultiTexCoord3f },
{ "glMultiTexCoord4fARB"     , (void **)&pglMultiTexCoord4f },
{ "glActiveTextureARB"       , (void **)&pglActiveTexture },
{ "glActiveTextureARB"       , (void **)&pglActiveTextureARB },
{ "glClientActiveTextureARB" , (void **)&pglClientActiveTexture },
{ "glClientActiveTextureARB" , (void **)&pglClientActiveTextureARB },
{ NULL, NULL }
};

static dllfunc_t texture3dextfuncs[] =
{
{ "glTexImage3DEXT"        , (void **)&pglTexImage3D },
{ "glTexSubImage3DEXT"     , (void **)&pglTexSubImage3D },
{ "glCopyTexSubImage3DEXT" , (void **)&pglCopyTexSubImage3D },
{ NULL, NULL }
};

static dllfunc_t shaderobjectsfuncs[] =
{
{ "glDeleteObjectARB"             , (void **)&pglDeleteObjectARB },
{ "glGetHandleARB"                , (void **)&pglGetHandleARB },
{ "glDetachObjectARB"             , (void **)&pglDetachObjectARB },
{ "glCreateShaderObjectARB"       , (void **)&pglCreateShaderObjectARB },
{ "glShaderSourceARB"             , (void **)&pglShaderSourceARB },
{ "glCompileShaderARB"            , (void **)&pglCompileShaderARB },
{ "glCreateProgramObjectARB"      , (void **)&pglCreateProgramObjectARB },
{ "glAttachObjectARB"             , (void **)&pglAttachObjectARB },
{ "glLinkProgramARB"              , (void **)&pglLinkProgramARB },
{ "glUseProgramObjectARB"         , (void **)&pglUseProgramObjectARB },
{ "glValidateProgramARB"          , (void **)&pglValidateProgramARB },
{ "glUniform1fARB"                , (void **)&pglUniform1fARB },
{ "glUniform2fARB"                , (void **)&pglUniform2fARB },
{ "glUniform3fARB"                , (void **)&pglUniform3fARB },
{ "glUniform4fARB"                , (void **)&pglUniform4fARB },
{ "glUniform1iARB"                , (void **)&pglUniform1iARB },
{ "glUniform2iARB"                , (void **)&pglUniform2iARB },
{ "glUniform3iARB"                , (void **)&pglUniform3iARB },
{ "glUniform4iARB"                , (void **)&pglUniform4iARB },
{ "glUniform1fvARB"               , (void **)&pglUniform1fvARB },
{ "glUniform2fvARB"               , (void **)&pglUniform2fvARB },
{ "glUniform3fvARB"               , (void **)&pglUniform3fvARB },
{ "glUniform4fvARB"               , (void **)&pglUniform4fvARB },
{ "glUniform1ivARB"               , (void **)&pglUniform1ivARB },
{ "glUniform2ivARB"               , (void **)&pglUniform2ivARB },
{ "glUniform3ivARB"               , (void **)&pglUniform3ivARB },
{ "glUniform4ivARB"               , (void **)&pglUniform4ivARB },
{ "glUniformMatrix2fvARB"         , (void **)&pglUniformMatrix2fvARB },
{ "glUniformMatrix3fvARB"         , (void **)&pglUniformMatrix3fvARB },
{ "glUniformMatrix4fvARB"         , (void **)&pglUniformMatrix4fvARB },
{ "glGetObjectParameterfvARB"     , (void **)&pglGetObjectParameterfvARB },
{ "glGetObjectParameterivARB"     , (void **)&pglGetObjectParameterivARB },
{ "glGetInfoLogARB"               , (void **)&pglGetInfoLogARB },
{ "glGetAttachedObjectsARB"       , (void **)&pglGetAttachedObjectsARB },
{ "glGetUniformLocationARB"       , (void **)&pglGetUniformLocationARB },
{ "glGetActiveUniformARB"         , (void **)&pglGetActiveUniformARB },
{ "glGetUniformfvARB"             , (void **)&pglGetUniformfvARB },
{ "glGetUniformivARB"             , (void **)&pglGetUniformivARB },
{ "glGetShaderSourceARB"          , (void **)&pglGetShaderSourceARB },
{ "glVertexAttribPointerARB"      , (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"  , (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB" , (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"       , (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"          , (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"        , (void **)&pglGetAttribLocationARB },
{ "glVertexAttrib2f"              , (void **)&pglVertexAttrib2fARB },
{ "glVertexAttrib2fv"             , (void **)&pglVertexAttrib2fvARB },
{ "glVertexAttrib3fv"             , (void **)&pglVertexAttrib3fvARB },
{ NULL, NULL }
};

static dllfunc_t vertexshaderfuncs[] =
{
{ "glVertexAttribPointerARB"      , (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"  , (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB" , (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"       , (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"          , (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"        , (void **)&pglGetAttribLocationARB },
{ NULL, NULL }
};

static dllfunc_t vbofuncs[] =
{
{ "glBindBufferARB"    , (void **)&pglBindBufferARB },
{ "glDeleteBuffersARB" , (void **)&pglDeleteBuffersARB },
{ "glGenBuffersARB"    , (void **)&pglGenBuffersARB },
{ "glIsBufferARB"      , (void **)&pglIsBufferARB },
{ "glMapBufferARB"     , (void **)&pglMapBufferARB },
{ "glUnmapBufferARB"   , (void **)&pglUnmapBufferARB },
{ "glBufferDataARB"    , (void **)&pglBufferDataARB },
{ "glBufferSubDataARB" , (void **)&pglBufferSubDataARB },
{ NULL, NULL }
};

static dllfunc_t vaofuncs[] =
{
{ "glBindVertexArray"    , (void **)&pglBindVertexArray },
{ "glDeleteVertexArrays" , (void **)&pglDeleteVertexArrays },
{ "glGenVertexArrays"    , (void **)&pglGenVertexArrays },
{ "glIsVertexArray"      , (void **)&pglIsVertexArray },
{ NULL, NULL }
};

static dllfunc_t fbofuncs[] =
{
{ "glIsRenderbuffer"                      , (void **)&pglIsRenderbuffer },
{ "glBindRenderbuffer"                    , (void **)&pglBindRenderbuffer },
{ "glDeleteRenderbuffers"                 , (void **)&pglDeleteRenderbuffers },
{ "glGenRenderbuffers"                    , (void **)&pglGenRenderbuffers },
{ "glRenderbufferStorage"                 , (void **)&pglRenderbufferStorage },
{ "glGetRenderbufferParameteriv"          , (void **)&pglGetRenderbufferParameteriv },
{ "glIsFramebuffer"                       , (void **)&pglIsFramebuffer },
{ "glBindFramebuffer"                     , (void **)&pglBindFramebuffer },
{ "glDeleteFramebuffers"                  , (void **)&pglDeleteFramebuffers },
{ "glGenFramebuffers"                     , (void **)&pglGenFramebuffers },
{ "glCheckFramebufferStatus"              , (void **)&pglCheckFramebufferStatus },
{ "glFramebufferTexture1D"                , (void **)&pglFramebufferTexture1D },
{ "glFramebufferTexture2D"                , (void **)&pglFramebufferTexture2D },
{ "glFramebufferTexture3D"                , (void **)&pglFramebufferTexture3D },
{ "glFramebufferRenderbuffer"             , (void **)&pglFramebufferRenderbuffer },
{ "glGetFramebufferAttachmentParameteriv" , (void **)&pglGetFramebufferAttachmentParameteriv },
{ "glGenerateMipmap"                      , (void **)&pglGenerateMipmap },
{ NULL, NULL}
};

static dllfunc_t drawbuffersfuncs[] =
{
{ "glDrawBuffersARB"			, (void **)&pglDrawBuffersARB },
{ NULL					, NULL }
};

static dllfunc_t wglproc_funcs[] =
{
{ "wglGetProcAddress"  , (void **)&pwglGetProcAddress },
{ NULL, NULL }
};

static dllfunc_t wgl_funcs[] =
{
{ "wglSwapBuffers"         , (void **)&pwglSwapBuffers },
{ "wglCreateContext"       , (void **)&pwglCreateContext },
{ "wglDeleteContext"       , (void **)&pwglDeleteContext },
{ "wglMakeCurrent"         , (void **)&pwglMakeCurrent },
{ "wglGetCurrentContext"   , (void **)&pwglGetCurrentContext },
{ NULL, NULL }
};

static dllhandle_t opengl_dll = NULL;

/*
========================
DebugCallback

For ARB_debug_output
========================
*/
static void CALLBACK GL_DebugOutput( GLuint source, GLuint type, GLuint id, GLuint severity, GLint length, const GLcharARB *message, GLvoid *userParam )
{
	switch( type )
	{
	case GL_DEBUG_TYPE_ERROR_ARB:
		Msg( "^1OpenGL Error:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		Msg( "^3OpenGL Warning:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		Msg( "^3OpenGL Warning:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		Msg( "^3OpenGL Warning:^7 %s\n", message );
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		Msg( "OpenGL Notify: %s\n", message );
		break;
	case GL_DEBUG_TYPE_OTHER_ARB:
	default:	
		Msg( "OpenGL: %s\n", message );
		break;
	}
}

/*
=================
GL_SetExtension
=================
*/
void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		glConfig.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else Msg( "^1Error:^7 GL_SetExtension: invalid extension %d\n", r_ext );
}

/*
=================
GL_Active
=================
*/
bool GL_Active( void )
{
	return (opengl_dll != NULL) ? true : false;
}

/*
=================
GL_Support
=================
*/
bool GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		return glConfig.extension[r_ext] ? true : false;
	Msg( "^1Error:^7 GL_Support: invalid extension %d\n", r_ext );

	return false;		
}

/*
=================
GL_GetProcAddress
=================
*/
void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p ) p = (void *)Sys_GetProcAddress( opengl_dll, name );

	return p;
}

/*
=================
GL_MaxTextureUnits
=================
*/
int GL_MaxTextureUnits( void )
{
	if( GL_Support( R_SHADER_GLSL100_EXT ))
		return min( max( glConfig.max_texture_coords, glConfig.max_teximage_units ), 32 );
	return glConfig.max_texture_units;
}

/*
=================
GL_CheckExtension
=================
*/
void GL_CheckExtension( const char *name, const dllfunc_t *funcs, int r_ext )
{
	const dllfunc_t *func;
	bool real_name = ( name[0] == 'P' || name[2] == '_' || name[3] == '_' ) ? true : false;

	if( real_name )
	{
		Msg( "GL_CheckExtension: %s ", name );
		Sleep( 100 );
	}

	if( real_name && !Q_strstr( glConfig.extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		if( name[0] == 'P' )
			Msg( "- ^2missed^7\n" );
		else Msg( "- ^1failed^7\n" );
		return;
	}

	// clear exports
	for( func = funcs; func && func->name; func++ )
		*func->func = NULL;

	GL_SetExtension( r_ext, true ); // predict extension state
	for( func = funcs; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!(*func->func = (void *)GL_GetProcAddress( func->name )))
			GL_SetExtension( r_ext, false ); // one or more functions are invalid, extension will be disabled
	}

	if( real_name )
	{
		if( GL_Support( r_ext ))
		{
			if( name[0] == 'P' )
				Msg( "- ^1present^7\n" );
			else Msg( "- ^2enabled^7\n" );
		}
		else Msg( "- ^1failed^7\n" );
	}
}

/*
===============
GL_ContextError
===============
*/
static void GL_ContextError( void )
{
	DWORD error = GetLastError();

	if( error == ( 0xc0070000|ERROR_INVALID_VERSION_ARB ))
		Msg( "^1Error:^7 Unsupported OpenGL context version (%s).\n", "2.0" );
	else if( error == ( 0xc0070000|ERROR_INVALID_PROFILE_ARB ))
		Msg( "^1Error:^7 Unsupported OpenGL profile (%s).\n", "compat" );
	else if( error == ( 0xc0070000|ERROR_INVALID_OPERATION ))
		Msg( "^1Error:^7 wglCreateContextAttribsARB returned invalid operation.\n" );
	else if( error == ( 0xc0070000|ERROR_DC_NOT_FOUND ))
		Msg( "^1Error:^7 wglCreateContextAttribsARB returned dc not found.\n" );
	else if( error == ( 0xc0070000|ERROR_INVALID_PIXEL_FORMAT ))
		Msg( "^1Error:^7 wglCreateContextAttribsARB returned dc not found.\n" );
	else if( error == ( 0xc0070000|ERROR_NO_SYSTEM_RESOURCES ))
		Msg( "^1Error:^7 wglCreateContextAttribsARB ran out of system resources.\n" );
	else if( error == ( 0xc0070000|ERROR_INVALID_PARAMETER ))
		Msg( "^1Error:^7 wglCreateContextAttribsARB reported invalid parameter.\n" );
	else Msg( "^1Error:^7 Unknown error creating an OpenGL (%s) Context.\n", "2.0" );
}

/*
=================
GL_DeleteContext
=================
*/
bool GL_DeleteContext( void )
{
	if( pwglMakeCurrent )
		pwglMakeCurrent( NULL, NULL );

	if( glw_state.hGLRC )
	{
		if( pwglDeleteContext )
			pwglDeleteContext( glw_state.hGLRC );
		glw_state.hGLRC = NULL;
	}

	if( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}

	return false;
}

/*
=================
GL_CreateContext
=================
*/
bool GL_CreateContext( void )
{
	HGLRC hBaseRC;

	if(!( glw_state.hGLRC = pwglCreateContext( glw_state.hDC )))
		return GL_DeleteContext();

	if(!( pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		return GL_DeleteContext();

	pwglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBS)GL_GetProcAddress( "wglCreateContextAttribsARB" );

	if( pwglCreateContextAttribsARB != NULL )
	{
		int attribs[] =
		{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
		WGL_CONTEXT_MINOR_VERSION_ARB, 0,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,         
//		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		0
		};

		hBaseRC = glw_state.hGLRC; // backup
		glw_state.hGLRC = NULL;

		if( !( glw_state.hGLRC = pwglCreateContextAttribsARB( glw_state.hDC, NULL, attribs )))
		{
			glw_state.hGLRC = hBaseRC;
			GL_ContextError();
			return true; // just use old context
		}

		if(!( pwglMakeCurrent( glw_state.hDC, glw_state.hGLRC )))
		{
			pwglDeleteContext( glw_state.hGLRC );
			glw_state.hGLRC = hBaseRC;
			GL_ContextError();
			return true;
		}

		Msg( "GL_CreateContext: using extended context\n" );
		pwglDeleteContext( hBaseRC );	// release first context
	}

	return true;
}

/*
=================
VID_ChoosePFD
=================
*/
static int VID_ChoosePFD( PIXELFORMATDESCRIPTOR *pfd, int colorBits, int alphaBits, int depthBits, int stencilBits )
{
	int	pixelFormat = 0;

	Msg( "VID_ChoosePFD( color %i, alpha %i, depth %i, stencil %i )\n", colorBits, alphaBits, depthBits, stencilBits );

	// Fill out the PFD
	pfd->nSize = sizeof (PIXELFORMATDESCRIPTOR);
	pfd->nVersion = 1;
	pfd->dwFlags = PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
	pfd->iPixelType = PFD_TYPE_RGBA;

	pfd->cColorBits = colorBits;
	pfd->cRedBits = 0;
	pfd->cRedShift = 0;
	pfd->cGreenBits = 0;
	pfd->cGreenShift = 0;
	pfd->cBlueBits = 0;
	pfd->cBlueShift = 0;	// wow! Blue Shift %)

	pfd->cAlphaBits = alphaBits;
	pfd->cAlphaShift = 0;

	pfd->cAccumBits = 0;
	pfd->cAccumRedBits = 0;
	pfd->cAccumGreenBits = 0;
	pfd->cAccumBlueBits = 0;
	pfd->cAccumAlphaBits= 0;

	pfd->cDepthBits = depthBits;
	pfd->cStencilBits = stencilBits;

	pfd->cAuxBuffers = 0;
	pfd->iLayerType = PFD_MAIN_PLANE;
	pfd->bReserved = 0;

	pfd->dwLayerMask = 0;
	pfd->dwVisibleMask = 0;
	pfd->dwDamageMask = 0;

	// count PFDs
	pixelFormat = ChoosePixelFormat( glw_state.hDC, pfd );

	if( !pixelFormat )
	{
		Msg( "^1Error:^7 VID_ChoosePFD failed\n" );
		return 0;
	}

	return pixelFormat;
}

/*
=================
GL_SetPixelformat
=================
*/
bool GL_SetPixelformat( void )
{
	PIXELFORMATDESCRIPTOR	PFD;
	int			pixelFormat;

	if(( glw_state.hDC = GetDC( glw_state.hWnd )) == NULL )
		return false;

	// choose a pixel format
	pixelFormat = VID_ChoosePFD( &PFD, 24, 0, 32, 0 );

	if( !pixelFormat )
	{
		Msg( "^1Error:^7 GL_SetPixelformat: failed to find an appropriate PIXELFORMAT\n" );
		return false;
	}

	// set the pixel format
	if( !SetPixelFormat( glw_state.hDC, pixelFormat, &PFD ))
	{
		Msg( "^1Error:^7 GL_SetPixelformat: failed\n" );
		return false;
	}

	DescribePixelFormat( glw_state.hDC, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), &PFD );

	if( PFD.dwFlags & PFD_GENERIC_FORMAT )
	{
		Msg( "^1Error:^7 GL_SetPixelformat: no hardware acceleration found\n" );
		glw_state.software = true;
		return false;
	}
	else
	{
		glw_state.software = false;
	}

	glConfig.color_bits = PFD.cColorBits;
	glConfig.alpha_bits = PFD.cAlphaBits;
	glConfig.depth_bits = PFD.cDepthBits;
	glConfig.stencil_bits = PFD.cStencilBits;

	// print out PFD specifics 
	Msg( "GL PFD: color( %d-bits ) alpha( %d-bits ) Z( %d-bit )\n", PFD.cColorBits, PFD.cAlphaBits, PFD.cDepthBits );

	return true;
}

/*
====================
IN_WndProc

main window procedure
====================
*/
long IN_WndProc( HWND hWnd, unsigned int uMsg, unsigned int wParam, long lParam )
{
	switch( uMsg )
	{
	case WM_CREATE:
		glw_state.hWnd = hWnd;
		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

bool VID_CreateWindow( int width, int height )
{
	WNDCLASS		wc;
	RECT		rect;
	int		x = 0, y = 0, w, h;
	int		stylebits = WINDOW_STYLE;
	int		exstyle = WINDOW_EX_STYLE;
	static char	wndname[256];
	HWND		window;
	
	strncpy( wndname, "TestWindow", sizeof( wndname ));

	glw_state.hInst = GetModuleHandle( NULL );

	// register the frame class
	wc.style         = CS_OWNDC|CS_NOCLOSE;
	wc.lpfnWndProc   = (WNDPROC)IN_WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = glw_state.hInst;
	wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)COLOR_3DSHADOW;
	wc.lpszClassName = WINDOW_NAME;
	wc.lpszMenuName  = 0;
	wc.hIcon         = LoadIcon( glw_state.hInst, MAKEINTRESOURCE( 101 ));

	if( !RegisterClass( &wc ))
	{ 
		Msg( "^1Error:^7 VID_CreateWindow: couldn't register window class %s\n" WINDOW_NAME );
		return false;
	}

	rect.left = 0;
	rect.top = 0;
	rect.right  = width;
	rect.bottom = height;

	AdjustWindowRect( &rect, stylebits, FALSE );
	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	window = CreateWindowEx( exstyle, WINDOW_NAME, wndname, stylebits, x, y, w, h, NULL, NULL, glw_state.hInst, NULL );

	if( glw_state.hWnd != window )
	{
		// probably never happens
		Msg( "^3Warning:^7 VID_CreateWindow: bad hWnd for '%s'\n", wndname );
	}

	// host.hWnd must be filled in IN_WndProc
	if( !glw_state.hWnd ) 
	{
		Msg( "^1Error:^7 VID_CreateWindow: couldn't create '%s'\n", wndname );
		return false;
	}

	ShowWindow( glw_state.hWnd, SW_HIDE );
	UpdateWindow( glw_state.hWnd );

	// init all the gl stuff for the window
	if( !GL_SetPixelformat( ))
	{
		DestroyWindow( glw_state.hWnd );
		glw_state.hWnd = NULL;

		UnregisterClass( WINDOW_NAME, glw_state.hInst );
		Msg( "^1Error:^7 OpenGL driver not installed\n" );

		return false;
	}

	if( !GL_CreateContext( ))
		return false;

	return true;
}

void VID_DestroyWindow( void )
{
	if( pwglMakeCurrent )
		pwglMakeCurrent( NULL, NULL );

	if( glw_state.hDC )
	{
		ReleaseDC( glw_state.hWnd, glw_state.hDC );
		glw_state.hDC = NULL;
	}

	if( glw_state.hWnd )
	{
		DestroyWindow( glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

	UnregisterClass( WINDOW_NAME, glw_state.hInst );
}

rserr_t R_ChangeDisplaySettings( void )
{
	HDC	hDC;

	// check our desktop attributes
	hDC = GetDC( GetDesktopWindow( ));
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	// destroy the existing window
	if( glw_state.hWnd ) VID_DestroyWindow();

	ChangeDisplaySettings( 0, 0 );

	if( !VID_CreateWindow( 1024, 768 ))
		return rserr_invalid_mode;

	return rserr_ok;
}

/*
==================
VID_SetMode

Set the described video mode
==================
*/
bool VID_SetMode( void )
{
	if( R_ChangeDisplaySettings() == rserr_ok )
	{
		return true;
	}

	return false;
}

static bool GL_InitExtensions( void )
{
	if( !Sys_LoadLibrary( "opengl32.dll", &opengl_dll, wgl_funcs ))
	{
		Msg( "^1Error:^7 OpenGL32.dll couldn't be loaded.\n" );
		return false;
	}

	// initalize until base opengl functions loaded
	GL_CheckExtension( "OpenGL ProcAddress", wglproc_funcs, R_WGL_PROCADDRESS );

	if( !VID_SetMode( ))
	{
		return false;
	}

	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, R_OPENGL_110 );

	if( !GL_Support( R_OPENGL_110 ))
	{
		Msg( "^1Error:^7 OpenGL 1.0 can't be installed.\n" );
		return false;
	}

	// get our various GL strings
	glConfig.vendor_string = pglGetString( GL_VENDOR );
	glConfig.renderer_string = pglGetString( GL_RENDERER );
	glConfig.version_string = pglGetString( GL_VERSION );
	glConfig.extensions_string = pglGetString( GL_EXTENSIONS );

	Msg( "\n" );
	Msg( "GL_VENDOR: %s\n", glConfig.vendor_string );
	Msg( "GL_RENDERER: %s\n", glConfig.renderer_string );
	Msg( "GL_VERSION: %s\n", glConfig.version_string );

	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, R_DRAW_RANGEELEMENTS_EXT );

	if( !GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		GL_CheckExtension( "GL_EXT_draw_range_elements", drawrangeelementsextfuncs, R_DRAW_RANGEELEMENTS_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElementsEXT == NULL ) pglDrawRangeElementsEXT = pglDrawRangeElements;

	if( !GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
	{
		Msg( "^1Error:^7 GL_EXT_draw_range_elements not support.\n" );
		return false;
	}

	// multitexture
	glConfig.max_texture_units = glConfig.max_texture_coords = glConfig.max_teximage_units = 1;
	GL_CheckExtension( "GL_ARB_multitexture", multitexturefuncs, R_ARB_MULTITEXTURE );

	if( GL_Support( R_ARB_MULTITEXTURE ))
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glConfig.max_texture_units );

	if( glConfig.max_texture_units == 1 )
		GL_SetExtension( R_ARB_MULTITEXTURE, false );

	if( !GL_Support( R_ARB_MULTITEXTURE ))
	{
		Msg( "^1Error:^7 GL_ARB_multitexture not support.\n" );
		return false;
	}

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, R_TEXTURE_3D_EXT );

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size );

		if( glConfig.max_3d_texture_size < 32 )
		{
			GL_SetExtension( R_TEXTURE_3D_EXT, false );
			Msg( "^3Warning:^7 GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	// 2d texture array support
	GL_CheckExtension( "GL_EXT_texture_array", texture3dextfuncs, R_TEXTURE_ARRAY_EXT );

	if( GL_Support( R_TEXTURE_ARRAY_EXT ))
		pglGetIntegerv( GL_MAX_ARRAY_TEXTURE_LAYERS_EXT, &glConfig.max_2d_texture_layers );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, R_TEXTURECUBEMAP_EXT );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_size );
	}
	else
	{
		Msg( "^3Warning:^7 GL_ARB_texture_cube_map not support. Cubemap reflections and omni shadows will be disabled.\n" );
	}

	GL_CheckExtension( "GL_ARB_draw_buffers", drawbuffersfuncs, R_DRAW_BUFFERS_EXT );

	if( !GL_Support( R_DRAW_BUFFERS_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_draw_buffers not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, R_ARB_TEXTURE_NPOT_EXT );

	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, R_ARB_VERTEX_BUFFER_OBJECT_EXT );

	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_vertex_buffer_object not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_ARB_vertex_array_object", vaofuncs, R_ARB_VERTEX_ARRAY_OBJECT_EXT );

	if( !GL_Support( R_ARB_VERTEX_ARRAY_OBJECT_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_vertex_array_object not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_EXT_gpu_shader4", NULL, R_EXT_GPU_SHADER4 );

	if( !GL_Support( R_EXT_GPU_SHADER4 ))
		Msg( "^3Warning:^7 GL_EXT_gpu_shader4 not support\n" );

	GL_CheckExtension( "GL_ARB_debug_output", debugoutputfuncs, R_DEBUG_OUTPUT );

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, R_SHADER_OBJECTS_EXT );

	if( !GL_Support( R_SHADER_OBJECTS_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_shader_objects not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, R_SHADER_GLSL100_EXT );

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_shading_language_100 not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, R_VERTEX_SHADER_EXT );

	if( !GL_Support( R_VERTEX_SHADER_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_vertext_shader not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, R_FRAGMENT_SHADER_EXT );

	if( !GL_Support( R_FRAGMENT_SHADER_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_fragment_shader not support.\n" );
		return false;
	}

	GL_CheckExtension( "GL_ARB_depth_texture", NULL, R_DEPTH_TEXTURE );
	GL_CheckExtension( "GL_ARB_shadow", NULL, R_SHADOW_EXT );

	if( !GL_Support( R_DEPTH_TEXTURE ) || !GL_Support( R_SHADOW_EXT ))
	{
		Msg( "^3Warning:^7 GL_ARB_depth_texture or GL_ARB_shadow not support. Dynamic shadows will be disabled\n" );
	}

	if( GL_Support( R_SHADER_GLSL100_EXT ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_COORDS_ARB, &glConfig.max_texture_coords );
		pglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &glConfig.max_teximage_units );
	}
	else
	{
		// just get from multitexturing
		glConfig.max_texture_coords = glConfig.max_teximage_units = glConfig.max_texture_units;
	}

	if( glConfig.max_teximage_units < 8 )
	{
		Msg( "^1Error:^7 not enough texture image units.\n" );
		return false;
	}

	glConfig.max_2d_texture_size = 0;
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	// FBO support
	GL_CheckExtension( "GL_ARB_framebuffer_object", fbofuncs, R_FRAMEBUFFER_OBJECT );

	// Paranoia OpenGL32.dll may be eliminate shadows. Run special check for it
	GL_CheckExtension( "PARANOIA_HACKS_V1", NULL, R_PARANOIA_EXT );

	if( GL_Support( R_PARANOIA_EXT ))
	{
		Msg( "^3Warning:^7 hacked Paranoia opengl32.dll was detected\n We recommend to remove it from Paranoia 2:Savior folder.\n" );
	}

	GL_CheckExtension( "GL_ARB_texture_float", NULL, R_ARB_TEXTURE_FLOAT_EXT );

	GL_CheckExtension( "GL_ARB_half_float_vertex", NULL, R_ARB_HALF_FLOAT_EXT );
//	GL_CheckExtension( "GL_ARB_half_float_pixel", NULL, R_ARB_HALF_FLOAT_EXT );

	if( !GL_Support( R_ARB_HALF_FLOAT_EXT ))
	{
		Msg( "^1Error:^7 GL_ARB_half_float_vertex not support by your hardware.\n" );
		return false;
	}

	// rectangle textures support
	if( Q_strstr( glConfig.extensions_string, "GL_NV_texture_rectangle" ))
	{
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &glConfig.max_2d_rectangle_size );
	}
	else if( Q_strstr( glConfig.extensions_string, "GL_EXT_texture_rectangle" ))
	{
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &glConfig.max_2d_rectangle_size );
	}
	else glConfig.max_2d_rectangle_size = 0; // no rectangle

	// check for hardware skinning
	pglGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &glConfig.max_vertex_uniforms );
	pglGetIntegerv( GL_MAX_VERTEX_ATTRIBS_ARB, &glConfig.max_vertex_attribs );

	if( glConfig.max_vertex_attribs < 10 )
	{
		Msg( "^1Error:^7 not enough vertex attrib count.\n" );
		return false;
	}

	if( Q_strstr( glConfig.renderer_string, "radeon" ) || Q_strstr( glConfig.renderer_string, "Radeon" ))
		glConfig.max_vertex_uniforms /= 4; // radeon returns not correct info

	// clamp to a minimal value on SM 2.0
	glConfig.max_vertex_uniforms = max( glConfig.max_vertex_uniforms, MIN_SHADER_UNIFOMS );

	glConfig.max_skinning_bones = bound( 0, ( max( glConfig.max_vertex_uniforms - MAX_RESERVED_UNIFORMS, 0 ) / 12 ), 128 );

	if( glConfig.max_skinning_bones < 64 )
	{
		Msg( "^1Error:^7 Hardware Skinning not support.\n" );
		return false;
	}
	else if( glConfig.max_skinning_bones < 128 )
		Msg( "^3Warning:^7 Hardware Skinning probably has a limitation (max %i bones)\n", glConfig.max_skinning_bones );

	Msg( "MaxSkinned bones %i\n", glConfig.max_skinning_bones );

	Msg( "GL_MAX_TEXTURE_SIZE: %i\n", glConfig.max_2d_texture_size );
	
	if( GL_Support( R_ARB_MULTITEXTURE ))
		Msg( "GL_MAX_TEXTURE_UNITS_ARB: %i\n", glConfig.max_texture_units );
	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		Msg( "GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB: %i\n", glConfig.max_cubemap_size );
	if( glConfig.max_2d_rectangle_size )
		Msg( "GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB: %i\n", glConfig.max_2d_rectangle_size );
	if( GL_Support( R_TEXTURE_ARRAY_EXT ))
		Msg( "GL_MAX_ARRAY_TEXTURE_LAYERS_EXT: %i\n", glConfig.max_2d_texture_layers );

	if( GL_Support( R_SHADER_GLSL100_EXT ))
	{
		Msg( "GL_MAX_TEXTURE_COORDS_ARB: %i\n", glConfig.max_texture_coords );
		Msg( "GL_MAX_TEXTURE_IMAGE_UNITS_ARB: %i\n", glConfig.max_teximage_units );
	}

	Msg( "GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB: %i\n", glConfig.max_vertex_uniforms );
	Msg( "GL_MAX_VERTEX_ATTRIBS_ARB: %i\n", glConfig.max_vertex_attribs );

	if( GL_Support( R_DEBUG_OUTPUT ))
	{
		pglDebugMessageCallbackARB( GL_DebugOutput, NULL );

		// force everything to happen in the main thread instead of in a separate driver thread
		pglEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB );

		// enable all the low priority messages
		pglDebugMessageControlARB( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, true );
	}

	return true;
}

int main( int argc, char **argv )
{
	Sys_InitLog( "status.log" );

	Msg("\n\n\t\t\t^3Xash XT Group^7 2018(c).\n");
	Msg("\t\t\tRequirements Test v1.03.\n\n\n");

	if( GL_InitExtensions( ))
	{
		Msg( "\n\t\t^2TEST SUCESSFULLY PASSED!\n\n\n\n\t\t\tHit any key to exit" );
	}
	else
	{
		Msg( "\n\t\t\t^1TEST FAILED!\n\n\n\n\t\t\tHit any key to exit" );
	}

	system( "pause>nul" );

	GL_DeleteContext ();
	VID_DestroyWindow ();
	Sys_FreeLibrary( &opengl_dll );
	Sys_CloseLog();

	return 0;
}