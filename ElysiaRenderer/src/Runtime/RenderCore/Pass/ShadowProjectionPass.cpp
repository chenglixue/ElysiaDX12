#include "stdafx.h"
#include "ShadowProjectionPass.h"

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
#include <Programs/SobolSequenceGenerator.h>

namespace ElysiaRenderer
{
    ShadowProjectionPass::ShadowProjectionPass()
    {

    }
    ShadowProjectionPass::~ShadowProjectionPass()
    {
        Dispose();
    }
    void ShadowProjectionPass::Dispose()
    {

    }

    void ShadowProjectionPass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_shadowMaskWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_shadowMaskHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);
        m_sobolWidth = m_sobolHeight = 128;

        for (UINT i = 0; i < m_ShadowMaskRTCount; ++i)
        {
            m_pShadowMaskRTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(m_shadowMaskWidth,
                                                                                           m_shadowMaskHeight,
                                                                                           DXGI_FORMAT_R8_UNORM,
                                                                                           true,
                                                                                           RenderResource::GetInstance()
                                                                                           .
                                                                                           GetPropertyName(
                                                                                               RenderTextureIDs::ShadowMaskRTID)
                                                                                           + std::to_wstring(i));
        }

        m_sobolSequences = Create2DSobolSqeuence(64);

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }
    void ShadowProjectionPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;
        {
            std::vector<std::wstring> enableKeywords{};

            switch (UserData::GetInstance().shadowParameter.shadowQuality)
            {
            case ShadowQuality::Low:
            {
                enableKeywords.emplace_back(L"SHADOW_QUALITY_LOW");
                break;
            }
            case ShadowQuality::Middle:
            {
                enableKeywords.emplace_back(L"SHADOW_QUALITY_MIDDLE");
                break;
            }
            case ShadowQuality::High:
            {
                enableKeywords.emplace_back(L"SHADOW_QUALITY_HIGH");
                break;
            }
            case ShadowQuality::VeryHigh:
            {
                enableKeywords.emplace_back(L"SHADOW_QUALITY_VERYHIGH");
                break;
            }
            }
            switch (UserData::GetInstance().shadowParameter.shadowType)
            {
            case ShadowType::Hard:
            {
                enableKeywords.emplace_back(L"HARD_SHADOW");
                break;
            }
            case ShadowType::Soft:
            {
                enableKeywords.emplace_back(L"SOFT_SHADOW");
                break;
            }
            }

            for (UINT i = 0; i < SHADOW_PROJECTION_PASS_COUNT; ++i)
            {
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

    }

    void ShadowProjectionPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Projection Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        m_readIndex = m_writeIndex;
        m_writeIndex = (m_writeIndex + 1) % 2;

        DoShadowMask();
        if (m_isFirstFrame)
        {
            m_isFirstFrame = false;
        }
        else
        {
            if (UserData::GetInstance().shadowParameter.EnableTAA)
            {
                DoTAA();
            }
        }
    }

    void ShadowProjectionPass::DoShadowMask()
    {
        auto passID = SHADOW_PROJECTION_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto pWriteRT = m_pShadowMaskRTs[m_writeIndex];
        m_pCommand->AddBarrier(pWriteRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                   passID);

            m_pMaterial->SetUINT(ShaderIDs::g_ShadowMaskTexIndex, pWriteRT->GetUAVResourceHeapIndex(), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_ShadowMaskTexSize,
                                   GetScreenSize(m_shadowMaskWidth, m_shadowMaskHeight),
                                   passID);
            m_pMaterial->SetVector2Array(ShaderIDs::g_SobolSequence, m_sobolSequences, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_shadowMaskWidth, threadGroupSize.x),
                                 CeilDivide(m_shadowMaskHeight, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(pWriteRT, false);
        }
        m_pCommand->AddBarrier(pWriteRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Shadow/") + passName).c_str());
    }
    void ShadowProjectionPass::DoTAA()
    {
        auto passID = SHADOW_TAA_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto pReadRT = m_pShadowMaskRTs[m_readIndex];
        auto pWriteRT = m_pShadowMaskRTs[m_writeIndex];
        m_pCommand->AddBarrier(pWriteRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_HistoryTexIndex, pReadRT->GetSRVResourceHeapIndex(), passID);
            m_pMaterial->SetUINT(ShaderIDs::g_CurrTexIndex, pWriteRT->GetUAVResourceHeapIndex(), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_ShadowMaskTexSize,
                                   GetScreenSize(m_shadowMaskWidth, m_shadowMaskHeight),
                                   passID);
            m_pMaterial->SetFloat(ShaderIDs::g_StaticBlendWeight,
                                  UserData::GetInstance().taaParameter.staticWeight,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_DynamicBlendWeight,
                                  UserData::GetInstance().taaParameter.dynamicWeight,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_MaxBlendWeight,
                                  UserData::GetInstance().taaParameter.maxWeight,
                                  passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_shadowMaskWidth, threadGroupSize.x),
                                 CeilDivide(m_shadowMaskHeight, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(pWriteRT, false);
        }
        m_pCommand->AddBarrier(pWriteRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Shadow/") + passName).c_str());
    }
}