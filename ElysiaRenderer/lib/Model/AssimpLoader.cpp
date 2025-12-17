#include "stdafx.h"
#include "AssimpLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "iosfwd"
#include "../File/Serialization.h"

namespace ElysiaModel
{
    struct SkeletonInfo
    {
        eastl::vector<int> jointMap; // maps from node index to actual joint index
        eastl::vector<bool> isJoint;
    };
    
    struct PerNodeData
    {
        eastl::string name;
        eastl::vector<bool> isJoint; // one per skeleton
        eastl::vector<Matrix> invBindMatrices; // one per skeleton
        eastl::vector<LoadedAnimationClip::JointAnimationClip> animClips;
    };

    struct ImportContext
    {
        eastl::vector<PerNodeData> perNodeData;
        eastl::vector<eastl::vector<int>> jointMap; // one per skeleton
    };

    static float byteToFloat(int8_t b)
    {
        return fmaxf(b / 127.0f, -1.0f);
    };

    static float ubyteToFloat(uint8_t b)
    {
        return b / 255.0f;
    };

    static float shortToFloat(int16_t b)
    {
        return fmaxf(b / 32767.0f, -1.0f);
    };

    static float ushortToFloat(uint16_t b)
    {
        return b / 65535.0f;
    };

    static float intToFloat(int32_t b)
    {
        return fmaxf(b / (float)INT32_MAX, -1.0f);
    };

    static float uintToFloat(uint32_t b)
    {
        return b / (float)UINT32_MAX;
    };

    void LoadMaterials(const aiScene* pScene, LoadedModel &model)
    {
        model.materials.reserve(pScene->mNumMaterials);
        for (size_t materialIndex = 0; materialIndex < pScene->mNumMaterials; materialIndex++)
        {
            const auto aiMaterial = pScene->mMaterials[materialIndex];
            LoadedMaterial material{};
            material.name = aiMaterial->GetName().C_Str();

            aiColor3D diffuse(1.0f, 1.0f, 1.0f);
            aiColor3D emission(0.0f, 0.0f, 0.0f);
            float opacity = 1.0f;
            float metallic = 0.0f;
            float roughness = 0.0f;
            float shininess = 0.0f;

            auto loadTexturePath = [&](aiTextureType type, eastl::wstring& outPath)
            {
                aiString path;
                if (aiReturn_SUCCESS == aiMaterial->GetTexture(type, 0, &path))
                {
                    outPath = ToEastlWString(GetFileName(StringToWstring(path.C_Str()).c_str()));
                }
            };
            
            // Base Color
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_BASE_COLOR, diffuse))
            {
                material.albedoFactor = Vector3(diffuse.r, diffuse.g, diffuse.b);
            }
            else if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
            {
                material.albedoFactor = Vector3(diffuse.r, diffuse.g, diffuse.b);
            }

            // Metalness Roughness
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
            {
                material.metallicFactor = metallic;
            }
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
            {
                material.roughnessFactor = roughness;
            }

            // Emissive
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
            {
                material.emissiveFactor = Vector3{ emission.r, emission.g, emission.b };
            }

            // opacity
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_OPACITY, opacity))
            {
                material.opacity = opacity;
            }

            //specular
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_SPECULAR_FACTOR, shininess))
            {
                material.specularFactor = shininess;
            }

            loadTexturePath(aiTextureType_DIFFUSE, material.textureNames[UINT64(MaterialTextures::Albedo)]);
            loadTexturePath(aiTextureType_NORMALS, material.textureNames[UINT64(MaterialTextures::Normal)]);
            loadTexturePath(aiTextureType_EMISSIVE, material.textureNames[UINT64(MaterialTextures::Emissive)]);
            loadTexturePath(aiTextureType_METALNESS, material.textureNames[UINT64(MaterialTextures::Metallic)]);
            loadTexturePath(aiTextureType_DIFFUSE_ROUGHNESS, material.textureNames[UINT64(MaterialTextures::Roughness)]);
            loadTexturePath(aiTextureType_AMBIENT_OCCLUSION, material.textureNames[UINT64(MaterialTextures::Occlusion)]);
            loadTexturePath(aiTextureType_SPECULAR, material.textureNames[UINT64(MaterialTextures::Specular)]);
            loadTexturePath(aiTextureType_HEIGHT, material.textureNames[UINT64(MaterialTextures::Height)]);

            model.materials.emplace_back(material);
        }
    }

    void LoadMaterialResource(eastl::vector<LoadedMaterial>& materials, eastl::wstring fileDirectory, GrowableList<MaterialTexture*>& materialTextures)
    {
        const UINT64 numMaterials = materials.size();

        for (UINT64 matIdx = 0; matIdx < numMaterials; matIdx++)
        {
            auto& material = materials[matIdx];

            for (UINT64 texType = 0; texType <= UINT64(MaterialTextures::Count); texType++)
            {
                material.textures[texType] = ElysiaRenderer::TextureManager::Handle::Invalid();

                eastl::wstring path = fileDirectory + material.textureNames[texType];
                if (material.textureNames[texType].length() <= 0 || FileExists(path.c_str()) == false)
                {
                    if (texType == UINT64(MaterialTextures::Albedo) || texType == UINT64(MaterialTextures::Roughness))
                    {
                        path = ToEastlWString(ElysiaRenderer::DefaultWhiteTexturePath);
                    }
                }

                const UINT64 numLoaded = materialTextures.Count();
                for (UINT64 i = 0; i < numLoaded; i++)
                {
                    if (materialTextures[i]->name == path)
                    {
                        material.textures[texType] = materialTextures[i]->texture;
                        material.textureIndices[texType] = UINT32(i);
                        break;
                    }
                }

                if (!material.textures[texType].IsValid())
                {
                    MaterialTexture* newMatTexture = new MaterialTexture();
                    newMatTexture->name = path;
                    bool useSRGB = texType == UINT64(MaterialTextures::Albedo);
                    newMatTexture->texture = ElysiaRenderer::TextureManager::GetInstance().LoadDynamicTexture(ToStdWString(path), useSRGB);
                    
                    UINT64 idx = materialTextures.Add(newMatTexture);

                    material.textures[texType] = newMatTexture->texture;
                    material.textureIndices[texType] = UINT32(idx);
                }
            }
        }
    }

    void LoadMeshData(const aiScene* pScene, LoadedModel &model)
    {
        const UINT64 numMeshes = pScene->mNumMeshes;
        UINT64 numVertices = 0;
        UINT64 numIndices = 0;
        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx++)
        {
            const aiMesh& pMesh = *pScene->mMeshes[meshIdx];

            numVertices += pMesh.mNumVertices;
            numIndices += pMesh.mNumFaces * 3;
        }

        model.vertices.reserve(numVertices);
        model.indices.reserve(numIndices);
        model.meshes.reserve(numMeshes);
        uint64 vtxOffset = 0;
        uint64 idxOffset = 0;
        for(UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx++)
        {
            model.meshes[meshIdx].InitFromAssimpMesh(*pScene->mMeshes[meshIdx], 1.f,
                &model.vertices[vtxOffset], &model.indices[idxOffset]);
        }
    }

    void LoadedModel::Mesh::InitFromAssimpMesh(const aiMesh& assimpMesh, float sceneScale,
                                    MeshVertex* dstVertices, UINT16* dstIndices)
    {
        numVertices = assimpMesh.mNumVertices;
        numIndices = assimpMesh.mNumFaces * 3;

        indexType = IndexType::Index16Bit;
        if(numVertices > 0xFFFF)
        {
            ShowErrorMessage(L"32-bit indices not currently supported");
        }

        if (assimpMesh.HasPositions())
        {
            for (UINT32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx++)
            {

                dstVertices->Position = 
            }
            
        }
    }
    
    bool LoadModel(const wchar_t* filePath, bool bMergeByMaterial, bool bInvertTexcoordY, bool bImportMeshes,
            bool bImportSkeletons, bool bImportAnimations, float scale, LoadedModel &model)
    {
        if (!FileExists(filePath))
        {
            throw ShowErrorMessage(MakeString(L"Model file with path '%ls' does not exist", filePath));
            return false;
        }
        WriteLog("Loading scene '%ls' with Assimp...", filePath);
        std::string fileNameAnsi = WstringToString(filePath);
        
        Assimp::Importer importer;
        auto fileDirectory = ToEastlWString(GetDirectoryFromFilePath(filePath));

        const aiScene* pScene = importer.ReadFile(fileNameAnsi, 0);
        if(pScene == nullptr || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
        {
            throw ShowErrorMessage(L"Failed to load scene " + std::wstring(filePath) +
                            L": " + StringToWstring(importer.GetErrorString()));
            return false;
        }
        if (pScene->mNumMeshes <= 0)
        {
             throw ShowErrorMessage(L"Scene " + std::wstring(filePath) + L" has no meshes");
            return false;
        }
        if (pScene->mNumMaterials <= 0)
        {
            throw ShowErrorMessage(L"Scene " + std::wstring(filePath) + L" has no materials");
            return false;
        }

        unsigned int flags = aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                    aiProcess_RemoveRedundantMaterials | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder;
        if (bInvertTexcoordY)
        {
            flags |= aiProcess_FlipUVs;
        }
        pScene = importer.ApplyPostProcessing(flags);
        
        if (bImportMeshes)
        {
            LoadMaterials(pScene, model);
            LoadMaterialResource(model.materials, fileDirectory, model.materialTextures);
        }
    }
}
