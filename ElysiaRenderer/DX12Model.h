#pragma once
#include "stdafx.h"
#include "DX12Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace ElysiaRenderer
{
	class DX12Model
	{
	public:
		DX12Model() = default;
		DX12Model(const std::string& path);
		DX12Model(const DX12Model& rhs) = delete;
		DX12Model operator=(const DX12Model& rhs) = delete;
		DX12Model(DX12Model&& rhs) = default;

		std::vector<DX12Vertex> GetVertices();
		std::vector<UINT> GetIndices();

		// Traverse and process the nodes in assimp in turn
		void TraverseNode(const aiScene* scene, aiNode* node);
		// load mesh, which includes vertex, index, normal, tangent, texture, material information
		DX12Mesh LoadMesh(const aiScene* scene, aiMesh* mesh);


	private:
		std::string m_directory;
		std::vector<DX12Mesh> m_meshs;
	};


}