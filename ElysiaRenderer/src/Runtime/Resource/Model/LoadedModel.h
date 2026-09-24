#pragma once
#include "stdafx.h"

#include "assimp/mesh.h"

#include "Programs/BoundingBox.h"
#include "Runtime/RenderCore/TextureManager.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Programs/Containers.h"
#include "Runtime/Core/DX12BufferResource.h"
#include "Runtime/Resource/Serialization.h"

namespace ElysiaModel
{
    using namespace ElysiaHelper;

    struct MaterialTexture
    {
        std::wstring name; // path + name
        ElysiaRenderer::TextureManager::Handle texture;
    };

    enum class IndexType
    {
        Index16Bit = 0,
        Index32Bit = 1
    };

    enum class MaterialTextureType : int
    {
        Albedo = 0,
        Normal,
        Roughness,
        Metallic,
        Specular,
        Occlusion,
        Emissive,
        Height,

        Count
    };

    struct TextureTransform
    {
        Vector2 offset = {0.0f, 0.0f};
        Vector2 scale = {1.0f, 1.0f};
        int texCoord = 0; // glTF 支持 TEXCOORD_0, TEXCOORD_1 等

        template <typename TSerializer>
        void SerializeCPU(TSerializer& serializer)
        {
            SerializeItem(serializer, offset);
            SerializeItem(serializer, scale);
            INT32 packedTexCoord = texCoord;
            SerializeItem(serializer, packedTexCoord);
            if (TSerializer::IsReadSerializer())
                texCoord = packedTexCoord;
        }
    };

    inline Vector4 TextureTransformToVector4(const TextureTransform& tran)
    {
        return Vector4(tran.scale.x,
                       tran.scale.y,
                       tran.offset.x,
                       tran.offset.y);
    }

    struct MeshVertex
    {
        Vector3 Position;
        Vector2 UV;
        Vector3 Normal;
        Vector4 Tangent;
        Vector4 Color;

        MeshVertex()
        {
        }

        MeshVertex(const Vector3& p,
                   const Vector3& n,
                   const Vector2& uv,
                   const Vector4& t,
                   const Vector3& b)
        {
            Position = p;
            Normal = n;
            UV = uv;
            Tangent = t;
        }

        void Transform(const Vector3& p, const Vector3& s, const Quaternion& q)
        {
            Position *= s;
            Position = Vector3::Transform(Position, q);
            Position += p;

            Normal = Vector3::Transform(Normal, q);
            Tangent = Vector4::Transform(Tangent, q);
        }

        template <typename TSerializer>
        void Serialize(TSerializer& serializer)
        {
            SerializeItem(serializer, Position);
            SerializeItem(serializer, Normal);
            SerializeItem(serializer, UV);
            SerializeItem(serializer, Tangent);
            SerializeItem(serializer, Color);
        }
    };

    struct LoadedMaterial
    {
        enum class Alpha
        {
            Opaque,
            Masked,
            Blend
        };

        std::string name;
        Alpha alpha;
        Vector3 albedoFactor;
        float opacity;
        float normalFactor;
        float metallicFactor;
        float roughnessFactor;
        float specularFactor;
        Vector3 emissiveFactor;
        Vector2 uvScale = Vector2(1.0f, 1.0f);
        Vector2 uvOffset = Vector2(0.0f, 0.0f);

        std::wstring textureNames[UINT64(MaterialTextureType::Count)];
        ElysiaRenderer::TextureManager::Handle textures[uint64(MaterialTextureType::Count)] = {};
        UINT32 textureIndices[uint64(MaterialTextureType::Count)] = {};
        TextureTransform textureTransforms[static_cast<size_t>(MaterialTextureType::Count)];

        template <typename TSerializer>
        void SerializeCPU(TSerializer& serializer)
        {
            SerializeItem(serializer, name);

            UINT32 packedAlpha = static_cast<UINT32>(alpha);
            SerializeItem(serializer, packedAlpha);
            if (TSerializer::IsReadSerializer())
                alpha = static_cast<Alpha>(packedAlpha);

            SerializeItem(serializer, albedoFactor);
            SerializeItem(serializer, opacity);
            SerializeItem(serializer, normalFactor);
            SerializeItem(serializer, metallicFactor);
            SerializeItem(serializer, roughnessFactor);
            SerializeItem(serializer, specularFactor);
            SerializeItem(serializer, emissiveFactor);
            SerializeItem(serializer, uvScale);
            SerializeItem(serializer, uvOffset);

            const UINT32 textureCount = static_cast<UINT32>(MaterialTextureType::Count);
            for (UINT32 i = 0; i < textureCount; ++i)
            {
                SerializeItem(serializer, textureNames[i]);
                textureTransforms[i].SerializeCPU(serializer);
            }

            if (TSerializer::IsReadSerializer())
            {
                for (UINT32 i = 0; i < textureCount; ++i)
                {
                    textures[i] = ElysiaRenderer::TextureManager::Handle::Invalid();
                    textureIndices[i] = 0;
                }
            }
        }
    };

#define INDEX_FORMAT UINT16
    constexpr DXGI_FORMAT IndexBufferFormat()
    {
        return DXGI_FORMAT_R16_UINT;
    }
    constexpr UINT IndexSize()
    {
        return 2;
    }

    struct LoadedModel
    {
        struct Mesh
        {
            std::string name;

            UINT materialIndex = 0;

            Vector3 aabbMin = Vector3(FLT_MAX);
            Vector3 aabbMax = Vector3(-FLT_MAX);
            Vector3 logicalCenter;

            UINT32 numVertices = 0;
            UINT32 numIndices = 0;
            UINT32 vtxOffset = 0;
            UINT32 idxOffset = 0;
            IndexType indexType = IndexType::Index32Bit;
            D3D12_VERTEX_BUFFER_VIEW vbView;
            D3D12_INDEX_BUFFER_VIEW ibView;

            eastl::vector<Vector4> weights;
            eastl::vector<Vector4> joints;

            Vector3 avgColor = Vector3::Zero;


            // Init from loaded files
            void InitFromAssimpMesh(const aiMesh& assimpMesh,
                                    float sceneScale,
                                    MeshVertex* dstVertices,
                                    UINT32* dstIndices);

            void InitCommon(uint64 vbAddress,
                            uint64 ibAddress,
                            uint64 vtxOffset_,
                            uint64 idxOffset_);

            template <typename TSerializer>
            void SerializeCPU(TSerializer& serializer)
            {
                SerializeItem(serializer, name);
                SerializeItem(serializer, materialIndex);
                SerializeItem(serializer, aabbMin);
                SerializeItem(serializer, aabbMax);
                SerializeItem(serializer, logicalCenter);
                SerializeItem(serializer, numVertices);
                SerializeItem(serializer, numIndices);
                SerializeItem(serializer, vtxOffset);
                SerializeItem(serializer, idxOffset);

                UINT32 packedIndexType = static_cast<UINT32>(indexType);
                SerializeItem(serializer, packedIndexType);
                if (TSerializer::IsReadSerializer())
                    indexType = static_cast<IndexType>(packedIndexType);

                SerializeEastlVectorBulk(serializer, weights);
                SerializeEastlVectorBulk(serializer, joints);
                SerializeItem(serializer, avgColor);

                if (TSerializer::IsReadSerializer())
                {
                    vbView = {};
                    ibView = {};
                }
            }
        };


        std::string name;
        float scale = 1.f;
        Vector3 aabbMin = Vector3(FLT_MAX);
        Vector3 aabbMax = Vector3(-FLT_MAX);
        eastl::vector<MeshVertex> vertices;
        eastl::vector<UINT32> indices;

        eastl::vector<Mesh> meshes;
        eastl::vector<LoadedMaterial> materials;
        GrowableList<MaterialTexture*> materialTextures;

        template <typename TSerializer>
        void SerializeCPU(TSerializer& serializer)
        {
            SerializeItem(serializer, name);
            SerializeItem(serializer, scale);
            SerializeItem(serializer, aabbMin);
            SerializeItem(serializer, aabbMax);
            SerializeEastlVectorBulk(serializer, vertices);
            SerializeEastlVectorBulk(serializer, indices);

            UINT32 meshCount = static_cast<UINT32>(meshes.size());
            SerializeItem(serializer, meshCount);
            if (TSerializer::IsReadSerializer())
                meshes.resize(meshCount);
            for (UINT32 i = 0; i < meshCount; ++i)
                meshes[i].SerializeCPU(serializer);

            UINT32 materialCount = static_cast<UINT32>(materials.size());
            SerializeItem(serializer, materialCount);
            if (TSerializer::IsReadSerializer())
                materials.resize(materialCount);
            for (UINT32 i = 0; i < materialCount; ++i)
                materials[i].SerializeCPU(serializer);
        }
    };

    struct LoadedSkeleton
    {
        struct Joint
        {
            Matrix invBindPose;
            // inverse bind pose transform (transforms from model space to joint space)
            eastl::string name; // human-readable joint name
            uint32_t parentIdx; // parent index or -1 if root
        };

        eastl::vector<Joint> joints;
    };

    struct LoadedAnimationClip
    {
        struct TranslationChannel
        {
            eastl::vector<float> m_timeKeys;
            eastl::vector<Vector3> m_translations;
        };

        struct RotationChannel
        {
            eastl::vector<float> m_timeKeys;
            eastl::vector<Quaternion> m_rotations;
        };

        struct ScaleChannel
        {
            eastl::vector<float> m_timeKeys;
            eastl::vector<float> m_scales;
        };

        struct JointAnimationClip
        {
            TranslationChannel m_translationChannel;
            RotationChannel m_rotationChannel;
            ScaleChannel m_scaleChannel;
        };

        eastl::string name;
        size_t skeletonIndex;
        float duration;
        eastl::vector<JointAnimationClip> jointAnimations;
    };

    struct MeshData
    {
        uint32_t meshCount;
        uint32_t materialCount;
        uint32_t indexCount = 0;
        uint32_t vertexDataByteSize;
        uint32_t indexDataByteSize;

        AxisAlignedBox boundingBox;

        template <typename TSerializer>
        void Serialize(TSerializer& serializer)
        {
            SerializeItem(serializer, meshCount);
            SerializeItem(serializer, materialCount);
            SerializeItem(serializer, vertexDataByteSize);
            SerializeItem(serializer, indexDataByteSize);
            SerializeItem(serializer, boundingBox);
        }
    };
}