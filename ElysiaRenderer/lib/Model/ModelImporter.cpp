#include "stdafx.h"
#include "../Utility/Helper.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "iosfwd"
#include "../File/Serialization.h"

#include "ModelImporter.h"
#include "../DX12/DX12Device.h"
#include "../DX12/DX12UploadContext.h"
#include "../DX12/DX12TextureBuffer.h"
#include "../Utility/BufferUtility.h"
#include "src/Parameter/CBVParameter.h"

namespace ElysiaModel
{
	using namespace std;
	using namespace ElysiaHelper;

	std::unique_ptr<ModelImporter> g_pModelImporter = nullptr;

	ModelImporter::ModelImporter(DX12Device* pDevice) :
		m_pDevice(pDevice)
	{

	}

	ModelImporter::~ModelImporter()
	{
	}

	UINT ModelImporter::GetMeshCount() const noexcept
	{
		return m_meshData.meshCount;
	}
	const Mesh& ModelImporter::GetMesh(UINT meshIndex) const
	{
		assert(meshIndex < m_meshData.meshCount);
		return m_pMesh[meshIndex];
	}

	UINT ModelImporter::GetMaterialCount() const noexcept
	{
		return m_meshData.materialCount;
	}
	const MaterialData& ModelImporter::GetMaterialData(UINT materialIndex) const
	{
		assert(materialIndex < m_meshData.materialCount);
		return m_pMaterialData[materialIndex];
	}

	UINT ModelImporter::GetVertexStride() const noexcept
	{
		return m_vertexStride;
	}

	const AxisAlignedBox& ModelImporter::GetBoundingBox() const noexcept
	{
		return m_meshData.boundingBox;
	}

	MeshRender& ModelImporter::GetMeshRenderer(UINT meshRendererIndex) const
	{
		assert(meshRendererIndex < m_meshData.meshCount);

		return m_pMeshRender[meshRendererIndex];
	}

	bool ModelImporter::Load(const LPCWSTR& fileName)
	{
		// get model resource in x64
		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		std::wstring modelFullPath = ElysiaHelper::GetAssetFullPath(assetsPath, fileName).c_str();
		auto modelAssimpPath = std::filesystem::path(modelFullPath).string();
		auto modelH3DPath = WstringToString(RemoveLastAnythingAndAfter(StringToWstring(modelAssimpPath), L".") + L".elysia");

		std::string modelPath;
		modelPath = modelAssimpPath;
		LoadAssimp(modelPath);
		/*if (!FileExists(stringToLPCWSTR(modelH3DPath)))
		{
			std::cout << "no seralized model" << std::endl;
			if (!FileExists(stringToLPCWSTR(modelAssimpPath)))
			{
				ThrowRuntimeError("invalid assimp model path");
				return false;
			}
			else
			{
				modelPath = modelAssimpPath;
				LoadAssimp(modelPath);
			}
		}
		else
		{
			modelPath = modelH3DPath;
			LoadSerialize(modelPath);
		}*/

		return true;
	}

	bool ModelImporter::Load(const std::vector<LPCWSTR>& fileNames)
	{
		bool isLoadSuccess = true;
		for (auto fileName : fileNames)
		{
			isLoadSuccess &= Load(fileName);
		}

		PrintModelStats();

		return isLoadSuccess;
	}

	bool ModelImporter::LoadAssimp(const std::string& fileName)
	{
		Assimp::Importer importer;

		// remove unused data
		//importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
		//	aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS);

		//// max triangles and vertices per mesh, splits above this threshold
		//importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, INT_MAX);
		//importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 0xfffe); // avoid the primitive restart index

		//// remove points and lines
		//importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

		const aiScene* pScene = importer.ReadFile(fileName, 0);

		UINT32 flags = aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_MakeLeftHanded |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_FlipUVs |
			aiProcess_FlipWindingOrder |
			aiProcess_PreTransformVertices | aiProcess_OptimizeMeshes;

		pScene = importer.ApplyPostProcessing(flags);

		if (pScene == nullptr) return false;

		if (pScene->HasTextures())
		{
			// embedded textures...
		}

		if (pScene->HasAnimations())
		{
			// todo
		}

		// load material
		m_meshData.materialCount = pScene->mNumMaterials;
		m_pMaterialData = new MaterialData[m_meshData.materialCount];
		memset(m_pMaterialData, 0, sizeof(MaterialData) * m_meshData.materialCount);
		for (UINT materialIndex = 0; materialIndex < m_meshData.materialCount; ++materialIndex)
		{
			const auto srcMaterial = pScene->mMaterials[materialIndex];
			auto destMaterial = m_pMaterialData + materialIndex;

			aiColor3D diffuse(1.0f, 1.0f, 1.0f);
			aiColor3D specular(1.0f, 1.0f, 1.0f);
			aiColor3D ambient(1.0f, 1.0f, 1.0f);
			aiColor3D emission(0.0f, 0.0f, 0.0f);
			float opacity = 1.0f;
			float shininess = 0.0f;
			float specularIntensity = 1.0f;
			aiString texDiffusePath;
			aiString texMetallicPath;
			aiString texEmissionPath;
			aiString texNormalPath;
			aiString texLightmapPath;
			aiString texReflectionPath;

			srcMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			srcMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);
			srcMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
			srcMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emission);
			srcMaterial->Get(AI_MATKEY_OPACITY, opacity);
			srcMaterial->Get(AI_MATKEY_SHININESS, shininess);
			srcMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, specularIntensity);
			if (srcMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texDiffusePath) == aiReturn_SUCCESS)
			{
				strncpy_s(destMaterial->texDiffusePath, texDiffusePath.C_Str(), MaterialData::maxTexPath - 1);
			}
			if (srcMaterial->GetTexture(aiTextureType_AMBIENT, 0, &texMetallicPath) == aiReturn_SUCCESS)
			{
				strncpy_s(destMaterial->texMetallicPath, texMetallicPath.C_Str(), MaterialData::maxTexPath - 1);
			}
			if (srcMaterial->GetTexture(aiTextureType_NORMALS, 0, &texNormalPath) == aiReturn_SUCCESS ||
				srcMaterial->GetTexture(aiTextureType_HEIGHT, 0, &texNormalPath) == aiReturn_SUCCESS)
			{
				strncpy_s(destMaterial->texNormalPath, texNormalPath.C_Str(), MaterialData::maxTexPath - 1);
			}
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), texEmissionPath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_LIGHTMAP, 0), texLightmapPath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_REFLECTION, 0), texReflectionPath);

			destMaterial->diffuse = Color(diffuse.r, diffuse.g, diffuse.b);
			destMaterial->specular = Color(specular.r, specular.g, specular.b);
			destMaterial->emission = Color(emission.r, emission.g, emission.b);
			destMaterial->ambient = Color(ambient.r, ambient.g, ambient.b);
			destMaterial->opacity = opacity;
			destMaterial->shininess = shininess;
			destMaterial->specularIntensity = specularIntensity;

			strncpy_s(destMaterial->texEmissionPath, texEmissionPath.C_Str(), MaterialData::maxTexPath - 1);
			strncpy_s(destMaterial->texLightmapPath, texLightmapPath.C_Str(), MaterialData::maxTexPath - 1);
			strncpy_s(destMaterial->texReflectionPath, texReflectionPath.C_Str(), MaterialData::maxTexPath - 1);

			aiString materialName;
			srcMaterial->Get(AI_MATKEY_NAME, materialName);
			strncpy_s(destMaterial->name, materialName.C_Str(), MaterialData::maxMaterialName - 1);
		}

		// load mesh
		m_meshData.meshCount = pScene->mNumMeshes;
		m_pMesh = new Mesh[m_meshData.meshCount];
		memset(m_pMesh, 0, sizeof(Mesh) * m_meshData.meshCount);
		for (unsigned int meshIndex = 0; meshIndex < pScene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* srcMesh = pScene->mMeshes[meshIndex];
			auto destMesh = m_pMesh + meshIndex;

			assert((srcMesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == aiPrimitiveType_TRIANGLE);

			destMesh->materialIndex = srcMesh->mMaterialIndex;

			destMesh->attribsEnabled |= attrib_mask_position;
			destMesh->attrib[attrib_position].offset = destMesh->vertexStride;
			destMesh->attrib[attrib_position].normalized = 0;
			destMesh->attrib[attrib_position].components = 3;
			destMesh->attrib[attrib_position].format = attrib_format_float;
			destMesh->vertexStride += sizeof(float) * destMesh->attrib[attrib_position].components;

			destMesh->attribsEnabled |= attrib_mask_texcoord0;
			destMesh->attrib[attrib_texcoord0].offset = destMesh->vertexStride;
			destMesh->attrib[attrib_texcoord0].normalized = 0;
			destMesh->attrib[attrib_texcoord0].components = 2;
			destMesh->attrib[attrib_texcoord0].format = attrib_format_float;
			destMesh->vertexStride += sizeof(float) * destMesh->attrib[attrib_texcoord0].components;

			destMesh->attribsEnabled |= attrib_mask_normal;
			destMesh->attrib[attrib_normal].offset = destMesh->vertexStride;
			destMesh->attrib[attrib_normal].normalized = 0;
			destMesh->attrib[attrib_normal].components = 3;
			destMesh->attrib[attrib_normal].format = attrib_format_float;
			destMesh->vertexStride += sizeof(float) * destMesh->attrib[attrib_normal].components;

			destMesh->attribsEnabled |= attrib_mask_tangent;
			destMesh->attrib[attrib_tangent].offset = destMesh->vertexStride;
			destMesh->attrib[attrib_tangent].normalized = 0;
			destMesh->attrib[attrib_tangent].components = 3;
			destMesh->attrib[attrib_tangent].format = attrib_format_float;
			destMesh->vertexStride += sizeof(float) * destMesh->attrib[attrib_tangent].components;

			/*destMesh->attribsEnabled |= attrib_mask_bitangent;
			destMesh->attrib[attrib_bitangent].offset = destMesh->vertexStride;
			destMesh->attrib[attrib_bitangent].normalized = 0;
			destMesh->attrib[attrib_bitangent].components = 3;
			destMesh->attrib[attrib_bitangent].format = attrib_format_float;
			destMesh->vertexStride += sizeof(float) * destMesh->attrib[attrib_bitangent].components;*/

			destMesh->vertexDataOffset = m_meshData.vertexDataByteSize;
			destMesh->vertexCount = srcMesh->mNumVertices;

			destMesh->indexDataOffset = m_meshData.indexDataByteSize;
			destMesh->indexCount = srcMesh->mNumFaces * 3;

			m_meshData.vertexDataByteSize += destMesh->vertexStride * destMesh->vertexCount;
			m_meshData.indexDataByteSize += sizeof(UINT16) * destMesh->indexCount;
		}
		if (m_meshData.meshCount > 0)
		{
			m_vertexStride = m_pMesh[0].vertexStride;
		}

		m_pVertexData = new uint8_t[m_meshData.vertexDataByteSize];
		m_pIndexData = new uint8_t[m_meshData.indexDataByteSize];
		for (unsigned int meshIndex = 0; meshIndex < pScene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* srcMesh = pScene->mMeshes[meshIndex];
			auto destMesh = m_pMesh + meshIndex;
			destMesh->name = srcMesh->mName.C_Str();

			float* destPos = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_position].offset);
			float* destTexcoord0 = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_texcoord0].offset);
			float* destNormal = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_normal].offset);
			float* destTangent = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_tangent].offset);
			//float* destBitangent = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_bitangent].offset);

			for (unsigned int v = 0; v < destMesh->vertexCount; ++v)
			{
				m_vertexCount++;

				if (srcMesh->mVertices)
				{
					destPos[0] = srcMesh->mVertices[v].x;
					destPos[1] = srcMesh->mVertices[v].y;
					destPos[2] = srcMesh->mVertices[v].z;
				} 
				else
				{
					ElysiaHelper::AssertError("No Vertex");
				}
				destPos = (float*)((uint8_t*)destPos + destMesh->vertexStride);

				if (srcMesh->mTextureCoords[0])
				{
					destTexcoord0[0] = srcMesh->mTextureCoords[0][v].x;
					destTexcoord0[1] = srcMesh->mTextureCoords[0][v].y;
				}
				else
				{
					destTexcoord0[0] = 0.f;
					destTexcoord0[1] = 0.f;
				}
				destTexcoord0 = (float*)((uint8_t*)destTexcoord0 + destMesh->vertexStride);

				if (srcMesh->mNormals)
				{
					destNormal[0] = srcMesh->mNormals[v].x;
					destNormal[1] = srcMesh->mNormals[v].y;
					destNormal[2] = srcMesh->mNormals[v].z;
				}
				else
				{
					ElysiaHelper::AssertError("No Normal");
				}
				destNormal = (float*)((uint8_t*)destNormal + destMesh->vertexStride);

				if (srcMesh->mTangents)
				{
					destTangent[0] = srcMesh->mTangents[v].x;
					destTangent[1] = srcMesh->mTangents[v].y;
					destTangent[2] = srcMesh->mTangents[v].z;
				}
				else
				{
					destTangent[0] = 1.f;
					destTangent[1] = 0.f;
					destTangent[2] = 0.f;
				}
				destTangent = (float*)((uint8_t*)destTangent + destMesh->vertexStride);

				/*if (srcMesh->mBitangents)
				{
					destBitangent[0] = srcMesh->mBitangents[v].x;
					destBitangent[1] = srcMesh->mBitangents[v].y;
					destBitangent[2] = srcMesh->mBitangents[v].z;
				}
				else
				{
					destBitangent[0] = 0.f;
					destBitangent[1] = 1.f;
					destBitangent[2] = 0.f;
				}
				destBitangent = (float*)((uint8_t*)destBitangent + destMesh->vertexStride);*/
			}

			UINT16* destIndex = (UINT16*)(m_pIndexData + destMesh->indexDataOffset);
			for (UINT f = 0; f < srcMesh->mNumFaces; f++)
			{
				m_indexCount++;

				assert(srcMesh->mFaces[f].mNumIndices == 3);

				*destIndex++ = srcMesh->mFaces[f].mIndices[0];
				*destIndex++ = srcMesh->mFaces[f].mIndices[1];
				*destIndex++ = srcMesh->mFaces[f].mIndices[2];
			}
		}

		ComputeAllBoundingBoxes();

		LoadTextures(GetBasePath(StringToWstring(fileName)));

		Save(fileName);

		return true;
	}

	bool ModelImporter::LoadSerialize(const std::string& fileName)
	{
		FileReadSerializer serializer(stringToLPCWSTR(fileName));

		SerializeData(serializer, m_meshData);
		SerializeData(serializer, m_vertexStride);

		m_pMesh = new Mesh[m_meshData.meshCount];
		m_pMaterialData = new MaterialData[m_meshData.materialCount];
		m_pVertexData = new uint8_t[m_meshData.vertexDataByteSize];
		m_pIndexData = new uint8_t[m_meshData.indexDataByteSize];

		BulkSerializeArray(serializer, m_pMesh, static_cast<UINT64>(m_meshData.meshCount));
		BulkSerializeArray(serializer, m_pMaterialData, static_cast<UINT64>(m_meshData.materialCount));
		BulkSerializeArray(serializer, m_pVertexData, static_cast<UINT64>(m_meshData.vertexDataByteSize));
		BulkSerializeArray(serializer, m_pIndexData, static_cast<UINT64> (m_meshData.vertexDataByteSize));

		return true;
	}

	bool ModelImporter::Save(const std::string& fileName)
	{
		auto modelH3DPath = WstringToString(RemoveLastAnythingAndAfter(StringToWstring(fileName), L".") + L".elysia");

		FileWriteSerializer serializer(stringToLPCWSTR(modelH3DPath));

		SerializeData(serializer, m_meshData);
		SerializeData(serializer, m_vertexStride);

		BulkSerializeArray(serializer, m_pMesh, static_cast<UINT64>(m_meshData.meshCount));
		BulkSerializeArray(serializer, m_pMaterialData, static_cast<UINT64>(m_meshData.materialCount));
		BulkSerializeArray(serializer, m_pVertexData, static_cast<UINT64>(m_meshData.vertexDataByteSize));
		BulkSerializeArray(serializer, m_pIndexData, static_cast<UINT64>(m_meshData.vertexDataByteSize));

		return true;
	}

	void ModelImporter::ComputeMeshBoundingBox(uint32_t meshIndex, AxisAlignedBox& bbox) const
	{
		const auto pMesh = m_pMesh + meshIndex;
		bbox = AxisAlignedBox();

		if (pMesh->vertexCount <= 0) return;

		UINT vertexStride = pMesh->vertexStride;
		const auto p = (float*)(m_pVertexData + pMesh->vertexDataOffset + pMesh->attrib[attrib_position].offset);
		const auto pEnd = (float*)(m_pVertexData + pMesh->vertexDataOffset + pMesh->attrib[attrib_position].offset + pMesh->vertexCount * pMesh->vertexStride);
		
		while (p < pEnd)
		{
			Vector3 pos(*(p + 0), *(p + 1), *(p + 2));

			bbox.AddPoint(pos);

			(*(uint8_t**) & p) += vertexStride;
		}
	}

	void ModelImporter::ComputeGlobalBoundingBox(AxisAlignedBox& bbox) const
	{
		bbox = AxisAlignedBox();

		if (m_meshData.meshCount <= 0)
		{
			AssertError("mesh Count < 0, Compute global bounding box error");
			return;
		}

		for (UINT meshIndex = 0; meshIndex < m_meshData.meshCount; ++meshIndex)
		{
			const auto pMesh = m_pMesh + meshIndex;
			bbox.AddBoundingBox(pMesh->boundingBox);
		}
	}

	void ModelImporter::ComputeAllBoundingBoxes()
	{
		for (UINT meshIndex = 0; meshIndex < m_meshData.meshCount; ++meshIndex)
		{
			Mesh* pMesh = m_pMesh + meshIndex;
			ComputeMeshBoundingBox(meshIndex, pMesh->boundingBox);
		}

		ComputeGlobalBoundingBox(m_meshData.boundingBox);
	}

	void ModelImporter::Optimize()
	{
		// TODO: quantize/compress vertex data

		OptimizeRemoveDuplicateVertices();

		// re-order indices for post transform cache
		//OptimizePostTransform();

		// re-order vertices for linear memory access
		//OptimizePreTransform();
	}

	void ModelImporter::OptimizePreTransform()
	{
		unsigned char* reorderedVertexData = new unsigned char[m_meshData.vertexDataByteSize];

		for (unsigned int meshIndex = 0; meshIndex < m_meshData.meshCount; meshIndex++)
		{
			Mesh* mesh = m_pMesh + meshIndex;
			unsigned int indexCount = mesh->indexCount;
			unsigned int vertexStride = mesh->vertexStride;
			unsigned char* meshVertexData = (m_pVertexData + mesh->vertexDataOffset);

			unsigned char* meshReorderedVertexData = reorderedVertexData + mesh->vertexDataOffset;
			unsigned int reorderedCount = 0;

			unsigned int vertexCount = mesh->vertexCount;
			uint32_t* vertexRemap = new uint32_t[vertexCount];
			memset(vertexRemap, (uint32_t)-1, sizeof(uint32_t) * vertexCount);
			assert(vertexCount <= (uint32_t)-1);

			uint16_t* indexArray = (uint16_t*)(m_pIndexData + mesh->indexDataOffset);
			for (unsigned int n = 0; n < indexCount; n++)
			{
				uint16_t index = indexArray[n];
				if (vertexRemap[index] == (uint32_t)-1)
				{
					// not relocated yet
					const unsigned char* vSrc = meshVertexData + index * vertexStride;
					unsigned char* vDst = meshReorderedVertexData + reorderedCount * vertexStride;
					memcpy(vDst, vSrc, vertexStride);

					vertexRemap[index] = reorderedCount;
					reorderedCount++;
				}
				indexArray[n] = vertexRemap[index];
			}

			delete[] vertexRemap;
		}

		{
			delete[] m_pVertexData;
			m_pVertexData = reorderedVertexData;
		}
	}

	void ModelImporter::OptimizeRemoveDuplicateVertices()
	{
		unsigned char* deduplicatedVertexData = new unsigned char[m_meshData.vertexDataByteSize];
		uint32_t deduplicatedVertexDataSize = 0;

		for (unsigned int meshIndex = 0; meshIndex < m_meshData.meshCount; meshIndex++)
		{
			Mesh* mesh = m_pMesh + meshIndex;
			unsigned int vertexStride = mesh->vertexStride;
			unsigned char* meshVertexData = (m_pVertexData + mesh->vertexDataOffset);

			unsigned char* meshDeduplicatedVertexData = deduplicatedVertexData + deduplicatedVertexDataSize;
			unsigned int deduplicatedCount = 0;

			unsigned int vertexCount = mesh->vertexCount;
			uint32_t* vertexRemap = new uint32_t[vertexCount];
			memset(vertexRemap, (uint32_t)-1, sizeof(uint32_t) * vertexCount);
			assert(vertexCount <= (uint32_t)-1);

			for (unsigned int v1 = 0; v1 < vertexCount; v1++)
			{
				if (vertexRemap[v1] != (uint32_t)-1)
					continue; // this was already found to be a duplicate

				const unsigned char* v1Data = meshVertexData + v1 * vertexStride;

				// this is a new unique vertex
				uint32_t remappedSlot = deduplicatedCount++;
				vertexRemap[v1] = remappedSlot;
				memcpy(meshDeduplicatedVertexData + remappedSlot * vertexStride, v1Data, vertexStride);

				// scan for duplicates
				for (unsigned int v2 = v1 + 1; v2 < vertexCount; v2++)
				{
					if (vertexRemap[v2] != (uint32_t)-1)
						continue; // this was already found to be a duplicate of another vertex

					const unsigned char* v2Data = meshVertexData + v2 * vertexStride;

					if (0 == memcmp(v1Data, v2Data, vertexStride))
					{
						vertexRemap[v2] = remappedSlot;
					}
				}
			}

			unsigned int indexCount = mesh->indexCount;
			uint16_t* indexArray = (uint16_t*)((m_pIndexData)+mesh->indexDataOffset);
			for (unsigned int n = 0; n < indexCount; n++)
			{
				indexArray[n] = vertexRemap[indexArray[n]];
			}

			delete[] vertexRemap;

			{
				mesh->vertexCount = deduplicatedCount;
				mesh->vertexDataOffset = deduplicatedVertexDataSize;
			}
			deduplicatedVertexDataSize += deduplicatedCount * vertexStride;
		}

		{
			delete[] m_pVertexData;
			m_pVertexData = deduplicatedVertexData;
			m_meshData.vertexDataByteSize = deduplicatedVertexDataSize;
		}
	}

	void ModelImporter::LoadTextures(const std::wstring& basePath)
	{
		TextureCreationDesc texBufferCreateDesc{};

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		 
		for (UINT materialIndex = 0; materialIndex < m_meshData.materialCount; ++materialIndex)
		{
			std::wstring diffusePath = basePath + RemoveExt(m_pMaterialData[materialIndex].texDiffusePath);
			texBufferCreateDesc.texturePath = diffusePath + L".png";
			texBufferCreateDesc.isSRGB = true;
			auto diffuseTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			if (diffuseTex != nullptr)
			{
				m_pMaterialData[materialIndex].diffuseTexIndex = diffuseTex->GetResourceHeapIndex();
			}
			else
			{
				texBufferCreateDesc.texturePath = assetsPath + DefaultWhiteTexturePath;
				diffuseTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
				assert(diffuseTex != nullptr);
				m_pMaterialData[materialIndex].diffuseTexIndex = diffuseTex->GetResourceHeapIndex();
			}
			TextureManager::GetInstance().AddTextureResource(std::move(diffuseTex));

			std::wstring metallicPath = basePath + RemoveExt(m_pMaterialData[materialIndex].texMetallicPath);
			texBufferCreateDesc.texturePath = metallicPath + L".png";
			texBufferCreateDesc.isSRGB = true;
			auto metallicTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			if (metallicTex == nullptr)
			{
				texBufferCreateDesc.texturePath = RemoveLastUnderscoreAndAfter(diffusePath) + L"_Metallic.png";
				metallicTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			}
			if (metallicTex != nullptr)
			{
				m_pMaterialData[materialIndex].metallicTexIndex = metallicTex->GetResourceHeapIndex();
			}
			else
			{
				texBufferCreateDesc.texturePath = assetsPath + DefaultBlackTexturePath;
				metallicTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
				assert(metallicTex != nullptr);
				m_pMaterialData[materialIndex].metallicTexIndex = metallicTex->GetResourceHeapIndex();
			}
			TextureManager::GetInstance().AddTextureResource(std::move(metallicTex));

			std::wstring roughnessPath = basePath + RemoveExt(m_pMaterialData[materialIndex].texRoughnessPath);
			texBufferCreateDesc.texturePath = roughnessPath + L".png";
			texBufferCreateDesc.isSRGB = true;
			auto roughnessTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			if (roughnessTex == nullptr)
			{
				texBufferCreateDesc.texturePath = RemoveLastUnderscoreAndAfter(diffusePath) + L"_Roughness.png";
				roughnessTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			}
			if (roughnessTex != nullptr)
			{
				m_pMaterialData[materialIndex].roughnessTexIndex = roughnessTex->GetResourceHeapIndex();
			}
			else
			{
				texBufferCreateDesc.texturePath = assetsPath + DefaultWhiteTexturePath;
				roughnessTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
				assert(roughnessTex != nullptr);
				m_pMaterialData[materialIndex].roughnessTexIndex = roughnessTex->GetResourceHeapIndex();
			}
			TextureManager::GetInstance().AddTextureResource(std::move(roughnessTex));

			std::wstring normalPath = basePath + RemoveExt(m_pMaterialData[materialIndex].texNormalPath);
			texBufferCreateDesc.texturePath = normalPath + L".png";
			texBufferCreateDesc.isSRGB = false;
			auto normalTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			if (normalTex == nullptr)
			{
				texBufferCreateDesc.texturePath = RemoveLastUnderscoreAndAfter(diffusePath) + L"_Normal.png";
				normalTex = std::move(m_pDevice->CreateTextureFromFile(texBufferCreateDesc));
			}
			if (normalTex != nullptr)
			{
				m_pMaterialData[materialIndex].normalTexIndex = normalTex->GetResourceHeapIndex();
				m_pMaterialData[materialIndex].hasNormal = true;
			}
			else
			{
				m_pMaterialData[materialIndex].hasNormal = false;
			}
			TextureManager::GetInstance().AddTextureResource(std::move(normalTex));
		}
	}

	void ModelImporter::PrintModelStats()
	{
		printf("model stats:\n");

		AxisAlignedBox bbox = GetBoundingBox();
		printf("bounding box: <%f, %f, %f> <%f, %f, %f>\n",
			(float)bbox.GetMin().x, (float)bbox.GetMin().y, (float)bbox.GetMin().z,
			(float)bbox.GetMax().x, (float)bbox.GetMax().y, (float)bbox.GetMax().z);

		printf("vertex data size: %u\n", m_meshData.vertexDataByteSize);
		printf("index data size: %u\n", m_meshData.indexDataByteSize);
		printf("\n");

		printf("mesh count: %u\n", m_meshData.meshCount);
		for (uint32_t meshIndex = 0; meshIndex < m_meshData.meshCount; meshIndex++)
		{
			const Mesh* mesh = m_pMesh + meshIndex;

			auto printAttribFormat = [](uint32_t format) -> void
				{
					switch (format)
					{
					case attrib_format_ubyte:   printf("ubyte");    break;
					case attrib_format_byte:    printf("byte");     break;
					case attrib_format_ushort:  printf("ushort");   break;
					case attrib_format_short:   printf("short");    break;
					case attrib_format_float:   printf("float");    break;
					}
				};

			printf("mesh %u\n", meshIndex);
			printf("vertices: %u\n", mesh->vertexCount);
			printf("indices: %u\n", mesh->indexCount);
			printf("vertex stride: %u\n", mesh->vertexStride);
			for (int n = 0; n < maxAttribs; n++)
			{
				if (mesh->attrib[n].format == attrib_format_none)
					continue;

				printf("attrib %d: offset %u, normalized %u, components %u, format "
					, n, mesh->attrib[n].offset, mesh->attrib[n].normalized
					, mesh->attrib[n].components);
				printAttribFormat(mesh->attrib[n].format);
				printf("\n");
			}

		}
		printf("\n");

		printf("material count: %u\n", m_meshData.materialCount);
		for (uint32_t materialIndex = 0; materialIndex < m_meshData.materialCount; materialIndex++)
		{
			//const MaterialData* material = m_pMaterialData + materialIndex;
			printf("material %u\n", materialIndex);
		}
		printf("\n");
	}

	bool ModelImporter::CreateVertexBuffer()
	{
		BufferCreationDesc bufferCreationDesc{};
		bufferCreationDesc.m_size = m_meshData.vertexDataByteSize;
		bufferCreationDesc.m_accessFlags = BufferAccessFlags::HostWritable;
		bufferCreationDesc.m_viewFlags = GPUResourceFlags::None;
		bufferCreationDesc.m_isRawAccess = false;

		BufferManager::GetInstance().AddVertexBuffer(bufferCreationDesc);

		if ((bufferCreationDesc.m_viewFlags & GPUResourceFlags::SRV) == GPUResourceFlags::SRV
			&& (bufferCreationDesc.m_accessFlags & BufferAccessFlags::GPUOnly) == BufferAccessFlags::GPUOnly)
		{
			auto pBufferUpload = std::make_unique<DX12BufferUpload>();
			pBufferUpload->m_buffer = BufferManager::GetInstance().GetVertexBuffer();
			pBufferUpload->m_bufferData = std::make_unique<uint8_t[]>(m_meshData.vertexDataByteSize);
			pBufferUpload->m_bufferDataSize = bufferCreationDesc.m_size;

			memcpy_s(pBufferUpload->m_bufferData.get(), pBufferUpload->m_bufferDataSize, m_pIndexData, pBufferUpload->m_bufferDataSize);

			m_pDevice->GetUploadContext()->AddBufferToUploads(std::move(pBufferUpload));
		}
		else
		{
			BufferManager::GetInstance().GetVertexBuffer()->SetMappedData(m_pVertexData, m_meshData.vertexDataByteSize);

			D3D12_VERTEX_BUFFER_VIEW bufferView{};
			bufferView.BufferLocation = BufferManager::GetInstance().GetVertexBuffer()->GetGPUAddress();
			bufferView.StrideInBytes = m_vertexStride;
			bufferView.SizeInBytes = m_meshData.vertexDataByteSize;
			BufferManager::GetInstance().SetVertexBufferView(bufferView);
		}

		return BufferManager::GetInstance().GetVertexBuffer();
	}

	bool ModelImporter::CreateIndexBuffer()
	{
		BufferCreationDesc bufferCreationDesc{};
		bufferCreationDesc.m_size = m_meshData.indexDataByteSize;
		bufferCreationDesc.m_accessFlags = BufferAccessFlags::HostWritable;
		bufferCreationDesc.m_viewFlags = GPUResourceFlags::None;
		bufferCreationDesc.m_isRawAccess = false;

		BufferManager::GetInstance().AddIndexBuffer(bufferCreationDesc);

		BufferManager::GetInstance().GetIndexBuffer()->SetMappedData(m_pIndexData, m_meshData.indexDataByteSize);

		D3D12_INDEX_BUFFER_VIEW bufferView{};
		bufferView.BufferLocation = BufferManager::GetInstance().GetIndexBuffer()->GetGPUAddress();
		bufferView.Format = DXGI_FORMAT_R16_UINT;
		bufferView.SizeInBytes = m_meshData.indexDataByteSize;
		BufferManager::GetInstance().SetIndexBufferView(bufferView);

		return BufferManager::GetInstance().GetIndexBuffer();
	}

	void ModelImporter::CreateMeshRenders()
	{
		m_pMeshRender = new MeshRender[m_meshData.meshCount];

		for (UINT meshIndex = 0; meshIndex < m_meshData.meshCount; ++meshIndex)
		{
			auto pCurrMeshRender = m_pMeshRender + meshIndex;

			pCurrMeshRender->m_mesh = m_pMesh + meshIndex;

			pCurrMeshRender->m_CBVObjectParameter = std::make_unique<CBVObjectParameter>();
			pCurrMeshRender->m_CBVObjectParameter->baseColorTint = Vector3::One;
			pCurrMeshRender->m_CBVObjectParameter->ambientCubemapTint = Vector3::One;
			pCurrMeshRender->m_CBVObjectParameter->normalIntensity = 1.f;
			pCurrMeshRender->m_CBVObjectParameter->ambientCubemapIntensity = 1.f;
			pCurrMeshRender->m_CBVObjectParameter->metallicIntensity = 1.f;
			pCurrMeshRender->m_CBVObjectParameter->roughnessIntensity = 1.f;
			pCurrMeshRender->m_CBVObjectParameter->opacity = 1.f;
			pCurrMeshRender->m_CBVObjectParameter->worldMatrix = pCurrMeshRender->m_worldMatrix;

			pCurrMeshRender->m_CBVObjectParameter->baseColorTexIndex = m_pMaterialData[meshIndex].diffuseTexIndex;
			pCurrMeshRender->m_CBVObjectParameter->specularTexIndex = m_pMaterialData[meshIndex].specularTexIndex;
			pCurrMeshRender->m_CBVObjectParameter->normalTexIndex = m_pMaterialData[meshIndex].normalTexIndex;
			pCurrMeshRender->m_CBVObjectParameter->metallicTexIndex = m_pMaterialData[meshIndex].metallicTexIndex;
			pCurrMeshRender->m_CBVObjectParameter->roughnessTexIndex = m_pMaterialData[meshIndex].roughnessTexIndex;

			//pCurrMeshRender->m_CBVObjectParameter->vertexIndex = m_pBufferManager->GetVertexBuffer()->GetResourceHeapIndex();
		}
	}

}