#include "stdafx.h"
#include "RenderMaterial.h"

#include "Shader.h"

namespace ElysiaRenderer
{
	RenderMaterial::RenderMaterial(std::vector<ShaderPass>& shaderPasses)
		: m_pShader(std::make_unique<Shader>(shaderPasses))
	{

	}
}