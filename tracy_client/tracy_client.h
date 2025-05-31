/*
tracy_client.h - Tracy profiler headers and utilities
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
#pragma once

#ifdef _MSC_VER
#ifndef TracyLine
// MSVC Edit and continue __LINE__ is non-constant.
// See https://developercommunity.visualstudio.com/t/-line-cannot-be-used-as-an-argument-for-constexpr/195665
#define TracyLine TracyConcat(__LINE__,U) 
#endif
#endif

#include <tracy/Tracy.hpp>
