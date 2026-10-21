#include "stdafx.h"
#include "../public/LocalExposureFusionPass.h"

#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Shader.h"
#include "Runtime/RenderCore/public/AutoExposureUtility.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/ShaderVariantManager.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/Pass/public/AutoExposurePass.h"

namespace ElysiaRenderer
{
    LocalExposureFusionPass::LocalExposureFusionPass()
    {
    }

    LocalExposureFusionPass::~LocalExposureFusionPass()
    {
    }

    void LocalExposureFusionPass::Dispose()
    {
    }

    void LocalExposureFusionPass::Configure()
    {
        m_levelCount = 0;
        UINT width = (UINT)m_displaySize.x;
        UINT height = (UINT)m_displaySize.y;
        if (width < 1 || height < 1)
            return;

        for (UINT level = 0; level < kLevelCount; ++level)
        {
            m_levelSize[level] = UINT2(std::max(width, 1u), std::max(height, 1u));
            ++m_levelCount;
            if (width <= 1 && height <= 1)
                break;
            width = std::max(1u, (width + 1) >> 1);
            height = std::max(1u, (height + 1) >> 1);
        }

        for (UINT level = 0; level < m_levelCount; ++level)
        {
            const UINT w = m_levelSize[level].x;
            const UINT h = m_levelSize[level].y;
            const std::wstring suffix = std::to_wstring(level);
            m_lumRTs[level] = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, true,
                RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::LumRTID) + suffix);
            m_weightRTs[level] = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, true,
                RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::WeightRTID) + suffix);
            m_resultRTs[level] = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                w, h, DXGI_FORMAT_R32_FLOAT, true,
                RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::ResultRTID) + suffix);
        }

        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        if (!m_pMaterial)
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        UpdatePipeline();
    }

    void LocalExposureFusionPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < LOCAL_EXPOSURE_FUSION_PASS_COUNT; ++i)
        {
            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto variantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &variantManager->GetOrCompileVariantByNames({});
            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }
    }

    void LocalExposureFusionPass::DispatchSetup()
    {
        auto passID = FUSION_SETUP_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        const auto& ae = UserData::GetInstance().autoExposureParameter;
        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_lumRTs[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(m_weightRTs[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (AutoExposurePass::m_pExposureScaleBuffer)
            m_pCommand->AddBarrier(AutoExposurePass::m_pExposureScaleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);

        m_pMaterial->SetFloat4(ShaderIDs::g_DestSize, GetScreenSize(m_levelSize[0].x, m_levelSize[0].y), passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize, GetScreenSize(m_levelSize[0].x, m_levelSize[0].y), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_SourceTexIndex, m_pDisplayRT->GetSRVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_DestLumTexIndex, m_lumRTs[0]->GetUAVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_DestWeightTexIndex, m_weightRTs[0]->GetUAVResourceHeapIndex(), passID);
        UINT exposureSrv = 0;
        if (AutoExposurePass::m_pExposureScaleBuffer)
            exposureSrv = AutoExposurePass::m_pExposureScaleBuffer->GetResourceHeapIndex();
        m_pMaterial->SetUINT(ShaderIDs::g_ExposureScaleBufferIndex, exposureSrv, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureScaleBufferIndex,
                             RenderPassResourceManager::GetInstance().Get<AutoExposureData>().pPreExposureBuffer->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_AutoExposureEnable, ae.enable ? 1u : 0u, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureEnable, UsePreExposure(ae) ? 1u : 0u, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_HighlightContrast, ae.fusionHighlightContrast, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_ShadowContrast, ae.fusionShadowContrast, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_TargetLuminance, 0.5f, passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        auto group = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide(m_levelSize[0].x, group.x),
                             CeilDivide(m_levelSize[0].y, group.y),
                             group.z);
        m_pCommand->AddUAVBarrier(m_lumRTs[0], false);
        m_pCommand->AddUAVBarrier(m_weightRTs[0]);
        
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Local Exposure Fusion/") + passName).c_str());
    }

    void LocalExposureFusionPass::DispatchDownsample(UINT level)
    {
        auto passID = FUSION_DOWNSAMPLE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto group = passData.GetKernelThreadGroupSizes();

        auto downsample = [&](RenderTexture* source, RenderTexture* dest, UINT sourceIndexId, UINT destIndexId)
        {
            m_pCommand->AddBarrier(source, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
            m_pCommand->AddBarrier(dest, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize, GetScreenSize(m_levelSize[level].x, m_levelSize[level].y), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize, GetScreenSize(m_levelSize[level - 1].x, m_levelSize[level - 1].y), passID);
            m_pMaterial->SetUINT(sourceIndexId, source->GetSRVResourceHeapIndex(), passID);
            m_pMaterial->SetUINT(destIndexId, dest->GetUAVResourceHeapIndex(), passID);
            SetSpaceResource(passData, PER_PASS_SPACE);
            m_pCommand->Dispatch(CeilDivide(m_levelSize[level].x, group.x),
                                 CeilDivide(m_levelSize[level].y, group.y),
                                 group.z);
            m_pCommand->AddUAVBarrier(dest);
        };

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        downsample(m_lumRTs[level - 1], m_lumRTs[level], ShaderIDs::g_SourceTexIndex, ShaderIDs::g_DestLumTexIndex);
        downsample(m_weightRTs[level - 1], m_weightRTs[level], ShaderIDs::g_SourceTexIndex, ShaderIDs::g_DestLumTexIndex);
    }

    void LocalExposureFusionPass::DispatchBlend(UINT level)
    {
        auto passID = FUSION_BLEND_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        const bool coarsest = level + 1 == m_levelCount;
        m_pCommand->AddBarrier(m_lumRTs[level], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_weightRTs[level], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        if (!coarsest)
        {
            m_pCommand->AddBarrier(m_lumRTs[level + 1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
            m_pCommand->AddBarrier(m_resultRTs[level + 1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        }
        m_pCommand->AddBarrier(m_resultRTs[level], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        m_pMaterial->SetFloat4(ShaderIDs::g_DestSize, GetScreenSize(m_levelSize[level].x, m_levelSize[level].y), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_LumTexIndex, m_lumRTs[level]->GetSRVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_WeightTexIndex, m_weightRTs[level]->GetSRVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_IsCoarsest, coarsest ? 1u : 0u, passID);
        if (!coarsest)
        {
            m_pMaterial->SetUINT(ShaderIDs::g_CoarseLumTexIndex, m_lumRTs[level + 1]->GetSRVResourceHeapIndex(), passID);
            m_pMaterial->SetUINT(ShaderIDs::g_CoarseResultTexIndex, m_resultRTs[level + 1]->GetSRVResourceHeapIndex(), passID);
        }
        m_pMaterial->SetUINT(ShaderIDs::g_DestResultTexIndex, m_resultRTs[level]->GetUAVResourceHeapIndex(), passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        auto group = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide(m_levelSize[level].x, group.x),
                             CeilDivide(m_levelSize[level].y, group.y),
                             group.z);
        m_pCommand->AddUAVBarrier(m_resultRTs[level]);
        m_pCommand->AddBarrier(m_resultRTs[level], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void LocalExposureFusionPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().autoExposureParameter.fusionEnable || m_levelCount == 0 || !m_pDisplayRT)
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "Local Exposure Fusion");
        m_pGPUTimer = context.pGPUTimer;
        DispatchSetup();
        {
            auto passID = FUSION_DOWNSAMPLE_PASS;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto passName = passData.Name.c_str();
            PIXHelper tempPix(m_pCommand->GetCommandList(), passName);
            
            for (UINT level = 1; level < m_levelCount; ++level)
                DispatchDownsample(level);
            
            m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Local Exposure Fusion/") + passName).c_str());
        }
        
        {
            auto passID = FUSION_BLEND_PASS;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto passName = passData.Name.c_str();
            PIXHelper tempPix(m_pCommand->GetCommandList(), passName);
            
            for (int level = (int)m_levelCount - 1; level >= 0; --level)
                DispatchBlend((UINT)level);
            
            m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Local Exposure Fusion/") + passName).c_str());
        }
        
    }
}
