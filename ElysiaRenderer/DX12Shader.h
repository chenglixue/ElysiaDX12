#pragma once

namespace ElysiaRenderer
{
	class ShaderType;
	class ShaderVariable;

	class DX12Shader
	{
	public:
		DX12Shader();
		DX12Shader(CComPtr<IDxcBlob> shader);
		~DX12Shader();

		CComPtr<IDxcBlob>& GetShader()
		{
			return m_shader;
		}

		void SetVariable(const std::vector<ShaderVariable>& shaderVariables);
		const std::vector<ShaderVariable>& GetVariable() const noexcept;

	private:
		CComPtr<IDxcBlob> m_shader;
		std::vector<ShaderVariable> m_variables;
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