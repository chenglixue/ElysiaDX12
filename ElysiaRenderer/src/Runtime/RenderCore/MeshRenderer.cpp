#include "stdafx.h"
#include "MeshRenderer.h"

#include "Programs/Helper.h"
#include "Programs/Log.h"
#include "Runtime/Resource/Model/LoadedModel.h"

namespace ElysiaRenderer
{
    void MeshRenderer::Init(const std::shared_ptr<LoadedModel>& loadedModel, size_t meshIndex)
    {
        m_pModel = loadedModel;
        if (m_pModel == nullptr)
        {
            ThrowRuntimeError("null model pointer");
        }

        m_meshIndex = meshIndex;
        if (m_meshIndex >= loadedModel->meshes.size())
        {
            ThrowRuntimeError("mesh index out of range");
        }

        m_materialIndex = loadedModel->meshes[m_meshIndex].materialIndex;
        if (m_materialIndex >= loadedModel->materials.size())
        {
            ThrowRuntimeError("material index out of range");
        }

        {
            const LoadedModel::Mesh& mesh = m_pModel->meshes[m_meshIndex];
            m_boundingBox.Extents = (mesh.aabbMax - mesh.aabbMin) * 0.5f;
            m_boundingBox.Center = mesh.aabbMin + m_boundingBox.Extents;
        }

        {
            const auto& materials = m_pModel->materials;
            const auto& material = materials[m_materialIndex];

            m_materialTexIndices.Albedo = material.textures[UINT64(MaterialTextureType::Albedo)].
                GetResourceHeapIndex();
            m_materialTexIndices.Normal = material.textures[UINT64(MaterialTextureType::Normal)].
                GetResourceHeapIndex();
            m_materialTexIndices.Metallic = material.textures[UINT64(MaterialTextureType::Metallic)]
                .GetResourceHeapIndex();
            m_materialTexIndices.Roughness = material.textures[UINT64(
                MaterialTextureType::Roughness)].GetResourceHeapIndex();
            m_materialTexIndices.Occlusion = material.textures[UINT64(
                MaterialTextureType::Occlusion)].GetResourceHeapIndex();
            m_materialTexIndices.Specular = material.textures[UINT64(MaterialTextureType::Specular)]
                .GetResourceHeapIndex();
            m_materialTexIndices.Height = material.textures[UINT64(MaterialTextureType::Height)].
                GetResourceHeapIndex();
            m_materialTexIndices.Emissive = material.textures[UINT64(MaterialTextureType::Emissive)]
                .GetResourceHeapIndex();

            ElysiaHelper::Log::Info("Mesh[%i] bound to Material: \"%s\"", m_meshIndex, material.name.c_str());
        }
    }

    void MeshRenderer::ShutDown()
    {
        m_pModel.reset();
        m_meshIndex = UINT_MAX;
        m_materialIndex = UINT_MAX;
        m_materialTexIndices = m_materialTexIndices.Invalid();
    }
}