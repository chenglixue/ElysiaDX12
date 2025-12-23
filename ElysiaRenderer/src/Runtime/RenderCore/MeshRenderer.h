#pragma once

#include "Programs/Helper.h"
#include "Runtime/Model/SharedTypes.h"

namespace ElysiaModel
{
    struct LoadedModel;
}

namespace ElysiaRenderer
{
    class MeshRenderer
    {
    public:
        MeshRenderer() = default;

        void Init(const std::shared_ptr<ElysiaModel::LoadedModel>& loadedModel);
        void ShutDown();
        
        ElysiaModel::LoadedModel* GetModel() const {return m_pModel.get();}
        MaterialTextureIndices GetTextureIndices(UINT64 idx) const
        {
            assert(idx < m_textureIndices.size());
            return m_textureIndices[idx];
        }
        
    private:
        std::shared_ptr<ElysiaModel::LoadedModel> m_pModel = nullptr;
        eastl::vector<BoundingBox> m_meshBoundingBoxes;
        eastl::vector<UINT32> m_meshDrawIndices;
        eastl::vector<MaterialTextureIndices> m_textureIndices;
    };
}

