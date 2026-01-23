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
    AOPass::AOPass() :
        BasePass()
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

        m_pHistoryRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            static_cast<UINT64>(m_renderSize.x),
            static_cast<UINT64>(m_renderSize.y),
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().
            GetPropertyName(
                RenderTextureIDs::TAARTID));

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
                .Name = "AO Pass",
                .FilePath = L"Shaders\\public\\CS_SSAO.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"SSAO",
            },
            ShaderPass
            {
                .Name = "TAA Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_AOTAA.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"TAA",
            },
        };
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        ShaderPasseIDs::HIZPassID = m_pMaterial->FindPassIndex("HIZ Pass");
        ShaderPasseIDs::AOPassID = m_pMaterial->FindPassIndex("AO Pass");
        ShaderPasseIDs::TAAPassID = m_pMaterial->FindPassIndex("TAA Pass");

        UpdatePipeline();

        if (m_kernels.empty())
            m_kernels = GenerateHBAOSampleKernel();
    }

    void AOPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "AO Pass");
        m_pCamera = context.pCamera;

        IsDebugLayerEnabled(m_pDevice->GetDevice());

        DoHIZ();
        DoSSAO();
        DoTAA();

        m_isFirstFrame = false;
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

            auto passID = ShaderPasseIDs::TAAPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(m_pDevice, m_pMaterial.get(), passID);
        }
    }

    void AOPass::DoHIZ()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "HIZ");

        UINT64 currWidth = m_pHIZRT->GetWidth();
        UINT64 currHeight = m_pHIZRT->GetHeight();

        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        auto passID = ShaderPasseIDs::HIZPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        for (UINT i = 0; i < m_HIZMipmapCount; ++i)
        {
            if (i == 0)
            {
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pHIZRT->GetUAVResourceHeapIndex(i), passID);
                m_pMaterial->SetFloat(ShaderIDs::g_MipmapLevel, i, passID);
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize, GetScreenSize(currWidth, currHeight), passID);

                SetSpaceResource(passData, PER_PASS_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                     CeilDivide(currHeight, threadGroupSize.y), threadGroupSize.z);
            }
            else
            {
                m_pCommand->AddUAVBarrier(m_pHIZRT);
                auto lastWidth = currWidth;
                auto lastHeight = currHeight;
                currWidth = MathHelper::Max(UINT64(1), currWidth >> 1);
                currHeight = MathHelper::Max(UINT64(1), currHeight >> 1);

                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pHIZRT->GetUAVResourceHeapIndex(i), passID);
                m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, m_pHIZRT->GetUAVResourceHeapIndex(i - 1),
                                     passID);
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize, GetScreenSize(currWidth, currHeight), passID);
                m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize, GetScreenSize(lastWidth, lastHeight), passID);
                m_pMaterial->SetFloat(ShaderIDs::g_MipmapLevel, i, passID);

                SetSpaceResource(passData, PER_PASS_SPACE);

                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                     CeilDivide(currHeight, threadGroupSize.y), threadGroupSize.z);
            }
        }

        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    void AOPass::DoSSAO()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Calc AO Pass");
        auto passID = ShaderPasseIDs::AOPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;

        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert(), passID);
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                               passID);
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                               (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(), passID);

        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleCount, UserData::GetInstance().aoParameter.SampleCount, passID);
        m_pMaterial->SetUInt(ShaderIDs::g_AOSampleStepCount, UserData::GetInstance().aoParameter.SampleStepCount,
                             passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AORadius, UserData::GetInstance().aoParameter.Radius, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOFadeRadius, UserData::GetInstance().aoParameter.FadeRadius, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOFadeDistance, UserData::GetInstance().aoParameter.FadeDistance, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOBias, UserData::GetInstance().aoParameter.Bias, passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityMul, UserData::GetInstance().aoParameter.IntensityMul,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AOIntensityPow, UserData::GetInstance().aoParameter.IntensityPow,
                              passID);
        m_pMaterial->SetVector4Array(ShaderIDs::g_AOSampleKernelArray, m_kernels, passID);
        m_pMaterial->SetUInt(ShaderIDs::g_HIZMaxMipmap, MathHelper::Max(m_HIZMipmapCount - 1, UINT(0)), passID);
        m_pMaterial->SetUInt(ShaderIDs::g_HIZTextureIndex, m_pHIZRT->GetResourceHeapIndex(), passID);
        m_pMaterial->SetFloat(ShaderIDs::g_LerpAOFactor, UserData::GetInstance().aoParameter.AOLerpFactor, passID);

        {
            PIXHelper pix(m_pCommand->GetCommandList(), "One Four SSAO");
            m_pCommand->SetPipeline(pipelineStateData);
            auto targetRT = m_pOneFourAORT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_pCommand->ClearRenderTarget(targetRT, Color::Black);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale, Vector2(
                                       float(targetRT->GetWidth()) / float(m_blueNoise.GetWidth()),
                                       float(targetRT->GetHeight()) / float(m_blueNoise.GetHeight())), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMinMipmap, 2, passID);
            m_pMaterial->SetBool(ShaderIDs::g_bLerpAO, false, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        {
            PIXHelper pix(m_pCommand->GetCommandList(), "Half SSAO");
            m_pCommand->SetPipeline(pipelineStateData);
            auto targetRT = m_pHalfAORT;
            auto sourceRT = m_pOneFourAORT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_pCommand->ClearRenderTarget(targetRT, Color::Black);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(sourceRT->GetWidth(), sourceRT->GetHeight()), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, sourceRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale, Vector2(
                                       float(targetRT->GetWidth()) / float(m_blueNoise.GetWidth()),
                                       float(targetRT->GetHeight()) / float(m_blueNoise.GetHeight())), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMinMipmap, 1, passID);
            m_pMaterial->SetBool(ShaderIDs::g_bLerpAO, UserData::GetInstance().aoParameter.IsLerpAO, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        }

        {
            PIXHelper pix(m_pCommand->GetCommandList(), "Full SSAO");
            m_pCommand->SetPipeline(pipelineStateData);
            auto targetRT = m_pAORT;
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_pCommand->ClearRenderTarget(targetRT, Color::Black);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
            m_pMaterial->SetFloat2(ShaderIDs::g_noiseScale, Vector2(
                                       float(targetRT->GetWidth()) / float(m_blueNoise.GetWidth()),
                                       float(targetRT->GetHeight()) / float(m_blueNoise.GetHeight())), passID);
            m_pMaterial->SetUInt(ShaderIDs::g_HIZMinMipmap, 0, passID);
            m_pMaterial->SetBool(ShaderIDs::g_bLerpAO, UserData::GetInstance().aoParameter.IsLerpAO, passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

    }
    void AOPass::DoTAA()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "TAA Pass");
        auto passID = ShaderPasseIDs::TAAPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                                                                 passID)
                                                             .pPipelineStateObject;

        auto targetRT = m_pAORT;
        auto preTargetRT = m_pHistoryRT;

        if (m_isFirstFrame)
        {
            m_pCommand->CopyTexture(targetRT, preTargetRT);

            TAAData::Pre_View_M = m_pCamera->GetViewMat();
            TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
            TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
            TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
            TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
            TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert();
        }
        else
        {
            m_pCommand->SetPipeline(pipelineStateData);

            m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            {
                m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
                m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert(), passID);
                m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
                m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                       passID);
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                       (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(), passID);

                m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix, TAAData::Pre_View_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix_I, TAAData::Pre_View_I_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix, TAAData::Pre_Proj_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix_I, TAAData::Pre_Proj_I_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix, TAAData::Pre_ViewProj_M, passID);
                m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix_I, TAAData::Pre_ViewProj_I_M, passID);

                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()), passID);
                m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, targetRT->GetResourceHeapIndex(), passID);
                m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, preTargetRT->GetResourceHeapIndex(), passID);
                m_pMaterial->SetFloat(ShaderIDs::g_BlendWeight, UserData::GetInstance().aoParameter.TAALerpFactor,
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
                m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                     CeilDivide(targetRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
                m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                m_pCommand->CopyTexture(targetRT, preTargetRT);
            }
        }

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