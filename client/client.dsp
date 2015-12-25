# Microsoft Developer Studio Project File - Name="client" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=client - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "client.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "client.mak" CFG="client - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "client - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "client - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/GoldSrc/client", HGEBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "client - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\!Release"
# PROP BASE Intermediate_Dir ".\!Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\client\!release"
# PROP Intermediate_Dir "..\temp\client\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /Ox /Ot /Og /Oi /Op /Ob2 /I ".\\" /I "..\engine" /I "..\common" /I "..\game_shared" /I ".\render" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /FD /c
# SUBTRACT CPP /Oa /Ow /Os /Gf /Gy /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 msvcrt.lib user32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /libpath:"..\common\libs"
# SUBTRACT LINK32 /map
# Begin Custom Build
TargetDir=\Xash3D\xash\devkit\source_code\temp\client\!release
InputPath=\Xash3D\xash\devkit\source_code\temp\client\!release\client.dll
SOURCE="$(InputPath)"

"D:\Xash3D\xash\bin\client.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\client.dll "D:\Xash3D\xash\bin\client.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "client - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "client___Win32_Debug"
# PROP BASE Intermediate_Dir "client___Win32_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\client\!debug"
# PROP Intermediate_Dir "..\temp\client\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /O2 /I "..\common\vgui" /I "..\client" /I "..\client\render" /I ".\hud" /I "..\common\engine" /I "..\common" /I "..\server" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /YX /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I ".\\" /I "..\engine" /I "..\common" /I "..\game_shared" /I ".\render" /D "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib opengl32.lib glu32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib winmm.lib vgui.lib wsock32.lib cvaLib.lib /nologo /subsystem:windows /dll /machine:I386 /nodefaultlib:"libc" /libpath:"..\common\libs"
# SUBTRACT BASE LINK32 /map
# ADD LINK32 msvcrtd.lib user32.lib opengl32.lib glu32.lib /nologo /subsystem:windows /dll /incremental:yes /debug /machine:I386 /nodefaultlib:"libc" /libpath:"..\common\libs"
# SUBTRACT LINK32 /map
# Begin Custom Build
TargetDir=\Xash3D\xash\devkit\source_code\temp\client\!debug
InputPath=\Xash3D\xash\devkit\source_code\temp\client\!debug\client.dll
SOURCE="$(InputPath)"

"D:\Xash3D\xash\bin\client.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\client.dll "D:\Xash3D\xash\bin\client.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "client - Win32 Release"
# Name "client - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\ammo.cpp
# End Source File
# Begin Source File

SOURCE=.\ammo_secondary.cpp
# End Source File
# Begin Source File

SOURCE=.\ammohistory.cpp
# End Source File
# Begin Source File

SOURCE=.\battery.cpp
# End Source File
# Begin Source File

SOURCE=.\cdll_int.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\common.cpp
# End Source File
# Begin Source File

SOURCE=.\death.cpp
# End Source File
# Begin Source File

SOURCE=.\flashlight.cpp
# End Source File
# Begin Source File

SOURCE=.\geiger.cpp
# End Source File
# Begin Source File

SOURCE=.\health.cpp
# End Source File
# Begin Source File

SOURCE=.\hud.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_msg.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_redraw.cpp
# End Source File
# Begin Source File

SOURCE=.\hud_update.cpp
# End Source File
# Begin Source File

SOURCE=.\input.cpp
# End Source File
# Begin Source File

SOURCE=.\inputw32.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\matrix.cpp
# End Source File
# Begin Source File

SOURCE=.\menu.cpp
# End Source File
# Begin Source File

SOURCE=.\message.cpp
# End Source File
# Begin Source File

SOURCE=.\motd.cpp
# End Source File
# Begin Source File

SOURCE=.\parsemsg.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\parser.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\playermove.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_backend.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_beams.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_bloom.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_cull.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_debug.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_grass.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_light.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_main.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_mirror.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_misc.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_monitor.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_movie.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_opengl.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_particle.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_portal.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_shadows.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_sprite.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_studio.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_surf.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_tempents.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_view.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_warp.cpp
# End Source File
# Begin Source File

SOURCE=.\render\r_weather.cpp
# End Source File
# Begin Source File

SOURCE=.\saytext.cpp
# End Source File
# Begin Source File

SOURCE=.\scoreboard.cpp
# End Source File
# Begin Source File

SOURCE=.\status_icons.cpp
# End Source File
# Begin Source File

SOURCE=.\statusbar.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=.\text_message.cpp
# End Source File
# Begin Source File

SOURCE=.\train.cpp
# End Source File
# Begin Source File

SOURCE=.\utils.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\ammo.h
# End Source File
# Begin Source File

SOURCE=.\ammohistory.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\cdll_dll.h
# End Source File
# Begin Source File

SOURCE=.\hud.h
# End Source File
# Begin Source File

SOURCE=.\hud_events.h
# End Source File
# Begin Source File

SOURCE=.\hud_health.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\mathlib.h
# End Source File
# Begin Source File

SOURCE=.\parsemsg.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\plane.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\pm_defs.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\pm_info.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\pm_materials.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\pm_movevars.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\pm_shared.h
# End Source File
# Begin Source File

SOURCE=.\render\r_grass.h
# End Source File
# Begin Source File

SOURCE=.\render\r_opengl.h
# End Source File
# Begin Source File

SOURCE=.\render\r_particle.h
# End Source File
# Begin Source File

SOURCE=.\render\r_programs.h
# End Source File
# Begin Source File

SOURCE=.\render\r_studio.h
# End Source File
# Begin Source File

SOURCE=.\render\r_view.h
# End Source File
# Begin Source File

SOURCE=.\render\r_weather.h
# End Source File
# Begin Source File

SOURCE=.\utils.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\vector.h
# End Source File
# End Group
# End Target
# End Project

