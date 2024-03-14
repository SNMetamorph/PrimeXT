//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxSound.h
// implementation: all
// last modified:  May 20 2019, Andrey Akhmichin
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXSOUND
#define INCLUDED_MXSOUND



#ifdef __cplusplus
extern "C" {
#endif

void mx_playsnd (const char *sound);
void mx_stopsnd ();

#ifdef __cplusplus
}
#endif



#endif // INCLUDED_MXSOUND
