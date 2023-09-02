/*
texture_handle.h - convenient wrapper for implicit separating
OpenGL texture handles and engine-side texture handles
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

#include "texture_handle.h"
#include "vector.h"
#include "enginecallback.h"
#include "com_model.h"
#include "studio.h"

TextureHandle::TextureHandle(mspriteframe_t *frame)
{
	ASSERT(frame != nullptr);
	m_iInternalIndex = frame->gl_texturenum;
}

TextureHandle::TextureHandle(texture_t *tex)
{
	ASSERT(tex != nullptr);
	m_iInternalIndex = tex->gl_texturenum;
}

TextureHandle::TextureHandle(mstudiotexture_t *tex)
{
	ASSERT(tex != nullptr);
	m_iInternalIndex = tex->index;
}

TextureHandle TextureHandle::Null()
{
	static TextureHandle nullHandle(0);
	return nullHandle;
}

TextureHandle TextureHandle::GetSkyboxTextures(int32_t index)
{
	return TextureHandle(RENDER_GET_PARM(PARM_TEX_SKYBOX, index));
}

bool TextureHandle::operator!=(const TextureHandle &operand) const
{
	return m_iInternalIndex != operand.m_iInternalIndex;
}

bool TextureHandle::operator==(const TextureHandle & operand) const
{
	return m_iInternalIndex == operand.m_iInternalIndex;
}

bool TextureHandle::Initialized() const
{
	return m_iInternalIndex > 0;
}

int TextureHandle::GetGlHandle() const
{
	return RENDER_GET_PARM(PARM_TEX_TEXNUM, m_iInternalIndex);
}

int32_t TextureHandle::GetGlFormat() const
{
	return RENDER_GET_PARM(PARM_TEX_GLFORMAT, m_iInternalIndex);
}

int32_t TextureHandle::GetDxtEncodeType() const
{
	// TODO maybe do it as enum? now it's DXT_ENCODE_* macros
	return RENDER_GET_PARM(PARM_TEX_ENCODE, m_iInternalIndex);
}

texFlags_t TextureHandle::GetFlags() const
{
	return static_cast<texFlags_t>(RENDER_GET_PARM(PARM_TEX_FLAGS, m_iInternalIndex));
}

uint32_t TextureHandle::GetWidth() const
{
	return static_cast<uint32_t>(RENDER_GET_PARM(PARM_TEX_SRC_WIDTH, m_iInternalIndex));
}

uint32_t TextureHandle::GetHeight() const
{
	return static_cast<uint32_t>(RENDER_GET_PARM(PARM_TEX_SRC_HEIGHT, m_iInternalIndex));
}

int32_t TextureHandle::GetDepth() const
{
	return static_cast<int32_t>(RENDER_GET_PARM(PARM_TEX_DEPTH, m_iInternalIndex));
}

uint32_t TextureHandle::GetGlTarget() const
{
	return static_cast<uint32_t>(RENDER_GET_PARM(PARM_TEX_TARGET, m_iInternalIndex));
}
