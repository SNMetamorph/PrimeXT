/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

extern int g_numthreads;

#define MAX_THREADS		16

typedef void (*pfnThreadWork)( int current, int threadnum );
typedef void (*pfnRunThreads)( int threadnum );

int GetThreadWork( void );
void ThreadSetDefault( void );
void RunThreadsOnIndividual( int workcnt, bool showpacifier, pfnThreadWork func );
void RunThreadsOnIncremental( int workcnt, bool showpacifier, pfnRunThreads func );
void RunThreadsOn( int workcnt, bool showpacifier, pfnRunThreads func );
bool ThreadLocked( void );
void ThreadLock( void );
void ThreadUnlock( void );
void ThreadPush( void );
void ThreadPop( void );

void StartPacifier( void );
void UpdatePacifier( float percent );
void EndPacifier( double total );

#ifndef NO_THREAD_NAMES
#define RunThreadsOn( n, p, f ) { if( p ) Msg( "%-20s", #f ":" ); RunThreadsOn( n, p, f ); }
#define RunThreadsOnIncremental( n, p, f, i ) { if( p ) Msg( "%s %i:", #f, i ); RunThreadsOnIncremental( n, p, f ); }
#define RunThreadsOnIndividual( n, p, f ) { if (p) Msg( "%-20s", #f ":" ); RunThreadsOnIndividual( n, p, f ); }
#endif
