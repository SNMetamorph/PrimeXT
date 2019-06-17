# Microsoft Developer Studio Project File - Name="server" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=server - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "server.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "server.mak" CFG="server - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "server - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "server - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/GoldSrc/server", ELEBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "server - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\server\!release"
# PROP Intermediate_Dir "..\temp\server\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Oy- /I ".\\" /I "..\engine" /I "..\common" /I "..\game_shared" /I "..\phys_shared" /I ".\monsters" /I ".\physics" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 msvcrt.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /def:".\server.def"
# SUBTRACT LINK32 /profile /map /debug
# Begin Custom Build - Custom Build in progress
TargetDir=\Xash3D\xash\src_main\temp\server\!release
InputPath=\Xash3D\xash\src_main\temp\server\!release\server.dll
SOURCE="$(InputPath)"

"D:\Xash3D\xash\bin\server.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\server.dll "D:\Xash3D\xash\bin\server.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "server - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "server___Win32_Debug"
# PROP BASE Intermediate_Dir "server___Win32_Debug"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\server\!debug"
# PROP Intermediate_Dir "..\temp\server\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /MT /W3 /O2 /I "..\server" /I "..\common\engine" /I "..\common" /I "..\server\ents" /I "..\server\global" /I "..\server\weapons" /I "..\server\game" /I "..\server\monsters" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /W3 /Gm /Gi /GX /ZI /Od /I ".\\" /I "..\engine" /I "..\common" /I "..\game_shared" /I "..\phys_shared" /I ".\monsters" /I ".\physics" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "DEBUG" /D "_DEBUG" /FAs /FR /FD /c
# SUBTRACT CPP /u /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /incremental:yes /machine:I386 /nodefaultlib:"libc" /def:".\server.def" /libpath:"..\common\libs"
# SUBTRACT BASE LINK32 /profile /map /debug
# ADD LINK32 msvcrt.lib /nologo /subsystem:windows /dll /incremental:yes /debug /machine:I386 /nodefaultlib:"libc.lib" /def:".\server.def"
# SUBTRACT LINK32 /profile /map
# Begin Custom Build - Custom Build in progress
TargetDir=\Xash3D\xash\src_main\temp\server\!debug
InputPath=\Xash3D\xash\src_main\temp\server\!debug\server.dll
SOURCE="$(InputPath)"

"D:\Xash3D\xash\bin\server.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\server.dll "D:\Xash3D\xash\bin\server.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "server - Win32 Release"
# Name "server - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\monsters\aflock.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\agrunt.cpp
# End Source File
# Begin Source File

SOURCE=.\airtank.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\animating.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\animation.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\apache.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\barnacle.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\barney.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\bigmomma.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\bloater.cpp
# End Source File
# Begin Source File

SOURCE=.\bmodels.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\bone_setup.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\bullsquid.cpp
# End Source File
# Begin Source File

SOURCE=.\buttons.cpp
# End Source File
# Begin Source File

SOURCE=.\cbase.cpp
# End Source File
# Begin Source File

SOURCE=.\ropes\CElectrifiedWire.cpp
# End Source File
# Begin Source File

SOURCE=.\client.cpp
# End Source File
# Begin Source File

SOURCE=.\combat.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\common.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\controller.cpp
# End Source File
# Begin Source File

SOURCE=.\ropes\CRope.cpp
# End Source File
# Begin Source File

SOURCE=.\ropes\CRopeSegment.cpp
# End Source File
# Begin Source File

SOURCE=.\crossbow.cpp
# End Source File
# Begin Source File

SOURCE=.\crowbar.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\defaultai.cpp
# End Source File
# Begin Source File

SOURCE=.\dll_int.cpp
# End Source File
# Begin Source File

SOURCE=.\doors.cpp
# End Source File
# Begin Source File

SOURCE=.\effects.cpp
# End Source File
# Begin Source File

SOURCE=.\egon.cpp
# End Source File
# Begin Source File

SOURCE=.\explode.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\flyingmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\func_break.cpp
# End Source File
# Begin Source File

SOURCE=.\func_monitor.cpp
# End Source File
# Begin Source File

SOURCE=.\func_tank.cpp
# End Source File
# Begin Source File

SOURCE=.\game.cpp
# End Source File
# Begin Source File

SOURCE=.\gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\gargantua.cpp
# End Source File
# Begin Source File

SOURCE=.\gauss.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\genericmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\ggrenade.cpp
# End Source File
# Begin Source File

SOURCE=.\globals.cpp
# End Source File
# Begin Source File

SOURCE=.\glock.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\gman.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\h_ai.cpp
# End Source File
# Begin Source File

SOURCE=.\h_battery.cpp
# End Source File
# Begin Source File

SOURCE=.\h_cycler.cpp
# End Source File
# Begin Source File

SOURCE=.\handgrenade.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\hassassin.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\headcrab.cpp
# End Source File
# Begin Source File

SOURCE=.\healthkit.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\hgrunt.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\hornet.cpp
# End Source File
# Begin Source File

SOURCE=.\hornetgun.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\houndeye.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\ichthyosaur.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\ikcontext.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\islave.cpp
# End Source File
# Begin Source File

SOURCE=.\items.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\leech.cpp
# End Source File
# Begin Source File

SOURCE=.\lights.cpp
# End Source File
# Begin Source File

SOURCE=.\mapents.cpp
# End Source File
# Begin Source File

SOURCE=.\maprules.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\mathlib.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\matrix.cpp
# End Source File
# Begin Source File

SOURCE=.\physics\meshdesc.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\monstermaker.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\monsters.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\monsterstate.cpp
# End Source File
# Begin Source File

SOURCE=.\mortar.cpp
# End Source File
# Begin Source File

SOURCE=.\mp5.cpp
# End Source File
# Begin Source File

SOURCE=.\multiplay_gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\nihilanth.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\nodes.cpp
# End Source File
# Begin Source File

SOURCE=.\novodex.cpp
# End Source File
# Begin Source File

SOURCE=.\physics\NxUserStream.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\osprey.cpp
# End Source File
# Begin Source File

SOURCE=.\pathcorner.cpp
# End Source File
# Begin Source File

SOURCE=.\physents.cpp
# End Source File
# Begin Source File

SOURCE=.\physic.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\plane.cpp
# End Source File
# Begin Source File

SOURCE=.\plats.cpp
# End Source File
# Begin Source File

SOURCE=.\player.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\playermonster.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\playermove.cpp
# End Source File
# Begin Source File

SOURCE=.\python.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\rat.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\roach.cpp
# End Source File
# Begin Source File

SOURCE=.\rpg.cpp
# End Source File
# Begin Source File

SOURCE=.\satchel.cpp
# End Source File
# Begin Source File

SOURCE=.\saverestore.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\schedule.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\scientist.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\scripted.cpp
# End Source File
# Begin Source File

SOURCE=.\shotgun.cpp
# End Source File
# Begin Source File

SOURCE=.\singleplay_gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\skill.cpp
# End Source File
# Begin Source File

SOURCE=.\sound.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\soundent.cpp
# End Source File
# Begin Source File

SOURCE=.\spectator.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\spritemonster.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\squadmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\squeakgrenade.cpp
# End Source File
# Begin Source File

SOURCE=.\stats.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\stringlib.cpp
# End Source File
# Begin Source File

SOURCE=.\strings.cpp
# End Source File
# Begin Source File

SOURCE=.\subs.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\talkmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\teamplay_gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\tempmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\tentacle.cpp
# End Source File
# Begin Source File

SOURCE=.\physics\tracemesh.cpp
# End Source File
# Begin Source File

SOURCE=.\triggers.cpp
# End Source File
# Begin Source File

SOURCE=.\tripmine.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\turret.cpp
# End Source File
# Begin Source File

SOURCE=.\util.cpp
# End Source File
# Begin Source File

SOURCE=..\game_shared\vector.cpp
# End Source File
# Begin Source File

SOURCE=.\weapons.cpp
# End Source File
# Begin Source File

SOURCE=.\world.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\xen.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\zombie.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\monsters\activity.h
# End Source File
# Begin Source File

SOURCE=.\monsters\activitymap.h
# End Source File
# Begin Source File

SOURCE=..\engine\alert.h
# End Source File
# Begin Source File

SOURCE=.\monsters\animation.h
# End Source File
# Begin Source File

SOURCE=..\engine\anorms.h
# End Source File
# Begin Source File

SOURCE=.\monsters\basemonster.h
# End Source File
# Begin Source File

SOURCE=.\cbase.h
# End Source File
# Begin Source File

SOURCE=..\engine\cdll_int.h
# End Source File
# Begin Source File

SOURCE=.\ropes\CElectrifiedWire.h
# End Source File
# Begin Source File

SOURCE=.\client.h
# End Source File
# Begin Source File

SOURCE=.\physics\collision.h
# End Source File
# Begin Source File

SOURCE=.\ropes\CRope.h
# End Source File
# Begin Source File

SOURCE=.\ropes\CRopeSample.h
# End Source File
# Begin Source File

SOURCE=.\ropes\CRopeSegment.h
# End Source File
# Begin Source File

SOURCE=..\engine\custom.h
# End Source File
# Begin Source File

SOURCE=..\engine\customentity.h
# End Source File
# Begin Source File

SOURCE=.\decals.h
# End Source File
# Begin Source File

SOURCE=.\monsters\defaultai.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\dmg_types.h
# End Source File
# Begin Source File

SOURCE=.\doors.h
# End Source File
# Begin Source File

SOURCE=..\engine\edict.h
# End Source File
# Begin Source File

SOURCE=.\effects.h
# End Source File
# Begin Source File

SOURCE=.\ehandle.h
# End Source File
# Begin Source File

SOURCE=..\engine\eiface.h
# End Source File
# Begin Source File

SOURCE=.\enginecallback.h
# End Source File
# Begin Source File

SOURCE=.\explode.h
# End Source File
# Begin Source File

SOURCE=.\extdll.h
# End Source File
# Begin Source File

SOURCE=.\monsters\flyingmonster.h
# End Source File
# Begin Source File

SOURCE=.\func_break.h
# End Source File
# Begin Source File

SOURCE=.\game.h
# End Source File
# Begin Source File

SOURCE=.\gamerules.h
# End Source File
# Begin Source File

SOURCE=.\monsters\hornet.h
# End Source File
# Begin Source File

SOURCE=.\items.h
# End Source File
# Begin Source File

SOURCE=..\engine\keydefs.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\matrix.h
# End Source File
# Begin Source File

SOURCE=..\engine\menu_int.h
# End Source File
# Begin Source File

SOURCE=.\physics\meshdesc.h
# End Source File
# Begin Source File

SOURCE=.\monsters\monsterevent.h
# End Source File
# Begin Source File

SOURCE=.\monsters\monsters.h
# End Source File
# Begin Source File

SOURCE=.\movelist.h
# End Source File
# Begin Source File

SOURCE=.\monsters\nodes.h
# End Source File
# Begin Source File

SOURCE=.\novodex.h
# End Source File
# Begin Source File

SOURCE=.\physics\NxUserStream.h
# End Source File
# Begin Source File

SOURCE=.\physcallback.h
# End Source File
# Begin Source File

SOURCE=.\physic.h
# End Source File
# Begin Source File

SOURCE=..\engine\physint.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\plane.h
# End Source File
# Begin Source File

SOURCE=.\player.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\plucker.h
# End Source File
# Begin Source File

SOURCE=..\engine\progdefs.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\randomrange.h
# End Source File
# Begin Source File

SOURCE=.\saverestore.h
# End Source File
# Begin Source File

SOURCE=.\monsters\schedule.h
# End Source File
# Begin Source File

SOURCE=.\monsters\scripted.h
# End Source File
# Begin Source File

SOURCE=.\monsters\scriptevent.h
# End Source File
# Begin Source File

SOURCE=..\engine\shake.h
# End Source File
# Begin Source File

SOURCE=.\skill.h
# End Source File
# Begin Source File

SOURCE=.\monsters\soundent.h
# End Source File
# Begin Source File

SOURCE=.\spectator.h
# End Source File
# Begin Source File

SOURCE=..\engine\sprite.h
# End Source File
# Begin Source File

SOURCE=.\monsters\squad.h
# End Source File
# Begin Source File

SOURCE=.\monsters\squadmonster.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\stringlib.h
# End Source File
# Begin Source File

SOURCE=.\strings.h
# End Source File
# Begin Source File

SOURCE=.\monsters\talkmonster.h
# End Source File
# Begin Source File

SOURCE=.\teamplay_gamerules.h
# End Source File
# Begin Source File

SOURCE=.\physics\tracemesh.h
# End Source File
# Begin Source File

SOURCE=.\trains.h
# End Source File
# Begin Source File

SOURCE=.\util.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utlarray.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utllinkedlist.h
# End Source File
# Begin Source File

SOURCE=..\game_shared\utlmemory.h
# End Source File
# Begin Source File

SOURCE=..\engine\warpsin.h
# End Source File
# Begin Source File

SOURCE=.\weapons.h
# End Source File
# End Group
# End Target
# End Project

