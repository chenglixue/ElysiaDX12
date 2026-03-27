#include "stdafx.h"
#include "SharpenPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Editor/IMGUIHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"
#include "Runtime/RenderCore/CameraManager.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    SharpenPass::SharpenPass()
    {

    }
    SharpenPass::~SharpenPass()
    {
        Dispose();
    }
    void SharpenPass::Dispose()
    {

    }

    void SharpenPass::Configure()
    {
        m_renderWidth = m_displaySize.x;
        m_renderHeight = m_displaySize.y;

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }
    void SharpenPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < TAA_PASS_COUNT; ++i)
        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(
                    m_pDevice,
                    m_pMaterial.get(),
                    passID);
        }
    }
    void SharpenPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().sharpenParameter.enable)
            return;
        PIXHelper pix(m_pCommand->GetCommandList(), "Sharpen Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DoCAS();
    }

    void SharpenPass::DoCAS()
    {
        auto passID = CAS_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetFloat(ShaderIDs::g_SharpenIntensity,
                                  UserData::GetInstance().sharpenParameter.sharpen,
                                  passID);
            m_pMaterial->SetUInt(ShaderIDs::g_SharpenTexIndex,
                                 m_pDisplayRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SharpenTexSize,
                                   GetScreenSize(m_renderWidth, m_renderHeight),
                                   passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_renderWidth, threadGroupSize.x),
                                 CeilDivide(m_renderHeight, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pDisplayRT, false);
        }
        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
}