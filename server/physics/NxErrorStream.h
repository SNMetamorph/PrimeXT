/*
NxErrorStream.h - a Novodex physics engine implementation
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

#ifndef NX_ERROR_STREAM_H
#define NX_ERROR_STREAM_H

class NxErrorStream : public NxUserOutputStream
{
	bool	m_fHideWarning;
public:
	void reportError( NxErrorCode e, const char *message, const char* file, int line )
	{
		switch( e )
		{
		case NXE_INVALID_PARAMETER:
			ALERT( at_error, "invalid parameter: %s\n", message );
			break;
		case NXE_INVALID_OPERATION:
			ALERT( at_error, "invalid operation: %s\n", message );
			break;
		case NXE_OUT_OF_MEMORY:
			ALERT( at_error, "out of memory: %s\n", message );
			break;
		case NXE_DB_INFO:
			ALERT( at_console, "%s\n", message );
			break;
		case NXE_DB_WARNING:
			if( !m_fHideWarning )
				ALERT( at_warning, "%s\n", message );
			break;
		default:
			ALERT( at_error, "unknown error: %s\n", message );
			break;
		}
	}

	NxAssertResponse reportAssertViolation( const char *message, const char *file, int line )
	{
		ALERT( at_aiconsole, "access violation : %s (%s line %d)\n", message, file, line );
		return NX_AR_BREAKPOINT;		
	}

	void print( const char *message )
	{
		ALERT( at_console, "%s\n", message );
	}

	// hide warning: "createActor: Dynamic triangle mesh instantiated!"
	void hideWarning( bool bHide ) { m_fHideWarning = bHide; }
};

#endif//NX_ERROR_STREAM
