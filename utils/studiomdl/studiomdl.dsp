# Microsoft Developer Studio Project File - Name="studiomdl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=studiomdl - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "studiomdl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "studiomdl.mak" CFG="studiomdl - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "studiomdl - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "studiomdl - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/SDKSrc/Tools/utils/studiomdl", IVGBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "studiomdl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\studiomdl\!release"
# PROP Intermediate_Dir "..\..\temp\studiomdl\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../../client" /I "../../common" /I "../../server" /I "../../engine" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 msvcrt.lib /nologo /subsystem:console /pdb:none /machine:I386 /nodefaultlib:"libc.lib"
# Begin Custom Build
TargetDir=\Xash3D\xash\devkit\source_code\temp\studiomdl\!release
InputPath=\Xash3D\xash\devkit\source_code\temp\studiomdl\!release\studiomdl.exe
SOURCE="$(InputPath)"

"D:\Xash3D\xash\devkit\tools\studiomdl.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\studiomdl.exe "D:\Xash3D\xash\devkit\tools\studiomdl.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "studiomdl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\studiomdl\!debug"
# PROP Intermediate_Dir "..\..\temp\studiomdl\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "../../client" /I "../../common" /I "../../server" /I "../../engine" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 msvcrtd.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libcd.lib"
# Begin Custom Build
TargetDir=\Xash3D\xash\devkit\source_code\temp\studiomdl\!debug
InputPath=\Xash3D\xash\devkit\source_code\temp\studiomdl\!debug\studiomdl.exe
SOURCE="$(InputPath)"

"D:\Xash3D\xash\devkit\tools\studiomdl.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\studiomdl.exe "D:\Xash3D\xash\devkit\tools\studiomdl.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "studiomdl - Win32 Release"
# Name "studiomdl - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\bmpread.c
# End Source File
# Begin Source File

SOURCE=.\cmdlib.c
# End Source File
# Begin Source File

SOURCE=.\mathlib.c
# End Source File
# Begin Source File

SOURCE=.\scriplib.c
# End Source File
# Begin Source File

SOURCE=.\studiomdl.c
# End Source File
# Begin Source File

SOURCE=.\trilib.c
# End Source File
# Begin Source File

SOURCE=.\tristrip.c
# End Source File
# Begin Source File

SOURCE=.\write.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\cmdlib.h
# End Source File
# Begin Source File

SOURCE=.\mathlib.h
# End Source File
# Begin Source File

SOURCE=.\scriplib.h
# End Source File
# Begin Source File

SOURCE=..\..\engine\studio.h
# End Source File
# Begin Source File

SOURCE=.\studiomdl.h
# End Source File
# Begin Source File

SOURCE=.\trilib.h
# End Source File
# End Group
# End Target
# End Project

