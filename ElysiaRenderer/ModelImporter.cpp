#include "ModelImporter.h"

namespace ElysiaModel
{
	ModelImporter::ModelImporter(DX12Device* pDevice, BufferManager* pBufferManager) :
		m_pDevice(std::move(pDevice)),
		m_pBufferManager(std::move(pBufferManager))
	{

	}

	ModelImporter::~ModelImporter()
	{
	}

	bool ModelImporter::Load(const LPCWSTR& fileName)
	{
		Assimp::Importer importer;

		// remove unused data
		importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
			aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS);

		// max triangles and vertices per mesh, splits above this threshold
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, INT_MAX);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 0xfffe); // avoid the primitive restart index

		// remove points and lines
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

		// get model resource in x64
		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));
		std::wstring modelFullPath = ElysiaHelper::GetAssetFullPath(assetsPath, fileName).c_str();
		auto modelPath = std::filesystem::path(modelFullPath).string();

		const aiScene* pScene = importer.ReadFile(modelPath,
			aiProcess_CalcTangentSpace |
			//aiProcess_ConvertToLeftHanded |
			aiProcess_JoinIdenticalVertices |	// Merge same vertices
			aiProcess_Triangulate |				// translat othrer shape to triangle
			aiProcess_RemoveComponent |
			aiProcess_GenSmoothNormals |
			aiProcess_SplitLargeMeshes |
			aiProcess_ValidateDataStructure |	// Verify the data structure validity of the imported model. When loading 3D models, you may encounter format errors or data inconsistencies
			//aiProcess_ImproveCacheLocality |	// handled by optimizePostTransform()
			aiProcess_RemoveRedundantMaterials |
			aiProcess_SortByPType |				// Sort the meshes in the scene by their geometric primitive types (such as points, lines, triangles, etc.)
			aiProcess_FindInvalidData |
			aiProcess_GenUVCoords |
			aiProcess_TransformUVCoords |
			aiProcess_OptimizeMeshes |
			aiProcess_OptimizeGraph);

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
		m_pMaterial = new Material[m_meshData.materialCount];
		memset(m_pMaterial, 0, sizeof(Material) * m_meshData.materialCount);
		for (UINT materialIndex = 0; materialIndex < m_meshData.materialCount; ++materialIndex)
		{
			const auto srcMaterial = pScene->mMaterials[materialIndex];
			auto destMaterial = m_pMaterial + materialIndex;

			aiColor3D diffuse(1.0f, 1.0f, 1.0f);
			aiColor3D specular(1.0f, 1.0f, 1.0f);
			aiColor3D ambient(1.0f, 1.0f, 1.0f);
			aiColor3D emission(0.0f, 0.0f, 0.0f);
			float opacity = 1.0f;
			float shininess = 0.0f;
			float specularIntensity = 1.0f;
			aiString texDiffusePath;
			aiString texSpecularPath;
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
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), texDiffusePath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), texSpecularPath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), texEmissionPath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_AMBIENT, 0), texNormalPath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_LIGHTMAP, 0), texLightmapPath);
			srcMaterial->Get(AI_MATKEY_TEXTURE(aiTextureType_REFLECTION, 0), texReflectionPath);

			destMaterial->diffuse = Color(diffuse.r, diffuse.g, diffuse.b);
			destMaterial->specular = Color(specular.r, specular.g, specular.b);
			destMaterial->emission = Color(emission.r, emission.g, emission.b);
			destMaterial->ambient = Color(ambient.r, ambient.g, ambient.b);
			destMaterial->opacity = opacity;
			destMaterial->shininess = shininess;
			destMaterial->specularIntensity = specularIntensity;

			strncpy_s(destMaterial->texDiffusePath, texDiffusePath.C_Str(), Material::maxTexPath - 1);
			strncpy_s(destMaterial->texSpecularPath, texSpecularPath.C_Str(), Material::maxTexPath - 1);
			strncpy_s(destMaterial->texEmissionPath, texEmissionPath.C_Str(), Material::maxTexPath - 1);
			strncpy_s(destMaterial->texNormalPath, texNormalPath.C_Str(), Material::maxTexPath - 1);
			strncpy_s(destMaterial->texLightmapPath, texLightmapPath.C_Str(), Material::maxTexPath - 1);
			strncpy_s(destMaterial->texReflectionPath, texReflectionPath.C_Str(), Material::maxTexPath - 1);

			aiString materialName;
			srcMaterial->Get(AI_MATKEY_NAME, materialName);
			strncpy_s(destMaterial->name, materialName.C_Str(), Material::maxMaterialName - 1);
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
				assert(srcMesh->mFaces[f].mNumIndices == 3);

				*(destIndex++) = srcMesh->mFaces[f].mIndices[0];
				*(destIndex++) = srcMesh->mFaces[f].mIndices[1];
				*(destIndex++) = srcMesh->mFaces[f].mIndices[2];
			}
		}

		ComputeAllBoundingBoxes();

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
			//const Material* material = m_pMaterial + materialIndex;
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

		m_pBufferManager->AddVertexBuffer(bufferCreationDesc);

		if ((bufferCreationDesc.m_viewFlags & GPUResourceFlags::SRV) == GPUResourceFlags::SRV
			&& (bufferCreationDesc.m_accessFlags & BufferAccessFlags::GPUOnly) == BufferAccessFlags::GPUOnly)
		{
			auto pBufferUpload = std::make_unique<DX12BufferUpload>();
			pBufferUpload->m_buffer = m_pBufferManager->GetVertexBuffer();
			pBufferUpload->m_bufferData = std::make_unique<uint8_t[]>(m_meshData.vertexDataByteSize);
			pBufferUpload->m_bufferDataSize = bufferCreationDesc.m_size;

			memcpy_s(pBufferUpload->m_bufferData.get(), pBufferUpload->m_bufferDataSize, m_pIndexData, pBufferUpload->m_bufferDataSize);

			m_pDevice->GetUploadContext()->AddBufferToUploads(std::move(pBufferUpload));
		}
		else
		{
			m_pBufferManager->GetVertexBuffer()->SetMappedData(m_pVertexData, m_meshData.vertexDataByteSize);

			D3D12_VERTEX_BUFFER_VIEW bufferView{};
			bufferView.BufferLocation = m_pBufferManager->GetVertexBuffer()->GetGPUAddress();
			bufferView.StrideInBytes = m_vertexStride;
			bufferView.SizeInBytes = m_meshData.vertexDataByteSize;
			m_pBufferManager->SetVertexBufferView(bufferView);
		}

		return m_pBufferManager->GetVertexBuffer();
	}

	bool ModelImporter::CreateIndexBuffer()
	{
		BufferCreationDesc bufferCreationDesc{};
		bufferCreationDesc.m_size = m_meshData.indexDataByteSize;
		bufferCreationDesc.m_accessFlags = BufferAccessFlags::HostWritable;
		bufferCreationDesc.m_viewFlags = GPUResourceFlags::None;
		bufferCreationDesc.m_isRawAccess = false;

		m_pBufferManager->AddIndexBuffer(bufferCreationDesc);

		m_pBufferManager->GetIndexBuffer()->SetMappedData(m_pIndexData, m_meshData.indexDataByteSize);

		D3D12_INDEX_BUFFER_VIEW bufferView{};
		bufferView.BufferLocation = m_pBufferManager->GetIndexBuffer()->GetGPUAddress();
		bufferView.Format = DXGI_FORMAT_R16_UINT;
		bufferView.SizeInBytes = m_meshData.indexDataByteSize;
		m_pBufferManager->SetIndexBufferView(bufferView);

		return m_pBufferManager->GetIndexBuffer();
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
			pCurrMeshRender->m_CBVObjectParameter->vertexIndex = m_pBufferManager->GetVertexBuffer()->GetResourceHeapIndex();
		}
	}
}