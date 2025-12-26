#include "stdafx.h"
#include "IMGUI.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"
#include "Runtime/Core/DX12Context.h"

#include "Runtime/Core/DX12Device.h"

namespace ElysiaEditor
{
    void IMGUI::OnCreate(HWND windowHandle, DX12Device* pDevice)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui::StyleColorsDark();

        auto UIDescriptor0 = pDevice->GetImguiDescriptor(0);
        auto UIDescriptor1 = pDevice->GetImguiDescriptor(1);
        ImGui_ImplWin32_Init(windowHandle);
        ImGui_ImplDX12_Init(pDevice->GetDevice(), NUM_FRAMES_IN_FLIGHT,
        	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, nullptr,
        	UIDescriptor0.GetCPUHandle(), UIDescriptor0.GetGPUHandle(), UIDescriptor1.GetCPUHandle(), UIDescriptor1.GetGPUHandle());
    }
    void IMGUI::OnDestory()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    void IMGUI::UpdatePipeline()
    {
        
    }
    void IMGUI::Draw(DX12Context* pCommand)
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        ImGui::Render();
        if (ImGui::GetDrawData())
        {
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommand->GetCommandList());
        }
    }
}
