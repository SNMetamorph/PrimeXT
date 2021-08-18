# Microsoft Developer Studio Project File - Name="hlmv" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=hlmv - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "hlmv.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "hlmv.mak" CFG="hlmv - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "hlmv - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "hlmv - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "hlmv - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\hlmv\!release"
# PROP Intermediate_Dir "..\..\temp\hlmv\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O1 /I "..\mxtk" /I "..\..\game_shared" /I "..\..\engine" /I "..\..\common" /I "..\common" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "HAVE_SCALE" /FD /I /mxtk/include" " /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x807 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib winspool.lib comctl32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib opengl32.lib glu32.lib winmm.lib ..\common\mxtk.lib /nologo /version:1.2 /entry:"mainCRTStartup" /subsystem:windows /pdb:none /machine:I386 /nodefaultlib:"libcmt" /libpath:"/mxtk/lib" /release /opt:nowin98
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\hlmv\!release
InputPath=\Paranoia2\src_main\temp\hlmv\!release\hlmv.exe
SOURCE="$(InputPath)"

"C:\Program Files\ModelViewer\hlmv.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\hlmv.exe "C:\Program Files\ModelViewer\hlmv.exe"

# End Custom Build

!ELSEIF  "$(CFG)" == "hlmv - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\hlmv\!debug"
# PROP Intermediate_Dir "..\..\temp\hlmv\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\mxtk" /I "..\..\game_shared" /I "..\..\engine" /I "..\..\common" /I "..\common" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FAs /FR /FD /I /mxtk/include" " /c
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
# ADD LINK32 msvcrtd.lib user32.lib gdi32.lib winspool.lib comctl32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib opengl32.lib glu32.lib winmm.lib ..\common\mxtk.lib /nologo /version:1.1 /entry:"mainCRTStartup" /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcmtd" /nodefaultlib:"msvcrt" /pdbtype:sept /libpath:"/mxtk/lib/"
# Begin Custom Build
TargetDir=\Paranoia2\src_main\temp\hlmv\!debug
InputPath=\Paranoia2\src_main\temp\hlmv\!debug\hlmv.exe
SOURCE="$(InputPath)"

"C:\Program Files\ModelViewer\hlmv.exe" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\hlmv.exe "C:\Program Files\ModelViewer\hlmv.exe"

# End Custom Build

!ENDIF 

# Begin Target

# Name "hlmv - Win32 Release"
# Name "hlmv - Win32 Debug"
# Begin Source File

SOURCE=.\anorms.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\bone_setup.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\bs_defs.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\common.cpp
# End Source File
# Begin Source File

SOURCE=..\common\conprint.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlPanel.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlPanel.h
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

SOURCE=.\hlmv.manifest
# End Source File
# Begin Source File

SOURCE=.\hlmview.manifest
# End Source File
# Begin Source File

SOURCE=.\hlmviewer.rc
# End Source File
# Begin Source File

SOURCE=.\icon1.ico
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\ikcontext.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\jigglebones.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\jigglebones.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\matrix.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\matrix.h
# End Source File
# Begin Source File

SOURCE=.\mdlviewer.cpp
# End Source File
# Begin Source File

SOURCE=.\mdlviewer.h
# End Source File
# Begin Source File

SOURCE=.\pakviewer.cpp
# End Source File
# Begin Source File

SOURCE=.\pakviewer.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\procbones.cpp
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\stringlib.h
# End Source File
# Begin Source File

SOURCE=.\studio_render.cpp
# End Source File
# Begin Source File

SOURCE=.\studio_utils.cpp
# End Source File
# Begin Source File

SOURCE=.\StudioModel.h
# End Source File
# Begin Source File

SOURCE=..\..\game_shared\vector.h
# End Source File
# Begin Source File

SOURCE=.\ViewerSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\ViewerSettings.h
# End Source File
# End Target
# End Project
