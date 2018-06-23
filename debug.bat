@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=debug
set BUILD_ERROR=
call vcvars32

%MSDEV% client/client.dsp %CONFIG%"client - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% server/server.dsp %CONFIG%"server - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

if "%BUILD_ERROR%"=="" goto build_ok

echo *********************
echo *********************
echo *** Build Errors! ***
echo *********************
echo *********************
echo press any key to exit
echo *********************
pause>nul
goto done


@rem
@rem Successful build
@rem
:build_ok

rem delete log files
if exist client\client.plg del /f /q client\client.plg
if exist server\server.plg del /f /q server\server.plg

echo
echo 	     Build succeeded!
echo
:done
