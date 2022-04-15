#pragma once
#include "imgui_window.h"
#include <vector>

class CImGuiWindowSystem
{
    friend class CImGuiManager;
public:
    void AddWindow(IImGuiWindow *window);

private:
    CImGuiWindowSystem();
    ~CImGuiWindowSystem() {};
    CImGuiWindowSystem(const CImGuiWindowSystem &) = delete;
    CImGuiWindowSystem &operator=(const CImGuiWindowSystem &) = delete;

    void LinkWindows();
    void Initialize();
    void VidInitialize();
    void Terminate();
    void NewFrame();
    bool KeyInput(bool keyDown, int keyNumber, const char *bindName);
    bool CursorRequired();

    std::vector<IImGuiWindow*> m_WindowList;
};
