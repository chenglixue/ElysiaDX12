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
        if (m_pBLASBuffer && m_pBLASScratchBuffer)
            return;

        auto mesh = pMeshRenderer->GetMesh();
        auto rootVBView = mesh.vbView;
        auto rootIBView = mesh.ibView;

        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        // describe vertex、index
        auto& triangles = geometryDesc.Triangles;
        triangles.VertexBuffer.StartAddress = rootVBView.BufferLocation;
        triangles.VertexBuffer.StrideInBytes = rootVBView.StrideInBytes;
        triangles.VertexCount = rootVBView.SizeInBytes / rootVBView.StrideInBytes;
        triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

        triangles.IndexBuffer = rootIBView.BufferLocation;
        triangles.IndexCount = rootIBView.SizeInBytes / ElysiaModel::IndexSize();
        triangles.IndexFormat = rootIBView.Format;

        triangles.Transform3x4 = 0;

        // Prebuild Info
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs =
        {
            .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
            .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
            .NumDescs = 1,
            .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
            .pGeometryDescs = &geometryDesc,
        };

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