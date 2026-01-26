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
        m_HIZMipmapCount = UINT(std::floor(std::log2(std::max(m_renderSize.x, m_renderSize.y)))) + 1;
        m_HIZMipmapCount /= 2;

        m_pHIZRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_renderSize.x,
            m_renderSize.y,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            m_HIZMipmapCount,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::HIZRTID));

        m_pAORT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_renderSize.x),
                                                                           static_cast<UINT64>(m_renderSize.y),
                                                                           DXGI_FORMAT_R16G16B16A16_FLOAT,
                                                                           true,
                                                                           RenderResource::GetInstance().
                                                                           GetPropertyName(
                                                                               RenderTextureIDs::AORTID));

        m_pHalfAORT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_renderSize.x) / 2,
                                                                               static_cast<UINT64>(m_renderSize.y) / 2,
                                                                               DXGI_FORMAT_R16G16B16A16_FLOAT,
                                                                               true,
                                                                               RenderResource::GetInstance().
                                                                               GetPropertyName(
                                                                                   RenderTextureIDs::HalfAORTID));

        m_pOneFourAORT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x) / 4,
            static_cast<UINT64>(m_renderSize.y) / 4,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::OneFourAORTID));

        m_pTAA0RT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::TAA0RTID));

        m_pTAA1RT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::TAA1RTID));

        m_pBlurHorizonRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::AOBlurHorizonRTID));

        m_pBlurVerticalRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::AOBlurVerticalRTID));

        m_blueNoise = TextureManager::GetInstance().LoadResidentTexture(
            L"Tex\\RandomNormalTexture.dds");

        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "HIZ Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_AOHIZNormal.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"AOHIZNormal",
            },
            ShaderPass
            {
                .Name = "Calc AO Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_AO.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"HBAOPlus",
            },
            ShaderPass
            {
                .Name = "AO TAA Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_AOTAA.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"TAA",
            },
            ShaderPass
            {
                .Name = "AO Bilateral Blur Horizon Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_Bilateral_Blur.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"HorizionBilateralBlur",
            },
            ShaderPass
            {
                .Name = "AO Bilateral Blur Vertical Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_Bilateral_Blur.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"VerticalBilateralBlur",
            },
        };
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        ShaderPasseIDs::HIZPassID = m_pMaterial->FindPassIndex("HIZ Pass");
        ShaderPasseIDs::AOPassID = m_pMaterial->FindPassIndex("Calc AO Pass");
        ShaderPasseIDs::TAAPassID = m_pMaterial->FindPassIndex("AO TAA Pass");
        ShaderPasseIDs::BlurHorizonPassID = m_pMaterial->FindPassIndex("AO Bilateral Blur Horizon Pass");
        ShaderPasseIDs::BlurVerticalPassID = m_pMaterial->FindPassIndex("AO Bilateral Blur Vertical Pass");

        UpdatePipeline();

        if (m_kernels.empty())
            m_kernels = GenerateHBAOSampleKernel();

        if (m_blurWeights.empty())
            m_blurWeights = GenerateBlurWeights(MAX_BLUR_RADIUS);
    }

    void AOPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "AO Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        IsDebugLayerEnabled(m_pDevice->GetDevice());

        DoHIZ();
        DoSSAO();
        DoTAA();
        if (UserData::GetInstance().aoParameter.IsBlur)
        {
            DoBilateralBlurHorizon();
            DoBilateralBlurVerical();
            m_pCommand->CopyTexture(m_pBlurVerticalRT, m_pAORT);
        }

    }

    void AOPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::HIZPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::AOPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::TAAPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(m_pDevice, m_pMaterial.get(), passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::BlurHorizonPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(m_pDevice, m_pMaterial.get(), passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::BlurVerticalPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(m_pDevice, m_pMaterial.get(), passID);
        }
    }

    void AOPass::DoHIZ()
    {
        auto passID = ShaderPasseIDs::HIZPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        UINT64 currWidth = m_pHIZRT->GetWidth();
        UINT64 currHeight = m_pHIZRT->GetHeight();

        struct alignas(16)
        {
            Vector4 g_TargetSize;
            Vector4 g_SourceSize;
            UINT g_TargetTexIndex;
            UINT g_SourceTexIndex;
            float g_MipmapLevel;
        } constantData;
        constexpr UINT constantSize = sizeof(constantData) / 4;

        for (UINT i = 0; i < m_HIZMipmapCount; ++i)
        {
            if (i == 0)
            {
                constantData =
                {
                    GetScreenSize(currWidth, currHeight),
                    Vector4::Zero,
                    m_pHIZRT->GetUAVResourceHeapIndex(i),
                    UINT_MAX,
                    (float)i,
                };
                m_pCommand->SetPushConstants(PER_MATERIAL_SPACE, &constantData, constantSize);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                     CeilDivide(currHeight, threadGroupSize.y),
                                     threadGroupSize.z);
            }
            else
            {
                m_pCommand->AddUAVBarrier(m_pHIZRT);
                auto lastWidth = currWidth;
                auto lastHeight = currHeight;
                currWidth = MathHelper::Max(UINT64(1), currWidth >> 1);
                currHeight = MathHelper::Max(UINT64(1), currHeight >> 1);

                constantData =
                {
                    GetScreenSize(currWidth, currHeight),
                    GetScreenSize(lastWidth, lastHeight),
                    m_pHIZRT->GetUAVResourceHeapIndex(i),
                    m_pHIZRT->GetUAVResourceHeapIndex(i - 1),
                    (float)i,
                };
                m_pCommand->SetPushConstants(PER_MATERIAL_SPACE, &constantData, constantSize);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                     CeilDivide(currHeight, threadGroupSize.y),
                                     threadGroupSize.z);
            }
        }

        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoSSAO()
    {
        auto passID = ShaderPasseIDs::AOPassID;
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
        m_pMaterial->SetFloat2(ShaderIDs::g_ProjectScale,
                               Vector2(1.f / tan(m_pCamera->GetFOVY() * 0.5f),
                                       1.0f / tan((m_pCamera->GetFOVY() / m_pCamera->GetAspect()) * 0.5f)));

        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleCount, UserData::GetInstance().aoParameter.SampleCount, passID);
        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleStepCount,
                             UserData::GetInstance().aoParameter.SampleStepCount,
                             passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AORadius, UserData::GetInstance().aoParameter.Radius, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOFadeRadius, UserData::GetInstance().aoParameter.FadeRadius, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOFadeDistance, UserData::GetInstance().aoParameter.FadeDistance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOBias, UserData::GetInstance().aoParameter.Bias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityMul,
                              UserData::GetInstance().aoParameter.IntensityMul,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityPow,
                              UserData::GetInstance().aoParameter.IntensityPow,
                              passID);
        m_pMaterial->SetVector4Array(ShaderIDs::g_AOSampleKernelArray, m_kernels, passID);
        m_pMaterial->SetUInt(ShaderIDs::g_HIZMaxMipmap, MathHelper::Max(m_HIZMipmapCount - 1, UINT(0)), passID);
        m_pMaterial->SetUInt(ShaderIDs::g_HIZTextureIndex, m_pHIZRT->GetResourceHeapIndex(), passID);
        m_pMaterial->SetFloat(ShaderIDs::g_LerpAOFactor, UserData::GetInstance().aoParameter.AOLerpFactor, passID);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        {
            PIXHelper pix(m_pCommand->GetCommandList(), "One Four SSAO");
            auto targetRT = m_pOneFourAORT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale,
                                   Vector2(
                                       float(targetRT->GetWidth()) / float(m_blueNoise.GetWidth()),
                                       float(targetRT->GetHeight()) / float(m_blueNoise.GetHeight())),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMinMipmap, 2, passID);
            m_pMaterial->SetBool(ShaderIDs::g_bLerpAO, false, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        }

        {
            PIXHelper pix(m_pCommand->GetCommandList(), "Half SSAO");
            auto targetRT = m_pHalfAORT;
            auto sourceRT = m_pOneFourAORT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(sourceRT->GetWidth(), sourceRT->GetHeight()),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, sourceRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale,
                                   Vector2(
                                       float(targetRT->GetWidth()) / float(m_blueNoise.GetWidth()),
                                       float(targetRT->GetHeight()) / float(m_blueNoise.GetHeight())),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMinMipmap, 1, passID);
            m_pMaterial->SetBool(ShaderIDs::g_bLerpAO, UserData::GetInstance().aoParameter.IsLerpAO, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);

        }

        {
            PIXHelper pix(m_pCommand->GetCommandList(), "Full SSAO");
            auto targetRT = m_pAORT;
            auto sourceRT = m_pHalfAORT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(sourceRT->GetWidth(), sourceRT->GetHeight()),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, sourceRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale,
                                   Vector2(
                                       float(targetRT->GetWidth()) / float(m_blueNoise.GetWidth()),
                                       float(targetRT->GetHeight()) / float(m_blueNoise.GetHeight())),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMinMipmap, 0, passID);
            m_pMaterial->SetBool(ShaderIDs::g_bLerpAO, UserData::GetInstance().aoParameter.IsLerpAO, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        m_pCommand->FlushBarrier();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoTAA()
    {
        auto passID = ShaderPasseIDs::TAAPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();

        PIXHelper pix(m_pCommand->GetCommandList(), passName);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;

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
            m_pCommand->CopyTexture(AORT, currTAART);
            m_pCommand->CopyTexture(AORT, historyTAART);

            TAAData::Pre_View_M = m_pCamera->GetViewMat();
            TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
            TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
            TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
            TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
            TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert();

            m_isFirstFrame = false;

            return;
        }
        else
        {
            m_pCommand->SetPipeline(pipelineStateData);

            m_pCommand->AddBarrier(currTAART, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

                m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix, TAAData::Pre_View_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix_I, TAAData::Pre_View_I_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix, TAAData::Pre_Proj_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix_I, TAAData::Pre_Proj_I_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix, TAAData::Pre_ViewProj_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix_I, TAAData::Pre_ViewProj_I_M, passID);

                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(currTAART->GetWidth(), currTAART->GetHeight()),
                                       passID);
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, currTAART->GetResourceHeapIndex(), passID);
                m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, historyTAART->GetResourceHeapIndex(), passID);
                m_pMaterial->SetFloat(ShaderIDs::g_BlendWeight,
                                      UserData::GetInstance().aoParameter.TAALerpFactor,
                                      passID);

                TAAData::Pre_View_M = m_pCamera->GetViewMat();
                TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
                TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
                TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
                TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
                TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert();

                SetSpaceResource(passData, PER_PASS_SPACE);
                SetSpaceResource(passData, PER_FRAME_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currTAART->GetWidth(), threadGroupSize.x),
                                     CeilDivide(currTAART->GetHeight(), threadGroupSize.y),
                                     threadGroupSize.z);
                m_pCommand->AddBarrier(currTAART, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            }

            m_pCommand->CopyTexture(currTAART, AORT);
            m_currHistoryIndex = (m_currHistoryIndex + 1) % 2;
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoBilateralBlurHorizon()
    {
        auto passID = ShaderPasseIDs::BlurHorizonPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);

        auto targetRT = m_pBlurHorizonRT;
        auto sourceRT = m_pAORT;

        {
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex());
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, sourceRT->GetResourceHeapIndex());
            m_pMaterial->SetUInt(ShaderIDs::g_HIZTextureIndex, m_pHIZRT->GetResourceHeapIndex());
            m_pMaterial->SetUInt(ShaderIDs::g_BlurRadius, UserData::GetInstance().aoParameter.BlurIntensity);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize, GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()));
            m_pMaterial->SetFloat(ShaderIDs::g_Sharpness, UserData::GetInstance().aoParameter.Sharpness);
            m_pMaterial->SetFloatArray(ShaderIDs::g_Weights, m_blurWeights);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(targetRT, false);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void AOPass::DoBilateralBlurVerical()
    {
        auto passID = ShaderPasseIDs::BlurVerticalPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);

        auto targetRT = m_pBlurVerticalRT;
        auto sourceRT = m_pBlurHorizonRT;

        {
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex());
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, sourceRT->GetResourceHeapIndex());
            m_pMaterial->SetUInt(ShaderIDs::g_HIZTextureIndex, m_pHIZRT->GetResourceHeapIndex());

            m_pMaterial->SetUInt(ShaderIDs::g_BlurRadius, UserData::GetInstance().aoParameter.BlurIntensity);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize, GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()));
            m_pMaterial->SetFloat(ShaderIDs::g_Sharpness, UserData::GetInstance().aoParameter.Sharpness);
            m_pMaterial->SetFloatArray(ShaderIDs::g_Weights, m_blurWeights);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);

            m_pCommand->AddUAVBarrier(targetRT, false);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    std::vector<Vector4> AOPass::GenerateSSAOSampleKernel()
    {
        int maxSampleCount = 16;
        maxSampleCount = MathHelper::Min(UserData::GetInstance().aoParameter.SampleCount, maxSampleCount);
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