#include "ModelImporter.h"
#include <string.h>

namespace ElysiaModel
{


	ModelImporter::ModelImporter(const LPCWSTR& path)
	{
		Assimp::Importer localImporter;

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

		std::wstring modelFullPath = ElysiaHelper::GetAssetFullPath(assetsPath, path).c_str();
		auto modelPath = std::filesystem::path(modelFullPath).string();

		const aiScene* pLocalScene = localImporter.ReadFile(
			modelPath,
			// Triangulates all faces of all meshes
			aiProcess_Triangulate |
			// Supersedes the aiProcess_MakeLeftHanded and aiProcess_FlipUVs and aiProcess_FlipWindingOrder flags
			aiProcess_ConvertToLeftHanded |
			// This preset enables almost every optimization step to achieve perfectly optimized data. In D3D, need combine with aiProcess_ConvertToLeftHanded
			aiProcessPreset_TargetRealtime_MaxQuality |
			// Calculates the tangents and bitangents for the imported meshes
			aiProcess_CalcTangentSpace |
			// Splits large meshes into smaller sub-meshes
			// This is quite useful for real-time rendering, 
			// where the number of triangles which can be maximally processed in a single draw - call is limited by the video driver / hardware
			aiProcess_SplitLargeMeshes |
			// A postprocessing step to reduce the number of meshes
			aiProcess_OptimizeMeshes |
			// A postprocessing step to optimize the scene hierarchy
			aiProcess_OptimizeGraph
		);

		// "localScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE" is used to check whether value data returned is incomplete
		if (pLocalScene == nullptr || pLocalScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || pLocalScene->mRootNode == nullptr)
		{
			std::stringstream ss;
			ss << "ERROR::ASSIMP::" << localImporter.GetErrorString();
			ElysiaHelper::ThrowRuntimeError(ss.str());
		}

		m_directory = modelPath.substr(0, modelPath.find_last_of('/'));

		TraverseNode(pLocalScene, pLocalScene->mRootNode);
	}

	ModelImporter::~ModelImporter()
	{
		m_meshs.clear();
	}

	void ModelImporter::TraverseNode(const aiScene* scene, aiNode* node)
	{
		// load mesh
		for (UINT i = 0; i < node->mNumMeshes; ++i)
		{
			aiMesh* pLocalMesh = scene->mMeshes[node->mMeshes[i]];
			LoadMesh(scene, pLocalMesh);
		}

		// traverse child node
		for (UINT i = 0; i < node->mNumChildren; ++i)
		{
			TraverseNode(scene, node->mChildren[i]);
		}
	}

	void ModelImporter::LoadMesh(const aiScene* scene, aiMesh* mesh)
	{
		//std::vector<DX12Vertex> localVertices;
		//std::vector<UINT> localIndices;
		//std::vector<DX12Material*> m_localMaterials{};

		//// process vertex position, normal, tangent, texture coordinates
		//for (UINT i = 0; i < mesh->mNumVertices; ++i)
		//{
		//	DX12Vertex localVertex = DX12Vertex();

		//	localVertex.m_position.x = mesh->mVertices[i].x;
		//	localVertex.m_position.y = mesh->mVertices[i].y;
		//	localVertex.m_position.z = mesh->mVertices[i].z;

		//	localVertex.m_normal.x = mesh->mNormals[i].x;
		//	localVertex.m_normal.y = mesh->mNormals[i].y;
		//	localVertex.m_normal.z = mesh->mNormals[i].z;

		//	localVertex.m_tangent.x = mesh->mTangents[i].x;
		//	localVertex.m_tangent.y = mesh->mTangents[i].y;
		//	localVertex.m_tangent.z = mesh->mTangents[i].z;

		//	// assimp allow one model have 8 different texture coordinates in one vertex, but we just care first texture coordinates because we will not use so many
		//	if (mesh->mTextureCoords[0])
		//	{
		//		localVertex.m_uv.x = mesh->mTextureCoords[0][i].x;
		//		localVertex.m_uv.y = mesh->mTextureCoords[0][i].y;
		//	}
		//	else
		//	{
		//		localVertex.m_uv = XMFLOAT2(0.0f, 0.0f);
		//	}

		//	localVertices.emplace_back(std::move(localVertex));
		//}

		//for (UINT i = 0; i < mesh->mNumFaces; ++i)
		//{
		//	aiFace localFace = mesh->mFaces[i];
		//	for (UINT j = 0; j < localFace.mNumIndices; ++j)
		//	{
		//		localIndices.emplace_back(std::move(localFace.mIndices[j]));
		//	}
		//}

		//for (UINT i = 0; i < mesh->mMaterialIndex; ++i)
		//{
		//	auto material = scene->mMaterials[mesh->mMaterialIndex];

		//	auto type = aiTextureType_DIFFUSE;
		//	for (UINT i = 0; i < material->GetTextureCount(type); ++i)
		//	{
		//		aiString path;
		//		material->GetTexture(type, i, &path);

		//		DX12Material* currMaterial = new DX12Material();
		//		currMaterial->m_material = material;
		//		
		//		auto texData = new LoadTexData();
		//		texData->m_path = path.C_Str();
		//		switch (type)
		//		{
		//			case aiTextureType_DIFFUSE:
		//			{
		//				texData->m_texType = LoadTexType::Albedo;
		//				break;
		//			}
		//			case aiTextureType_NORMALS:
		//			{
		//				texData->m_texType = LoadTexType::Normal;
		//				break;
		//			}
		//			default:
		//			{
		//				ElysiaHelper::ThrowRuntimeError("Load invalild tex type");
		//				break;
		//			}
		//		}
		//		currMaterial->m_texData.push_back(std::move(texData));

		//		m_localMaterials.emplace_back(std::move(currMaterial));
		//	}
		//}

		//m_drawIndexCount = localIndices.size();

		//DX12Mesh resultMesh(localVertices, localIndices, m_localMaterials);
		//resultMesh.m_name = mesh->mName.C_Str();
		//resultMesh.m_indexCount = m_drawIndexCount;
		//resultMesh.m_currStartIndex = m_startIndex;
		//resultMesh.m_currStartVertex = m_startVertex;

		//m_startIndex += m_drawIndexCount;
		//m_startVertex += resultMesh.m_vertices.size();

		//m_meshs.emplace_back(std::move(resultMesh));
	}

	bool ModelImporter::Load(const LPCWSTR& fileName)
	{
		Assimp::Importer importer;

		// remove unused data:color,light,camera
		importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
			aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS);

		// set max triangles and vertices per mesh, splits above this threshold
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
			aiProcess_CalcTangentSpace	|
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
		for (UINT meshIndex = 0; meshIndex < m_meshData.meshCount; ++meshIndex)
		{
			const auto srcMesh = pScene->mMeshes[meshIndex];
			auto destMesh = m_pMesh + meshIndex;

			assert(srcMesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);

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

			destMesh->attribsEnabled |= attrib_mask_bitangent;
			destMesh->attrib[attrib_bitangent].offset = destMesh->vertexStride;
			destMesh->attrib[attrib_bitangent].normalized = 0;
			destMesh->attrib[attrib_bitangent].components = 3;
			destMesh->attrib[attrib_bitangent].format = attrib_format_float;
			destMesh->vertexStride += sizeof(float) * destMesh->attrib[attrib_bitangent].components;

			destMesh->vertexDataOffset = m_meshData.vertexDataByteSize;
			destMesh->vertexCount = srcMesh->mNumVertices;

			destMesh->indexDataOffset = m_meshData.indexDataByteSize;
			destMesh->indexCount = srcMesh->mNumFaces * 3;

			m_meshData.vertexDataByteSize += destMesh->vertexStride * destMesh->vertexCount;
			m_meshData.indexDataByteSize += sizeof(UINT16) * destMesh->indexCount;
		}

		m_pVertexData = new unsigned char[m_meshData.vertexDataByteSize];
		m_pIndexData = new unsigned char[m_meshData.indexDataByteSize];

		for (UINT meshIndex = 0; meshIndex < m_meshData.meshCount; ++meshIndex)
		{
			const auto srcMesh = pScene->mMeshes[meshIndex];
			auto destMesh = m_pMesh + meshIndex;

			float* destPos = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_position].offset);
			float* destTexcoord0 = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_texcoord0].offset);
			float* destNormal = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_normal].offset);
			float* destTangent = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_tangent].offset);
			float* destBitangent = (float*)(m_pVertexData + destMesh->vertexDataOffset + destMesh->attrib[attrib_bitangent].offset);

			for (UINT v = 0; v < destMesh->vertexCount; ++v)
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
				destPos = (float*)((unsigned char*)destPos + destMesh->vertexStride);

				if(srcMesh->)
			}
		}
	}

	bool ModelImporter::Load(const std::vector<LPCWSTR>& fileNames)
	{
		bool isLoadSuccess = true;
		for (auto fileName : fileNames)
		{
			isLoadSuccess &= Load(fileName);
		}

		return isLoadSuccess;
	}
}