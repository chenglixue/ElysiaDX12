#include "stdafx.h"
#include "AssimpLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "iosfwd"
#include "LoadedModel.h"
#include "Runtime/Core/DX12BufferResource.h"

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

            loadTexturePath(aiTextureType_DIFFUSE, material.textureNames[UINT64(MaterialTextureType::Albedo)]);
            loadTexturePath(aiTextureType_NORMALS, material.textureNames[UINT64(MaterialTextureType::Normal)]);
            loadTexturePath(aiTextureType_EMISSIVE, material.textureNames[UINT64(MaterialTextureType::Emissive)]);
            loadTexturePath(aiTextureType_METALNESS, material.textureNames[UINT64(MaterialTextureType::Metallic)]);
            loadTexturePath(aiTextureType_DIFFUSE_ROUGHNESS, material.textureNames[UINT64(MaterialTextureType::Roughness)]);
            loadTexturePath(aiTextureType_AMBIENT_OCCLUSION, material.textureNames[UINT64(MaterialTextureType::Occlusion)]);
            loadTexturePath(aiTextureType_SPECULAR, material.textureNames[UINT64(MaterialTextureType::Specular)]);
            loadTexturePath(aiTextureType_HEIGHT, material.textureNames[UINT64(MaterialTextureType::Height)]);

            model.materials.emplace_back(material);
        }
    }

    void LoadMaterialResource(eastl::vector<LoadedMaterial>& materials, eastl::wstring fileDirectory, GrowableList<MaterialTexture*>& materialTextures)
    {
        const UINT64 numMaterials = materials.size();

        for (UINT64 matIdx = 0; matIdx < numMaterials; matIdx++)
        {
            auto& material = materials[matIdx];

            for (UINT64 texType = 0; texType < UINT64(MaterialTextureType::Count); texType++)
            {
                material.textures[texType] = ElysiaRenderer::TextureManager::Handle::Invalid();

                eastl::wstring path = fileDirectory;
                if (material.textureNames[texType].length() <= 0 || FileExists((path + material.textureNames[texType]).c_str()) == false)
                {
                    if (texType == UINT64(MaterialTextureType::Albedo) || texType == UINT64(MaterialTextureType::Roughness)
                        || texType == UINT64(MaterialTextureType::Occlusion))
                    {
                        path += ElysiaRenderer::DefaultWhiteTexturePath;
                    }
                    else if (texType == UINT64(MaterialTextureType::Height) || texType == UINT64(MaterialTextureType::Emissive) ||
                        texType == UINT64(MaterialTextureType::Metallic) || texType == UINT64(MaterialTextureType::Specular))
                    {
                        path += ElysiaRenderer::DefaultWhiteTexturePath;
                    }
                    else if (texType == UINT64(MaterialTextureType::Normal))
                    {
                        path += ElysiaRenderer::DefaultNormalTexturePath;
                    }
                }
                else
                {
                    path += material.textureNames[texType].c_str();
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
                    bool useSRGB = texType == UINT64(MaterialTextureType::Albedo);
                    newMatTexture->texture = ElysiaRenderer::TextureManager::GetInstance().LoadDynamicTexture(ToStdWString(path), useSRGB);
                    
                    UINT64 idx = materialTextures.Add(newMatTexture);

                    material.textures[texType] = newMatTexture->texture;
                    material.textureIndices[texType] = UINT32(idx);
                }
            }
        }
    }

    void LoadMeshData(const wchar_t* filePath, const aiScene* pScene, float sceneScale, LoadedModel &model)
    {
        model.scale = sceneScale;
        const UINT64 numMeshes = pScene->mNumMeshes;
        UINT64 numVertices = 0;
        UINT64 numIndices = 0;
        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx++)
        {
            const aiMesh& pMesh = *pScene->mMeshes[meshIdx];

            numVertices += pMesh.mNumVertices;
            numIndices += pMesh.mNumFaces * 3;
        }

        model.vertices.resize(numVertices);
        model.indices.resize(numIndices);
        model.meshes.resize(numMeshes);
        uint64 vtxOffset = 0;
        uint64 idxOffset = 0;
        for(UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx++)
        {
            model.meshes[meshIdx].InitFromAssimpMesh(*pScene->mMeshes[meshIdx], sceneScale,
                &model.vertices[vtxOffset], &model.indices[idxOffset]);
            
            model.aabbMin.x = eastl::min(model.aabbMin.x, model.meshes[meshIdx].aabbMin.x);
            model.aabbMin.y = eastl::min(model.aabbMin.y, model.meshes[meshIdx].aabbMin.y);
            model.aabbMin.z = eastl::min(model.aabbMin.z, model.meshes[meshIdx].aabbMin.z);

            model.aabbMax.x = eastl::max(model.aabbMax.x, model.meshes[meshIdx].aabbMax.x);
            model.aabbMax.y = eastl::max(model.aabbMax.y, model.meshes[meshIdx].aabbMax.y);
            model.aabbMax.z = eastl::max(model.aabbMax.z, model.meshes[meshIdx].aabbMax.z);

            vtxOffset += model.meshes[meshIdx].numVertices;
            idxOffset += model.meshes[meshIdx].numIndices;
        }

        vtxOffset = 0;
        idxOffset = 0;
        model.vertexBuffer = ElysiaRenderer::BufferManager::GetInstance().CreateVertexBuffer(model);
        model.indexBuffer = ElysiaRenderer::BufferManager::GetInstance().CreateIndexBuffer(model);
        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx++)
        {
            UINT64 vbOffset = vtxOffset * sizeof(MeshVertex);
            UINT64 ibOffset = idxOffset * sizeof(UINT16);

            model.meshes[meshIdx].InitCommon(model.vertexBuffer->GetGPUAddress() + vbOffset,
                model.indexBuffer->GetGPUAddress() + ibOffset,
                vtxOffset, idxOffset);

            vtxOffset += model.meshes[meshIdx].numVertices;
            idxOffset += model.meshes[meshIdx].numIndices;
        }
    };

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
        
        auto ConvertVec = [&](aiVector3D aiVec3)
        {
            return Vector3(aiVec3.x, aiVec3.y, aiVec3.z);
        };
        
        aabbMin = Vector3(FLT_MAX);
        aabbMax = Vector3(-FLT_MAX);
        for (UINT32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx++)
        {
            if (assimpMesh.HasPositions())
            {
                Vector3 position = ConvertVec(assimpMesh.mVertices[vertexIdx]);
                
                aabbMin.x = eastl::min(aabbMin.x, position.x);
                aabbMin.y = eastl::min(aabbMin.y, position.y);
                aabbMin.z = eastl::min(aabbMin.z, position.z);
                
                aabbMax.x = eastl::max(aabbMax.x, position.x);
                aabbMax.y = eastl::max(aabbMax.y, position.y);
                aabbMax.z = eastl::max(aabbMax.z, position.z);
                
                dstVertices[vertexIdx].Position = position;
            }
            if(assimpMesh.HasNormals())
            {
                dstVertices[vertexIdx].Normal = ConvertVec(assimpMesh.mNormals[vertexIdx]);
            }
            if(assimpMesh.HasTextureCoords(0))
            {
                dstVertices[vertexIdx].UV = ConvertVec(assimpMesh.mTextureCoords[0][vertexIdx]).xy();
            }
            if(assimpMesh.HasTangentsAndBitangents())
            {
                dstVertices[vertexIdx].Tangent = ConvertVec(assimpMesh.mTangents[vertexIdx]);
            }
        }

        const UINT64 numTriangles = assimpMesh.mNumFaces;
        for(uint64 triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            dstIndices[triIdx * 3 + 0] = UINT16(assimpMesh.mFaces[triIdx].mIndices[0]);
            dstIndices[triIdx * 3 + 1] = UINT16(assimpMesh.mFaces[triIdx].mIndices[1]);
            dstIndices[triIdx * 3 + 2] = UINT16(assimpMesh.mFaces[triIdx].mIndices[2]);
        }
        
        materialIndex = assimpMesh.mMaterialIndex;
    }

    void LoadedModel::Mesh::InitCommon(uint64 vbAddress, uint64 ibAddress, uint64 vtxOffset_, uint64 idxOffset_)
    {
        vtxOffset = UINT(vtxOffset_);
        idxOffset = UINT(idxOffset_);

        vbView.BufferLocation = vbAddress;
        vbView.SizeInBytes = sizeof(MeshVertex) * numVertices;
        vbView.StrideInBytes = sizeof(MeshVertex);

        ibView.BufferLocation = ibAddress;
        ibView.SizeInBytes = IndexSize() * numIndices;
        ibView.Format = IndexBufferFormat();
    }

    
    bool LoadModel(const wchar_t* filePath, bool bInvertTexcoordY, bool bImportMeshes,
            bool bImportSkeletons, bool bImportAnimations, float scale, LoadedModel &model)
    {
        if (!FileExists(filePath))
        {
            ShowErrorMessage(MakeString(L"Model file with path '%ls' does not exist", filePath));
            return false;
        }
        WriteLog("Loading scene '%ls' with Assimp...", filePath);
        std::string fileNameAnsi = WstringToString(filePath);
        
        Assimp::Importer importer;
        auto fileDirectory = ToEastlWString(GetDirectoryFromFilePath(filePath));
        
        const aiScene* pScene = importer.ReadFile(fileNameAnsi, 0);
        if(pScene == nullptr || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !pScene->mRootNode)
        {
            ShowErrorMessage(L"Failed to load scene " + std::wstring(filePath) +
                            L": " + StringToWstring(importer.GetErrorString()));
            return false;
        }
        if (pScene->mNumMeshes <= 0)
        {
             ShowErrorMessage(L"Scene " + std::wstring(filePath) + L" has no meshes");
            return false;
        }
        if (pScene->mNumMaterials <= 0)
        {
            ShowErrorMessage(L"Scene " + std::wstring(filePath) + L" has no materials");
            return false;
        }

        unsigned int flags = aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                    aiProcess_RemoveRedundantMaterials | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder;
        if (bInvertTexcoordY)
        {
            flags |= aiProcess_FlipUVs;
        }
        flags |= aiProcess_PreTransformVertices | aiProcess_OptimizeMeshes;
        pScene = importer.ApplyPostProcessing(flags);
        
        if (bImportMeshes)
        {
            LoadMaterials(pScene, model);
            LoadMaterialResource(model.materials, fileDirectory, model.materialTextures);
            LoadMeshData(filePath, pScene, scale, model);
        }
        
        std::cout << "Finished loading scene '%ls'" + WstringToString(filePath) << std::endl;
        
        return true;
    }
}
