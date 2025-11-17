#pragma once
#include "MObject.h"
#include "PipelineResourceUtility.h"
#include "ShaderUtility.h"
#include "BufferUtility.h"

namespace ElysiaRenderer
{
	struct PipelineStateObject;
	
	class DX12Shader : MObject
	{
	public:
		DX12Shader();
		DX12Shader(CComPtr<IDxcBlob> shader);
		~DX12Shader();

		CComPtr<IDxcBlob>& GetShader();

		void SetVariable(const std::vector<ShaderVariable> shaderVariables);
		const std::vector<ShaderVariable>& GetVariable() const noexcept;

		void SetInputElementData(const std::vector<D3D12_INPUT_ELEMENT_DESC>);
		void SetInputElementSemanticNames(const std::vector <std::string>);
		const std::vector <std::string>& GetInputElementSemanticNames() const noexcept;
		const D3D12_INPUT_LAYOUT_DESC& GetInputElementDesc() const noexcept;

		void SetConstantBufferVariable(const std::string name, const ShaderConstantVariableDesc&& desc);
		std::unordered_map<std::string, ShaderConstantVariableDesc>& GetConstantBufferVariables() noexcept;
	private:
		CComPtr<IDxcBlob> m_shader;
		D3D12_INPUT_LAYOUT_DESC m_inputLayoutDesc;
		std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElementData;
		std::vector <std::string > m_inputElementSemanticNames;
		std::vector<ShaderVariable> m_variables;
		std::unordered_map<std::string, ShaderConstantVariableDesc> m_constantBufferVariables;
	};

	struct PassData
	{
		UINT PassIndex;
		std::unique_ptr<DX12Shader> pVSShader = nullptr;
		std::unique_ptr<DX12Shader>	pPSShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		std::unique_ptr<PipelineResourceLayout> MeshResourceLayouts;
		BufferCreationDesc			ObjectBufferDesc;
		BufferCreationDesc			MaterialBufferDesc;
	};
}