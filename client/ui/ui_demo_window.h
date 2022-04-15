#include "imgui_window.h"

class CImGuiDemoWindow : public IImGuiWindow
{
public:
    void Initialize();
    void VidInitialize();
    void Terminate();
    void Think();
    void Draw();
    bool Active();
    bool CursorRequired();
    bool HandleKey(bool keyDown, int keyNumber, const char *bindName);

private:
    static void CmdShowDemoWindow();
    static bool m_ShowWindow;
};
