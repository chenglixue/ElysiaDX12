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

        {
            m_pVisbibleCounterBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"GBuffer Visbible Counter Buffer",
                .stride = sizeof(int),
                .size = 16,
                .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
            });

            m_pVisbibleIndexBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"GBuffer Visbible Index Buffer",
                .stride = sizeof(int),
                .size = sizeof(int) * Max_RenderItem_Count,
                .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
            });
        }
    }

    GBufferPass::~GBufferPass()
    {
        Dispose();
    }

    void GBufferPass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_cameraWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_cameraHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);
        CreateRTs();

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }

    void GBufferPass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        if (context.renderList.empty())
            return;
        UpdateTAAMatrices();
        DoCulling(context.renderList);
        UploadMeshData(context.renderList);
        DrawGBufferPass(context);

        TAAData::Pre_View_M = m_pCamera->GetViewMat();
        TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
        TAAData::Pre_Proj_M = m_currMatrixP;
        TAAData::Pre_Proj_I_M = m_currMatrixP.Invert();
        TAAData::Pre_ViewProj_M = m_currMatrixVP;
        TAAData::Pre_ViewProj_I_M = m_currMatrixVP_I;
    }

    void GBufferPass::Dispose()
    {
        m_GBufferRTs.clear();
    }

    void GBufferPass::CreateRTs()
    {
        m_GBufferRTs.clear();
        // Base Color , ShadingModel
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_cameraWidth),
                static_cast<UINT64>(m_cameraHeight),
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer0ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Metallic, Specular, Roughness, AO
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_cameraWidth),
                static_cast<UINT64>(m_cameraHeight),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer1ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Encode World Tangent, Anisotropy
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_cameraWidth),
                static_cast<UINT64>(m_cameraHeight),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer2ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Encode World Normal, per object data
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_cameraWidth),
                static_cast<UINT64>(m_cameraHeight),
                DXGI_FORMAT_R10G10B10A2_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer3ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Emission, opacity
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_cameraWidth),
                static_cast<UINT64>(m_cameraHeight),
                DXGI_FORMAT_R10G10B10A2_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer4ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Velocity
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_cameraWidth),
                static_cast<UINT64>(m_cameraHeight),
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
        UpdateGBufferCullingVariant(CS_GBUFFER_CULLING_PASS);
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
    void GBufferPass::UpdateGBufferCullingVariant(UINT passIndex)
    {
        std::vector<std::wstring> enableKeywords{};

        auto passID = PassID(passIndex);
        auto& passData = m_pMaterial->GetPassData(passID);
        auto VariantManager = passData.pShader->GetVariantManager();
        passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

        passData.pPipelineStateObject =
            PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pMaterial.get(),
                passID);
    }

    void GBufferPass::UpdateTAAMatrices()
    {
        auto viewMatrix = m_pCamera->GetViewMat();
        auto projMatrix = m_currMatrixP = m_pCamera->GetProjMat();

        m_jitterMatrixProj = projMatrix;

        m_preJitterUV = m_currJitterUV;
        m_currJitterUV = Jitter::SampleJitterUV(UserData::GetInstance().taaParameter.jitterType);
        m_jitterMatrixProj.m[2][0] += m_currJitterUV.x * 2.f / m_displayWidth * UserData::GetInstance().taaParameter.
                                                                                                        jitterIntensity;
        m_jitterMatrixProj.m[2][1] -= m_currJitterUV.y * 2.f / m_displayHeight * UserData::GetInstance().taaParameter.
                                                                                                         jitterIntensity;

        m_currMatrixVP = viewMatrix * projMatrix;
        m_currMatrixVP_I = m_currMatrixVP.Invert();
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

    std::vector<Vector4> GBufferPass::ExtractFrustumPlanes(const Matrix& viewProj)
    {
        // XYZ 是法线，W 是距离
        std::vector<Vector4> frustumPlanes;
        frustumPlanes.resize(6);

        // Left plane 
        frustumPlanes[0] = Vector4(viewProj._14 + viewProj._11,
                                   viewProj._24 + viewProj._21,
                                   viewProj._34 + viewProj._31,
                                   viewProj._44 + viewProj._41);
        // Right plane
        frustumPlanes[1] = Vector4(viewProj._14 - viewProj._11,
                                   viewProj._24 - viewProj._21,
                                   viewProj._34 - viewProj._31,
                                   viewProj._44 - viewProj._41);
        // Bottom plane
        frustumPlanes[2] = Vector4(viewProj._14 + viewProj._12,
                                   viewProj._24 + viewProj._22,
                                   viewProj._34 + viewProj._32,
                                   viewProj._44 + viewProj._42);
        // Top plane
        frustumPlanes[3] = Vector4(viewProj._14 - viewProj._12,
                                   viewProj._24 - viewProj._22,
                                   viewProj._34 - viewProj._32,
                                   viewProj._44 - viewProj._42);
        // Near plane (DirectX 的 Z 是 0 到 1，所以 Near 是 row3)
        frustumPlanes[4] = Vector4(viewProj._13, viewProj._23, viewProj._33, viewProj._43);
        // Far plane
        frustumPlanes[5] = Vector4(viewProj._14 - viewProj._13,
                                   viewProj._24 - viewProj._23,
                                   viewProj._34 - viewProj._33,
                                   viewProj._44 - viewProj._43);

        // 归一化平面的法线
        for (int i = 0; i < 6; ++i)
        {
            float length = sqrt(
                frustumPlanes[i].x * frustumPlanes[i].x + frustumPlanes[i].y * frustumPlanes[i].y + frustumPlanes[i].z *
                frustumPlanes[i].z);
            frustumPlanes[i] /= length;
        }
        return frustumPlanes;
    }
    void GBufferPass::DoCulling(const std::vector<RenderItem>& renderItems)
    {
        auto passID = CS_GBUFFER_CULLING_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_aabbLoader.Clear();
        if (SceneManager::GetInstance().GetEntities().empty())
            return;

        auto& entities = SceneManager::GetInstance().GetRootEntity();
        m_aabbLoader.m_instanceCpuData.reserve(entities->GetChildren().size());
        for (UINT i = 0; i < entities->GetChildren().size(); i ++)
        {
            const auto entity = entities->GetChildren()[i].get();
            const auto AABB = entity->GetWorldAABB();
            const auto min = AABB.Center - AABB.Extents;
            const auto max = AABB.Center + AABB.Extents;
            m_aabbLoader.AddAABB(min, max);
        }

        auto viewMatrix = m_pCamera->GetViewMat();
        auto projMatrix = m_currMatrixP = m_pCamera->GetProjMat();

        m_pCommand->AddBarrier(*m_pVisbibleCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pVisbibleIndexBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_aabbLoader.Bind(m_pMaterial.get());
            m_pMaterial->SetUINT(ShaderIDs::g_VisbibleCounterBufferIndex,
                                 m_pVisbibleCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_VisbibleIndexBufferIndex,
                                 m_pVisbibleIndexBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_TotalObjectCount,
                                 renderItems.size(),
                                 passID);
            m_pMaterial->SetVector4Array(ShaderIDs::g_FrustumPlanes,
                                         ExtractFrustumPlanes(viewMatrix * projMatrix),
                                         passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(renderItems.size(), threadGroupSize.x),
                                 threadGroupSize.y,
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pVisbibleCounterBuffer, false);
            m_pCommand->AddUAVBarrier(m_pVisbibleIndexBuffer, false);
        }
        m_pCommand->AddBarrier(*m_pVisbibleCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(*m_pVisbibleIndexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
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
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_cameraWidth, m_cameraHeight));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pCommand->SetIndexBuffer(BufferManager::GetInstance().GetGlobalIndexBufferView());
        m_pCommand->SetVertexBuffer(0,
                                    1,
                                    BufferManager::GetInstance().GetGlobalVertexBufferView());

        m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                               GetScreenSize(Vector2(m_cameraWidth, m_cameraHeight)));
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_currMatrixP);
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_currMatrixP.Invert());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix, m_currMatrixVP);
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I, m_currMatrixVP_I);
        m_pMaterial->SetMatrix(ShaderIDs::jitterProjMatrix, m_jitterMatrixProj);
        m_pMaterial->SetMatrix(ShaderIDs::jitterProjMatrix_I, m_jitterMatrixProj.Invert());
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
        m_pMaterial->SetUINT(GIPass::ShaderIDs::g_IrradianceTexIndex,
                             GIPass::m_pIrradianceRT->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(GIPass::ShaderIDs::g_DistanceTexIndex,
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
        m_pMaterial->SetUINT(GIPass::ShaderIDs::g_ProbeStatesIndex,
                             GIPass::m_pProbeStateBuffer->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(GIPass::ShaderIDs::g_ProbeOffsetIndexTexIndex,
                             GIPass::m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetUINT(GIPass::ShaderIDs::g_ProbeRelocationLUTBufferIndex,
                             GIPass::m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                             passID);
        m_pMaterial->SetFloat(GIPass::ShaderIDs::g_DDGIEncodingGamma,
                              UserData::GetInstance().GIParameter.gamma,
                              passID);
        m_pMaterial->SetFloat(ShaderIDs::g_AmbientIntensity,
                              UserData::GetInstance().AmbientCubemapIntensity,
                              passID);
        m_pMaterial->SetUINT(ShaderIDs::g_VisbibleIndexBufferIndex,
                             m_pVisbibleIndexBuffer->GetResourceHeapIndex(),
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
                                                      m_pVisbibleCounterBuffer->GetResource(),
                                                      0);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pIndirectDataBuffer->GetResource(),
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_pCommand->GetCommandList()->ResourceBarrier(1, &barrier);
    }

    void GBufferPass::AABBLoader::AddAABB(const Vector3& min, const Vector3& max)
    {
        AABBInstanceData data
        {
            .Min = min,
            .Max = max,
        };
        m_instanceCpuData.emplace_back(data);
    }
    void GBufferPass::AABBLoader::Clear()
    {
        m_instanceCpuData.clear();
    }
    void GBufferPass::AABBLoader::Bind(Material* pMaterail)
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
}