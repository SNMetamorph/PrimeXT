#ifndef FILELIB_H__
#define FILELIB_H__
#include "cmdlib.h" //--vluzacn

#if _MSC_VER >= 1000
#pragma once
#endif

extern time_t   getfiletime(const char* const filename);
extern long     getfilesize(const char* const filename);
extern long     getfiledata(const char* const filename, char* buffer, const int buffersize);
extern bool     q_exists(const char* const filename);
extern int      q_filelength(FILE* f);

extern FILE*    SafeOpenWrite(const char* const filename);
extern FILE*    SafeOpenRead(const char* const filename);
extern void     SafeRead(FILE* f, void* buffer, int count);
extern void     SafeWrite(FILE* f, const void* const buffer, int count);

extern int      LoadFile(const char* const filename, char** bufferptr);
extern void     SaveFile(const char* const filename, const void* const buffer, int count);

#endif //**/ FILELIB_H__

