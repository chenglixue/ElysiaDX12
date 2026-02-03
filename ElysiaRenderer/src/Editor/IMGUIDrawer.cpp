#include "stdafx.h"
#include "IMGUIDrawer.h"

#include "IMGUIHelper.h"
#include "Runtime/Core/DX12Context.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/DX12StagingDescriptorHeap.h"
#include "Runtime/Core/SwapChain.h"

namespace ElysiaEditor
{
    void IMGUIDrawer::OnCreate(DX12Device* pDevice, SwapChain* pSwapChain)
    {
        auto UIDescriptor0 = pDevice->GetImGUIRenderHeap().GetReservedDescriptor(
            IMGUI_RESERVED_DESCRIPTOR_INDEX);

        ImGui_ImplDX12_Init(pDevice->GetDevice(),
                            1,
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