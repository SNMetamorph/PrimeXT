# Microsoft Developer Studio Project File - Name="hlsv" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=hlsv - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "hlsv.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "hlsv.mak" CFG="hlsv - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "hlsv - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "hlsv - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "hlsv - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\hlsv\!release"
# PROP Intermediate_Dir "..\..\temp\hlsv\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O1 /I "..\mxtk" /I "..\..\game_shared" /I "..\..\engine" /I "..\..\common" /I "..\common" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x807 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib opengl32.lib glu32.lib comctl32.lib winmm.lib ..\common\mxtk.lib /nologo /entry:"mainCRTStartup" /subsystem:windows /pdb:none /machine:I386 /nodefaultlib:"libcmt" /release /opt:nowin98
# SUBTRACT LINK32 /nodefaultlib
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\hlsv\!release
InputPath=\Paranoia2\src_main\temp\hlsv\!release\hlsv.exe
SOURCE="$(InputPath)"

"C:\Program Files\ModelViewer\hlsv.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\hlsv.exe "C:\Program Files\ModelViewer\hlsv.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "hlsv - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\hlsv\!debug"
# PROP Intermediate_Dir "..\..\temp\hlsv\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\mxtk" /I "..\..\game_shared" /I "..\..\engine" /I "..\..\common" /I "..\common" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x807 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 msvcrtd.lib user32.lib gdi32.lib winspool.lib comctl32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib opengl32.lib glu32.lib winmm.lib ..\common\mxtk.lib /nologo /entry:"mainCRTStartup" /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcd" /pdbtype:sept
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\hlsv\!debug
InputPath=\Paranoia2\src_main\temp\hlsv\!debug\hlsv.exe
SOURCE="$(InputPath)"

"C:\Program Files\ModelViewer\hlsv.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\hlsv.exe "C:\Program Files\ModelViewer\hlsv.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "hlsv - Win32 Release"
# Name "hlsv - Win32 Debug"
# Begin Source File

SOURCE=..\..\game_shared\common.cpp
# End Source File
# Begin Source File

SOURCE=.\FileAssociation.cpp
# End Source File
# Begin Source File

SOURCE=.\FileAssociation.h
# End Source File
# Begin Source File

SOURCE=.\GlWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\GlWindow.h
# End Source File
# Begin Source File

SOURCE=.\hlsv.rc
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\matrix.cpp
# End Source File
# Begin Source File

SOURCE=.\pakviewer.cpp
# End Source File
# Begin Source File

SOURCE=.\pakviewer.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\sprite.h
# End Source File
# Begin Source File

SOURCE=.\sprite_render.cpp
# End Source File
# Begin Source File

SOURCE=.\sprite_utils.cpp
# End Source File
# Begin Source File

SOURCE=.\SpriteModel.h
# End Source File
# Begin Source File

SOURCE=.\sprviewer.cpp
# End Source File
# Begin Source File

SOURCE=.\sprviewer.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=.\ViewerSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\ViewerSettings.h
# End Source File
# End Target
# End Project
