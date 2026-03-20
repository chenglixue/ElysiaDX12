#include "stdafx.h"
#include "GIPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Programs/SobolSequenceGenerator.h"
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
#include "Runtime/RenderCore/MeshRenderer.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"


namespace ElysiaRenderer
{
    GIPass::GIPass()
        : BasePass()
    {

        BufferCreationDesc vertexBufferDesc =
        {
            .name = L"GI Vertex Buffer",
            .stride = sizeof(Vector3),
            .size = sizeof(Vector3) * NumVertices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_probeVertices
        };
        BufferCreationDesc indexBufferDesc =
        {
            .name = L"GI Index Buffer",
            .stride = 0,
            .size = sizeof(INDEX_FORMAT) * NumIndices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_probeIndices
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

        m_pRayDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"DDGI Ray Data Buffer",
            .stride = sizeof(RayData),
            .size = sizeof(RayData) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false
        });

        m_pGIDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"DDGI Ray Data Buffer",
            .stride = sizeof(GIData),
            .size = sizeof(GIData) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false
        });

        // m_pProbeOffsetBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        // {
        //     .name = L"Probe Offset Buffer",
        //     .stride = sizeof(Vector3),
        //     .size = sizeof(Vector3) * Probe_Count,
        //     .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
        //     .accessFlags = BufferAccessFlags::GPUOnly,
        // });

        m_pProbeStateBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Probe State Buffer",
            .stride = sizeof(UINT),
            .size = sizeof(UINT) * Probe_Count,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pStaticAABBDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Static AABB Buffer",
            .stride = sizeof(AABBData),
            .size = sizeof(AABBData) * Max_RenderItem_Count,
            .viewFlags = GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::HostWritable,
        });

        m_pProbeRelocationLUTBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Probe Relocation LUT Buffer",
            .stride = sizeof(Vector4),
            .size = sizeof(Vector4) * 256,
            .viewFlags = GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::HostWritable,
        });

        m_pCompactedRayDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Compacted RayData Buffer",
            .stride = sizeof(CompactedRay),
            .size = sizeof(CompactedRay) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pCompactedRayIndexBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Compacted Ray Index Buffer",
            .stride = sizeof(UINT),
            .size = sizeof(UINT) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pCounterBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Counter Buffer",
            .stride = sizeof(UINT),
            .size = 16,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pIndirectArgsBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Indirect Args Buffer",
            .stride = sizeof(UINT3),
            .size = 256,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pReservoirBuffer0 = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Reservoir Buffer 0",
            .stride = sizeof(Reservoir),
            .size = sizeof(Reservoir) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pReservoirBuffer1 = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Reservoir Buffer 1",
            .stride = sizeof(Reservoir),
            .size = sizeof(Reservoir) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_DXRBlob = CompileRaytracingLibrary(L"Shaders\\public\\GI\\DDGI_Library.hlsl");
    }

    GIPass::~GIPass()
    {
        Dispose();
    }
    void GIPass::Dispose()
    {
    }

    void GIPass::Configure()
    {
        if (!m_pDevice5)
            ThrowIfFailed(m_pDevice->GetDevice()->QueryInterface(IID_PPV_ARGS(&m_pDevice5)));

        m_halfWidth = UINT(m_renderSize.x) >> 1;
        m_halfHeight = UINT(m_renderSize.y) >> 1;
        m_quarterWidth = UINT(m_renderSize.x) >> 2;
        m_quarterHeight = UINT(m_renderSize.y) >> 2;

        m_pIrradianceRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            Grid_Dimensions.x * 8,
            Grid_Dimensions.y * Grid_Dimensions.z * 8,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::IrradianceRTID));
        m_pDistanceRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            Grid_Dimensions.x * 16,
            Grid_Dimensions.y * Grid_Dimensions.z * 16,
            DXGI_FORMAT_R16G16_FLOAT,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::DistanceRTID));

        m_pProbeOffsetIndexRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            64,
            ceil(Probe_Count / 64),
            DXGI_FORMAT_R8_UINT,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::ProbeOffsetIndexRTID));

        auto data = std::vector<UINT>(Probe_Count * Rays_Per_Probe, 0);
        DX12BufferUpload uploadData = DX12BufferUpload
        {
            .buffer = m_pCompactedRayIndexBuffer,
            .pBufferData = std::make_unique<uint8_t[]>(sizeof(UINT) * Probe_Count * Rays_Per_Probe),
            .bufferDataSize = sizeof(UINT) * Probe_Count * Rays_Per_Probe,
        };
        memcpy(uploadData.pBufferData.get(), data.data(), data.size() * sizeof(UINT));
        BufferManager::GetInstance().UploadBufferData(m_pDevice->GetUploadContext(), &uploadData);

        auto pBufferData = DX12BufferUpload
        {
            .buffer = m_pProbeStateBuffer,
            .pBufferData = std::make_unique<uint8_t[]>(sizeof(UINT) * Probe_Count),
            .bufferDataSize = sizeof(UINT) * Probe_Count
        };
        memcpy(pBufferData.pBufferData.get(), m_probeStates.data(), sizeof(UINT) * Probe_Count);
        BufferManager::GetInstance().UploadBufferData(m_pDevice->GetUploadContext(), &pBufferData);

        memcpy(m_pProbeRelocationLUTBuffer->GetMappedBuffer(),
               GenerateRelocationLUT(m_gridSpacing).data(),
               sizeof(Vector4) * 256);

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);

        UpdatePipeline();
    }

    void GIPass::Render(FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "GI Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_frameIndex = context.frameIndex;
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "GI Begin");

        if (!SceneManager::GetInstance().GetEntities().empty() && !SceneManager::GetInstance().
            GetEntities()[0]->GetChildren().empty())
        {
            auto& allEntities = SceneManager::GetInstance().GetEntities()[0]->GetChildren();
            auto entityCount = allEntities.size();
            auto instanceSize = entityCount * allEntities[0]->pMeshRenderer->m_pModel->meshes.size();

            m_instanceDatas.clear();
            m_AABBDatas.clear();
            m_instanceDatas.reserve(instanceSize);
            m_AABBDatas.reserve(Max_RenderItem_Count);
            bool needFlushBarrier = false;
            // first is root list
            for (UINT i = 0; i < entityCount; i ++)
            {
                auto currEntity = allEntities[i].get();

                auto AABB = currEntity->GetWorldAABB();
                m_AABBDatas.emplace_back(AABBData
                {
                    .Min = AABB.Center - AABB.Extents,
                    .Max = AABB.Center + AABB.Extents,
                });
                if (!currEntity->GetBLASBuffer())
                {
                    needFlushBarrier = true;
                    currEntity->GenerateBLAS(m_pDevice5, m_pCommand);
                }
                const auto& materials = currEntity->pMeshRenderer->m_pModel->materials;
                for (auto& mesh : currEntity->pMeshRenderer->m_pModel->meshes)
                {
                    m_instanceDatas.emplace_back(InstanceData
                    {
                        .BaseColorTexIndex = materials[mesh.materialIndex].
                        textures[UINT64(MaterialTextureType::Albedo)].GetResourceHeapIndex(),
                        .NormalTexIndex = materials[mesh.materialIndex].
                        textures[UINT64(MaterialTextureType::Normal)].GetResourceHeapIndex(),
                        .MetallicTexIndex = materials[mesh.materialIndex].
                        textures[UINT64(MaterialTextureType::Metallic)].GetResourceHeapIndex(),
                        .RoughnessTexIndex = materials[mesh.materialIndex].
                        textures[UINT64(MaterialTextureType::Roughness)].GetResourceHeapIndex(),

                        .VertexOffset = mesh.vtxOffset,
                        .IndexOffset = mesh.idxOffset,
                        .VertexBufferIndex = BufferManager::GetInstance().GetGlobalVertexBuffer()->
                                                                          GetResourceHeapIndex(),
                        .IndexBufferIndex = BufferManager::GetInstance().GetGlobalIndexBuffer()->
                                                                         GetResourceHeapIndex(),
                    });
                }
            }
            if (needFlushBarrier)
            {
                m_pCommand->FlushBarrier();
            }

            static bool isInitStaticAABB = false;
            if (!isInitStaticAABB)
            {
                isInitStaticAABB = true;
                memcpy(m_pStaticAABBDataBuffer->GetMappedBuffer(),
                       m_AABBDatas.data(),
                       sizeof(AABBData) * Max_RenderItem_Count);
            }

            if (!m_pInstanceDataBuffer || m_pInstanceDataBuffer->GetResourceDesc().Width < sizeof(
                    InstanceData) * entityCount)
            {
                if (m_pInstanceDataBuffer)
                    BufferManager::GetInstance().DestoryBuffer(m_pInstanceDataBuffer);
                m_pInstanceDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
                {
                    .name = L"GI Instance Data",
                    .stride = sizeof(InstanceData),
                    .size = instanceSize * sizeof(InstanceData),
                    .viewFlags = GPUResourceFlags::SRV,
                    .accessFlags = BufferAccessFlags::HostWritable,
                    .isRawAccess = false,
                    .InitData = m_instanceDatas.data(),
                });
            }
            else
            {
                memcpy(m_pInstanceDataBuffer->GetMappedBuffer(),
                       m_instanceDatas.data(),
                       instanceSize * sizeof(InstanceData));
            }
            GenerateTLAS(allEntities);
            if (!m_pRTPSO || !m_pGlobalRootSig)
            {
                CreateRaytracingPipeline(m_pGlobalRootSig, allEntities);
                m_stbHelper.Build(m_pDevice->GetDevice(), m_pRTPSO);
            }

            const Entity* pEntity = SceneManager::GetInstance().GetEntities()[0].get();
            auto sceneAABB = pEntity->GetWorldAABB();
            auto sceneMin = sceneAABB.Center - sceneAABB.Extents;
            auto sceneMax = sceneAABB.Center + sceneAABB.Extents;

            // 希望探针能稍微往里面缩一点，不要紧贴墙壁
            float padding = 0.5f;
            Vector3 effectiveMin = sceneMin + Vector3(padding, padding, padding);
            Vector3 effectiveMax = sceneMax - Vector3(padding, padding, padding);
            Vector3 effectiveSize = effectiveMax - effectiveMin;

            // 计算间距 (注意是 数量-1)
            float spacingX = effectiveSize.x / (Grid_Dimensions.x - 1);
            float spacingY = effectiveSize.y / (Grid_Dimensions.y - 1);
            float spacingZ = effectiveSize.z / (Grid_Dimensions.z - 1);

            m_gridSpacing = Vector3(spacingX, spacingY, spacingZ);
            m_gridOrigin = effectiveMin;
        }
        else
        {
            return;
        }
        if (!m_vertexBuffer->GetIsReady() || !m_indexBuffer->GetIsReady() || !m_pTLASBuffer || !
            m_pRayDataBuffer)
            return;

        ComputeRandomRotation();

        InitProbeOffsetIndex();
        ClearProbeOffset();
        GenerateRay();
        ResetCounterBuffer();
        CalcCompactedRay();
        CalcIndirectArgs();
        CalcIrradiance();
        ProbeBlend();
        RelocateProbes();
        UpdateProbeStates();
    }

    void GIPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < GI_PASS_COUNT; ++i)
        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            if (i == DDGI_SHADING)
                continue;

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(
                    m_pDevice,
                    m_pMaterial.get(),
                    passID);
        }

        auto passID = PassID(DDGI_SHADING);
        auto& passData = m_pMaterial->GetPassData(passID);
        if (!passData.pPipelineStateObject)
        {
            std::vector<std::wstring> enableKeywords{};

            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            if (passData.pRootSignature == nullptr)
            {
                auto resourceMapping = PipelineResourceMapping();
                std::vector<DX12RootParameter*> rootParameters{};
                std::array<std::vector<D3D12_DESCRIPTOR_RANGE1>, NUM_RESOURCE_SPACES> desciptorRanges;
                auto resourceLayout = *passData.pCurrVariantData->pMeshResourceLayout;
                for (UINT currSpaceID = 0; currSpaceID < NUM_RESOURCE_SPACES; ++currSpaceID)
                {
                    auto currSpace = resourceLayout.m_spaces[currSpaceID];
                    if (!currSpace)
                        continue;

                    if (currSpace->IsPushConstantSpace() && (
                            currSpaceID == PER_MATERIAL_SPACE || currSpaceID == PER_OBJECT_SPACE))
                    {
                        DX12RootParameter* rootParameter = new DX12RootParameter();
                        rootParameter->InitAsConstants(16, 0, currSpaceID, D3D12_SHADER_VISIBILITY_ALL);

                        resourceMapping.m_PushConstantMappings[currSpaceID] = static_cast<UINT>(
                            rootParameters.size());
                        rootParameters.emplace_back(std::move(rootParameter));
                        continue; // 处理完常量后跳过后续 Table 处理
                    }

                    std::vector<D3D12_DESCRIPTOR_RANGE1>& currDescriptorRange = desciptorRanges[
                        currSpaceID];

                    auto SRVs = currSpace->GetSRVs();
                    auto UAVs = currSpace->GetUAVs();

                    if (currSpace->HasExpectedCBV())
                    {
                        DX12RootParameter* rootParameter = new DX12RootParameter();
                        rootParameter->
                            InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL, currSpaceID);

                        resourceMapping.m_CBVMappings[currSpaceID] = static_cast<UINT>(rootParameters.
                            size());
                        rootParameters.emplace_back(std::move(rootParameter));
                    }

                    if (SRVs.empty() && UAVs.empty())
                    {
                        continue;
                    }

                    for (auto& uav : UAVs)
                    {
                        D3D12_DESCRIPTOR_RANGE1 range{};
                        range.BaseShaderRegister = uav->m_bindingIndex;
                        range.NumDescriptors = 1;
                        range.OffsetInDescriptorsFromTableStart = static_cast<uint32_t>(currDescriptorRange.
                            size());
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        range.RegisterSpace = currSpaceID;
                        range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                                      D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                        currDescriptorRange.push_back(range);
                    }

                    // all of SRV Resource has one DESCRIPTOR RANGE which only has one descriptor
                    for (auto& SRV : SRVs)
                    {
                        D3D12_DESCRIPTOR_RANGE1 pDescriptorRange{};
                        pDescriptorRange.BaseShaderRegister = SRV->m_bindingIndex;
                        pDescriptorRange.NumDescriptors = 1;
                        pDescriptorRange.OffsetInDescriptorsFromTableStart = static_cast<UINT>(
                            currDescriptorRange.size());
                        pDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                        pDescriptorRange.RegisterSpace = currSpaceID;
                        pDescriptorRange.Flags =
                            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE |
                            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                        currDescriptorRange.emplace_back(pDescriptorRange);
                    }

                    DX12RootParameter* rootParameter = new DX12RootParameter();
                    rootParameter->InitAsDescriptorTable(static_cast<UINT>(currDescriptorRange.size()),
                                                         currDescriptorRange.data(),
                                                         D3D12_SHADER_VISIBILITY_ALL);

                    resourceMapping.m_TableMappings[currSpaceID] = static_cast<UINT>(rootParameters.size());
                    rootParameters.emplace_back(std::move(rootParameter));
                }

                DX12RootParameter* rootParameter = new DX12RootParameter();
                rootParameter->InitAsShaderResourceView(0, PER_PASS_SPACE);
                rootParameters.emplace_back(rootParameter);

                UINT numRootParamter = static_cast<UINT>(rootParameters.size());
                UINT numSampler = 0;
                DX12RootSignature* rootSignature = new DX12RootSignature(numRootParamter, numSampler);

                m_pDevice->CreateRootParameters(rootSignature, rootParameters);

                rootSignature->Init(m_pDevice->GetDevice(),
                                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);

                assert(rootSignature);
                assert(rootSignature->GetSignature());

                passData.pRootSignature = std::unique_ptr<DX12RootSignature>(std::move(rootSignature));
                passData.resourceMapping = std::move(resourceMapping);
            }

            D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc{};
            PSODesc.CS = D3D12_SHADER_BYTECODE
            {
                .pShaderBytecode = passData.pCurrVariantData->StageShaders.at(ShaderType::Compute).
                                            bytecode->GetBufferPointer(),
                .BytecodeLength = passData.pCurrVariantData->StageShaders.at(ShaderType::Compute).
                                           bytecode->GetBufferSize(),
            };
            PSODesc.pRootSignature = passData.pRootSignature->GetSignature();

            auto pipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(m_pDevice,
                                                                                         PSODesc,
                                                                                         passData.pRootSignature.get());
            if (pipelineStateObject != nullptr)
            {
                pipelineStateObject->m_pipelineResourceMapping = passData.resourceMapping;
                pipelineStateObject->m_rootSignature = passData.pRootSignature.get();
            }

            passData.pPipelineStateObject = pipelineStateObject;
        }

        if (!m_pCommandSignature)
        {
            D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
            argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
            D3D12_COMMAND_SIGNATURE_DESC sigDesc = {};
            sigDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
            sigDesc.NumArgumentDescs = 1;
            sigDesc.pArgumentDescs = &argDesc;
            m_pDevice->GetDevice()->CreateCommandSignature(&sigDesc, nullptr, IID_PPV_ARGS(&m_pCommandSignature));
        }

        if (!m_pRTPSO || !m_pGlobalRootSig)
        {
            assert(m_DXRBlob && m_pDevice->GetDevice());
            {
                CreateDXRRootSignature(m_pDevice->GetDevice());
            }
        }

    }

    void GIPass::InitProbeOffsetIndex()
    {
        static bool isInit = false;
        if (isInit)
            return;

        auto passID = RESET_PROBE_STATES;
        auto passName = m_PassData[passID].Name.c_str();
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);

        m_pCommand->AddBarrier(m_pProbeOffsetIndexRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetIndexTexIndex,
                                 m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                                 passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pProbeOffsetIndexRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pProbeOffsetIndexRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
        }
        m_pCommand->AddBarrier(m_pProbeOffsetIndexRT,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);

        isInit = true;
    }
    void GIPass::ResetProbeStates()
    {
        auto passID = RESET_PROBE_STATES;
        auto passName = m_PassData[passID].Name.c_str();
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);

        m_pCommand->AddBarrier(*m_pProbeStateBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeStatesIndex,
                                 m_pProbeStateBuffer->GetResourceHeapIndex(),
                                 passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(Probe_Count, threadGroupSize.x),
                                 threadGroupSize.y,
                                 threadGroupSize.z);
        }
        m_pCommand->AddBarrier(*m_pProbeStateBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::UpdateProbeStates()
    {
        auto passID = UPDATE_PROBE_STATES;
        auto passName = m_PassData[passID].Name.c_str();
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pProbeStateBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                   passID);

            m_pMaterial->SetUInt(ShaderIDs::g_ProbeStatesIndex,
                                 m_pProbeStateBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_StaticAABBIndex,
                                 m_pStaticAABBDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_RayDataBufferIndex,
                                 m_pRayDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetIndexTexIndex,
                                 m_pProbeOffsetIndexRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_RelocationLUTIndex,
                                 m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridOrigin,
                                   Vector4(m_gridOrigin.x, m_gridOrigin.y, m_gridOrigin.z, 0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridSpacing,
                                   Vector4(m_gridSpacing.x, m_gridSpacing.y, m_gridSpacing.z, 0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridDimensions,
                                   Vector4(Grid_Dimensions.x,
                                           Grid_Dimensions.y,
                                           Grid_Dimensions.z,
                                           0.f),
                                   passID);
            m_pMaterial->SetUInt(ShaderIDs::g_StaticAABBCount, m_AABBDatas.size(), passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(Probe_Count, threadGroupSize.x),
                                 threadGroupSize.y,
                                 threadGroupSize.z);
        }
        m_pCommand->AddBarrier(*m_pProbeStateBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::GenerateRay()
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Generate Ray");

        m_pCommand->GetCommandList()->SetPipelineState1(m_pRTPSO);
        struct alignas(16)
        {
            Vector4 g_GridSpacing;
            Vector4 g_GridOrigin;
            Vector4 g_GridDimensions;
            Vector4 g_IrradianceTexSize;
            Vector4 g_DistanceTexSize;
            Vector4 g_RandomRotation;

            uint g_RayDataBufferIndex;
            UINT g_IrradianceTexIndex;
            UINT g_DistanceTexIndex;
            UINT g_ProbeOffsetIndexTexIndex;

            UINT g_PreReservoirBufferIndex;
            UINT g_CurrReservoirBufferIndex;
            float g_ProbeNormalBias;
            float g_ProbeViewBias;

            float g_DDGIEncodingGamma;
        } constantData;
        constexpr UINT constantSize = sizeof(constantData) / 4;

        if (!m_currHistoryIndex)
        {
            m_pPreReservoirBuffer = m_pReservoirBuffer0;
            m_pCurrReservoirBuffer = m_pReservoirBuffer1;
        }
        else
        {
            m_pPreReservoirBuffer = m_pReservoirBuffer1;
            m_pCurrReservoirBuffer = m_pReservoirBuffer0;
        }
        m_currHistoryIndex = (m_currHistoryIndex + 1) % 2;

        m_pCommand->AddBarrier(*m_pRayDataBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pCurrReservoirBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            assert(m_pGlobalRootSig != nullptr);
            m_pCommand->GetCommandList()->SetComputeRootSignature(m_pGlobalRootSig);

            constantData =
            {
                .g_GridSpacing = Vector4(m_gridSpacing.x, m_gridSpacing.y, m_gridSpacing.z, 0.f),
                .g_GridOrigin = Vector4(m_gridOrigin.x, m_gridOrigin.y, m_gridOrigin.z, 0.f),
                .g_GridDimensions = Vector4(Grid_Dimensions.x,
                                            Grid_Dimensions.y,
                                            Grid_Dimensions.z,
                                            0.f),
                .g_IrradianceTexSize = GetScreenSize(m_pIrradianceRT->GetWidth(), m_pIrradianceRT->GetHeight()),
                .g_DistanceTexSize = GetScreenSize(m_pDistanceRT->GetWidth(), m_pDistanceRT->GetHeight()),
                .g_RandomRotation = m_RandomRotation,

                .g_RayDataBufferIndex = m_pRayDataBuffer->GetUAVResourceHeapIndex(),
                .g_IrradianceTexIndex = m_pIrradianceRT->GetUAVResourceHeapIndex(),
                .g_DistanceTexIndex = m_pDistanceRT->GetUAVResourceHeapIndex(),
                .g_ProbeOffsetIndexTexIndex = m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),

                .g_PreReservoirBufferIndex = m_pPreReservoirBuffer->GetUAVResourceHeapIndex(),
                .g_CurrReservoirBufferIndex = m_pCurrReservoirBuffer->GetUAVResourceHeapIndex(),
                .g_ProbeNormalBias = UserData::GetInstance().GIParameter.normalBias,
                .g_ProbeViewBias = UserData::GetInstance().GIParameter.viewBias,

                .g_DDGIEncodingGamma = UserData::GetInstance().GIParameter.gamma,
            };

            UINT rootParameterIndex = 0;
            m_pCommand->GetCommandList()->SetComputeRoot32BitConstants(
                rootParameterIndex ++,
                constantSize,
                &constantData,
                0);
            m_pCommand->GetCommandList()->
                        SetComputeRootConstantBufferView(rootParameterIndex ++,
                                                         RenderResource::GetInstance().
                                                         GetPerFrameBindResourceSpace(
                                                             m_pDevice->GetFrameID())->
                                                         GetDynamicCBV());

            m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(
                rootParameterIndex ++,
                m_pTLASBuffer->GetGPUAddress());
            m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(
                rootParameterIndex ++,
                m_pInstanceDataBuffer->GetGPUAddress());
            // m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(
            //     rootParameterIndex ++,
            //     m_pProbeOffsetBuffer->GetGPUAddress());
            m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(
                rootParameterIndex ++,
                m_pProbeStateBuffer->GetGPUAddress());
            m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(
                rootParameterIndex ++,
                m_pProbeRelocationLUTBuffer->GetGPUAddress());
            m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(
                rootParameterIndex ++,
                m_pGIDataBuffer->GetGPUAddress());

            D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
            dispatchDesc.Width = Probe_Count;
            dispatchDesc.Height = Rays_Per_Probe;
            dispatchDesc.Depth = 1;
            dispatchDesc.RayGenerationShaderRecord = m_stbHelper.GetRayGenRange();
            dispatchDesc.MissShaderTable = m_stbHelper.GetMissRange();
            dispatchDesc.HitGroupTable = m_stbHelper.GetHitGroupRange();
            m_pCommand->GetCommandList()->DispatchRays(&dispatchDesc);
            m_pCommand->AddUAVBarrier(m_pRayDataBuffer, false);
            m_pCommand->AddUAVBarrier(m_pCurrReservoirBuffer, false);
        }
        m_pCommand->AddBarrier(*m_pCurrReservoirBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                               false);
        m_pCommand->AddBarrier(*m_pRayDataBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "Generate Ray");
    }
    void GIPass::ResetCounterBuffer()
    {
        auto passID = PassID(RESET_COUNTER);
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = m_PassData[passID].Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_GlobalCounterBufferIndex,
                                 m_pCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->Dispatch(1, 1, 1);
            m_pCommand->AddUAVBarrier(m_pCounterBuffer, false);
        }
        m_pCommand->AddBarrier(*m_pCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::CalcCompactedRay()
    {
        auto passID = PassID(RAY_COMPACTION);
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = m_PassData[passID].Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pCompactedRayDataBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_SourceRayDataBufferIndex,
                                 m_pRayDataBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_CompactedRayBufferIndex,
                                 m_pCompactedRayDataBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_CompactedIndicesBufferIndex,
                                 m_pCompactedRayIndexBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GlobalCounterBufferIndex,
                                 m_pCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_IndirectArgsBufferIndex,
                                 m_pIndirectArgsBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeStatesIndex,
                                 m_pProbeStateBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GIDataBufferIndex,
                                 m_pGIDataBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetIndexTexIndex,
                                 m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_RelocationLUTIndex,
                                 m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridOrigin,
                                   Vector4(m_gridOrigin.x, m_gridOrigin.y, m_gridOrigin.z, 0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridSpacing,
                                   Vector4(m_gridSpacing.x, m_gridSpacing.y, m_gridSpacing.z, 0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridDimensions,
                                   Vector4(Grid_Dimensions.x,
                                           Grid_Dimensions.y,
                                           Grid_Dimensions.z,
                                           0.f),
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(Probe_Count * Rays_Per_Probe, threadGroupSize.x),
                                 threadGroupSize.y,
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pCompactedRayDataBuffer, false);
            m_pCommand->AddUAVBarrier(m_pCounterBuffer, false);
        }
        m_pCommand->AddBarrier(*m_pCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(*m_pCompactedRayDataBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::CalcIndirectArgs()
    {
        auto passID = PassID(CALC_INDIRECT_ARGS);
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = m_PassData[passID].Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pIndirectArgsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_GlobalCounterBufferIndex,
                                 m_pCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_IndirectArgsBufferIndex,
                                 m_pIndirectArgsBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->Dispatch(1, 1, 1);
            m_pCommand->AddUAVBarrier(m_pIndirectArgsBuffer, false);
        }
        m_pCommand->AddBarrier(*m_pIndirectArgsBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::CalcIrradiance()
    {
        auto passID = PassID(DDGI_SHADING);
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = m_PassData[passID].Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pIndirectArgsBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, false);
        m_pCommand->AddBarrier(*m_pGIDataBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                   passID);

            m_pMaterial->SetFloat4(ShaderIDs::g_GridOrigin,
                                   Vector4(m_gridOrigin.x, m_gridOrigin.y, m_gridOrigin.z, 0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridSpacing,
                                   Vector4(m_gridSpacing.x, m_gridSpacing.y, m_gridSpacing.z, 0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridDimensions,
                                   Vector4(Grid_Dimensions.x,
                                           Grid_Dimensions.y,
                                           Grid_Dimensions.z,
                                           0.f),
                                   passID);

            m_pMaterial->SetFloat4(ShaderIDs::g_IrradianceTexSize,
                                   GetScreenSize(m_pIrradianceRT->GetWidth(), m_pIrradianceRT->GetHeight()),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_DistanceTexSize,
                                   GetScreenSize(m_pDistanceRT->GetWidth(), m_pDistanceRT->GetHeight()),
                                   passID);

            m_pMaterial->SetUInt(ShaderIDs::g_RayDataBufferIndex,
                                 m_pRayDataBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeStatesIndex,
                                 m_pProbeStateBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetIndexTexIndex,
                                 m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_RelocationLUTIndex,
                                 m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_IrradianceTexIndex,
                                 m_pIrradianceRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DistanceTexIndex,
                                 m_pDistanceRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GIDataBufferIndex,
                                 m_pGIDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_CompactedRayBufferIndex,
                                 m_pCompactedRayDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_CompactedIndicesBufferIndex,
                                 m_pCompactedRayIndexBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GlobalCounterBufferIndex,
                                 m_pCounterBuffer->GetResourceHeapIndex(),
                                 passID);

            m_pMaterial->SetFloat(ShaderIDs::g_ProbeNormalBias, UserData::GetInstance().GIParameter.normalBias, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_ProbeViewBias, UserData::GetInstance().GIParameter.viewBias, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_DDGIEncodingGamma,
                                  UserData::GetInstance().GIParameter.gamma,
                                  passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->GetCommandList()->SetComputeRootShaderResourceView(2, m_pTLASBuffer->GetGPUAddress());

            m_pCommand->GetCommandList()->ExecuteIndirect(m_pCommandSignature,
                                                          1,
                                                          m_pIndirectArgsBuffer->GetResource(),
                                                          0,
                                                          nullptr,
                                                          0
                );
            // auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            // m_pCommand->Dispatch(CeilDivide(Probe_Count * Rays_Per_Probe, threadGroupSize.x),
            //                      threadGroupSize.y,
            //                      threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pGIDataBuffer, false);
        }
        m_pCommand->AddBarrier(*m_pGIDataBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    void GIPass::ClearProbeOffset()
    {
        // static bool hasClear = false;
        //
        // auto passID = CLEAR_PROBE_OFFSET_PASS;
        // auto passName = m_PassData[passID].Name.c_str();
        // auto& passData = m_pMaterial->GetPassData(passID);
        // PIXHelper pix(m_pCommand->GetCommandList(), passName);
        //
        // PipelineInfo pipelineStateData{};
        // pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
        //     passID).pPipelineStateObject;
        // m_pCommand->SetPipeline(pipelineStateData);
        //
        // m_pCommand->AddBarrier(*m_pProbeOffsetBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        // {
        //     m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetsIndex,
        //                          m_pProbeOffsetBuffer->GetResourceHeapIndex());
        //     SetSpaceResource(passData, PER_PASS_SPACE);
        //
        //     auto threadGroupSize = passData.GetKernelThreadGroupSizes();
        //     m_pCommand->Dispatch(CeilDivide(Probe_Count, threadGroupSize.x),
        //                          threadGroupSize.y,
        //                          threadGroupSize.z);
        // }
        // m_pCommand->AddBarrier(*m_pProbeOffsetBuffer,
        //                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        //
        // hasClear = true;
        // m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::RelocateProbes()
    {
        auto passID = RELOCATE_PROBES_PASS;
        auto passName = m_PassData[passID].Name.c_str();
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        // m_pCommand->AddBarrier(*m_pProbeOffsetBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(m_pProbeOffsetIndexRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {

            // m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetsIndex,
            //                      m_pProbeOffsetBuffer->GetUAVResourceHeapIndex(),
            //                      passID);
            m_pMaterial->SetUInt(ShaderIDs::g_RayDataBufferIndex,
                                 m_pRayDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeStatesIndex,
                                 m_pProbeStateBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_ProbeOffsetIndexTexIndex,
                                 m_pProbeOffsetIndexRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_RelocationLUTIndex,
                                 m_pProbeRelocationLUTBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GIDataBufferIndex,
                                 m_pGIDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_RandomRotation,
                                   m_RandomRotation,
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridSpacing,
                                   Vector4(m_gridSpacing.x, m_gridSpacing.y, m_gridSpacing.z, 0.f),
                                   passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            // m_pCommand->Dispatch(CeilDivide(Probe_Count, threadGroupSize.x),
            //                      threadGroupSize.y,
            //                      threadGroupSize.z);
            m_pCommand->Dispatch(CeilDivide(m_pProbeOffsetIndexRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pProbeOffsetIndexRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(m_pProbeOffsetIndexRT, false);
        }
        m_pCommand->AddBarrier(m_pProbeOffsetIndexRT,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::ProbeBlend()
    {
        ProbeBlendIrradiance();
        ProbeBlendDepth();
    }
    void GIPass::ProbeBlendIrradiance()
    {
        auto passID = PROBE_IRRADIANCE_BLENDING;
        auto passName = m_PassData[passID].Name.c_str();
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pIrradianceRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_RayDataBufferIndex,
                                 m_pRayDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_IrradianceTexIndex,
                                 m_pIrradianceRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DistanceTexIndex,
                                 m_pDistanceRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GIDataBufferIndex,
                                 m_pGIDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_RandomRotation, m_RandomRotation, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_DDGIBlendWeight,
                                  UserData::GetInstance().GIParameter.blendWeight,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_ProbeIrradianceThreshold,
                                  UserData::GetInstance().GIParameter.probeIrradianceThreshold,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_ProbeBrightnessThreshold,
                                  UserData::GetInstance().GIParameter.probeBrightnessThreshold,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_DDGIEncodingGamma,
                                  UserData::GetInstance().GIParameter.gamma,
                                  passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridSpacing,
                                   Vector4(m_gridSpacing.x,
                                           m_gridSpacing.y,
                                           m_gridSpacing.z,
                                           0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridDimensions,
                                   Vector4(Grid_Dimensions.x,
                                           Grid_Dimensions.y,
                                           Grid_Dimensions.z,
                                           0.f),
                                   passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->Dispatch(Grid_Dimensions.x,
                                 Grid_Dimensions.y * Grid_Dimensions.z,
                                 1);
            m_pCommand->AddUAVBarrier(m_pIrradianceRT, false);
        }
        m_pCommand->AddBarrier(m_pIrradianceRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
    void GIPass::ProbeBlendDepth()
    {
        auto passID = PROBE_DEPTH_BLENDING;
        auto passName = m_PassData[passID].Name.c_str();
        auto& passData = m_pMaterial->GetPassData(passID);
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(m_pDistanceRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUInt(ShaderIDs::g_RayDataBufferIndex,
                                 m_pRayDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_IrradianceTexIndex,
                                 m_pIrradianceRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_DistanceTexIndex,
                                 m_pDistanceRT->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUInt(ShaderIDs::g_GIDataBufferIndex,
                                 m_pGIDataBuffer->GetResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_RandomRotation, m_RandomRotation, passID);
            m_pMaterial->SetFloat(ShaderIDs::g_DDGIBlendWeight,
                                  UserData::GetInstance().GIParameter.blendWeight,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_ProbeIrradianceThreshold,
                                  UserData::GetInstance().GIParameter.probeIrradianceThreshold,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_ProbeBrightnessThreshold,
                                  UserData::GetInstance().GIParameter.probeBrightnessThreshold,
                                  passID);
            m_pMaterial->SetFloat(ShaderIDs::g_DDGIEncodingGamma,
                                  UserData::GetInstance().GIParameter.gamma,
                                  passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridSpacing,
                                   Vector4(m_gridSpacing.x,
                                           m_gridSpacing.y,
                                           m_gridSpacing.z,
                                           0.f),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_GridDimensions,
                                   Vector4(Grid_Dimensions.x,
                                           Grid_Dimensions.y,
                                           Grid_Dimensions.z,
                                           0.f),
                                   passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->Dispatch(Grid_Dimensions.x,
                                 Grid_Dimensions.y * Grid_Dimensions.z,
                                 1);
            m_pCommand->AddUAVBarrier(m_pDistanceRT, false);
        }
        m_pCommand->AddBarrier(m_pDistanceRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }

    void GIPass::GenerateTLAS(const std::vector<std::unique_ptr<Entity>>& entityies)
    {
        UINT64 entityCount = entityies.size();
        if (!entityCount)
            return;

        // 定义实例描述符 (Instance Desc)
        std::vector<std::string> instanceNames(entityCount);
        std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(entityCount);
        uint32_t currentHitGroupOffset = 0;
        for (UINT64 i = 0; i < entityCount; ++i)
        {
            const auto& entity = entityies[i];
            instanceNames[i] = std::string(entity->name.c_str());
            instanceDescs[i].InstanceMask = 0xFF;                                         // 与 TraceRay 的 mask 匹配
            instanceDescs[i].InstanceID = currentHitGroupOffset;                          // 对应 HLSL 中的 InstanceID()
            instanceDescs[i].InstanceContributionToHitGroupIndex = currentHitGroupOffset; // 对应 HitGroup 偏移
            instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

            auto entityWorld_M = entity->transform.GetWorldMatrix();
            for (int row = 0; row < 3; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    instanceDescs[i].Transform[row][col] = entityWorld_M.m[row][col];
                }
            }
            // 关联BLAS
            instanceDescs[i].AccelerationStructure = entity->GetBLASBuffer()->GetGPUAddress();

            currentHitGroupOffset += entity->pMeshRenderer->m_pModel->meshes.size();
        }

        size_t bufferSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * entityCount;

        // 创建并填充 Instance Upload Buffer
        if (!m_pTLASUploadBuffer || m_pTLASUploadBuffer->GetResourceDesc().Width < bufferSize)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pTLASUploadBuffer);
            m_pTLASUploadBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"DXR TLAS Instance Upload Buffer",
                .stride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
                .size = bufferSize,
                .viewFlags = GPUResourceFlags::None,
                .accessFlags = BufferAccessFlags::HostWritable,
                .isRawAccess = false,
                .InitData = instanceDescs.data(),
            });
        }
        else
        {
            // 如果 Buffer 已经够大，直接更新内容即可（Map/Unmap 或直接使用之前映射过的指针）
            memcpy(m_pTLASUploadBuffer->GetMappedBuffer(), instanceDescs.data(), bufferSize);
        }

        // 获取 TLAS Prebuild Info
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = UINT(entityCount),
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .InstanceDescs = m_pTLASUploadBuffer->GetGPUAddress()
        };
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
        m_pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

        // 分配 TLAS 结果和 Scratch 空间
        if (!m_pTLASBuffer || m_pTLASBuffer->GetResourceDesc().Width < prebuildInfo.
            ResultDataMaxSizeInBytes)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pTLASBuffer);
            m_pTLASBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"DXR TLAS Result Buffer",
                .size = prebuildInfo.ResultDataMaxSizeInBytes,
                .viewFlags = GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
                .isRawAccess = true,
                .isAccelerationStructure = true
            });
        }

        if (!m_pTLASScratchBuffer || m_pTLASScratchBuffer->GetResourceDesc().Width < prebuildInfo.
            ScratchDataSizeInBytes)
        {
            BufferManager::GetInstance().DestoryBuffer(m_pTLASScratchBuffer);
            m_pTLASScratchBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"DXR TLAS Scratch Buffer",
                .size = prebuildInfo.ScratchDataSizeInBytes,
                .viewFlags = GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
                .isRawAccess = true
            });
        }

        m_pCommand->AddBarrier(*m_pTLASScratchBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddUAVBarrier(m_pTLASScratchBuffer);

        // Build
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc =
        {
            .DestAccelerationStructureData = m_pTLASBuffer->GetGPUAddress(),
            .Inputs = buildInputs,
            .ScratchAccelerationStructureData = m_pTLASScratchBuffer->GetGPUAddress(),
        };
        m_pCommand->GetCommandList()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        m_pCommand->AddUAVBarrier(m_pTLASBuffer);

        //DebugDumpTLASInstances(instanceDescs, instanceNames);
    }

    CComPtr<IDxcBlob> GIPass::CompileRaytracingLibrary(const std::wstring& fileName)
    {
        m_tempStrings.clear();
        m_tempStrings.reserve(20);
        CComPtr<IDxcUtils> pUtils;
        CComPtr<IDxcCompiler3> pCompiler;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

        CComPtr<IDxcIncludeHandler> pIncludeHandler;
        ThrowIfFailed(pUtils->CreateDefaultIncludeHandler(&pIncludeHandler));

        WCHAR assetsPath[512];
        ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
        auto path = ElysiaHelper::GetAssetFullPath(assetsPath, fileName.c_str());

        // 1. 读取文件内容
        CComPtr<IDxcBlobEncoding> pSource;
        ThrowIfFailed(pUtils->LoadFile(path.c_str(), nullptr, &pSource));

        auto shaderDir = std::filesystem::path(assetsPath).wstring();
        shaderDir += L"\\Shaders";

        // 2. 设置编译参数
        // DXR 必须使用 lib_6_x profile
        std::vector<LPCWSTR> arguments;
        arguments.push_back(L"-Od");
        arguments.push_back(L"-Zi");
        arguments.push_back(L"-Qembed_debug");
        arguments.push_back(L"-Qstrip_reflect");
        arguments.push_back(L"-T");
        arguments.push_back(L"lib_6_6");
        auto addInclude = [&](const std::wstring& relPath)
        {
            m_tempStrings.emplace_back(L"-I" + shaderDir + relPath);
            arguments.push_back(m_tempStrings.back().c_str());
        };
        addInclude(L"");
        addInclude(L"\\public");
        addInclude(L"\\private");
        arguments.push_back(L"-Fd");
        arguments.push_back(L".\\");
        arguments.push_back(L"-Zpr"); // 强制序数排列 (可选)
        arguments.push_back(L"-Zss"); // 建立源码摘要
        // arguments.push_back(L"-fspv-extension=SPV_KHR_non_semantic_info");

        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = pSource->GetBufferPointer();
        sourceBuffer.Size = pSource->GetBufferSize();
        sourceBuffer.Encoding = DXC_CP_UTF8;

        // 3. 执行编译
        CComPtr<IDxcResult> pResults;
        auto hr = (pCompiler->Compile(&sourceBuffer,
                                      arguments.data(),
                                      (uint32_t)arguments.size(),
                                      pIncludeHandler,
                                      IID_PPV_ARGS(&pResults)));
        if (FAILED(hr))
        {
            std::wstring hrmsg = FormatHrMessage(hr);
            std::wostringstream oss;
            oss << L"HRESULT: 0x" << std::hex << hr << L" (" << hrmsg << L")\n";
            // also try to get any error text returned in pResults (sometimes present even if Compile returned failure)
            if (pResults)
            {
                CComPtr<IDxcBlobUtf8> pErrors;
                if (SUCCEEDED(
                    pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr)) && pErrors
                    && pErrors->GetStringLength() > 0)
                {
                    oss << L"Compiler errors/warnings:\n" << (const char*)pErrors->
                        GetStringPointer() << L"\n";
                }
            }
            ThrowRuntimeError(
                std::string("DXC compile call failed: ") + WstringToString(oss.str()));
        }

        // 4. 检查错误
        CComPtr<IDxcBlobUtf8> pErrors;
        pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (pErrors != nullptr && pErrors->GetStringLength() != 0)
            wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

        HRESULT hrStatus;
        pResults->GetStatus(&hrStatus);
        if (FAILED(hrStatus))
        {
            wprintf(L"Compilation Failed\n");
        }

        CComPtr<IDxcBlob> pShader = nullptr;
        CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
        pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr);
        if (pShader != nullptr)
        {
            FILE* fp = NULL;

            /*_wfopen_s(&fp, pShaderName->GetStringPointer(), L"wb");
            fwrite(pShader->GetBufferPointer(), pShader->GetBufferSize(), 1, fp);
            fclose(fp);*/

            std::cout << "Shader compiled successfully. Size: " << pShader->GetBufferSize() <<
                " bytes" << std::endl;
            std::cout << "Shader compiled successfully. Adress: " << pShader->GetBufferPointer() <<
                std::endl;
        }

        //
        // Save pdb.
        //
        CComPtr<IDxcBlob> pPDB = nullptr;
        CComPtr<IDxcBlobUtf16> pPDBName = nullptr;
        pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
        if (pPDB != nullptr && pPDBName != nullptr)
        {
            std::wstring pdbPath = pPDBName->GetStringPointer();
            std::wstring fullPdbPath = assetsPath + pdbPath;

            // 如果路径包含文件夹，确保文件夹存在
            FILE* fp = NULL;
            _wfopen_s(&fp, fullPdbPath.c_str(), L"wb");
            if (fp)
            {
                fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
                fclose(fp);

                // 打印一下路径，确认它是不是在 .exe 旁边
                std::wcout << L"PDB saved to: " << fullPdbPath << std::endl;
            }
        }

        CComPtr<IDxcBlob> pHash = nullptr;
        pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
        if (pHash != nullptr && pHash->GetBufferSize() >= 16)
        {
            wprintf(L"Hash: ");
            DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
            for (int i = 0; i < _countof(pHashBuf->HashDigest); i ++)
                wprintf(L"%.2x", pHashBuf->HashDigest[i]);
            wprintf(L"\n");
        }

        CComPtr<IDxcBlob> pHashDigestBlob = nullptr;
        CComPtr<IDxcBlob> pDebugDxilContainer = nullptr;
        if (SUCCEEDED(pUtils->GetPDBContents(pPDB, &pHashDigestBlob, &pDebugDxilContainer)))
        {
            // This API returns the raw hash digest, rather than a DxcShaderHash structure.
            // This will be the same as the DxcShaderHash::HashDigest returned from
            // IDxcResult::GetOutput(DXC_OUT_SHADER_HASH, ...).
            wprintf(L"Hash from PDB: ");
            const BYTE* pHashDigest = (const BYTE*)pHashDigestBlob->GetBufferPointer();
            assert(pHashDigestBlob->GetBufferSize() == 16); // hash digest is always 16 bytes.
            for (int i = 0; i < pHashDigestBlob->GetBufferSize(); i ++)
                wprintf(L"%.2x", pHashDigest[i]);
            wprintf(L"\n");

            // The pDebugDxilContainer blob will contain a DxilContainer formatted
            // binary, but with different parts than the pShader blob retrieved
            // earlier.
            // The parts in this container will vary depending on debug options and
            // the compiler version.
            // This blob is not meant to be directly interpreted by an application.
        }

        assert(pShader);
        return pShader;
    }
    void GIPass::CreateRaytracingPipeline(ID3D12RootSignature* pRootSignature,
                                          const std::vector<std::unique_ptr<Entity>>& entities)
    {
        CD3DX12_STATE_OBJECT_DESC pipelineDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

        D3D12_SHADER_BYTECODE rayGenBytecode =
        {
            .pShaderBytecode = m_DXRBlob->GetBufferPointer(),
            .BytecodeLength = m_DXRBlob->GetBufferSize()
        };

        auto lib = pipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libBytecode = rayGenBytecode;
        lib->SetDXILLibrary(&libBytecode);
        lib->DefineExport(L"GenerateRayMain");
        lib->DefineExport(L"RayMiss");
        lib->DefineExport(L"RayClosestHit");
        // lib->DefineExport(L"ShadowMiss");

        // 创建 Hit Group 子对象
        auto opaqueHitGroup = pipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        // 必须匹配 HLSL 中 [shader("closesthit")] 标注的函数名
        opaqueHitGroup->SetClosestHitShaderImport(L"RayClosestHit");
        // 给这个 Hit Group 取一个名字，后续用于 GetShaderIdentifier
        opaqueHitGroup->SetHitGroupExport(L"OpaqueHitGroup");
        // 指定几何体类型为三角形
        opaqueHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

        auto shaderConfig = pipelineDesc.CreateSubobject<
            CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        uint32_t maxPayloadSize = 32;
        uint32_t maxAttributeSize = 8; // float2 barycentrics
        shaderConfig->Config(maxPayloadSize, maxAttributeSize);

        auto pipelineConfig = pipelineDesc.CreateSubobject<
            CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        pipelineConfig->Config(2);

        auto globalRootSig = pipelineDesc.CreateSubobject<
            CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSig->SetRootSignature(pRootSignature);

        auto hr = m_pDevice5->CreateStateObject(pipelineDesc, IID_PPV_ARGS(&m_pRTPSO));
        if (SUCCEEDED(hr))
        {
            // 构建成功后，提取 Shader ID 用于SBTHelper
            CComPtr<ID3D12StateObjectProperties> pRTProps;
            m_pRTPSO->QueryInterface(IID_PPV_ARGS(&pRTProps));

            m_stbHelper.AddRayGen(pRTProps->GetShaderIdentifier(L"GenerateRayMain"));

            m_stbHelper.AddMiss(pRTProps->GetShaderIdentifier(L"RayMiss"));
            // m_stbHelper.AddMiss(pRTProps->GetShaderIdentifier(L"ShadowMiss"));

            for (auto& entity : entities)
            {
                auto& pModel = entity->pMeshRenderer->m_pModel;
                for (size_t meshIndex = 0; meshIndex < pModel->meshes.size(); meshIndex ++)
                {
                    m_stbHelper.AddHitGroup(pRTProps->GetShaderIdentifier(L"OpaqueHitGroup"));
                }
            }
        }
    }
    void GIPass::CreateDXRRootSignature(ID3D12Device* pDevice)
    {
        // 1. 定义根参数：对于 Bindless 方案，我们通常只需要“根常量 (Root Constants)”
        // 用来传递诸如 g_RayDataUAVIndex 或 DDGI 配置结构体的索引
        CD3DX12_ROOT_PARAMETER1 rootParameters[7];

        // 假设我们需要 16 个 32位常量 (比如一个 ViewProj 矩阵或一组索引)
        UINT rootParameterIndex = 0;
        rootParameters[rootParameterIndex ++].InitAsConstants(48, 0, 2);
        rootParameters[rootParameterIndex ++].InitAsConstantBufferView(0, PER_FRAME_SPACE);

        UINT SRVIndex = 0;
        rootParameters[rootParameterIndex ++].InitAsShaderResourceView(SRVIndex ++, 0);
        rootParameters[rootParameterIndex ++].InitAsShaderResourceView(SRVIndex ++, 0);
        rootParameters[rootParameterIndex ++].InitAsShaderResourceView(SRVIndex ++, 0);
        rootParameters[rootParameterIndex ++].InitAsShaderResourceView(SRVIndex ++, 0);
        rootParameters[rootParameterIndex ++].InitAsShaderResourceView(SRVIndex ++, 0);

        auto samplerDescs = GenerateSampler();
        CD3DX12_STATIC_SAMPLER_DESC staticSamplers[NUM_SAMPLER_DESCRIPTORS];
        for (UINT i = 0; i < NUM_SAMPLER_DESCRIPTORS; ++i)
        {
            // CD3DX12_STATIC_SAMPLER_DESC 的构造函数支持直接传入常见的参数
            staticSamplers[i].Init(
                i,
                // shaderRegister: 对应 HLSL 中的 s0, s1, s2...
                samplerDescs[i].Filter,
                samplerDescs[i].AddressU,
                samplerDescs[i].AddressV,
                samplerDescs[i].AddressW,
                samplerDescs[i].MipLODBias,
                samplerDescs[i].MaxAnisotropy,
                samplerDescs[i].ComparisonFunc,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                // 静态采样器对边界色有特殊枚举限制
                samplerDescs[i].MinLOD,
                samplerDescs[i].MaxLOD,
                D3D12_SHADER_VISIBILITY_ALL // DXR 通常使用 ALL
                );
        }

        // 2. 核心标志位：必须包含直接索引堆的标志
        D3D12_ROOT_SIGNATURE_FLAGS rsFlags =
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
        rsDesc.Init_1_1(_countof(rootParameters),
                        rootParameters,
                        NUM_SAMPLER_DESCRIPTORS,
                        staticSamplers,
                        rsFlags);

        // 3. 序列化并创建
        CComPtr<ID3DBlob> signature;
        CComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&rsDesc, &signature, &error);

        if (FAILED(hr))
        {
            if (error)
            {
                OutputDebugStringA((char*)error->GetBufferPointer());
            }
            return;
        }

        ElysiaHelper::ThrowIfFailed(pDevice->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_pGlobalRootSig)));
    }
    std::vector<D3D12_SAMPLER_DESC> GIPass::GenerateSampler()
    {
        std::vector<D3D12_SAMPLER_DESC> samplerDescs(NUM_SAMPLER_DESCRIPTORS);
        for (size_t i = 0; i < NUM_SAMPLER_DESCRIPTORS; ++i)
        {
            samplerDescs[0].BorderColor[0] =
                samplerDescs[0].BorderColor[1] =
                samplerDescs[0].BorderColor[2] = samplerDescs[0].BorderColor[3] = 0.0f;
            samplerDescs[i].MipLODBias = 0;
            samplerDescs[i].MaxAnisotropy = 16;
            samplerDescs[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
            samplerDescs[i].MinLOD = 0;
            samplerDescs[i].MaxLOD = D3D12_FLOAT32_MAX;
        }
        UINT samplerIndex = 0;
        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        //rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
        samplerIndex ++;

        return samplerDescs;
    }

    void GIPass::ComputeRandomRotation()
    {
        static std::mt19937 gen(std::random_device{}());
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        auto GetRandomFloat = [&]()
        {
            return dist(gen);
        };

        // 基于 James Arvo 的算法生成 3 个均匀分布的随机变量 
        float u1 = 2.0f * 3.14159265f * GetRandomFloat();
        float u2 = 2.0f * 3.14159265f * GetRandomFloat();
        float u3 = GetRandomFloat();

        float cos1 = cosf(u1);
        float sin1 = sinf(u1);
        float cos2 = cosf(u2);
        float sin2 = sinf(u2);

        float sq3 = 2.f * sqrtf(u3 * (1.f - u3));
        float s2 = 2.f * u3 * sin2 * sin2 - 1.f;
        float c2 = 2.f * u3 * cos2 * cos2 - 1.f;
        float sc = 2.f * u3 * sin2 * cos2;

        // 随机旋转矩阵
        Matrix rotationMatrix = Matrix::Identity;

        rotationMatrix._11 = cos1 * c2 - sin1 * sc;
        rotationMatrix._12 = sin1 * c2 + cos1 * sc;
        rotationMatrix._13 = sq3 * cos2;

        rotationMatrix._21 = cos1 * sc - sin1 * s2;
        rotationMatrix._22 = sin1 * sc + cos1 * s2;
        rotationMatrix._23 = sq3 * sin2;

        rotationMatrix._31 = cos1 * (sq3 * cos2) - sin1 * (sq3 * sin2);
        rotationMatrix._32 = sin1 * (sq3 * cos2) + cos1 * (sq3 * sin2);
        rotationMatrix._33 = 1.f - 2.f * u3;

        m_RandomRotation = Quaternion::CreateFromRotationMatrix(rotationMatrix);
    }

    Vector3 GetFibonacciDir(int i)
    {
        int dirIndex = i % 64;
        float b = (sqrt(5.0f) * 0.5f + 0.5f) - 1.0f;
        float phi = 2.0f * 3.14159265f * b;
        float theta = phi * dirIndex;
        float cosPhi = 1.0f - (float(dirIndex) + 0.5f) / 64.0f * 2.0f;
        float sinPhi = sqrt(std::max(0.0f, 1.0f - cosPhi * cosPhi));
        Vector3 dir(cos(theta) * sinPhi, cosPhi, sin(theta) * sinPhi);

        return dir;
    }
    float GetDistScale(int i)
    {
        // 分配 4 种不同的距离权重
        int distStep = i / 64; // 0, 1, 2, 3
        float distScale = (distStep + 1) / 4.0f;

        return distScale;
    }
    std::vector<Vector4> GenerateRelocationLUT(Vector3 probeSpacing)
    {
        std::vector<Vector4> lut(256);
        const auto maxOffset = probeSpacing * 0.45f; // 限制在半个网格内，防止越界 

        // 第一项通常设为 (0,0,0)，代表“不偏移” 
        lut[0] = Vector4(0, 0, 0, 0);

        for (int i = 1; i < 256; ++i)
        {
            // 使用斐波那契球面算法生成 64 个方向 
            auto dir = GetFibonacciDir(i);
            float distScale = GetDistScale(i);

            Vector3 finalOffset = dir * maxOffset * distScale;

            lut[i] = Vector4(finalOffset.x, finalOffset.y, finalOffset.z, 0.0f);
        }
        return lut;
    }
}