#pragma once
#include "lib/Utility/Helper.h"

#include "lib/DX12/DX12Shader.h"

namespace ElysiaRenderer
{
	class Shader
	{
	public:
		Shader() = default;
		Shader(std::vector<ShaderPass>& shaderPasses);

		const PassData& GetPassData(UINT passIndex) const noexcept;
		const UINT FindPassIndex(const std::string& name) const noexcept;

		template<typename T> 
		void SetConstantVariable(const std::string& name, const T data, UINT passID = 0);
		template<typename T>
		void SetConstantVariable(const size_t hash, const T data, UINT passID = 0);
		void ApplyConstantData(); 

	private:
		std::mutex m_setDataMutex;
		std::vector<PassData> m_passDatas;
	};
}