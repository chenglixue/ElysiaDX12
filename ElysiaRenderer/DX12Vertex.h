#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace DirectX;

	struct DX12Vertex
	{
		Vector3 m_position{};
		Vector3 m_uv{};
		Vector3 m_normal{};
		Vector3 m_tangent{};
		//XMFLOAT3 m_color{};
	};
}