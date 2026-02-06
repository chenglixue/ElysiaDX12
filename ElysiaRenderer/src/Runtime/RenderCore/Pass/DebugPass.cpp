#include "stdafx.h"
#include "DebugPass.h"

#include "GIPass.h"
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
    size_t DebugPass::ShaderIDs::g_TargetSize = SIZE_MAX;

    DebugPass::DebugPass()
        : BasePass()
    {
        ShaderIDs::g_DebugMode = PropertyToID(L"g_DebugMode");
        ShaderIDs::g_TargetTexIndex = PropertyToID(L"g_TargetTexIndex");
        ShaderIDs::g_SourceTexIndex = PropertyToID(L"g_SourceTexIndex");
        ShaderIDs::g_MipmapLevel = PropertyToID(L"g_MipmapLevel");
        ShaderIDs::g_SourceSize = PropertyToID(L"g_SourceSize");
        ShaderIDs::g_TargetSize = PropertyToID(L"g_TargetSize");
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
            // ShaderPass
            // {
            //     .Name = "Debug Pass",
            //     .FilePath = L"Shaders\\public\\PostProcess\\CS_Debug.hlsl",
            //     .IsComputeShader = true,
            //     .ComputeEntryPoint = L"Debug",
            // },
            ShaderPass
            {
                .Name = "Debug Pass",
                .FilePath = L"Shaders\\public\\PostProcess\\Debug.hlsl",
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
        auto passID = ShaderPasseIDs::DebugPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), "Debug Pass");

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        pipelineStateData.m_renderTargets = {m_pCameraColorRT->GetTexture()};
        pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pMaterial->SetUInt(ShaderIDs::g_DebugMode,
                             static_cast<UINT>(UserData::GetInstance().debugMode));
        m_pMaterial->SetUInt(ShaderIDs::g_TargetTexIndex, m_pCameraColorRT->GetResourceHeapIndex());
        m_pMaterial->SetUInt(ShaderIDs::g_MipmapLevel, UserData::GetInstance().mipmapLevel);
        m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                               GetScreenSize(m_pCameraColorRT->GetWidth(),
                                             m_pCameraColorRT->GetHeight()));

        switch (UserData::GetInstance().debugMode)
        {
        case DebugMode::None:
        {
            return;
        }
        case DebugMode::AO:
        {
            auto clampValue = std::ranges::clamp(UserData::GetInstance().mipmapLevel, 0, 3);
            auto RT = RenderTargetManager::GetInstance().GetRenderTexture(
                L"AO RT");
            if (UserData::GetInstance().aoParameter.debugTarget == AODebugTarget::Importance)
            {
                RT = RenderTargetManager::GetInstance().GetRenderTexture(
                    L"AO Importance RT");

            }
            // RT = RenderTargetManager::GetInstance().GetRenderTexture(
            //     L"Deinterleaved AO RT" + std::to_wstring(clampValue));

            m_pMaterial->SetUInt(ShaderIDs::g_SourceTexIndex, RT->GetResourceHeapIndex());
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(RT->GetWidth(), RT->GetHeight()));
            break;
        }
        case DebugMode::GI:
        {
            if (!GIPass::m_vertexBuffer->GetIsReady() || !GIPass::m_indexBuffer->GetIsReady())
                return;

            m_pCommand->SetVertexBuffer(0, 1, GIPass::m_vertexView);
            m_pCommand->SetIndexBuffer(GIPass::m_indexView);

            m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                m_pMaterial->SetUInt(GIPass::ShaderIDs::g_IrradianceTexIndex,
                                     RenderTargetManager::GetInstance().GetRenderTexture(
                                                                           GIPass::RenderTextureIDs::IrradianceRTID)
                                                                       ->GetResourceHeapIndex());
                m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                                       GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
                m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
                m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
                m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
                m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                       m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                       (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).
                                       Invert());

                m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridDimensions,
                                       Vector3(GIPass::Grid_Dimensions.x,
                                               GIPass::Grid_Dimensions.y,
                                               GIPass::Grid_Dimensions.z));
                m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridOrigin, GIPass::m_gridOrigin);
                m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridSpacing, GIPass::m_gridSpacing);
                m_pMaterial->SetFloat(GIPass::ShaderIDs::g_ProbeRadius, 0.5f);
                SetSpaceResource(passData, PER_PASS_SPACE);

                m_pCommand->DrawInstanced(GIPass::NumIndices, GIPass::Probe_Count, 0, 0, 0);
            }
            m_pCommand->AddBarrier(m_pCameraColorRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            return;
            break;
        }
        }

        // m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        //
        // {
        //
        //     SetSpaceResource(passData, PER_PASS_SPACE);
        //
        //     auto threadGroupSize = passData.GetKernelThreadGroupSizes();
        //     m_pCommand->Dispatch(CeilDivide(m_pCameraColorRT->GetWidth(), threadGroupSize.x),
        //                          CeilDivide(m_pCameraColorRT->GetHeight(), threadGroupSize.y),
        //                          threadGroupSize.z);
        // }
        //
        // m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

            // passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
            //     m_pDevice,
            //     m_pMaterial.get(),
            //     passID);

            const RenderTargetDesc desc =
            {
                .m_renderTargetFormats = {m_pCameraColorRT->GetFormat()},
                .m_numRenderTargets = 1,
                .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
            };
            passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID,
                desc);
        }
    }
}