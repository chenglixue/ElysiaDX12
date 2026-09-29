#include "stdafx.h"
#include "../public/Entity.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/RenderCore/public/BufferManager.h"
#include "Runtime/RenderCore/public/MeshRenderer.h"

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
        if (m_pBLASBuffer || !pMeshRenderer || !pMeshRenderer->m_pModel)
            return;

        const auto& subMesh = pMeshRenderer->GetMesh();
        if (subMesh.numVertices == 0 || subMesh.numIndices == 0)
            return;

        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        auto& triangles = geometryDesc.Triangles;
        triangles.VertexBuffer.StartAddress = subMesh.vbView.BufferLocation;
        triangles.VertexBuffer.StrideInBytes = subMesh.vbView.StrideInBytes;
        triangles.VertexCount = subMesh.numVertices;
        triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        triangles.IndexBuffer = subMesh.ibView.BufferLocation;
        triangles.IndexCount = subMesh.numIndices;
        triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        triangles.Transform3x4 = 0;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS buildInputs = {};
        buildInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        buildInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        buildInputs.NumDescs = 1;
        buildInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildInputs.pGeometryDescs = &geometryDesc;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
        pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&buildInputs, &prebuildInfo);

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

        struct SharedScratch
        {
            BufferHandle buffer;
            UINT64 size = 0;
        };
        static SharedScratch scratch;
        if (!scratch.buffer || scratch.size < prebuildInfo.ScratchDataSizeInBytes)
        {
            if (scratch.buffer)
                BufferManager::GetInstance().DestoryBuffer(scratch.buffer);
            scratch.buffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"DXR BLAS Scratch Buffer",
                .stride = 0,
                .size = prebuildInfo.ScratchDataSizeInBytes,
                .viewFlags = GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
                .isRawAccess = true,
                .isAccelerationStructure = false
            });
            scratch.size = prebuildInfo.ScratchDataSizeInBytes;
            pCommand->AddBarrier(*scratch.buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        }
        else
        {
            pCommand->AddUAVBarrier(scratch.buffer, false);
        }

        auto vertexBuffer = BufferManager::GetInstance().GetGlobalVertexBuffer();
        auto indexBuffer = BufferManager::GetInstance().GetGlobalIndexBuffer();
        const D3D12_RESOURCE_STATES vertexState = vertexBuffer->GetUsageState();
        const D3D12_RESOURCE_STATES indexState = indexBuffer->GetUsageState();
        pCommand->AddBarrier(*vertexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        pCommand->AddBarrier(*indexBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        pCommand->FlushBarrier();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc =
        {
            .DestAccelerationStructureData = m_pBLASBuffer->GetGPUAddress(),
            .Inputs = buildInputs,
            .SourceAccelerationStructureData = 0,
            .ScratchAccelerationStructureData = scratch.buffer->GetGPUAddress(),
        };

        pCommand->GetCommandList()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        pCommand->AddBarrier(*vertexBuffer, vertexState, false);
        pCommand->AddBarrier(*indexBuffer, indexState, false);
        pCommand->AddUAVBarrier(m_pBLASBuffer, false);
        pCommand->FlushBarrier();
    }
}