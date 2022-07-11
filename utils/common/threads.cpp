/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

#include "cmdlib.h"
#define NO_THREAD_NAMES
#include "threads.h"
#include <cstdio>
//#include <cstring>


#define PACIFIER_STEP	40
#define PACIFIER_REM	( PACIFIER_STEP / 10 )

static volatile int		g_oldf = -1;
static qboolean		g_pacifier = false;
static volatile int		g_dispatch = 0;
static volatile int		g_workcount = 0;

void UpdatePacifier( float percent )
{
	int	f;

	f = (int)(percent * (float)PACIFIER_STEP);
	f = bound( g_oldf, f, PACIFIER_STEP );
	
	if( f != g_oldf )
	{
		for( int i = g_oldf + 1; i <= f; i++ )
		{
			if(( i % PACIFIER_REM ) == 0 )
			{
				Msg( "%d%%", ( i / PACIFIER_REM ) * 10 );
				fflush( stdout );
			}
			else
			{
				if( i != PACIFIER_STEP )
				{
					Msg( "." );
					fflush( stdout );
				}
			}
		}

		g_oldf = f;
	}
}

void StartPacifier( void )
{
	g_oldf = -1;
	UpdatePacifier( 0.001f );
}

void EndPacifier( double total )
{
	UpdatePacifier( 1.0f );
	Msg( " (%.2f secs)\n", total );
}


#ifdef WIN32
#include <windows.h>

#define THREAD_STACK_SIZE	(4096 * 1024)	// 4 Mb

typedef struct thread_s
{
	int		number;	// threadnum
	pfnRunThreads	func;	// thread func
} thread_t;

static HANDLE		g_threadhandles[MAX_THREADS];
static thread_t		g_threads[MAX_THREADS];
static qboolean		g_threaded = false;
static pfnThreadWork	g_workfunction;
intz			g_numthreads = -1;
static int		g_oldnumthreads;
static bool		g_enter;
CRITICAL_SECTION		g_crit;

/*
=============
GetThreadWork

=============
*/
int GetThreadWork( void )
{
	int	r;

	ThreadLock();

	if( g_dispatch == g_workcount )
	{
		ThreadUnlock();
		return -1;
	}

	if( g_pacifier )
		UpdatePacifier( (float)g_dispatch / g_workcount );

	r = g_dispatch;
	g_dispatch++;
	ThreadUnlock ();

	return r;
}

static void ThreadWorkerFunction( int thread )
{
	int	work;

	while( 1 )
	{
		work = GetThreadWork ();
		if( work == -1 ) break;
		g_workfunction( work, thread );
	}
}

// This runs in the thread and dispatches a RunThreadsFn call.
static DWORD WINAPI InternalRunThreadsFn( LPVOID pData )
{
	thread_t *pThread = (thread_t *)pData;

	pThread->func( pThread->number );

	return 0;
}

void ThreadSetDefault( void )
{
	SYSTEM_INFO	info;

	if( g_numthreads == -1 ) 
	{
		// not set manually
		GetSystemInfo( &info );
		g_numthreads = info.dwNumberOfProcessors;
	}

	if( g_numthreads < 1 || g_numthreads > MAX_THREADS )
		g_numthreads = 1;

	MsgDev( D_REPORT, "%i threads\n", g_numthreads );
	g_oldnumthreads = g_numthreads;
}

void ThreadLock( void )
{
	if( !g_threaded ) return;

	EnterCriticalSection( &g_crit );

	if( g_enter ) COM_FatalError( "recursive ThreadLock\n" );
	g_enter = true;
}

void ThreadUnlock( void )
{
	if( !g_threaded ) return;

	if( !g_enter ) COM_FatalError( "ThreadUnlock without lock\n" );
	g_enter = false;

	LeaveCriticalSection( &g_crit );
}

bool ThreadLocked( void )
{
	return g_enter;
}

void ThreadPush( void )
{
	g_numthreads = 1;
}

void ThreadPop( void )
{
	g_numthreads = g_oldnumthreads;
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn( int workcnt, bool showpacifier, pfnRunThreads func )
{
	double	start, end;
	DWORD	dwDummy;
	int	i;

	if( showpacifier )
	{
		if( g_numthreads == 1 )
			Msg( " (single-threaded)\n" );
		else Msg( "\n" );
	}

	start = I_FloatTime();
	g_pacifier = showpacifier;
	g_workcount = workcnt;
	g_dispatch = 0;
	if( g_pacifier ) StartPacifier();

	if( g_numthreads == 1 )
	{	
		// use same thread
		func( 0 );
	}
	else
	{
		// run threads in parallel
		InitializeCriticalSection( &g_crit );
		g_threaded = true;

		for( i = 0; i < g_numthreads; i++ )
		{
			g_threads[i].number = i;
			g_threads[i].func = func;

			g_threadhandles[i] = CreateThread( NULL, THREAD_STACK_SIZE, InternalRunThreadsFn, &g_threads, 0, &dwDummy );
		}

		WaitForMultipleObjects( g_numthreads, g_threadhandles, TRUE, INFINITE );

		for ( i = 0; i < g_numthreads; i++ )
			CloseHandle( g_threadhandles[i] );

		DeleteCriticalSection( &g_crit );
		g_threaded = false;
	}

	end = I_FloatTime ();

	if( g_pacifier ) EndPacifier( end - start );
}

void RunThreadsOnIndividual( int workcnt, bool showpacifier, pfnThreadWork func )
{
	g_workfunction = func;
	RunThreadsOn( workcnt, showpacifier, ThreadWorkerFunction );
}

void RunThreadsOnIncremental( int workcnt, bool showpacifier, pfnRunThreads func )
{
	RunThreadsOn( workcnt, showpacifier, func );
}

#endif

#ifdef __linux__
#define Error COM_FatalError

static pfnThreadWork	g_workfunction;
static pfnRunThreads	g_runfunction;


qboolean	threaded;

#define	USED

int		g_numthreads = 4;

void ThreadSetDefault (void)
{
	g_numthreads = 4;
}

#include <pthread.h>

pthread_mutex_t	*my_mutex;

void ThreadLock (void)
{
	if (my_mutex)
		pthread_mutex_lock (my_mutex);
}

void ThreadUnlock (void)
{
	if (my_mutex)
		pthread_mutex_unlock (my_mutex);
}

/*
=============
GetThreadWork

=============
*/
int GetThreadWork( void )
{
	int	r;

	ThreadLock();

	if( g_dispatch == g_workcount )
	{
		ThreadUnlock();
		return -1;
	}

	if( g_pacifier )
		UpdatePacifier( (float)g_dispatch / g_workcount );

	r = g_dispatch;
	g_dispatch++;
	ThreadUnlock ();

	return r;
}

static void ThreadWorkerFunction( int thread )
{
	int work;

	while( 1 )
	{
		work = GetThreadWork ();
		if( work == -1 ) break;
		g_workfunction( work, thread );
	}
}

static void *ThreadRunFunction( void *data )
{
	g_runfunction( (size_t)data );
	return NULL;
}

void RunThreadsOnIndividual( int workcnt, bool showpacifier, pfnThreadWork func )
{
	g_workfunction = func;
	RunThreadsOn( workcnt, showpacifier, ThreadWorkerFunction );
}

void RunThreadsOnIncremental( int workcnt, bool showpacifier, pfnRunThreads func )
{
	RunThreadsOn( workcnt, showpacifier, func );
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn (int workcnt, bool showpacifier, pfnRunThreads func )
{
	g_runfunction  = func;

	size_t		i;
	pthread_t	work_threads[MAX_THREADS];
	void *status;
	pthread_attr_t	attrib;
	pthread_mutexattr_t	mattrib;
	int		start, end;

	start = I_FloatTime ();
	g_dispatch = 0;
	g_workcount = workcnt;
	g_pacifier = showpacifier;
	threaded = true;

	if( showpacifier )
	{
		if( g_numthreads == 1 ) Msg( " (single-threaded)\n" );
		else Msg( "\n" );
        }

	if (g_pacifier)
		StartPacifier();

	if (!my_mutex)
	{
		my_mutex = (pthread_mutex_t*)Mem_Alloc (sizeof(*my_mutex));
		if (pthread_mutexattr_init (&mattrib) == -1)
			Error ("pthread_mutex_attr_create failed");
		if (pthread_mutex_init (my_mutex, &mattrib) == -1)
			Error ("pthread_mutex_init failed");
	}

	if (pthread_attr_init (&attrib) == -1)
		Error ("pthread_attr_create failed");
	if (pthread_attr_setstacksize (&attrib, 0x100000) == -1)
		Error ("pthread_attr_setstacksize failed");

	for (i=0 ; i<g_numthreads ; i++)
	{
  		if (pthread_create(&work_threads[i], &attrib
		, ThreadRunFunction, (void*)i) == -1)
			Error ("pthread_create failed");
	}

	for (i=0 ; i<g_numthreads ; i++)
	{
		if (pthread_join (work_threads[i], &status) == -1)
			Error ("pthread_join failed");
	}

	threaded = false;

	end = I_FloatTime ();

	if( g_pacifier ) EndPacifier( end - start );
}


#endif
