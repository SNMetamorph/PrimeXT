/*
gl_export.cpp - OpenGL dynamically linkage
Copyright (C) 2010 Uncle Mike

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
#include "hud.h"
#include "utils.h"
#include "gl_local.h"
#include <stringlib.h>
#include "gl_world.h"
#include "gl_decals.h"
#include "gl_studio.h"
#include "gl_grass.h"
#include "material.h"
#include "gl_occlusion.h"
#include "gl_cvars.h"
#include "gl_debug.h"
#include "imgui_manager.h"
#include "r_weather.h"

#define MAX_RESERVED_UNIFORMS		22	// while MAX_LIGHTSTYLES 64
#define PROJ_SIZE			64

glState_t		glState;
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
{ "glDrawArrays"       	, (void **)&pglDrawArrays },
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
{ "glGetTexImage"			, (void **)&pglGetTexImage },
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

static dllfunc_t opengl_200funcs[] =
{
{ "glCreateShader"				, (void **)&pglCreateShader },
{ "glAttachShader"				, (void **)&pglAttachShader },
{ "glDetachShader"				, (void **)&pglDetachShader },
{ "glCompileShader"				, (void **)&pglCompileShader },
{ "glShaderSource"				, (void **)&pglShaderSource },
{ "glGetShaderSource"			, (void **)&pglGetShaderSource },
{ "glGetShaderiv"				, (void **)&pglGetShaderiv },
{ "glDeleteShader"		        , (void **)&pglDeleteShader },
{ "glUseProgram"				, (void **)&pglUseProgram },
{ "glLinkProgram"				, (void **)&pglLinkProgram },
{ "glValidateProgram"			, (void **)&pglValidateProgram },
{ "glCreateProgram"				, (void **)&pglCreateProgram },
{ "glDeleteProgram"				, (void **)&pglDeleteProgram },
{ "glGetShaderInfoLog"			, (void **)&pglGetShaderInfoLog },
{ "glGetProgramInfoLog"			, (void **)&pglGetProgramInfoLog },
{ "glGetActiveUniform"			, (void **)&pglGetActiveUniform },
{ "glGetUniformLocation"		, (void **)&pglGetUniformLocation },
{ "glGetProgramiv"              , (void **)&pglGetProgramiv },
{ "glBlendEquation"             , (void **)&pglBlendEquation },
{ "glVertexAttribPointer"		, (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArray"	, (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArray"	, (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocation"		, (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttrib"			, (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocation"			, (void **)&pglGetAttribLocationARB },
{ "glTexImage3D"				, (void **)&pglTexImage3D },
{ "glTexSubImage3D"				, (void **)&pglTexSubImage3D },
{ "glCopyTexSubImage3D"			, (void **)&pglCopyTexSubImage3D },
{ "glDrawRangeElements"			, (void **)&pglDrawRangeElements },
{ "glDrawBuffers"				, (void **)&pglDrawBuffersARB },
{ "glMultiTexCoord1f"			, (void **)&pglMultiTexCoord1f },
{ "glMultiTexCoord2f"			, (void **)&pglMultiTexCoord2f },
{ "glMultiTexCoord3f"			, (void **)&pglMultiTexCoord3f },
{ "glMultiTexCoord4f"			, (void **)&pglMultiTexCoord4f },
{ "glActiveTexture"				, (void **)&pglActiveTexture },
{ "glUniformMatrix2fv"			, (void **)&pglUniformMatrix2fvARB },
{ "glUniformMatrix3fv"			, (void **)&pglUniformMatrix3fvARB },
{ "glUniformMatrix4fv"			, (void **)&pglUniformMatrix4fvARB },
{ "glUniform1f"					, (void **)&pglUniform1fARB },
{ "glUniform2f"					, (void **)&pglUniform2fARB },
{ "glUniform3f"					, (void **)&pglUniform3fARB },
{ "glUniform4f"					, (void **)&pglUniform4fARB },
{ "glUniform1i"					, (void **)&pglUniform1iARB },
{ "glUniform2i"					, (void **)&pglUniform2iARB },
{ "glUniform3i"					, (void **)&pglUniform3iARB },
{ "glUniform4i"					, (void **)&pglUniform4iARB },
{ "glUniform1fv"				, (void **)&pglUniform1fvARB },
{ "glUniform2fv"				, (void **)&pglUniform2fvARB },
{ "glUniform3fv"				, (void **)&pglUniform3fvARB },
{ "glUniform4fv"				, (void **)&pglUniform4fvARB },
{ "glUniform1iv"				, (void **)&pglUniform1ivARB },
{ "glUniform2iv"				, (void **)&pglUniform2ivARB },
{ "glUniform3iv"				, (void **)&pglUniform3ivARB },
{ "glUniform4iv"				, (void **)&pglUniform4ivARB },
{ "glVertexAttrib2f"            , (void **)&pglVertexAttrib2fARB },
{ "glVertexAttrib2fv"           , (void **)&pglVertexAttrib2fvARB },
{ "glVertexAttrib3fv"           , (void **)&pglVertexAttrib3fvARB },
{ "glVertexAttrib4fv"           , (void **)&pglVertexAttrib4fvARB },
{ "glVertexAttrib4ubv"			, (void **)&pglVertexAttrib4ubvARB },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsextfuncs[] =
{
{ "glDrawRangeElementsEXT" , (void **)&pglDrawRangeElements },
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

static dllfunc_t khr_debug_funcs[] =
{
{ "glGetObjectLabel"	, (void **)&pglGetObjectLabel },
{ "glGetObjectPtrLabel"	, (void **)&pglGetObjectPtrLabel },
{ "glObjectLabel"		, (void **)&pglObjectLabel },
{ "glObjectPtrLabel"	, (void **)&pglObjectPtrLabel },
{ "glPopDebugGroup"		, (void **)&pglPopDebugGroup },
{ "glPushDebugGroup"	, (void **)&pglPushDebugGroup },
{ NULL, NULL }
};

static dllfunc_t blendseparatefunc[] =
{
{ "glBlendFuncSeparateEXT", (void **)&pglBlendFuncSeparate },
{ NULL, NULL }
};

static dllfunc_t shaderobjectsfuncs[] =
{
{ "glGetInfoLogARB"               , (void **)&pglGetInfoLogARB },
{ "glGetAttachedObjectsARB"       , (void **)&pglGetAttachedObjectsARB },
{ "glGetUniformfvARB"             , (void **)&pglGetUniformfvARB },
{ "glGetUniformivARB"             , (void **)&pglGetUniformivARB },
{ NULL, NULL }
};

static dllfunc_t binaryshaderfuncs[] =
{
{ "glProgramBinary"              , (void **)&pglProgramBinary },
{ "glGetProgramBinary"           , (void **)&pglGetProgramBinary },
{ "glProgramParameteri"          , (void **)&pglProgramParameteri },
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
{ "glBlitFramebuffer"                     , (void **)&pglBlitFramebuffer },
{ "glDeleteFramebuffers"                  , (void **)&pglDeleteFramebuffers },
{ "glGenFramebuffers"                     , (void **)&pglGenFramebuffers },
{ "glCheckFramebufferStatus"              , (void **)&pglCheckFramebufferStatus },
{ "glFramebufferTexture1D"                , (void **)&pglFramebufferTexture1D },
{ "glFramebufferTexture2D"                , (void **)&pglFramebufferTexture2D },
{ "glFramebufferTexture3D"                , (void **)&pglFramebufferTexture3D },
{ "glFramebufferRenderbuffer"             , (void **)&pglFramebufferRenderbuffer },
{ "glGetFramebufferAttachmentParameteriv" , (void **)&pglGetFramebufferAttachmentParameteriv },
{ "glGenerateMipmap"                      , (void **)&pglGenerateMipmap },
{ "glColorMaski"                          , (void **)&pglColorMaski },
{ NULL, NULL }
};

static dllfunc_t occlusionfunc[] =
{
{ "glGenQueriesARB"        , (void **)&pglGenQueriesARB },
{ "glDeleteQueriesARB"     , (void **)&pglDeleteQueriesARB },
{ "glIsQueryARB"           , (void **)&pglIsQueryARB },
{ "glBeginQueryARB"        , (void **)&pglBeginQueryARB },
{ "glEndQueryARB"          , (void **)&pglEndQueryARB },
{ "glGetQueryivARB"        , (void **)&pglGetQueryivARB },
{ "glGetQueryObjectivARB"  , (void **)&pglGetQueryObjectivARB },
{ "glGetQueryObjectuivARB" , (void **)&pglGetQueryObjectuivARB },
{ NULL, NULL }
};

/*
========================
DebugCallback

For ARB_debug_output
========================
*/
static void CALLBACK GL_DebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLcharARB *message, GLvoid *userParam)
{
	char *msg;
	int verboseLevel; 
	static char	string[8192];

	if (GL_Support(R_KHR_DEBUG))
	{
		switch (type)
		{
			case GL_DEBUG_TYPE_MARKER:
			case GL_DEBUG_TYPE_PUSH_GROUP:
			case GL_DEBUG_TYPE_POP_GROUP:
				return; // ignore debug group messages because they needed only for graphics profiler
		}
	}

	string[0] = '\0';
	verboseLevel = static_cast<int>(gl_debug_verbose->value);

	if (verboseLevel < 1)
		return;

	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR_ARB:
			string[0] = 3; // no extra refresh
			msg = va("^1OpenGL Error:^7 %s\n", message);
			Q_strncat(string, msg, sizeof(string));
			break;

		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
			if (verboseLevel < 2)
				return;

			string[0] = 3; // no extra refresh
			msg = va("^3OpenGL Deprecated Behavior:^7 %s\n", message);
			Q_strncat(string, msg, sizeof(string));
			break;

		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
			string[0] = 3; // no extra refresh
			msg = va("^3OpenGL Undefined Behavior:^7 %s\n", message);
			Q_strncat(string, msg, sizeof(string));
			break;

		case GL_DEBUG_TYPE_PORTABILITY_ARB:
			if (verboseLevel < 2)
				return;

			string[0] = 3; // no extra refresh
			msg = va("^3OpenGL Portability:^7 %s\n", message);
			Q_strncat(string, msg, sizeof(string));
			break;

		case GL_DEBUG_TYPE_PERFORMANCE_ARB:
			if (verboseLevel < 2)
				return;

			string[0] = 3; // no extra refresh
			msg = va("OpenGL Perfomance: %s\n", message);
			Q_strncat(string, msg, sizeof(string));
			break;

		default:
		case GL_DEBUG_TYPE_OTHER_ARB:
			if (verboseLevel < 2)
				return;

			// ignore spam about detailed infos
			if (!Q_strnicmp(message, "Buffer detailed info", 20))
				return;
			if (!Q_strnicmp(message, "Framebuffer detailed info", 25))
				return;

			string[0] = 3; // no extra refresh
			msg = va("OpenGL: %s\n", message);
			Q_strncat(string, msg, sizeof(string));
			break;
	}

	if (!string[0]) {
		return;
	}
	else {
		const int lastCharacterIndex = sizeof(string) - 1;
		string[lastCharacterIndex] = '\0';
		gEngfuncs.Con_Printf(string);
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
	else ALERT( at_error, "GL_SetExtension: invalid extension %d\n", r_ext );
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
	ALERT( at_error, "GL_Support: invalid extension %d\n", r_ext );
	return false;		
}

bool GL_SupportExtension(const char *name)
{
	if (name && name[0] && !Q_strstr(glConfig.extensions_string, name))
		return true;
	else
		return false;
}

/*
=================
GL_CheckExtension
=================
*/
static void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext, bool cvar_from_engine = false )
{
	const dllfunc_t	*func;
	cvar_t		*parm;

	ALERT( at_aiconsole, "GL_CheckExtension: %s ", name );

	// ugly hack for p1 opengl32.dll
	if(( name[0] == 'P' || name[2] == '_' || name[3] == '_' ) && !Q_strstr( glConfig.extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		ALERT( at_aiconsole, "- ^1failed\n" );
		return;
	}

	if( cvarname )
	{
		// NOTE: engine will be ignore cvar value if variable already exitsts (e.g. created on exec opengl.cfg)
		// so this call just update variable description (because Host_WriteOpenGLConfig won't archive cvars without it)
		if( cvar_from_engine ) parm = CVAR_GET_POINTER( cvarname );
		else parm = CVAR_REGISTER( (char *)cvarname, "1", FCVAR_GLCONFIG );

		if( !CVAR_TO_BOOL( parm ) || ( !CVAR_TO_BOOL( gl_extensions ) && r_ext != R_OPENGL_110 ))
		{
			ALERT( at_aiconsole, "- ^3disabled\n" );
			GL_SetExtension( r_ext, false );
			return; // nothing to process at
		}
		GL_SetExtension( r_ext, true );
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

	if( GL_Support( r_ext ))
		ALERT( at_aiconsole, "- ^2enabled\n" );
	else 
		ALERT( at_aiconsole, "- ^1failed\n" );
}

static void GL_InitExtensions( void )
{
	// initialize gl extensions
	GL_CheckExtension("OpenGL 1.1.0", opengl_110funcs, NULL, R_OPENGL_110);
	GL_CheckExtension("OpenGL 2.0", opengl_200funcs, NULL, R_OPENGL_200);

	if (!GL_Support(R_OPENGL_110))
	{
		ALERT( at_error, "OpenGL 1.0 can't be installed. Custom renderer disabled\n" );
		// TODO this is used in P2 but not here 
		// g_fRenderInterfaceValid = FALSE;
		g_fRenderInitialized = FALSE;
		return;
	}

	// get our various GL strings
	glConfig.vendor_string = (const char*)pglGetString( GL_VENDOR );
	glConfig.renderer_string = (const char*)pglGetString( GL_RENDERER );
	glConfig.version_string = (const char*)pglGetString( GL_VERSION );
	glConfig.extensions_string = (const char*)pglGetString( GL_EXTENSIONS );

	if( Q_stristr( glConfig.renderer_string, "geforce" ))
		glConfig.hardware_type = GLHW_NVIDIA;
	else if( Q_stristr( glConfig.renderer_string, "quadro fx" ))
		glConfig.hardware_type = GLHW_NVIDIA;
	else if( Q_stristr(glConfig.renderer_string, "rv770" ))
		glConfig.hardware_type = GLHW_RADEON;
	else if( Q_stristr(glConfig.renderer_string, "radeon hd" ))
		glConfig.hardware_type = GLHW_RADEON;
	else if( Q_stristr( glConfig.renderer_string, "eah4850" ) || Q_stristr( glConfig.renderer_string, "eah4870" ))
		glConfig.hardware_type = GLHW_RADEON;
	else if( Q_stristr( glConfig.renderer_string, "radeon" ))
		glConfig.hardware_type = GLHW_RADEON;
	else glConfig.hardware_type = GLHW_GENERIC;

	glConfig.version = Q_atof( glConfig.version_string );

	Msg( "GL_VERSION: %g\n", glConfig.version );

	GL_CheckExtension("GL_EXT_draw_range_elements", (!pglDrawRangeElements) ? drawrangeelementsextfuncs : nullptr, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT);
	if (!GL_Support(R_DRAW_RANGEELEMENTS_EXT))
	{
		ALERT(at_error, "GL_EXT_draw_range_elements not support. Custom renderer disabled\n");
		g_fRenderInitialized = FALSE;
		return;
	}

	// multitexture
	glConfig.max_texture_units = 1;
	GL_CheckExtension( "GL_ARB_multitexture", NULL, "gl_arb_multitexture", R_ARB_MULTITEXTURE, true );

	if( GL_Support( R_ARB_MULTITEXTURE ))
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glConfig.max_texture_units );

	if( glConfig.max_texture_units == 1 )
		GL_SetExtension( R_ARB_MULTITEXTURE, false );

	if( !GL_Support( R_ARB_MULTITEXTURE ))
	{
		ALERT( at_error, "GL_ARB_multitexture not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", NULL, "gl_texture_3d", R_TEXTURE_3D_EXT, true );

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size );

		if( glConfig.max_3d_texture_size < 32 )
		{
			GL_SetExtension( R_TEXTURE_3D_EXT, false );
			ALERT( at_error, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	// 2d texture array support
	GL_CheckExtension( "GL_EXT_texture_array", NULL, "gl_texture_2d_array", R_TEXTURE_ARRAY_EXT, true );

	if( !GL_Support( R_TEXTURE_ARRAY_EXT ))
		ALERT( at_warning, "GL_EXT_texture_array not support. Landscapes will be unavailable\n" );

	// occlusion queries
	GL_CheckExtension( "GL_ARB_occlusion_query", occlusionfunc, "gl_occlusion_queries", R_OCCLUSION_QUERIES_EXT );

	// separate blend
	GL_CheckExtension( "GL_EXT_blend_func_separate", blendseparatefunc, "gl_separate_blend", R_SEPARATE_BLENDFUNC_EXT );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", R_TEXTURECUBEMAP_EXT, true );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_size );
	else ALERT( at_warning, "GL_ARB_texture_cube_map not support. Cubemap reflections and omni shadows will be disabled\n" );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", R_ARB_TEXTURE_NPOT_EXT, true );

	GL_CheckExtension( "GL_ARB_draw_buffers", NULL, "gl_draw_buffers", R_DRAW_BUFFERS_EXT );

	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, "gl_vertex_buffer_object", R_ARB_VERTEX_BUFFER_OBJECT_EXT );

	if( !GL_Support( R_ARB_VERTEX_BUFFER_OBJECT_EXT ))
	{
		ALERT( at_error, "GL_ARB_vertex_buffer_object not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	GL_CheckExtension( "GL_ARB_vertex_array_object", vaofuncs, "gl_vertex_array_object", R_ARB_VERTEX_ARRAY_OBJECT_EXT );

	if( !GL_Support( R_ARB_VERTEX_ARRAY_OBJECT_EXT ))
	{
		ALERT( at_error, "GL_ARB_vertex_array_object not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	GL_CheckExtension( "GL_EXT_gpu_shader4", NULL, "gl_ext_gpu_shader4", R_EXT_GPU_SHADER4 );

	if( !GL_Support( R_EXT_GPU_SHADER4 ))
		ALERT( at_warning, "GL_EXT_gpu_shader4 not support. Shadows from omni lights will be disabled\n" );

	GL_CheckExtension("GL_ARB_debug_output", debugoutputfuncs, "gl_debug_output", R_DEBUG_OUTPUT);
	GL_CheckExtension("GL_KHR_debug", khr_debug_funcs, "gl_khr_debug", R_KHR_DEBUG);

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, "gl_shaderobjects", R_SHADER_OBJECTS_EXT );

	if( !GL_Support( R_SHADER_OBJECTS_EXT ))
	{
		ALERT( at_error, "GL_ARB_shader_objects not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, "gl_shading_language", R_SHADER_GLSL100_EXT );

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
	{
		ALERT( at_error, "GL_ARB_shading_language_100 not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	GL_CheckExtension( "GL_ARB_vertex_shader", NULL, "gl_vertexshader", R_VERTEX_SHADER_EXT );

	if( !GL_Support( R_VERTEX_SHADER_EXT ))
	{
		ALERT( at_error, "GL_ARB_vertex_shader not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, "gl_pixelshader", R_FRAGMENT_SHADER_EXT );

	if( !GL_Support( R_FRAGMENT_SHADER_EXT ))
	{
		ALERT( at_error, "GL_ARB_fragment_shader not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	GL_CheckExtension("GL_ARB_get_program_binary", binaryshaderfuncs, "gl_binaryshader", R_BINARY_SHADER_EXT);
	GL_CheckExtension("GL_ARB_depth_texture", NULL, "gl_depthtexture", R_DEPTH_TEXTURE);
	GL_CheckExtension("GL_ARB_shadow", NULL, "gl_arb_shadow", R_SHADOW_EXT);
	GL_CheckExtension("GL_ARB_texture_rectangle", NULL, "gl_texture_rectangle", R_TEXTURE_2D_RECT_EXT, true);
	GL_CheckExtension("GL_ARB_pixel_buffer_object", NULL, NULL, R_ARB_PIXEL_BUFFER_OBJECT);
	
	if( GL_Support( R_BINARY_SHADER_EXT ))
	{
		pglGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &glConfig.num_formats );
		pglGetIntegerv( GL_PROGRAM_BINARY_FORMATS, &glConfig.binary_formats );
	}

	if( !GL_Support( R_DEPTH_TEXTURE ) || !GL_Support( R_SHADOW_EXT ))
	{
		ALERT( at_warning, "GL_ARB_depth_texture or GL_ARB_shadow not support. Dynamic shadows disabled\n" );
		tr.shadows_notsupport = true;
	}

	glConfig.max_2d_texture_size = 0;
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	if( GL_Support( R_TEXTURE_ARRAY_EXT ))
		pglGetIntegerv( GL_MAX_ARRAY_TEXTURE_LAYERS_EXT, &glConfig.max_2d_texture_layers );

	// FBO support
	GL_CheckExtension( "GL_ARB_framebuffer_object", fbofuncs, "gl_framebuffers", R_FRAMEBUFFER_OBJECT );

	// Paranoia OpenGL32.dll may be eliminate shadows. Run special check for it
	GL_CheckExtension( "PARANOIA_HACKS_V1", NULL, NULL, R_PARANOIA_EXT );

	GL_CheckExtension( "GL_ARB_seamless_cube_map", NULL, "gl_seamless_cubemap", R_SEAMLESS_CUBEMAP );

	// check for hardware skinning
	pglGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &glConfig.max_vertex_uniforms );
	pglGetIntegerv( GL_MAX_VERTEX_ATTRIBS_ARB, &glConfig.max_vertex_attribs );
	pglGetIntegerv( GL_MAX_VARYING_FLOATS_ARB, &glConfig.max_varying_floats );

	// is this actual hack? we can't say without testing on wide range of AMD hardware
	// if this problem will occur, this hack should be returned and made togglable with cvar
	//if( glConfig.hardware_type == GLHW_RADEON && glConfig.max_vertex_uniforms > 512 )
	//	glConfig.max_vertex_uniforms /= 4; // only radion returns count of floats other returns count of vec4

	glConfig.max_skinning_bones = bound( 0, ( Q_max( glConfig.max_vertex_uniforms - MAX_RESERVED_UNIFORMS, 0 ) / 7 ), MAXSTUDIOBONES );

	if( glConfig.max_skinning_bones < 32 )
	{
		ALERT( at_error, "Hardware Skinning not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}
	else if( glConfig.max_skinning_bones < MAXSTUDIOBONES )
		ALERT( at_warning, "Hardware Skinning has a limitation (max %i bones)\n", glConfig.max_skinning_bones );

	ALERT( at_aiconsole, "GL_InitExtensions: max vertex uniforms %i\n", glConfig.max_vertex_uniforms );
	ALERT( at_aiconsole, "GL_InitExtensions: max varying floats %i\n", glConfig.max_varying_floats );
	ALERT( at_aiconsole, "GL_InitExtensions: max skinned bones %i\n", glConfig.max_skinning_bones );

	glConfig.max_texture_units = RENDER_GET_PARM( PARM_MAX_IMAGE_UNITS, 0 );

	if (GL_Support(R_KHR_DEBUG)) {
		pglEnable(GL_DEBUG_OUTPUT);
	}

	if (GL_Support(R_DEBUG_OUTPUT) || GL_Support(R_KHR_DEBUG))
	{
		// force everything to happen in the main thread instead of in a separate driver thread
		pglEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		pglDebugMessageCallbackARB(GL_DebugOutput, NULL);
		pglDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, true); // enable all the low priority messages
	}
}

/*
===============
GL_SetDefaultState
===============
*/
void GL_SetDefaultState( void )
{
	glState.depthmin = glState.depthmax = -1.0f;
	glState.depthmask = -1;

	GL_CleanupAllTextureUnits();
	pglBindVertexArray( GL_FALSE ); // should be first!
	pglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
	pglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	pglDisable( GL_SCISSOR_TEST );
	pglShadeModel( GL_SMOOTH );
	GL_BindFBO( FBO_MAIN );
	GL_BindShader( NULL );
}

void R_CreateSpotLightTexture( void )
{
	if( tr.defaultProjTexture ) return;

	byte data[PROJ_SIZE*PROJ_SIZE*4];
	byte *p = data;

	for( int i = 0; i < PROJ_SIZE; i++ )
	{
		float dy = (PROJ_SIZE * 0.5f - i + 0.5f) / (PROJ_SIZE * 0.5f);

		for( int j = 0; j < PROJ_SIZE; j++ )
		{
			float dx = (PROJ_SIZE * 0.5f - j + 0.5f) / (PROJ_SIZE * 0.5f);
			float r = cos( M_PI / 2.0f * sqrt(dx * dx + dy * dy));
			float c;

			r = (r < 0) ? 0 : r * r;
			c = 0xFF * r;
			p[0] = (c <= 0xFF) ? c : 0xFF;
			p[1] = (c <= 0xFF) ? c : 0xFF;
			p[2] = (c <= 0xFF) ? c : 0xFF;
			p[3] = (c <= 0xff) ? c : 0xFF;
			p += 4;
		}
	}

	tr.defaultProjTexture = CREATE_TEXTURE( "*spotlight", PROJ_SIZE, PROJ_SIZE, data, TF_SPOTLIGHT ); 
}

/*
==================
R_InitBlankBumpTexture
==================
*/
static void R_InitBlankBumpTexture( void )
{
	byte	data2D[256*4];

	// default normalmap texture
	for( int i = 0; i < 256; i++ )
	{
		data2D[i*4+0] = 127;
		data2D[i*4+1] = 127;
		data2D[i*4+2] = 255;
	}

	tr.normalmapTexture = CREATE_TEXTURE( "*blankbump", 16, 16, data2D, TF_NORMALMAP ); 
}

/*
==================
R_InitVSDCTCubemap

Virtual Shadow Depth Cube Texture
==================
*/
static void R_InitVSDCTCubemap( void )
{
	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		return;

	// maps to a 2x3 texture rectangle with normalized coordinates
	// +-
	// XX
	// YY
	// ZZ
	// stores abs(dir.xy), offset.xy/2.5
	static byte data[4*6] =
	{
		0xFF, 0x00, 0x33, 0x33, // +X: <1, 0>, <0.5, 0.5>
		0xFF, 0x00, 0x99, 0x33, // -X: <1, 0>, <1.5, 0.5>
		0x00, 0xFF, 0x33, 0x99, // +Y: <0, 1>, <0.5, 1.5>
		0x00, 0xFF, 0x99, 0x99, // -Y: <0, 1>, <1.5, 1.5>
		0x00, 0x00, 0x33, 0xFF, // +Z: <0, 0>, <0.5, 2.5>
		0x00, 0x00, 0x99, 0xFF, // -Z: <0, 0>, <1.5, 2.5>
	};

	tr.vsdctCubeTexture = CREATE_TEXTURE( "*vsdct", 1, 1, data, TF_NEAREST|TF_HAS_ALPHA|TF_CUBEMAP|TF_CLAMP ); 
}

/*
==================
R_InitWhiteCubemap
==================
*/
static void R_InitWhiteCubemap()
{
	int	size = 4;
	byte dataCM[4 * 4 * 6 *4];
	
	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		return;

	// white cubemap used as stub for pointlights
	memset(dataCM, 0xFF, sizeof(dataCM));
	tr.whiteCubeTexture = CREATE_TEXTURE("*whiteCube", size, size, dataCM, TF_NOMIPMAP | TF_CUBEMAP | TF_CLAMP); 
}


/*
==================
R_InitBlackCubemap
==================
*/
static void R_InitBlackCubemap()
{
	int	size = 4;
	byte dataCM[4 * 4 * 6 * 4];
	
	if (!GL_Support(R_TEXTURECUBEMAP_EXT))
		return;

	// black cubemap used as stub for specular IBL on maps without cubemaps
	memset(dataCM, 0x00, sizeof(dataCM));
	tr.blackCubeTexture = CREATE_TEXTURE("*blackCube", size, size, dataCM, TF_NOMIPMAP | TF_CUBEMAP | TF_CLAMP);
}

/*
=============
R_LoadIdentity
=============
*/
static void R_LoadIdentity( void )
{
	glState.identityMatrix[ 0] = 1.0f;
	glState.identityMatrix[ 1] = 0.0f;
	glState.identityMatrix[ 2] = 0.0f;
	glState.identityMatrix[ 3] = 0.0f;
	glState.identityMatrix[ 4] = 0.0f;
	glState.identityMatrix[ 5] = 1.0f;
	glState.identityMatrix[ 6] = 0.0f;
	glState.identityMatrix[ 7] = 0.0f;
	glState.identityMatrix[ 8] = 0.0f;
	glState.identityMatrix[ 9] = 0.0f;
	glState.identityMatrix[10] = 1.0f;
	glState.identityMatrix[11] = 0.0f;
	glState.identityMatrix[12] = 0.0f;
	glState.identityMatrix[13] = 0.0f;
	glState.identityMatrix[14] = 0.0f;
	glState.identityMatrix[15] = 1.0f;
}

void R_InitDefaultLights( void )
{
	// this used to precache uber-shaders
	memset( &tr.defaultlightSpot, 0, sizeof( tr.defaultlightSpot ));
	memset( &tr.defaultlightOmni, 0, sizeof( tr.defaultlightOmni ));
	memset( &tr.defaultlightProj, 0, sizeof( tr.defaultlightProj ));

	tr.defaultlightSpot.type = LIGHT_SPOT;
	tr.defaultlightOmni.type = LIGHT_OMNI;
	tr.defaultlightProj.type = LIGHT_DIRECTIONAL;
}

static void GL_InitTextures( void )
{
	// just get it from engine
	tr.defaultTexture = FIND_TEXTURE( "*default" );   	// use for bad textures
	tr.deluxemapTexture = FIND_TEXTURE( "*gray" );		// like Vector( 0, 0, 0 )
	tr.whiteTexture = FIND_TEXTURE( "*white" );
	tr.grayTexture = FIND_TEXTURE( "*gray" );
	tr.blackTexture = FIND_TEXTURE( "*black" );
	tr.depthTexture = CREATE_TEXTURE( "*depth", 8, 8, NULL, TF_SHADOW ); 

	R_InitWhiteCubemap();
	R_InitBlackCubemap();
	R_InitBlankBumpTexture();
	R_InitVSDCTCubemap();

	if( GL_Support( R_EXT_GPU_SHADER4 ))
		tr.depthCubemap = CREATE_TEXTURE( "depthCube", 8, 8, NULL, TF_SHADOW_CUBEMAP ); 

	// BRDF look-up table
	tr.brdfApproxTexture = LOAD_TEXTURE("gfx/brdf_approx.dds", NULL, 0, TF_KEEP_SOURCE | TF_CLAMP);

	// best fit normals
	tr.normalsFitting = LOAD_TEXTURE( "gfx/normalsfitting.dds", NULL, 0, TF_KEEP_SOURCE|TF_CLAMP|TF_NEAREST );
	if( !tr.normalsFitting ) tr.normalsFitting = tr.whiteTexture; // fallback

	// load water animation
	for( int i = 0; i < WATER_TEXTURES; i++ )
	{
		char path[256];
		Q_snprintf( path, sizeof( path ), "gfx/water/water_normal_%i", i );
		tr.waterTextures[i] = LOAD_TEXTURE( path, NULL, 0, TF_NORMALMAP );
	}

	R_CreateSpotLightTexture ();

	// initialize spotlights
	if( !tr.spotlightTexture[0] )
	{
		char path[256];

		tr.spotlightTexture[0] = tr.defaultProjTexture;	// always present
		tr.flashlightTexture = LOAD_TEXTURE( "gfx/flashlight", NULL, 0, TF_SPOTLIGHT );
		if( !tr.flashlightTexture ) tr.flashlightTexture = tr.defaultProjTexture;

		// 7 custom textures allowed
		for( int i = 1; i < 8; i++ )
		{
			Q_snprintf( path, sizeof( path ), "gfx/spotlight%i", i );

			if( IMAGE_EXISTS( path ))
				tr.spotlightTexture[i] = LOAD_TEXTURE( path, NULL, 0, TF_SPOTLIGHT );

			if( !tr.spotlightTexture[i] )
				tr.spotlightTexture[i] = tr.defaultProjTexture; // make default if missed
		}
	}
}

/*
==================
GL_Init
==================
*/
bool GL_Init( void )
{
	R_InitRefState();
	GL_InitExtensions();
	BuildGammaTable();
	R_LoadIdentity();

	if( !g_fRenderInitialized )
	{
		CVAR_SET_FLOAT( "gl_renderer", 0 );
		GL_Shutdown();
		return false;
	}

	DBG_PrintVertexVBOSizes();
	GL_InitGPUShaders();
	InitPostEffects();
	GL_InitTextures();
	COM_InitMatdef();
	CL_InitMaterials();
	GL_SetDefaultState();
	GL_InitRandomTable();
	g_ImGuiManager.Initialize();

	pglPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	pglPixelStorei( GL_PACK_ALIGNMENT, 1 );
	pglPointSize( 10.0f );
	pglLineWidth( 1.0f );

	R_InitCubemaps();
	R_InitWeather();
	DecalsInit();
	R_GrassInit();

	return true;
}

/*
===============
GL_Shutdown
===============
*/
void GL_Shutdown( void )
{
	int	i;

	g_StudioRenderer.DestroyAllModelInstances();
	g_StudioRenderer.FreeStudioCacheVL();
	g_StudioRenderer.FreeStudioCacheFL();

	if( tr.materials )
		Mem_Free( tr.materials );

	for( i = 0; i < tr.num_framebuffers; i++ )
	{
		if( !tr.frame_buffers[i].init )
			break;
		R_FreeFrameBuffer( i );
	}

	tr.fbo_shadow2D.Free();
	tr.fbo_shadowCM.Free();

	for( i = 0; i < MAX_SHADOWMAPS; i++ )
	{
		tr.sunShadowFBO[i].Free();
	}

	R_FreeCinematics();
	DecalsShutdown();
	R_GrassShutdown();
	GL_FreeGPUShaders();
	GL_FreeDrawbuffers();
	g_ImGuiManager.Terminate();

	// now all extensions are disabled
	memset( glConfig.extension, 0, sizeof( glConfig.extension[0] ) * R_EXTCOUNT );
}
