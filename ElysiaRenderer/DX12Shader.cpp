#include "DX12Shader.h"

namespace ElysiaRenderer
{
	DX12Shader::DX12Shader() : 
		m_shader(nullptr)
	{

	}
	DX12Shader::DX12Shader(ID3DBlob* shader)
	{
		m_shader = shader;
	}

	DX12Shader::~DX12Shader()
	{
		ElysiaHelper::SafeRelease(m_shader);
	}
}