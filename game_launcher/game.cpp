/*
game.cpp -- executable to run Xash3D Engine
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"
#include "build.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#if XASH_WIN32 == 1
#include <shellapi.h> // CommandLineToArgvW
#endif

#define GAMEDIR	"primext"
#define DISABLE_MENU_CHANGEGAME
#define ENGINE_LIBRARY OS_LIB_PREFIX "xash." OS_LIB_EXT

#if XASH_WIN32 == 1
extern "C"
{
	// Enable NVIDIA High Performance Graphics while using Integrated Graphics.
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

	// Enable AMD High Performance Graphics while using Integrated Graphics.
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

typedef void (*pfnChangeGame_t)(const char *progname);
typedef int  (*pfnInit_t)(int argc, char **argv, const char *progname, int bChangeGame, pfnChangeGame_t func);
typedef void (*pfnShutdown_t)(void);

static pfnInit_t     pfnMain = nullptr;
static pfnShutdown_t pfnShutdown = nullptr;
static char          szGameDir[128]; // safe place to keep gamedir
static int           szArgc;
static char          **szArgv;
static HINSTANCE     hEngine;

static void ReportError(const char *szFmt, ...)
{
	static char	buffer[16384];	// must support > 1k messages
	va_list		args;

	va_start(args, szFmt);
	vsnprintf(buffer, sizeof(buffer), szFmt, args);
	va_end(args);

#if XASH_WIN32 == 1
	MessageBoxA(NULL, buffer, "Launcher Error", MB_OK);
#else
	fprintf(stderr, "Launcher Error: %s\n", buffer);
#endif
	exit(1);
}

static const char *GetStringLastError()
{
#if XASH_WIN32 == 1
	static char buf[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		buf, sizeof(buf), NULL);
	return buf;
#else
	return dlerror();
#endif
}

static void Sys_LoadEngine()
{
	hEngine = LoadLibrary(ENGINE_LIBRARY);
	if (!hEngine) {
		ReportError("Unable to load the " ENGINE_LIBRARY ": %s", GetStringLastError());
		return;
	}

	pfnMain = (pfnInit_t)GetProcAddress(hEngine, "Host_Main");
	if (!pfnMain) {
		ReportError(ENGINE_LIBRARY " missed 'Host_Main' export: %s", GetStringLastError());
		return;
	}

	// this is non-fatal for us but change game will not working
	pfnShutdown = (pfnShutdown_t)GetProcAddress(hEngine, "Host_Shutdown");
}

static void Sys_UnloadEngine()
{
	if (pfnShutdown) 
		pfnShutdown();
	if (hEngine) 
		FreeLibrary(hEngine);

	pfnMain = nullptr;
	pfnShutdown = nullptr;
}

static void Sys_ChangeGame(const char *progname)
{
	if (!progname || !progname[0]) {
		ReportError("Sys_ChangeGame: NULL gamedir");
		return;
	}

	if (pfnShutdown == NULL) {
		ReportError("Sys_ChangeGame: missed 'Host_Shutdown' export\n");
		return;
	}

	strncpy(szGameDir, progname, sizeof(szGameDir) - 1);
	Sys_UnloadEngine();
	Sys_LoadEngine();
	pfnMain(szArgc, szArgv, szGameDir, 1, Sys_ChangeGame);
}

int Sys_Start(void)
{
	pfnChangeGame_t pfnChangeGame = NULL;

#ifndef DISABLE_MENU_CHANGEGAME
	if (pfnShutdown)
		pfnChangeGame = Sys_ChangeGame;
#endif

	Sys_LoadEngine();
	int returnCode = pfnMain(szArgc, szArgv, GAMEDIR, 0, pfnChangeGame);
	Sys_UnloadEngine();
	return returnCode;
}

#if XASH_WIN32 == 1
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nShow)
{
	LPWSTR *lpArgv;
	int returnCode;

	lpArgv = CommandLineToArgvW(GetCommandLineW(), &szArgc);
	szArgv = (char **)malloc((szArgc + 1) * sizeof(char*));

	// converting wide string argv to utf-8
	for (int i = 0; i < szArgc; ++i)
	{
		int stringSize = wcstombs(nullptr, lpArgv[i], 9999) + 1;
		szArgv[i] = (char *)malloc(stringSize);
		wcstombs(szArgv[i], lpArgv[i], stringSize);
	}

	szArgv[szArgc] = nullptr;
	LocalFree(lpArgv);
	returnCode = Sys_Start();

	for (int i = 0; i < szArgc; ++i) {
		free(szArgv[i]);
	}
	free(szArgv);

	return returnCode;
}
#else
int main(int argc, char **argv)
{
	szArgc = argc;
	szArgv = argv;

	return Sys_Start();
}
#endif
