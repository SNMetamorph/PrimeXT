/*
bsp31migrate.cpp - bsp tool main file
Copyright (C) 2016 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "bsp31migrate.h"

int main( int argc, char **argv )
{
	int  i;

//	if( argc <= 1 )
//		COM_FatalError( "usage: bsp31migrate -file <path.bsp> -output <path.bsp>\n" );

	COM_InitCmdlib( argv, argc );

	if(( i = COM_CheckParm( "-dev" )) != 0 )
	{
		SetDeveloperLevel( atoi( argv[i+1] ));
	}

	return BspConvert( argc, argv );
}