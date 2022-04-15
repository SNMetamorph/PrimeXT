#include "imgui_window_system.h"
#include "ui_demo_window.h"

void CImGuiWindowSystem::LinkWindows()
{
    static CImGuiDemoWindow demoWindow;
    AddWindow(&demoWindow);
}
