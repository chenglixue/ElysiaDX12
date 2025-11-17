#pragma once
#include "Helper.h"

#include "ShaderUtility.h"
#include "MObject.h"
#include "Shader.h"

namespace ElysiaRenderer
{
	class Shader;

	class RenderMaterial : MObject
	{
	public:
		RenderMaterial() = default;
		RenderMaterial(std::vector<ShaderPass>& shaderPasses);
		~RenderMaterial() = default;

		const PassData& GetPassData(UINT passIndex) const noexcept;
		const PassData& GetPassData(std::string passName) const noexcept;
		UINT FindPassIndex(std::string passName) const noexcept;

		void SetConstantVariable(const std::string& name, const void* data);
		void SetConstantVariable(const std::string& name, const void* data, UINT passID);
		/*template<typename T>
		void SetConstantVariable(const std::string name, T data);*/
		void ApplyConstantData();

	private:
		std::unique_ptr<Shader> m_pShader = nullptr;
	};
}