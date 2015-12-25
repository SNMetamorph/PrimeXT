#ifdef SYSTEM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "cmdlib.h"
#include "messages.h"
#include "hlassert.h"
#include "blockmem.h"
#include "log.h"
#include "mathlib.h"

#ifdef SYSTEM_POSIX
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#define PATHSEPARATOR(c) ((c) == '\\' || (c) == '/')

/*
 * ================
 * I_FloatTime
 * ================
 */

double          I_FloatTime()
{
#ifdef SYSTEM_WIN32
    FILETIME        ftime;
    double rval;

    GetSystemTimeAsFileTime(&ftime);

    rval = ftime.dwLowDateTime;
    rval += ((__int64)ftime.dwHighDateTime) << 32;

    return (rval / 10000000.0);
#endif

#ifdef SYSTEM_POSIX
    struct timeval  tp;
    struct timezone tzp;
    static int      secbase;

    gettimeofday(&tp, &tzp);

    if (!secbase)
    {
        secbase = tp.tv_sec;
        return tp.tv_usec / 1000000.0;
    }

    return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
#endif
}

#ifdef SYSTEM_POSIX
char*           strupr(char* string)
{
    int             i;
    int             len = strlen(string);

    for (i = 0; i < len; i++)
    {
        string[i] = toupper(string[i]);
    }
    return string;
}

char*           strlwr(char* string)
{
    int             i;
    int             len = strlen(string);

    for (i = 0; i < len; i++)
    {
        string[i] = tolower(string[i]);
    }
    return string;
}
#endif

// Case Insensitive substring matching
const char*     stristr(const char* const string, const char* const substring)
{
    char*           string_copy;
    char*           substring_copy;
    const char*     match;

    string_copy = _strdup(string);
    _strlwr(string_copy);

    substring_copy = _strdup(substring);
    _strlwr(substring_copy);

    match = strstr(string_copy, substring_copy);
    if (match)
    {
        match = (string + (match - string_copy));
    }

    free(string_copy);
    free(substring_copy);
    return match;
}

/*--------------------------------------------------------------------
// New implementation of FlipSlashes, DefaultExtension, StripFilename, 
// StripExtension, ExtractFilePath, ExtractFile, ExtractFileBase, etc.
----------------------------------------------------------------------*/
#ifdef ZHLT_NEW_FILE_FUNCTIONS //added "const". --vluzacn

//Since all of these functions operate around either the extension 
//or the directory path, centralize getting both numbers here so we
//can just reference them everywhere else.  Use strrchr to give a
//speed boost while we're at it.
inline void getFilePositions(const char* path, int* extension_position, int* directory_position)
{
	const char* ptr = strrchr(path,'.');
	if(ptr == 0)
	{ *extension_position = -1; }
	else
	{ *extension_position = ptr - path; }

	ptr = qmax(strrchr(path,'/'),strrchr(path,'\\'));
	if(ptr == 0)
	{ *directory_position = -1; }
	else
	{ 
		*directory_position = ptr - path;
		if(*directory_position > *extension_position)
		{ *extension_position = -1; }
		
		//cover the case where we were passed a directory - get 2nd-to-last slash
		if(*directory_position == (int)strlen(path) - 1)
		{
			do
			{
				--(*directory_position);
			}
			while(*directory_position > -1 && path[*directory_position] != '/' && path[*directory_position] != '\\');
		}
	}
}

char* FlipSlashes(char* string)
{
	char* ptr = string;
	if(SYSTEM_SLASH_CHAR == '\\')
	{
		while(ptr = strchr(ptr,'/'))
		{ *ptr = SYSTEM_SLASH_CHAR; }
	}
	else
	{
		while(ptr = strchr(ptr,'\\'))
		{ *ptr = SYSTEM_SLASH_CHAR; }
	}
	return string;
}

void DefaultExtension(char* path, const char* extension)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);
	if(extension_pos == -1)
	{ strcat(path,extension); }
}

void StripFilename(char* path)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);
	if(directory_pos == -1)
	{ path[0] = 0; }
	else
	{ path[directory_pos] = 0; }
}

void StripExtension(char* path)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);
	if(extension_pos != -1)
	{ path[extension_pos] = 0; }
}

void ExtractFilePath(const char* const path, char* dest)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);
	if(directory_pos != -1)
	{
	    memcpy(dest,path,directory_pos+1); //include directory slash
	    dest[directory_pos+1] = 0;
	}
	else
	{ dest[0] = 0; }
}

void ExtractFile(const char* const path, char* dest)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);

	int length = strlen(path);

#ifdef ZHLT_FILE_FUNCTIONS_FIX
	length -= directory_pos + 1;
#else
	if(directory_pos == -1)	{ directory_pos = 0; }
	else { length -= directory_pos + 1; }
#endif

    memcpy(dest,path+directory_pos+1,length); //exclude directory slash
    dest[length] = 0;
}

void ExtractFileBase(const char* const path, char* dest)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);
	int length = extension_pos == -1 ? strlen(path) : extension_pos;

#ifdef ZHLT_FILE_FUNCTIONS_FIX
	length -= directory_pos + 1;
#else
	if(directory_pos == -1)	{ directory_pos = 0; }
	else { length -= directory_pos + 1; }
#endif

    memcpy(dest,path+directory_pos+1,length); //exclude directory slash
    dest[length] = 0;
}

void ExtractFileExtension(const char* const path, char* dest)
{
	int extension_pos, directory_pos;
	getFilePositions(path,&extension_pos,&directory_pos);
	if(extension_pos != -1)
	{
		int length = strlen(path) - extension_pos;
	    memcpy(dest,path+extension_pos,length); //include extension '.'
	    dest[length] = 0;
	}
	else
	{ dest[0] = 0; }
}
//-------------------------------------------------------------------
#else //old cmdlib functions

char*           FlipSlashes(char* string)
{
    while (*string)
    {
        if (PATHSEPARATOR(*string))
        {
            *string = SYSTEM_SLASH_CHAR;
        }
        string++;
    }
    return string;
}

void            DefaultExtension(char* path, const char* extension)
{
    char*           src;

    //
    // if path doesn't have a .EXT, append extension
    // (extension should include the .)
    //
    src = path + strlen(path) - 1;

    while (!PATHSEPARATOR(*src) && src != path)
    {
        if (*src == '.')
            return;                                        // it has an extension
        src--;
    }

    strcat(path, extension);
}

void            StripFilename(char* path)
{
    int             length;

    length = strlen(path) - 1;
    while (length > 0 && !PATHSEPARATOR(path[length]))
        length--;
    path[length] = 0;
}

void            StripExtension(char* path)
{
    int             length;

    length = strlen(path) - 1;
    while (length > 0 && path[length] != '.')
    {
        length--;
        if (PATHSEPARATOR(path[length]))
            return;                                        // no extension
    }
    if (length)
        path[length] = 0;
}

/*
 * ====================
 * Extract file parts
 * ====================
 */
void            ExtractFilePath(const char* const path, char* dest)
{
    hlassert (path != dest);

    const char*           src;

    src = path + strlen(path) - 1;

    //
    // back up until a \ or the start
    //
    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    memcpy(dest, path, src - path);
    dest[src - path] = 0;
}

void            ExtractFile(const char* const path, char* dest)
{
    hlassert (path != dest);

    const char*           src;

    src = path + strlen(path) - 1;

    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = 0;
}

void            ExtractFileBase(const char* const path, char* dest)
{
    hlassert (path != dest);

    const char*           src;

    src = path + strlen(path) - 1;

    //
    // back up until a \ or the start
    //
    while (src != path && !PATHSEPARATOR(*(src - 1)))
        src--;

    while (*src && *src != '.')
    {
        *dest++ = *src++;
    }
    *dest = 0;
}

void            ExtractFileExtension(const char* const path, char* dest)
{
    hlassert (path != dest);

    const char*           src;

    src = path + strlen(path) - 1;

    //
    // back up until a . or the start
    //
    while (src != path && *(src - 1) != '.')
        src--;
    if (src == path)
    {
        *dest = 0;                                         // no extension
        return;
    }

    strcpy_s(dest, src);
}

#endif

/*
 * ============================================================================
 * 
 * BYTE ORDER FUNCTIONS
 * 
 * ============================================================================
 */

#ifdef WORDS_BIGENDIAN

short           LittleShort(const short l)
{
    byte            b1, b2;

    b1 = l & 255;
    b2 = (l >> 8) & 255;

    return (b1 << 8) + b2;
}

short           BigShort(const short l)
{
    return l;
}

int             LittleLong(const int l)
{
    byte            b1, b2, b3, b4;

    b1 = l & 255;
    b2 = (l >> 8) & 255;
    b3 = (l >> 16) & 255;
    b4 = (l >> 24) & 255;

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int             BigLong(const int l)
{
    return l;
}

float           LittleFloat(const float l)
{
    union
    {
        byte            b[4];
        float           f;
    }
    in             , out;

    in.f = l;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];

    return out.f;
}

float           BigFloat(const float l)
{
    return l;
}

#else // Little endian (Intel, etc)

short           BigShort(const short l)
{
    byte            b1, b2;

    b1 = (byte) (l & 255);
    b2 = (byte) ((l >> 8) & 255);

    return (short)((b1 << 8) + b2);
}

short           LittleShort(const short l)
{
    return l;
}

int             BigLong(const int l)
{
    byte            b1, b2, b3, b4;

    b1 = (byte) (l & 255);
    b2 = (byte) ((l >> 8) & 255);
    b3 = (byte) ((l >> 16) & 255);
    b4 = (byte) ((l >> 24) & 255);

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int             LittleLong(const int l)
{
    return l;
}

float           BigFloat(const float l)
{
    union
    {
        byte            b[4];
        float           f;
    }
    in             , out;

    in.f = l;
    out.b[0] = in.b[3];
    out.b[1] = in.b[2];
    out.b[2] = in.b[1];
    out.b[3] = in.b[0];

    return out.f;
}

float           LittleFloat(const float l)
{
    return l;
}

#endif

//=============================================================================

bool CDECL FORMAT_PRINTF(3,4)      safe_snprintf(char* const dest, const size_t count, const char* const args, ...)
{
    size_t          amt;
    va_list         argptr;

    hlassert(count > 0);

    va_start(argptr, args);
    amt = vsnprintf(dest, count, args, argptr);
    va_end(argptr);

    // truncated (bad!, snprintf doesn't null terminate the string when this happens)
    if (amt == count)
    {
        dest[count - 1] = 0;
        return false;
    }

    return true;
}

bool            safe_strncpy(char* const dest, const char* const src, const size_t count)
{
    return safe_snprintf(dest, count, "%s", src);
}

bool            safe_strncat(char* const dest, const char* const src, const size_t count)
{
    if (count)
    {
        strncat(dest, src, count);

        dest[count - 1] = 0;                               // Ensure it is null terminated
        return true;
    }
    else
    {
        Warning("safe_strncat passed empty count");
        return false;
    }
}

bool            TerminatedString(const char* buffer, const int size)
{
    int             x;

    for (x = 0; x < size; x++, buffer++)
    {
        if ((*buffer) == 0)
        {
            return true;
        }
    }
    return false;
}

