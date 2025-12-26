#include "stdafx.h"
#include "IMGUI.h"

#include "IMGUIHelper.h"
#include "Runtime/Core/DX12Context.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/SwapChain.h"

namespace ElysiaEditor
{
    void IMGUI::OnCreate(DX12Device* pDevice, ElysiaCore::SwapChain* pSwapChain)
    {
        auto UIDescriptor0 = pDevice->GetImguiDescriptor(0);
        auto UIDescriptor1 = pDevice->GetImguiDescriptor(1);
        ImGui_ImplDX12_Init(pDevice->GetDevice(), NUM_FRAMES_IN_FLIGHT,
        	pSwapChain->GetFormat(), nullptr,
        	UIDescriptor0.GetCPUHandle(), UIDescriptor0.GetGPUHandle(), UIDescriptor1.GetCPUHandle(), UIDescriptor1.GetGPUHandle());
    }
    void IMGUI::OnDestory()
    {
        
    }
    void IMGUI::UpdatePipeline()
    {
        
    }
    void IMGUI::Draw(DX12Context* pCommand)
    {
        ImGui::Render();
        if (ImGui::GetDrawData())
        {
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommand->GetCommandList());
        }
    }
}
