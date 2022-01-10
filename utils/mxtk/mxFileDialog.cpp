//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxFileDialog.cpp
// implementation: Win32 API
// last modified:  Mar 14 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mxFileDialog.h>
#include <mxWindow.h>
#include <windows.h>
#include <commdlg.h>
#include <string.h>

static char sd_open_path[_MAX_PATH] = "";
static char sd_save_path[_MAX_PATH] = "";

const char *mxGetOpenFileName( mxWindow *parent, const char *path, const char *filter )
{
	CHAR szPath[_MAX_PATH], szFilter[_MAX_PATH];

	strcpy (sd_open_path, "");

	if (path)
		strcpy (szPath, path);
	else
		strcpy (szPath, "");

	if (filter)
	{
		memset (szFilter, 0, _MAX_PATH);
		strcpy (szFilter, filter);
		strcpy (szFilter + strlen (szFilter) + 1, filter);
	}
	else
		strcpy (szFilter, "");


	OPENFILENAME ofn;
	memset (&ofn, 0, sizeof (ofn));
	ofn.lStructSize = sizeof (ofn);
	if (parent)
		ofn.hwndOwner = (HWND) parent->getHandle ();
	ofn.hInstance = (HINSTANCE) GetModuleHandle (NULL);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = sd_open_path;
	ofn.nMaxFile = _MAX_PATH;
	if (path && strlen (path))
		ofn.lpstrInitialDir = szPath;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if( GetOpenFileName( &ofn ))
		return sd_open_path;
	return 0;
}

const char *mxGetSaveFileName (mxWindow *parent, const char *path, const char *filter, const char *filepath )
{
	CHAR szPath[_MAX_PATH], szFilter[_MAX_PATH];

	if( filepath )
		strcpy (sd_save_path, filepath );
	else strcpy (sd_save_path, "");

	if (path)
		strcpy (szPath, path);
	else
		strcpy (szPath, "");

	if (filter)
	{
		memset (szFilter, 0, _MAX_PATH);
		strcpy (szFilter, filter);
		strcpy (szFilter + strlen (szFilter) + 1, filter);
	}
	else
		strcpy (szFilter, "");

	OPENFILENAME ofn;
	memset (&ofn, 0, sizeof (ofn));
	ofn.lStructSize = sizeof (ofn);
	if (parent)
		ofn.hwndOwner = (HWND) parent->getHandle ();
	ofn.hInstance = (HINSTANCE) GetModuleHandle (NULL);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = sd_save_path;
	ofn.nMaxFile = _MAX_PATH;
	if (path && strlen (path))
		ofn.lpstrInitialDir = szPath;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if( GetSaveFileName( &ofn ))
		return sd_save_path;
	return 0;
}