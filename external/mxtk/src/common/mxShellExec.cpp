//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxShellExec.cpp
// implementation: all
// last modified:  May 26 2019, Andrey Akhmichin
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mx/mxWindow.h"
#include "mx/mxShellExec.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif



void
mx_shellexec (mxWindow *parent, const char *path)
{
	const char *openProgram;
#if defined(_WIN32)
	openProgram = "open";

	ShellExecute ((HWND) (parent ? parent->getHandle () : 0), openProgram, path, 0, 0, SW_SHOW);
#else
	openProgram = "xdg-open";

	if (!fork())
		execlp(openProgram, openProgram, path, NULL);
#endif
}
