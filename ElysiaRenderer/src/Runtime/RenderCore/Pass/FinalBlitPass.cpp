#include "stdafx.h"
#include "FinalBlitPass.h"

#include "AOPass.h"
#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Editor/IMGUIHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    int FinalBlitPass::ShaderPassIDs::BlitPassID = -1;
    size_t FinalBlitPass::ShaderIDs::blitterTextureIndex = SIZE_MAX;
    size_t FinalBlitPass::ShaderIDs::mipmapLevel = SIZE_MAX;
    size_t FinalBlitPass::ShaderIDs::g_ScreenSize = SIZE_MAX;

    FinalBlitPass::FinalBlitPass()
        : BasePass()
    {
        ShaderIDs::blitterTextureIndex = PropertyToID(L"blitterTextureIndex");
        ShaderIDs::mipmapLevel = PropertyToID(L"mipmapLevel");
        ShaderIDs::g_ScreenSize = PropertyToID(L"g_ScreenSize");
    }

    FinalBlitPass::~FinalBlitPass()
    {
        Dispose();
    }
    void FinalBlitPass::Dispose()
    {

    }

    void FinalBlitPass::Configure()
    {
        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Blit Pass",
                .FilePath = L"Shaders\\public\\Blit.hlsl",
            },
        };
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        ShaderPassIDs::BlitPassID = m_pMaterial->FindPassIndex("Blit Pass");

        UpdatePipeline();
    }
    void FinalBlitPass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;

        PIXHelper pix(m_pCommand->GetCommandList(), "Final Blit Pass");

        DoFinalBlit();
    }

    void FinalBlitPass::DoFinalBlit()
    {
        auto& backBuffer = m_pSwaiChain->GetCurrBackBuffer();

        m_pCommand->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommand->ClearRenderTarget(backBuffer, Color::Black);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            ShaderPassIDs::BlitPassID).pPipelineStateObject;
        pipelineStateData.m_renderTargets.emplace_back(&backBuffer);
        //pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);

        auto& passData = m_pMaterial->GetPassData(ShaderPassIDs::BlitPassID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        {
            m_pMaterial->SetUInt(ShaderIDs::blitterTextureIndex,
                                 m_pDisplayRT->GetResourceHeapIndex());

            m_pCommand->SetDefaultViewportAndScissor(UINT2(m_displaySize));
            m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            m_pCommand->DrawFullScreenTriangle();
        }

        ImGui::Render();
        if (ImGui::GetDrawData() && ImGui::GetDrawData()->CmdListsCount > 0)
        {
            ID3D12DescriptorHeap* currentHeap = m_pDevice->GetImGUIRenderHeap()
                                                         .GetDescriptorHeap();
            m_pCommand->GetCommandList()->SetDescriptorHeaps(1, &currentHeap);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pCommand->GetCommandList());
        }

        m_pCommand->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
    }

    void FinalBlitPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        UpdateFinalBlitVariant(ShaderPassIDs::BlitPassID);
    }
    void FinalBlitPass::UpdateFinalBlitVariant(UINT passID)
    {
        std::vector<std::wstring> enableKeywords{};
        auto& passData = m_pMaterial->GetPassData(passID);

        auto VariantManager = passData.pShader->GetVariantManager();
        passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

        RenderTargetDesc RTDesc = RenderTargetDesc
        {
            .m_renderTargetFormats = m_pSwaiChain->GetFormat(),
            .m_numRenderTargets = 1,
            .m_depthStencilFormat = m_pCameraDepthRT->GetFormat()
        };
        m_backBufferFormat = m_pSwaiChain->GetFormat();
        passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
            m_pDevice,
            m_pMaterial.get(),
            passID,
            RTDesc);
    }

}