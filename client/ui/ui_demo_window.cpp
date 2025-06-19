#include "ui_demo_window.h"
#include "imgui.h"
#include "hud.h"

bool CImGuiDemoWindow::m_ShowWindow = false;

void CImGuiDemoWindow::Initialize()
{
    gEngfuncs.pfnAddCommand("ui_imgui_demo", CImGuiDemoWindow::CmdShowDemoWindow);
}

void CImGuiDemoWindow::VidInitialize()
{
}

void CImGuiDemoWindow::Terminate()
{
}

void CImGuiDemoWindow::Think()
{
}

void CImGuiDemoWindow::Draw()
{
    if (m_ShowWindow) {
        ImGui::ShowDemoWindow(&m_ShowWindow);
    }
}

bool CImGuiDemoWindow::Active()
{
    return m_ShowWindow;
}

bool CImGuiDemoWindow::CursorRequired()
{
    return true;
}

bool CImGuiDemoWindow::HandleKey(bool keyDown, int keyNumber, const char *bindName)
{
    // block all incoming key events
    return false;
}

void CImGuiDemoWindow::CmdShowDemoWindow()
{
    m_ShowWindow = true;
}
