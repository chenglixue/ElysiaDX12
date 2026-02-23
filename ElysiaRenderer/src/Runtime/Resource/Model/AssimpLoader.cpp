#include "stdafx.h"
#include "AssimpLoader.h"

#define ASSIMP_LOADER 1
#define GLTF_LOADER 1

#if ASSIMP_LOADER == 1
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#endif

#include "iosfwd"
#include "LoadedModel.h"
#include "Runtime/Core/DX12BufferResource.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Programs/Log.h"
#include "src/ThirdParty/GLTF/tiny_gltf.h"

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

    struct GltfAccessorView
    {
        const uint8_t* dataPtr = nullptr;
        size_t stride = 0;
        size_t count = 0;

        bool IsValid() const
        {
            return dataPtr != nullptr;
        }

        template <typename T>
        T Get(size_t index) const
        {
            // 使用 Stride 跳转到正确位置
            return *reinterpret_cast<const T*>(dataPtr + (index * stride));
        }
    };

    static float ByteToFloat(int8_t b)
    {
        return fmaxf(b / 127.0f, -1.0f);
    }
    static float UByteToFloat(uint8_t b)
    {
        return b / 255.0f;
    }
    static float ShortToFloat(int16_t b)
    {
        return fmaxf(b / 32767.0f, -1.0f);
    }
    static float UShortToFloat(uint16_t b)
    {
        return b / 65535.0f;
    }
    static float IntToFloat(int32_t b)
    {
        return fmaxf(b / (float)INT32_MAX, -1.0f);
    }
    static float UIntToFloat(uint32_t b)
    {
        return b / (float)UINT32_MAX;
    }

    template <typename T>
    static const T* GetBufferDataPtr(const tinygltf::Accessor& accessor,
                                     const tinygltf::BufferView& bufferView,
                                     const tinygltf::Buffer& buffer,
                                     size_t componentCount,
                                     size_t elementIndex,
                                     size_t componentIndex)
    {
        const size_t compSize = sizeof(T);
        const size_t stride = bufferView.byteStride == 0 ? componentCount * compSize : bufferView.byteStride;
        return (T*)&buffer.data[accessor.byteOffset + bufferView.byteOffset + elementIndex * stride + componentIndex *
                                compSize];
    }

    static Matrix GetLocalNodeTransform(const tinygltf::Node& node)
    {
        if (!node.matrix.empty())
        {
            return Matrix((float*)node.matrix.data());
        }

        Vector3 scale = node.scale.size() == 3
                            ? Vector3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2])
                            : Vector3::One;

        Quaternion rot = node.rotation.size() == 4
                             ? Quaternion((float)node.rotation[0],
                                          (float)node.rotation[1],
                                          (float)node.rotation[2],
                                          (float)node.rotation[3])
                             : Quaternion::Identity;

        Vector3 trans = node.translation.size() == 3
                            ? Vector3((float)node.translation[0],
                                      (float)node.translation[1],
                                      (float)node.translation[2])
                            : Vector3::Zero;

        return Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rot) * Matrix::CreateTranslation(trans);
    }

    inline D3D12_TEXTURE_ADDRESS_MODE GetDxAddressMode(int gltfWrap)
    {
        switch (gltfWrap)
        {
        case 10497:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP; // REPEAT
        case 33648:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR; // MIRRORED_REPEAT
        case 33071:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // CLAMP_TO_EDGE
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

    GltfAccessorView GetAccessorView(const tinygltf::Model& model, int accessorIdx)
    {
        if (accessorIdx < 0)
            return {};
        const auto& acc = model.accessors[accessorIdx];
        const auto& view = model.bufferViews[acc.bufferView];
        GltfAccessorView res;
        res.count = acc.count;
        // 如果 glTF 未指定 stride，则默认为紧凑排列
        res.stride = view.byteStride == 0
                         ? tinygltf::GetComponentSizeInBytes(acc.componentType) * tinygltf::GetNumComponentsInType(
                               acc.type)
                         : view.byteStride;
        res.dataPtr = model.buffers[view.buffer].data.data() + acc.byteOffset + view.byteOffset;
        return res;
    }

    float NormalizeComponent(const unsigned char* ptr, int componentType)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return *(const float*)ptr;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: // 5123
            return *(const uint16_t*)ptr / 65535.0f;
        case TINYGLTF_COMPONENT_TYPE_SHORT: // 5122 <--- 重点检查这里！
            // AMD 做法：fmaxf(val / 32767.0f, -1.0f)
            return fmaxf(*(const int16_t*)ptr / 32767.0f, -1.0f);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: // 5121
            return *(const uint8_t*)ptr / 255.0f;
        case TINYGLTF_COMPONENT_TYPE_BYTE: // 5120
            return fmaxf(*(const int8_t*)ptr / 127.0f, -1.0f);
        default:
            return 0.0f;
        }
    }

#if ASSIMP_LOADER == 1
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

                std::wstring textureFileName = material.textureNames[texType];
                std::wstring fullPath;

                if (textureFileName.empty())
                {
                    // 如果 glTF 没提供该贴图，根据类型分配默认兜底图
                    fullPath = fileDirectory;
                    switch (static_cast<MaterialTextureType>(texType))
                    {
                    case MaterialTextureType::Albedo:
                    case MaterialTextureType::Roughness:
                    case MaterialTextureType::Occlusion:
                        fullPath += ElysiaRenderer::DefaultWhiteTexturePath;
                        break;
                    case MaterialTextureType::Normal:
                        fullPath += ElysiaRenderer::DefaultNormalTexturePath;
                        break;
                    default:
                        fullPath += ElysiaRenderer::DefaultBlackTexturePath;
                        break;
                    }
                }
                else
                {
                    // glTF 的路径解析逻辑：目录 + 文件名
                    fullPath = textureFileName;
                }

                // 2. 资源去重检查 (防止 Metallic 和 Roughness 重复加载同一张 ORM 图)
                bool alreadyLoaded = false;
                const UINT64 numLoaded = materialTextures.Count();
                for (UINT64 i = 0; i < numLoaded; i ++)
                {
                    if (materialTextures[i]->name == fullPath)
                    {
                        material.textures[texType] = materialTextures[i]->texture;
                        material.textureIndices[texType] = static_cast<UINT32>(i);
                        alreadyLoaded = true;
                        break;
                    }
                }

                // 3. 执行动态加载
                if (!alreadyLoaded)
                {
                    auto newMatTexture = new MaterialTexture();
                    newMatTexture->name = fullPath;

                    // glTF 规范：只有 BaseColor (Albedo) 和 Emissive 通常使用 sRGB
                    // Normal, Metallic, Roughness, Occlusion 必须作为线性数据处理
                    bool useSRGB = (texType == static_cast<UINT64>(MaterialTextureType::Albedo) ||
                                    texType == static_cast<UINT64>(MaterialTextureType::Emissive));

                    newMatTexture->texture = ElysiaRenderer::TextureManager::GetInstance().LoadDynamicTexture(
                        fullPath,
                        useSRGB);

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
        auto vertexBuffer = ElysiaRenderer::BufferManager::GetInstance().
            CreateVertexBuffer(model);
        auto indexBuffer = ElysiaRenderer::BufferManager::GetInstance().
            CreateIndexBuffer(model);
        auto vbView = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation = vertexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(numVertices) * vertexBuffer->
                           GetStride(),
            .StrideInBytes = vertexBuffer->GetStride()
        };
        auto ibView = D3D12_INDEX_BUFFER_VIEW
        {
            .BufferLocation = indexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(numIndices) * IndexSize(),
            .Format = IndexBufferFormat(),
        };

        for (UINT64 meshIdx = 0; meshIdx < numMeshes; meshIdx ++)
        {
            UINT64 vbOffset = vtxOffset * sizeof(MeshVertex);
            UINT64 ibOffset = idxOffset * sizeof(UINT16);

            model.meshes[meshIdx].InitCommon(
                vertexBuffer->GetGPUAddress() + vbOffset,
                indexBuffer->GetGPUAddress() + ibOffset,
                vtxOffset,
                idxOffset);

            vtxOffset += model.meshes[meshIdx].numVertices;
            idxOffset += model.meshes[meshIdx].numIndices;
        }

        ElysiaRenderer::BufferManager::GetInstance().SetGlobalVertexBuffer(std::move(vertexBuffer));
        ElysiaRenderer::BufferManager::GetInstance().SetGlobalIndexBuffer(std::move(indexBuffer));
        ElysiaRenderer::BufferManager::GetInstance().SetGlobalVertexBufferView(
            std::move(vbView));
        ElysiaRenderer::BufferManager::GetInstance().SetGlobalIndexBufferView(
            std::move(ibView));
    };

    void LoadedModel::Mesh::InitFromAssimpMesh(const aiMesh& assimpMesh,
                                               float sceneScale,
                                               MeshVertex* dstVertices,
                                               UINT32* dstIndices)
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
                             aiProcess_MakeLeftHanded | aiProcess_JoinIdenticalVertices |
                             aiProcess_GenSmoothNormals;
        if (bInvertTexcoordY)
        {
            flags |= aiProcess_FlipUVs;
        }
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
#endif

#if GLTF_LOADER == 1
    void FillNodeData(
        const tinygltf::Model& gltfModel,
        int nodeIdx,
        const Matrix& parentTransform,
        float sceneScale,
        bool bInvertY,
        uint32_t& vtxOffset,
        // 全局顶点计数器 (引用)
        uint32_t& idxOffset,
        // 全局索引计数器 (引用)
        LoadedModel& model)
    {
        const auto& node = gltfModel.nodes[nodeIdx];

        // 1. 计算变换矩阵
        Matrix localTransform = GetLocalNodeTransform(node);
        Matrix worldTransform = localTransform * parentTransform;

        // 法线变换矩阵 (逆转置)，防止非均匀缩放导致法线错误
        Matrix invTranspose = worldTransform;
        invTranspose.Invert();
        invTranspose.Transpose();

        // 2. 处理几何数据
        if (node.mesh >= 0)
        {
            const auto& gltfMesh = gltfModel.meshes[node.mesh];
            for (const auto& prim : gltfMesh.primitives)
            {
                if (prim.mode != TINYGLTF_MODE_TRIANGLES)
                    continue;

                float minU = 1e10f, maxU = -1e10f;
                float minV = 1e10f, maxV = -1e10f;

                LoadedModel::Mesh newElysiaMesh;

                // 修复名称：优先使用 Node 名，并附加全局唯一 ID，解决“全是 Mesh0”的问题
                std::string baseName = node.name.empty() ? (gltfMesh.name.empty() ? "Mesh" : gltfMesh.name) : node.name;
                newElysiaMesh.name = baseName + "_" + std::to_string(model.meshes.size());
                newElysiaMesh.materialIndex = prim.material >= 0 ? prim.material : 0;

                // 获取 Accessor Views
                auto posView = GetAccessorView(gltfModel, prim.attributes.at("POSITION"));
                auto normView = GetAccessorView(gltfModel,
                                                prim.attributes.count("NORMAL") ? prim.attributes.at("NORMAL") : -1);
                auto uvView = GetAccessorView(gltfModel,
                                              prim.attributes.count("TEXCOORD_0")
                                                  ? prim.attributes.at("TEXCOORD_0")
                                                  : -1);
                auto tanView = GetAccessorView(gltfModel,
                                               prim.attributes.count("TANGENT") ? prim.attributes.at("TANGENT") : -1);

                uint32_t vCount = (uint32_t)posView.count;
                // --- 自适应倍率计算逻辑 ---
                if (uvView.IsValid())
                {
                    for (size_t v = 0; v < vCount; ++v)
                    {
                        Vector2 uv = uvView.Get<Vector2>(v);
                        minU = fminf(minU, uv.x);
                        maxU = fmaxf(maxU, uv.x);
                        minV = fminf(minV, uv.y);
                        maxV = fmaxf(maxV, uv.y);
                    }
                }
                float rangeU = maxU - minU;
                float rangeV = maxV - minV;
                bool bNeedsNormalizationU = (rangeU > 0.001f && rangeU < 0.7f);
                bool bNeedsNormalizationV = (rangeV > 0.001f && rangeV < 0.7f);

                // -------------------------------------------------------
                // 填充顶点
                // -------------------------------------------------------
                for (size_t v = 0; v < vCount; ++v)
                {
                    // 直接写入全局 buffer 的指定位置
                    MeshVertex& vtx = model.vertices[vtxOffset + v];
                    vtx.Position = Vector3::Transform(posView.Get<Vector3>(v) * sceneScale, worldTransform);

                    if (normView.IsValid())
                    {
                        vtx.Normal = Vector3::TransformNormal(normView.Get<Vector3>(v), invTranspose);
                        vtx.Normal.Normalize();
                    }

                    // --- 第二遍填充数据 ---
                    // ... 前面的 Position 计算 ...
                    if (uvView.IsValid())
                    {
                        Vector2 uv = uvView.Get<Vector2>(v);

                        // 如果 U 被压缩了，将其映射回 0..1
                        if (bNeedsNormalizationU)
                        {
                            uv.x = (uv.x - minU) / rangeU;
                        }
                        // 如果 V 被压缩了，将其映射回 0..1
                        if (bNeedsNormalizationV)
                        {
                            uv.y = (uv.y - minV) / rangeV;
                        }

                        // 最后应用 DX 翻转
                        vtx.UV = bInvertY ? Vector2(uv.x, 1.0f - uv.y) : uv;
                    }

                    if (tanView.IsValid())
                    {
                        // glTF切线是vec4
                        Vector4 t = tanView.Get<Vector4>(v);
                        vtx.Tangent = Vector3::TransformNormal(Vector3(t.x, t.y, t.z), worldTransform);
                        vtx.Tangent.Normalize();
                    }

                    // 更新 AABB
                    newElysiaMesh.aabbMin = Vector3::Min(newElysiaMesh.aabbMin, vtx.Position);
                    newElysiaMesh.aabbMax = Vector3::Max(newElysiaMesh.aabbMax, vtx.Position);
                }

                // -------------------------------------------------------
                // 填充索引 (核心修复区)
                // -------------------------------------------------------
                if (prim.indices >= 0)
                {
                    auto idxAcc = gltfModel.accessors[prim.indices];
                    auto idxView = GetAccessorView(gltfModel, prim.indices);
                    uint32_t iCount = (uint32_t)idxView.count;

                    for (size_t i = 0; i < iCount; ++i)
                    {
                        // 读取原始索引值
                        uint32_t localIdx = 0;
                        if (idxAcc.componentType == 5123)               // unsigned short
                            localIdx = ((uint16_t*)idxView.dataPtr)[i]; // 这里不需要 Stride，因为索引是紧凑的
                        else if (idxAcc.componentType == 5125)          // unsigned int
                            localIdx = ((uint32_t*)idxView.dataPtr)[i];
                        else if (idxAcc.componentType == 5121) // unsigned byte
                            localIdx = ((uint8_t*)idxView.dataPtr)[i];

                        // 【核心修正】
                        // 因为你在 InitCommon 里使用了 vb->GetGPUAddress() + vtxOffset
                        // 所以这里的索引必须是【局部索引】(从 0 开始)。
                        // 之前代码加了 vtxOffset，导致了双重偏移，所以画面炸裂。
                        model.indices[idxOffset + i] = (uint32_t)(localIdx);
                    }

                    newElysiaMesh.numIndices = iCount;
                    newElysiaMesh.idxOffset = idxOffset;
                    idxOffset += iCount;
                }

                newElysiaMesh.numVertices = vCount;
                newElysiaMesh.vtxOffset = vtxOffset;
                vtxOffset += vCount; // 累加全局偏移

                model.meshes.push_back(newElysiaMesh);
            }
        }

        for (int child : node.children)
            FillNodeData(gltfModel, child, worldTransform, sceneScale, bInvertY, vtxOffset, idxOffset, model);
    }

    bool LoadGLTFModel(const std::wstring& filePath,
                       bool bInvertTexcoordY,
                       bool bImportMeshes,
                       bool bImportSkeletons,
                       bool bImportAnimations,
                       float scale,
                       LoadedModel& model)
    {
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        std::string fileName = WstringToString(filePath);
        if (!loader.LoadASCIIFromFile(&gltfModel, &err, &warn, fileName))
        {
            // 尝试加载二进制
            if (!loader.LoadBinaryFromFile(&gltfModel, &err, &warn, fileName))
                return false;
        }

        auto fileDir = GetDirectoryFromFilePath(filePath);

        // 1. 加载材质与 Alpha 模式
        for (const auto& gMat : gltfModel.materials)
        {
            LoadedMaterial elysiaMat;
            elysiaMat.name = gMat.name;

            // 自动分类 Alpha 模式，用于 SBT 绑定
            if (gMat.alphaMode == "MASK")
                elysiaMat.alpha = LoadedMaterial::Alpha::Masked;
            else if (gMat.alphaMode == "BLEND")
                elysiaMat.alpha = LoadedMaterial::Alpha::Blend;
            else
                elysiaMat.alpha = LoadedMaterial::Alpha::Opaque;

            auto pbr = gMat.pbrMetallicRoughness;
            elysiaMat.albedoFactor = Vector3((float)pbr.baseColorFactor[0],
                                             (float)pbr.baseColorFactor[1],
                                             (float)pbr.baseColorFactor[2]);
            elysiaMat.normalFactor = gMat.normalTexture.scale;
            elysiaMat.metallicFactor = (float)pbr.metallicFactor;
            elysiaMat.roughnessFactor = (float)pbr.roughnessFactor;
            elysiaMat.emissiveFactor = Vector3((float)gMat.emissiveFactor[0],
                                               (float)gMat.emissiveFactor[1],
                                               (float)gMat.emissiveFactor[2]);
            elysiaMat.opacity = (float)pbr.baseColorFactor[3];
            elysiaMat.specularFactor = 0.04f;

            // 纹理映射逻辑
            auto getTexPath = [&](int texIdx) -> std::wstring
            {
                if (texIdx < 0)
                    return L"";
                int imgIdx = gltfModel.textures[texIdx].source;
                std::string uri = gltfModel.images[imgIdx].uri;
                // 防止 uri 为空 (glb 内部纹理) 的情况，这里暂不处理 glb 纹理提取
                if (uri.empty())
                    return L"";
                return fileDir + StringToWstring(uri);
            };

            elysiaMat.textureNames[(int)MaterialTextureType::Albedo] = getTexPath(pbr.baseColorTexture.index);
            elysiaMat.textureNames[(int)MaterialTextureType::Normal] = getTexPath(gMat.normalTexture.index);
            elysiaMat.textureNames[(int)MaterialTextureType::Emissive] = getTexPath(gMat.emissiveTexture.index);
            int mrTexIdx = pbr.metallicRoughnessTexture.index;
            if (mrTexIdx >= 0)
            {
                std::wstring mrPath = getTexPath(mrTexIdx);
                elysiaMat.textureNames[(int)MaterialTextureType::Roughness] = mrPath;
                elysiaMat.textureNames[(int)MaterialTextureType::Metallic] = mrPath;
            }
            int ocTexIdx = gMat.occlusionTexture.index;
            if (ocTexIdx >= 0)
            {
                elysiaMat.textureNames[(int)MaterialTextureType::Occlusion] = getTexPath(ocTexIdx);
            }

            model.materials.push_back(elysiaMat);
        }

        LoadMaterialResource(model.materials, fileDir, model.materialTextures);

        uint32_t totalV = 0, totalI = 0;
        std::function<void(int)> preCount = [&](int n)
        {
            const auto& node = gltfModel.nodes[n];
            if (node.mesh >= 0)
            {
                for (const auto& p : gltfModel.meshes[node.mesh].primitives)
                {
                    totalV += (uint32_t)gltfModel.accessors[p.attributes.at("POSITION")].count;
                    if (p.indices >= 0)
                        totalI += (uint32_t)gltfModel.accessors[p.indices].count;
                }
            }
            for (int c : node.children)
                preCount(c);
        };
        const auto& scene = gltfModel.scenes[gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0];
        for (int r : scene.nodes)
            preCount(r);

        model.vertices.resize(totalV);
        model.indices.resize(totalI);
        model.meshes.clear();

        uint32_t currentV = 0, currentI = 0;
        for (int r : scene.nodes)
            FillNodeData(gltfModel, r, Matrix::Identity, scale, bInvertTexcoordY, currentV, currentI, model);

        // 3. 构建 GPU 资源
        auto vb = ElysiaRenderer::BufferManager::GetInstance().CreateVertexBuffer(model);
        auto ib = ElysiaRenderer::BufferManager::GetInstance().CreateIndexBuffer({
            .name = StringToWstring(model.name + " Index Buffer"),
            .stride = 0,
            .size = model.indices.size() * sizeof(UINT32),
            .viewFlags = ElysiaCore::GPUResourceFlags::SRV | ElysiaCore::GPUResourceFlags::UAV,
            .accessFlags = ElysiaCore::BufferAccessFlags::GPUOnly,
            .isRawAccess = true,
            .InitData = model.indices.data()
        });

        // 映射回 Mesh 结构
        for (auto& mesh : model.meshes)
        {
            // 注意：InitCommon 使用了偏移后的地址
            mesh.InitCommon(vb->GetGPUAddress() + (uint64)mesh.vtxOffset * sizeof(MeshVertex),
                            ib->GetGPUAddress() + (uint64)mesh.idxOffset * sizeof(UINT32),
                            mesh.vtxOffset,
                            mesh.idxOffset);
        }

        auto vbView = D3D12_VERTEX_BUFFER_VIEW{vb->GetGPUAddress(), (UINT)totalV * (UINT)sizeof(MeshVertex),
                                               (UINT)sizeof(MeshVertex)};
        auto ibView = D3D12_INDEX_BUFFER_VIEW{ib->GetGPUAddress(), (UINT)totalI * (UINT)sizeof(UINT32),
                                              DXGI_FORMAT_R32_UINT};

        ElysiaRenderer::BufferManager::GetInstance().SetGlobalVertexBuffer(std::move(vb));
        ElysiaRenderer::BufferManager::GetInstance().SetGlobalIndexBuffer(std::move(ib));
        ElysiaRenderer::BufferManager::GetInstance().SetGlobalVertexBufferView(std::move(vbView));
        ElysiaRenderer::BufferManager::GetInstance().SetGlobalIndexBufferView(std::move(ibView));

        CalculateModelTransformFromBounds(model);

        return true;
    }
#endif
}