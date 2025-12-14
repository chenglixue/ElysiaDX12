#include "stdafx.h"
#include "Material.h"

#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12BufferResource.h"
#include "lib/Utility/PipelineResourceUtility.h"
#include "RenderResource.h"
#include "Manager/ShaderVariantManager.h"
#include "lib/DX12/UploadRingBuffer.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;
	
	Material::Material(DX12Device* pDevice, std::vector<ShaderPass>& shaderPasses) :
		m_pDevice(pDevice)
	{
		assert(pDevice);
		Init(shaderPasses);
	}

	void Material::Init(std::vector<ShaderPass>& shaderPasses)
	{
		m_passDatas.reserve(shaderPasses.size());

		for (UINT passID = 0; passID < shaderPasses.size(); ++passID)
		{
			auto newPassData = PassData();

			// set shader pass
			newPassData.PassIndex = passID;
			newPassData.Name = shaderPasses[passID].Name;

			ShaderCreateDesc desc;
			if (!shaderPasses[passID].IsComputeShader)
			{
				desc.stages = 
				{
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Vertex,
						.ShaderName = shaderPasses[passID].FilePath,
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
					},
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Pixel,
						.ShaderName = shaderPasses[passID].FilePath,
						.EntryPoint = shaderPasses[passID].FragmentEntryPoint,
					}
				};
			}
			else
			{
				desc.stages = 
				{
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Compute,
						.EntryPoint = shaderPasses[passID].ComputeEntryPoint,
					}
				};
			}

			newPassData.pShader = std::move(m_pDevice->CreateShader(desc));
			newPassData.BlendDesc = GetBlendState(newPassData.pShader->GetRenderStates().at(L"Blend"));
			newPassData.RasterizerDesc = GetRasterizerState(newPassData.pShader->GetRenderStates().at(L"Rasterizer"));
			newPassData.DepthStencilDesc = GetDepthState(newPassData.pShader->GetRenderStates().at(L"Depth"));

			m_passDatas.emplace_back(std::move(newPassData));
		} 
	}

	PassData& Material::GetPassData(UINT passIndex)  noexcept
	{
		return m_passDatas.at(passIndex);
	}

	const UINT Material::FindPassIndex(const std::string& name) const noexcept
	{
		for (auto& passData : m_passDatas)
		{
			if (passData.Name == name)
			{
				return passData.PassIndex;
			}
		}
	}

	void Material::SetInt(size_t nameHash, int v, size_t passID)
	{
		m_parameterBlock.SetInt(nameHash, v, passID);
	}
	void Material::SetUInt(size_t nameHash, unsigned int v, size_t passID)
	{
		m_parameterBlock.SetUInt(nameHash, v, passID);
	}
	void Material::SetBool(size_t nameHash, bool v, size_t passID)
	{
		m_parameterBlock.SetFloat(nameHash, v ? 1.f : 0.f, passID);
	}
	void Material::SetFloat(size_t nameHash, float v, size_t passID)
	{
		m_parameterBlock.SetFloat(nameHash, v, passID);
	}
	void Material::SetFloat2(size_t nameHash, const Vector2& v, size_t passID)
	{
		m_parameterBlock.SetFloat2(nameHash, v, passID);
	}
	void Material::SetFloat3(size_t nameHash, const Vector3& v, size_t passID)
	{
		m_parameterBlock.SetFloat3(nameHash, v, passID);
	}
	void Material::SetFloat4(size_t nameHash, const Vector4& v, size_t passID)
	{
		m_parameterBlock.SetFloat4(nameHash, v, passID);
	}
	void Material::SetMatrix(size_t nameHash, const Matrix& m, size_t passID)
	{
		m_parameterBlock.SetMatrix(nameHash, m, passID);
	}
	void Material::SetFloatArray(size_t nameHash, const std::vector<float>& v, size_t passID)
	{
		m_parameterBlock.SetFloatArray(nameHash, v, passID);
	}
	void Material::SetIntArray(size_t nameHash, const std::vector<int>& v, size_t passID)
	{
		m_parameterBlock.SetIntArray(nameHash, v, passID);
	}
	void Material::SetUINTArray(size_t nameHash, const std::vector<UINT>& v, size_t passID)
	{
		m_parameterBlock.SetUINTArray(nameHash, v, passID);
	}
	void Material::SetVector2Array(size_t nameHash, const std::vector<Vector2>& v, size_t passID)
	{
		m_parameterBlock.SetVector2Array(nameHash, v, passID);
	}
	void Material::SetVector3Array(size_t nameHash, const std::vector<Vector3>& v, size_t passID)
	{
		m_parameterBlock.SetVector3Array(nameHash, v, passID);
	}
	void Material::SetVector4Array(size_t nameHash, const std::vector<Vector4>& v, size_t passID)
	{
		m_parameterBlock.SetVector4Array(nameHash, v, passID);
	}
	void Material::SetMatrixArray(size_t nameHash, const std::vector<Matrix>& v, size_t passID)
	{
		m_parameterBlock.SetMatrixArray(nameHash, v, passID);
	}
}