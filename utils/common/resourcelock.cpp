#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "cmdlib.h"
#include "messages.h"
#include "log.h"
#include "blockmem.h"

#include "resourcelock.h"

#ifdef SYSTEM_WIN32

typedef struct
{
    HANDLE          Mutex;
}
ResourceLock_t;

void*           CreateResourceLock(int LockNumber)
{
    char            lockname[_MAX_PATH];
    char            mapname[_MAX_PATH];
    ResourceLock_t* lock = (ResourceLock_t*)Alloc(sizeof(ResourceLock_t));

    ExtractFile(g_Mapname, mapname);
    safe_snprintf(lockname, _MAX_PATH, "%d%s", LockNumber, mapname);

    lock->Mutex = CreateMutex(NULL, FALSE, lockname);

    if (lock->Mutex == NULL)
    {
        LPVOID          lpMsgBuf;

        Log("lock->Mutex is NULL! [%s]", lockname);
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) & lpMsgBuf, 0, NULL);
        Error((LPCTSTR)lpMsgBuf);
    }

    WaitForSingleObject(lock->Mutex, INFINITE);

    return lock;
}

void            ReleaseResourceLock(void** lock)
{
    ResourceLock_t* lockTmp = (ResourceLock_t*)*lock;

    if (!ReleaseMutex(lockTmp->Mutex))
    {
        Error("Failed to release mutex");
    }
    Free(lockTmp);
    *lock = NULL;
}

#endif

