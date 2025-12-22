#include "stdafx.h"
#include "MeshRenderer.h"

#include "Model/LoadedModel.h"

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
            m_textureIndices.resize(materialCount);
            for (UINT64 materialIndex = 0; materialIndex < materialCount; ++materialIndex)
            {
                const auto& material = materials[materialIndex];
                auto& materialIndices = m_textureIndices[materialIndex];
                
                materialIndices.Albedo = material.textures[UINT64(ElysiaModel::MaterialTextureType::Albedo)].GetResourceHeapIndex();
                materialIndices.Normal = material.textures[UINT64(ElysiaModel::MaterialTextureType::Normal)].GetResourceHeapIndex();
                materialIndices.Metallic = material.textures[UINT64(ElysiaModel::MaterialTextureType::Metallic)].GetResourceHeapIndex();
                materialIndices.Roughness = material.textures[UINT64(ElysiaModel::MaterialTextureType::Roughness)].GetResourceHeapIndex();
                materialIndices.Occlusion = material.textures[UINT64(ElysiaModel::MaterialTextureType::Occlusion)].GetResourceHeapIndex();
                materialIndices.Specular = material.textures[UINT64(ElysiaModel::MaterialTextureType::Specular)].GetResourceHeapIndex();
                materialIndices.Height = material.textures[UINT64(ElysiaModel::MaterialTextureType::Height)].GetResourceHeapIndex();
                materialIndices.Emissive = material.textures[UINT64(ElysiaModel::MaterialTextureType::Emissive)].GetResourceHeapIndex();
            }
        }
    }

    void MeshRenderer::ShutDown()
    {
        m_meshDrawIndices.clear();
        m_textureIndices.clear();
        m_meshBoundingBoxes.clear();
    }
}
