#ifndef THREADS_H__
#define THREADS_H__
#include "cmdlib.h" //--vluzacn

#if _MSC_VER >= 1000
#pragma once
#endif

#define	MAX_THREADS	64

typedef enum
{
    eThreadPriorityLow = -1,
    eThreadPriorityNormal,
    eThreadPriorityHigh
}
q_threadpriority;

typedef void    (*q_threadfunction) (int);

#ifdef SYSTEM_WIN32
#define DEFAULT_NUMTHREADS -1
#endif
#ifdef SYSTEM_POSIX
#define DEFAULT_NUMTHREADS 1
#endif

#define DEFAULT_THREAD_PRIORITY eThreadPriorityNormal

extern int      g_numthreads;
extern q_threadpriority g_threadpriority;

extern void     ThreadSetPriority(q_threadpriority type);
extern void     ThreadSetDefault();
extern int      GetThreadWork();
extern void     ThreadLock();
extern void     ThreadUnlock();

extern void     RunThreadsOnIndividual(int workcnt, bool showpacifier, q_threadfunction);
extern void     RunThreadsOn(int workcnt, bool showpacifier, q_threadfunction);

#ifdef ZHLT_NETVIS
extern void     threads_InitCrit();
extern void     threads_UninitCrit();
#endif

#ifdef ZHLT_LANGFILE
#define NamedRunThreadsOn(n,p,f) { Log("%s\n", Localize(#f ":")); RunThreadsOn(n,p,f); }
#define NamedRunThreadsOnIndividual(n,p,f) { Log("%s\n", Localize(#f ":")); RunThreadsOnIndividual(n,p,f); }
#else
#define NamedRunThreadsOn(n,p,f) { Log("%s\n", #f ":"); RunThreadsOn(n,p,f); }
#define NamedRunThreadsOnIndividual(n,p,f) { Log("%s\n", #f ":"); RunThreadsOnIndividual(n,p,f); }
#endif

#endif //**/ THREADS_H__

