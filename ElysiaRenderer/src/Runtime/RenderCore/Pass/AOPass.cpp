#include "stdafx.h"
#include "AOPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
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

    size_t AOPass::RenderTextureIDs::AORTID = SIZE_MAX;

    size_t AOPass::ShaderIDs::g_DestSize = SIZE_MAX;
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

    AOPass::AOPass() :
        BasePass()
    {
        RenderTextureIDs::AORTID = PropertyToID(L"AO RT");

        ShaderIDs::g_DestSize = PropertyToID(L"g_DestSize");
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
        m_pAORT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_renderSize.x),
                                                                           static_cast<UINT64>(m_renderSize.y),
                                                                           DXGI_FORMAT_R32_FLOAT,
                                                                           true,
                                                                           RenderResource::GetInstance().
                                                                           GetPropertyName(
                                                                               RenderTextureIDs::AORTID));

        if (!m_blueNoise.IsValid())
        {
            m_blueNoise = TextureManager::GetInstance().LoadResidentTexture(
                L"Tex\\blue_noise.dds");
        }

        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "AO Pass",
                .FilePath = L"Shaders\\public\\CS_SSAO.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"SSAO",
            }
        };
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        ShaderPasseIDs::AOPassID = m_pMaterial->FindPassIndex("AO Pass");

        UpdatePipeline();

        if (m_kernels.size() <= 0)
            m_kernels = GenerateSSAOSampleKernel();
    }

    void AOPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "AO Pass");
        m_pCamera = context.pCamera;

        m_pMaterial->SetFloat4(ShaderIDs::g_DestSize,
                               GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                               (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());

        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleCount, UserData::GetInstance().aoParameter.SampleCount);
        m_pMaterial->SetFloat(ShaderIDs::g_AORadius, UserData::GetInstance().aoParameter.Radius);
        m_pMaterial->SetFloat(ShaderIDs::g_AOBias, UserData::GetInstance().aoParameter.Bias);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityMul, UserData::GetInstance().aoParameter.IntensityMul);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityPow, UserData::GetInstance().aoParameter.IntensityPow);

        m_pMaterial->SetVector4Array(ShaderIDs::g_AOSampleKernelArray, m_kernels);

        auto blueNoiseTex = TextureManager::GetInstance().GetTexture(m_blueNoise);
        m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale, Vector2(
                                   m_renderSize.x / blueNoiseTex->GetResourceDesc().Width,
                                   m_renderSize.y / blueNoiseTex->GetResourceDesc().Height));

        DoCalcAO();
    }

    void AOPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        {
            std::vector<std::wstring> enableKeywords{};

            auto& passData = m_pMaterial->GetPassData(ShaderPasseIDs::AOPassID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), ShaderPasseIDs::AOPassID);
        }
    }

    void AOPass::DoCalcAO()
    {
        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->ClearRenderTarget(m_pAORT, Color::Black);

        {
            auto& passData = m_pMaterial->GetPassData(ShaderPasseIDs::AOPassID);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                     ShaderPasseIDs::AOPassID)
                                                                 .pPipelineStateObject;

            m_pCommand->SetPipeline(pipelineStateData);
            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pCameraColorRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pCameraColorRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pAORT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    std::vector<Vector4> AOPass::GenerateSSAOSampleKernel()
    {
        std::vector<Vector4> o{};
        UINT maxSampleCount = 32;
        maxSampleCount = MathHelper::Min(UserData::GetInstance().aoParameter.SampleCount, maxSampleCount);
        o.reserve(maxSampleCount);

        std::random_device rd;
        std::default_random_engine generator(rd());
        std::uniform_real_distribution<float> minusOneToOneDistribution(-1.f, 1.f);
        std::uniform_real_distribution<float> zeroToOneDistribution(0.f, 1.f);

        for (UINT i = 0; i < maxSampleCount; i ++)
        {
            auto randomVec = Vector4(minusOneToOneDistribution(generator),
                                     minusOneToOneDistribution(generator),
                                     zeroToOneDistribution(generator), 1.f);

            auto scale = (float)i / maxSampleCount;
            scale = MathHelper::Lerp(0.1f, 1.f, scale * scale); // 二次函数分布
            randomVec = randomVec * scale;

            o.emplace_back(randomVec);
        }

        return o;
    }
}