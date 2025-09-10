#pragma once
#include "stdafx.h"
#include "DX12Model.h"

namespace ElysiaRenderer
{
	using namespace DirectX;

	struct DX12MeshRender
	{
		DX12MeshRender() = default;
		~DX12MeshRender()
		{
			if (m_mesh)
			{
				delete m_mesh;
				m_mesh = nullptr;
			}
		}
		DX12MeshRender(DX12MeshRender&& rhs) = default;
		DX12MeshRender(const DX12MeshRender& rhs) = delete;
		DX12MeshRender operator=(const DX12MeshRender& rhs) = delete;

		XMFLOAT4X4	m_worldMatrix = ElysiaHelper::MathHelper::Identity4x4();
		DX12Mesh*	m_mesh			= nullptr;
		UINT		m_meshIndex		= 0;
		UINT		m_startVertex	= 0;
		UINT		m_indexCount	= 0;
		UINT		m_startIndex	= 0;
	};
}