/*
conprint.h - extended printf function that allows
colored printing scheme from Quake3 in Win32 console
Copyright (C) 2012 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef CONPRINT_H
#define CONPRINT_H

#define DEFAULT_DEVELOPER		D_ERROR

void Msg(const char *pMsg, ...);
void MsgDev(int level, const char *pMsg, ...);
void MsgAnim(int level, const char *pMsg, ...);
void SetDeveloperLevel(int level);
int GetDeveloperLevel(void);
void Sys_InitLog(const char *logname);
void Sys_InitLogAppend(const char *logname);
void Sys_CloseLog(void);
void Sys_PrintLog(const char *pMsg);
void Sys_IgnoreLog(bool ignore);
void Sys_Print(const char *pMsg);
void Sys_Sleep(unsigned int msec);
void Sys_WaitForKeyInput();

#endif // CONPRINT_H
