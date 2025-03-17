/*
game_event_manager.cpp - class that responsible for registering game events handlers
Copyright (C) 2024 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "game_event_manager.h"
#include "hud.h"
#include "utils.h"
#include "event_api.h"
#include "event_args.h"
#include "glock_fire_event.h"
#include "crossbow_fire_event.h"
#include "python_fire_event.h"
#include "mp5_fire_event.h"
#include "shotgun_fire_event.h"
#include "crowbar_swing_event.h"
#include "tripmine_deploy_event.h"
#include "snark_throw_event.h"
#include "hornetgun_fire_event.h"
#include "rpg_fire_event.h"
#include "egon_stop_event.h"
#include "egon_fire_event.h"
#include "gauss_fire_event.h"
#include "gauss_spin_event.h"

CGameEventManager::CGameEventManager()
{
	RegisterGlockEvents();
	RegisterCrossbowEvents();
	RegisterPythonEvents();
	RegisterMP5Events();
	RegisterShotgunEvents();
	RegisterCrowbarEvents();
	RegisterTripmineEvents();
	RegisterSnarkEvents();
	RegisterHornetgunEvents();
	RegisterRPGEvents();
	RegisterEgonEvents();
	RegisterGaussEvents();
}

void CGameEventManager::RegisterGlockEvents()
{
	gEngfuncs.pfnHookEvent("events/glock1.sc", [](event_args_s *args) {
		CGlockFireEvent event(args);
		event.Execute();
	});
	gEngfuncs.pfnHookEvent("events/glock2.sc", [](event_args_s *args) {
		CGlockFireEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterCrossbowEvents()
{
	gEngfuncs.pfnHookEvent("events/crossbow1.sc", [](event_args_s *args) {
		CCrossbowFireEvent event(args);
		event.Execute();
	});
	gEngfuncs.pfnHookEvent("events/crossbow2.sc", [](event_args_s *args) {
		CCrossbowFireEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterPythonEvents()
{
	gEngfuncs.pfnHookEvent("events/python.sc", [](event_args_s *args) {
		CPythonFireEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterMP5Events()
{
	gEngfuncs.pfnHookEvent("events/mp5.sc", [](event_args_s *args) {
		CMP5FireEvent event(args);
		event.Execute(false);
	});
	gEngfuncs.pfnHookEvent("events/mp52.sc", [](event_args_s *args) {
		CMP5FireEvent event(args);
		event.Execute(true);
	});
}

void CGameEventManager::RegisterShotgunEvents()
{
	gEngfuncs.pfnHookEvent("events/shotgun1.sc", [](event_args_s *args) {
		CShotgunFireEvent event(args);
		event.Execute(true);
	});
	gEngfuncs.pfnHookEvent("events/shotgun2.sc", [](event_args_s *args) {
		CShotgunFireEvent event(args);
		event.Execute(false);
	});
}

void CGameEventManager::RegisterCrowbarEvents()
{
	gEngfuncs.pfnHookEvent("events/crowbar.sc", [](event_args_s *args) {
		CCrowbarSwingEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterTripmineEvents()
{
	gEngfuncs.pfnHookEvent("events/tripfire.sc", [](event_args_s *args) {
		CTripmineDeployEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterSnarkEvents()
{
	gEngfuncs.pfnHookEvent("events/snarkfire.sc", [](event_args_s *args) {
		CSnarkThrowEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterHornetgunEvents()
{
	gEngfuncs.pfnHookEvent("events/firehornet.sc", [](event_args_s *args) {
		CHornetgunFireEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterRPGEvents()
{
	gEngfuncs.pfnHookEvent("events/rpg.sc", [](event_args_s *args) {
		CRpgFireEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterEgonEvents()
{
	gEngfuncs.pfnHookEvent("events/egon_fire.sc", [](event_args_s *args) {
		CEgonFireEvent event(args);
		event.Execute();
	});
	gEngfuncs.pfnHookEvent("events/egon_stop.sc", [](event_args_s *args) {
		CEgonStopEvent event(args);
		event.Execute();
	});
}

void CGameEventManager::RegisterGaussEvents()
{
	gEngfuncs.pfnHookEvent("events/gauss.sc", [](event_args_s *args) {
		CGaussFireEvent event(args);
		event.Execute();
	});
	gEngfuncs.pfnHookEvent("events/gaussspin.sc", [](event_args_s *args) {
		CGaussSpinEvent event(args);
		event.Execute();
	});
}
