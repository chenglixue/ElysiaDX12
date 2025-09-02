#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace DirectX;

	struct DX12Vertex
	{
		DX12Vertex() = default;
		DX12Vertex(XMFLOAT3 position, XMFLOAT3 color = XMFLOAT3(0.f, 0.f, 0.f),
			XMFLOAT2 uv = XMFLOAT2(0.f, 0.f),
			XMFLOAT3 normal = XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3 tangent = XMFLOAT3(0.f, 0.f, 0.f))
			: m_position(position)
		{
		}
		DX12Vertex(const DX12Vertex& rhs) = default;
		DX12Vertex& operator=(DX12Vertex& rhs) = default;
		DX12Vertex(DX12Vertex&& rhs) noexcept = default;
		DX12Vertex& operator=(DX12Vertex&& rhs) noexcept
		{
			if (this != &rhs)
			{
				this->m_position = rhs.m_position;
				this->m_color = rhs.m_color;
				this->m_uv = rhs.m_uv;
				this->m_normal = rhs.m_normal;
				this->m_tangent = rhs.m_tangent;
			}
			return *this;
		}
		~DX12Vertex() = default;
		
		XMFLOAT3 m_position{};
		XMFLOAT3 m_color{};
		XMFLOAT2 m_uv{};
		XMFLOAT3 m_normal{};
		XMFLOAT3 m_tangent{};
	};
}