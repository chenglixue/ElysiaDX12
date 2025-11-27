#include "stdafx.h"
#include "DX12Shader.h"
#include "DX12PipelineState.h"
#include <d3d12shader.h>    // Shader reflection.

namespace ElysiaRenderer
{
	DX12Shader::DX12Shader() : 
		m_shader(nullptr),
		m_constantBufferVariables(std::unordered_map<std::string, ShaderConstantVariableDesc>())
	{
	}
	DX12Shader::DX12Shader(CComPtr<IDxcBlob> shader) : 
		m_shader(shader)
	{
	}

	DX12Shader::~DX12Shader()
	{
		m_constantBufferVariables.clear();
	}

	CComPtr<IDxcBlob>& DX12Shader::GetShader()
	{
		return m_shader;
	}

	void DX12Shader::SetVariable(const std::vector<ShaderVariable> shaderVariables)
	{
		m_variables = shaderVariables;
	}
	const std::vector<ShaderVariable>& DX12Shader::GetVariable() const noexcept
	{
		return m_variables;
	}


	void DX12Shader::SetInputElementData(const std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementData)
	{
		m_inputElementData = inputElementData;

		m_inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC
		{
			.pInputElementDescs = m_inputElementData.data(),
			.NumElements = static_cast<UINT32>(m_inputElementData.size()),
		};
	}
	void DX12Shader::SetInputElementSemanticNames(const std::vector <std::string> inputElementSemanticNames)
	{
		m_inputElementSemanticNames = inputElementSemanticNames;
	}
	const std::vector <std::string>& DX12Shader::GetInputElementSemanticNames() const noexcept
	{
		return m_inputElementSemanticNames;
	}
	const D3D12_INPUT_LAYOUT_DESC& DX12Shader::GetInputElementDesc() const noexcept
	{
		return m_inputLayoutDesc;
	}

	void DX12Shader::SetConstantBufferVariable(const std::string name, const ShaderConstantVariableDesc&& desc)
	{
		m_constantBufferVariables[name] = desc;
	}
	std::unordered_map<std::string, ShaderConstantVariableDesc>& DX12Shader::GetConstantBufferVariables() noexcept
	{
		return m_constantBufferVariables;
	}
}