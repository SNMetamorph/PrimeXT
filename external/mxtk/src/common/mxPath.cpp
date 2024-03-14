//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxpath.cpp
// implementation: all
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxPath.h>
#ifdef WIN32
#include <windows.h>
#include <shlwapi.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



bool
mx_setcwd (const char *path)
{
#ifdef WIN32
	return (SetCurrentDirectory (path) == TRUE);
#else
	return (chdir (path) != -1);
#endif
}



const char *
mx_getcwd ()
{
	static char path[256];
#ifdef WIN32
	GetCurrentDirectory (256, path);
#else
	getcwd (path, 256);
#endif
	return path;
}



const char *
mx_getpath (const char *filename)
{
	static char path[256];
	char drive[64];
	char tmp[256];
#ifdef WIN32
	_splitpath (filename, drive, tmp, 0, 0);
	strcpy(path, drive);
	strcat(path, tmp);
#else
	strcpy (path, filename);
	char *ptr = (char *)strrchr (path, '/');
	if (ptr)
		*ptr = '\0';
#endif
	return path;
}



const char *
mx_getextension (const char *filename)
{
	static char ext[256];
#ifdef WIN32	
	_splitpath (filename, 0, 0, 0, ext);
#else
	char *ptr = (char *)strrchr (filename, '.');
	if (ptr)
		strcpy (ext, ptr);
	else
		strcpy (ext, "");
#endif
	return ext;
}



const char *
mx_getfilename (const char *path)
{
	static char filename[256];
#ifdef WIN32
	strcpy(filename, path);
	PathStripPathA(filename);
#else
	char *ptr = (char *)strrchr (path, '/');
	if (ptr)
		strcpy (filename, ptr + 1);
	else
		strcpy (filename, path);

#endif
	return filename;
}



const char *
mx_getfilebase (const char *path)
{
	static char filename[256];
#ifdef WIN32
	_splitpath (path, 0, 0, filename, 0);
#else
	char *ptr = (char *)strrchr (path, '/');
	if (ptr)
		strcpy (filename, ptr + 1);
	else
		strcpy (filename, path);

	ptr = (char *)strrchr (filename, '.');

	if (ptr)
		*ptr = '\0';

#endif
        return filename;
}



const char *
mx_gettemppath ()
{
	static char path[256];
#ifdef WIN32
	GetTempPath (256, path);
#else
	strcpy (path, "/tmp");
#endif

	return path;
}
