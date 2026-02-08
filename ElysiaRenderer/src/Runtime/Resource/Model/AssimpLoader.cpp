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
        eastl::vector<bool> isJoint;           // one per skeleton
        eastl::vector<Matrix> invBindMatrices; // one per skeleton
        eastl::vector<LoadedAnimationClip::JointAnimationClip> animClips;
    };

    struct ImportContext
    {
        eastl::vector<PerNodeData> perNodeData;
        eastl::vector<eastl::vector<int>> jointMap; // one per skeleton
    };

    void LoadMaterials(const aiScene* pScene, LoadedModel& model)
    {
        model.materials.reserve(pScene->mNumMaterials);
        for (size_t materialIndex = 0; materialIndex < pScene->mNumMaterials;
             materialIndex ++)
        {
            const auto aiMaterial = pScene->mMaterials[materialIndex];
            LoadedMaterial material{};
            material.name = aiMaterial->GetName().C_Str();

            aiColor4D diffuse(1.0f, 1.0f, 1.0f, 1.f);
            aiColor4D emission(0.0f, 0.0f, 0.0f, 0.f);
            float opacity = 1.0f;
            float metallic = 0.0f;
            float roughness = 0.0f;
            float shininess = 0.0f;

            auto loadTexturePath = [&](aiTextureType type, std::wstring& outPath)
            {
                aiString path;
                if (aiReturn_SUCCESS == aiMaterial->GetTexture(type, 0, &path))
                {
                    outPath = StringToWstring(path.C_Str());
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
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif

            // Metalness Roughness
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
            {
                material.metallicFactor = metallic;
            }
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
            {
                material.roughnessFactor = roughness;
            }
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif

            // Emissive
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
            {
                material.emissiveFactor = Vector3{emission.r, emission.g,
                                                  emission.b};
            }
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif

            // opacity
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_OPACITY, opacity))
            {
                material.opacity = opacity;
            }
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif

            //specular
            if (AI_SUCCESS == aiMaterial->Get(AI_MATKEY_SPECULAR_FACTOR, shininess))
            {
                material.specularFactor = shininess;
            }
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif

            if (aiMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                loadTexturePath(aiTextureType_DIFFUSE,
                                material.textureNames[static_cast<UINT64>(
                                    MaterialTextureType::Albedo)]);
            }
            else
            {
                loadTexturePath(aiTextureType_BASE_COLOR,
                                material.textureNames[static_cast<UINT64>(
                                    MaterialTextureType::Albedo)]);
            }
            if (aiMaterial->GetTextureCount(aiTextureType_NORMALS) > 0)
            {
                loadTexturePath(aiTextureType_NORMALS,
                                material.textureNames[static_cast<UINT64>(
                                    MaterialTextureType::Normal)]);
            }
            else
            {
                loadTexturePath(aiTextureType_HEIGHT,
                                material.textureNames[static_cast<UINT64>(
                                    MaterialTextureType::Normal)]);
            }

            loadTexturePath(aiTextureType_EMISSIVE,
                            material.textureNames[static_cast<UINT64>(
                                MaterialTextureType::Emissive)]);
            loadTexturePath(aiTextureType_METALNESS,
                            material.textureNames[static_cast<UINT64>(
                                MaterialTextureType::Metallic)]);
            loadTexturePath(aiTextureType_DIFFUSE_ROUGHNESS,
                            material.textureNames[static_cast<UINT64>(
                                MaterialTextureType::Roughness)]);
            loadTexturePath(aiTextureType_AMBIENT_OCCLUSION,
                            material.textureNames[static_cast<UINT64>(
                                MaterialTextureType::Occlusion)]);
            loadTexturePath(aiTextureType_SPECULAR,
                            material.textureNames[static_cast<UINT64>(
                                MaterialTextureType::Specular)]);
            loadTexturePath(aiTextureType_HEIGHT,
                            material.textureNames[static_cast<UINT64>(
                                MaterialTextureType::Height)]);
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif

            model.materials.emplace_back(material);
#ifdef _DEBUG
            assert(_CrtCheckMemory());
#endif
        }
    }

    void LoadMaterialResource(eastl::vector<LoadedMaterial>& materials,
                              std::wstring fileDirectory,
                              GrowableList<MaterialTexture*>& materialTextures)
    {
        const UINT64 numMaterials = materials.size();

        for (UINT64 matIdx = 0; matIdx < numMaterials; matIdx ++)
        {
            auto& material = materials[matIdx];

            for (UINT64 texType = 0; texType < static_cast<UINT64>(MaterialTextureType::Count);
                 texType ++)
            {
                material.textures[texType] = ElysiaRenderer::TextureManager::Handle::Invalid();

                std::wstring path = fileDirectory;

                if (material.textureNames[texType].length() <= 0 || FileExists(
                        path + material.textureNames[texType]) == false)
                {
                    bool hasTex = false;
                    switch (static_cast<MaterialTextureType>(texType))
                    {
                    case MaterialTextureType::Normal:
                    {
                        if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(
                                material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                            L"_Normal" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(
                                    material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                                L"_Normal" + L".png";
                            hasTex = true;
                        }
                        else if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(
                                material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                            L"_normal" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(
                                    material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                                L"_normal" + L".png";
                            hasTex = true;
                        }
                        break;
                    }
                    case MaterialTextureType::Metallic:
                    {
                        if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(
                                material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                            L"_Metallic" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(
                                    material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                                L"_Metallic" + L".png";
                            hasTex = true;
                        }
                        else if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(
                                material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                            L"_metallic" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(
                                    material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                                L"_metallic" + L".png";
                            hasTex = true;
                        }
                        break;
                    }
                    case MaterialTextureType::Roughness:
                    {
                        if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(
                                material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                            L"_Roughness" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(
                                    material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                                L"_Roughness" + L".png";
                            hasTex = true;
                        }
                        else if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(
                                material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                            L"_roughness" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(
                                    material.textureNames[UINT64(MaterialTextureType::Albedo)]) +
                                L"_roughness" + L".png";
                            hasTex = true;
                        }
                        break;
                    }
                    case MaterialTextureType::Occlusion:
                    {
                        if (FileExists(
                            path + RemoveLastUnderscoreAndAfter(material.textureNames[texType]) +
                            L"_AO" + L".png"))
                        {
                            path += RemoveLastUnderscoreAndAfter(material.textureNames[texType]) +
                                L"_AO" + L".png";
                            hasTex = true;
                        }
                        break;
                    }

                    default:
                    {
                        break;
                    }
                    }
                    if (!hasTex)
                    {
                        if (texType == static_cast<UINT64>(MaterialTextureType::Albedo) || texType
                            == static_cast<UINT64>(MaterialTextureType::Roughness)
                            || texType == static_cast<UINT64>(MaterialTextureType::Occlusion))
                        {
                            path += ElysiaRenderer::DefaultWhiteTexturePath;
                        }
                        else if (texType == static_cast<UINT64>(MaterialTextureType::Height) ||
                                 texType == static_cast<UINT64>(MaterialTextureType::Emissive)
                                 || texType == static_cast<UINT64>(MaterialTextureType::Metallic) ||
                                 texType == static_cast<UINT64>(MaterialTextureType::Specular))
                        {
                            path += ElysiaRenderer::DefaultBlackTexturePath;
                        }
                        else if (texType == static_cast<UINT64>(MaterialTextureType::Normal))
                        {
                            path += ElysiaRenderer::DefaultNormalTexturePath;
                        }
                    }
                }
                else
                {
                    path += material.textureNames[texType].c_str();
                }

                const UINT64 numLoaded = materialTextures.Count();
                for (UINT64 i = 0; i < numLoaded; i ++)
                {
                    if (materialTextures[i]->name == path)
                    {
                        material.textures[texType] = materialTextures[i]->texture;
                        material.textureIndices[texType] = static_cast<UINT32>(i);
                        break;
                    }
                }

                if (!material.textures[texType].IsValid())
                {
                    auto newMatTexture = new MaterialTexture();
                    newMatTexture->name = path;
                    bool useSRGB = texType == static_cast<UINT64>(
                                       MaterialTextureType::Albedo);
                    newMatTexture->texture =
                        ElysiaRenderer::TextureManager::GetInstance().
                        LoadDynamicTexture((path), useSRGB);

                    UINT64 idx = materialTextures.Add(newMatTexture);

                    material.textures[texType] = newMatTexture->texture;
                    material.textureIndices[texType] = static_cast<UINT32>(idx);
                }
            }
        }
    }

    void LoadMeshData(const std::wstring& filePath,
                      const aiScene* pScene,
                      float sceneScale,
                      LoadedModel& model)
    {
        const UINT64 numMeshes = pScene->mNumMeshes;
        UINT64 numVertices = 0;
        UINT64 numIndices = 0;
        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx ++)
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
        model.aabbMin = Vector3(FLT_MAX);
        model.aabbMax = Vector3(-FLT_MAX);
        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx ++)
        {
            const aiMesh* pMesh = pScene->mMeshes[meshIdx];

            model.meshes[meshIdx].InitFromAssimpMesh(
                *pScene->mMeshes[meshIdx],
                sceneScale,
                &model.vertices[vtxOffset],
                &model.indices[idxOffset]);
            model.meshes[meshIdx].name = pMesh->mName.C_Str();

            model.aabbMin = Vector3::Min(model.aabbMin,
                                         model.meshes[meshIdx].aabbMin);

            model.aabbMax = Vector3::Max(model.aabbMax,
                                         model.meshes[meshIdx].aabbMax);

            vtxOffset += model.meshes[meshIdx].numVertices;
            idxOffset += model.meshes[meshIdx].numIndices;
        }

        vtxOffset = 0;
        idxOffset = 0;
        model.vertexBuffer = ElysiaRenderer::BufferManager::GetInstance().
            CreateVertexBuffer(model);
        model.indexBuffer = ElysiaRenderer::BufferManager::GetInstance().
            CreateIndexBuffer(model);
        model.vbView = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation = model.vertexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(numVertices) * model.vertexBuffer->
                                                                  GetStride(),
            .StrideInBytes = model.vertexBuffer->GetStride()
        };
        model.ibView =
        {
            .BufferLocation = model.indexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(numIndices) * IndexSize(),
            .Format = IndexBufferFormat(),
        };

        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx ++)
        {
            UINT64 vbOffset = vtxOffset * sizeof(MeshVertex);
            UINT64 ibOffset = idxOffset * sizeof(UINT16);

            model.meshes[meshIdx].InitCommon(
                model.vertexBuffer->GetGPUAddress() + vbOffset,
                model.indexBuffer->GetGPUAddress() + ibOffset,
                vtxOffset,
                idxOffset);

            vtxOffset += model.meshes[meshIdx].numVertices;
            idxOffset += model.meshes[meshIdx].numIndices;
        }
    };

    void LoadedModel::Mesh::InitFromAssimpMesh(const aiMesh& assimpMesh,
                                               float sceneScale,
                                               MeshVertex* dstVertices,
                                               UINT16* dstIndices)
    {
        numVertices = assimpMesh.mNumVertices;
        numIndices = assimpMesh.mNumFaces * 3;

        indexType = IndexType::Index16Bit;
        if (numVertices > 0xFFFF)
        {
            ShowErrorMessage(L"32-bit indices not currently supported");
        }

        auto ConvertVec = [](aiVector3D& aiVec3)
        {
            return Vector3(aiVec3.x, aiVec3.y, aiVec3.z);
        };

        aabbMin = Vector3(FLT_MAX);
        aabbMax = Vector3(-FLT_MAX);
        for (UINT32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx ++)
        {
            if (assimpMesh.HasPositions())
            {
                Vector3 position = ConvertVec(assimpMesh.mVertices[vertexIdx]) * sceneScale;

                aabbMin = Vector3::Min(aabbMin, position);
                aabbMax = Vector3::Max(aabbMax, position);

                dstVertices[vertexIdx].Position = position;
            }
            if (assimpMesh.HasNormals())
            {
                dstVertices[vertexIdx].Normal = ConvertVec(
                    assimpMesh.mNormals[vertexIdx]);
            }
            if (assimpMesh.HasTextureCoords(0))
            {
                dstVertices[vertexIdx].UV = ConvertVec(
                    assimpMesh.mTextureCoords[0][vertexIdx]).xy();
            }
            if (assimpMesh.HasTangentsAndBitangents())
            {
                dstVertices[vertexIdx].Tangent = ConvertVec(
                    assimpMesh.mTangents[vertexIdx]);
            }
        }

        const UINT32 numTriangles = assimpMesh.mNumFaces;
        for (UINT32 triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            dstIndices[triIdx * 3 + 0] = static_cast<UINT16>(assimpMesh.mFaces[
                triIdx].mIndices[0]);
            dstIndices[triIdx * 3 + 1] = static_cast<UINT16>(assimpMesh.mFaces[
                triIdx].mIndices[1]);
            dstIndices[triIdx * 3 + 2] = static_cast<UINT16>(assimpMesh.mFaces[
                triIdx].mIndices[2]);
        }

        materialIndex = assimpMesh.mMaterialIndex;
        logicalCenter = (aabbMax + aabbMin) * 0.5f;
    }

    void LoadedModel::Mesh::InitCommon(uint64 vbAddress,
                                       uint64 ibAddress,
                                       uint64 vtxOffset_,
                                       uint64 idxOffset_)
    {
        vtxOffset = static_cast<UINT>(vtxOffset_);
        idxOffset = static_cast<UINT>(idxOffset_);

        vbView.BufferLocation = vbAddress;
        vbView.SizeInBytes = sizeof(MeshVertex) * numVertices;
        vbView.StrideInBytes = sizeof(MeshVertex);

        ibView.BufferLocation = ibAddress;
        ibView.SizeInBytes = IndexSize() * numIndices;
        ibView.Format = IndexBufferFormat();
    }

    void CalculateModelTransformFromBounds(LoadedModel& model)
    {
        if (model.meshes.empty())
            return;

        // 计算所有网格的总体包围盒（局部空间）
        Vector3 overallMin(FLT_MAX);
        Vector3 overallMax(-FLT_MAX);

        for (const auto& mesh : model.meshes)
        {
            overallMin = Vector3::Min(overallMin, mesh.aabbMin); // 需要存储局部AABB
            overallMax = Vector3::Max(overallMax, mesh.aabbMax);
        }

        // 存储包围盒信息
        model.aabbMin = overallMin;
        model.aabbMax = overallMax;
    }

    bool LoadModel(const std::wstring& filePath,
                   bool bInvertTexcoordY,
                   bool bImportMeshes,
                   bool bImportSkeletons,
                   bool bImportAnimations,
                   float scale,
                   LoadedModel& model)
    {
        if (!FileExists(filePath))
        {
            ShowErrorMessage(
                MakeString(L"Model file with path '%ls' does not exist", filePath));
            return false;
        }
        WriteLog("Loading scene '%ls' with Assimp...", filePath);
        std::string fileNameAnsi = WstringToString(filePath);

        Assimp::Importer importer;
        auto fileDirectory = GetDirectoryFromFilePath(filePath);

        unsigned int flags = aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                             aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder
                             | aiProcess_RemoveRedundantMaterials | aiProcess_JoinIdenticalVertices;
        if (bInvertTexcoordY)
        {
            flags |= aiProcess_FlipUVs;
        }
        flags |= aiProcess_OptimizeMeshes;
        const aiScene* pScene = importer.ReadFile(fileNameAnsi, flags);
        model.name = WstringToString(
            GetFileName(RemoveExt(WstringToString(filePath).c_str()).c_str()));
        if (pScene == nullptr || pScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !
            pScene->mRootNode)
        {
            ShowErrorMessage(L"Failed to load scene " + std::wstring(filePath) +
                             L": " + StringToWstring(importer.GetErrorString()));
            return false;
        }
        if (pScene->mNumMeshes <= 0)
        {
            ShowErrorMessage(
                L"Scene " + std::wstring(filePath) + L" has no meshes");
            return false;
        }
        if (pScene->mNumMaterials <= 0)
        {
            ShowErrorMessage(
                L"Scene " + std::wstring(filePath) + L" has no materials");
            return false;
        }

        if (bImportMeshes)
        {
            LoadMaterials(pScene, model);

            LoadMaterialResource(model.materials,
                                 fileDirectory,
                                 model.materialTextures);

            LoadMeshData(filePath, pScene, scale, model);

            CalculateModelTransformFromBounds(model);
        }

        std::cout << "Finished loading scene '%ls'" + WstringToString(filePath) <<
            std::endl;

        return true;
    }
}