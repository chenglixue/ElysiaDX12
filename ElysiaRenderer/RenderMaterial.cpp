#include "stdafx.h"
#include "RenderMaterial.h"

namespace ElysiaRenderer
{
	RenderMaterial::RenderMaterial(std::vector<ShaderPass>& shaderPasses)
		: m_pShader(std::make_unique<Shader>(shaderPasses))
	{

	}

	const PassData& RenderMaterial::GetPassData(std::string passName) const noexcept
	{
		return m_pShader->GetPassData(passName);
	}

	const PassData& RenderMaterial::GetPassData(UINT passIndex) const noexcept
	{
		return m_pShader->GetPassData(passIndex);
	}

	UINT RenderMaterial::FindPassIndex(std::string passName) const noexcept
	{
		return m_pShader->FindPassIndex(passName);
	}

	template<typename T>
	void RenderMaterial::SetConstantVariable(const std::string& name, T data)
	{
		m_pShader->SetConstantVariable(name, data);
	}
	void RenderMaterial::ApplyConstantData()
	{
		m_pShader->ApplyConstantData();
	}
}