#include "stdafx.h"
#include "../public/SSGIPass.h"

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

namespace ElysiaRenderer
{
    namespace
    {
        float ClampPositive(float value, float minValue)
        {
            return value < minValue ? minValue : value;
        }
    }

    SSGIPass::SSGIPass() = default;
    SSGIPass::~SSGIPass() = default;

    void SSGIPass::Dispose()
    {
    }

    void SSGIPass::Configure()
    {
        m_radianceValid = false;
        m_indirectValid = false;
        m_indirectRead = 0;
        m_fullWidth = 0;
        m_fullHeight = 0;
        m_halfWidth = 0;
        m_halfHeight = 0;
        m_pRawRT = nullptr;
        m_pIndirectRT[0] = nullptr;
        m_pIndirectRT[1] = nullptr;
        m_pRadianceHistoryRT = nullptr;

        if (!m_pCameraColorRT)
            return;

        m_fullWidth = static_cast<UINT>(m_pCameraColorRT->GetWidth());
        m_fullHeight = static_cast<UINT>(m_pCameraColorRT->GetHeight());
        if (m_fullWidth < 1 || m_fullHeight < 1)
            return;

        m_halfWidth = std::max(1u, (m_fullWidth + 1u) / 2u);
        m_halfHeight = std::max(1u, (m_fullHeight + 1u) / 2u);

        auto& textures = RenderTargetManager::GetInstance();
        auto& names = RenderResource::GetInstance();
        m_pRawRT = textures.CreateRWRenderTexture(
            m_halfWidth,
            m_halfHeight,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            names.GetPropertyName(RenderTextureIDs::RawRTID));
        m_pIndirectRT[0] = textures.CreateRWRenderTexture(
            m_halfWidth,
            m_halfHeight,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            names.GetPropertyName(RenderTextureIDs::IndirectRTID) + L"0");
        m_pIndirectRT[1] = textures.CreateRWRenderTexture(
            m_halfWidth,
            m_halfHeight,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            names.GetPropertyName(RenderTextureIDs::IndirectRTID) + L"1");
        m_pRadianceHistoryRT = textures.CreateRWRenderTexture(
            m_fullWidth,
            m_fullHeight,
            m_pCameraColorRT->GetFormat(),
            true,
            names.GetPropertyName(RenderTextureIDs::RadianceRTID));

        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        if (!m_pMaterial)
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        UpdatePipeline();
    }

    void SSGIPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < SSGI_PASS_COUNT; ++i)
        {
            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto* variantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &variantManager->GetOrCompileVariantByNames({});
            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID);
        }
    }

    void SSGIPass::BindFrame(PassID passID)
    {
        const SSGIParameter& param = UserData::GetInstance().ssgiParameter;
        const UINT indirectWrite = 1u - m_indirectRead;
        BufferHandle preExposure = RenderPassResourceManager::GetInstance().Get<AutoExposureData>().pPreExposureBuffer;

        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_FullSize, GetScreenSize(m_fullWidth, m_fullHeight), passID);
        m_pMaterial->SetFloat4(ShaderIDs::g_HalfSize, GetScreenSize(m_halfWidth, m_halfHeight), passID);
        m_pMaterial->SetFloat(ShaderIDs::g_MaxDistance, ClampPositive(param.maxDistance, 0.1f), passID);
        m_pMaterial->SetFloat(ShaderIDs::g_Thickness, ClampPositive(param.thickness, 0.001f), passID);
        m_pMaterial->SetFloat(ShaderIDs::g_SelfBias, param.selfBias < 0.0f ? 0.0f : param.selfBias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_Intensity, param.intensity < 0.0f ? 0.0f : param.intensity, passID);
        float historyWeight = param.historyWeight;
        if (historyWeight < 0.0f)
            historyWeight = 0.0f;
        if (historyWeight > 0.98f)
            historyWeight = 0.98f;
        m_pMaterial->SetFloat(ShaderIDs::g_HistoryWeight, historyWeight, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_FrameIndex, m_frameIndex, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_RadianceHistorySRV,
                             m_pRadianceHistoryRT->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_RadianceHistoryUAV,
                             m_pRadianceHistoryRT->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_RawIndirectSRV, m_pRawRT->GetSRVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_RawIndirectUAV, m_pRawRT->GetUAVResourceHeapIndex(), passID);
        m_pMaterial->SetUINT(ShaderIDs::g_IndirectHistorySRV,
                             m_pIndirectRT[m_indirectRead]->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_IndirectDestSRV,
                             m_pIndirectRT[indirectWrite]->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_IndirectDestUAV,
                             m_pIndirectRT[indirectWrite]->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_CameraColorSRV,
                             m_pCameraColorRT->GetSRVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_CameraColorUAV,
                             m_pCameraColorRT->GetUAVResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureBufferIndex,
                             preExposure ? preExposure->GetUAVResourceHeapIndex() : 0u,
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_PreExposureEnable,
                             (preExposure && UsePreExposure(UserData::GetInstance().autoExposureParameter)) ? 1u : 0u,
                             passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HasRadianceHistory, m_radianceValid ? 1u : 0u, passID);
        m_pMaterial->SetUINT(ShaderIDs::g_HasIndirectHistory, m_indirectValid ? 1u : 0u, passID);
    }

    void SSGIPass::Dispatch(PassID passID, UINT width, UINT height)
    {
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        BindFrame(passID);
        SetSpaceResource(passData, PER_PASS_SPACE);

        auto group = passData.GetKernelThreadGroupSizes();
        m_pCommand->Dispatch(CeilDivide(width, group.x),
                             CeilDivide(height, group.y),
                             1);
    }

    void SSGIPass::Render(ElysiaEngine::FrameContext& context)
    {
        const SSGIParameter& param = UserData::GetInstance().ssgiParameter;
        if (!param.enable || m_fullWidth < 1 || !m_pRawRT || !m_pRadianceHistoryRT)
        {
            m_radianceValid = false;
            m_indirectValid = false;
            return;
        }

        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_frameIndex = static_cast<UINT>(context.frameIndex);
        if (!m_pCamera)
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "SSGI");

        if (m_radianceValid)
        {
            BufferHandle preExposure = RenderPassResourceManager::GetInstance().Get<AutoExposureData>().pPreExposureBuffer;
            m_pCommand->AddBarrier(m_pRadianceHistoryRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
            m_pCommand->AddBarrier(m_pRawRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
            if (preExposure)
            {
                m_pCommand->AddBarrier(preExposure,
                                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                       false);
            }
            m_pCommand->AddBarrier(m_pRawRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Dispatch(SSGI_TRACE_PASS, m_halfWidth, m_halfHeight);
            m_pCommand->AddUAVBarrier(m_pRawRT, false);
            m_pCommand->AddBarrier(m_pRawRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            const UINT indirectWrite = 1u - m_indirectRead;
            if (m_indirectValid)
            {
                m_pCommand->AddBarrier(m_pIndirectRT[m_indirectRead],
                                       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                       false);
            }
            m_pCommand->AddBarrier(m_pIndirectRT[indirectWrite], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Dispatch(SSGI_TEMPORAL_PASS, m_halfWidth, m_halfHeight);
            m_pCommand->AddUAVBarrier(m_pIndirectRT[indirectWrite], false);
            m_pCommand->AddBarrier(m_pIndirectRT[indirectWrite],
                                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Dispatch(SSGI_COMPOSITE_PASS, m_fullWidth, m_fullHeight);
            m_pCommand->AddUAVBarrier(m_pCameraColorRT);

            m_indirectRead = indirectWrite;
            m_indirectValid = true;
        }

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pRadianceHistoryRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Dispatch(SSGI_COPY_HISTORY_PASS, m_fullWidth, m_fullHeight);
        m_pCommand->AddUAVBarrier(m_pRadianceHistoryRT, false);
        m_pCommand->AddBarrier(m_pRadianceHistoryRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_radianceValid = true;
    }
}
