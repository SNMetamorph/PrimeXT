//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxSettings.cpp
// implementation: Qt Free Edition
// last modified:  Sep 05 2019, Andrey Akhmichin
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxSettings.h>
#include <QSettings>

static QSettings *d_settings;



void
mx_init_usersettings (const char *org, const char *appName)
{
	if (!d_settings)
		d_settings = new QSettings (org, appName);
}



int
mx_create_usersettings ()
{
	return 1;
}



int
mx_set_usersettings_string (const char *lpValueName, const char *lpData)
{
	d_settings->setValue (lpValueName, lpData);
	d_settings->sync ();

	return 1;
}



int
mx_get_usersettings_string (const char *lpValueName, char *lpData)
{
	strcpy (lpData, qPrintable (d_settings->value (lpValueName, "").toString()));

	return 1;
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

