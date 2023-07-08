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
#include "build_enums.h"
#include <stdint.h>

namespace BuildInfo
{
	// Returns current name of operating system without any spaces.
	constexpr const char *GetPlatform()
	{
#if XASH_PLATFORM == PLATFORM_WIN32
		return "win32";
#elif XASH_PLATFORM == PLATFORM_ANDROID
		return "android";
#elif XASH_PLATFORM == PLATFORM_LINUX_UNKNOWN
		return "linuxunkabi";
#elif XASH_PLATFORM == PLATFORM_LINUX
		return "linux";
#elif XASH_PLATFORM == PLATFORM_APPLE
		return "apple";
#elif XASH_PLATFORM == PLATFORM_FREEBSD
		return "freebsd";
#elif XASH_PLATFORM == PLATFORM_NETBSD
		return "netbsd";
#elif XASH_PLATFORM == PLATFORM_OPENBSD
		return "openbsd";
#elif XASH_PLATFORM == PLATFORM_EMSCRIPTEN
		return "emscripten";
#elif XASH_PLATFORM == PLATFORM_DOS4GW
		return "dos4gw";
#elif XASH_PLATFORM == PLATFORM_HAIKU
		return "haiku";
#elif XASH_PLATFORM == PLATFORM_SERENITY
		return "serenityos";
#elif XASH_PLATFORM == PLATFORM_IRIX
		return "irix";
#elif XASH_PLATFORM == PLATFORM_NSWITCH
		return "nswitch";
#elif XASH_PLATFORM == PLATFORM_PSVITA
		return "psvita";
#else
#error "Place your operating system name here! If this is a mistake, try to fix conditions above and report a bug"
#endif
		return "unknown";
	}

	// Returns current name of the architecture without any spaces.
	constexpr const char *GetArchitecture()
	{
		constexpr uint32_t arch = XASH_ARCHITECTURE;
		constexpr uint32_t abi = XASH_ARCHITECTURE_ABI;
		constexpr uint32_t endianness = XASH_ENDIANNESS;
#if XASH_64BIT
		constexpr uint32_t is64 = true;
#else
		constexpr uint32_t is64 = false;
#endif

		switch( arch )
		{
		case ARCHITECTURE_AMD64:
			return "amd64";
		case ARCHITECTURE_X86:
			return "i386";
		case ARCHITECTURE_E2K:
			return "e2k";
		case ARCHITECTURE_JS:
			return "javascript";
		case ARCHITECTURE_MIPS:
			return endianness == ENDIANNESS_LITTLE ?
				( is64 ? "mips64el" : "mipsel" ):
				( is64 ? "mips64" : "mips" );
		case ARCHITECTURE_ARM:
			// no support for big endian ARM here
			if( endianness == ENDIANNESS_LITTLE )
			{
				constexpr uint32_t ver = ( abi >> ARCH_ARM_VER_SHIFT ) & ARCH_ARM_VER_MASK;
				constexpr bool hardfp = ( abi & ARCH_ARM_HARDFP ) != 0;

				if( is64 )
					return "arm64"; // keep as arm64, it's not aarch64!

				switch( ver )
				{
				case 8:
					return hardfp ? "armv8_32hf" : "armv8_32l";
				case 7:
					return hardfp ? "armv7hf" : "armv7l";
				case 6:
					return "armv6l";
				case 5:
					return "armv5l";
				case 4:
					return "armv4l";
				}
			}
			break;
		case ARCHITECTURE_RISCV:
			switch( abi )
			{
			case ARCH_RISCV_FP_SOFT:
				return is64 ? "riscv64" : "riscv32";
			case ARCH_RISCV_FP_SINGLE:
				return is64 ? "riscv64f" : "riscv32f";
			case ARCH_RISCV_FP_DOUBLE:
				return is64 ? "riscv64d" : "riscv64f";
			}
			break;
		}

		return is64 ?
			( endianness == ENDIANNESS_LITTLE ? "unknown64el" : "unknownel" ) :
			( endianness == ENDIANNESS_LITTLE ? "unknown64be" : "unknownbe" );
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
