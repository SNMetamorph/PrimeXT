/*
gl_imgui_backend.cpp - implementation of OpenGL backend for ImGui
Copyright (C) 2023 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "imgui.h"
#include "gl_imgui_backend.h"
#include "gl_local.h"
#include "gl_export.h"
#include "build_info.h"
#include <stdio.h>
#include <stdint.h> // intptr_t

// OpenGL Data
struct ImGui_ImplOpenGL3_Data
{
    GLuint          GlVersion;               // Extracted at runtime using GL_MAJOR_VERSION, GL_MINOR_VERSION queries (e.g. 320 for GL 3.2)
    char            GlslVersionString[32];   // Specified by user or detected based on compile time GL settings.
    GLuint          FontTexture;
    GLuint          ShaderHandle;
    GLint           AttribLocationTex;       // Uniforms location
    GLint           AttribLocationProjMtx;
    GLuint          AttribLocationVtxPos;    // Vertex attributes location
    GLuint          AttribLocationVtxUV;
    GLuint          AttribLocationVtxColor;
    unsigned int    VboHandle, ElementsHandle;
    GLsizeiptrARB   VertexBufferSize;
    GLsizeiptrARB   IndexBufferSize;
    bool            HasClipOrigin;

    ImGui_ImplOpenGL3_Data() { memset((void *)this, 0, sizeof(*this)); }
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplOpenGL3_Data *ImGui_ImplOpenGL3_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplOpenGL3_Data *)ImGui::GetIO().BackendRendererUserData : NULL;
}

// Functions
CImGuiBackend::CImGuiBackend()
{
}

CImGuiBackend::~CImGuiBackend()
{
}

bool CImGuiBackend::Init(const char *glsl_version)
{
    ImGuiIO &io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplOpenGL3_Data *bd = IM_NEW(ImGui_ImplOpenGL3_Data)();
    io.BackendRendererUserData = (void *)bd;
    io.BackendRendererName = "imgui_impl_primext_opengl3";
    io.BackendPlatformName = BuildInfo::GetPlatform();

    // Query for GL version (e.g. 320 for GL 3.2)
    GLint major = 0;
    GLint minor = 0;
    if (major == 0 && minor == 0)
    {
        // Query GL_VERSION in desktop GL 2.x, the string will start with "<major>.<minor>"
        const char *gl_version = (const char *)pglGetString(GL_VERSION);
        sscanf(gl_version, "%d.%d", &major, &minor);
    }
    bd->GlVersion = (GLuint)(major * 100 + minor * 10);

    if (bd->GlVersion >= 320) {
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    }

    // Store GLSL version string so we can refer to it later in case we recreate shaders.
    // Note: GLSL version is NOT the same as GL version. Leave this to NULL if unsure.
    if (glsl_version == NULL) {
        glsl_version = "#version 130";
    }
    IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(bd->GlslVersionString));
    strcpy(bd->GlslVersionString, glsl_version);
    strcat(bd->GlslVersionString, "\n");

    // Make an arbitrary GL call (we don't actually need the result)
    // IF YOU GET A CRASH HERE: it probably means the OpenGL function loader didn't do its job. Let us know!
    GLint current_texture;
    pglGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

    // Detect extensions we support
    bd->HasClipOrigin = (bd->GlVersion >= 450);

    if (GL_SupportExtension("GL_ARB_clip_control")) {
        bd->HasClipOrigin = true;
    }
    return true;
}

void CImGuiBackend::Shutdown()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO &io = ImGui::GetIO();

    DestroyDeviceObjects();
    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;
    IM_DELETE(bd);
}

void CImGuiBackend::NewFrame()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call CImGuiBackend::Init()?");

    if (!bd->ShaderHandle) {
        CreateDeviceObjects();
    }

    // Setup display size (every frame to accommodate for window resizing)
    io.DisplaySize = ImVec2(glState.width, glState.height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    io.DeltaTime = (tr.frametime > 0.0f) ? Q_min(tr.frametime, 1.0f / 20.0f) : (1.f / CVAR_GET_FLOAT("fps_max"));
}

static void ImGui_ImplOpenGL3_SetupRenderState(ImDrawData *draw_data, int fb_width, int fb_height, GLuint vertex_array_object)
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    pglEnable(GL_BLEND);
    pglBlendEquation(GL_FUNC_ADD_EXT);
    pglBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    pglDisable(GL_CULL_FACE);
    pglDisable(GL_DEPTH_TEST);
    pglDisable(GL_STENCIL_TEST);
    pglEnable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    if (bd->GlVersion >= 310)
        glDisable(GL_PRIMITIVE_RESTART);
#endif
    pglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);


    // Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)
#if defined(GL_CLIP_ORIGIN)
    bool clip_origin_lower_left = true;
    if (bd->HasClipOrigin)
    {
        GLenum current_clip_origin = 0; glGetIntegerv(GL_CLIP_ORIGIN, (GLint *)&current_clip_origin);
        if (current_clip_origin == GL_UPPER_LEFT)
            clip_origin_lower_left = false;
    }
#endif

    // Setup viewport, orthographic projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    pglViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
#if defined(GL_CLIP_ORIGIN)
    if (!clip_origin_lower_left) { float tmp = T; T = B; B = tmp; } // Swap top and bottom if origin is upper left
#endif
    const float ortho_projection[4][4] =
    {
        { 2.0f / (R - L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f / (T - B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R + L) / (L - R),  (T + B) / (B - T),  0.0f,   1.0f },
    };
    pglUseProgram(bd->ShaderHandle);
    pglUniform1iARB(bd->AttribLocationTex, 0);
    pglUniformMatrix4fvARB(bd->AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);

#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330)
        glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 may set that otherwise.
#endif

    // Bind vertex/index buffers and setup attributes for ImDrawVert
    pglBindVertexArray(vertex_array_object);
    pglBindBufferARB(GL_ARRAY_BUFFER_ARB, bd->VboHandle);
    pglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bd->ElementsHandle);
    pglEnableVertexAttribArrayARB(bd->AttribLocationVtxPos);
    pglEnableVertexAttribArrayARB(bd->AttribLocationVtxUV);
    pglEnableVertexAttribArrayARB(bd->AttribLocationVtxColor);
    pglVertexAttribPointerARB(bd->AttribLocationVtxPos, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, pos));
    pglVertexAttribPointerARB(bd->AttribLocationVtxUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, uv));
    pglVertexAttribPointerARB(bd->AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid *)IM_OFFSETOF(ImDrawVert, col));
}

// OpenGL3 Render function.
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state explicitly.
// This is in order to be able to run within an OpenGL engine that doesn't do so.
void CImGuiBackend::RenderDrawData(ImDrawData *draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Backup GL state
    GLenum last_active_texture; pglGetIntegerv(GL_ACTIVE_TEXTURE_ARB, (GLint *)&last_active_texture);
    GL_SelectTexture(GL_TEXTURE0);
    GLuint last_program; pglGetIntegerv(GL_FRAGMENT_PROGRAM_ARB, (GLint *)&last_program);
    GLuint last_texture; pglGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    GLuint last_sampler; if (bd->GlVersion >= 330) { glGetIntegerv(GL_SAMPLER_BINDING, (GLint *)&last_sampler); }
    else { last_sampler = 0; }
#endif
    GLuint last_array_buffer; pglGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, (GLint *)&last_array_buffer);
    GLuint last_vertex_array_object; pglGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint *)&last_vertex_array_object);
    GLint last_polygon_mode[2]; pglGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    GLint last_viewport[4]; pglGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; pglGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    //GLenum last_blend_src_rgb; pglGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&last_blend_src_rgb);
    //GLenum last_blend_dst_rgb; pglGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&last_blend_dst_rgb);
    //GLenum last_blend_src_alpha; pglGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&last_blend_src_alpha);
    //GLenum last_blend_dst_alpha; pglGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&last_blend_dst_alpha);
    //GLenum last_blend_equation_rgb; pglGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&last_blend_equation_rgb);
    //GLenum last_blend_equation_alpha; pglGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&last_blend_equation_alpha);
    GLboolean last_enable_blend = pglIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = pglIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = pglIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_stencil_test = pglIsEnabled(GL_STENCIL_TEST);
    GLboolean last_enable_scissor_test = pglIsEnabled(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    GLboolean last_enable_primitive_restart = (bd->GlVersion >= 310) ? glIsEnabled(GL_PRIMITIVE_RESTART) : GL_FALSE;
#endif

    // Setup desired GL state
    // Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared among GL contexts)
    // The renderer would actually work without any VAO bound, but then our VertexAttrib calls would overwrite the default one currently bound.
    GLuint vertex_array_object = 0;
    pglGenVertexArrays(1, &vertex_array_object);
    ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];

        // Upload vertex/index buffers
        GLsizeiptrARB vtx_buffer_size = (GLsizeiptrARB)cmd_list->VtxBuffer.Size * (int)sizeof(ImDrawVert);
        GLsizeiptrARB idx_buffer_size = (GLsizeiptrARB)cmd_list->IdxBuffer.Size * (int)sizeof(ImDrawIdx);
        if (bd->VertexBufferSize < vtx_buffer_size)
        {
            bd->VertexBufferSize = vtx_buffer_size;
            pglBufferDataARB(GL_ARRAY_BUFFER_ARB, bd->VertexBufferSize, NULL, GL_STREAM_DRAW_ARB);
        }
        if (bd->IndexBufferSize < idx_buffer_size)
        {
            bd->IndexBufferSize = idx_buffer_size;
            pglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bd->IndexBufferSize, NULL, GL_STREAM_DRAW_ARB);
        }
        pglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, vtx_buffer_size, (const GLvoid *)cmd_list->VtxBuffer.Data);
        pglBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, idx_buffer_size, (const GLvoid *)cmd_list->IdxBuffer.Data);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                pglScissor((int)clip_min.x, (int)((float)fb_height - clip_max.y), (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));

                // Bind texture, Draw
                pglBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->GetTexID());
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_VTX_OFFSET
                if (bd->GlVersion >= 320)
                    pglDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void *)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset);
                else
#endif
                pglDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void *)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)));
            }
        }
    }

    // Destroy the temporary VAO
    pglDeleteVertexArrays(1, &vertex_array_object);

    // Restore modified GL state
    pglUseProgram(last_program);
    pglBindTexture(GL_TEXTURE_2D, last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330)
        glBindSampler(0, last_sampler);
#endif
    pglActiveTexture(last_active_texture);
    pglBindVertexArray(last_vertex_array_object);
    pglBindBufferARB(GL_ARRAY_BUFFER_ARB, last_array_buffer);
    //pglBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    //pglBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) pglEnable(GL_BLEND); else pglDisable(GL_BLEND);
    if (last_enable_cull_face) pglEnable(GL_CULL_FACE); else pglDisable(GL_CULL_FACE);
    if (last_enable_depth_test) pglEnable(GL_DEPTH_TEST); else pglDisable(GL_DEPTH_TEST);
    if (last_enable_stencil_test) pglEnable(GL_STENCIL_TEST); else pglDisable(GL_STENCIL_TEST);
    if (last_enable_scissor_test) pglEnable(GL_SCISSOR_TEST); else pglDisable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    if (bd->GlVersion >= 310) { if (last_enable_primitive_restart) glEnable(GL_PRIMITIVE_RESTART); else glDisable(GL_PRIMITIVE_RESTART); }
#endif

    pglPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
    pglViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    pglScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
    (void)bd; // Not all compilation paths use this
}

bool CImGuiBackend::CreateFontsTexture()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Build texture atlas
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

    // Upload texture to graphics system
    GLint last_texture;
    pglGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    pglGenTextures(1, &bd->FontTexture);
    pglBindTexture(GL_TEXTURE_2D, bd->FontTexture);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_UNPACK_ROW_LENGTH // Not on WebGL/ES
    pglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)(intptr_t)bd->FontTexture);

    // Restore state
    pglBindTexture(GL_TEXTURE_2D, last_texture);

    return true;
}

void CImGuiBackend::DestroyFontsTexture()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    if (bd->FontTexture)
    {
        pglDeleteTextures(1, &bd->FontTexture);
        io.Fonts->SetTexID(0);
        bd->FontTexture = 0;
    }
}

// If you get an error please report on github. You may try different GL context version or GLSL version. See GL<>GLSL version table at the top of this file.
static bool ImGui_CheckShader(GLuint handle, const char *desc)
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    GLint status = 0, log_length = 0;
    pglGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    pglGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s! With GLSL: %s\n", desc, bd->GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        pglGetShaderInfoLog(handle, log_length, NULL, (GLcharARB *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

// If you get an error please report on GitHub. You may try different GL context version or GLSL version.
static bool ImGui_CheckProgram(GLuint handle, const char *desc)
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    GLint status = 0, log_length = 0;
    pglGetProgramiv(handle, GL_LINK_STATUS, &status);
    pglGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to link %s! With GLSL %s\n", desc, bd->GlslVersionString);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        pglGetProgramInfoLog(handle, log_length, NULL, (GLcharARB *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

bool CImGuiBackend::CreateDeviceObjects()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();

    // Backup GL state
    GLint last_texture, last_array_buffer;
    pglGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    pglGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &last_array_buffer);
    GLint last_vertex_array;
    pglGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    // Parse GLSL version string
    int glsl_version = 130;
    sscanf(bd->GlslVersionString, "#version %d", &glsl_version);

    const GLcharARB *vertex_shader_glsl_120 =
        "uniform mat4 ProjMtx;\n"
        "attribute vec2 Position;\n"
        "attribute vec2 UV;\n"
        "attribute vec4 Color;\n"
        "varying vec2 Frag_UV;\n"
        "varying vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    Frag_UV = UV;\n"
        "    Frag_Color = Color;\n"
        "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLcharARB *vertex_shader_glsl_130 =
        "uniform mat4 ProjMtx;\n"
        "in vec2 Position;\n"
        "in vec2 UV;\n"
        "in vec4 Color;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    Frag_UV = UV;\n"
        "    Frag_Color = Color;\n"
        "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLcharARB *vertex_shader_glsl_300_es =
        "precision highp float;\n"
        "layout (location = 0) in vec2 Position;\n"
        "layout (location = 1) in vec2 UV;\n"
        "layout (location = 2) in vec4 Color;\n"
        "uniform mat4 ProjMtx;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    Frag_UV = UV;\n"
        "    Frag_Color = Color;\n"
        "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLcharARB *vertex_shader_glsl_410_core =
        "layout (location = 0) in vec2 Position;\n"
        "layout (location = 1) in vec2 UV;\n"
        "layout (location = 2) in vec4 Color;\n"
        "uniform mat4 ProjMtx;\n"
        "out vec2 Frag_UV;\n"
        "out vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    Frag_UV = UV;\n"
        "    Frag_Color = Color;\n"
        "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
        "}\n";

    const GLcharARB *fragment_shader_glsl_120 =
        "#ifdef GL_ES\n"
        "    precision mediump float;\n"
        "#endif\n"
        "uniform sampler2D Texture;\n"
        "varying vec2 Frag_UV;\n"
        "varying vec4 Frag_Color;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);\n"
        "}\n";

    const GLcharARB *fragment_shader_glsl_130 =
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";

    const GLcharARB *fragment_shader_glsl_300_es =
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";

    const GLcharARB *fragment_shader_glsl_410_core =
        "in vec2 Frag_UV;\n"
        "in vec4 Frag_Color;\n"
        "uniform sampler2D Texture;\n"
        "layout (location = 0) out vec4 Out_Color;\n"
        "void main()\n"
        "{\n"
        "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
        "}\n";

    // Select shaders matching our GLSL versions
    const GLcharARB *vertex_shader = NULL;
    const GLcharARB *fragment_shader = NULL;
    if (glsl_version < 130)
    {
        vertex_shader = vertex_shader_glsl_120;
        fragment_shader = fragment_shader_glsl_120;
    }
    else if (glsl_version >= 410)
    {
        vertex_shader = vertex_shader_glsl_410_core;
        fragment_shader = fragment_shader_glsl_410_core;
    }
    else if (glsl_version == 300)
    {
        vertex_shader = vertex_shader_glsl_300_es;
        fragment_shader = fragment_shader_glsl_300_es;
    }
    else
    {
        vertex_shader = vertex_shader_glsl_130;
        fragment_shader = fragment_shader_glsl_130;
    }

    // Create shaders
    const GLcharARB *vertex_shader_with_version[2] = { bd->GlslVersionString, vertex_shader };
    GLuint vert_handle = pglCreateShader(GL_VERTEX_SHADER_ARB);
    pglShaderSource(vert_handle, 2, vertex_shader_with_version, NULL);
    pglCompileShader(vert_handle);
    ImGui_CheckShader(vert_handle, "vertex shader");

    const GLcharARB *fragment_shader_with_version[2] = { bd->GlslVersionString, fragment_shader };
    GLuint frag_handle = pglCreateShader(GL_FRAGMENT_SHADER_ARB);
    pglShaderSource(frag_handle, 2, fragment_shader_with_version, NULL);
    pglCompileShader(frag_handle);
    ImGui_CheckShader(frag_handle, "fragment shader");

    // Link
    bd->ShaderHandle = pglCreateProgram();
    pglAttachShader(bd->ShaderHandle, vert_handle);
    pglAttachShader(bd->ShaderHandle, frag_handle);
    pglLinkProgram(bd->ShaderHandle);
    ImGui_CheckProgram(bd->ShaderHandle, "shader program");

    pglDetachShader(bd->ShaderHandle, vert_handle);
    pglDetachShader(bd->ShaderHandle, frag_handle);
    pglDeleteShader(vert_handle);
    pglDeleteShader(frag_handle);

    bd->AttribLocationTex = pglGetUniformLocation(bd->ShaderHandle, "Texture");
    bd->AttribLocationProjMtx = pglGetUniformLocation(bd->ShaderHandle, "ProjMtx");
    bd->AttribLocationVtxPos = (GLuint)pglGetAttribLocationARB(bd->ShaderHandle, "Position");
    bd->AttribLocationVtxUV = (GLuint)pglGetAttribLocationARB(bd->ShaderHandle, "UV");
    bd->AttribLocationVtxColor = (GLuint)pglGetAttribLocationARB(bd->ShaderHandle, "Color");

    // Create buffers
    pglGenBuffersARB(1, &bd->VboHandle);
    pglGenBuffersARB(1, &bd->ElementsHandle);

    CreateFontsTexture();

    // Restore modified GL state
    pglBindTexture(GL_TEXTURE_2D, last_texture);
    pglBindBufferARB(GL_ARRAY_BUFFER_ARB, last_array_buffer);
    pglBindVertexArray(last_vertex_array);

    return true;
}

void CImGuiBackend::DestroyDeviceObjects()
{
    ImGui_ImplOpenGL3_Data *bd = ImGui_ImplOpenGL3_GetBackendData();
    if (bd->VboHandle) { pglDeleteBuffersARB(1, &bd->VboHandle); bd->VboHandle = 0; }
    if (bd->ElementsHandle) { pglDeleteBuffersARB(1, &bd->ElementsHandle); bd->ElementsHandle = 0; }
    if (bd->ShaderHandle) { pglDeleteProgram(bd->ShaderHandle); bd->ShaderHandle = 0; }
    DestroyFontsTexture();
}
