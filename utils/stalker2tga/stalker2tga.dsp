# Microsoft Developer Studio Project File - Name="stalker2tga" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=stalker2tga - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "stalker2tga.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "stalker2tga.mak" CFG="stalker2tga - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "stalker2tga - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "stalker2tga - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/SDKSrc/Tools/utils/stalker2tga", IVGBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "stalker2tga - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\stalker2tga\!release"
# PROP Intermediate_Dir "..\..\temp\stalker2tga\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../common" /I "../common" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "IGNORE_SEARCH_IN_WADS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386 /opt:nowin98
# ADD LINK32 msvcrt.lib ..\common\squish.lib /nologo /subsystem:console /pdb:none /machine:I386 /nodefaultlib:"libcmt" /nodefaultlib:"libc" /libpath:"./" /opt:nowin98
# SUBTRACT LINK32 /nodefaultlib
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\stalker2tga\!release
InputPath=\Paranoia2\src_main\temp\stalker2tga\!release\stalker2tga.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\stalker2tga.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\stalker2tga.exe "D:\Paranoia2\tools\stalker2tga.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "stalker2tga - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\stalker2tga\!debug"
# PROP Intermediate_Dir "..\..\temp\stalker2tga\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "../../common" /I "../common" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "IGNORE_SEARCH_IN_WADS" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 msvcrtd.lib ..\common\squish.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libc.lib" /libpath:"./"
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\stalker2tga\!debug
InputPath=\Paranoia2\src_main\temp\stalker2tga\!debug\stalker2tga.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\stalker2tga.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\stalker2tga.exe "D:\Paranoia2\tools\stalker2tga.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "stalker2tga - Win32 Release"
# Name "stalker2tga - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=..\common\cmdlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\conprint.cpp
# End Source File
# Begin Source File

SOURCE=..\common\ddstex.cpp
# End Source File
# Begin Source File

SOURCE=..\common\filesystem.cpp
# End Source File
# Begin Source File

SOURCE=..\common\gamma.cpp
# End Source File
# Begin Source File

SOURCE=..\common\imagelib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=.\stalker2tga.cpp
# End Source File
# Begin Source File

SOURCE=..\common\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\virtualfs.cpp
# End Source File
# Begin Source File

SOURCE=..\common\zone.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\common\ddstex.h
# End Source File
# Begin Source File

SOURCE=..\common\filesystem.h
# End Source File
# Begin Source File

SOURCE=.\imagelib.h
# End Source File
# Begin Source File

SOURCE=..\common\stringlib.h
# End Source File
# End Group
# End Target
# End Project
