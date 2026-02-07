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
    DebugPass::DebugPass()
        : BasePass()
    {
        BufferCreationDesc vertexBufferDesc =
        {
            .name = L"Debug Vertex Buffer",
            .stride = sizeof(Vector3),
            .size = sizeof(Vector3) * NumVertices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_vertices
        };
        BufferCreationDesc indexBufferDesc =
        {
            .name = L"Debug Index Buffer",
            .stride = 0,
            .size = sizeof(INDEX_FORMAT) * NumIndices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_indices
        };
        m_vertexBuffer = BufferManager::GetInstance().CreateVertexBuffer(vertexBufferDesc);
        m_indexBuffer = BufferManager::GetInstance().CreateIndexBuffer(indexBufferDesc);

        m_vertexView = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation = m_vertexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(NumVertices) * m_vertexBuffer->GetStride(),
            .StrideInBytes = m_vertexBuffer->GetStride()
        };
        m_indexView =
        {
            .BufferLocation = m_indexBuffer->GetGPUAddress(),
            .SizeInBytes = NumIndices * ElysiaModel::IndexSize(),
            .Format = ElysiaModel::IndexBufferFormat(),
        };
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

        switch (UserData::GetInstance().debugMode)
        {
        case DebugMode::None:
        {
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
            break;
        }

        case DebugMode::AO:
        case DebugMode::Normal:
        {
            m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                SetSpaceResource(passData, PER_PASS_SPACE);
                m_pCommand->DrawFullScreenTriangle();
            }
            m_pCommand->AddBarrier(m_pCameraColorRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            return;
            break;
        }

        }
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