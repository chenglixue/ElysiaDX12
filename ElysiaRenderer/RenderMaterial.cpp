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
	void RenderMaterial::SetConstantVariable(const std::string& name, T data, UINT passID)
	{
		m_pShader->SetConstantVariable(name, data, passID);
	}
	void RenderMaterial::ApplyConstantData()
	{
		m_pShader->ApplyConstantData();
	}

	template void RenderMaterial::SetConstantVariable<UINT>(const std::string&, UINT, UINT passID);
	template void RenderMaterial::SetConstantVariable<int>(const std::string&, int, UINT passID);
	template void RenderMaterial::SetConstantVariable<float>(const std::string&, float, UINT passID);
	template void RenderMaterial::SetConstantVariable<Vector2>(const std::string&, Vector2, UINT passID);
	template void RenderMaterial::SetConstantVariable<Vector3>(const std::string&, Vector3, UINT passID);
	template void RenderMaterial::SetConstantVariable<Vector4>(const std::string&, Vector4, UINT passID);
	template void RenderMaterial::SetConstantVariable<Matrix>(const std::string&, Matrix, UINT passID);
	template void RenderMaterial::SetConstantVariable<bool>(const std::string&, bool, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector2>>(const std::string&, std::vector<Vector2>, UINT passID);
}