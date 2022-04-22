#include "imgui_window_system.h"
#include "ui_demo_window.h"
#include "ui_material_editor.h"

void CImGuiWindowSystem::LinkWindows()
{
    static CImGuiDemoWindow demoWindow;
    static CImGuiMaterialEditor materialEditor;
    AddWindow(&demoWindow);
    AddWindow(&materialEditor);
}
