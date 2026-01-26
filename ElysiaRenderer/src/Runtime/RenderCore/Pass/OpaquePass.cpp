#include "stdafx.h"
#include "OpaquePass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12TextureBuffer.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/DX12Camera.h"

#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    int OpaquePass::ShaderPassIDs::OpaqueLightPassID = -1;

    size_t OpaquePass::ShaderIDs::g_AOIndex = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::screenSize = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::viewMatrix = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::viewMatrix_I = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::projMatrix = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::projMatrix_I = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::viewProjMatrix = SIZE_MAX;
    size_t OpaquePass::ShaderIDs::viewProjMatrix_I = SIZE_MAX;

    OpaquePass::OpaquePass()
        : BasePass()
    {
        ShaderIDs::g_AOIndex = PropertyToID(L"g_AOIndex");

        ShaderIDs::screenSize = PropertyToID(L"screenSize");
        ShaderIDs::viewMatrix = PropertyToID(L"viewMatrix");
        ShaderIDs::viewMatrix_I = PropertyToID(L"viewMatrix_I");
        ShaderIDs::projMatrix = PropertyToID(L"projMatrix");
        ShaderIDs::projMatrix_I = PropertyToID(L"projMatrix_I");
        ShaderIDs::viewProjMatrix = PropertyToID(L"viewProjMatrix");
        ShaderIDs::viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
    }

    OpaquePass::~OpaquePass()
    {
        Dispose();
    }

    void OpaquePass::Dispose()
    {

    }

    void OpaquePass::Configure()
    {
        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Opaque Light Pass",
                .FilePath = L"Shaders\\public\\Opaque.hlsl",
            }
        };
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        ShaderPassIDs::OpaqueLightPassID = m_pMaterial->FindPassIndex("Opaque Light Pass");

        UpdatePipeline();
    }

    void OpaquePass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        PIXHelper pix(m_pCommand->GetCommandList(), "Opaque Light Pass");

        DrawLightingPass(context);
    }

    void OpaquePass::DrawLightingPass(ElysiaEngine::FrameContext& context)
    {
        auto passID = ShaderPassIDs::OpaqueLightPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData
        {
            .m_pipelineStateObject = passData.pPipelineStateObject,
            .m_renderTargets = {m_pCameraColorRT->GetTexture()},
            .m_depthStencilTarget = m_pCameraDepthRT->GetTexture()
        };
        m_pCommand->SetPipeline(pipelineStateData);

        m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                               GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                               m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                               (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());

        SetSpaceResource(passData, PER_PASS_SPACE);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pCommand->ClearRenderTarget(m_pCameraColorRT, Color::Black);

        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pCommand->DrawFullScreenTriangle();

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    void OpaquePass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        UpdateLightingPassVariant(ShaderPassIDs::OpaqueLightPassID);
    }
    void OpaquePass::UpdateLightingPassVariant(UINT passIndex)
    {
        std::vector<std::wstring> enableKeywords{};

        switch (UserData::GetInstance().shadowQuality)
        {
        case ShadowQuality::Low:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_LOW");
            break;
        }
        case ShadowQuality::Middle:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_MIDDLE");
            break;
        }
        case ShadowQuality::High:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_HIGH");
            break;
        }
        case ShadowQuality::VeryHigh:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_VERYHIGH");
            break;
        }
        }
        switch (UserData::GetInstance().shadowType)
        {
        case ShadowType::Hard:
        {
            enableKeywords.emplace_back(L"HARD_SHADOW");
            break;
        }
        case ShadowType::Soft:
        {
            enableKeywords.emplace_back(L"SOFT_SHADOW");
            break;
        }
        }

        auto& passData = m_pMaterial->GetPassData(passIndex);
        auto VariantManager = passData.pShader->GetVariantManager();
        passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

        RenderTargetDesc RTDesc = RenderTargetDesc
        {
            .m_renderTargetFormats = m_pCameraColorRT->GetFormat(),
            .m_numRenderTargets = 1,
            .m_depthStencilFormat = m_pCameraDepthRT->GetFormat()
        };
        passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
            m_pDevice,
            m_pMaterial.get(),
            passIndex,
            RTDesc);
    }
}