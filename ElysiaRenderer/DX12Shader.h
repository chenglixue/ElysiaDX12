#pragma once
#include "MObject.h"
#include "PipelineResourceUtility.h"
#include "ShaderUtility.h"

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

		void SetVariable(const std::vector<ShaderVariable>& shaderVariables);
		const std::vector<ShaderVariable>& GetVariable() const noexcept;

		void SetInputLayoutDesc(const D3D12_INPUT_LAYOUT_DESC& inputLayoutDesc);
		const D3D12_INPUT_LAYOUT_DESC& GetInputElementDesc() const noexcept;

		void SetConstantBufferVariable(const std::string& name, const ShaderConstantVariableDesc& desc);
		const std::unordered_map<std::string, ShaderConstantVariableDesc>& GetConstantBufferVariables() const noexcept;
	private:
		CComPtr<IDxcBlob> m_shader;
		D3D12_INPUT_LAYOUT_DESC m_inputLayoutDesc;
		std::vector<ShaderVariable> m_variables;
		std::unordered_map<std::string, ShaderConstantVariableDesc> m_constantBufferVariables;
		std::unique_ptr<PipelineStateObject> m_pPipelineStateObject = nullptr;
	};

	extern std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_vertexShaders;
	extern std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>> g_pixelShaders;
	extern std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>  g_computeShaders;

	inline std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>& GetVertexShaders()
	{
		return g_vertexShaders;
	}
	inline std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>& GetPixelShaders()
	{
		return g_pixelShaders;
	}
	inline std::unordered_map<UINT, std::unordered_map<ShaderType, std::unique_ptr<DX12Shader>>>& GetComputeShaders()
	{
		return g_computeShaders;
	}

	struct PassData
	{
		UINT PassIndex;
		std::unique_ptr<DX12Shader> pVSShader = nullptr;
		std::unique_ptr<DX12Shader>	pPSShader = nullptr;
		D3D12_RASTERIZER_DESC		RasterizerDesc;
		D3D12_BLEND_DESC			BlendDesc;
		D3D12_DEPTH_STENCIL_DESC	DepthStencilDesc;
		std::unique_ptr<PipelineResourceLayout> MeshResourceLayouts{};
	};
}