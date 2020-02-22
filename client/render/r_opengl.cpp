/*
r_opengl.cpp - OpenGL dynamically linkage
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

#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_particle.h"
#include "r_weather.h"
#include "mathlib.h"

#define MAX_RESERVED_UNIFORMS		22	// dynamic lighting estimates 42 uniforms... 

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

static dllfunc_t blendseparatefunc[] =
{
{ "glBlendFuncSeparateEXT", (void **)&pglBlendFuncSeparate },
{ NULL, NULL }
};

static dllfunc_t blendequationfuncs[] =
{
{ "glBlendEquationEXT" , (void **)&pglBlendEquationEXT },
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
{ "glVertexAttrib4f"              , (void **)&pglVertexAttrib4fARB },
{ "glVertexAttrib4fv"             , (void **)&pglVertexAttrib4fvARB },
{ "glVertexAttrib4ubv"            , (void **)&pglVertexAttrib4ubvARB },
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

#ifndef _WIN32
#define CALLBACK
#endif

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
		gEngfuncs.Con_Printf( va( "^1OpenGL Error:^7 %s\n", message ));
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		gEngfuncs.Con_Printf( va( "^3OpenGL Warning:^7 %s\n", message ));
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		gEngfuncs.Con_Printf( va( "^3OpenGL Warning:^7 %s\n", message ));
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		if( developer_level < DEV_EXTENDED )
			return;
		gEngfuncs.Con_Printf( va( "^3OpenGL Warning:^7 %s\n", message ));
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		gEngfuncs.Con_Printf( va( "OpenGL Perfomance: %s\n", message ));
		break;
	case GL_DEBUG_TYPE_OTHER_ARB:
	default:
		if( developer_level < DEV_EXTENDED )
			return;
		gEngfuncs.Con_Printf( va( "OpenGL: %s\n", message ));
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

/*
=================
GL_CheckExtension
=================
*/
void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext, bool cvar_from_engine = false )
{
	const dllfunc_t	*func;
	cvar_t		*parm;

	ALERT( at_aiconsole, "GL_CheckExtension: %s ", name );

	if(( name[2] == '_' || name[3] == '_' ) && !Q_strstr( glConfig.extensions_string, name ))
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

		if( !CVAR_TO_BOOL( parm ) || ( !CVAR_TO_BOOL( r_extensions ) && r_ext != R_OPENGL_110 ))
		{
			ALERT( at_aiconsole, "- disabled\n" );
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
}

static void GL_InitExtensions( void )
{
	char *maxbonesstr;

	if( g_iXashEngineBuildNumber < 4140 )
	{
		ALERT( at_error, "too old version of Xash3D engine. XashXT required at least build 4140 or higher\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, NULL, R_OPENGL_110 );

	if( !GL_Support( R_OPENGL_110 ))
	{
		ALERT( at_error, "OpenGL 1.0 can't be installed. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	// get our various GL strings
	glConfig.vendor_string = pglGetString( GL_VENDOR );
	glConfig.renderer_string = pglGetString( GL_RENDERER );
	glConfig.version_string = pglGetString( GL_VERSION );
	glConfig.extensions_string = pglGetString( GL_EXTENSIONS );

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

	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT );

	if( !GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		GL_CheckExtension( "GL_EXT_draw_range_elements", drawrangeelementsextfuncs, "gl_drawrangeelments", R_DRAW_RANGEELEMENTS_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElementsEXT == NULL ) pglDrawRangeElementsEXT = pglDrawRangeElements;

	if( !GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
	{
		ALERT( at_error, "GL_EXT_draw_range_elements not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	// multitexture
	glConfig.max_texture_units = 1;
	GL_CheckExtension( "GL_ARB_multitexture", multitexturefuncs, "gl_arb_multitexture", R_ARB_MULTITEXTURE, true );

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
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, "gl_texture_3d", R_TEXTURE_3D_EXT, true );

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
	GL_CheckExtension( "GL_EXT_texture_array", texture3dextfuncs, "gl_texture_2d_array", R_TEXTURE_ARRAY_EXT, true );

	if( !GL_Support( R_TEXTURE_ARRAY_EXT ))
		ALERT( at_warning, "GL_EXT_texture_array not support. Landscapes will be unavailable\n" );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, "gl_texture_cubemap", R_TEXTURECUBEMAP_EXT, true );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_size );
	else ALERT( at_warning, "GL_ARB_texture_cube_map not support. Cubemap reflections and omni shadows will be disabled\n" );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, "gl_texture_npot", R_ARB_TEXTURE_NPOT_EXT, true );

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
	GL_CheckExtension( "GL_ARB_debug_output", debugoutputfuncs, NULL, R_DEBUG_OUTPUT );

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

	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, "gl_vertexshader", R_VERTEX_SHADER_EXT );

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

	GL_CheckExtension( "GL_ARB_depth_texture", NULL, "gl_depthtexture", R_DEPTH_TEXTURE );
	GL_CheckExtension( "GL_ARB_shadow", NULL, "gl_arb_shadow", R_SHADOW_EXT );

	if( !GL_Support( R_DEPTH_TEXTURE ) )
	{
		ALERT( at_warning, "GL_ARB_depth_texture not supported. Dynamic shadows disabled\n" );
		tr.shadows_notsupport = true;
	}

	glConfig.max_2d_texture_size = 0;
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	// FBO support
	GL_CheckExtension( "GL_ARB_framebuffer_object", fbofuncs, "gl_framebuffers", R_FRAMEBUFFER_OBJECT );

	// Paranoia OpenGL32.dll may be eliminate shadows. Run special check for it
	GL_CheckExtension( "PARANOIA_HACKS_V1", NULL, NULL, R_PARANOIA_EXT );

	// check for hardware skinning
	pglGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &glConfig.max_vertex_uniforms );
	pglGetIntegerv( GL_MAX_VERTEX_ATTRIBS_ARB, &glConfig.max_vertex_attribs );
	pglGetIntegerv( GL_MAX_VARYING_FLOATS_ARB, &glConfig.max_varying_floats );

	if( glConfig.hardware_type == GLHW_RADEON && glConfig.max_vertex_uniforms > 512 )
		glConfig.max_vertex_uniforms /= 4; // only radion returns count of floats other returns count of vec4

	if( !glConfig.max_vertex_uniforms )
	{
		pglGetIntegerv( GL_MAX_VERTEX_UNIFORM_VECTORS, &glConfig.max_vertex_uniforms );
		glConfig.max_vertex_uniforms *= 4;
	}

	if( !glConfig.max_varying_floats )
	{
		pglGetIntegerv( GL_MAX_VARYING_VECTORS, &glConfig.max_varying_floats );
		glConfig.max_varying_floats *= 4;
	}

	glConfig.max_skinning_bones = bound( 0, ( Q_max( glConfig.max_vertex_uniforms - MAX_RESERVED_UNIFORMS, 0 ) / 7 ), MAXSTUDIOBONES );

	if( gEngfuncs.CheckParm("-r_overridemaxskinningbones", &maxbonesstr ) )
		glConfig.max_skinning_bones = Q_atoi( maxbonesstr );
	else if( glConfig.max_skinning_bones < 32 )
	{
		ALERT( at_error, "Hardware Skinning not support. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}
	else if( glConfig.max_skinning_bones < MAXSTUDIOBONES )
	{
		ALERT( at_warning, "Hardware Skinning has a limitation (max %i bones)\n", glConfig.max_skinning_bones );

		// g-cont. this produces too many shaders...
//		glConfig.uniforms_economy = true;
	}

	glConfig.uniforms_economy = gEngfuncs.CheckParm("-r_uniforms_economy", NULL );

	ALERT( at_aiconsole, "GL_InitExtensions: max vertex uniforms %i\n", glConfig.max_vertex_uniforms );
	ALERT( at_aiconsole, "GL_InitExtensions: max varying floats %i\n", glConfig.max_varying_floats );
	ALERT( at_aiconsole, "GL_InitExtensions: MaxSkinned bones %i\n", glConfig.max_skinning_bones );

	glConfig.max_texture_units = RENDER_GET_PARM( PARM_MAX_IMAGE_UNITS, 0 );

	if( GL_Support( R_DEBUG_OUTPUT ))
	{
		pglDebugMessageCallbackARB( GL_DebugOutput, NULL );

		// force everything to happen in the main thread instead of in a separate driver thread
		pglEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB );

		// enable all the low priority messages
		pglDebugMessageControlARB( GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, true );
	}
}

/*
===============
GL_SetDefaultState
===============
*/
static void GL_SetDefaultState( void )
{
	if( RENDER_GET_PARM( PARM_STENCIL_ACTIVE, 0 ))
		glState.stencilEnabled = true;
	else glState.stencilEnabled = false;

	glState.frameBuffer = FBO_MAIN;
	glState.depthmask = -1;
}

void R_CreateSpotLightTexture( void )
{
	int width = 64, height = 64;
	byte *data = new byte[width*height*4];
	byte *p = data;

	for( int i = 0; i < height; i++ )
	{
		float dy = (height * 0.5f - i + 0.5f) / (height * 0.5f);

		for( int j = 0; j < width; j++ )
		{
			float dx = (width * 0.5f - j + 0.5f) / (width * 0.5f);
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

	tr.spotlightTexture = CREATE_TEXTURE( "*spotlight", width, height, data, TF_SPOTLIGHT ); 
	delete [] data;
}

static void R_InitAlphaContrast( void )
{
	byte	data[64*64*4];
	int	size = 64;

	memset( data, size, size * size * 4 );

	tr.alphaContrastTexture = CREATE_TEXTURE( "*acont", size, size, data, TF_ALPHACONTRAST );
}

/*
==================
R_InitWhiteCubemap
==================
*/
static void R_InitWhiteCubemap( void )
{
	byte	dataCM[4*4*6*4];
	int	size = 4;

	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		return;

	// white cubemap - just stub for pointlights
	memset( dataCM, 0xFF, sizeof( dataCM ));

	tr.dlightCubeTexture = CREATE_TEXTURE( "*whiteCube", size, size, dataCM, TF_NOMIPMAP|TF_CUBEMAP|TF_CLAMP ); 
}

static void GL_InitTextures( void )
{
	// NOTE: these textures already created by engine
	// get their texturenums for local using
	tr.defaultTexture = FIND_TEXTURE( "*default" );   	// use for bad textures
	tr.whiteTexture = FIND_TEXTURE( "*white" );
	tr.grayTexture = FIND_TEXTURE( "*gray" );
	tr.depthTexture = CREATE_TEXTURE( "*depth", 8, 8, NULL, TF_SHADOW ); 

	R_InitWhiteCubemap();

	R_CreateSpotLightTexture();

	R_InitAlphaContrast();
}

/*
==================
R_Init
==================
*/
bool R_Init( void )
{
	R_InitRefState();
	GL_InitExtensions();

	if( !g_fRenderInitialized )
	{
		CVAR_SET_FLOAT( "gl_renderer", 0 );
		R_Shutdown();
		return false;
	}

	GL_InitGPUShaders();
	GL_InitTextures();
	GL_InitRandomTable();
	GL_SetDefaultState();

	R_InitWeather();
	R_GrassInit();
	InitPostEffects();

	pglPointSize( 10.0f );

	return true;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( void )
{
	g_StudioRenderer.DestroyAllModelInstances();
	g_StudioRenderer.FreeMeshCacheVL();

	R_FreeCinematics();
	R_ResetWeather();
	R_GrassShutdown();
	GL_FreeGPUShaders();

	// now all extensions are disabled
	memset( glConfig.extension, 0, sizeof( glConfig.extension[0] ) * R_EXTCOUNT );
}

/*
=================
GL_CheckForErrors

obsolete
=================
*/
void GL_CheckForErrors_( const char *filename, const int fileline )
{
	int	err;
	char	*str;

	if( !CVAR_TO_BOOL( gl_check_errors ))
		return;

	if(( err = pglGetError( )) == GL_NO_ERROR )
		return;

	switch( err )
	{
	case GL_STACK_OVERFLOW:
		str = "GL_STACK_OVERFLOW";
		break;
	case GL_STACK_UNDERFLOW:
		str = "GL_STACK_UNDERFLOW";
		break;
	case GL_INVALID_ENUM:
		str = "GL_INVALID_ENUM";
		break;
	case GL_INVALID_VALUE:
		str = "GL_INVALID_VALUE";
		break;
	case GL_INVALID_OPERATION:
		str = "GL_INVALID_OPERATION";
		break;
	case GL_OUT_OF_MEMORY:
		str = "GL_OUT_OF_MEMORY";
		break;
	default:
		str = "UNKNOWN ERROR";
		break;
	}

	gEngfuncs.Con_Printf( "^3OpenGL Error:^7 %s (called at %s:%i)\n", str, filename, fileline );
}
