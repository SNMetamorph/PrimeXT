#include "vgui_support_int.h"
#include "exportdef.h"
#include "imgui_manager.h"

vguiapi_t *g_VguiApiFuncs;

// interface functions stubs
static void VGUI_Startup(int w, int h) {}
static void VGUI_Shutdown() {}
static void *VGUI_GetPanel() { return nullptr; }
static void VGUI_Paint() {}
static void VGUI_Mouse(VGUI_MouseAction action, int code) {}
static void VGUI_MouseMove(int x, int y) {}
static void VGUI_Key(VGUI_KeyAction action, VGUI_KeyCode code) {}

extern "C" void DLLEXPORT InitVGUISupportAPI(vguiapi_t *api)
{
	g_VguiApiFuncs = api;
	api->Startup = VGUI_Startup;
	api->Shutdown = VGUI_Shutdown;
	api->GetPanel = VGUI_GetPanel;
	api->Paint = VGUI_Paint;
	api->Mouse = VGUI_Mouse;
	api->MouseMove = VGUI_MouseMove;
	api->Key = VGUI_Key;
	api->TextInput = CImGuiManager::TextInputCallback;
}
