#include "ui_postfx_menu.h"
#include "imgui.h"
#include "hud.h"
#include "gl_cvars.h"

bool CImGuiPostFxMenu::m_Visible = false;

void CImGuiPostFxMenu::Initialize()
{
    gEngfuncs.pfnAddCommand("r_postfx_showmenu", CImGuiPostFxMenu::ShowMaterialEditor);
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
    ImGui::Begin("Postprocessing Menu", &m_Visible, windowFlags);
    if (ImGui::Checkbox("Enable", &m_ParametersCache.enabled)) {
        StoreParameters();
    };
    if (ImGui::SliderFloat("Brightness", &m_ParametersCache.brightness, 0.0f, 1.0f)) {
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
    if (ImGui::Button("Reset")) {
        ResetParameters();
    }
    ImGui::End();
}

bool CImGuiPostFxMenu::Active()
{
    return m_Visible;
}

bool CImGuiPostFxMenu::CursorRequired()
{
    return true;
}

bool CImGuiPostFxMenu::HandleKey(bool keyDown, int keyNumber, const char *bindName)
{
    // block all incoming key events
    return false;
}

void CImGuiPostFxMenu::LoadParameters()
{
    m_ParametersCache.enabled = CVAR_TO_BOOL(r_postfx_enable);
    m_ParametersCache.brightness = r_postfx_brightness->value;
    m_ParametersCache.saturation = r_postfx_saturation->value;
    m_ParametersCache.contrast = r_postfx_contrast->value;
    m_ParametersCache.redLevel = r_postfx_redlevel->value;
    m_ParametersCache.greenLevel = r_postfx_greenlevel->value;
    m_ParametersCache.blueLevel = r_postfx_bluelevel->value;
    m_ParametersCache.vignetteScale = r_postfx_vignettescale->value;
    m_ParametersCache.filmGrainScale = r_postfx_filmgrainscale->value;
}

void CImGuiPostFxMenu::StoreParameters()
{
    r_postfx_enable->value = m_ParametersCache.enabled ? 1.0f : 0.0f;
    r_postfx_brightness->value = m_ParametersCache.brightness;
    r_postfx_saturation->value = m_ParametersCache.saturation;
    r_postfx_contrast->value = m_ParametersCache.contrast;
    r_postfx_redlevel->value = m_ParametersCache.redLevel;
    r_postfx_greenlevel->value = m_ParametersCache.greenLevel;
    r_postfx_bluelevel->value = m_ParametersCache.blueLevel;
    r_postfx_vignettescale->value = m_ParametersCache.vignetteScale;
    r_postfx_filmgrainscale->value = m_ParametersCache.filmGrainScale;
}

void CImGuiPostFxMenu::ResetParameters()
{
    m_ParametersCache.brightness = 0.0f;
    m_ParametersCache.saturation = 1.0f;
    m_ParametersCache.contrast = 1.0f;
    m_ParametersCache.redLevel = 1.0f;
    m_ParametersCache.greenLevel = 1.0f;
    m_ParametersCache.blueLevel = 1.0f;
    m_ParametersCache.vignetteScale = 0.0f;
    m_ParametersCache.filmGrainScale = 0.0f;
    StoreParameters();
}

void CImGuiPostFxMenu::ShowMaterialEditor()
{
    m_Visible = !m_Visible;
}
