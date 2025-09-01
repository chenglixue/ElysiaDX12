#include "DX12Model.h"

namespace ElysiaRenderer
{
	DX12Model::DX12Model(const LPCWSTR& path)
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

	DX12Model::~DX12Model()
	{
		m_meshs.clear();
	}

	std::vector<DX12Vertex> DX12Model::GetVertices()
	{
		std::vector<DX12Vertex> localVertices;

		for (auto& mesh : m_meshs)
		{
			localVertices.insert(localVertices.end(), mesh.m_vertices.begin(), mesh.m_vertices.end());
		}

		return localVertices;
	}

	std::vector<UINT> DX12Model::GetIndices()
	{
		std::vector<uint32_t> localIndices;

		for (auto& mesh : m_meshs)
		{
			localIndices.insert(localIndices.end(), mesh.m_indices.begin(), mesh.m_indices.end());
		}

		return localIndices;
	}

	void DX12Model::TraverseNode(const aiScene* scene, aiNode* node)
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

	void DX12Model::LoadMesh(const aiScene* scene, aiMesh* mesh)
	{
		std::vector<DX12Vertex> localVertices;
		std::vector<UINT> localIndices;
		std::vector<DX12Material*> m_localMaterials{};

		// process vertex position, normal, tangent, texture coordinates
		for (UINT i = 0; i < mesh->mNumVertices; ++i)
		{
			DX12Vertex localVertex = DX12Vertex();

			localVertex.m_position.x = mesh->mVertices[i].x;
			localVertex.m_position.y = mesh->mVertices[i].y;
			localVertex.m_position.z = mesh->mVertices[i].z;

			localVertex.m_normal.x = mesh->mNormals[i].x;
			localVertex.m_normal.y = mesh->mNormals[i].y;
			localVertex.m_normal.z = mesh->mNormals[i].z;

			localVertex.m_tangent.x = mesh->mTangents[i].x;
			localVertex.m_tangent.y = mesh->mTangents[i].y;
			localVertex.m_tangent.z = mesh->mTangents[i].z;

			// assimp allow one model have 8 different texture coordinates in one vertex, but we just care first texture coordinates because we will not use so many
			if (mesh->mTextureCoords[0])
			{
				localVertex.m_uv.x = mesh->mTextureCoords[0][i].x;
				localVertex.m_uv.y = mesh->mTextureCoords[0][i].y;
			}
			else
			{
				localVertex.m_uv = XMFLOAT2(0.0f, 0.0f);
			}

			localVertices.emplace_back(std::move(localVertex));
		}

		for (UINT i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace localFace = mesh->mFaces[i];
			for (UINT j = 0; j < localFace.mNumIndices; ++j)
			{
				localIndices.emplace_back(std::move(localFace.mIndices[j]));
			}
		}

		for (UINT i = 0; i < mesh->mMaterialIndex; ++i)
		{
			auto material = scene->mMaterials[mesh->mMaterialIndex];

			auto type = aiTextureType_DIFFUSE;
			for (UINT i = 0; i < material->GetTextureCount(type); ++i)
			{
				aiString path;
				material->GetTexture(type, i, &path);

				DX12Material* currMaterial = new DX12Material();
				currMaterial->m_material = material;
				
				auto texData = new LoadTexData();
				texData->m_path = path.C_Str();
				switch (type)
				{
					case aiTextureType_DIFFUSE:
					{
						texData->m_texType = LoadTexType::Albedo;
						break;
					}
					case aiTextureType_NORMALS:
					{
						texData->m_texType = LoadTexType::Normal;
						break;
					}
					default:
					{
						ElysiaHelper::ThrowRuntimeError("Load invalild tex type");
						break;
					}
				}
				currMaterial->m_texData.push_back(std::move(texData));

				m_localMaterials.emplace_back(std::move(currMaterial));
			}
		}

		m_drawIndexCount = localIndices.size();

		DX12Mesh resultMesh(localVertices, localIndices, m_localMaterials);
		resultMesh.m_name = mesh->mName.C_Str();
		resultMesh.m_indexCount = m_drawIndexCount;
		resultMesh.m_currStartIndex = m_startIndex;
		resultMesh.m_currStartVertex = m_startVertex;

		m_startIndex += m_drawIndexCount;
		m_startVertex += resultMesh.m_vertices.size();

		m_meshs.emplace_back(std::move(resultMesh));
	}
}