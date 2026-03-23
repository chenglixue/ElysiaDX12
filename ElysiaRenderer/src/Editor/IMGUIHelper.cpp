#include "stdafx.h"
#include "IMGUIHelper.h"

#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/SwapChain.h"

namespace ElysiaEditor
{
    static HWND g_hWnd;

    bool ImGUI_Init(HWND windowHandle,
                    ElysiaCore::DX12Device* pDevice,
                    ElysiaCore::SwapChain& pSwapChain)
    {
        g_hWnd = windowHandle;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplWin32_Init(windowHandle);

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 启用键盘控制
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // 启用Docking

        io.ConfigDockingWithShift = true;

        // 2. 设置样式（docking分支可能使用不同的默认样式）
        ImGui::StyleColorsDark();

        // 3. 调整docking分支的样式以支持多视口
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui_ImplWin32_EnableDpiAwareness();
        }

        return true;
    }

    void ImGUI_Shutdown()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGUI_NewFrame()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!io.Fonts->IsBuilt())
        {
            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGUI_EndFrame(ElysiaCore::DX12Device* pDevice)
    {
        // ImGuiIO& io = ImGui::GetIO();
        // if (io.ConfigFlags)
        // {
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault(nullptr, (void*)pDevice->GetDevice());
        // }
    }

    void ImGUI_UpdateIO()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Setup display size (every frame to accommodate for window resizing)
        RECT rect;
        GetClientRect(g_hWnd, &rect);
        io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

        // Read keyboard modifiers inputs
        io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        io.KeySuper = false;
        // io.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
        // io.MousePos : filled by WM_MOUSEMOVE events
        // io.MouseDown : filled by WM_*BUTTON* events
        // io.MouseWheel : filled by WM_MOUSEWHEEL events

        // Hide OS mouse cursor if ImGui is drawing it
        if (io.MouseDrawCursor)
            SetCursor(NULL); // Start the frame
    }

    static bool IsAnyMouseButtonDown()
    {
        ImGuiIO& io = ImGui::GetIO();
        for (int n = 0; n < IM_ARRAYSIZE(io.MouseDown); n ++)
            if (io.MouseDown[n])
                return true;
        return false;
    }
}