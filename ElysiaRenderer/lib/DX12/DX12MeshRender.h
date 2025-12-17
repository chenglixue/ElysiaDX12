#pragma once
#include "stdafx.h"

namespace ElysiaRenderer
{
	using namespace DirectX;
	using namespace DirectX::SimpleMath;

	struct LoadedModel;
	struct CBVObjectParameter;
	class DX12BufferResource;

	struct MeshRender
	{
		Matrix	m_worldMatrix	= Matrix::Identity;
		LoadedModel*	m_mesh			= nullptr;

		std::unique_ptr<CBVObjectParameter> m_CBVObjectParameter = nullptr;
	};
}