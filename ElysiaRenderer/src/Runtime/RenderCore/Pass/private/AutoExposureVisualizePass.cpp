#include "stdafx.h"
#include "../public/AutoExposureVisualizePass.h"
#include "../public/AutoExposurePass.h"

#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Shader.h"
#include "Runtime/Core/public/DX12Device.h"
#include "Runtime/Core/public/DX12TextureBuffer.h"
#include "Runtime/Core/public/SwapChain.h"

#include "Runtime/RenderCore/public/RenderResource.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/ShaderVariantManager.h"
#include "Runtime/RenderCore/public/AutoExposureUtility.h"

namespace ElysiaRenderer
{
    AutoExposureVisualizePass::AutoExposureVisualizePass()
        : BasePass()
    {
    }

    AutoExposureVisualizePass::~AutoExposureVisualizePass()
    {
        Dispose();
    }

    void AutoExposureVisualizePass::Dispose()
    {
    }

    void AutoExposureVisualizePass::Configure()
    {
        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        for (auto& pass : m_shaderPasses)
        {
            pass.Environment.SetDefine(L"HISTOGRAM_SIZE", kAutoExposureHistogramSize);
        }
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }

    void AutoExposureVisualizePass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < AUTO_EXPOSURE_VISUALIZE_PASS_COUNT; ++i)
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

    void AutoExposureVisualizePass::DoVisualizeHDR()
    {
        const auto& ae = UserData::GetInstance().autoExposureParameter;
        auto& names = RenderResource::GetInstance();
        auto& rtm = RenderTargetManager::GetInstance();

        RenderTexture* pHistogramRT = rtm.GetRenderTexture(
            names.GetPropertyName(AutoExposurePass::RenderTextureIDs::AutoExposureHistogramRTID));
        RenderTexture* pHdrRT = rtm.GetRenderTexture(
            names.GetPropertyName(AutoExposurePass::RenderTextureIDs::AutoExposureHdrSnapshotRTID));

        if (!pHistogramRT || !pHdrRT || !m_pDisplayRT || !AutoExposurePass::m_pExposureScaleBuffer)
            return;

        auto passID = VISUALIZE_HDR_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(pHistogramRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(AutoExposurePass::m_pExposureScaleBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(pHdrRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        float histogramScale = 0.0f;
        float histogramBias = 0.0f;
        ComputeAutoExposureHistogramScaleBias(ae.minLogLuminance,
                                              ae.maxLogLuminance,
                                              histogramScale,
                                              histogramBias);

        const UINT debugType = (UINT)ae.visualizeDebugType;
        m_pMaterial->SetUINT(ShaderIDs::g_DestTexIndex,
                             m_pDisplayRT->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HdrTexIndex,
                             pHdrRT->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HistogramTexIndex,
                             pHistogramRT->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_ExposureScaleBufferIndex,
                             AutoExposurePass::m_pExposureScaleBuffer->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_VisualizeDebugType, debugType, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_LuminanceScale,
                              (std::max)(ae.visualizeLuminanceScale, 0.0f),
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HistogramScale, histogramScale, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HistogramBias, histogramBias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_LowPercent, ae.lowPercent, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HighPercent, ae.highPercent, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MinAverageLuminance, ae.minAverageLuminance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MaxAverageLuminance, ae.maxAverageLuminance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MiddleGrey, ae.middleGrey, passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                               GetScreenSize(m_pDisplayRT->GetWidth(), m_pDisplayRT->GetHeight()),
                               passID);

        SetSpaceResource(passData, PER_PASS_SPACE);
        auto threadGroupSize = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide((UINT)m_pDisplayRT->GetWidth(), threadGroupSize.x),
                             CeilDivide((UINT)m_pDisplayRT->GetHeight(), threadGroupSize.y),
                             threadGroupSize.z);
        m_pCommand->AddUAVBarrier(m_pDisplayRT, false);
        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void AutoExposureVisualizePass::Render(FrameContext& context)
    {
        const auto& ae = UserData::GetInstance().autoExposureParameter;
        // Local exposure visualization runs next and replaces this image.
        if (!ae.visualizeHDR || ae.visualizeLocalExposure)
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "Auto Exposure Visualize");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DoVisualizeHDR();

        if (m_pGPUTimer)
            m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "AutoExposureVisualize");
    }
}
