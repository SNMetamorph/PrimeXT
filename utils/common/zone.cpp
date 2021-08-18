/*
zone.cpp - simple memory manager with leak detector
Copyright (C) 2015 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <windows.h>
#include "cmdlib.h"
#include "threads.h"
#include "stringlib.h"
#include "mathlib.h"

#define ZONE_ATTEMPT_CALLOC
//#define ZONE_DEBUG

static int	c_alloc[C_MAXSTAT] = { 0 };
static size_t	total_active, total_peakactive;

typedef struct memhdr_s
{
	size_t	size;
} memhdr_t;

const char *c_stats[] =
{
	"Common",
	"Temporary",
	"Template",
	"FileSystem",
	"Winding",
	"Brush Side",
	"Bsp Brush",
	"LeafNode",
	"Surface",
	"Bsp Tree",
	"Portal",
	"String",
	"EntityPair",
	"Patch",
};

// some platforms have a malloc that returns NULL but succeeds later
// (Windows growing its swapfile for example)
static void *attempt_calloc( size_t size )
{
	uint	attempts = 600;

	// one minute before completely failed
	while( attempts-- )
	{
		void	*base;

		if(( base = (void *)calloc( size, 1 )) != NULL )
			return base;
		// try for half a second or so
		Sleep( 100 );
	}
	return NULL;
}

/*
=============
Mem_Alloc

allocate mem
=============
*/
void *Mem_Alloc( size_t size, unsigned int target )
{
	memhdr_t	*memhdr = NULL;
	void	*mem;

	if( size <= 0 ) return NULL;

#ifdef ZONE_ATTEMPT_CALLOC
	mem = attempt_calloc( sizeof( memhdr_t ) + size );
#else
	mem = calloc( sizeof( memhdr_t ) + size, 1 );
#endif
	if( mem == NULL )
	{
		if( target == C_SAFEALLOC )
			return NULL;
		COM_FatalError( "out of memory!\n" );
	}

	memhdr = (memhdr_t *)mem;
	memhdr->size = size;
#ifdef ZONE_DEBUG
	ThreadLock();
	total_active += size;
	total_peakactive = Q_max( total_peakactive, total_active );
	c_alloc[target]++;
	ThreadUnlock();
#endif
	return (void *)((byte *)mem + sizeof( memhdr_t ));
}

void *Mem_Realloc( void *ptr, size_t size, unsigned int target )
{
	memhdr_t	*memhdr = NULL;
	void	*mem;

	if( size <= 0 ) return ptr; // no need to reallocate

	if( ptr )
	{
		memhdr = (memhdr_t *)((byte *)ptr - sizeof( memhdr_t ));
		if( size == memhdr->size ) return ptr;
	}

	mem = Mem_Alloc( size, target );

	if( ptr ) // first allocate?
	{ 
		size_t	newsize = memhdr->size < size ? memhdr->size : size; // upper data can be trucnated!
		memcpy( mem, ptr, newsize );
		Mem_Free( ptr, target ); // free unused old block
          }

	return mem;
}

void Mem_Free( void *ptr, unsigned int target )
{
	memhdr_t	*chunk;

	if( !ptr ) return;

	chunk = (memhdr_t *)((byte *)ptr - sizeof( memhdr_t ));
#ifdef ZONE_DEBUG
	ThreadLock();
	total_active -= chunk->size;
	c_alloc[target]--;
	ThreadUnlock();
#endif
	free( chunk );
}

void Mem_Check( void )
{
#ifdef ZONE_DEBUG
	MsgDev( D_INFO, "active memory %s, peak memory %s\n", Q_memprint( total_active ), Q_memprint( total_peakactive ));
	for( int i = 0; i < C_MAXSTAT; i++ )
	{
		if( c_alloc[i] ) MsgDev( D_REPORT, "%s memory allocations leaks count: %d\n", c_stats[i], c_alloc[i] );
	}
#endif
}

void Mem_Peak( void )
{
#ifdef ZONE_DEBUG
	MsgDev( D_INFO, "active memory %s, peak memory %s\n", Q_memprint( total_active ), Q_memprint( total_peakactive ));
#endif
}

size_t Mem_Size( void *ptr )
{
	memhdr_t	*chunk;

	if( !ptr ) return 0;

	chunk = (memhdr_t *)((byte *)ptr - sizeof( memhdr_t ));

	return chunk->size;
}