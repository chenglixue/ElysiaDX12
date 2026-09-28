#include "stdafx.h"
#include "../public/IMGUIDrawer.h"

#include "../public/IMGUIHelper.h"
#include "Runtime/Core/public/DX12Context.h"
#include "Runtime/Core/public/DX12Device.h"
#include "Runtime/Core/public/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/public/DX12StagingDescriptorHeap.h"
#include "Runtime/Core/public/SwapChain.h"

namespace ElysiaEditor
{
    void IMGUIDrawer::OnCreate(DX12Device* pDevice, SwapChain* pSwapChain)
    {
        auto UIDescriptor0 = pDevice->GetImGUIRenderHeap().GetReservedDescriptor(
            IMGUI_RESERVED_DESCRIPTOR_INDEX);

        ImGui_ImplDX12_Init(pDevice->GetDevice(),
                            NUM_FRAMES_IN_FLIGHT,
                            pSwapChain->GetFormat(),
                            pDevice->GetImGUIRenderHeap().GetDescriptorHeap(),
                            UIDescriptor0.GetCPUHandle(),
                            UIDescriptor0.GetGPUHandle());
    }
    void IMGUIDrawer::OnDestory()
    {

    }
    void IMGUIDrawer::UpdatePipeline()
    {

    }
    void IMGUIDrawer::Draw(DX12Context* pCommand)
    {
        ImGui::ShowDemoWindow();
        ImGui::Render();
        if (ImGui::GetDrawData())
        {
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommand->GetCommandList());
        }
    }
}