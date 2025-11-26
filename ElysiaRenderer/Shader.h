#pragma once
#include "Helper.h"
#include "MObject.h"

#include "DX12Shader.h"

namespace ElysiaRenderer
{
	class Shader
	{
	public:
		Shader() = default;
		Shader(std::vector<ShaderPass>& shaderPasses);

		const PassData& GetPassData(UINT passIndex) const noexcept;

		template<typename T> 
		void SetConstantVariable(const std::string& name, const T data, UINT passID = 0);
		template<typename T>
		void Shader::SetConstantVariable(const size_t hash, const T data, UINT passID = 0);
		void ApplyConstantData(); 

		const std::unordered_map<std::string, ShaderVariable>& GetShaderVariables() const noexcept
		{
			return m_shaderVariables;
		}

	private:
		std::mutex m_setDataMutex;
		std::unordered_map<std::string, ShaderVariable> m_shaderVariables;
		std::unordered_map<UINT, std::unordered_map<size_t, ShaderConstantVariableDesc>> m_constantVariableDescs;
		std::vector<PassData> m_passDatas;
	};
}