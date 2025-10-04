#pragma once
#include "stdafx.h"
#include "Mesh.h"
#include "CBVParameter.h"

namespace ElysiaRenderer
{
	using namespace DirectX;
	using namespace DirectX::SimpleMath;

	struct MeshRender
	{
		Matrix	m_worldMatrix	= Matrix::Identity;
		Mesh*	m_mesh			= nullptr;

		std::unique_ptr<CBVObjectParameter> m_CBVObjectParameter = nullptr;
	};
}