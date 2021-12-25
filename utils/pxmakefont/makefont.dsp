# Microsoft Developer Studio Project File - Name="makefont" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=makefont - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "makefont.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "makefont.mak" CFG="makefont - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "makefont - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "makefont - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "makefont - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\makefont\!release"
# PROP Intermediate_Dir "..\..\temp\makefont\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../../common" /I "../common" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "IGNORE_SEARCH_IN_WADS" /FD /c
# SUBTRACT CPP /Z<none> /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 msvcrt.lib gdi32.lib user32.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libcmt"
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\makefont\!release
InputPath=\Paranoia2\src_main\temp\makefont\!release\makefont.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\makefont.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\makefont.exe "D:\Paranoia2\tools\makefont.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "makefont - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\makefont\!debug"
# PROP Intermediate_Dir "..\..\temp\makefont\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I "../../common" /I "../common" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "IGNORE_SEARCH_IN_WADS" /FAs /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 msvcrtd.lib gdi32.lib user32.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libcd.lib"
# SUBTRACT LINK32 /pdbtype:<none>
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\makefont\!debug
InputPath=\Paranoia2\src_main\temp\makefont\!debug\makefont.exe
SOURCE="$(InputPath)"

"D:\Paranoia2\tools\makefont.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\makefont.exe "D:\Paranoia2\tools\makefont.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "makefont - Win32 Release"
# Name "makefont - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\common\cmdlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\conprint.cpp
# End Source File
# Begin Source File

SOURCE=..\common\filesystem.cpp
# End Source File
# Begin Source File

SOURCE=.\makefont.cpp
# End Source File
# Begin Source File

SOURCE=..\common\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=..\common\wadfile.cpp
# End Source File
# Begin Source File

SOURCE=..\common\zone.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
