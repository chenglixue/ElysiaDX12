#include "stdafx.h"
#include "../public/AutoExposurePass.h"

#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Shader.h"
#include "Runtime/Core/public/DX12Device.h"
#include "Runtime/Core/public/DX12TextureBuffer.h"
#include "Runtime/Core/public/SwapChain.h"

#include "Runtime/RenderCore/public/BufferManager.h"
#include "Runtime/RenderCore/public/RenderResource.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/ShaderVariantManager.h"
#include "Runtime/RenderCore/public/AutoExposureUtility.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"

namespace ElysiaRenderer
{
    static constexpr UINT HISTOGRAM_SIZE = kAutoExposureHistogramSize;
    static constexpr UINT BUILD_HISTOGRAM_THREAD_GROUP_SIZE = 32;
    static constexpr UINT HISTOGRAM_ELEMENT_SIZE = sizeof(UINT);
    static constexpr UINT EXPOSURE_ELEMENT_SIZE = sizeof(float) * 4;
    
    AutoExposurePass::AutoExposurePass()
        : BasePass()
    {
    }

    AutoExposurePass::~AutoExposurePass()
    {
        Dispose();
    }

    void AutoExposurePass::Dispose()
    {
        if (m_pHistogramBuffer)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pHistogramBuffer);
            m_pHistogramBuffer.reset();
        }
        if (m_pExposureScaleBuffer)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pExposureScaleBuffer);
            m_pExposureScaleBuffer.reset();
        }
        if (m_pPreExposureBuffer)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pPreExposureBuffer);
            m_pPreExposureBuffer.reset();
        }
        m_pHistogramRT = nullptr;
        m_pHDRSnapshotRT = nullptr;
    }

    void AutoExposurePass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;

        m_pHistogramBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc{
            .name = L"Auto Exposure Histogram Buffer",
            .stride = HISTOGRAM_ELEMENT_SIZE,
            .size = (size_t)HISTOGRAM_SIZE * HISTOGRAM_ELEMENT_SIZE,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });
        
        m_pExposureScaleBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc{
            .name = RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::AutoExposureScaleRTID),
            .stride = EXPOSURE_ELEMENT_SIZE,
            .size = EXPOSURE_ELEMENT_SIZE,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly
            });

        m_pHistogramRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            HISTOGRAM_SIZE,
            1,
            DXGI_FORMAT_R32_UINT,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::AutoExposureHistogramRTID));

        m_pHDRSnapshotRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_displayWidth,
            m_displayHeight,
            m_pDisplayRT->GetFormat(),
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::AutoExposureHdrSnapshotRTID));
        
        // x = history pre-exposure, y = current pre-exposure. First frame both are 1.
        const Vector2 preExposureInit(1.0f, 1.0f);
        m_pPreExposureBuffer = BufferManager::GetInstance().CreateVertexBuffer(BufferCreationDesc{
            .name = RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::AutoExposurePreScaleRTID),
            .stride = sizeof(Vector2),
            .size = sizeof(Vector2),
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .InitData = &preExposureInit,
        });
        m_autoExposureData.pPreExposureBuffer = m_pPreExposureBuffer;
        RenderPassResourceManager::GetInstance().Create<AutoExposureData>(&m_autoExposureData);

        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        for (auto& pass : m_shaderPasses)
        {
            pass.Environment.SetDefine(L"HISTOGRAM_SIZE", static_cast<int>(HISTOGRAM_SIZE));
            pass.Environment.SetDefine(L"BUILD_HISTOGRAM_THREAD_GROUP_SIZE",
                                       static_cast<int>(BUILD_HISTOGRAM_THREAD_GROUP_SIZE));
        }
        
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }

    void AutoExposurePass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < AUTO_EXPOSURE_PASS_COUNT; ++i)
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

    void AutoExposurePass::BindCommonParams(PassID passID)
    {
        const auto& p = UserData::GetInstance().autoExposureParameter;

        float histogramScale = 0.0f;
        float histogramBias = 0.0f;
        ComputeAutoExposureHistogramScaleBias(p.minLogLuminance,
                                              p.maxLogLuminance,
                                              histogramScale,
                                              histogramBias);

        m_pMaterial->SetUINT(ShaderIDs::g_SourceTexIndex,
                             m_pDisplayRT->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HistogramBufferIndex,
                             m_pHistogramBuffer->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_ExposureBufferScaleIndex,
                             m_pExposureScaleBuffer->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureScaleBufferIndex,
                             RenderPassResourceManager::GetInstance().Get<AutoExposureData>().pPreExposureBuffer->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HistogramTexIndex,
                             m_pHistogramRT->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                               GetScreenSize(m_displayWidth, m_displayHeight),
                               passID);

        m_pMaterial->SetFloat(ShaderIDs::g_HistogramScale, histogramScale, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HistogramBias, histogramBias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_LowPercent, p.lowPercent, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HighPercent, p.highPercent, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MinAverageLuminance, p.minAverageLuminance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MaxAverageLuminance, p.maxAverageLuminance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MiddleGrey, p.middleGrey, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureCompensationEV, p.exposureCompensation, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureSpeedUp, p.speedUp, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureSpeedDown, p.speedDown, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExponentialTransitionDistance, p.exponentialTransitionDistance, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureEnable, UsePreExposure(p) ? 1u : 0u, passID);
    }

    void AutoExposurePass::DoClearHistogram()
    {
        auto passID = CLEAR_HISTOGRAM_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pHistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        m_pCommand->Dispatch(1, 1, 1);
        m_pCommand->AddUAVBarrier(m_pHistogramBuffer);
    }

    void AutoExposurePass::DoBuildHistogram()
    {
        auto passID = BUILD_HISTOGRAM_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pHistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        auto threadGroupSize = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide(m_displayWidth, threadGroupSize.x),
                             CeilDivide(m_displayHeight, threadGroupSize.y),
                             threadGroupSize.z);
        m_pCommand->AddUAVBarrier(m_pHistogramBuffer);
    }

    void AutoExposurePass::DoComputeExposure()
    {
        auto passID = COMPUTE_EXPOSURE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pHistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(m_pHistogramRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        m_pCommand->Dispatch(1, 1, 1);
        m_pCommand->AddUAVBarrier(m_pExposureScaleBuffer, false);
        m_pCommand->AddUAVBarrier(m_pHistogramRT, false);

        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(m_pHistogramRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void AutoExposurePass::DoResetPreExposure()
    {
        auto passID = RESET_PRE_EXPOSURE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pPreExposureBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);
        m_pCommand->Dispatch(1, 1, 1);
        m_pCommand->AddUAVBarrier(m_pPreExposureBuffer);
    }

    void AutoExposurePass::DoSnapshotHdr()
    {
        if (!m_pDisplayRT)
            return;

        if (!m_pHDRSnapshotRT ||
            m_pHDRSnapshotRT->GetWidth() != m_pDisplayRT->GetWidth() ||
            m_pHDRSnapshotRT->GetHeight() != m_pDisplayRT->GetHeight() ||
            m_pHDRSnapshotRT->GetFormat() != m_pDisplayRT->GetFormat())
        {
            m_pHDRSnapshotRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                m_pDisplayRT->GetWidth(),
                m_pDisplayRT->GetHeight(),
                m_pDisplayRT->GetFormat(),
                true,
                RenderResource::GetInstance().GetPropertyName(
                    RenderTextureIDs::AutoExposureHdrSnapshotRTID));
        }

        PIXHelper pix(m_pCommand->GetCommandList(), "AutoExposure HDR Snapshot");
        m_pCommand->CopyTexture(m_pDisplayRT, m_pHDRSnapshotRT);
        m_pCommand->AddBarrier(m_pHDRSnapshotRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void AutoExposurePass::Render(FrameContext& context)
    {
        const auto& ae = UserData::GetInstance().autoExposureParameter;
        if (!UsePreExposure(ae))
            DoResetPreExposure();
        if (!WantsAutoExposureHistogram(ae))
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "Auto Exposure Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DoClearHistogram();
        DoBuildHistogram();
        DoComputeExposure();
        if (ae.visualizeHDR)
            DoSnapshotHdr();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "AutoExposure");
    }
}
