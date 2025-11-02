#include "stdafx.h"
#include "ShaderManager.h"

#include "DX12Device.h"

namespace ElysiaRenderer
{
	std::unique_ptr<ShaderManager> g_pShaderManager = nullptr;

	ShaderManager::~ShaderManager()
	{

	}

	void ShaderManager::Init()
	{
		AddShader(ShaderQueue::Blit, L"Shaders\\public\\FullScreenTriangle.hlsl", L"VS", ShaderType::Vertex);
		AddShader(ShaderQueue::Blit, L"Shaders\\public\\Blit.hlsl", L"PS", ShaderType::Pixel);

	}

	void ShaderManager::Destory()
	{

	}
}