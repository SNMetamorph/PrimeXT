#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <malloc.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cmdlib.h"
#include "messages.h"
#include "log.h"
#include "threads.h"
#include "blockmem.h"

#ifdef SYSTEM_POSIX
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#include <pthread.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#endif

#include "hlassert.h"

q_threadpriority g_threadpriority = DEFAULT_THREAD_PRIORITY;

#define THREADTIMES_SIZE 100
#define THREADTIMES_SIZEf (float)(THREADTIMES_SIZE)

static int      dispatch = 0;
static int      workcount = 0;
static int      oldf = 0;
static bool     pacifier = false;
static bool     threaded = false;
static double   threadstart = 0;
static double   threadtimes[THREADTIMES_SIZE];

int             GetThreadWork()
{
    int             r, f, i;
    double          ct, finish, finish2, finish3;
#ifdef ZHLT_LANGFILE
	static const char *s1 = NULL; // avoid frequent call of Localize() in PrintConsole
	static const char *s2 = NULL;
#endif

    ThreadLock();
#ifdef ZHLT_LANGFILE
	if (s1 == NULL)
		s1 = Localize ("  (%d%%: est. time to completion %ld/%ld/%ld secs)   ");
	if (s2 == NULL)
		s2 = Localize ("  (%d%%: est. time to completion <1 sec)   ");
#endif

    if (dispatch == 0)
    {
        oldf = 0;
    }

    if (dispatch > workcount)
    {
        Developer(DEVELOPER_LEVEL_ERROR, "dispatch > workcount!!!\n");
        ThreadUnlock();
        return -1;
    }
    if (dispatch == workcount)
    {
        Developer(DEVELOPER_LEVEL_MESSAGE, "dispatch == workcount, work is complete\n");
        ThreadUnlock();
        return -1;
    }
    if (dispatch < 0)
    {
        Developer(DEVELOPER_LEVEL_ERROR, "negative dispatch!!!\n");
        ThreadUnlock();
        return -1;
    }

    f = THREADTIMES_SIZE * dispatch / workcount;
    if (pacifier)
    {
#ifdef ZHLT_CONSOLE
		PrintConsole(
#else
        fprintf(stdout,
#endif
			"\r%6d /%6d", dispatch, workcount);
#ifdef ZHLT_PROGRESSFILE // AJM
        if (g_progressfile)
        {
            

        }
#endif

        if (f != oldf)
        {
            ct = I_FloatTime();
            /* Fill in current time for threadtimes record */
            for (i = oldf; i <= f; i++)
            {
                if (threadtimes[i] < 1)
                {
                    threadtimes[i] = ct;
                }
            }
            oldf = f;

            if (f > 10)
            {
                finish = (ct - threadtimes[0]) * (THREADTIMES_SIZEf - f) / f;
                finish2 = 10.0 * (ct - threadtimes[f - 10]) * (THREADTIMES_SIZEf - f) / THREADTIMES_SIZEf;
                finish3 = THREADTIMES_SIZEf * (ct - threadtimes[f - 1]) * (THREADTIMES_SIZEf - f) / THREADTIMES_SIZEf;

                if (finish > 1.0)
                {
#ifdef ZHLT_CONSOLE
					PrintConsole(
#else
                    fprintf(stdout,
#endif
#ifdef ZHLT_LANGFILE
						s1, f, (long)(finish), (long)(finish2),
                           (long)(finish3));
#else
						"  (%d%%: est. time to completion %ld/%ld/%ld secs)   ", f, (long)(finish), (long)(finish2),
                           (long)(finish3));
#endif
#ifdef ZHLT_PROGRESSFILE // AJM
                    if (g_progressfile)
                    {


                    }
#endif
                }
                else
                {
#ifdef ZHLT_CONSOLE
					PrintConsole(
#else
                    fprintf(stdout,
#endif
#ifdef ZHLT_LANGFILE
						s2, f);
#else
						"  (%d%%: est. time to completion <1 sec)   ", f);
#endif
#ifndef ZHLT_CONSOLE
        fflush(stdout);
#endif
#ifdef ZHLT_PROGRESSFILE // AJM
                    if (g_progressfile)
                    {


                    }
#endif
                }
            }
        }
    }
    else
    {
        if (f != oldf)
        {
            oldf = f;
            switch (f)
            {
            case 10:
            case 20:
            case 30:
            case 40:
            case 50:
            case 60:
            case 70:
            case 80:
            case 90:
            case 100:
/*
            case 5:
            case 15:
            case 25:
            case 35:
            case 45:
            case 55:
            case 65:
            case 75:
            case 85:
            case 95:
*/
#ifdef ZHLT_CONSOLE
				PrintConsole(
#else
                fprintf(stdout,
#endif
					"%d%%...", f);
#ifndef ZHLT_CONSOLE
        fflush(stdout);
#endif
            default:
                break;
            }
        }
    }

    r = dispatch;
    dispatch++;

    ThreadUnlock();
    return r;
}

q_threadfunction workfunction;

#ifdef SYSTEM_WIN32
#pragma warning(push)
#pragma warning(disable: 4100)                             // unreferenced formal parameter
#endif
static void     ThreadWorkerFunction(int unused)
{
    int             work;

    while ((work = GetThreadWork()) != -1)
    {
        workfunction(work);
    }
}

#ifdef SYSTEM_WIN32
#pragma warning(pop)
#endif

void            RunThreadsOnIndividual(int workcnt, bool showpacifier, q_threadfunction func)
{
    workfunction = func;
    RunThreadsOn(workcnt, showpacifier, ThreadWorkerFunction);
}

#ifndef SINGLE_THREADED

/*====================
| Begin SYSTEM_WIN32
=*/
#ifdef SYSTEM_WIN32

#define	USED
#include <windows.h>

int             g_numthreads = DEFAULT_NUMTHREADS;
static CRITICAL_SECTION crit;
static int      enter;

void            ThreadSetPriority(q_threadpriority type)
{
    int             val;

    g_threadpriority = type;

    switch (g_threadpriority)
    {
    case eThreadPriorityLow:
        val = IDLE_PRIORITY_CLASS;
        break;

    case eThreadPriorityHigh:
        val = HIGH_PRIORITY_CLASS;
        break;

    case eThreadPriorityNormal:
    default:
        val = NORMAL_PRIORITY_CLASS;
        break;
    }

    SetPriorityClass(GetCurrentProcess(), val);
}

#if 0
static void     AdjustPriority(HANDLE hThread)
{
    int             val;

    switch (g_threadpriority)
    {
    case eThreadPriorityLow:
        val = THREAD_PRIORITY_HIGHEST;
        break;

    case eThreadPriorityHigh:
        val = THREAD_PRIORITY_LOWEST;
        break;

    case eThreadPriorityNormal:
    default:
        val = THREAD_PRIORITY_NORMAL;
        break;
    }
    SetThreadPriority(hThread, val);
}
#endif

void            ThreadSetDefault()
{
    SYSTEM_INFO     info;

    if (g_numthreads == -1)                                // not set manually
    {
        GetSystemInfo(&info);
        g_numthreads = info.dwNumberOfProcessors;
        if (g_numthreads < 1 || g_numthreads > 32)
        {
            g_numthreads = 1;
        }
    }
}

void            ThreadLock()
{
    if (!threaded)
    {
        return;
    }
    EnterCriticalSection(&crit);
    if (enter)
    {
        Warning("Recursive ThreadLock\n");
    }
    enter++;
}

void            ThreadUnlock()
{
    if (!threaded)
    {
        return;
    }
    if (!enter)
    {
        Error("ThreadUnlock without lock\n");
    }
    enter--;
    LeaveCriticalSection(&crit);
}

q_threadfunction q_entry;

static DWORD WINAPI ThreadEntryStub(LPVOID pParam)
{
    q_entry((int)pParam);
    return 0;
}

void            threads_InitCrit()
{
    InitializeCriticalSection(&crit);
    threaded = true;
}

void            threads_UninitCrit()
{
    DeleteCriticalSection(&crit);
}

void            RunThreadsOn(int workcnt, bool showpacifier, q_threadfunction func)
{
    DWORD           threadid[MAX_THREADS];
    HANDLE          threadhandle[MAX_THREADS];
    int             i;
    double          start, end;

    threadstart = I_FloatTime();
    start = threadstart;
    for (i = 0; i < THREADTIMES_SIZE; i++)
    {
        threadtimes[i] = 0;
    }
    dispatch = 0;
    workcount = workcnt;
    oldf = -1;
    pacifier = showpacifier;
    threaded = true;
    q_entry = func;

    if (workcount < dispatch)
    {
        Developer(DEVELOPER_LEVEL_ERROR, "RunThreadsOn: Workcount(%i) < dispatch(%i)\n", workcount, dispatch);
    }
    hlassume(workcount >= dispatch, assume_BadWorkcount);

    //
    // Create all the threads (suspended)
    //
    threads_InitCrit();
    for (i = 0; i < g_numthreads; i++)
    {
        HANDLE          hThread = CreateThread(NULL,
                                               0,
                                               (LPTHREAD_START_ROUTINE) ThreadEntryStub,
                                               (LPVOID) i,
                                               CREATE_SUSPENDED,
                                               &threadid[i]);

        if (hThread != NULL)
        {
            threadhandle[i] = hThread;
        }
        else
        {
            LPVOID          lpMsgBuf;

            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),       // Default language
                          (LPTSTR) & lpMsgBuf, 0, NULL);
            // Process any inserts in lpMsgBuf.
            // ...
            // Display the string.
            Developer(DEVELOPER_LEVEL_ERROR, "CreateThread #%d [%08X] failed : %s\n", i, threadhandle[i], lpMsgBuf);
            Fatal(assume_THREAD_ERROR, "Unable to create thread #%d", i);
            // Free the buffer.
            LocalFree(lpMsgBuf);
        }
    }
    CheckFatal();

    // Start all the threads
    for (i = 0; i < g_numthreads; i++)
    {
        if (ResumeThread(threadhandle[i]) == 0xFFFFFFFF)
        {
            LPVOID          lpMsgBuf;

            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),       // Default language
                          (LPTSTR) & lpMsgBuf, 0, NULL);
            // Process any inserts in lpMsgBuf.
            // ...
            // Display the string.
            Developer(DEVELOPER_LEVEL_ERROR, "ResumeThread #%d [%08X] failed : %s\n", i, threadhandle[i], lpMsgBuf);
            Fatal(assume_THREAD_ERROR, "Unable to start thread #%d", i);
            // Free the buffer.
            LocalFree(lpMsgBuf);
        }
    }
    CheckFatal();

    // Wait for threads to complete
    for (i = 0; i < g_numthreads; i++)
    {
        Developer(DEVELOPER_LEVEL_MESSAGE, "WaitForSingleObject on thread #%d [%08X]\n", i, threadhandle[i]);
        WaitForSingleObject(threadhandle[i], INFINITE);
    }
    threads_UninitCrit();

    q_entry = NULL;
    threaded = false;
    end = I_FloatTime();
    if (pacifier)
    {
#ifdef ZHLT_CONSOLE
		PrintConsole
#else
        printf
#endif
			("\r%60s\r", "");
    }
    Log(" (%.2f seconds)\n", end - start);
}

#endif

/*=
| End SYSTEM_WIN32
=====================*/

/*====================
| Begin SYSTEM_POSIX
=*/
#ifdef SYSTEM_POSIX

#define	USED

int             g_numthreads = DEFAULT_NUMTHREADS;

void            ThreadSetPriority(q_threadpriority type)
{
    int             val;

    g_threadpriority = type;

    // Currently in Linux land users are incapable of raising the priority level of their processes
    // Unless you are root -high is useless . . . 
    switch (g_threadpriority)
    {
    case eThreadPriorityLow:
        val = PRIO_MAX;
        break;

    case eThreadPriorityHigh:
        val = PRIO_MIN;
        break;

    case eThreadPriorityNormal:
    default:
        val = 0;
        break;
    }
    setpriority(PRIO_PROCESS, 0, val);
}

void            ThreadSetDefault()
{
    if (g_numthreads == -1)
    {
        g_numthreads = 1;
    }
}

typedef void*    pthread_addr_t;
pthread_mutex_t* my_mutex;

void            ThreadLock()
{
    if (my_mutex)
    {
        pthread_mutex_lock(my_mutex);
    }
}

void            ThreadUnlock()
{
    if (my_mutex)
    {
        pthread_mutex_unlock(my_mutex);
    }
}

q_threadfunction q_entry;

static void*    CDECL ThreadEntryStub(void* pParam)
{
#ifdef ZHLT_64BIT_FIX
    q_entry((int)(intptr_t)pParam);
#else
    q_entry((int)pParam);
#endif
    return NULL;
}

void            threads_InitCrit()
{
    pthread_mutexattr_t mattrib;

    if (!my_mutex)
    {
        my_mutex = (pthread_mutex_t*)Alloc(sizeof(*my_mutex));
        if (pthread_mutexattr_init(&mattrib) == -1)
        {
            Error("pthread_mutex_attr_init failed");
        }
        if (pthread_mutex_init(my_mutex, &mattrib) == -1)
        {
            Error("pthread_mutex_init failed");
        }
    }
}

void            threads_UninitCrit()
{
    Free(my_mutex);
    my_mutex = NULL;
}

/*
 * =============
 * RunThreadsOn
 * =============
 */
void            RunThreadsOn(int workcnt, bool showpacifier, q_threadfunction func)
{
    int             i;
    pthread_t       work_threads[MAX_THREADS];
    pthread_addr_t  status;
    pthread_attr_t  attrib;
    double          start, end;

    threadstart = I_FloatTime();
    start = threadstart;
    for (i = 0; i < THREADTIMES_SIZE; i++)
    {
        threadtimes[i] = 0;
    }

    dispatch = 0;
    workcount = workcnt;
    oldf = -1;
    pacifier = showpacifier;
    threaded = true;
    q_entry = func;

    if (pacifier)
    {
        setbuf(stdout, NULL);
    }

    threads_InitCrit();

    if (pthread_attr_init(&attrib) == -1)
    {
        Error("pthread_attr_init failed");
    }
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
    if (pthread_attr_setstacksize(&attrib, 0x400000) == -1)
    {
        Error("pthread_attr_setstacksize failed");
    }
#endif

    for (i = 0; i < g_numthreads; i++)
    {
        if (pthread_create(&work_threads[i], &attrib, ThreadEntryStub, (void*)i) == -1)
        {
            Error("pthread_create failed");
        }
    }

    for (i = 0; i < g_numthreads; i++)
    {
        if (pthread_join(work_threads[i], &status) == -1)
        {
            Error("pthread_join failed");
        }
    }

    threads_UninitCrit();

    q_entry = NULL;
    threaded = false;

    end = I_FloatTime();
    if (pacifier)
    {
#ifdef ZHLT_CONSOLE
		PrintConsole
#else
        printf
#endif
			("\r%60s\r", "");
    }

    Log(" (%.2f seconds)\n", end - start);
}

#endif /*SYSTEM_POSIX */

/*=
| End SYSTEM_POSIX
=====================*/

#endif /*SINGLE_THREADED */

/*====================
| Begin SINGLE_THREADED
=*/
#ifdef SINGLE_THREADED

int             g_numthreads = 1;

void            ThreadSetPriority(q_threadpriority type)
{
}

void            threads_InitCrit()
{
}

void            threads_UninitCrit()
{
}

void            ThreadSetDefault()
{
    g_numthreads = 1;
}

void            ThreadLock()
{
}

void            ThreadUnlock()
{
}

void            RunThreadsOn(int workcnt, bool showpacifier, q_threadfunction func)
{
    int             i;
    double          start, end;

    dispatch = 0;
    workcount = workcnt;
    oldf = -1;
    pacifier = showpacifier;
    threadstart = I_FloatTime();
    start = threadstart;
    for (i = 0; i < THREADTIMES_SIZE; i++)
    {
        threadtimes[i] = 0.0;
    }

    if (pacifier)
    {
        setbuf(stdout, NULL);
    }
    func(0);

    end = I_FloatTime();

    if (pacifier)
    {
#ifdef ZHLT_CONSOLE
		PrintConsole
#else
        printf
#endif
			("\r%60s\r", "");
    }

    Log(" (%.2f seconds)\n", end - start);
}

#endif

/*=
| End SINGLE_THREADED
=====================*/

