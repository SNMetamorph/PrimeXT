#pragma once

class IImGuiWindow
{
public:
    virtual void Initialize() = 0;
    virtual void VidInitialize() = 0;
    virtual void Terminate() = 0;
    virtual void Think() = 0;
    virtual void Draw() = 0;
    virtual bool Active() = 0;
    virtual bool CursorRequired() = 0;
    virtual bool HandleKey(bool keyDown, int keyNumber, const char *bindName) = 0;
};
