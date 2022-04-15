#include "imgui_window_system.h"
#include "imgui.h"

void CImGuiWindowSystem::Initialize()
{
    LinkWindows();
    for (IImGuiWindow *window : m_WindowList) {
        window->Initialize();
    }
}

void CImGuiWindowSystem::VidInitialize()
{
    for (IImGuiWindow *window : m_WindowList) {
        window->VidInitialize();
    }
}

void CImGuiWindowSystem::Terminate()
{
    for (IImGuiWindow *window : m_WindowList) {
        window->Terminate();
    }
}

void CImGuiWindowSystem::NewFrame()
{
    for (IImGuiWindow *window : m_WindowList)
    {
        window->Think();
        if (window->Active()) {
            window->Draw();
        }
    }
}

void CImGuiWindowSystem::AddWindow(IImGuiWindow *window)
{
    m_WindowList.push_back(window);
}

CImGuiWindowSystem::CImGuiWindowSystem()
{
    m_WindowList.clear();
}

bool CImGuiWindowSystem::KeyInput(bool keyDown, int keyNumber, const char *bindName)
{
    for (IImGuiWindow *window : m_WindowList)
    {
        if (window->Active()) {
            return window->HandleKey(keyDown, keyNumber, bindName);
        }
    }
    return true;
}

bool CImGuiWindowSystem::CursorRequired()
{
    for (IImGuiWindow *window : m_WindowList)
    {
        if (window->Active() && window->CursorRequired()) {
            return true;
        }
    }
    return false;
}
