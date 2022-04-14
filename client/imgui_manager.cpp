#include "imgui_manager.h"
#include "gl_imgui_backend.h"
#include "imgui.h"
#include "keydefs.h"
#include "utils.h"

CImGuiManager &g_ImGuiManager = CImGuiManager::GetInstance();

CImGuiManager &CImGuiManager::GetInstance()
{
    static CImGuiManager instance;
    return instance;
}

void CImGuiManager::Initialize()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    LoadFonts();
    ApplyStyles();
    SetupKeyboardMapping();
    ImGui_ImplOpenGL3_Init(nullptr);
}

void CImGuiManager::Terminate()
{
    ImGui_ImplOpenGL3_Shutdown();
}

void CImGuiManager::NewFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    UpdateMouse();
    ImGui::NewFrame();

    bool show_demo_window = true;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/*
    if return true, key input event will be reported to engine
    else, key will be ignored by engine but handled by ImGui
*/
bool CImGuiManager::KeyInput(bool keyDown, int keyNumber, const char *bindName)
{
    HandleKeyInput(keyDown, keyNumber);
    return false;
}

void CImGuiManager::LoadFonts()
{
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
}

void CImGuiManager::ApplyStyles()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();
}

void CImGuiManager::UpdateMouse()
{
    int mx, my;
    ImGuiIO &io = ImGui::GetIO();
    gEngfuncs.GetMousePosition(&mx, &my);

    io.MouseDown[0] = m_MouseButtonsState.left;
    io.MouseDown[1] = m_MouseButtonsState.right;
    io.MouseDown[2] = m_MouseButtonsState.middle;
    io.MousePos = ImVec2((float)mx, (float)my);
}

void CImGuiManager::HandleKeyInput(bool keyDown, int keyNumber)
{
    ImGuiIO &io = ImGui::GetIO();
    if (keyNumber == K_SHIFT) {
        io.KeyShift = keyDown;
    }
    else if (keyNumber == K_CTRL) {
        io.KeyCtrl = keyDown;
    }
    else if (keyNumber == K_ALT) {
        io.KeyAlt = keyDown;
    }
    else if (keyNumber == K_WIN) {
        io.KeySuper = keyDown;
    }
    else if (HandleMouseInput(keyDown, keyNumber)) {
        return;
    }
    io.AddKeyEvent(m_KeysMapping[keyNumber], keyDown);
}

bool CImGuiManager::HandleMouseInput(bool keyDown, int keyNumber)
{
    ImGuiIO &io = ImGui::GetIO();
    if (keyNumber == K_MOUSE1) {
        m_MouseButtonsState.left = keyDown;
        return true;
    }
    else if (keyNumber == K_MOUSE2) {
        m_MouseButtonsState.right = keyDown;
        return true;
    }
    else if (keyNumber == K_MOUSE3) {
        m_MouseButtonsState.middle = keyDown;
        return true;
    }
    else if (keyNumber == K_MWHEELDOWN && keyDown) {
        io.MouseWheel -= 1;
        return true;
    }
    else if (keyNumber == K_MWHEELUP && keyDown) {
        io.MouseWheel += 1;
        return true;
    }
    return false;
}

void CImGuiManager::SetupKeyboardMapping()
{
    m_KeysMapping.insert({ K_TAB, ImGuiKey_Tab });
    m_KeysMapping.insert({ K_LEFTARROW, ImGuiKey_LeftArrow });
    m_KeysMapping.insert({ K_RIGHTARROW, ImGuiKey_RightArrow });
    m_KeysMapping.insert({ K_UPARROW, ImGuiKey_UpArrow });
    m_KeysMapping.insert({ K_DOWNARROW, ImGuiKey_DownArrow });
    m_KeysMapping.insert({ K_PGUP, ImGuiKey_PageUp });
    m_KeysMapping.insert({ K_PGDN, ImGuiKey_PageDown });
    m_KeysMapping.insert({ K_HOME, ImGuiKey_Home });
    m_KeysMapping.insert({ K_END, ImGuiKey_End });
    m_KeysMapping.insert({ K_INS, ImGuiKey_Insert });
    m_KeysMapping.insert({ K_DEL, ImGuiKey_Delete });
    m_KeysMapping.insert({ K_PAUSE, ImGuiKey_Pause });
    m_KeysMapping.insert({ K_CAPSLOCK, ImGuiKey_CapsLock });
    m_KeysMapping.insert({ K_BACKSPACE, ImGuiKey_Backspace });
    m_KeysMapping.insert({ K_WIN, ImGuiKey_LeftSuper });
    m_KeysMapping.insert({ K_SPACE, ImGuiKey_Space });
    m_KeysMapping.insert({ K_ENTER, ImGuiKey_Enter });
    m_KeysMapping.insert({ K_ESCAPE, ImGuiKey_Escape });
    m_KeysMapping.insert({ K_CTRL, ImGuiKey_LeftCtrl });
    m_KeysMapping.insert({ K_ALT, ImGuiKey_LeftAlt });
    m_KeysMapping.insert({ K_SHIFT, ImGuiKey_LeftShift });
    m_KeysMapping.insert({ K_KP_ENTER, ImGuiKey_KeypadEnter });
    m_KeysMapping.insert({ K_KP_NUMLOCK, ImGuiKey_NumLock });
    m_KeysMapping.insert({ K_KP_SLASH, ImGuiKey_KeypadDivide });
    m_KeysMapping.insert({ K_KP_MUL, ImGuiKey_KeypadMultiply });
    m_KeysMapping.insert({ K_KP_MINUS, ImGuiKey_KeypadSubtract });
    m_KeysMapping.insert({ K_KP_PLUS, ImGuiKey_KeypadAdd });
    m_KeysMapping.insert({ K_KP_INS, ImGuiKey_Keypad0 });
    m_KeysMapping.insert({ K_KP_END, ImGuiKey_Keypad1 });
    m_KeysMapping.insert({ K_KP_DOWNARROW, ImGuiKey_Keypad2 });
    m_KeysMapping.insert({ K_KP_PGDN, ImGuiKey_Keypad3 });
    m_KeysMapping.insert({ K_KP_LEFTARROW, ImGuiKey_Keypad4 });
    m_KeysMapping.insert({ K_KP_5, ImGuiKey_Keypad5 });
    m_KeysMapping.insert({ K_KP_RIGHTARROW, ImGuiKey_Keypad6 });
    m_KeysMapping.insert({ K_KP_HOME, ImGuiKey_Keypad7 });
    m_KeysMapping.insert({ K_KP_UPARROW, ImGuiKey_Keypad8 });
    m_KeysMapping.insert({ K_KP_PGUP, ImGuiKey_Keypad9 });
    m_KeysMapping.insert({ K_KP_INS, ImGuiKey_Keypad0 });
    m_KeysMapping.insert({ K_KP_DEL, ImGuiKey_KeypadDecimal });

    // special symbols
    m_KeysMapping.insert({ '`', ImGuiKey_GraveAccent });
    m_KeysMapping.insert({ '[', ImGuiKey_LeftBracket });
    m_KeysMapping.insert({ ']', ImGuiKey_RightBracket });
    m_KeysMapping.insert({ '\'', ImGuiKey_Apostrophe });
    m_KeysMapping.insert({ ';', ImGuiKey_Semicolon });
    m_KeysMapping.insert({ '.', ImGuiKey_Period });
    m_KeysMapping.insert({ ',', ImGuiKey_Comma });
    m_KeysMapping.insert({ '-', ImGuiKey_Minus });
    m_KeysMapping.insert({ '=', ImGuiKey_Equal });
    m_KeysMapping.insert({ '/', ImGuiKey_Slash });
    m_KeysMapping.insert({ '\\', ImGuiKey_Backslash });

    // alphabet characters
    for (int i = 0; i < 26; ++i) {
        m_KeysMapping.insert({ 'a' + i, ImGuiKey_A + i });
    }

    // keyboard numbers
    m_KeysMapping.insert({ '0', ImGuiKey_0 });
    for (int i = 0; i < 9; ++i) {
        m_KeysMapping.insert({ '1' + i, ImGuiKey_1 + i });
    }

    // F-buttons
    for (int i = 0; i < 12; ++i) {
        m_KeysMapping.insert({ K_F1 + i, ImGuiKey_F1 + i });
    }
}
