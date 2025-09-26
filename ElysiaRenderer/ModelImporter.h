#pragma once
#include "stdafx.h"
#include "Mesh.h"
#include "Material.h"
#include "DX12MeshRender.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


namespace ElysiaModel
{
	using namespace ElysiaRenderer;

	class ModelImporter
	{
	public:
		ModelImporter() = default;
		ModelImporter(const LPCWSTR& path);
		ModelImporter(const ModelImporter& rhs) = delete;
		ModelImporter& operator=(const ModelImporter& rhs) = delete;
		ModelImporter(ModelImporter&& rhs) = default;
		~ModelImporter();

		std::vector<Mesh> GetMeshs()
		{
			return m_meshs;
		}

		// Traverse and process the nodes in assimp in turn
		void TraverseNode(const aiScene* scene, aiNode* node);
		// load mesh, which includes vertex, index, normal, tangent, texture, material information
		void LoadMesh(const aiScene* scene, aiMesh* mesh);
		bool Load(const LPCWSTR& fileName);
		bool Load(const std::vector<LPCWSTR>& fileNames);

	private:
		std::string m_directory;
		std::vector<Mesh> m_meshs{};

		MeshData m_meshData{};
		Material* m_pMaterial = nullptr;
		Mesh* m_pMesh = nullptr;

		unsigned char* m_pVertexData;
		unsigned char* m_pIndexData;
	};


}