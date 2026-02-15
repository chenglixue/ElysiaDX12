#pragma once

#include "Runtime/Resource/Model/LoadedModel.h"
#include "Runtime/Resource/Model/SharedTypes.h"

namespace ElysiaRenderer
{
    using namespace ElysiaModel;

    class MeshRenderer
    {
    public:
        std::shared_ptr<LoadedModel> m_pModel = nullptr;

    public:
        MeshRenderer() = default;

        void Init(const std::shared_ptr<LoadedModel>& loadedModel, size_t meshIndex);
        void ShutDown();

        LoadedModel::Mesh& GetMesh() const
        {
            return m_pModel->meshes[m_meshIndex];
        }
        const LoadedMaterial& GetMaterial() const
        {
            return m_pModel->materials[m_materialIndex];
        }
        const BoundingBox& GetBoundingBox() const noexcept
        {
            return m_boundingBox;
        }
        const MaterialTextureIndices& GetTextureIndices() const noexcept
        {
            return m_materialTexIndices;
        }
        const MeshVertex* GetVertices() const
        {
            return m_pModel->vertices.data() + GetMesh().vtxOffset;
        }
        const UINT32* GetIndices() const
        {
            return m_pModel->indices.data() + GetMesh().idxOffset;
        }
        const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const
        {
            return m_pModel->meshes[m_meshIndex].vbView;
        }
        const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const
        {
            return m_pModel->meshes[m_meshIndex].ibView;
        }

    private:
        UINT m_meshIndex;
        UINT m_materialIndex;
        BoundingBox m_boundingBox;
        MaterialTextureIndices m_materialTexIndices;
    };
}