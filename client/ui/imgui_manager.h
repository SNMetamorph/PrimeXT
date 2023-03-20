#pragma once
#include "imgui_window_system.h"
#include "cursor_type.h"
#include <map>
#include <memory>

// use forward declaration to avoid exposing ImGui headers to global namespace
class CImGuiBackend;

class CImGuiManager
{
public:
    static CImGuiManager &GetInstance();

    void Initialize();
    void VidInitialize();
    void Terminate();
    void NewFrame();
    bool KeyInput(bool keyDown, int keyNumber, const char *bindName);
    static void TextInputCallback(const char *text);

private:
    struct MouseButtonsState
    {
        bool left = false;
        bool middle = false;
        bool right = false;
    };

    CImGuiManager();
    ~CImGuiManager();
    CImGuiManager(const CImGuiManager &) = delete;
    CImGuiManager &operator=(const CImGuiManager &) = delete;

    void LoadFonts();
    void ApplyStyles();
    void UpdateMouseState();
    void UpdateCursorState();
    void UpdateTextInputState();
    void UpdateKeyModifiers();
    void HandleKeyInput(bool keyDown, int keyNumber);
    bool HandleMouseInput(bool keyDown, int keyNumber);
    void SetupConfig();
    void SetupKeyboardMapping();
    void SetupCursorMapping();
    void CheckVguiApiPresence();
    static const char *GetClipboardText(void *userData);
    static void SetClipboardText(void *userData, const char *text);

    bool m_bWasCursorRequired = false;
    MouseButtonsState m_MouseButtonsState;
    std::map<int, int> m_KeysMapping;
    std::map<int, VGUI_DefaultCursor> m_CursorMapping;
    std::unique_ptr<CImGuiBackend> m_pBackend;
    CImGuiWindowSystem m_WindowSystem;
};
extern CImGuiManager &g_ImGuiManager;
