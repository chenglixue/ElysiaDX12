#pragma once
#include "stdafx.h"
//#include "DX12BufferResource.h"

namespace ElysiaRenderer
{
	using namespace DirectX;
	using namespace DirectX::SimpleMath;

	struct Mesh;
	struct CBVObjectParameter;
	class DX12BufferResource;

	struct MeshRender
	{
		Matrix	m_worldMatrix	= Matrix::Identity;
		Mesh*	m_mesh			= nullptr;

		std::unique_ptr<CBVObjectParameter> m_CBVObjectParameter = nullptr;
		std::array<std::unique_ptr<DX12BufferResource>, NUM_FRAMES_IN_FLIGHT> m_objectBuffers{};
		std::array< std::unique_ptr<DX12BufferResource>, NUM_FRAMES_IN_FLIGHT> m_materialBuffers{};
	};
}