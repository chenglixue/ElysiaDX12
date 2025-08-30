#pragma once
#include "stdafx.h"
#include "DX12Mesh.h"
#include "DX12MeshRender.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


namespace ElysiaRenderer
{
	class DX12Model
	{
	public:
		DX12Model() = default;
		DX12Model(const LPCWSTR& path);
		DX12Model(const DX12Model& rhs) = default;
		DX12Model& operator=(const DX12Model& rhs) = default;
		DX12Model(DX12Model&& rhs) = default;
		~DX12Model();

		std::vector<DX12Vertex> GetVertices();
		std::vector<UINT> GetIndices();
		std::vector<DX12Mesh> GetMeshs()
		{
			return m_meshs;
		}
		const UINT& GetVertexOffset()
		{
			return m_startVertex;
		}
		const UINT& GetIndexOffset()
		{
			return m_startIndex;
		}
		const UINT& GetIndexCount()
		{
			return m_drawIndexCount;
		}
		void SetDrawIndexCount(UINT drawIndexCount)
		{
			m_drawIndexCount = drawIndexCount;
		}
		void SetVertexOffset(UINT startVertex)
		{
			m_startVertex = startVertex;
		}
		void SetIndexOffset(UINT startIndex)
		{
			m_startIndex = startIndex;
		}

		// Traverse and process the nodes in assimp in turn
		void TraverseNode(const aiScene* scene, aiNode* node);
		// load mesh, which includes vertex, index, normal, tangent, texture, material information
		void LoadMesh(const aiScene* scene, aiMesh* mesh);

	private:
		std::string m_directory;
		std::vector<DX12Mesh> m_meshs;

		UINT m_startVertex = 0;
		UINT m_startIndex = 0;
		UINT m_drawIndexCount = 0;
	};


}