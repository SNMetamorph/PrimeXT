# Microsoft Developer Studio Project File - Name="p2bsp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=p2bsp - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "p2bsp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "p2bsp.mak" CFG="p2bsp - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "p2bsp - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "p2bsp - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/SDKSrc/Tools/utils/p2bsp", VUGBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "p2bsp - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\p2bsp___"
# PROP BASE Intermediate_Dir ".\p2bsp___"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\p2bsp\!release"
# PROP Intermediate_Dir "..\..\temp\p2bsp\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\common" /I "..\..\common" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "DOUBLEVEC_T" /D "IGNORE_SEARCH_IN_WADS" /FD /c
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
TargetDir=\Paranoia2\src_main\temp\p2bsp\!release
InputPath=\Paranoia2\src_main\temp\p2bsp\!release\p2bsp.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\p2bsp.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\p2bsp.exe "D:\Paranoia2\tools\p2bsp.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "p2bsp - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\p2bsp__0"
# PROP BASE Intermediate_Dir ".\p2bsp__0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\p2bsp\!debug"
# PROP Intermediate_Dir "..\..\temp\p2bsp\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\common" /I "..\..\common" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "DOUBLEVEC_T" /D "IGNORE_SEARCH_IN_WADS" /FAs /FR /FD /c
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
TargetDir=\Paranoia2\src_main\temp\p2bsp\!debug
InputPath=\Paranoia2\src_main\temp\p2bsp\!debug\p2bsp.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\p2bsp.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\p2bsp.exe "D:\Paranoia2\tools\p2bsp.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "p2bsp - Win32 Release"
# Name "p2bsp - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\bsp5.h
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

SOURCE=.\detail.cpp
# End Source File
# Begin Source File

SOURCE=..\common\filesystem.cpp
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=.\merge.cpp
# End Source File
# Begin Source File

SOURCE=.\outside.cpp
# End Source File
# Begin Source File

SOURCE=.\partition.cpp
# End Source File
# Begin Source File

SOURCE=..\common\polylib.cpp
# End Source File
# Begin Source File

SOURCE=.\portals.cpp
# End Source File
# Begin Source File

SOURCE=.\qbsp.cpp
# End Source File
# Begin Source File

SOURCE=..\common\scriplib.cpp
# End Source File
# Begin Source File

SOURCE=.\solidbsp.cpp
# End Source File
# Begin Source File

SOURCE=..\common\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=.\surfaces.cpp
# End Source File
# Begin Source File

SOURCE=..\common\threads.cpp
# End Source File
# Begin Source File

SOURCE=.\tjunc.cpp
# End Source File
# Begin Source File

SOURCE=.\tree.cpp
# End Source File
# Begin Source File

SOURCE=.\writebsp.cpp
# End Source File
# Begin Source File

SOURCE=..\common\zone.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\common\bspfile.h
# End Source File
# Begin Source File

SOURCE=..\common\cmdlib.h
# End Source File
# Begin Source File

SOURCE=..\common\mathlib.h
# End Source File
# Begin Source File

SOURCE=..\common\scriplib.h
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
