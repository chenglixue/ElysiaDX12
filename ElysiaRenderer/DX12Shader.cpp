#include "DX12Shader.h"

namespace ElysiaRenderer
{
	std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_vertexShaders{};
	std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_pixelShaders{};
	std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>  g_computeShaders{};

	DX12Shader::DX12Shader() : 
		m_shader(nullptr)
	{

	}
	DX12Shader::DX12Shader(CComPtr<IDxcBlob> shader)
	{
		m_shader = shader;
	}

	DX12Shader::~DX12Shader()
	{
		//ElysiaHelper::SafeRelease(m_shader);
	}
}