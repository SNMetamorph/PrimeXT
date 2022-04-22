#include "imgui_window.h"

class CImGuiMaterialEditor : public IImGuiWindow
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
    static void ShowMaterialEditor();
    static bool m_Visible;
};
