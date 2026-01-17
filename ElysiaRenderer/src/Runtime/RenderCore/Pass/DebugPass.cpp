#include "stdafx.h"
#include "DebugPass.h"

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
    int DebugPass::ShaderPasseIDs::DebugPassID = -1;

    size_t DebugPass::ShaderIDs::g_DebugMode = SIZE_MAX;
    size_t DebugPass::ShaderIDs::g_TargetTexIndex = SIZE_MAX;
    size_t DebugPass::ShaderIDs::g_SourceTexIndex = SIZE_MAX;
    size_t DebugPass::ShaderIDs::g_MipmapLevel = SIZE_MAX;
    size_t DebugPass::ShaderIDs::g_SourceSize = SIZE_MAX;

    DebugPass::DebugPass() :
        BasePass()
    {
        ShaderIDs::g_DebugMode = PropertyToID(L"g_DebugMode");
        ShaderIDs::g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
        ShaderIDs::g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
        ShaderIDs::g_MipmapLevel = PropertyToID(L"g_MipmapLevel");
        ShaderIDs::g_SourceSize = PropertyToID(L"g_SourceSize");
    }
    DebugPass::~DebugPass()
    {
        Dispose();
    }
    void DebugPass::Dispose()
    {

    }

    void DebugPass::Configure()
    {
        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Debug Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\CS_Debug.hlsl",
                .IsComputeShader = true,
                .ComputeEntryPoint = L"Debug",
            },
        };

        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        ShaderPasseIDs::DebugPassID = m_pMaterial->FindPassIndex("Debug Pass");

        UpdatePipeline();
    }

    void DebugPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Debug Pass");
        m_pCamera = context.pCamera;

        DoDebugPass();
    }

    void DebugPass::DoDebugPass()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Debug Pass");

        m_pMaterial->SetUInt(ShaderIDs::g_DebugMode, static_cast<UINT>(UserData::GetInstance().debugMode));
        m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pCameraColorRT->GetResourceHeapIndex());
        m_pMaterial->SetUInt(ShaderIDs::g_MipmapLevel, UserData::GetInstance().mipmapLevel);

        switch (UserData::GetInstance().debugMode)
        {
        case DebugMode::None:
        {
            return;
        }
        case DebugMode::AO:
        {
            auto RT = RenderTargetManager::GetInstance().GetRenderTexture(
                L"AO RT");
            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, RT->GetResourceHeapIndex());
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize, GetScreenSize(RT->GetWidth(), RT->GetHeight()));
            break;
        }
        }

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto passID = ShaderPasseIDs::DebugPassID;

        {
            auto& passData = m_pMaterial->GetPassData(passID);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(passID)
                                                                 .pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);

            SetSpaceResource(passData, PER_PASS_SPACE);
            SetSpaceResource(passData, PER_FRAME_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pCameraColorRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pCameraColorRT->GetHeight(), threadGroupSize.y), threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    void DebugPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = ShaderPasseIDs::DebugPassID;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice, m_pMaterial.get(), passID);
        }
    }
}