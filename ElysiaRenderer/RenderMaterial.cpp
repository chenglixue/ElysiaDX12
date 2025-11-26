#include "stdafx.h"
#include "RenderMaterial.h"

namespace ElysiaRenderer
{
	RenderMaterial::RenderMaterial(std::vector<ShaderPass>& shaderPasses)
		: m_pShader(std::make_unique<Shader>(shaderPasses))
	{

	}

	const PassData& RenderMaterial::GetPassData(UINT passIndex) const noexcept
	{
		return m_pShader->GetPassData(passIndex);
	}

	template<typename T>
	void RenderMaterial::SetConstantVariable(const std::string& name, T data, UINT passID)
	{
		m_pShader->SetConstantVariable(name, data, passID);
	}
	
	template<typename T>
	void RenderMaterial::SetConstantVariable(const size_t hash, T data, UINT passID)
	{
		m_pShader->SetConstantVariable(hash, data, passID);
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
	template void RenderMaterial::SetConstantVariable<math::Matrix4>(const std::string&, math::Matrix4, UINT passID);
	template void RenderMaterial::SetConstantVariable<bool>(const std::string&, bool, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector2>>(const std::string&, std::vector<Vector2>, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector3>>(const std::string&, std::vector<Vector3>, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector4>>(const std::string&, std::vector<Vector4>, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<UINT>>(const std::string&, std::vector<UINT>, UINT passID);

	template void RenderMaterial::SetConstantVariable<UINT>(const size_t, UINT, UINT passID);
	template void RenderMaterial::SetConstantVariable<int>(const size_t, int, UINT passID);
	template void RenderMaterial::SetConstantVariable<float>(const size_t, float, UINT passID);
	template void RenderMaterial::SetConstantVariable<Vector2>(const size_t, Vector2, UINT passID);
	template void RenderMaterial::SetConstantVariable<Vector3>(const size_t, Vector3, UINT passID);
	template void RenderMaterial::SetConstantVariable<Vector4>(const size_t, Vector4, UINT passID);
	template void RenderMaterial::SetConstantVariable<Matrix>(const size_t, Matrix, UINT passID);
	template void RenderMaterial::SetConstantVariable<math::Matrix4>(const size_t, math::Matrix4, UINT passID);
	template void RenderMaterial::SetConstantVariable<bool>(const size_t, bool, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector2>>(const size_t, std::vector<Vector2>, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector3>>(const size_t, std::vector<Vector3>, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<Vector4>>(const size_t, std::vector<Vector4>, UINT passID);
	template void RenderMaterial::SetConstantVariable<std::vector<UINT>>(const size_t, std::vector<UINT>, UINT passID);
}