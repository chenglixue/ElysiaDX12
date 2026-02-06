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
        m_halfWidth = UINT(m_renderSize.x) >> 1;
        m_halfHeight = UINT(m_renderSize.y) >> 1;
        m_quarterWidth = UINT(m_renderSize.x) >> 2;
        m_quarterHeight = UINT(m_renderSize.y) >> 2;

        m_pRayDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Ray Data Buffer",
            .stride = sizeof(RayData),
            .size = sizeof(RayData) * Probe_Count * Rays_Per_Probe,
            .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false
        });

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

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);

        UpdatePipeline();
    }

    void GIPass::Render(FrameContext& context)
    {
        static bool isCalcSceneSize = false;
        if (!SceneManager::GetInstance().GetEntities().empty() && !isCalcSceneSize)
        {
            auto sceneAABB = SceneManager::GetInstance().GetEntities()[0]->GetWorldAABB();
            auto sceneMin = sceneAABB.Center - sceneAABB.Extents;
            auto sceneMax = sceneAABB.Center + sceneAABB.Extents;

            // 希望探针能稍微往里面缩一点，不要紧贴墙壁
            float padding = 0.5f;
            Vector3 effectiveMin = sceneMin + Vector3(padding, padding, padding);
            Vector3 effectiveMax = sceneMax - Vector3(padding, padding, padding);
            Vector3 effectiveSize = effectiveMax - effectiveMin;

            // 计算间距 (注意是 数量-1)
            float spacingX = effectiveSize.x / (16 - 1);
            float spacingY = effectiveSize.y / (4 - 1);
            float spacingZ = effectiveSize.z / (16 - 1);

            m_gridSpacing = Vector3(spacingX, spacingY, spacingZ);
            m_gridOrigin = effectiveMin;

            isCalcSceneSize = true;
        }
        if (!isCalcSceneSize)
            return;
        if (!m_vertexBuffer->GetIsReady() || !m_indexBuffer->GetIsReady())
            return;

        PIXHelper pix(m_pCommand->GetCommandList(), "GI Pass");
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), "GI Begin");

        GenerateRay();
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

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(
                    m_pDevice,
                    m_pMaterial.get(),
                    passID);
        }
    }

    void GIPass::GenerateRay()
    {
        auto passID = RAY_GENERATE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        // m_pCommand->AddBarrier(*m_pRayDataBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_pCommand->AddBarrier(m_pIrradianceRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        {
            m_pMaterial->SetFloat3(ShaderIDs::g_GridDimensions,
                                   Vector3(Grid_Dimensions.x,
                                           Grid_Dimensions.y,
                                           Grid_Dimensions.z));
            m_pMaterial->SetFloat3(ShaderIDs::g_GridOrigin, m_gridOrigin);
            m_pMaterial->SetFloat3(ShaderIDs::g_GridSpacing, m_gridSpacing);
            m_pMaterial->SetUInt(ShaderIDs::g_IrradianceTexIndex,
                                 m_pIrradianceRT->GetResourceHeapIndex());
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(1024,
                                 1,
                                 1);
        }

        // m_pCommand->AddBarrier(*m_pRayDataBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(m_pIrradianceRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
}