#pragma once
#include "imgui_window_system.h"
#include "cursor_type.h"
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
    void UpdateCursorState();
    void HandleKeyInput(bool keyDown, int keyNumber);
    bool HandleMouseInput(bool keyDown, int keyNumber);
    void SetupConfig();
    void SetupKeyboardMapping();
    void SetupCursorMapping();
    static const char *GetClipboardText(void *userData);
    static void SetClipboardText(void *userData, const char *text);
    
    MouseButtonsState m_MouseButtonsState;
    std::map<int, int> m_KeysMapping;
    std::map<int, cursor_type_t> m_CursorMapping;
};
extern CImGuiManager &g_ImGuiManager;
