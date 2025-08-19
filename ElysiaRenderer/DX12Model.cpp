#include "DX12Model.h"

namespace ElysiaRenderer
{
	DX12Model::DX12Model(const std::string& path)
	{
		Assimp::Importer localImporter;

		const aiScene* pLocalScene = localImporter.ReadFile(
			path,
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
			aiProcess_OptimizeGraph);

		// "localScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE" is used to check whether value data returned is incomplete
		if (pLocalScene == nullptr || pLocalScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || pLocalScene->mRootNode == nullptr)
		{
			std::stringstream ss;
			ss << "ERROR::ASSIMP::" << localImporter.GetErrorString();
			ElysiaHelper::ThrowRuntimeError(ss.str());
		}

		m_directory = path.substr(0, path.find_last_of('/'));

		TraverseNode(pLocalScene, pLocalScene->mRootNode);
	}

	std::vector<DX12Vertex> DX12Model::GetVertices()
	{
		std::vector<DX12Vertex> localVertices;

		for (auto& mesh : m_meshs)
		{
			for (auto& v : mesh.m_vertices)
			{
				localVertices.push_back(v);
			}
		}

		return localVertices;
	}

	std::vector<UINT> DX12Model::GetIndices()
	{
		std::vector<uint32_t> localIndices;

		for (auto& mesh : m_meshs)
		{
			for (auto& i : mesh.m_indices)
			{
				localIndices.push_back(i);
			}
		}

		return localIndices;
	}

	void DX12Model::TraverseNode(const aiScene* scene, aiNode* node)
	{
		// load mesh
		for (UINT i = 0; i < node->mNumMeshes; ++i)
		{
			aiMesh* pLocalMesh = scene->mMeshes[node->mMeshes[i]];
			m_meshs.push_back(LoadMesh(scene, pLocalMesh));
		}

		// traverse child node
		for (UINT i = 0; i < node->mNumChildren; ++i)
		{
			TraverseNode(scene, node->mChildren[i]);
		}
	}

	DX12Mesh DX12Model::LoadMesh(const aiScene* scene, aiMesh* mesh)
	{
		std::vector<DX12Vertex> localVertices;
		std::vector<uint32_t> localIndices;

		// process vertex position, normal, tangent, texture coordinates
		for (UINT i = 0; i < mesh->mNumVertices; ++i)
		{
			DX12Vertex localVertex = DX12Vertex();

			localVertex.m_position.x = mesh->mVertices[i].x;
			localVertex.m_position.y = mesh->mVertices[i].y;
			localVertex.m_position.z = mesh->mVertices[i].z;

			//localVertex.m_normal.x = mesh->mNormals[i].x;
			//localVertex.m_normal.y = mesh->mNormals[i].y;
			//localVertex.m_normal.z = mesh->mNormals[i].z;

			//localVertex.m_tangent.x = mesh->mTangents[i].x;
			//localVertex.m_tangent.y = mesh->mTangents[i].y;
			//localVertex.m_tangent.z = mesh->mTangents[i].z;

			//// assimp allow one model have 8 different texture coordinates in one vertex, but we just care first texture coordinates because we will not use so many
			//if (mesh->mTextureCoords[0])
			//{
			//	localVertex.m_uv.x = mesh->mTextureCoords[0][i].x;
			//	localVertex.m_uv.y = mesh->mTextureCoords[0][i].y;
			//}
			//else
			//{
			//	localVertex.m_uv = XMFLOAT2(0.0f, 0.0f);
			//}

			localVertices.push_back(localVertex);
		}

		for (UINT i = 0; i < mesh->mNumFaces; ++i)
		{
			aiFace localFace = mesh->mFaces[i];
			for (UINT j = 0; j < localFace.mNumIndices; ++j)
			{
				localIndices.push_back(localFace.mIndices[j]);
			}
		}

		return DX12Mesh(localVertices, localIndices);
	}

}