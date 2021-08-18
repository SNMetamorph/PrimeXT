# Microsoft Developer Studio Project File - Name="mxtk" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mxtk - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mxtk.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mxtk.mak" CFG="mxtk - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mxtk - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mxtk - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mxtk - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\mxtk\!release"
# PROP Intermediate_Dir "..\..\temp\mxtk\!release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I ".\\" /I "..\..\game_shared" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x807
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\common\mxtk.lib"

!ELSEIF  "$(CFG)" == "mxtk - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\mxtk\!debug"
# PROP Intermediate_Dir "..\..\temp\mxtk\!debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /GX /ZI /Od /I ".\\" /I "..\..\game_shared" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FAs /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x807
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\common\mxtk.lib"

!ENDIF 

# Begin Target

# Name "mxtk - Win32 Release"
# Name "mxtk - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp"
# Begin Source File

SOURCE=.\mx.cpp
# End Source File
# Begin Source File

SOURCE=.\mxBmp.cpp
# End Source File
# Begin Source File

SOURCE=.\mxButton.cpp
# End Source File
# Begin Source File

SOURCE=.\mxCheckBox.cpp
# End Source File
# Begin Source File

SOURCE=.\mxChoice.cpp
# End Source File
# Begin Source File

SOURCE=.\mxChooseColor.cpp
# End Source File
# Begin Source File

SOURCE=.\mxFileDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\mxGlWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\mxGroupBox.cpp
# End Source File
# Begin Source File

SOURCE=.\mxLabel.cpp
# End Source File
# Begin Source File

SOURCE=.\mxLineEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\mxListBox.cpp
# End Source File
# Begin Source File

SOURCE=.\mxMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\mxMenuBar.cpp
# End Source File
# Begin Source File

SOURCE=.\mxMessageBox.cpp
# End Source File
# Begin Source File

SOURCE=.\mxMultiLineEdit.cpp
# End Source File
# Begin Source File

SOURCE=.\mxpath.cpp
# End Source File
# Begin Source File

SOURCE=.\mxPcx.cpp
# End Source File
# Begin Source File

SOURCE=.\mxPopupMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\mxProgressBar.cpp
# End Source File
# Begin Source File

SOURCE=.\mxRadioButton.cpp
# End Source File
# Begin Source File

SOURCE=.\mxSlider.cpp
# End Source File
# Begin Source File

SOURCE=.\mxstring.cpp
# End Source File
# Begin Source File

SOURCE=.\mxTab.cpp
# End Source File
# Begin Source File

SOURCE=.\mxTga.cpp
# End Source File
# Begin Source File

SOURCE=.\mxToggleButton.cpp
# End Source File
# Begin Source File

SOURCE=.\mxToolTip.cpp
# End Source File
# Begin Source File

SOURCE=.\mxTreeView.cpp
# End Source File
# Begin Source File

SOURCE=.\mxWidget.cpp
# End Source File
# Begin Source File

SOURCE=.\mxWindow.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\gl.h
# End Source File
# Begin Source File

SOURCE=.\mx.h
# End Source File
# Begin Source File

SOURCE=.\mxBmp.h
# End Source File
# Begin Source File

SOURCE=.\mxButton.h
# End Source File
# Begin Source File

SOURCE=.\mxCheckBox.h
# End Source File
# Begin Source File

SOURCE=.\mxChoice.h
# End Source File
# Begin Source File

SOURCE=.\mxChooseColor.h
# End Source File
# Begin Source File

SOURCE=.\mxEvent.h
# End Source File
# Begin Source File

SOURCE=.\mxFileDialog.h
# End Source File
# Begin Source File

SOURCE=.\mxGlWindow.h
# End Source File
# Begin Source File

SOURCE=.\mxGroupBox.h
# End Source File
# Begin Source File

SOURCE=.\mxImage.h
# End Source File
# Begin Source File

SOURCE=.\mxInit.h
# End Source File
# Begin Source File

SOURCE=.\mxLabel.h
# End Source File
# Begin Source File

SOURCE=.\mxLineEdit.h
# End Source File
# Begin Source File

SOURCE=.\mxLinkedList.h
# End Source File
# Begin Source File

SOURCE=.\mxListBox.h
# End Source File
# Begin Source File

SOURCE=.\mxMenu.h
# End Source File
# Begin Source File

SOURCE=.\mxMenuBar.h
# End Source File
# Begin Source File

SOURCE=.\mxMessageBox.h
# End Source File
# Begin Source File

SOURCE=.\mxMultiLineEdit.h
# End Source File
# Begin Source File

SOURCE=.\mxpath.h
# End Source File
# Begin Source File

SOURCE=.\mxPcx.h
# End Source File
# Begin Source File

SOURCE=.\mxPopupMenu.h
# End Source File
# Begin Source File

SOURCE=.\mxProgressBar.h
# End Source File
# Begin Source File

SOURCE=.\mxRadioButton.h
# End Source File
# Begin Source File

SOURCE=.\mxSlider.h
# End Source File
# Begin Source File

SOURCE=.\mxstring.h
# End Source File
# Begin Source File

SOURCE=.\mxTab.h
# End Source File
# Begin Source File

SOURCE=.\mxTga.h
# End Source File
# Begin Source File

SOURCE=.\mxToggleButton.h
# End Source File
# Begin Source File

SOURCE=.\mxToolTip.h
# End Source File
# Begin Source File

SOURCE=.\mxTreeView.h
# End Source File
# Begin Source File

SOURCE=.\mxWidget.h
# End Source File
# Begin Source File

SOURCE=.\mxWindow.h
# End Source File
# End Group
# End Target
# End Project
