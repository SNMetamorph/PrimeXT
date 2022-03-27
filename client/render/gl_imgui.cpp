#include "gl_imgui.h"
#include "gl_imgui_backend.h"
#include "imgui.h"

void GL_InitImGui()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGui_ImplOpenGL3_Init(nullptr);
}

void GL_TerminateImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
}

void GL_RenderFrameImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    bool show_demo_window = true;
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
