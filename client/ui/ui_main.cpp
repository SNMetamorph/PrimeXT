#include "imgui_window_system.h"
#include "ui_demo_window.h"
#include "ui_material_editor.h"
#include "ui_postfx_menu.h"

void CImGuiWindowSystem::LinkWindows()
{
    static CImGuiDemoWindow demoWindow;
    static CImGuiMaterialEditor materialEditor;
    static CImGuiPostFxMenu postFxMenu;
    AddWindow(&demoWindow);
    AddWindow(&materialEditor);
    AddWindow(&postFxMenu);
}
