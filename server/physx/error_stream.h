/*
error_stream.h - class for reporting errors from PhysX engine
Copyright (C) 2012 Uncle Mike
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
#include <PxConfig.h>
#include <PxErrorCallback.h>

class ErrorCallback : public physx::PxErrorCallback
{
public:
	virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line);\
	void hideWarning(bool bHide) { m_fHideWarning = bHide; }

private:
	bool m_fHideWarning = false;
};
