#include "stdafx.h"
#include "Entity.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/RenderCore/MeshRenderer.h"

namespace ElysiaEngine
{
    Entity::Entity(eastl::string n)
        : name(std::move(n))
    {

    }

    Entity::~Entity()
    {

    }

    BoundingBox Entity::LocalAABB() const noexcept
    {
        return pMeshRenderer->GetBoundingBox();
    }
    bool Entity::HasMeshRenderer() const noexcept
    {
        return pMeshRenderer != nullptr;
    }


    void Entity::AddChild(std::unique_ptr<Entity>&& pChild)
    {
        pChild->SetParent(this);
        m_childs.emplace_back(std::move(pChild));
    }

    void Entity::SetParent(Entity* pParent)
    {
        m_pParent = pParent;
    }

    void Entity::OnTransformChanged()
    {
        m_IsDirty = true;
        if (pAttachedCamera)
        {
            pAttachedCamera->m_transform = transform;

            auto fpCam = dynamic_cast<FirstPersonCamera*>(pAttachedCamera);
            if (fpCam)
            {
                fpCam->SyncFromTransform();
            }
            pAttachedCamera->UpdateViewMatrix();
            pAttachedCamera->UpdateFrustum();
        }
    }

    void Entity::UpdateWorldAABB()
    {
        auto world_M = transform.GetWorldMatrix();
        m_localAABB.Transform(m_worldAABB, world_M);
    }

    void Entity::GenerateBLAS(ID3D12Device5* pDevice, DX12GraphicsContext* pCommand)
    {
        if (m_pBLASBuffer && m_pBLASScratchBuffer || !pMeshRenderer)
            return;

        // auto mesh = pMeshRenderer->GetMesh();
        auto& model = pMeshRenderer->m_pModel;
        UINT64 numSubMeshes = model->meshes.size();
        if (numSubMeshes == 0)
            return;

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> tempGeometryDescs;
        tempGeometryDescs.reserve(numSubMeshes);

        for (const auto& subMesh : model->meshes)
        {
            D3D12_RAYTRACING_GEOMETRY_DESC gd = {};
            gd.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

            // 判定材质是否不透明 (Opaque)
            // Sponza 的旗帜等 Mask 材质不能带此 Flag，否则 AnyHit 不起作用
            auto& material = model->materials[subMesh.materialIndex];
            gd.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            // if (material.alpha == LoadedMaterial::Alpha::Opaque)
            // {
            //     gd.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            // }
            // else
            // {
            //     gd.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
            // }

            auto& triangles = gd.Triangles;
            auto rootVBView = subMesh.vbView;
            auto rootIBView = subMesh.ibView;

            // 直接使用 subMesh 在 InitCommon 中已经计算好的 GPU 地址
            triangles.VertexBuffer.StartAddress = rootVBView.BufferLocation;
            triangles.VertexBuffer.StrideInBytes = rootVBView.StrideInBytes;
            triangles.VertexCount = subMesh.numVertices;
            triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

            triangles.IndexBuffer = rootIBView.BufferLocation;
            triangles.IndexCount = subMesh.numIndices;
            triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

            triangles.Transform3x4 = 0; // 局部空间无需变换

            tempGeometryDescs.emplace_back(gd);
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs = {};
        buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        buildInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        buildInputs.NumDescs = static_cast<UINT>(tempGeometryDescs.size());
        buildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildInputs.pGeometryDescs = tempGeometryDescs.data();

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
        pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

        // allocate GPU Buffer
        if (m_pBLASBuffer)
            BufferManager::GetInstance().DestoryBuffer(m_pBLASBuffer);
        if (m_pBLASScratchBuffer)
            BufferManager::GetInstance().DestoryBuffer(m_pBLASScratchBuffer);
        m_pBLASBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"DXR BLAS Result Buffer",
            .stride = 0,
            .size = prebuildInfo.ResultDataMaxSizeInBytes,
            .viewFlags = GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = true,
            .isAccelerationStructure = true
        });
        m_pBLASScratchBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"DXR BLAS Scratch Buffer",
            .stride = 0,
            .size = prebuildInfo.ScratchDataSizeInBytes,
            .viewFlags = GPUResourceFlags::UAV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = true,
            .isAccelerationStructure = false
        });
        pCommand->AddBarrier(*m_pBLASScratchBuffer,
                             D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // Build
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc =
        {
            .DestAccelerationStructureData = m_pBLASBuffer->GetGPUAddress(),
            .Inputs = buildInputs,
            .SourceAccelerationStructureData = 0, // 仅在更新（Update）时使用
            .ScratchAccelerationStructureData = m_pBLASScratchBuffer->GetGPUAddress(),
        };

        pCommand->GetCommandList()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        pCommand->AddUAVBarrier(m_pBLASBuffer, false);
    }
}