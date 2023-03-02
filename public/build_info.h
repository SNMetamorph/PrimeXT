/*
build_info.h - routines for getting information about build host OS, architecture and VCS state
Copyright (C) 2022 SNMetamorph

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
#include "build.h"

namespace BuildInfo
{
	// Returns current name of operating system without any spaces.
	constexpr const char *GetPlatform()
	{
#if XASH_MINGW
		return "win32-mingw";
#elif XASH_WIN32
		return "win32";
#elif XASH_ANDROID
		return "android";
#elif XASH_LINUX
		return "linux";
#elif XASH_APPLE
		return "apple";
#elif XASH_FREEBSD
		return "freebsd";
#elif XASH_NETBSD
		return "netbsd";
#elif XASH_OPENBSD
		return "openbsd";
#elif XASH_EMSCRIPTEN
		return "emscripten";
#elif XASH_DOS4GW
		return "dos4gw";
#elif XASH_HAIKU
		return "haiku";
#elif XASH_SERENITY
		return "serenityos";
#else
#error "Place your operating system name here! If this is a mistake, try to fix conditions above and report a bug"
#endif
		return "unknown";
	}

	// Returns current name of the architecture without any spaces.
	constexpr const char *GetArchitecture()
	{
#if XASH_AMD64
		return "amd64";
#elif XASH_X86
		return "i386";
#elif XASH_ARM && XASH_64BIT
		return "arm64";
#elif XASH_ARM
		return "armv"
	#if XASH_ARM == 8
		"8_32" // for those who (mis)using 32-bit OS on 64-bit CPU
	#elif XASH_ARM == 7
		"7"
	#elif XASH_ARM == 6
		"6"
	#elif XASH_ARM == 5
		"5"
	#elif XASH_ARM == 4
		"4"
	#endif

	#if XASH_ARM_HARDFP
		"hf"
	#else
		"l"
	#endif
	;
#elif XASH_MIPS && XASH_BIG_ENDIAN
		return "mips"
	#if XASH_64BIT
		"64"
	#endif
	#if XASH_LITTLE_ENDIAN
		"el"
	#endif
	;
#elif XASH_RISCV
		return "riscv"
	#if XASH_64BIT
		"64"
	#else
		"32"
	#endif
	#if XASH_RISCV_SINGLEFP
		"d"
	#elif XASH_RISCV_DOUBLEFP
		"f"
	#endif
	;
#elif XASH_JS
		return "javascript";
#elif XASH_E2K
		return "e2k";
#else
#error "Place your architecture name here! If this is a mistake, try to fix conditions above and report a bug"
#endif
		return "unknown";
	}

	// Returns a short hash of current commit in VCS as string.
	// XASH_BUILD_COMMIT must be passed in quotes
	constexpr const char *GetCommitHash()
	{
#ifdef XASH_BUILD_COMMIT
		return XASH_BUILD_COMMIT;
#else
		return "notset";
#endif
	}

	// Returns project GitHub repository URL.
	constexpr const char *GetGitHubLink()
	{
		return "https://github.com/SNMetamorph/PrimeXT";
	}

	// Returns build host machine date when program was built.
	constexpr const char *GetDate()
	{
		return __DATE__;
	}
};
