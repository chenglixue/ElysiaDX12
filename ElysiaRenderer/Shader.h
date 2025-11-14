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
		void SetConstantVariable(const std::string name, T data);
		void ApplyConstantData();

	private:
		std::unordered_map<std::string, ShaderVariable> m_shaderVariables;
		std::unordered_map<std::string, ShaderConstantVariableDesc> m_constantVariableDescs;
		std::unordered_map<std::string, PassData> m_passDatas;
	};
}