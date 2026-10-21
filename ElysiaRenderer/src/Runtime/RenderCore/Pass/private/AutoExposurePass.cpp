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
    static constexpr UINT BUILD_HISTOGRAM_THREAD_GROUP_SIZE = 16;
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
        if (m_pDebugReadback)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pDebugReadback);
            m_pDebugReadback.reset();
        }
        m_pHistogramRT = nullptr;
        m_pHDRSnapshotRT = nullptr;
    }

    void AutoExposurePass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_halfWidth = UINT(m_displayWidth) >> 1;
        m_halfHeight = UINT(m_displayHeight) >> 1;

        m_pHistogramBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc{
            .name = L"Auto Exposure Histogram Buffer",
            .stride = HISTOGRAM_ELEMENT_SIZE,
            .size = (size_t)HISTOGRAM_SIZE * HISTOGRAM_ELEMENT_SIZE,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        // .r = final scale, .g = scale without constant EV, .b = avg lum, .a = middleGrey * compensation
        // Do not upload from the copy queue. EndFrame runs that upload after graphics
        // has already moved the buffer to UAV, which the copy queue rejects.
        m_pExposureScaleBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc{
            .name = RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::AutoExposureScaleRTID),
            .stride = EXPOSURE_ELEMENT_SIZE,
            .size = EXPOSURE_ELEMENT_SIZE,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });
        m_snapExposure = true;

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
        m_pPreExposureBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc{
            .name = RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::AutoExposurePreScaleRTID),
            .stride = sizeof(Vector2),
            .size = sizeof(Vector2),
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });
        m_exposureInitialized = false;
        for (bool& pending : m_debugReadbackPending)
            pending = false;
        m_pDebugReadback = BufferManager::GetInstance().CreateReadBackBuffer(
            sizeof(float) * 8 * ElysiaHelper::NUM_FRAMES_IN_FLIGHT,
            "Auto Exposure Debug Readback");
        m_autoExposureData.pPreExposureBuffer = m_pPreExposureBuffer;
        RenderPassResourceManager::GetInstance().Create<AutoExposureData>(&m_autoExposureData);

        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        for (auto& pass : m_shaderPasses)
        {
            pass.Environment.SetDefine(L"HISTOGRAM_SIZE", static_cast<int>(HISTOGRAM_SIZE));
            pass.Environment.SetDefine(L"BUILD_HISTOGRAM_THREAD_GROUP_SIZE",
                                       static_cast<int>(BUILD_HISTOGRAM_THREAD_GROUP_SIZE));
            pass.Environment.SetDefine(L"EXPOSURE_COMPENSATION_CURVE_SAMPLES",
                                       kAutoExposureCompensationCurveSamples);
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

        const auto HalfResSceneTex = RenderTargetManager::GetInstance().GetRenderTexture(
            RenderTextureIDs::HalfResSceneRTID);
        if (!HalfResSceneTex->IsValid())
            return;

        m_pMaterial->SetUINT(ShaderIDs::g_HalfResSourceTexIndex,
                             HalfResSceneTex->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HistogramBufferIndex,
                             m_pHistogramBuffer->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_ExposureBufferScaleIndex,
                             m_pExposureScaleBuffer->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureScaleBufferIndex,
                             RenderPassResourceManager::GetInstance().Get<AutoExposureData>().pPreExposureBuffer->
                                                                      GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HistogramTexIndex,
                             m_pHistogramRT->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_HalfSize,
                               GetScreenSize(m_halfWidth, m_halfHeight),
                               passID);

        m_pMaterial->SetFloat(ShaderIDs::g_HistogramScale, histogramScale, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HistogramBias, histogramBias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_LowPercent, p.lowPercent, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HighPercent, p.highPercent, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MinAverageLuminance, p.minAverageLuminance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MaxAverageLuminance, p.maxAverageLuminance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MiddleGrey, p.middleGrey, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureCompensationEV, p.exposureCompensation, passID);

        float curveScale = 0.0f;
        float curveBias = 0.0f;
        ComputeAutoExposureCompensationCurveScaleBias(curveScale, curveBias);
        float curveSamples[kAutoExposureCompensationCurveSamples];
        BakeAutoExposureCompensationCurve(p, curveSamples, kAutoExposureCompensationCurveSamples);
        std::vector<Vector4> curveLut;
        curveLut.reserve(kAutoExposureCompensationCurveSamples / 4);
        for (int i = 0; i < kAutoExposureCompensationCurveSamples; i += 4)
        {
            curveLut.emplace_back(curveSamples[i],
                                  curveSamples[i + 1],
                                  curveSamples[i + 2],
                                  curveSamples[i + 3]);
        }
        m_pMaterial->SetVector4Array(ShaderIDs::g_ExposureCompensationCurve, curveLut, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureCompensationCurveScale, curveScale, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureCompensationCurveBias, curveBias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureSpeedUp, p.speedUp, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExposureSpeedDown, p.speedDown, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ExponentialTransitionDistance, p.exponentialTransitionDistance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_DeltaTime, m_deltaTime, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureEnable, UsePreExposure(p) ? 1u : 0u, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_WriteHistogramTex, p.visualizeHDR ? 1u : 0u, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ForceTarget, m_snapExposure ? 1.0f : 0.0f, passID);
    }

    void AutoExposurePass::DoClearHistogram()
    {
        auto passID = CLEAR_HISTOGRAM_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pHistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        m_pCommand->Dispatch(1, 1, 1);
        m_pCommand->AddUAVBarrier(m_pHistogramBuffer);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Auto Exposure/") + passName).c_str());
    }

    void AutoExposurePass::DoBuildHistogram()
    {
        auto passID = BUILD_HISTOGRAM_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        const auto HalfResSceneRT = RenderTargetManager::GetInstance().GetRenderTexture(
            RenderTextureIDs::HalfResSceneRTID);

        m_pCommand->AddBarrier(HalfResSceneRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pHistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        auto threadGroupSize = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide(m_halfWidth, threadGroupSize.x),
                             CeilDivide(m_halfHeight, threadGroupSize.y),
                             threadGroupSize.z);
        m_pCommand->AddUAVBarrier(m_pHistogramBuffer);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Auto Exposure/") + passName).c_str());
    }

    void AutoExposurePass::DoComputeExposure()
    {
        auto passID = COMPUTE_EXPOSURE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        const bool writeHistogramTex = UserData::GetInstance().autoExposureParameter.visualizeHDR;

        m_pCommand->AddBarrier(m_pHistogramBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (writeHistogramTex)
            m_pCommand->AddBarrier(m_pHistogramRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(m_pPreExposureBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        m_pCommand->Dispatch(1, 1, 1);
        m_pCommand->AddUAVBarrier(m_pExposureScaleBuffer, false);
        if (writeHistogramTex)
            m_pCommand->AddUAVBarrier(m_pHistogramRT, false);
        m_pCommand->AddUAVBarrier(m_pPreExposureBuffer, false);

        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if (writeHistogramTex)
            m_pCommand->AddBarrier(m_pHistogramRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_snapExposure = false;

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Auto Exposure/") + passName).c_str());
    }

    void AutoExposurePass::EnsureInitialExposure()
    {
        if (m_exposureInitialized || !m_pCommand || !m_pMaterial ||
            !m_pExposureScaleBuffer || !m_pPreExposureBuffer)
            return;

        DoInitExposureState();
        m_exposureInitialized = true;
    }

    void AutoExposurePass::DoInitExposureState()
    {
        auto passID = INIT_EXPOSURE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(m_pPreExposureBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        BindCommonParams(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);
        m_pCommand->Dispatch(1, 1, 1);
        m_pCommand->AddUAVBarrier(m_pExposureScaleBuffer, false);
        m_pCommand->AddUAVBarrier(m_pPreExposureBuffer);
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

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(),
                                  (std::string("Auto Exposure/") + "AutoExposure HDR Snapshot").c_str());
    }

    namespace
    {
        AutoExposurePass::DebugStats g_autoExposureDebugStats;

        struct ExposureDebugSlot
        {
            float exposure[4];
            float preExposure[2];
            float pad[2];
        };
        static_assert(sizeof(ExposureDebugSlot) == sizeof(float) * 8, "readback slot must stay 32 bytes");
    }

    const AutoExposurePass::DebugStats& AutoExposurePass::GetDebugStats()
    {
        return g_autoExposureDebugStats;
    }

    void AutoExposurePass::PullDebugReadback(UINT frameID)
    {
        if (!m_pDebugReadback || frameID >= ElysiaHelper::NUM_FRAMES_IN_FLIGHT ||
            !m_debugReadbackPending[frameID])
            return;

        auto* resource = m_pDebugReadback->GetResource().Get();
        if (!resource)
            return;

        const UINT64 slotBytes = sizeof(ExposureDebugSlot);
        ExposureDebugSlot* slots = nullptr;
        const D3D12_RANGE readRange{frameID * slotBytes, (frameID + 1) * slotBytes};
        if (FAILED(resource->Map(0, &readRange, reinterpret_cast<void**>(&slots))) || !slots)
            return;

        const ExposureDebugSlot& slot = slots[frameID];
        g_autoExposureDebugStats.averageSceneLuminance = slot.exposure[2];
        g_autoExposureDebugStats.preExposure = slot.preExposure[0];
        g_autoExposureDebugStats.valid = true;

        const D3D12_RANGE writeRange{0, 0};
        resource->Unmap(0, &writeRange);
    }

    void AutoExposurePass::CopyDebugReadback(UINT frameID)
    {
        if (!m_pCommand || !m_pDebugReadback || !m_pExposureScaleBuffer || !m_pPreExposureBuffer ||
            frameID >= ElysiaHelper::NUM_FRAMES_IN_FLIGHT)
            return;

        const UINT64 slotBytes = sizeof(ExposureDebugSlot);
        const UINT64 dest = frameID * slotBytes;

        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, false);
        m_pCommand->AddBarrier(m_pPreExposureBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_pCommand->CopyBufferRegion(*m_pDebugReadback, dest, *m_pExposureScaleBuffer, 0, sizeof(float) * 4);
        m_pCommand->CopyBufferRegion(*m_pDebugReadback, dest + sizeof(float) * 4, *m_pPreExposureBuffer, 0,
                                     sizeof(float) * 2);
        m_pCommand->AddBarrier(m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pPreExposureBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_debugReadbackPending[frameID] = true;
    }

    void AutoExposurePass::Render(FrameContext& context)
    {
        m_deltaTime = context.deltaTime;
        PullDebugReadback(context.frameID);

        const auto& ae = UserData::GetInstance().autoExposureParameter;
        if (!UsePreExposure(ae))
            DoResetPreExposure();
        if (!WantsAutoExposureHistogram(ae))
        {
            CopyDebugReadback(context.frameID);
            return;
        }

        PIXHelper pix(m_pCommand->GetCommandList(), "Auto Exposure Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DoClearHistogram();
        DoBuildHistogram();
        DoComputeExposure();
        CopyDebugReadback(context.frameID);
        if (ae.visualizeHDR)
            DoSnapshotHdr();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "AutoExposure");
    }
}