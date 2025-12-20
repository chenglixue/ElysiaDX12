#pragma once

#include "lib/Utility/Helper.h"
#include "Utility/SharedTypes.h"

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

        void Init(const ElysiaModel::LoadedModel* loadedModel);
        void ShutDown();
        
    private:
        const ElysiaModel::LoadedModel* m_pModel = nullptr;
        eastl::vector<BoundingBox> m_meshBoundingBoxes;
        eastl::vector<UINT32> m_meshDrawIndices;
        eastl::vector<MaterialTextureIndices> m_textureIndices;
    };
}

