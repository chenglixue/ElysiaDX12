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
		const UINT FindPassIndex(const std::string& name) const noexcept;

		template<typename T>
		void SetConstantVariable(const std::string& name, T data, UINT passID = 0);
		template<typename T>
		void SetConstantVariable(const size_t hash, T data, UINT passID = 0);
		void ApplyConstantData();

	private:
		std::unique_ptr<Shader> m_pShader = nullptr;
	};
}