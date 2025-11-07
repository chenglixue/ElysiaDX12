#pragma once
#include "Helper.h"

#include "ShaderUtility.h"
#include "MObject.h"

namespace ElysiaRenderer
{
	class Shader;

	class RenderMaterial : MObject
	{
	public:
		RenderMaterial() = default;
		RenderMaterial(std::vector<ShaderPass>& shaderPasses);

	private:
		std::unique_ptr<Shader> m_pShader = nullptr;
	};
}