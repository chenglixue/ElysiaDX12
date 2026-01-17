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
    int AOPass::ShaderPasseIDs::AOPassID = -1;
    int AOPass::ShaderPasseIDs::BlurHorizionPassID = -1;
    int AOPass::ShaderPasseIDs::BlurVerticalPassID = -1;
    int AOPass::ShaderPasseIDs::HIZPassID = -1;
    int AOPass::ShaderPasseIDs::CopyTexturePassID = -1;

    size_t AOPass::RenderTextureIDs::AORTID = SIZE_MAX;
    size_t AOPass::RenderTextureIDs::BlurHorizionRTID = SIZE_MAX;
    size_t AOPass::RenderTextureIDs::BlurVerticalRTID = SIZE_MAX;
    size_t AOPass::RenderTextureIDs::HIZRTID = SIZE_MAX;

    size_t AOPass::ShaderIDs::g_TargetSize = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_SourceSize = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_TargetTexIndex = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_SourceTexIndex = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_TargetMipmapLevel = SIZE_MAX;
    size_t AOPass::ShaderIDs::viewMatrix = SIZE_MAX;
    size_t AOPass::ShaderIDs::viewMatrix_I = SIZE_MAX;
    size_t AOPass::ShaderIDs::projMatrix = SIZE_MAX;
    size_t AOPass::ShaderIDs::projMatrix_I = SIZE_MAX;
    size_t AOPass::ShaderIDs::viewProjMatrix = SIZE_MAX;
    size_t AOPass::ShaderIDs::viewProjMatrix_I = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AOSampleKernelArray = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AOSampleCount = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AORadius = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AOBias = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AOIntensityMul = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AOIntensityPow = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_AOIndex = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_noiseScale = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_BlurDir = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_Sharpness = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_BlurRadius = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_Weights = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_BlurIntensity = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_HIZMaxMipmap = SIZE_MAX;
    size_t AOPass::ShaderIDs::g_HIZTextureIndex = SIZE_MAX;

    AOPass::AOPass() :
        BasePass()
    {
        RenderTextureIDs::AORTID = PropertyToID(L"AO RT");
        RenderTextureIDs::BlurHorizionRTID = PropertyToID(L"AO Blur Horizion RT");
        RenderTextureIDs::BlurVerticalRTID = PropertyToID(L"AO Blur Vertical RT");
        RenderTextureIDs::HIZRTID = PropertyToID(L"AO HIZ RT");

        ShaderIDs::g_TargetSize = PropertyToID(L"g_TargetSize");
        ShaderIDs::g_SourceSize = PropertyToID(L"g_SourceSize");
        ShaderIDs::g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
        ShaderIDs::g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
        ShaderIDs::g_TargetMipmapLevel = PropertyToID(L"g_TargetMipmapLevel");
        ShaderIDs::viewMatrix = PropertyToID(L"viewMatrix");
        ShaderIDs::viewMatrix_I = PropertyToID(L"viewMatrix_I");
        ShaderIDs::projMatrix = PropertyToID(L"projMatrix");
        ShaderIDs::projMatrix_I = PropertyToID(L"projMatrix_I");
        ShaderIDs::viewProjMatrix = PropertyToID(L"viewProjMatrix");
        ShaderIDs::viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");

        ShaderIDs::g_AOSampleKernelArray = PropertyToID(L"g_AOSampleKernelArray");
        ShaderIDs::g_AOSampleCount = PropertyToID(L"g_AOSampleCount");
        ShaderIDs::g_AORadius = PropertyToID(L"g_AORadius");
        ShaderIDs::g_AOBias = PropertyToID(L"g_AOBias");
        ShaderIDs::g_AOIntensityMul = PropertyToID(L"g_AOIntensityMul");
        ShaderIDs::g_AOIntensityPow = PropertyToID(L"g_AOIntensityPow");
        ShaderIDs::g_AOIndex = PropertyToID(L"g_AOIndex");
        ShaderIDs::g_noiseScale = PropertyToID(L"g_noiseScale");

        ShaderIDs::g_HIZMaxMipmap = PropertyToID(L"g_HIZMaxMipmap");
        ShaderIDs::g_HIZTextureIndex = PropertyToID(L"g_HIZTextureIndex");

        ShaderIDs::g_BlurDir = PropertyToID(L"g_BlurDir");
        ShaderIDs::g_Sharpness = PropertyToID(L"g_Sharpness");
        ShaderIDs::g_BlurRadius = PropertyToID(L"g_BlurRadius");
        ShaderIDs::g_Weights = PropertyToID(L"g_Weights");
        ShaderIDs::g_BlurIntensity = PropertyToID(L"g_BlurIntensity");
    }
    AOPass::~AOPass()
    {
        Dispose();
    }
    void AOPass::Dispose()
    {

    }

    void AOPass::Configure()
    {
        m_mipmapCount = UINT(std::floor(std::log2(std::max(m_renderSize.x, m_renderSize.y)))) + 1;

        m_pTempRTs.resize(m_mipmapCount);
        for (UINT i = 0; i < m_mipmapCount; i ++)
        {
            auto currWidth = MathHelper::Max(UINT64(1), UINT64(m_renderSize.x) / UINT64(std::pow(2, i)));
            auto currHeight = MathHelper::Max(UINT64(1), UINT64(m_renderSize.y) / UINT64(std::pow(2, i)));
            m_pTempRTs[i] = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                currWidth,
                currHeight,
                DXGI_FORMAT_R32_FLOAT,
                true,
                L"HIZ RT" + std::to_wstring(i));
        }

        m_pHIZRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_renderSize.x,
            m_renderSize.y,
            DXGI_FORMAT_R32_FLOAT,
            true,
            m_mipmapCount,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::HIZRTID));

        m_pAORT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_renderSize.x),
                                                                           static_cast<UINT64>(m_renderSize.y),
                                                                           DXGI_FORMAT_R8G8B8A8_UNORM,
                                                                           true,
                                                                           RenderResource::GetInstance().
                                                                           GetPropertyName(
                                                                               RenderTextureIDs::AORTID));

        m_pBlurHorizionRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::BlurHorizionRTID));

        m_pBlurVerticalRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R8G8B8A8_UNORM,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::BlurVerticalRTID));

        if (!m_blueNoise.IsValid())
        {
            m_blueNoise = TextureManager::GetInstance().LoadResidentTexture(
                L"Tex\\blue_noise.dds");
        }

        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Copy Texture Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_CopyTexture.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"CopyTexture",
            },
            ShaderPass
            {
                .Name = "HIZ Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_TwoTwoMinHIZ.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"TwoTwoMinHIZ",
            },
            ShaderPass
            {
                .Name = "AO Pass",
                .FilePath = L"Shaders\\public\\CS_SSAO.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"SSAO",
            },
            ShaderPass
            {
                .Name = "Bilateral Blur Horizion Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_Bilateral_Blur.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"HorizionBilateralBlur",
            },
            ShaderPass
            {
                .Name = "Bilateral Blur Vertical Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_Bilateral_Blur.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"VerticalBilateralBlur",
            },
        };
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        ShaderPasseIDs::HIZPassID = m_pMaterial->FindPassIndex("HIZ Pass");
        ShaderPasseIDs::CopyTexturePassID = m_pMaterial->FindPassIndex("Copy Texture Pass");
        ShaderPasseIDs::AOPassID = m_pMaterial->FindPassIndex("AO Pass");
        ShaderPasseIDs::BlurHorizionPassID = m_pMaterial->FindPassIndex("Bilateral Blur Horizion Pass");
        ShaderPasseIDs::BlurVerticalPassID = m_pMaterial->FindPassIndex("Bilateral Blur Vertical Pass");

        UpdatePipeline();

        if (m_kernels.empty())
            m_kernels = GenerateSSAOSampleKernel();

        m_lastBlurQuality = UserData::GetInstance().aoParameter.BlurQuality;
        if (m_blurWeights.empty())
        {
            switch (UserData::GetInstance().aoParameter.BlurQuality)
            {
            case AOBlurQuality::High:
            {
                m_blurRadius = 9;
                m_blurSigma = 3;
                break;
            }
            case AOBlurQuality::Middle:
            {
                m_blurRadius = 6;
                m_blurSigma = 2.2;
                break;
            }
            case AOBlurQuality::Low:
            {
                m_blurRadius = 4;
                m_blurSigma = 1.5;
                break;
            }
            }
            m_blurWeights = GenerateBlurWeights(m_blurRadius, m_blurSigma);

        }
    }

    void AOPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "SSAO Pass");
        m_pCamera = context.pCamera;

        if (UserData::GetInstance().aoParameter.BlurQuality != m_lastBlurQuality)
        {
            switch (UserData::GetInstance().aoParameter.BlurQuality)
            {
            case AOBlurQuality::High:
            {
                m_blurRadius = 9;
                m_blurSigma = 3;
                break;
            }
            case AOBlurQuality::Middle:
            {
                m_blurRadius = 6;
                m_blurSigma = 2.2;
                break;
            }
            case AOBlurQuality::Low:
            {
                m_blurRadius = 4;
                m_blurSigma = 1.5;
                break;
            }
            }

            m_blurWeights = GenerateBlurWeights(m_blurRadius, m_blurSigma);
        }
        m_lastBlurQuality = UserData::GetInstance().aoParameter.BlurQuality;

        DoHIZ();
        DoSSAO();
        DoBlurHorizion();
        DoBlurVertical();

        m_pCommand->CopyTexture(m_pBlurVerticalRT, m_pAORT);
    }

    void AOPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::CopyTexturePassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::HIZPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::AOPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::BlurHorizionPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::BlurVerticalPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }

    }

    void AOPass::DoHIZ()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "HIZ");

        UINT64 currWidth = m_pHIZRT->GetWidth();
        UINT64 currHeight = m_pHIZRT->GetHeight();

        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommand->GetCommandList()->DiscardResource(m_pHIZRT->GetResource(), nullptr);

        for (UINT i = 0; i < m_mipmapCount; ++i)
        {
            m_pCommand->AddBarrier(m_pTempRTs[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
            m_pCommand->GetCommandList()->DiscardResource(m_pTempRTs[i]->GetResource(), nullptr);
            m_pCommand->AddBarrier(m_pTempRTs[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            if (i == 0)
            {
                auto passID = ShaderPasseIDs::CopyTexturePassID;
                auto& passData = m_pMaterial->GetPassData(passID);

                PipelineInfo pipelineStateData{};
                pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
                m_pCommand->SetPipeline(pipelineStateData);

                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pTempRTs[i]->GetResourceHeapIndex(), passID);

                SetSpaceResource(passData, PER_PASS_SPACE);
                SetSpaceResource(passData, PER_FRAME_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                     CeilDivide(currHeight, threadGroupSize.y), threadGroupSize.z);
            }
            else
            {
                m_pCommand->AddBarrier(m_pTempRTs[i - 1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                auto lastWidth = currWidth;
                auto lastHeight = currHeight;
                currWidth = MathHelper::Max(UINT64(1), currWidth / 2);
                currHeight = MathHelper::Max(UINT64(1), currHeight / 2);

                auto passID = ShaderPasseIDs::HIZPassID;
                auto& passData = m_pMaterial->GetPassData(passID);

                PipelineInfo pipelineStateData{};
                pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
                m_pCommand->SetPipeline(pipelineStateData);

                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pTempRTs[i]->GetResourceHeapIndex(), passID);
                m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, m_pTempRTs[i - 1]->GetResourceHeapIndex(), passID);
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize, GetScreenSize(currWidth, currHeight), passID);
                m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize, GetScreenSize(lastWidth, lastHeight), passID);

                SetSpaceResource(passData, PER_PASS_SPACE);
                SetSpaceResource(passData, PER_FRAME_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                     CeilDivide(currHeight, threadGroupSize.y), threadGroupSize.z);
            }

            m_pCommand->AddBarrier(m_pTempRTs[i], D3D12_RESOURCE_STATE_COPY_SOURCE, false);
            m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_COPY_DEST, false);
            m_pCommand->FlushBarrier();

            m_pCommand->CopyTextureRegion(m_pTempRTs[i], m_pHIZRT, i);
        }
    }
    void AOPass::DoSSAO()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "SSAO");
        auto passID = ShaderPasseIDs::AOPassID;

        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->ClearRenderTarget(m_pAORT, Color::Black);

        {
            auto& passData = m_pMaterial->GetPassData(passID);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                     passID)
                                                                 .pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_pAORT->GetWidth(), m_pAORT->GetHeight()), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_AOSampleCount, UserData::GetInstance().aoParameter.SampleCount, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_AORadius, UserData::GetInstance().aoParameter.Radius, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_AOBias, UserData::GetInstance().aoParameter.Bias, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityMul, UserData::GetInstance().aoParameter.IntensityMul,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityPow, UserData::GetInstance().aoParameter.IntensityPow,
                                  passID);
            m_pMaterial->SetVector4Array(ShaderIDs::g_AOSampleKernelArray, m_kernels, passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale, Vector2(
                                       m_renderSize.x / m_blueNoise.GetWidth(),
                                       m_renderSize.y / m_blueNoise.GetHeight()), passID);

            m_pMaterial->SetUInt(ShaderIDs::g_HIZMaxMipmap, MathHelper::Max(m_mipmapCount - 1, UINT(0)), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZTextureIndex, m_pHIZRT->GetResourceHeapIndex(), passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pAORT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pAORT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    void AOPass::DoBlurHorizion()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Blur Horizion");

        m_pCommand->AddBarrier(m_pBlurHorizionRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->ClearRenderTarget(m_pBlurHorizionRT, Color::Black);

        {
            auto passID = ShaderPasseIDs::BlurHorizionPassID;
            auto& passData = m_pMaterial->GetPassData(passID);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);

            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pBlurHorizionRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, m_pAORT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_BlurRadius, m_blurRadius, passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_BlurDir, Vector2(1.f, 0.f), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_pBlurHorizionRT->GetWidth(),
                                                 m_pBlurHorizionRT->GetHeight()), passID);
            m_pMaterial->SetFloat(ShaderIDs::g_Sharpness, UserData::GetInstance().aoParameter.Sharpness, passID);
            m_pMaterial->SetFloatArray(ShaderIDs::g_Weights, m_blurWeights, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pBlurHorizionRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pBlurHorizionRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pBlurHorizionRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    void AOPass::DoBlurVertical()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Blur Vertical");

        m_pCommand->AddBarrier(m_pBlurVerticalRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->ClearRenderTarget(m_pBlurVerticalRT, Color::Black);

        {
            auto passID = ShaderPasseIDs::BlurVerticalPassID;
            auto& passData = m_pMaterial->GetPassData(passID);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);

            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pBlurVerticalRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, m_pBlurHorizionRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_BlurRadius, m_blurRadius, passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_BlurDir, Vector2(0.f, 1.f), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_pBlurVerticalRT->GetWidth(),
                                                 m_pBlurVerticalRT->GetHeight()), passID);
            m_pMaterial->SetFloat(ShaderIDs::g_Sharpness, UserData::GetInstance().aoParameter.Sharpness, passID);
            m_pMaterial->SetFloatArray(ShaderIDs::g_Weights, m_blurWeights, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pBlurVerticalRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pBlurVerticalRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pBlurVerticalRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    std::vector<Vector4> AOPass::GenerateSSAOSampleKernel()
    {
        std::vector<Vector4> o{};
        UINT maxSampleCount = 16;
        maxSampleCount = MathHelper::Min(UserData::GetInstance().aoParameter.SampleCount, maxSampleCount);
        o.resize(maxSampleCount);

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
    std::vector<float> AOPass::GenerateBlurWeights(UINT blurRadius, float sigma)
    {
        std::vector<float> weights(blurRadius + 1, 0);
        float sum = 0.0f;

        const float denominator = 2.0f * sigma * sigma;

        // 计算权重
        for (int i = 0; i <= blurRadius; ++i)
        {
            weights[i] = expf(-(i * i) / denominator);
            sum += i == 0 ? weights[i] : 2.0f * weights[i];
        }

        // 归一化，确保权重总和为 1
        for (int i = 0; i <= blurRadius; ++i)
        {
            weights[i] /= sum;
        }
        return weights;
    }

}