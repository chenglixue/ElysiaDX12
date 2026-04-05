#include "stdafx.h"
#include "TAAPass.h"

#include "GBufferPass.h"
#include "../TAAUtility.h"

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
    TAAPass::TAAPass()
    {

    }
    TAAPass::~TAAPass()
    {
        Dispose();
    }
    void TAAPass::Dispose()
    {

    }

    void TAAPass::Configure()
    {
        m_TAAWidth = m_displaySize.x;
        m_TAAHeight = m_displaySize.y;
        m_downSampleWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_downSampleHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);
        m_upScale = (float)m_TAAWidth / (float)m_downSampleWidth;

        for (UINT i = 0; i < m_TAARTCount; ++i)
        {
            m_TAARTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(m_TAAWidth,
                                                                                   m_TAAHeight,
                                                                                   !UserData::GetInstance().IsUseHDR
                                                                                       ? DXGI_FORMAT_R8G8B8A8_UNORM
                                                                                       : UserData::GetInstance().
                                                                                         HDRLevel == HDRQuality::Low
                                                                                       ? DXGI_FORMAT_R11G11B10_FLOAT
                                                                                       : DXGI_FORMAT_R16G16B16A16_FLOAT,
                                                                                   true,
                                                                                   RenderResource::GetInstance().
                                                                                   GetPropertyName(
                                                                                       RenderTextureIDs::TAARTID) +
                                                                                   std::to_wstring(i)
                );
        }

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }
    void TAAPass::UpdatePipeline()
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
    void TAAPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().taaParameter.Enable)
            return;
        PIXHelper pix(m_pCommand->GetCommandList(), "TAA Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        m_readIndex = m_writeIndex;
        m_writeIndex = (m_writeIndex + 1) % 2;
        if (m_isFirstFrame)
        {
            m_isFirstFrame = false;
            DoFirstFrameCopyRT(m_pCameraColorRT, m_TAARTs[0]);
            DoFirstFrameCopyRT(m_pCameraColorRT, m_TAARTs[1]);
        }
        else
        {
            DoTAA();
            DoCopyTAA2CameraColor();
        }
    }

    void TAAPass::DoTAA()
    {
        auto passID = TAA_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_TAARTs[m_writeIndex], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_HistoryTexIndex,
                                 m_TAARTs[m_readIndex]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_CurrTexIndex, m_pCameraColorRT->GetUAVResourceHeapIndex(), passID);
            m_pMaterial->SetUINT(ShaderIDs::g_DestTexIndex, m_TAARTs[m_writeIndex]->GetUAVResourceHeapIndex(), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TAATexSize,
                                   GetScreenSize(m_TAAWidth, m_TAAHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DownSampleTexSize,
                                   GetScreenSize(m_downSampleWidth, m_downSampleHeight),
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
            m_pMaterial->SetFloat(ShaderIDs::g_UpScaleFactor,
                                  m_upScale,
                                  passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_Jitter,
                                   GBufferPass::m_currJitterUV / Vector2(m_TAAWidth, m_TAAHeight),
                                   passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_HistoryJitter,
                                   GBufferPass::m_preJitterUV / Vector2(m_TAAWidth, m_TAAHeight),
                                   passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_JitterPixels,
                                   GBufferPass::m_preJitterUV / m_upScale,
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix, GBufferPass::TAAData::Pre_ViewProj_M, passID);
            m_pMaterial->SetMatrix(ShaderIDs::g_ProjMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                   passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_TAAWidth, threadGroupSize.x),
                                 CeilDivide(m_TAAHeight, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_TAARTs[m_writeIndex], false);
        }
        m_pCommand->AddBarrier(m_TAARTs[m_writeIndex], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void TAAPass::DoFirstFrameCopyRT(RenderTexture* pSourceRT, RenderTexture* pDestRT)
    {
        auto passID = COPY_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(pDestRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_SourceTexIndex, pSourceRT->GetUAVResourceHeapIndex(), passID);
            m_pMaterial->SetUINT(ShaderIDs::g_DestTexIndex, pDestRT->GetUAVResourceHeapIndex(), passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_TAAWidth, threadGroupSize.x),
                                 CeilDivide(m_TAAHeight, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(pDestRT, false);
        }
        m_pCommand->AddBarrier(pDestRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void TAAPass::DoCopyTAA2CameraColor()
    {
        auto passID = COPY_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_SourceTexIndex,
                                 m_TAARTs[m_writeIndex]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_DestTexIndex,
                                 m_pDisplayRT->GetUAVResourceHeapIndex(),
                                 passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_TAAWidth, threadGroupSize.x),
                                 CeilDivide(m_TAAHeight, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pDisplayRT, false);
        }
        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
}