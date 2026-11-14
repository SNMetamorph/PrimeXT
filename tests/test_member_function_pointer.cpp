/*
test_member_function_pointer.cpp - unit test for the member function pointer helpers
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

#include <cstdio>
#include <cstring>

#include "functionptr.h"

// The game declares the entity class with the single inheritance attribute, so
// that the compiler may use a smaller representation of the pointers to its
// member functions. The attribute is a no-op everywhere but MSVC, so the size
// of such a pointer depends on the compiler and on the flags it uses.
#if defined( _MSC_VER )
#define SINGLE_INHERITANCE __single_inheritance
#else
#define SINGLE_INHERITANCE
#endif

// The attribute belongs to the forward declaration, exactly like CBaseEntity
// in server/datamap.h does.
class SINGLE_INHERITANCE TestEntity;

class TestEntity
{
public:
	virtual ~TestEntity() {}

	void Think();
	void Touch( TestEntity *pOther );
};

class TestEntityDerived : public TestEntity
{
public:
	void Think();
	void Touch( TestEntity *pOther );
};

static int g_Counter = 0;

void TestEntity::Think() { g_Counter += 1; }
void TestEntity::Touch( TestEntity *pOther ) { g_Counter += 2; (void)pOther; }
void TestEntityDerived::Think() { g_Counter += 3; }
void TestEntityDerived::Touch( TestEntity *pOther ) { g_Counter += 4; (void)pOther; }

// This is the type the datamap stores all the callbacks as.
typedef void (TestEntity::*func_t)(void);

struct testcase_t
{
	const char *name;
	func_t func;
};

// The datamap casts every callback to func_t, exactly like DEFINE_FUNCTION does.
static const testcase_t tests[] =
{
	{ "TestEntity::Think", (func_t)( &TestEntity::Think ) },
	{ "TestEntityDerived::Think", (func_t)( &TestEntityDerived::Think ) },
	{ "TestEntity::Touch", (func_t)( (void (TestEntity::*)(void))( &TestEntity::Touch ) ) },
	{ "TestEntityDerived::Touch", (func_t)( (void (TestEntity::*)(void))( &TestEntityDerived::Touch ) ) },
};

#define ARRAYSIZE( x ) ( sizeof( x ) / sizeof( ( x )[0] ) )

int main( void )
{
	for( size_t i = 0; i < ARRAYSIZE( tests ); i++ )
	{
		const testcase_t *test = &tests[i];

		// The identity of the function has to be extractable, otherwise the
		// server can't tell which function an entity refers to, and the
		// assertion in server/util.cpp used to fire (issue #209).
		void *identity = MemberFunctionIdentity( test->func );

		if( !identity )
		{
			std::fprintf( stderr, "%s: failed to extract the identity of %s\n", __FILE__, test->name );
			std::fprintf( stderr, "a pointer to a member function is %u bytes here, a plain pointer is %u bytes\n",
				(unsigned)sizeof( func_t ), (unsigned)sizeof( void * ) );
			return 1;
		}

		// The identity has to be stable.
		func_t copy = test->func;

		if( MemberFunctionIdentity( copy ) != identity )
		{
			std::fprintf( stderr, "%s: the identity of %s is not stable\n", __FILE__, test->name );
			return 1;
		}

		// The whole pointer has to survive the copy: the adjustment of the
		// "this" pointer is a part of it.
		func_t restored = nullptr;

		CopyMemberFunction( restored, test->func );

		if( memcmp( &restored, &test->func, sizeof( func_t ) ) != 0 )
		{
			std::fprintf( stderr, "%s: %s was not copied as a whole\n", __FILE__, test->name );
			return 1;
		}
	}

	// Different functions have to have different identities, otherwise the
	// datamap would find the wrong name for a function.
	for( size_t i = 0; i < ARRAYSIZE( tests ); i++ )
	{
		for( size_t j = i + 1; j < ARRAYSIZE( tests ); j++ )
		{
			if( MemberFunctionIdentity( tests[i].func ) == MemberFunctionIdentity( tests[j].func ) )
			{
				std::fprintf( stderr, "%s: %s and %s have the same identity\n",
					__FILE__, tests[i].name, tests[j].name );
				return 1;
			}
		}
	}

	std::fprintf( stderr, "test_member_function_pointer: all cases passed\n" );
	return 0;
}
