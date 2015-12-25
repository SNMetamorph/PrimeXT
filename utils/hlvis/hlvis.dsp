# Microsoft Developer Studio Project File - Name="hlvis" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=hlvis - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "hlvis.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "hlvis.mak" CFG="hlvis - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "hlvis - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "hlvis - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "hlvis - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\hlvis\!release"
# PROP Intermediate_Dir "..\..\temp\hlvis\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\common" /I "..\template" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "SYSTEM_WIN32" /D "STDC_HEADERS" /D "HLVIS" /D "VERSION_32BIT" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /opt:nowin98
# ADD LINK32 msvcrt.lib /nologo /stack:0x400000,0x100000 /subsystem:console /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# SUBTRACT LINK32 /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\xash\devkit\source_code\temp\hlvis\!release
InputPath=\Xash3D\xash\devkit\source_code\temp\hlvis\!release\hlvis.exe
SOURCE="$(InputPath)"

"D:\Xash3D\xash\devkit\tools\hlvis31.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\hlvis.exe "D:\Xash3D\xash\devkit\tools\hlvis31.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "hlvis - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "hlvis___Win32_Debug"
# PROP BASE Intermediate_Dir "hlvis___Win32_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\hlvis\!debug"
# PROP Intermediate_Dir "..\..\temp\hlvis\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /I "..\common" /I "..\template" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "SYSTEM_WIN32" /D "STDC_HEADERS" /D "HLVIS" /FD /c
# SUBTRACT BASE CPP /Fr /YX
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "..\common" /I "..\template" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "SYSTEM_WIN32" /D "STDC_HEADERS" /D "HLVIS" /D "VERSION_32BIT" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 msvcrt.lib /nologo /stack:0x400000,0x100000 /subsystem:console /pdb:none /machine:I386 /nodefaultlib:"libc.lib"
# SUBTRACT BASE LINK32 /nodefaultlib
# ADD LINK32 msvcrtd.lib /nologo /stack:0x400000,0x100000 /subsystem:console /incremental:yes /debug /machine:I386 /nodefaultlib:"libcd.lib"
# SUBTRACT LINK32 /pdb:none /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\xash\devkit\source_code\temp\hlvis\!debug
InputPath=\Xash3D\xash\devkit\source_code\temp\hlvis\!debug\hlvis.exe
SOURCE="$(InputPath)"

"D:\Xash3D\xash\devkit\tools\hlvis31.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\hlvis.exe "D:\Xash3D\xash\devkit\tools\hlvis31.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "hlvis - Win32 Release"
# Name "hlvis - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\common\blockmem.cpp
# End Source File
# Begin Source File

SOURCE=..\common\bspfile.cpp
# End Source File
# Begin Source File

SOURCE=..\common\cmdlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\filelib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\log.cpp
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\messages.cpp
# End Source File
# Begin Source File

SOURCE=..\common\scriplib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\threads.cpp
# End Source File
# Begin Source File

SOURCE=..\common\winding.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\common\cmdlinecfg.cpp
# End Source File
# Begin Source File

SOURCE=..\common\files.cpp
# End Source File
# Begin Source File

SOURCE=.\flow.cpp
# End Source File
# Begin Source File

SOURCE=..\common\resourcelock.cpp
# End Source File
# Begin Source File

SOURCE=.\vis.cpp
# End Source File
# Begin Source File

SOURCE=.\zones.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\common\anorms.h
# End Source File
# Begin Source File

SOURCE=..\common\blockmem.h
# End Source File
# Begin Source File

SOURCE=..\common\boundingbox.h
# End Source File
# Begin Source File

SOURCE=..\common\bspfile.h
# End Source File
# Begin Source File

SOURCE=..\common\cmdlib.h
# End Source File
# Begin Source File

SOURCE=..\common\cmdlinecfg.h
# End Source File
# Begin Source File

SOURCE=..\common\filelib.h
# End Source File
# Begin Source File

SOURCE=..\common\hlassert.h
# End Source File
# Begin Source File

SOURCE=..\common\log.h
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.h
# End Source File
# Begin Source File

SOURCE=..\common\mathtypes.h
# End Source File
# Begin Source File

SOURCE=..\common\messages.h
# End Source File
# Begin Source File

SOURCE=..\common\resourcelock.h
# End Source File
# Begin Source File

SOURCE=..\common\scriplib.h
# End Source File
# Begin Source File

SOURCE=..\common\threads.h
# End Source File
# Begin Source File

SOURCE=..\common\TimeCounter.h
# End Source File
# Begin Source File

SOURCE=.\vis.h
# End Source File
# Begin Source File

SOURCE=..\common\win32fix.h
# End Source File
# Begin Source File

SOURCE=..\common\winding.h
# End Source File
# Begin Source File

SOURCE=.\zones.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project

