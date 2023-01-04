#include "ui_postfx_menu.h"
#include "postfx_controller.h"
#include "imgui.h"
#include "hud.h"
#include "gl_cvars.h"
#include "keydefs.h"

bool CImGuiPostFxMenu::m_bVisible = false;

void CImGuiPostFxMenu::Initialize()
{
    gEngfuncs.pfnAddCommand("r_postfx_showmenu", CImGuiPostFxMenu::ShowMenu);
}

void CImGuiPostFxMenu::VidInitialize()
{
}

void CImGuiPostFxMenu::Terminate()
{
}

void CImGuiPostFxMenu::Think()
{
}

void CImGuiPostFxMenu::Draw()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize;
    LoadParameters();
    ImGui::Begin("Postprocessing Menu", &m_bVisible, windowFlags);
    if (ImGui::Checkbox("Enable", &m_ParametersCache.enabled)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Brightness", &m_ParametersCache.brightness, -1.0f, 1.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Saturation", &m_ParametersCache.saturation, 0.0f, 10.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Contrast", &m_ParametersCache.contrast, 0.0f, 5.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Red Level", &m_ParametersCache.redLevel, 0.0f, 1.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Green Level", &m_ParametersCache.greenLevel, 0.0f, 1.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Blue Level", &m_ParametersCache.blueLevel, 0.0f, 1.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Vignette Scale", &m_ParametersCache.vignetteScale, 0.0f, 1.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Film Grain Scale", &m_ParametersCache.filmGrainScale, 0.0f, 1.0f)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Color Accent Scale", &m_ParametersCache.colorAccentScale, 0.0f, 1.0f)) {
        StoreParameters();
    };
    BeginColorPicker();
    if (ImGui::Button("Reset")) {
        ResetParameters();
    }
    ImGui::End();
}

bool CImGuiPostFxMenu::Active()
{
    return m_bVisible;
}

bool CImGuiPostFxMenu::CursorRequired()
{
    return true;
}

bool CImGuiPostFxMenu::HandleKey(bool keyDown, int keyNumber, const char *bindName)
{
    if (keyNumber == K_ESCAPE)
    {
        m_bVisible = false;
    }
    return false;
}

void CImGuiPostFxMenu::BeginColorPicker()
{
    const Vector &accentColor = m_ParametersCache.accentColor;
    ImVec4 color = ImVec4(accentColor.x, accentColor.y, accentColor.z, 1.0f);
    const ImGuiColorEditFlags flags = (
        ImGuiColorEditFlags_DisplayRGB | 
        ImGuiColorEditFlags_InputRGB |
        ImGuiColorEditFlags_PickerHueWheel
    );
    
    if (ImGui::ColorEdit3("Accent Color", (float*)&color, flags))
    {
        m_ParametersCache.accentColor.x = color.x;
        m_ParametersCache.accentColor.y = color.y;
        m_ParametersCache.accentColor.z = color.z;
        StoreParameters();
    }
}

void CImGuiPostFxMenu::LoadParameters()
{
    CPostFxParameters state = g_PostFxController.GetState();
    m_ParametersCache.enabled = CVAR_TO_BOOL(r_postfx_enable);
    m_ParametersCache.brightness = state.GetBrightness();
    m_ParametersCache.saturation = state.GetSaturation();
    m_ParametersCache.contrast = state.GetContrast();
    m_ParametersCache.redLevel = state.GetRedLevel();
    m_ParametersCache.greenLevel = state.GetGreenLevel();
    m_ParametersCache.blueLevel = state.GetBlueLevel();
    m_ParametersCache.vignetteScale = state.GetVignetteScale();
    m_ParametersCache.filmGrainScale = state.GetFilmGrainScale();
    m_ParametersCache.accentColor = state.GetAccentColor();
}

void CImGuiPostFxMenu::StoreParameters()
{
    CPostFxParameters state;
    r_postfx_enable->value = m_ParametersCache.enabled ? 1.0f : 0.0f;
    state.SetBrightness(m_ParametersCache.brightness);
    state.SetSaturation(m_ParametersCache.saturation);
    state.SetContrast(m_ParametersCache.contrast);
    state.SetRedLevel(m_ParametersCache.redLevel);
    state.SetGreenLevel(m_ParametersCache.greenLevel);
    state.SetBlueLevel(m_ParametersCache.blueLevel);
    state.SetVignetteScale(m_ParametersCache.vignetteScale);
    state.SetFilmGrainScale(m_ParametersCache.filmGrainScale);
    state.SetColorAccentScale(m_ParametersCache.colorAccentScale);
    state.SetAccentColor(m_ParametersCache.accentColor);
    g_PostFxController.UpdateState(state, false);
}

void CImGuiPostFxMenu::ResetParameters()
{
    g_PostFxController.UpdateState(CPostFxParameters::Defaults(), false);
}

void CImGuiPostFxMenu::ShowMenu()
{
    m_bVisible = !m_bVisible;
}
