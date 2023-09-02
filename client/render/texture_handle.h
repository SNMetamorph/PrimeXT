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

#pragma once
#include "render_api.h"
#include <stdint.h>

class TextureHandle
{
public:
	friend TextureHandle CREATE_TEXTURE(const char *name, int width, int height, const void *buffer, int flags);
	friend TextureHandle CREATE_TEXTURE_ARRAY(const char *name, int width, int height, int depth, const void *buffer, int flags);
	friend TextureHandle LOAD_TEXTURE(const char *name, const uint8_t *buf, size_t size, int flags);
	friend TextureHandle LOAD_TEXTURE_ARRAY(const char **names, int flags);
	friend TextureHandle FIND_TEXTURE(const char *name);
	friend const uint8_t *GET_TEXTURE_DATA(TextureHandle texHandle);
	friend void CIN_UPLOAD_FRAME(TextureHandle texture, int cols, int rows, int width, int height, const byte *data);
	friend void GL_Bind(int32_t tmu, TextureHandle texnum);
	friend void GL_UpdateTexSize(TextureHandle texture, int width, int height, int depth);
	friend void FREE_TEXTURE(TextureHandle texHandle);

	TextureHandle() = default;
	~TextureHandle() = default;
	TextureHandle(struct mspriteframe_s *frame);
	TextureHandle(struct texture_s *tex);
	TextureHandle(struct mstudiotex_s *tex);

	static TextureHandle Null();
	static TextureHandle GetSkyboxTextures(int32_t index);
	bool operator!=(const TextureHandle& operand) const;
	bool operator==(const TextureHandle& operand) const;

	bool Initialized() const;
	int GetGlHandle() const;
	int32_t GetGlFormat() const;
	int32_t GetDxtEncodeType() const;
	texFlags_t GetFlags() const;
	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	int32_t GetDepth() const;
	uint32_t GetGlTarget() const;
	int32_t ToInt() const {  
		return m_iInternalIndex; 
	}

private:
	TextureHandle(int internalHandle) {
		m_iInternalIndex = internalHandle;
	};

	int32_t m_iInternalIndex = 0;
};
