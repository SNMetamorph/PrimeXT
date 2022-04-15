#pragma once
#include "imgui_window_system.h"
#include <map>

class CImGuiManager
{
public:
    static CImGuiManager &GetInstance();

    void Initialize();
    void VidInitialize();
    void Terminate();
    void NewFrame();
    bool KeyInput(bool keyDown, int keyNumber, const char *bindName);

    CImGuiWindowSystem m_WindowSystem;
private:
    struct MouseButtonsState
    {
        bool left = false;
        bool middle = false;
        bool right = false;
    };

    CImGuiManager() {};
    ~CImGuiManager() {};
    CImGuiManager(const CImGuiManager &) = delete;
    CImGuiManager &operator=(const CImGuiManager &) = delete;

    void LoadFonts();
    void ApplyStyles();
    void UpdateMouseState();
    void HandleKeyInput(bool keyDown, int keyNumber);
    bool HandleMouseInput(bool keyDown, int keyNumber);
    void SetupKeyboardMapping();
    
    MouseButtonsState m_MouseButtonsState;
    std::map<int, int> m_KeysMapping;
};
extern CImGuiManager &g_ImGuiManager;
