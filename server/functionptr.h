/*
functionptr.h - helpers for working with pointers to member functions
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <cstddef>
#include <cstring>

//
// The representation of a pointer to a member function is implementation-defined.
//
// The game declares the entity class with the single inheritance attribute, so
// that the compiler may use a smaller representation for the pointers to its
// member functions - but that attribute is a no-op everywhere but MSVC, and
// even MSVC falls back to the full representation with the /vmm and /vmg flags.
//
// The full representation consists of two words: the address of the function
// itself and the adjustment of the "this" pointer. On a 64-bit build that's
// 16 bytes, while a plain pointer is only 8 bytes long, so comparing the two
// sizes is meaningless.
//
// The datamap has to compare the pointers to member functions of the entities
// with the ones it stores itself, and has to restore them, see server/util.cpp
// and issue #209.
//

// Returns the identity of the function, which is the address of the function
// itself. With the two-word representation it's the first word of the object.
template<typename T>
inline void *MemberFunctionIdentity( T func )
{
	static_assert( sizeof(T) == sizeof(void *) || sizeof(T) == 2 * sizeof(void *),
		"unsupported pointer-to-member-function representation" );

	void *identity = nullptr;
	memcpy( &identity, &func, sizeof( identity ) );
	return identity;
}

// Returns the identity of the function stored in the given field of an entity.
// The type of the field is not known here, so only the first word is read.
inline void *MemberFunctionIdentityOfField( const void *field )
{
	void *identity = nullptr;
	memcpy( &identity, field, sizeof( identity ) );
	return identity;
}

// Copies a member function pointer into the field of an entity.
//
// The datamap stores all the callbacks as func_t, while the entities use the
// exact type of the callback, so the pointer is copied as a whole: copying
// only the address of the function would leave the adjustment of the "this"
// pointer of the destination untouched.
template<typename Dst, typename Src>
inline void CopyMemberFunction( Dst &dst, const Src &src )
{
	static_assert( sizeof(Dst) == sizeof(Src),
		"member function pointers of different size" );

	memcpy( &dst, &src, sizeof( dst ) );
}
