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
		DX12Vertex(const DX12Vertex& rhs)
		{
			/*m_position = rhs.m_position;
			m_color = rhs.m_color;
			m_uv = rhs.m_uv;
			m_normal = rhs.m_normal;
			m_tangent = rhs.m_tangent;*/
		}
		DX12Vertex& operator=(DX12Vertex& rhs)
		{
			return rhs;
		}
		DX12Vertex(DX12Vertex&& rhs) = default;
		~DX12Vertex() = default;
		
		XMFLOAT3 m_position;
		//float padding0;
		/*XMFLOAT3 m_color;
		float padding1;
		XMFLOAT2 m_uv;
		XMFLOAT2 padding2;
		XMFLOAT3 m_normal;
		float padding3;
		XMFLOAT3 m_tangent;
		float padding4;*/
	};
}