#include "stdafx.h"
#include "MeshRenderer.h"

#include "Model/LoadedModel.h"
#include "Utility/SharedTypes.h"

namespace ElysiaRenderer
{
    void MeshRenderer::Init(const ElysiaModel::LoadedModel* loadedModel)
    {
        m_pModel = loadedModel;

        const UINT64 meshCount = m_pModel->meshes.size();
        m_meshBoundingBoxes.resize(meshCount);
        m_meshDrawIndices.resize(meshCount);
        
        for (UINT64 meshIndex = 0; meshIndex < meshCount; ++meshIndex)
        {
            const ElysiaModel::LoadedModel::Mesh& mesh = m_pModel->meshes[meshIndex];
            BoundingBox& boundingBox = m_meshBoundingBoxes[meshIndex];
            boundingBox.Extents = (mesh.aabbMax - mesh.aabbMin) * 0.5f;
            boundingBox.Center = mesh.aabbMin + boundingBox.Extents;
        }
        
        {
            const auto& materials = m_pModel->materials;
            const UINT64 materialCount = materials.size();
            eastl::vector<MaterialTextureIndices> 
        }
    }

}
