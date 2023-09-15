/*
error_stream.cpp - class for reporting errors from PhysX engine
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

#include "error_stream.h"
#include "alert.h"
#include "extdll.h"
#include "util.h"

using namespace physx;

void ErrorCallback::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
{
	switch (code)
	{
	case PxErrorCode::eINVALID_PARAMETER:
		ALERT(at_error, "invalid parameter: %s\n", message);
		break;
	case PxErrorCode::eINVALID_OPERATION:
		ALERT(at_error, "invalid operation: %s\n", message);
		break;
	case PxErrorCode::eOUT_OF_MEMORY:
		ALERT(at_error, "out of memory: %s\n", message);
		break;
	case PxErrorCode::eDEBUG_INFO:
		ALERT(at_console, "%s\n", message);
		break;
	case PxErrorCode::eDEBUG_WARNING:
		if (!m_fHideWarning) {
			ALERT(at_warning, "%s\n", message);
		}
		break;
	case PxErrorCode::ePERF_WARNING:
		ALERT(at_warning, "performance warning: %s\n", message);
		break;
	case PxErrorCode::eABORT:
		ALERT(at_error, "abort: %s\n", message);
		break;
	default:
		ALERT(at_error, "unknown error: %s\n", message);
		break;
	}
}
