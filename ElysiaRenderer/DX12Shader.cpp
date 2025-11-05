#include "stdafx.h"
#include "DX12Shader.h"

#include <d3d12shader.h>    // Shader reflection.

namespace ElysiaRenderer
{
	std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_vertexShaders{};
	std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_pixelShaders{};
	std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>  g_computeShaders{};

	DX12Shader::DX12Shader() : 
		m_shader(nullptr),
		m_constantBufferVariables(std::unordered_map<std::string, ShaderConstantVariableDesc>()),
		m_pipelineResourceLayout(PipelineResourceLayout())
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

	void DX12Shader::SetInputLayoutDesc(const D3D12_INPUT_LAYOUT_DESC& inputLayoutDesc)
	{
		m_inputLayoutDesc = inputLayoutDesc;
	}
	const D3D12_INPUT_LAYOUT_DESC& DX12Shader::GetInputElementDesc() const noexcept
	{
		return m_inputLayoutDesc;
	}

	void DX12Shader::SetConstantBufferVariable(const std::string& name, const ShaderConstantVariableDesc& desc)
	{
		m_constantBufferVariables[name] = desc;
	}
	const std::unordered_map<std::string, ShaderConstantVariableDesc>& DX12Shader::GetConstantBufferVariables() const noexcept
	{
		return m_constantBufferVariables;
	}
}