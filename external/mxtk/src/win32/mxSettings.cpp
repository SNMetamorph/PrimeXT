//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxSettings.cpp
// implementation: Win32 API
// last modified:  Sep 05 2019, Andrey Akhmichin
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxSettings.h>
#include <windows.h>

static char subKey[1024];



void
mx_init_usersettings (const char *org, const char *appName)
{
	sprintf (subKey, "Software\\%s\\%s", org, appName);
}



static int
mx_create_settings (HKEY hKey, LPCSTR lpSubKey)
{
	HKEY phkResult = 0;

	if (RegCreateKeyExA (hKey, lpSubKey, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &phkResult, (LPDWORD)&lpSubKey))
		return 0;

	RegCloseKey (hKey);

	return 1;
}



static int
mx_get_settings (HKEY hKey, LPCSTR lpSubKey, LPCSTR lpValueName, LPBYTE lpData)
{
	DWORD cbData = 256;

	if (RegOpenKeyExA (hKey, lpSubKey, 0, KEY_EXECUTE, &hKey))
		return 0;

	if (RegQueryValueExA (hKey, lpValueName, 0, REG_NONE, lpData, &cbData))
		return 0;

	RegCloseKey (hKey);

	return 1;
}



static int
mx_set_settings (HKEY hKey, LPCSTR lpSubKey, LPCSTR lpValueName, LPBYTE lpData)
{
	if (RegOpenKeyExA (hKey, lpSubKey, 0, KEY_WRITE, &hKey))
		return 0;

	if (RegSetValueExA (hKey, lpValueName, 0, REG_SZ, lpData, MAX_USERSETTINGS_ENTRY_SIZE))
		return 0;

	RegCloseKey (hKey);

	return 1;
}



int
mx_create_usersettings ()
{
	return mx_create_settings (HKEY_CURRENT_USER, subKey);
}



int
mx_get_usersettings_string (const char *lpValueName, char *lpData)
{
	return mx_get_settings (HKEY_CURRENT_USER, subKey, lpValueName, (LPBYTE)lpData);
}



int
mx_set_usersettings_string (const char *lpValueName, const char *lpData)
{
	return mx_set_settings (HKEY_CURRENT_USER, subKey, lpValueName, (LPBYTE)lpData);
}



int
mx_set_usersettings_vec4d (const char *lpValueName, float *data)
{
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	snprintf (buf, sizeof buf, "%g %g %g %g", data[0], data[1], data[2], data[3]);
	return mx_set_usersettings_string (lpValueName, buf);
}



int
mx_get_usersettings_vec4d (const char *lpValueName, float *data)
{
	int ret;
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	ret = mx_get_usersettings_string (lpValueName, buf);
	if (ret)
		ret = sscanf (buf, "%g %g %g %g", &data[0], &data[1], &data[2], &data[3]);

	return ret;
}



int
mx_set_usersettings_vec3d (const char *lpValueName, float *data)
{
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	snprintf (buf, sizeof buf, "%g %g %g", data[0], data[1], data[2]);
	return mx_set_usersettings_string (lpValueName, buf);
}



int
mx_get_usersettings_vec3d (const char *lpValueName, float *data)
{
	int ret;
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	ret = mx_get_usersettings_string (lpValueName, buf);
	if (ret)
		ret = sscanf (buf, "%g %g %g", &data[0], &data[1], &data[2]);

	return ret;
}



int
mx_set_usersettings_float (const char *lpValueName, float data)
{
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	sprintf (buf, "%f", data);
	return mx_set_usersettings_string (lpValueName, buf);
}



int
mx_get_usersettings_float (const char *lpValueName, float *data)
{
	int ret;
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	ret = mx_get_usersettings_string (lpValueName, buf);
	if (ret)
		ret = sscanf (buf, "%f", data);

	return ret;
}



int
mx_set_usersettings_int (const char *lpValueName, int data)
{
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	sprintf (buf, "%d", data);
	return mx_set_usersettings_string (lpValueName, buf);
}



int
mx_get_usersettings_int (const char *lpValueName, int *data)
{
	int ret;
	char buf[MAX_USERSETTINGS_ENTRY_SIZE];

	ret = mx_get_usersettings_string (lpValueName, buf);
	if (ret)
		ret = sscanf (buf, "%d", data);

	return ret;
}

