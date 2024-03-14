//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxSound.cpp
// implementation: all
// last modified:  May 20 2019, Andrey Akhmichin
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxSound.h>
#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#endif



void
mx_playsnd (const char *sound)
{
#if defined(_WIN32)
	PlaySoundA (sound, 0, SND_FILENAME | SND_ASYNC);
#endif
}



void
mx_stopsnd ()
{
#if defined(_WIN32)
      PlaySoundA (0, 0, SND_FILENAME | SND_ASYNC);
#endif
}
