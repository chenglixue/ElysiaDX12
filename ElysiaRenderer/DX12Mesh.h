#pragma once
#include "stdafx.h"
#include "DX12Vertex.h"

namespace ElysiaRenderer
{
	struct DX12Mesh
	{
		DX12Mesh() = default;
		DX12Mesh(const std::vector<DX12Vertex>& vertices, const std::vector<UINT>& indices)
		{
			this->m_vertices = vertices;
			this->m_indices = indices;
		}
		DX12Mesh(DX12Mesh&& rhs) = default;
		DX12Mesh(const DX12Mesh& rhs) = delete;
		DX12Mesh operator=(const DX12Mesh& rhs) = delete;
		~DX12Mesh()
		{
			m_vertices.clear();
			m_indices.clear();
		}

		std::vector<DX12Vertex> m_vertices{};
		std::vector<UINT> m_indices{};
	};


}