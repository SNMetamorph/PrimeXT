#include "imgui_manager.h"
#include "imgui.h"
#include "keydefs.h"
#include "utils.h"
#include "enginecallback.h"
#include "vgui_support_int.h"
#include "gl_imgui_backend.h"

CImGuiManager &g_ImGuiManager = CImGuiManager::GetInstance();

CImGuiManager &CImGuiManager::GetInstance()
{
    static CImGuiManager instance;
    return instance;
}

void CImGuiManager::Initialize()
{
    ImGui::CreateContext();
    SetupConfig();
    LoadFonts();
    ApplyStyles();
    SetupKeyboardMapping();
    SetupCursorMapping();
    m_pBackend->Init(nullptr);
    m_WindowSystem.Initialize();
}

void CImGuiManager::VidInitialize()
{
    m_WindowSystem.VidInitialize();
}

void CImGuiManager::Terminate()
{
    m_WindowSystem.Terminate();
    m_pBackend->Shutdown();
}

void CImGuiManager::NewFrame()
{
    CheckVguiApiPresence();
    m_pBackend->NewFrame();
    UpdateMouseState();
    UpdateKeyModifiers();
    ImGui::NewFrame();
    m_WindowSystem.NewFrame();
    ImGui::Render();
    m_pBackend->RenderDrawData(ImGui::GetDrawData());
}

/*
    if return true, key input event will be reported to engine
    else, key will be ignored by engine but handled by ImGui
*/
bool CImGuiManager::KeyInput(bool keyDown, int keyNumber, const char *bindName)
{
    if (m_WindowSystem.CursorRequired()) {
        HandleKeyInput(keyDown, keyNumber);
    }
    return m_WindowSystem.KeyInput(keyDown, keyNumber, bindName);
}

CImGuiManager::CImGuiManager()
{
    m_pBackend = std::make_unique<CImGuiBackend>();
}

CImGuiManager::~CImGuiManager()
{
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

void CImGuiManager::UpdateMouseState()
{
    int mx, my;
    ImGuiIO &io = ImGui::GetIO();
    gEngfuncs.GetMousePosition(&mx, &my);

    io.MouseDown[0] = m_MouseButtonsState.left;
    io.MouseDown[1] = m_MouseButtonsState.right;
    io.MouseDown[2] = m_MouseButtonsState.middle;
    io.MousePos = ImVec2((float)mx, (float)my);
    UpdateCursorState();
}

void CImGuiManager::UpdateCursorState()
{
    ImGuiIO &io = ImGui::GetIO();
    bool cursorRequired = m_WindowSystem.CursorRequired();
    if (cursorRequired) 
    {
        // it's better to call CursorSelect only when cursor hidden but required
        // but since we can't find out is cursor hidden or not,
        // all we can do is just call this function every time
        if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) {
            g_VguiApiFuncs->CursorSelect(dc_arrow);
        }
        else
        {
            ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
            g_VguiApiFuncs->CursorSelect(m_CursorMapping[imgui_cursor]);
        }
    }
    else if (cursorRequired != m_bWasCursorRequired) {
        g_VguiApiFuncs->CursorSelect(dc_none);
    }
    m_bWasCursorRequired = cursorRequired;
}

void CImGuiManager::UpdateKeyModifiers()
{
    ImGuiIO &io = ImGui::GetIO();
    key_modifier_t modFlags = g_VguiApiFuncs->GetKeyModifiers();
    io.KeyShift = modFlags & (KeyModifier_LeftShift | KeyModifier_RightShift);
    io.KeyCtrl = modFlags & (KeyModifier_LeftCtrl | KeyModifier_RightCtrl);
    io.KeyAlt = modFlags & (KeyModifier_LeftAlt | KeyModifier_RightAlt);
    io.KeySuper = modFlags & (KeyModifier_LeftSuper | KeyModifier_RightSuper);
}

void CImGuiManager::HandleKeyInput(bool keyDown, int keyNumber)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!HandleMouseInput(keyDown, keyNumber)) {
        io.AddKeyEvent(m_KeysMapping[keyNumber], keyDown);
    }
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

void CImGuiManager::SetupConfig()
{
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.SetClipboardTextFn = CImGuiManager::SetClipboardText;
    io.GetClipboardTextFn = CImGuiManager::GetClipboardText;
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

void CImGuiManager::SetupCursorMapping()
{
    m_CursorMapping.insert({ ImGuiMouseCursor_Arrow, dc_arrow });
    m_CursorMapping.insert({ ImGuiMouseCursor_TextInput, dc_ibeam });
    m_CursorMapping.insert({ ImGuiMouseCursor_ResizeAll,  dc_sizeall });
    m_CursorMapping.insert({ ImGuiMouseCursor_ResizeNS, dc_sizens });
    m_CursorMapping.insert({ ImGuiMouseCursor_ResizeEW, dc_sizewe });
    m_CursorMapping.insert({ ImGuiMouseCursor_ResizeNESW, dc_sizenesw });
    m_CursorMapping.insert({ ImGuiMouseCursor_ResizeNWSE, dc_sizenwse });
    m_CursorMapping.insert({ ImGuiMouseCursor_Hand, dc_hand });
    m_CursorMapping.insert({ ImGuiMouseCursor_NotAllowed, dc_no });
}

void CImGuiManager::CheckVguiApiPresence()
{
    if (!g_VguiApiFuncs) {
        HOST_ERROR("VGUI API interface not available! Make sure that you use last version of Xash3D FWGS engine.\n");
    }
}

const char *CImGuiManager::GetClipboardText(void *userData)
{
    static std::string textBuffer;
    int textLength = g_VguiApiFuncs->GetClipboardText(nullptr, 0) + 1;
    if (textBuffer.size() < textLength) {
        textBuffer.assign(textLength, '\0');
    }
    g_VguiApiFuncs->GetClipboardText((char *)textBuffer.data(), textBuffer.size());
    return textBuffer.c_str();
}

void CImGuiManager::SetClipboardText(void *userData, const char *text)
{
    g_VguiApiFuncs->SetClipboardText(text);
}

void CImGuiManager::TextInputCallback(const char *text)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddInputCharactersUTF8(text);
}
