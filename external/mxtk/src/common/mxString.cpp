//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxstring.cpp
// implementation: all
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxString.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <shlwapi.h>
#endif


int
mx_strncasecmp (const char *s1, const char *s2, int count)
{
#ifdef WIN32
	return _strnicmp (s1, s2, count);
#else
	return strncasecmp (s1, s2, count);
#endif
}



int
mx_strcasecmp (const char *s1, const char *s2)
{
#ifdef WIN32
	return _stricmp (s1, s2);
#else
	return strcasecmp (s1, s2);
#endif
}



char *
mx_strlower (char *str)
{
	int i;
	for (i = strlen (str) - 1; i >= 0; i--)
		str[i] = tolower (str[i]);

	return str;
}



#ifdef _WIN32
int
mx_snprintf(char *buffer, int buffersize, const char *format, ...)
{
	va_list	args;
	int	result;

	va_start(args, format);
	result = wvnsprintfA(buffer, buffersize, format, args);
	va_end(args);

	if (result < 0 || result >= buffersize)
	{
		buffer[buffersize - 1] = '\0';
		return -1;
	}

	return result;
}
#endif



char *
mx_stristr(const char *string, const char *string2)
{
	int	c, len;

	if (!string || !string2)
		return NULL;

	c = tolower(*string2);
	len = strlen(string2);

	while (string)
	{
		for (; *string && tolower(*string) != c; string++);

		if (*string)
		{
			if (!mx_strncasecmp(string, string2, len))
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
}

