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
		const PassData& GetPassData(std::string passName) const noexcept;
		UINT FindPassIndex(std::string passName) const noexcept;

		template<typename T> 
		void SetConstantVariable(const std::string& name, const T data, UINT passID = 0);
		void ApplyConstantData(); 

		const std::unordered_map<std::string, ShaderVariable>& GetShaderVariables() const noexcept
		{
			return m_shaderVariables;
		}

	private:
		std::mutex m_setDataMutex;
		std::unordered_map<std::string, ShaderVariable> m_shaderVariables;
		std::unordered_multimap<std::string, ShaderConstantVariableDesc> m_constantVariableDescs;
		std::unordered_map<std::string, PassData> m_passDatas;
	};
}