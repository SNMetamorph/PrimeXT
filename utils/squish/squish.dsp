# Microsoft Developer Studio Project File - Name="squish" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=squish - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "squish.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "squish.mak" CFG="squish - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "squish - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "squish - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "squish - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\temp\squish\!release"
# PROP Intermediate_Dir "..\..\temp\squish\!release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\common" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\common\squish.lib"

!ELSEIF  "$(CFG)" == "squish - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\temp\squish\!debug"
# PROP Intermediate_Dir "..\..\temp\squish\!debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I "..\common" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FAs /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\common\squish.lib"

!ENDIF 

# Begin Target

# Name "squish - Win32 Release"
# Name "squish - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\alpha.cpp
# End Source File
# Begin Source File

SOURCE=.\clusterfit.cpp
# End Source File
# Begin Source File

SOURCE=.\colourblock.cpp
# End Source File
# Begin Source File

SOURCE=.\colourfit.cpp
# End Source File
# Begin Source File

SOURCE=.\colourset.cpp
# End Source File
# Begin Source File

SOURCE=.\maths.cpp
# End Source File
# Begin Source File

SOURCE=.\rangefit.cpp
# End Source File
# Begin Source File

SOURCE=.\singlecolourfit.cpp
# End Source File
# Begin Source File

SOURCE=.\squish.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\alpha.h
# End Source File
# Begin Source File

SOURCE=.\clusterfit.h
# End Source File
# Begin Source File

SOURCE=.\colourblock.h
# End Source File
# Begin Source File

SOURCE=.\colourfit.h
# End Source File
# Begin Source File

SOURCE=.\colourset.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\maths.h
# End Source File
# Begin Source File

SOURCE=.\rangefit.h
# End Source File
# Begin Source File

SOURCE=.\simd.h
# End Source File
# Begin Source File

SOURCE=.\simd_float.h
# End Source File
# Begin Source File

SOURCE=.\simd_sse.h
# End Source File
# Begin Source File

SOURCE=.\simd_ve.h
# End Source File
# Begin Source File

SOURCE=.\singlecolourfit.h
# End Source File
# Begin Source File

SOURCE=.\singlecolourlookup.inl
# End Source File
# Begin Source File

SOURCE=.\squish.h
# End Source File
# End Group
# End Target
# End Project
