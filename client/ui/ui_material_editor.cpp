#include "ui_material_editor.h"
#include "imgui.h"
#include "hud.h"

bool CImGuiMaterialEditor::m_Visible = false;

void CImGuiMaterialEditor::Initialize()
{
    gEngfuncs.pfnAddCommand("mat_editor", CImGuiMaterialEditor::ShowMaterialEditor);
}

void CImGuiMaterialEditor::VidInitialize()
{
}

void CImGuiMaterialEditor::Terminate()
{
}

void CImGuiMaterialEditor::Think()
{
}

void CImGuiMaterialEditor::Draw()
{
    ImGui::Begin("Material Editor", &m_Visible);
    ImGui::Text("Somewhere it will be implemented :)");
    ImGui::End();
}

bool CImGuiMaterialEditor::Active()
{
    return m_Visible;
}

bool CImGuiMaterialEditor::CursorRequired()
{
    return true;
}

bool CImGuiMaterialEditor::HandleKey(bool keyDown, int keyNumber, const char *bindName)
{
    // block all incoming key events
    return false;
}

void CImGuiMaterialEditor::ShowMaterialEditor()
{
    m_Visible = !m_Visible;
}
