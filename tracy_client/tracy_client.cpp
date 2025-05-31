/*
tracy_client.cpp - Tracy profiler dummy exports
Copyright (C) 2025 ugo_zapad

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/	

#if defined _WIN32 || defined __CYGWIN__
	#ifdef __GNUC__
		#define EXPORT __attribute__ ((dllexport))
	#else
		#define EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
	#endif
#else
	#if __GNUC__ >= 4
		#define EXPORT __attribute__ ((visibility ("default")))
	#else
		#define EXPORT
	#endif
#endif
#define DLLEXPORT EXPORT
#define _DLLEXPORT EXPORT

DLLEXPORT int g_ProfilerDummy = 1;
