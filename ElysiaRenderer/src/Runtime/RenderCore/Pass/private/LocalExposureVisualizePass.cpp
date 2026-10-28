#include "stdafx.h"
#include "../public/LocalExposureVisualizePass.h"

#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Shader.h"
#include "Runtime/RenderCore/public/AutoExposureUtility.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/public/ShaderVariantManager.h"
#include "Runtime/RenderCore/Pass/public/AutoExposurePass.h"
#include "Runtime/RenderCore/Pass/public/LocalExposureFusionPass.h"

namespace ElysiaRenderer
{
    constexpr int kLocalExposureVisualizeGroupSize = 8;

    LocalExposureVisualizePass::LocalExposureVisualizePass() = default;

    LocalExposureVisualizePass::~LocalExposureVisualizePass()
    {
        Dispose();
    }

    void LocalExposureVisualizePass::Dispose()
    {
    }

    void LocalExposureVisualizePass::Configure()
    {
        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        for (auto& pass : m_shaderPasses)
            pass.Environment.SetDefine(L"GROUP_SIZE", kLocalExposureVisualizeGroupSize);

        if (!m_pMaterial)
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        UpdatePipeline();
    }

    void LocalExposureVisualizePass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < LOCAL_EXPOSURE_VISUALIZE_PASS_COUNT; ++i)
        {
            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto variantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &variantManager->GetOrCompileVariantByNames({});
            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID);
        }
    }

    void LocalExposureVisualizePass::CaptureSceneColor(DX12GraphicsContext* pCommand, RenderTexture* pDisplayRT)
    {
        if (!pCommand || !pDisplayRT || pDisplayRT->GetWidth() < 1 || pDisplayRT->GetHeight() < 1)
            return;

        auto& names = RenderResource::GetInstance();
        RenderTexture* pScene = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            pDisplayRT->GetWidth(),
            pDisplayRT->GetHeight(),
            pDisplayRT->GetFormat(),
            true,
            names.GetPropertyName(RenderTextureIDs::SceneSnapshotRTID));
        if (!pScene)
            return;

        // CopyTexture leaves both resources in copy states. Put them back so the
        // following debug / visualize passes see the same usage as before the copy.
        const D3D12_RESOURCE_STATES displayState = pDisplayRT->GetTexture()->GetUsageState();
        pCommand->CopyTexture(pDisplayRT, pScene);
        pCommand->AddBarrier(pScene, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        pCommand->AddBarrier(pDisplayRT, displayState);
    }

    void LocalExposureVisualizePass::DoVisualize()
    {
        const auto& ae = UserData::GetInstance().autoExposureParameter;
        auto& names = RenderResource::GetInstance();
        auto& rtm = RenderTargetManager::GetInstance();

        RenderTexture* pHdr = rtm.GetRenderTexture(
            names.GetPropertyName(LocalExposureFusionPass::RenderTextureIDs::HdrSnapshotRTID));
        RenderTexture* pScene = rtm.GetRenderTexture(
            names.GetPropertyName(RenderTextureIDs::SceneSnapshotRTID));
        RenderTexture* pLum = rtm.GetRenderTexture(
            names.GetPropertyName(LocalExposureFusionPass::RenderTextureIDs::LumRTID) + L"0");
        RenderTexture* pWeight = rtm.GetRenderTexture(
            names.GetPropertyName(LocalExposureFusionPass::RenderTextureIDs::WeightRTID) + L"0");
        RenderTexture* pFusion = rtm.GetRenderTexture(
            names.GetPropertyName(LocalExposureFusionPass::RenderTextureIDs::ResultRTID) + L"0");

        const bool hasFusion = pLum && pWeight && pFusion;
        if (!pHdr || !pScene || !m_pDisplayRT || !AutoExposurePass::m_pExposureScaleBuffer)
            return;
        if (pHdr->GetWidth() != m_pDisplayRT->GetWidth() || pHdr->GetHeight() != m_pDisplayRT->GetHeight())
            return;
        if (pScene->GetWidth() != m_pDisplayRT->GetWidth() || pScene->GetHeight() != m_pDisplayRT->GetHeight())
            return;

        auto passID = VISUALIZE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(pHdr, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(pScene, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        if (hasFusion)
        {
            m_pCommand->AddBarrier(pLum, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
            m_pCommand->AddBarrier(pWeight, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
            m_pCommand->AddBarrier(pFusion, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        }
        if (AutoExposurePass::m_pExposureScaleBuffer)
            m_pCommand->AddBarrier(AutoExposurePass::m_pExposureScaleBuffer,
                                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                   false);
        BufferHandle preExposure =
            RenderPassResourceManager::GetInstance().Get<AutoExposureData>().pPreExposureBuffer;
        if (preExposure)
            m_pCommand->AddBarrier(preExposure, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        const LocalExposureVisualizationMode mode =
            SanitizeLocalExposureVisualizationMode(ae.localExposureVisualizationMode);

        m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                               GetScreenSize(m_pDisplayRT->GetWidth(), m_pDisplayRT->GetHeight()),
                               passID);
        m_pMaterial->SetUINT(ShaderIDs::g_DestTexIndex, m_pDisplayRT->GetUAVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HdrTexIndex, pHdr->GetSRVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_SceneTexIndex, pScene->GetSRVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_LumTexIndex,
                             hasFusion ? pLum->GetSRVResourceHeapIndex() : 0u,
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_WeightTexIndex,
                             hasFusion ? pWeight->GetSRVResourceHeapIndex() : 0u,
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_FusionTexIndex,
                             hasFusion ? pFusion->GetSRVResourceHeapIndex() : 0u,
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_ExposureScaleBufferIndex,
                             AutoExposurePass::m_pExposureScaleBuffer->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureScaleBufferIndex,
                             preExposure ? preExposure->GetUAVResourceHeapIndex() : 0u,
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_Mode, static_cast<UINT>(mode), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_AutoExposureEnable, ae.enable ? 1u : 0u, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureEnable, UsePreExposure(ae) ? 1u : 0u, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HasFusion, hasFusion ? 1u : 0u, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MiddleGrey, std::max(ae.middleGrey, 1e-4f), passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HighlightThreshold,
                              std::max(ae.localExposureHighlightThreshold, 0.0f),
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ShadowThreshold,
                              std::max(ae.localExposureShadowThreshold, 0.0f),
                              passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        auto group = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide(m_pDisplayRT->GetWidth(), group.x),
                             CeilDivide(m_pDisplayRT->GetHeight(), group.y),
                             group.z);
        m_pCommand->AddUAVBarrier(m_pDisplayRT, false);
        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void LocalExposureVisualizePass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().autoExposureParameter.visualizeLocalExposure)
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "Local Exposure Visualize");
        m_pGPUTimer = context.pGPUTimer;
        DoVisualize();
        if (m_pGPUTimer)
            m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "LocalExposureVisualize");
    }
}
