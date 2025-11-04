#pragma once
#include "MObject.h"
#include "ShaderUtility.h"

namespace ElysiaRenderer
{
	class PipelineStateObject;
	
	class DX12Shader : MObject
	{
	public:
		DX12Shader();
		DX12Shader(CComPtr<IDxcBlob> shader);
		~DX12Shader();

		CComPtr<IDxcBlob>& GetShader();

		void SetVariable(const std::vector<ShaderVariable>& shaderVariables);
		const std::vector<ShaderVariable>& GetVariable() const noexcept;

		void SetInputElementSemanticNames(const std::vector<std::string>& inputElementSemanticNames);

	private:
		CComPtr<IDxcBlob> m_shader;
		std::vector<std::string> m_inputElementSemanticNames;
		std::vector<ShaderVariable> m_variables;
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
}