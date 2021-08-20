# Microsoft Developer Studio Project File - Name="p2csg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=p2csg - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "p2csg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "p2csg.mak" CFG="p2csg - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "p2csg - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "p2csg - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/SDKSrc/Tools/utils/p2csg", XUGBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "p2csg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\p2csg\!release"
# PROP Intermediate_Dir "..\..\temp\p2csg\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\common" /I "..\..\common" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "DOUBLEVEC_T" /D "IGNORE_SEARCH_IN_WADS" /D "ALLOW_WADS_IN_PACKS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /opt:nowin98
# ADD LINK32 /nologo /subsystem:console /pdb:none /machine:I386 /opt:nowin98
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\p2csg\!release
InputPath=\Paranoia2\src_main\temp\p2csg\!release\p2csg.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\p2csg.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\p2csg.exe "D:\Paranoia2\tools\p2csg.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "p2csg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\p2csg\!debug"
# PROP Intermediate_Dir "..\..\temp\p2csg\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\common" /I "..\..\common" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "DOUBLEVEC_T" /D "IGNORE_SEARCH_IN_WADS" /D "ALLOW_WADS_IN_PACKS" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 msvcrtd.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libcmtd.lib"
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\p2csg\!debug
InputPath=\Paranoia2\src_main\temp\p2csg\!debug\p2csg.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\p2csg.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\p2csg.exe "D:\Paranoia2\tools\p2csg.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "p2csg - Win32 Release"
# Name "p2csg - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\aselib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\basefs.cpp
# End Source File
# Begin Source File

SOURCE=.\brush.cpp
# End Source File
# Begin Source File

SOURCE=..\common\bspfile.cpp
# End Source File
# Begin Source File

SOURCE=..\common\cmdlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\conprint.cpp
# End Source File
# Begin Source File

SOURCE=.\csg4.cpp
# End Source File
# Begin Source File

SOURCE=..\common\filesystem.cpp
# End Source File
# Begin Source File

SOURCE=.\map.cpp
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=.\patch.cpp
# End Source File
# Begin Source File

SOURCE=..\common\polylib.cpp
# End Source File
# Begin Source File

SOURCE=.\qcsg.cpp
# End Source File
# Begin Source File

SOURCE=..\common\scriplib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\shaders.cpp
# End Source File
# Begin Source File

SOURCE=..\common\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=.\textures.cpp
# End Source File
# Begin Source File

SOURCE=..\common\threads.cpp
# End Source File
# Begin Source File

SOURCE=.\utils.cpp
# End Source File
# Begin Source File

SOURCE=..\common\wadfile.cpp
# End Source File
# Begin Source File

SOURCE=..\common\zone.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\aselib.h
# End Source File
# Begin Source File

SOURCE=..\common\bspfile.h
# End Source File
# Begin Source File

SOURCE=..\common\cmdlib.h
# End Source File
# Begin Source File

SOURCE=.\csg.h
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.h
# End Source File
# Begin Source File

SOURCE=..\common\polylib.h
# End Source File
# Begin Source File

SOURCE=..\common\scriplib.h
# End Source File
# Begin Source File

SOURCE=..\common\shaders.h
# End Source File
# Begin Source File

SOURCE=..\common\threads.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
