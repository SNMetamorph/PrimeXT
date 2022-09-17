#include "imgui_window.h"

class CImGuiPostFxMenu : public IImGuiWindow
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
    class ParametersCache
    {
    public:
        bool enabled;
        float brightness;
        float saturation;
        float contrast;
        float redLevel;
        float greenLevel;
        float blueLevel;
        float vignetteScale;
        float filmGrainScale;
    };
    
    void LoadParameters();
    void StoreParameters();

    static void ShowMaterialEditor();
    static bool m_Visible;
    ParametersCache m_ParametersCache;
};
