#pragma once

#include "imgui/imgui.h"

namespace ElysiaEditor
{
    bool ImGUI_Init(void *hwnd);
    void ImGUI_Shutdown();
    void ImGUI_UpdateIO();
    LRESULT ImGUI_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}