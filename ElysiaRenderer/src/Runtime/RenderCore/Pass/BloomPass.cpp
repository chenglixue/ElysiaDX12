#include "stdafx.h"
#include "BloomPass.h"

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
    BloomPass::BloomPass()
    {

    }
    BloomPass::~BloomPass()
    {
        Dispose();
    }
    void BloomPass::Dispose()
    {

    }

    void BloomPass::Configure()
    {
        m_cameraWidth = m_renderSize.x;
        m_cameraHeight = m_renderSize.y;

        m_mipmapResolutions[0] = UINT2(m_cameraWidth, m_cameraHeight);
        for (UINT i = 1; i < m_mipmapCount; ++i)
        {
            UINT lastWidth = m_mipmapResolutions[i - 1].x;
            UINT lastHeight = m_mipmapResolutions[i - 1].y;
            UINT currWidth = (lastWidth + 1) >> 1;
            UINT currHeight = (lastHeight + 1) >> 1;

            m_mipmapResolutions[i] = UINT2(currWidth, currHeight);
        }

        for (UINT i = 0; i < m_mipmapCount; ++i)
        {
            m_downSampleRTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(m_mipmapResolutions[i].x,
                                                                                          m_mipmapResolutions[i].y,
                                                                                          !UserData::GetInstance().
                                                                                          IsUseHDR
                                                                                              ? DXGI_FORMAT_R8G8B8A8_UNORM
                                                                                              : UserData::GetInstance().
                                                                                                HDRLevel
                                                                                                ==
                                                                                                HDRQuality::High
                                                                                              ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                                                                              : DXGI_FORMAT_R11G11B10_FLOAT,
                                                                                          true,
                                                                                          RenderResource::GetInstance().
                                                                                          GetPropertyName(
                                                                                              RenderTextureIDs::BloomDownSampleRTID
                                                                                              ) + std::to_wstring(i));
            m_upSampleRTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(m_mipmapResolutions[i].x,
                                                                                        m_mipmapResolutions[i].y,
                                                                                        !UserData::GetInstance().
                                                                                        IsUseHDR
                                                                                            ? DXGI_FORMAT_R8G8B8A8_UNORM
                                                                                            : UserData::GetInstance().
                                                                                              HDRLevel
                                                                                              ==
                                                                                              HDRQuality::High
                                                                                            ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                                                                            : DXGI_FORMAT_R11G11B10_FLOAT,
                                                                                        true,
                                                                                        RenderResource::GetInstance().
                                                                                        GetPropertyName(
                                                                                            RenderTextureIDs::BloomUpSampleRTID)
                                                                                        + std::to_wstring(i));
        }

        m_pBloomRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(m_cameraWidth,
                                                                              m_cameraHeight,
                                                                              !UserData::GetInstance().IsUseHDR
                                                                                  ? DXGI_FORMAT_R8G8B8A8_UNORM
                                                                                  : UserData::GetInstance().HDRLevel ==
                                                                                    HDRQuality::High
                                                                                  ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                                                                  : DXGI_FORMAT_R11G11B10_FLOAT,
                                                                              true,
                                                                              m_mipmapCount,
                                                                              RenderResource::GetInstance().
                                                                              GetPropertyName(
                                                                                  RenderTextureIDs::BloomRTID));
        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);

        UpdatePipeline();
    }
    void BloomPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < BLOOM_PASS_COUNT; ++i)
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
    void BloomPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().bloomParameter.enable)
            return;
        PIXHelper pix(m_pCommand->GetCommandList(), "Bloom Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "Bloom Begin");

        m_pCommand->CopyTextureRegion(m_pCameraColorRT, m_downSampleRTs[0]);
        DoBloomFirstDownSample();
        DoBloomWeightDownSample();
        DoBloom3x3TentUpSample();
        DoBloomBlendSceneColor();
        m_pCommand->CopyTexture(m_pBloomRT, m_pCameraColorRT);
    }

    void BloomPass::DoBloomFirstDownSample()
    {
        auto passID = BLOOM_FIRST_DOWN_SAMPLE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_downSampleRTs[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTextureIndex,
                                 m_downSampleRTs[0]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DestTextureIndexID,
                                 m_downSampleRTs[1]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(m_mipmapResolutions[0].x, m_mipmapResolutions[0].y),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                                   GetScreenSize(m_mipmapResolutions[1].x, m_mipmapResolutions[1].y),
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_mipmapResolutions[1].x, threadGroupSize.x),
                                 CeilDivide(m_mipmapResolutions[1].y, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_downSampleRTs[1], false);
        }
        m_pCommand->AddBarrier(m_downSampleRTs[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void BloomPass::DoBloomWeightDownSample()
    {
        auto passID = BLOOM_WEIGHT_DOWN_SAMPLE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        for (UINT i = 2; i < m_mipmapCount; ++i)
        {
            m_pCommand->AddBarrier(m_downSampleRTs[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetUInt(ShaderIDs::g_SourceTextureIndex,
                                 m_downSampleRTs[i - 1]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DestTextureIndexID,
                                 m_downSampleRTs[i]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(m_mipmapResolutions[i - 1].x, m_mipmapResolutions[i - 1].y),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                                   GetScreenSize(m_mipmapResolutions[i].x, m_mipmapResolutions[i].y),
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_mipmapResolutions[i].x, threadGroupSize.x),
                                 CeilDivide(m_mipmapResolutions[i].y, threadGroupSize.y),
                                 threadGroupSize.z);

            m_pCommand->AddUAVBarrier(m_downSampleRTs[i], false);
            m_pCommand->AddBarrier(m_downSampleRTs[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        }
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void BloomPass::DoBloom3x3TentUpSample()
    {
        auto passID = BLOOM_3X3TENT_UP_SAMPLE;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->CopyTextureRegion(m_downSampleRTs[m_mipmapCount - 1], m_upSampleRTs[m_mipmapCount - 1]);
        for (int i = m_mipmapCount - 2; i >= 0; --i)
        {
            m_pCommand->AddBarrier(m_upSampleRTs[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetUInt(ShaderIDs::g_SourceTextureIndex,
                                 m_downSampleRTs[i + 1]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DestTextureIndexID, m_upSampleRTs[i]->GetUAVResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DownSampleDestTexIndex,
                                 m_downSampleRTs[i]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(m_mipmapResolutions[i + 1].x, m_mipmapResolutions[i + 1].y),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                                   GetScreenSize(m_mipmapResolutions[i].x, m_mipmapResolutions[i].y),
                                   passID);
            m_pMaterial->SetFloat(ShaderIDs::g_BloomRadius, UserData::GetInstance().bloomParameter.radius, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_mipmapResolutions[i].x, threadGroupSize.x),
                                 CeilDivide(m_mipmapResolutions[i].y, threadGroupSize.y),
                                 threadGroupSize.z);

            m_pCommand->AddUAVBarrier(m_upSampleRTs[i], false);
            m_pCommand->AddBarrier(m_upSampleRTs[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        }
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void BloomPass::DoBloomBlendSceneColor()
    {
        auto passID = BLOOM_BLEND_SCENE_COLOR;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pBloomRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTextureIndex,
                                 m_upSampleRTs[0]->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DestTextureIndexID,
                                 m_pBloomRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(m_mipmapResolutions[0].x, m_mipmapResolutions[0].y),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                                   GetScreenSize(m_mipmapResolutions[0].x, m_mipmapResolutions[0].y),
                                   passID);
            m_pMaterial->SetFloat(ShaderIDs::g_BloomIntensity,
                                  UserData::GetInstance().bloomParameter.intensity,
                                  passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_mipmapResolutions[0].x, threadGroupSize.x),
                                 CeilDivide(m_mipmapResolutions[0].y, threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pBloomRT, false);
        }
        m_pCommand->AddBarrier(m_pBloomRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
}