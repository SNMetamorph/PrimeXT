#include "imgui_window.h"
#include "vector.h"

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
        float colorAccentScale;
        Vector accentColor;
    };
    
    void BeginColorPicker();
    void LoadParameters();
    void StoreParameters();
    void ResetParameters();

    static void ShowMenu();
    static bool m_bVisible;
    ParametersCache m_ParametersCache;
};
