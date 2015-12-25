@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=release
set BUILD_ERROR=
call vcvars32

%MSDEV% client/client.dsp %CONFIG%"client - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% server/server.dsp %CONFIG%"server - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% utils/hlcsg/hlcsg.dsp %CONFIG%"hlcsg - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% utils/hlbsp/hlbsp.dsp %CONFIG%"hlbsp - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% utils/hlvis/hlvis.dsp %CONFIG%"hlvis - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% utils/hlrad/hlrad.dsp %CONFIG%"hlrad - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% utils/studiomdl/studiomdl.dsp %CONFIG%"studiomdl - Win32 Release" %build_target%
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
if exist utils\hlcsg\hlcsg.plg del /f /q utils\hlcsg\hlcsg.plg
if exist utils\hlbsp\hlbsp.plg del /f /q utils\hlbsp\hlbsp.plg
if exist utils\hlvis\hlvis.plg del /f /q utils\hlvis\hlvis.plg
if exist utils\hlrad\hlrad.plg del /f /q utils\hlrad\hlrad.plg
if exist utils\studiomdl\studiomdl.plg del /f /q utils\studiomdl\studiomdl.plg

echo
echo 	     Build succeeded!
echo
:done
