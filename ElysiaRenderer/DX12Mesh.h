#pragma once
#include "stdafx.h"
#include "DX12Vertex.h"
#include "DX12Material.h"

namespace ElysiaRenderer
{
	struct DX12Mesh
	{
		DX12Mesh() = default;
		DX12Mesh(std::vector<DX12Vertex>& vertices, 
			std::vector<UINT>& indices, 
			std::vector<DX12Material*>& materials)
		{
			this->m_vertices = std::move(vertices);
			this->m_indices = indices;
			this->m_materials = std::move(materials);
		}

		DX12Mesh(DX12Mesh&& rhs) = default;
		DX12Mesh& operator=(DX12Mesh&& rhs) noexcept
		{
			if (this != &rhs)
			{
				m_vertices = std::move(rhs.m_vertices);
				m_indices = rhs.m_indices;
				m_materials.resize(rhs.m_materials.size());
				for (int i = 0; i < rhs.m_materials.size(); ++i)
				{
					m_materials[i] = std::move(rhs.m_materials[i]);
				}
			}

			return *this;
		}

		DX12Mesh(const DX12Mesh& rhs) = default;
		DX12Mesh& operator=(const DX12Mesh& rhs) = default;

		~DX12Mesh()
		{
			m_vertices.clear();
			m_indices.clear();
		}

		std::string m_name;
		std::vector<DX12Vertex> m_vertices{};
		std::vector<UINT> m_indices{};
		std::vector<DX12Material*> m_materials{};

		UINT m_currStartVertex = 0;
		UINT m_currStartIndex = 0;
		UINT m_indexCount = 0;
	};


}