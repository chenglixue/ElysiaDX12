#pragma once

#include "Runtime/Resource/Model/LoadedModel.h"
#include "Runtime/Resource/Model/SharedTypes.h"

namespace ElysiaRenderer
{
    using namespace ElysiaModel;
    
    class MeshRenderer
    {
    public:
        MeshRenderer() = default;

        void Init(const std::shared_ptr<LoadedModel>& loadedModel, size_t meshIndex);
        void Update();
        void ShutDown();

        LoadedModel::Mesh& GetMesh() const {return m_pModel->meshes[m_meshIndex];}
        const LoadedMaterial& GetMaterial() const {return m_pModel->materials[m_materialIndex];}
        const BoundingBox& GetBoundingBox() const noexcept {return m_boundingBox;}
        const MaterialTextureIndices& GetTextureIndices() const noexcept {return m_materialTexIndices;}
        
    private:
        std::shared_ptr<LoadedModel> m_pModel = nullptr;
        UINT m_meshIndex;
        UINT m_materialIndex;
        BoundingBox m_boundingBox;
        MaterialTextureIndices m_materialTexIndices;

        void UpdateAABB();
    };
}

