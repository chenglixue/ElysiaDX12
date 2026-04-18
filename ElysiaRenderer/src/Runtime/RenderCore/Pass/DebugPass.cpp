#include "stdafx.h"
#include "DebugPass.h"

#include "BloomPass.h"
#include "GBufferPass.h"
#include "GIPass.h"
#include "ShadowProjectionPass.h"
#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"
#include "Runtime/Engine/ECS/Entity.h"
#include "Runtime/RenderCore/DX12Camera.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    DebugPass::DebugPass()
        : BasePass(),
          m_aabbDrawer()
    {
        m_aabbDrawer.Init();
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

        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        ShaderPasseIDs::DebugPassID = m_pMaterial->FindPassIndex("Debug Pass");

        UpdatePipeline();
    }

    void DebugPass::Render(FrameContext& context)
    {
        UpdatePipeline();
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
        pipelineStateData.m_renderTargets = {m_pDisplayRT->GetTexture()};
        // pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_displaySize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pMaterial->SetUINT(ShaderIDs::g_DebugMode,
                             static_cast<UINT>(UserData::GetInstance().debugMode));
        m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex, m_pDisplayRT->GetResourceHeapIndex());
        m_pMaterial->SetUINT(ShaderIDs::g_MipmapLevel, UserData::GetInstance().mipmapLevel);
        m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                               GetScreenSize(m_pDisplayRT->GetWidth(),
                                             m_pDisplayRT->GetHeight()));
        m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                               GetScreenSize(Vector2(m_displaySize.x, m_displaySize.y)));
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
        case DebugMode::AABB:
        {
            DoAABBPass();
            break;
        }
        case DebugMode::GIProbe:
        {
            DoGIPass();
            // DoAABBPass();
            break;
        }
        case DebugMode::Bloom:
        {
            int mipmapLevel = UserData::GetInstance().bloomParameter.mipmap;
            switch (UserData::GetInstance().bloomParameter.debugMode)
            {
            case DebugDownOrUp::Up:
            {
                m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex,
                                     RenderTargetManager::GetInstance().GetRenderTexture(
                                                                           RenderResource::GetInstance().
                                                                           GetPropertyName(
                                                                               BloomPass::RenderTextureIDs::BloomUpSampleRTID)
                                                                           + std::to_wstring(mipmapLevel))
                                                                       ->GetUAVResourceHeapIndex(),
                                     passID);
                break;
            }
            case DebugDownOrUp::Down:
            {
                m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex,
                                     RenderTargetManager::GetInstance().GetRenderTexture(
                                                                           RenderResource::GetInstance().
                                                                           GetPropertyName(
                                                                               BloomPass::RenderTextureIDs::BloomDownSampleRTID)
                                                                           + std::to_wstring(mipmapLevel))
                                                                       ->GetUAVResourceHeapIndex(),
                                     passID);
            }
            }
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(
                                       std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate),
                                       std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate)),
                                   passID);

            Vector2 renderSize = Vector2(std::floor(
                                             m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate),
                                         std::floor(
                                             m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate));
            m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(renderSize),
                                       passID);
                SetSpaceResource(passData, PER_PASS_SPACE);
                m_pCommand->DrawFullScreenTriangle();
            }
            m_pCommand->AddBarrier(m_pDisplayRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            break;
        }
        case DebugMode::AO:
        {
            m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex,
                                 RenderTargetManager::GetInstance().GetRenderTexture(
                                                                       RenderResource::GetInstance().GetPropertyName(
                                                                           GBufferPass::RenderTextureIDs::GBufferHIZID))
                                                                   ->GetSRVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_MipmapLevel,
                                 UserData::GetInstance().mipmapLevel,
                                 passID);
            Vector2 renderSize = Vector2(std::floor(
                                             m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate),
                                         std::floor(
                                             m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate));
            m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(renderSize),
                                       passID);
                SetSpaceResource(passData, PER_PASS_SPACE);
                m_pCommand->DrawFullScreenTriangle();
            }
            m_pCommand->AddBarrier(m_pDisplayRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            break;
        }
        case DebugMode::ShadowMask:
        {
            m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex,
                                 ShadowProjectionPass::m_pShadowMaskRTs[ShadowProjectionPass::m_writeIndex]->
                                 GetSRVResourceHeapIndex(),
                                 passID);
            Vector2 renderSize = Vector2(std::floor(
                                             m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate),
                                         std::floor(
                                             m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate));
            m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(m_displaySize),
                                       passID);
                SetSpaceResource(passData, PER_PASS_SPACE);
                m_pCommand->DrawFullScreenTriangle();
            }
            m_pCommand->AddBarrier(m_pDisplayRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            break;
        }
        case DebugMode::Velocity:
        case DebugMode::Albedo:
        case DebugMode::Emission:
        case DebugMode::Metallic:
        case DebugMode::Roughness:
        case DebugMode::Normal:
        {
            Vector2 renderSize = Vector2(std::floor(
                                             m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate),
                                         std::floor(
                                             m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate));
            m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                       GetScreenSize(m_displaySize),
                                       passID);
                SetSpaceResource(passData, PER_PASS_SPACE);
                m_pCommand->DrawFullScreenTriangle();
            }
            m_pCommand->AddBarrier(m_pDisplayRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            break;
        }

        }
    }

    void DebugPass::DoAABBPass()
    {
        auto passID = ShaderPasseIDs::DebugPassID;
        auto& passData = m_pMaterial->GetPassData(passID);

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
                .m_renderTargetFormats = {m_pDisplayRT->GetFormat()},
                .m_numRenderTargets = 1,
                .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
            };
            passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID,
                desc,
                D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
        }
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        pipelineStateData.m_renderTargets = {m_pDisplayRT->GetTexture()};
        // pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_displaySize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

        m_aabbDrawer.Clear();
        if (!m_aabbDrawer.IsReady() || SceneManager::GetInstance().GetEntities().empty())
            return;

        auto& entities = SceneManager::GetInstance().GetEntities()[0];
        auto instanceID = UserData::GetInstance().instanceID;
        instanceID = MathHelper::Max(0, instanceID);
        if (instanceID < entities->GetChildren().size())
        {
            const auto entity = entities->GetChildren()[instanceID].get();
            const auto AABB = entity->GetWorldAABB();
            const auto min = AABB.Center - AABB.Extents;
            const auto max = AABB.Center + AABB.Extents;
            m_aabbDrawer.AddAABB(min, max, instanceID);
        }
        else
        {
            m_aabbDrawer.m_instanceCpuData.reserve(entities->GetChildren().size());
            for (UINT i = 0; i < entities->GetChildren().size(); i ++)
            {
                const auto entity = entities->GetChildren()[i].get();
                const auto AABB = entity->GetWorldAABB();
                const auto min = AABB.Center - AABB.Extents;
                const auto max = AABB.Center + AABB.Extents;
                m_aabbDrawer.AddAABB(min, max, instanceID);
            }
        }

        m_pCommand->SetVertexBuffer(0, 1, m_aabbDrawer.vertexView);
        m_pCommand->SetIndexBuffer(m_aabbDrawer.indexView);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_DebugMode,
                                 static_cast<UINT>(DebugMode::AABB));
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
            m_aabbDrawer.Bind(m_pMaterial.get());
            SetSpaceResource(passData, PER_PASS_SPACE);
            m_aabbDrawer.Draw(m_pCommand);
        }
        m_pCommand->AddBarrier(m_pDisplayRT,
                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void DebugPass::DoGIPass()
    {
        auto passID = ShaderPasseIDs::DebugPassID;
        auto& passData = m_pMaterial->GetPassData(passID);

        if (!GIPass::m_vertexBuffer->GetIsReady() || !GIPass::m_indexBuffer->GetIsReady())
            return;

        m_pCommand->SetVertexBuffer(0, 1, GIPass::m_vertexView);
        m_pCommand->SetIndexBuffer(GIPass::m_indexView);

        m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        {
            m_pMaterial->SetUINT(GIPass::ShaderIDs::g_IrradianceTexIndex,
                                 RenderTargetManager::GetInstance().GetRenderTexture(
                                                                       GIPass::RenderTextureIDs::IrradianceRTID)
                                                                   ->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(GIPass::ShaderIDs::g_RayDataBufferIndex,
                                 GIPass::m_pRayDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(GIPass::ShaderIDs::g_GIDataBufferIndex,
                                 GIPass::m_pGIDataBuffer->GetResourceHeapIndex(),
                                 passID);
            // m_pMaterial->SetUInt(GIPass::ShaderIDs::g_ProbeOffsetsIndex,
            //                      GIPass::m_pProbeOffsetBuffer->GetResourceHeapIndex());
            m_pMaterial->SetUINT(GIPass::ShaderIDs::g_ProbeStatesIndex,
                                 GIPass::m_pProbeStateBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(GIPass::ShaderIDs::g_ProbeOffsetIndexTexIndex,
                                 GIPass::m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(GIPass::ShaderIDs::g_ProbeRelocationLUTBufferIndex,
                                 GIPass::m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_IsEnableGILine,
                                 false,
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_bHideInactiveProbe,
                                 UserData::GetInstance().GIParameter.bHideInactiveProbe,
                                 passID);

            m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridDimensions,
                                   Vector3(GIPass::Grid_Dimensions.x,
                                           GIPass::Grid_Dimensions.y,
                                           GIPass::Grid_Dimensions.z),
                                   passID);
            m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridOrigin,
                                   UserData::GetInstance().GIParameter.probeGroupOrigin,
                                   passID);
            m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridSpacing,
                                   GIPass::m_gridSpacing,
                                   passID);
            m_pMaterial->SetFloat(GIPass::ShaderIDs::g_ProbeRadius,
                                  0.1f,
                                  passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->DrawInstanced(GIPass::NumIndices, GIPass::Probe_Count, 0, 0, 0);
        }
        m_pCommand->AddBarrier(m_pDisplayRT,
                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        if (UserData::GetInstance().GIParameter.enableLine)
        {
            {
                std::vector<std::wstring> enableKeywords{};

                auto passID = ShaderPasseIDs::DebugPassID;
                auto& passData = m_pMaterial->GetPassData(passID);
                auto VariantManager = passData.pShader->GetVariantManager();
                passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(
                    enableKeywords);
                const RenderTargetDesc desc =
                {
                    .m_renderTargetFormats = {m_pDisplayRT->GetFormat()},
                    .m_numRenderTargets = 1,
                    .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
                };
                passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
                    m_pDevice,
                    m_pMaterial.get(),
                    passID,
                    desc,
                    D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

                PipelineInfo pipelineStateData{};
                pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                    passID).pPipelineStateObject;
                pipelineStateData.m_renderTargets = {m_pDisplayRT->GetTexture()};
                // pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
                m_pCommand->SetPipeline(pipelineStateData);
                SetSpaceResource(passData, PER_FRAME_SPACE);
            }
            m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_displaySize));
            m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

            m_pCommand->GetCommandList()->IASetVertexBuffers(0, 0, nullptr);
            m_pCommand->GetCommandList()->IASetIndexBuffer(nullptr);

            m_pCommand->AddBarrier(m_pDisplayRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            {
                m_pMaterial->SetUINT(GIPass::ShaderIDs::g_RayDataBufferIndex,
                                     GIPass::m_pRayDataBuffer->GetResourceHeapIndex());
                // m_pMaterial->SetUInt(GIPass::ShaderIDs::g_ProbeOffsetsIndex,
                //                      GIPass::m_pProbeOffsetBuffer->GetResourceHeapIndex());
                m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridDimensions,
                                       Vector3(GIPass::Grid_Dimensions.x,
                                               GIPass::Grid_Dimensions.y,
                                               GIPass::Grid_Dimensions.z));
                m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridOrigin, GIPass::m_gridOrigin);
                m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridSpacing, GIPass::m_gridSpacing);
                m_pMaterial->SetFloat4(GIPass::ShaderIDs::g_RandomRotation,
                                       GIPass::m_RandomRotation);
                m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                       m_pCamera->GetViewMat() * m_pCamera->GetProjMat());

                m_pMaterial->SetUINT(ShaderIDs::g_DebugMode,
                                     static_cast<UINT>(UserData::GetInstance().debugMode));
                m_pMaterial->SetUINT(ShaderIDs::g_IsEnableGILine, 1);
                m_pMaterial->SetFloat(ShaderIDs::g_DebugLineScale,
                                      UserData::GetInstance().GIParameter.lineWidth);

                SetSpaceResource(passData, PER_PASS_SPACE);
                uint32_t totalVertexCount = GIPass::Probe_Count * GIPass::Rays_Per_Probe * 2;
                m_pCommand->DrawInstanced(totalVertexCount, 1, 0, 0);
            }
            m_pCommand->AddBarrier(m_pDisplayRT,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
                .m_renderTargetFormats = {!UserData::GetInstance().hdrParameter.IsUseHDR
                                              ? DXGI_FORMAT_R8G8B8A8_UNORM
                                              : UserData::GetInstance().hdrParameter.
                                                                        HDRLevel == HDRQuality::Low
                                              ? DXGI_FORMAT_R11G11B10_FLOAT
                                              : DXGI_FORMAT_R16G16B16A16_FLOAT},
                .m_numRenderTargets = 1,
                .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
            };
            passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID,
                desc,
                D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        }
    }

    void DebugPass::AABBDrawer::Init()
    {
        UploadVertexIndex();
    }

    void DebugPass::AABBDrawer::UploadVertexIndex()
    {
        BufferCreationDesc vertexBufferDesc =
        {
            .name = L"AABB Vertex Buffer",
            .stride = sizeof(Vector3),
            .size = sizeof(Vector3) * NumVertices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = vertices
        };
        BufferCreationDesc indexBufferDesc =
        {
            .name = L"AABB Index Buffer",
            .stride = 0,
            .size = sizeof(INDEX_FORMAT) * NumIndices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = indices
        };
        vertexBuffer = BufferManager::GetInstance().CreateVertexBuffer(vertexBufferDesc);
        indexBuffer = BufferManager::GetInstance().CreateIndexBuffer(indexBufferDesc);

        vertexView = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation = vertexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(NumVertices) * vertexBuffer->GetStride(),
            .StrideInBytes = vertexBuffer->GetStride()
        };
        indexView =
        {
            .BufferLocation = indexBuffer->GetGPUAddress(),
            .SizeInBytes = NumIndices * ElysiaModel::IndexSize(),
            .Format = ElysiaModel::IndexBufferFormat(),
        };
    }

    void DebugPass::AABBDrawer::AddAABB(const Vector3& min,
                                        const Vector3& max,
                                        const Vector4& color)
    {
        AABBInstanceData data
        {
            .Min = min,
            .Max = max,
            .Color = color

        };
        m_instanceCpuData.emplace_back(data);
    }

    void DebugPass::AABBDrawer::AddAABB(const Vector3& min,
                                        const Vector3& max,
                                        const UINT index)
    {
        AABBInstanceData data
        {
            .Min = min,
            .Max = max,
            .Color = GetDebugColor(index)
        };
        m_instanceCpuData.emplace_back(data);
    }

    void DebugPass::AABBDrawer::Clear()
    {
        m_instanceCpuData.clear();
    }

    void DebugPass::AABBDrawer::Bind(Material* pMaterail)
    {
        BufferCreationDesc instanceDataDesc =
        {
            .name = L"AABB Data Buffer",
            .stride = sizeof(AABBInstanceData),
            .size = sizeof(AABBInstanceData) * m_instanceCpuData.size(),
            .viewFlags = GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::HostWritable,
            .isRawAccess = false,
            .InitData = m_instanceCpuData.data()
        };
        if (instanceDataBuffer)
            BufferManager::GetInstance().DestoryBuffer(instanceDataBuffer);
        instanceDataBuffer = BufferManager::GetInstance().CreateBuffer(instanceDataDesc);
        pMaterail->SetUINT(ShaderIDs::g_AABBInstanceDatasIndex,
                           instanceDataBuffer->GetResourceHeapIndex());
    }
    void DebugPass::AABBDrawer::Draw(DX12GraphicsContext* context)
    {
        context->DrawInstanced(NumIndices, m_instanceCpuData.size(), 0, 0, 0);
    }

    Color DebugPass::AABBDrawer::GetDebugColor(uint32_t id)
    {
        float r = ((id * 183L + 123L) % 255) / 255.0f;
        float g = ((id * 592L + 456L) % 255) / 255.0f;
        float b = ((id * 721L + 789L) % 255) / 255.0f;
        return Color(r, g, b, 1.0f);
    }
}