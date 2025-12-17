#pragma once
#include "stdafx.h"
#include "../DX12/DX12Vertex.h"
#include "BoundingBox.h"
#include "Manager/TextureManager.h"
#include "Utility/Containers.h"

namespace ElysiaModel
{
    using namespace ElysiaHelper;

	struct MaterialTexture
	{
		eastl::wstring name;	// path + name
		ElysiaRenderer::TextureManager::Handle texture;
	};

	enum class IndexType
	{
		Index16Bit = 0,
		Index32Bit = 1
	};

	enum class MaterialTextures
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

	struct MeshVertex
	{
		Vector3 Position;
		Vector3 Normal;
		Vector2 UV;
		Vector3 Tangent;

		MeshVertex()
		{
		}

		MeshVertex(const Vector3& p, const Vector3& n, const Vector2& uv, const Vector3& t, const Vector3& b)
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
			Tangent = Vector3::Transform(Tangent, q);
		}

		template<typename TSerializer> void Serialize(TSerializer& serializer)
		{
			SerializeItem(serializer, Position);
			SerializeItem(serializer, Normal);
			SerializeItem(serializer, UV);
			SerializeItem(serializer, Tangent);
		}
	};

	struct LoadedSkeleton
	{
		struct Joint
		{
			Matrix invBindPose; // inverse bind pose transform (transforms from model space to joint space)
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
	
	struct LoadedMaterial
	{
		enum class Alpha
		{
			Opaque,
			Masked,
			Blend
		};

		eastl::string name;
		Alpha alpha;
		Vector3 albedoFactor;
		float opacity;
		float normalFactor;
		float metallicFactor;
		float roughnessFactor;
		float specularFactor;
		Vector3 emissiveFactor;

		eastl::wstring textureNames[UINT64(MaterialTextures::Count)];
		ElysiaRenderer::TextureManager::Handle textures[uint64(MaterialTextures::Count)] = { };
		UINT32 textureIndices[uint64(MaterialTextures::Count)] = {};
	};

	struct LoadedModel
	{
		struct Mesh
		{
			std::string name;

			UINT materialIndex = 0;

			Vector3 aabbMin = Vector3(FLT_MAX);
			Vector3 aabbMax = Vector3(-FLT_MAX);

			UINT32 numVertices = 0;
			UINT32 numIndices = 0;
			UINT32 vtxOffset = 0;
			UINT32 idxOffset = 0;
			IndexType indexType = IndexType::Index16Bit;
			D3D12_VERTEX_BUFFER_VIEW vbView = { };
			D3D12_INDEX_BUFFER_VIEW ibView = { };

			eastl::vector<MeshVertex> vertices;
			eastl::vector<UINT16> indices;

			eastl::vector<Vector4> weights;
			eastl::vector<Vector4> joints;

			// Init from loaded files
			void InitFromAssimpMesh(const aiMesh& assimpMesh, float sceneScale,
									MeshVertex* dstVertices, UINT16* dstIndices);
		};
		
		Vector3 aabbMin = Vector3(FLT_MAX);
		Vector3 aabbMax = Vector3(-FLT_MAX);
		eastl::vector<MeshVertex> vertices;
		eastl::vector<UINT16> indices;
		
		eastl::vector<Mesh> meshes;
		eastl::vector<LoadedMaterial> materials;
		GrowableList<MaterialTexture*> materialTextures;
		
		
	};

	struct MeshData
	{
		uint32_t meshCount;
		uint32_t materialCount;
		uint32_t indexCount = 0;
		uint32_t vertexDataByteSize;
		uint32_t indexDataByteSize;

        AxisAlignedBox boundingBox;

        template<typename TSerializer> 
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
