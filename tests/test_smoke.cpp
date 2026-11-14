/*
test_smoke.cpp - sanity check for the test harness itself
Copyright (C) 2026 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <cstdio>

static int Test_Harness( void )
{
	if( 1 + 1 != 2 )
	{
		std::fprintf( stderr, "%s:%d: arithmetic is broken\n", __FILE__, __LINE__ );
		return 1;
	}

	return 0;
}

int main( void )
{
	return Test_Harness();
}
