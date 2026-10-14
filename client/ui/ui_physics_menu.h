#pragma once
#include "imgui_window.h"

class CImGuiPhysicsMenu : public IImGuiWindow
{
public:
    void Initialize();
    void VidInitialize() {}
    void Terminate();
    void Think() {}
    void Draw();
    bool Active();
    bool CursorRequired() { return true; }
    bool HandleKey(bool keyDown, int keyNumber, const char *bindName);
private:
    static void ShowMenu();
    static bool m_bVisible;
};
