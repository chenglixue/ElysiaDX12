#include "stdafx.h"
#include "DX12Shader.h"

#include <d3d12shader.h>    // Shader reflection.

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

	CComPtr<IDxcBlob>& DX12Shader::GetShader()
	{
		return m_shader;
	}

	void DX12Shader::SetVariable(const std::vector<ShaderVariable>& shaderVariables)
	{
		m_variables = shaderVariables;
	}

	const std::vector<ShaderVariable>& DX12Shader::GetVariable() const noexcept
	{
		return m_variables;
	}

	void DX12Shader::SetInputElementSemanticNames(const std::vector<std::string>& inputElementSemanticNames)
	{
		m_inputElementSemanticNames = inputElementSemanticNames;
	}
}