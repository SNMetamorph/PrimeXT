//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxSettings.h
// implementation: all
// last modified:  Sep 05 2019, Andrey Akhmichin
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXSETTINGS
#define INCLUDED_MXSETTINGS

#include <stdlib.h>
#include <stdio.h>

#define MAX_USERSETTINGS_ENTRY_SIZE	100



#ifdef __cplusplus
extern "C" {
#endif

void mx_init_usersettings (const char *org, const char *appName);

int mx_create_usersettings ();
int mx_set_usersettings_string (const char *lpValueName, const char *lpData);
int mx_get_usersettings_string (const char *lpValueName, char *lpData);
int mx_set_usersettings_vec4d (const char *lpValueName, float *data);
int mx_get_usersettings_vec4d (const char *lpValueName, float *data);
int mx_set_usersettings_vec3d (const char *lpValueName, float *data);
int mx_get_usersettings_vec3d (const char *lpValueName, float *data);
int mx_set_usersettings_float (const char *lpValueName, float data);
int mx_get_usersettings_float (const char *lpValueName, float *data);
int mx_set_usersettings_int (const char *lpValueName, int data);
int mx_get_usersettings_int (const char *lpValueName, int *data);

#ifdef __cplusplus
}
#endif



#endif // INCLUDED_MXSETTINGS

