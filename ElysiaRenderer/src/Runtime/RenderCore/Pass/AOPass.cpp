#include "stdafx.h"
#include "AOPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Programs/SobolSequenceGenerator.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"
#include "Runtime/RenderCore/DX12Camera.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"


namespace ElysiaRenderer
{
    AOPass::AOPass()
        : BasePass()
    {
        m_DeinterleavedDepthRTs = std::vector<RenderTexture*>(DEINTERLEAVED_DEPTH_COUNT, nullptr);
        m_DeinterleavedAORTs = std::vector<RenderTexture*>(DEINTERLEAVED_DEPTH_COUNT, nullptr);

        m_DeinterleavedDepthIndices = std::vector<UINT>(DEINTERLEAVED_DEPTH_COUNT, UINT_MAX);
        m_DeinterleavedAOIndices = std::vector<UINT>(DEINTERLEAVED_DEPTH_COUNT, UINT_MAX);
    }
    AOPass::~AOPass()
    {
        Dispose();
    }
    void AOPass::Dispose()
    {
        m_kernels.clear();
    }

    void AOPass::Configure()
    {
        m_cameraWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_cameraHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);
        m_halfWidth = UINT(m_cameraWidth) >> 1;
        m_halfHeight = UINT(m_cameraHeight) >> 1;
        m_quarterWidth = UINT(m_cameraWidth) >> 2;
        m_quarterHeight = UINT(m_cameraHeight) >> 2;
        m_DeinterleavedDepthWidth = m_halfWidth;
        m_DeinterleavedDepthHeight = m_halfHeight;
        m_DeinterleavedAOWidth = m_halfWidth;
        m_DeinterleavedAOHeight = m_halfHeight;
        m_DeinterleavedBlurWidth = m_halfWidth;
        m_DeinterleavedBlurHeight = m_halfHeight;
        m_ImportanceWidth = m_quarterWidth;
        m_ImportanceHeight = m_quarterHeight;

        m_HIZMipmapCount = UINT(std::floor(std::log2(std::max(
                               m_DeinterleavedDepthWidth,
                               m_DeinterleavedDepthHeight)))) / 2;

        for (UINT i = 0; i < DEINTERLEAVED_DEPTH_COUNT; ++i)
        {
            m_DeinterleavedDepthRTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                m_DeinterleavedDepthWidth,
                m_DeinterleavedDepthHeight,
                DXGI_FORMAT_R16_FLOAT,
                true,
                m_HIZMipmapCount,
                RenderResource::GetInstance().GetPropertyName(
                    PropertyToID(L"Deinterleaved Depth RT" + std::to_wstring(i))));
            m_DeinterleavedDepthIndices[i] = m_DeinterleavedDepthRTs[i]->GetUAVResourceHeapIndex();

            m_DeinterleavedAORTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                m_DeinterleavedAOWidth,
                m_DeinterleavedAOHeight,
                DXGI_FORMAT_R8G8_UNORM,
                true,
                RenderResource::GetInstance().GetPropertyName(
                    PropertyToID(L"Deinterleaved AO RT" + std::to_wstring(i))));
            m_DeinterleavedAOIndices[i] = m_DeinterleavedAORTs[i]->GetUAVResourceHeapIndex();
        }

        m_pImportanceRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_ImportanceWidth,
            m_ImportanceHeight,
            DXGI_FORMAT_R8_UNORM,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::AOImportanceID));

        m_pAORT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_cameraWidth),
            static_cast<UINT64>(m_cameraHeight),
            DXGI_FORMAT_R8G8_UNORM,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::AORTID));

        m_pTAA0RT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_cameraWidth),
            static_cast<UINT64>(m_cameraHeight),
            DXGI_FORMAT_R8G8_UNORM,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::TAA0RTID));

        m_pTAA1RT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_cameraWidth),
            static_cast<UINT64>(m_cameraHeight),
            DXGI_FORMAT_R8G8_UNORM,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::TAA1RTID));

        m_blueNoise = TextureManager::GetInstance().LoadResidentTexture(
            L"Tex\\blue_noise.dds");

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }

        UpdatePipeline();

        if (m_kernels.empty())
            m_kernels = GenerateHBAOSampleKernel();

        if (m_blurWeights.empty())
            m_blurWeights = GenerateBlurWeights(MAX_BLUR_RADIUS);
    }

    void AOPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().aoParameter.IsEnableAO)
            return;
        PIXHelper pix(m_pCommand->GetCommandList(), "AO Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        for (UINT i = 0; i < DEINTERLEAVED_DEPTH_COUNT; ++i)
        {
            m_pCommand->AddBarrier(m_DeinterleavedDepthRTs[i],
                                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                                   false);
            m_pCommand->AddBarrier(m_DeinterleavedAORTs[i],
                                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                                   false);
        }
        m_pCommand->AddBarrier(m_pImportanceRT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
        m_pCommand->AddBarrier(m_pTAA0RT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
        m_pCommand->AddBarrier(m_pTAA1RT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        for (UINT i = 0; i < DEINTERLEAVED_DEPTH_COUNT; ++i)
        {
            m_pCommand->Discard(m_DeinterleavedDepthRTs[i]);
            m_pCommand->Discard(m_DeinterleavedAORTs[i]);
        }
        m_pCommand->Discard(m_pImportanceRT);
        m_pCommand->Discard(m_pAORT);
        m_pCommand->Discard(m_pTAA0RT);
        m_pCommand->Discard(m_pTAA1RT);

        DoDeinterleaveDepth();
        DoHIZ();
        DoDeinterleaveBaseAO();
        DoImportance();
        DoDeinterleaveCalcAO();
        DoReinterleave();
        if (UserData::GetInstance().aoParameter.IsTAA)
        {
            DoTAA();
        }
        DoBilateralBlur();
    }

    void AOPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < AO_PASS_COUNT; ++i)
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

    void AOPass::DoHIZ()
    {
        auto passID = AO_HIZ_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        for (auto pRT : m_DeinterleavedDepthRTs)
        {
            m_pCommand->AddBarrier(pRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        }
        m_pCommand->FlushBarrier();

        UINT64 currWidth = UINT64(m_cameraWidth) >> 1;
        UINT64 currHeight = UINT64(m_cameraHeight) >> 1;

        for (UINT i = 1; i < m_HIZMipmapCount; ++i)
        {
            auto lastWidth = currWidth;
            auto lastHeight = currHeight;
            currWidth = MathHelper::Max(UINT64(1), currWidth >> 1);
            currHeight = MathHelper::Max(UINT64(1), currHeight >> 1);

            m_pMaterial->SetFloat4(ShaderIDs::g_SourceTexIndices,
                                   Vector4(
                                       m_DeinterleavedDepthRTs[0]->GetUAVResourceHeapIndex(i - 1),
                                       m_DeinterleavedDepthRTs[1]->GetUAVResourceHeapIndex(i - 1),
                                       m_DeinterleavedDepthRTs[2]->GetUAVResourceHeapIndex(i - 1),
                                       m_DeinterleavedDepthRTs[3]->GetUAVResourceHeapIndex(i - 1)
                                       ),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetTexIndices,
                                   Vector4(
                                       m_DeinterleavedDepthRTs[0]->GetUAVResourceHeapIndex(i),
                                       m_DeinterleavedDepthRTs[1]->GetUAVResourceHeapIndex(i),
                                       m_DeinterleavedDepthRTs[2]->GetUAVResourceHeapIndex(i),
                                       m_DeinterleavedDepthRTs[3]->GetUAVResourceHeapIndex(i)
                                       ),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(currWidth, currHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(lastWidth, lastHeight),
                                   passID);
            m_pMaterial->SetFloat(ShaderIDs::g_AORadius,
                                  UserData::GetInstance().aoParameter.Radius,
                                  passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                 CeilDivide(currHeight, threadGroupSize.y),
                                 DEINTERLEAVED_DEPTH_COUNT);
        }

        for (auto pRT : m_DeinterleavedDepthRTs)
        {
            m_pCommand->AddBarrier(pRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        }
        m_pCommand->FlushBarrier();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoDeinterleaveDepth()
    {
        auto passID = Deinterleaved_Depth_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        for (auto pRT : m_DeinterleavedDepthRTs)
        {
            m_pCommand->AddBarrier(pRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        }
        m_pCommand->FlushBarrier();

        {
            auto projMat = m_pCamera->GetProjMat();

            // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
            // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
            float depthLinearizeMul = -projMat.m[3][2];
            float depthLinearizeAdd = projMat.m[2][2];

            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex,
                                 m_pCameraDepthRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetTexIndices,
                                   Vector4(m_DeinterleavedDepthRTs[0]->GetUAVResourceHeapIndex(),
                                           m_DeinterleavedDepthRTs[1]->GetUAVResourceHeapIndex(),
                                           m_DeinterleavedDepthRTs[2]->GetUAVResourceHeapIndex(),
                                           m_DeinterleavedDepthRTs[3]->GetUAVResourceHeapIndex()),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_cameraWidth, m_cameraHeight));
            m_pMaterial->SetFloat2(ShaderIDs::g_DepthUnpackConsts,
                                   Vector2(depthLinearizeMul, depthLinearizeAdd),
                                   passID
                );
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_cameraWidth, threadGroupSize.x),
                                 CeilDivide(m_cameraHeight, threadGroupSize.y),
                                 threadGroupSize.z);
        }

        for (auto pRT : m_DeinterleavedDepthRTs)
        {
            m_pCommand->AddBarrier(pRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        }
        m_pCommand->FlushBarrier();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoDeinterleaveBaseAO()
    {
        auto passID = Deinterleaved_AO_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        for (auto pRT : m_DeinterleavedAORTs)
        {
            m_pCommand->AddBarrier(pRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        }
        m_pCommand->FlushBarrier();

        {
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I,
                                   m_pCamera->GetViewMat().Invert(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I,
                                   m_pCamera->GetProjMat().Invert(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                   passID);

            Vector2 cameraTanHalfFOV = {1 / m_pCamera->GetProjMat().m[0][0],
                                        1 / m_pCamera->GetProjMat().m[1][1]};
            m_pMaterial->SetFloat2(ShaderIDs::g_NDCToViewMul,
                                   Vector2(cameraTanHalfFOV.x * 2.f, -cameraTanHalfFOV.y * 2.f));
            m_pMaterial->SetFloat2(ShaderIDs::g_NDCToViewAdd,
                                   Vector2(-cameraTanHalfFOV.x, cameraTanHalfFOV.y));

            m_pMaterial->SetFloat(ShaderIDs::g_AORadius,
                                  UserData::GetInstance().aoParameter.Radius,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_AOBias,
                                  UserData::GetInstance().aoParameter.Bias,
                                  passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMaxMipmap,
                                 MathHelper::Max(m_HIZMipmapCount - 1, UINT(0)),
                                 passID);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_DeinterleavedAOWidth, m_DeinterleavedAOHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_FullScreenSize,
                                   GetScreenSize(m_cameraWidth, m_cameraHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetTexIndices,
                                   Vector4(m_DeinterleavedAOIndices[0],
                                           m_DeinterleavedAOIndices[1],
                                           m_DeinterleavedAOIndices[2],
                                           m_DeinterleavedAOIndices[3]),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceTexIndices,
                                   Vector4(m_DeinterleavedDepthIndices[0],
                                           m_DeinterleavedDepthIndices[1],
                                           m_DeinterleavedDepthIndices[2],
                                           m_DeinterleavedDepthIndices[3]),
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_DeinterleavedAOWidth, threadGroupSize.x),
                                 CeilDivide(m_DeinterleavedAOHeight, threadGroupSize.y),
                                 DEINTERLEAVED_DEPTH_COUNT);
        }

        for (auto pRT : m_DeinterleavedAORTs)
        {
            m_pCommand->AddBarrier(pRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        }
        m_pCommand->FlushBarrier();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoImportance()
    {
        {
            auto passID = Generate_AO_Importance_PASS;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto passName = passData.Name.c_str();
            PIXHelper pix(m_pCommand->GetCommandList(), passName);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                     passID)
                                                                 .pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto targetRT = m_pImportanceRT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            {
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex());
                m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleavedAOSize,
                                       GetScreenSize(m_DeinterleavedAOWidth,
                                                     m_DeinterleavedAOHeight));
                m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityMul,
                                      UserData::GetInstance().aoParameter.IntensityMul);
                m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityPow,
                                      UserData::GetInstance().aoParameter.IntensityPow);
                m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleaveAOTexIndices,
                                       Vector4(m_DeinterleavedAOIndices[0],
                                               m_DeinterleavedAOIndices[1],
                                               m_DeinterleavedAOIndices[2],
                                               m_DeinterleavedAOIndices[3]),
                                       passID);
                SetSpaceResource(passData, PER_PASS_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(m_ImportanceWidth, threadGroupSize.x),
                                     CeilDivide(m_ImportanceHeight, threadGroupSize.y),
                                     threadGroupSize.z);
            }

            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        {
            auto passID = Post_AO_Importance_A;
            auto& passData = m_pMaterial->GetPassData(passID);
            PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                     passID)
                                                                 .pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto targetRT = m_pImportanceRT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            {
                m_pMaterial->SetFloat4(ShaderIDs::g_ImportanceBufferSize,
                                       GetScreenSize(m_ImportanceWidth, m_ImportanceHeight),
                                       passID);
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex,
                                     targetRT->GetResourceHeapIndex(),
                                     passID);
                SetSpaceResource(passData, PER_PASS_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(m_ImportanceWidth, threadGroupSize.x),
                                     CeilDivide(m_ImportanceHeight, threadGroupSize.y),
                                     threadGroupSize.z);
            }

            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        {
            auto passID = Post_AO_Importance_B;
            auto& passData = m_pMaterial->GetPassData(passID);
            PIXHelper pix(m_pCommand->GetCommandList(), passData.Name.c_str());

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                     passID)
                                                                 .pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto targetRT = m_pImportanceRT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            {
                m_pMaterial->SetFloat4(ShaderIDs::g_ImportanceBufferSize,
                                       GetScreenSize(m_ImportanceWidth, m_ImportanceHeight),
                                       passID);
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex,
                                     targetRT->GetResourceHeapIndex(),
                                     passID);
                SetSpaceResource(passData, PER_PASS_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(m_ImportanceWidth, threadGroupSize.x),
                                     CeilDivide(m_ImportanceHeight, threadGroupSize.y),
                                     threadGroupSize.z);
            }

            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(),
                                  m_shaderPasses[Generate_AO_Importance_PASS].Name.c_str());
    }
    void AOPass::DoDeinterleaveCalcAO()
    {
        auto passID = Calc_AO_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();

        PIXHelper pix(m_pCommand->GetCommandList(), passName);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);

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

        Vector2 cameraTanHalfFOV = {1 / m_pCamera->GetProjMat().m[0][0],
                                    1 / m_pCamera->GetProjMat().m[1][1]};
        m_pMaterial->SetFloat2(ShaderIDs::g_NDCToViewMul,
                               Vector2(cameraTanHalfFOV.x * 2.f, -cameraTanHalfFOV.y * 2.f));
        m_pMaterial->SetFloat2(ShaderIDs::g_NDCToViewAdd,
                               Vector2(-cameraTanHalfFOV.x, cameraTanHalfFOV.y));

        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleCount,
                             UserData::GetInstance().aoParameter.SampleCount,
                             passID);
        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleStepCount,
                             UserData::GetInstance().aoParameter.SampleStepCount,
                             passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AORadius,
                              UserData::GetInstance().aoParameter.Radius,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOFadeRadius,
                              UserData::GetInstance().aoParameter.FadeRadius,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOFadeDistance,
                              UserData::GetInstance().aoParameter.FadeDistance,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOBias,
                              UserData::GetInstance().aoParameter.Bias,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityMul,
                              UserData::GetInstance().aoParameter.IntensityMul,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityPow,
                              UserData::GetInstance().aoParameter.IntensityPow,
                              passID);

        SetSpaceResource(passData, PER_FRAME_SPACE);

        for (UINT i = 0; i < DEINTERLEAVED_DEPTH_COUNT; ++i)
        {
            m_pCommand->AddBarrier(m_DeinterleavedAORTs[i],
                                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                   false);
        }
        m_pCommand->FlushBarrier();
        {
            m_pMaterial->SetFloat4(ShaderIDs::g_ImportanceBufferSize,
                                   GetScreenSize(m_ImportanceWidth, m_ImportanceHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleavedAOSize,
                                   GetScreenSize(m_DeinterleavedAOWidth, m_DeinterleavedAOHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_FullScreenSize,
                                   GetScreenSize(m_cameraWidth, m_cameraHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleaveAOTexIndices,
                                   Vector4(m_DeinterleavedAOIndices[0],
                                           m_DeinterleavedAOIndices[1],
                                           m_DeinterleavedAOIndices[2],
                                           m_DeinterleavedAOIndices[3]),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleaveDepthTexIndices,
                                   Vector4(m_DeinterleavedDepthIndices[0],
                                           m_DeinterleavedDepthIndices[1],
                                           m_DeinterleavedDepthIndices[2],
                                           m_DeinterleavedDepthIndices[3]),
                                   passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale,
                                   Vector2(
                                       m_cameraWidth / float(m_blueNoise.GetWidth()),
                                       m_cameraHeight / float(m_blueNoise.GetHeight())),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMaxMipmap,
                                 MathHelper::Max(m_HIZMipmapCount - 1, UINT(0)),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_AOImportanceTexIndex,
                                 m_pImportanceRT->GetResourceHeapIndex(),
                                 passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_DeinterleavedAOWidth, threadGroupSize.x),
                                 CeilDivide(m_DeinterleavedAOHeight, threadGroupSize.y),
                                 DEINTERLEAVED_DEPTH_COUNT);
        }
        for (UINT i = 0; i < DEINTERLEAVED_DEPTH_COUNT; ++i)
        {
            m_pCommand->AddBarrier(m_DeinterleavedAORTs[i],
                                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                   false);
        }
        m_pCommand->FlushBarrier();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoBilateralBlur()
    {
        auto passID = Deinterleaved_Blur_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto inputIndices = m_DeinterleavedAOIndices;
        auto outputIndices = m_DeinterleavedAOIndices;

        auto targetRT = m_pAORT;
        for (UINT i = 0; i <= UserData::GetInstance().aoParameter.BlurCount; ++i)
        {
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetUInt(ShaderIDs::g_BlurTexIndex,
                                 targetRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ReinterleaveAOTexIndex,
                                 targetRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_AOImportanceTexIndex,
                                 targetRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_FullScreenSize,
                                   GetScreenSize(m_cameraWidth, m_cameraHeight),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_BlurRadius,
                                 i + 1,
                                 passID);

            m_pMaterial->SetFloat(ShaderIDs::g_Sharpness_Inv,
                                  1.f - UserData::GetInstance().aoParameter.Sharpness);
            m_pMaterial->SetUInt(ShaderIDs::g_IsBlur,
                                 UserData::GetInstance().aoParameter.IsBlur);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_cameraWidth, threadGroupSize.x),
                                 CeilDivide(m_cameraHeight, threadGroupSize.y),
                                 threadGroupSize.z);

            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            std::swap(inputIndices, outputIndices);
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoReinterleave()
    {
        auto passID = AO_Reinterleave_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto targetRT = m_pAORT;
        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleaveAOTexIndices,
                                   Vector4(m_DeinterleavedAOIndices[0],
                                           m_DeinterleavedAOIndices[1],
                                           m_DeinterleavedAOIndices[2],
                                           m_DeinterleavedAOIndices[3]),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DeinterleaveDepthTexIndices,
                                   Vector4(m_DeinterleavedDepthIndices[0],
                                           m_DeinterleavedDepthIndices[1],
                                           m_DeinterleavedDepthIndices[2],
                                           m_DeinterleavedDepthIndices[3]),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ReinterleaveAOTexIndex,
                                 targetRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_ImportanceBufferSize,
                                   GetScreenSize(m_ImportanceWidth, m_ImportanceHeight),
                                   passID);
            m_pMaterial->SetFloat(ShaderIDs::g_Sharpness_Inv,
                                  1.f - UserData::GetInstance().aoParameter.Sharpness);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
        }
        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoTAA()
    {
        auto passID = AO_TAA_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();

        PIXHelper pix(m_pCommand->GetCommandList(), passName);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        RenderTexture* AORT = m_pAORT;
        RenderTexture* currTAART = nullptr;
        RenderTexture* historyTAART = nullptr;

        if (m_currHistoryIndex == 0)
        {
            historyTAART = m_pTAA0RT;
            currTAART = m_pTAA1RT;
        }
        else
        {
            historyTAART = m_pTAA1RT;
            currTAART = m_pTAA0RT;
        }

        if (m_isFirstFrame)
        {
            m_pCommand->AddBarrier(AORT, D3D12_RESOURCE_STATE_COPY_SOURCE, false);
            m_pCommand->AddBarrier(currTAART, D3D12_RESOURCE_STATE_COPY_DEST, false);
            m_pCommand->AddBarrier(historyTAART, D3D12_RESOURCE_STATE_COPY_DEST);
            m_pCommand->CopyTexture(AORT, currTAART);
            m_pCommand->CopyTexture(AORT, historyTAART);
            m_pCommand->AddBarrier(AORT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            TAAData::Pre_View_M = m_pCamera->GetViewMat();
            TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
            TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
            TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
            TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
            TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).
                Invert();

            m_isFirstFrame = false;

            return;
        }
        else
        {
            m_pCommand->AddBarrier(currTAART, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            {
                m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
                m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I,
                                       m_pCamera->GetViewMat().Invert(),
                                       passID);
                m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
                m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I,
                                       m_pCamera->GetProjMat().Invert(),
                                       passID);
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                       m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                       passID);
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                       (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                       passID);

                m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix, TAAData::Pre_View_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix_I, TAAData::Pre_View_I_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix, TAAData::Pre_Proj_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix_I, TAAData::Pre_Proj_I_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix,
                                       TAAData::Pre_ViewProj_M,
                                       passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix_I,
                                       TAAData::Pre_ViewProj_I_M,
                                       passID);

                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(currTAART->GetWidth(), currTAART->GetHeight()),
                                       passID);
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex,
                                     currTAART->GetResourceHeapIndex(),
                                     passID);
                m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex,
                                     historyTAART->GetResourceHeapIndex(),
                                     passID);
                m_pMaterial->SetUInt(ShaderIDs::g_AOImportanceTexIndex,
                                     m_pImportanceRT->GetResourceHeapIndex(),
                                     passID);
                m_pMaterial->SetFloat(ShaderIDs::g_BlendWeight,
                                      UserData::GetInstance().aoParameter.TAALerpFactor,
                                      passID);
                m_pMaterial->SetFloat(ShaderIDs::g_Sharpness_Inv,
                                      1.f - UserData::GetInstance().aoParameter.Sharpness,
                                      passID);
                SetSpaceResource(passData, PER_PASS_SPACE);

                TAAData::Pre_View_M = m_pCamera->GetViewMat();
                TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
                TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
                TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
                TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
                TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).
                    Invert();

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currTAART->GetWidth(), threadGroupSize.x),
                                     CeilDivide(currTAART->GetHeight(), threadGroupSize.y),
                                     threadGroupSize.z);

            }
            m_pCommand->AddBarrier(currTAART, D3D12_RESOURCE_STATE_COPY_SOURCE);
            m_pCommand->CopyTexture(currTAART, AORT);
            m_pCommand->AddBarrier(currTAART, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            m_currHistoryIndex = (m_currHistoryIndex + 1) % 2;
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    std::vector<Vector4> AOPass::GenerateSSAOSampleKernel()
    {
        int maxSampleCount = 16;
        maxSampleCount = MathHelper::Min(UserData::GetInstance().aoParameter.SampleCount,
                                         maxSampleCount);
        std::vector<Vector4> o{size_t(maxSampleCount)};

        SobolSequenceGenerator sobol(2);
        uint32_t* C = sobol.rightmostZeroBit(maxSampleCount);

        for (UINT32 i = 0; i < maxSampleCount; ++i)
        {
            auto point = sobol.nextPoint(C[i]);
            float u1 = static_cast<float>(point[0]);
            float u2 = static_cast<float>(point[1]);

            // 核心：使用余弦加权映射到半球 (Cosine-weighted Hemisphere)
            float phi = 2.0f * 3.14159265f * u1;
            float cosTheta = sqrt(1.0f - u2);
            float sinTheta = sqrt(u2);

            Vector4 v;
            v.x = cos(phi) * sinTheta;
            v.y = sin(phi) * sinTheta;
            v.z = cosTheta; // Z轴始终为正，保证在半球内
            v.w = 0.0f;

            // 距离缩放（靠近原点的点权重更高，增强接触阴影）
            float scale = (float)i / (float)maxSampleCount;
            scale = 0.1f + 0.9f * (scale * scale); // Lerp(0.1, 1.0, scale^2)

            o[i] = v * scale;
        }

        return o;
    }
    std::vector<Vector4> AOPass::GenerateHBAOSampleKernel()
    {
        int maxSampleCount = 16;
        int sampleCount = std::min(UserData::GetInstance().aoParameter.SampleCount, maxSampleCount);

        std::vector<Vector4> kernel;
        kernel.reserve(sampleCount);

        // --- 随机数引擎 (用于旋转和步长抖动) ---
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis01(0.0f, 1.0f);
        std::uniform_real_distribution<float> disRot(0.0f, 2.0f * 3.14159265f); // 旋转角度

        // --- 核心生成逻辑 ---
        // 使用类似 Fibonacci 的螺旋分布来均匀填充半球
        // 这种方法能保证点与点之间距离大致相等
        float goldenRatio = (1.0f + std::sqrt(5.0f)) * 0.5f; // 黄金比例

        for (int i = 0; i < sampleCount; ++i)
        {
            // --- 1. 生成均匀的球面坐标 ---
            // 使用黄金比例分割法，避免点聚集
            float t = float(i) / float(sampleCount);

            // 在半球上采样，所以 y (Vy) 从 0 到 1 (对应角度 90度到 0度)
            // 使用 sqrt 来修正分布，使其在平面上看起来更均匀
            float y = 1.0f - t;                // 从 1 (顶部) 到 0 (赤道)
            float r = std::sqrt(1.0f - y * y); // 计算圆的半径

            // 引入黄金比例旋转，使角度分布更均匀
            float theta = 2.0f * 3.14159265f * float(i) / goldenRatio;

            // 转换为笛卡尔坐标 (切线空间)
            float x = std::cos(theta) * r;
            float z = std::sin(theta) * r; // 这里 z 实际上是切线空间的 V 分量

            // --- 2. 随机旋转 (模拟切线空间的旋转，防止闪烁) ---
            // 生成一个随机旋转角度
            float angle = disRot(gen);
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);

            // 将生成的点绕 Z 轴旋转 (在切线平面内旋转)
            float rotatedX = x * cosA - z * sinA;
            float rotatedY = x * sinA + z * cosA;

            // --- 3. 打包数据 ---
            // X, Y: 归一化的采样方向 (在切线平面内)
            // Z:   随机步长抖动 (0-1)，用于 Shader 中的 step jitter
            // W:   预留 (通常为 1.0)
            float randomStepJitter = dis01(gen);

            kernel.emplace_back(rotatedX, rotatedY, randomStepJitter, 1.0f);
        }

        return kernel;
    }
    std::vector<float> AOPass::GenerateBlurWeights(UINT blurRadius)
    {
        // 如果传入的 sigma 为 0 或负数，则自动应用你发现的黄金比例：sigma = radius * 0.5
        // 这样可以确保即便 UI 没有调整 sigma，效果也是正确的
        float sigma = static_cast<float>(blurRadius) * 0.5f;

        // 防止除以 0 (当半径为 0 时)
        sigma = std::max(sigma, 0.0001f);

        std::vector<float> weights(blurRadius + 1, 0.0f);
        float sum = 0.0f;

        // 高斯公式分母: 2 * sigma^2
        const float denominator = 2.0f * sigma * sigma;

        // 1. 计算原始高斯分布值
        for (int i = 0; i <= (int)blurRadius; ++i)
        {
            // G(i) = exp(-i^2 / (2 * sigma^2))
            weights[i] = expf(-(static_cast<float>(i * i)) / denominator);

            // 累加总权重用于归一化
            // 注意：中心点 i=0 只计一次，其余偏移点 i>0 在卷积时会覆盖左右/上下两端，故计两次
            sum += (i == 0) ? weights[i] : (2.0f * weights[i]);
        }

        // 2. 归一化 (Normalization)
        // 这一步至关重要，确保模糊后的 AO 亮度（能量）守恒，不会变亮或变暗
        for (int i = 0; i <= (int)blurRadius; ++i)
        {
            weights[i] /= sum;
        }

        return weights;
    }

}