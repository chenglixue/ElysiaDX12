#include "stdafx.h"
#include "SSSRPass.h"

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
    SSSRPass::SSSRPass()
    {

    }
    SSSRPass::~SSSRPass()
    {
        Dispose();
    }
    void SSSRPass::Dispose()
    {

    }

    void SSSRPass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_cameraWidth = (UINT)m_displaySize.x >> 1;
        m_cameraHeight = (UINT)m_displaySize.x >> 1;

        uint32_t elementSize = 4;
        m_pRayCounterBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Static AABB Buffer",
            .stride = elementSize,
            .size = 4ull * elementSize,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }
    void SSSRPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < SSSR_PASS_COUNT; ++i)
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
    void SSSRPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().bloomParameter.enable)
            return;
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DoTileClassify();
    }

    void SSSRPass::DoTileClassify()
    {
        auto passID = BLOOM_FIRST_DOWN_SAMPLE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
    }
}