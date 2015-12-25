
#include "csg.h"

#ifdef HLCSG_GAMETEXTMESSAGE_UTF8
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

char * ANSItoUTF8 (const char *string)
{
	int len;
	char *utf8;
	wchar_t *unicode;
	len = MultiByteToWideChar (CP_ACP, 0, string, -1, NULL, 0);
	unicode = (wchar_t *)calloc (len+1, sizeof(wchar_t));
	MultiByteToWideChar (CP_ACP, 0, string, -1, unicode, len);
	len = WideCharToMultiByte (CP_UTF8, 0, unicode, -1, NULL, 0, NULL, NULL);
	utf8 = (char *)calloc (len+1, sizeof(char));
	WideCharToMultiByte (CP_UTF8, 0, unicode, -1, utf8, len, NULL, NULL);
	free (unicode);
	return utf8;
}
#endif

