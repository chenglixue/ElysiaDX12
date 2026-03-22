#include "stdafx.h"
#include "GBufferPass.h"

#include "GIPass.h"
#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12UploadContext.h"

#include "Runtime/Resource/Model/ModelManager.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    using namespace ElysiaModel;

    GBufferPass::GBufferPass()
        : BasePass()
    {
        m_indirectCommands.reserve(Max_RenderItem_Count);
        m_meshDatas.reserve(Max_RenderItem_Count);

        {
            auto bufferSize = sizeof(IndirectCommand) * Max_RenderItem_Count;
            m_pIndirectDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"GBuffer Indirect Buffer",
                .stride = sizeof(IndirectCommand),
                .size = bufferSize,
                .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
                .isRawAccess = true,
                .isIndirectBuffer = true
            });

            m_uploads.reserve(2);
            auto pUpload = new DX12BufferUpload();
            pUpload->buffer = nullptr;
            pUpload->bufferDataSize = sizeof(MeshData) * Max_RenderItem_Count;
            pUpload->pBufferData = std::make_unique<uint8_t[]>(
                sizeof(MeshData) * Max_RenderItem_Count);
            m_uploads.emplace_back(pUpload);

            pUpload = new DX12BufferUpload();
            pUpload->buffer = nullptr;
            pUpload->bufferDataSize = sizeof(IndirectCommand) * Max_RenderItem_Count;
            pUpload->pBufferData = std::make_unique<uint8_t[]>(
                sizeof(IndirectCommand) * Max_RenderItem_Count);
            m_uploads.emplace_back(pUpload);
        }

    }

    GBufferPass::~GBufferPass()
    {
        Dispose();
    }

    void GBufferPass::Configure()
    {
        CreateRTs();

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);

        UpdatePipeline();
    }

    void GBufferPass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        if (context.renderList.empty())
            return;
        UploadMeshData(context.renderList);
        DrawGBufferPass(context);

        TAAData::Pre_View_M = m_pCamera->GetViewMat();
        TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
        TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
        TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
        TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
        TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).
            Invert();
    }

    void GBufferPass::Dispose()
    {
        m_GBufferRTs.clear();
    }

    void GBufferPass::UploadMeshData(const std::vector<RenderItem>& renderItems)
    {
        const auto renderItemCount = renderItems.size();

        m_meshDatas.clear();
        for (UINT64 i = 0; i < renderItemCount; i ++)
        {
            const auto& materialData = renderItems[i].loadedMaterial;
            const auto& textureIndices = renderItems[i].textureIndices;
            auto meshData = MeshData
            {
                .world_M = renderItems[i].worldMatrix,

                .opacity = materialData.opacity,
                .cutoff = 0.5,
                .baseColorTexIndex = textureIndices.Albedo,
                .normalTexIndex = textureIndices.Normal,

                .metallicTexIndex = textureIndices.Metallic,
                .roughnessTexIndex = textureIndices.Roughness,
                .specularTexIndex = textureIndices.Specular,
                .metallicIntensity = UserData::GetInstance().MetallicIntensity,

                .baseColorTint = UserData::GetInstance().BaseColorTint,

                .roughnessIntensity = UserData::GetInstance().RoughnessIntensity,
                .normalIntensity = UserData::GetInstance().NormalIntensity,
                .vertexOffset = renderItems[i].baseVertex,
                .indexOffset = renderItems[i].startIndex
            };
            m_meshDatas.emplace_back(meshData);
        }

        if (!m_pMeshDataBuffer)
        {
            m_pMeshDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"GBuffer Mesh Data Buffer",
                .stride = sizeof(MeshData),
                .size = Max_RenderItem_Count * sizeof(MeshData),
                .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
                .isRawAccess = false,
            });
        }
        m_uploads[0]->buffer = m_pMeshDataBuffer;
        memcpy(m_uploads[0]->pBufferData.get(),
               m_meshDatas.data(),
               Max_RenderItem_Count * sizeof(MeshData));

        m_indirectCommands.clear();
        UINT renderItemIndex = 0;
        UINT meshDataBufferIndex = m_pMeshDataBuffer->GetResourceHeapIndex();
        for (auto& renderItem : renderItems)
        {
            IndirectCommand indirectCommand{};
            indirectCommand.pushConstants =
            {
                .meshDataBufferIndex = meshDataBufferIndex,
                .meshDataIndex = renderItemIndex
            };
            indirectCommand.drawArguments = D3D12_DRAW_INDEXED_ARGUMENTS
            {
                .IndexCountPerInstance = renderItem.indexCount,
                .InstanceCount = 1,
                .StartIndexLocation = renderItem.startIndex,
                .BaseVertexLocation = int(renderItem.baseVertex),
                .StartInstanceLocation = 0,
            };
            m_indirectCommands.emplace_back(indirectCommand);
            renderItemIndex ++;
        }
        m_uploads[1]->buffer = m_pIndirectDataBuffer;
        memcpy(m_uploads[1]->pBufferData.get(),
               m_indirectCommands.data(),
               Max_RenderItem_Count * sizeof(IndirectCommand));
    }

    void GBufferPass::CreateRTs()
    {
        m_GBufferRTs.clear();
        // Base Color , ShadingModel
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer0ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Metallic, Specular, Roughness, AO
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer1ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Encode World Tangent, Anisotropy
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer2ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Encode World Normal, per object data
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R10G10B10A2_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer3ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Emission, opacity
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R10G10B10A2_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer4ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Velocity
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R16G16B16A16_SNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer5ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }
    }

    std::vector<DX12TextureResource*> GBufferPass::GetGBuffers()
    {
        std::vector<DX12TextureResource*> temp{};
        temp.reserve(m_GBufferRTs.size());
        for (auto& RT : m_GBufferRTs)
        {
            temp.emplace_back(RT->GetTexture());
        }

        return temp;
    }

    void GBufferPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;
        UpdateGBufferPassVariant(DRAW_GBUFFER_PASS);

    }

    void GBufferPass::UpdateGBufferPassVariant(UINT passIndex)
    {
        std::vector<std::wstring> enableKeywords{};

        auto& passData = m_pMaterial->GetPassData(passIndex);
        auto VariantManager = passData.pShader->GetVariantManager();
        passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

        RenderTargetDesc RTDesc = RenderTargetDesc
        {
            .m_numRenderTargets = static_cast<UINT8>(m_GBufferRTs.size()),
            .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
        };
        for (int i = 0; i < m_GBufferRTs.size(); ++i)
        {
            RTDesc.m_renderTargetFormats[i] = m_GBufferRTs[i]->GetFormat();
        }
        passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
            m_pDevice,
            m_pMaterial.get(),
            passIndex,
            RTDesc);

        if (!m_pCommandSignature)
        {
            // 对应 IndirectCommand::pushConstants
            D3D12_INDIRECT_ARGUMENT_DESC args[2] = {};
            args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            args[0].Constant.RootParameterIndex = PER_MATERIAL_SPACE - 1;
            // 对应 PER_MATERIAL_SPACE 的槽位
            args[0].Constant.DestOffsetIn32BitValues = 0;
            args[0].Constant.Num32BitValuesToSet = 2; // 两个 UINT

            // 对应 IndirectCommand::drawArguments
            args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

            D3D12_COMMAND_SIGNATURE_DESC desc = {};
            desc.ByteStride = sizeof(IndirectCommand);
            desc.NumArgumentDescs = 2;
            desc.pArgumentDescs = args;

            m_pDevice->GetDevice()->CreateCommandSignature(&desc,
                                                           passData.pRootSignature->GetSignature(),
                                                           IID_PPV_ARGS(&m_pCommandSignature));
        }

    }

    void GBufferPass::DrawGBufferPass(ElysiaEngine::FrameContext& context)
    {
        auto passID = DRAW_GBUFFER_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(passID).
                                                               pPipelineStateObject;
        pipelineStateData.m_renderTargets = std::move(GetGBuffers());
        pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);

        for (auto& RT : m_GBufferRTs)
        {
            m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
        }
        m_pCommand->AddBarrier(m_pCameraDepthRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        for (auto& RT : m_GBufferRTs)
        {
            m_pCommand->ClearRenderTarget(RT, Color::Black);
        }
        m_pCommand->ClearDepthStencilTarget(m_pCameraDepthRT, 1.f, 0);
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pCommand->SetIndexBuffer(BufferManager::GetInstance().GetGlobalIndexBufferView());
        m_pCommand->SetVertexBuffer(0,
                                    1,
                                    BufferManager::GetInstance().GetGlobalVertexBufferView());

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
        m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridDimensions,
                               Vector3(GIPass::Grid_Dimensions.x,
                                       GIPass::Grid_Dimensions.y,
                                       GIPass::Grid_Dimensions.z),
                               passID);
        m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridOrigin,
                               GIPass::m_gridOrigin,
                               passID);
        m_pMaterial->SetFloat3(GIPass::ShaderIDs::g_GridSpacing,
                               GIPass::m_gridSpacing,
                               passID);
        m_pMaterial->SetUInt(GIPass::ShaderIDs::g_IrradianceTexIndex,
                             GIPass::m_pIrradianceRT->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUInt(GIPass::ShaderIDs::g_DistanceTexIndex,
                             GIPass::m_pDistanceRT->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetFloat4(GIPass::ShaderIDs::g_IrradianceTexSize,
                               GetScreenSize(GIPass::m_pIrradianceRT->GetWidth(),
                                             GIPass::m_pIrradianceRT->GetHeight()),
                               passID);
        m_pMaterial->SetFloat4(GIPass::ShaderIDs::g_DistanceTexSize,
                               GetScreenSize(GIPass::m_pDistanceRT->GetWidth(),
                                             GIPass::m_pDistanceRT->GetHeight()),
                               passID);
        m_pMaterial->SetFloat(GIPass::ShaderIDs::g_ProbeNormalBias,
                              UserData::GetInstance().GIParameter.normalBias,
                              passID);
        m_pMaterial->SetFloat(GIPass::ShaderIDs::g_ProbeViewBias,
                              UserData::GetInstance().GIParameter.viewBias,
                              passID);
        m_pMaterial->SetFloat3(ShaderIDs::g_AmbientTint,
                               UserData::GetInstance().AmbientCubemapTint,
                               passID);
        // m_pMaterial->SetUInt(GIPass::ShaderIDs::g_ProbeOffsetsIndex,
        //                      GIPass::m_pProbeOffsetBuffer->GetResourceHeapIndex(),
        //                      passID);
        m_pMaterial->SetUInt(GIPass::ShaderIDs::g_ProbeStatesIndex,
                             GIPass::m_pProbeStateBuffer->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUInt(GIPass::ShaderIDs::g_ProbeOffsetIndexTexIndex,
                             GIPass::m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUInt(GIPass::ShaderIDs::g_ProbeRelocationLUTBufferIndex,
                             GIPass::m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetFloat(GIPass::ShaderIDs::g_DDGIEncodingGamma,
                              UserData::GetInstance().GIParameter.gamma,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AmbientIntensity,
                              UserData::GetInstance().AmbientCubemapIntensity,
                              passID);

        SetSpaceResource(passData, PER_PASS_SPACE);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        DrawMesh(context, passID);

        for (auto& RT : m_GBufferRTs)
        {
            m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
        }
        m_pCommand->AddBarrier(m_pCameraDepthRT,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                               D3D12_RESOURCE_STATE_DEPTH_READ);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    void GBufferPass::DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex)
    {
        auto& passData = m_pMaterial->GetPassData(passIndex);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pIndirectDataBuffer->GetResource(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST
            );
        m_pCommand->GetCommandList()->ResourceBarrier(1, &barrier);

        BufferManager::GetInstance().UploadBufferData(m_pCommand, m_uploads);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pIndirectDataBuffer->GetResource(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT
            );
        m_pCommand->GetCommandList()->ResourceBarrier(1, &barrier);

        m_pCommand->GetCommandList()->ExecuteIndirect(m_pCommandSignature,
                                                      context.renderList.size(),
                                                      m_pIndirectDataBuffer->GetResource(),
                                                      0,
                                                      nullptr,
                                                      0);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pIndirectDataBuffer->GetResource(),
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_pCommand->GetCommandList()->ResourceBarrier(1, &barrier);
    }
}