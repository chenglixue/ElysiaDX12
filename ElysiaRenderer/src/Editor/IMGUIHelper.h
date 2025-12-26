#pragma once

#include <ThirdParty/imgui/imgui.h>
#include <ThirdParty/imgui/imgui_impl_win32.h>
#include <ThirdParty/imgui/imgui_impl_dx12.h>

namespace ElysiaCore
{
    class SwapChain;
    class DX12Device;
}

namespace ElysiaEditor
{
    bool ImGUI_Init(HWND windowHandle, ElysiaCore::DX12Device* pDevice, ElysiaCore::SwapChain& pSwapChain);
    void ImGUI_Shutdown();
    void ImGUI_UpdateIO();
    void ImGUI_NewFrame();
    LRESULT ImGUI_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
