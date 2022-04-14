#pragma once
#include <map>

class CImGuiManager
{
public:
    static CImGuiManager &GetInstance();

    void Initialize();
    void Terminate();
    void NewFrame();
    bool KeyInput(bool keyDown, int keyNumber, const char *bindName);

private:
    CImGuiManager() {};
    ~CImGuiManager() {};
    CImGuiManager(const CImGuiManager &) = delete;
    CImGuiManager &operator=(const CImGuiManager &) = delete;

    void SetupKeyboardMapping();
    
    std::map<int, int> m_KeysMapping;
};
extern CImGuiManager &g_ImGuiManager;
