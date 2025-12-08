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

	void Material::SetInt(size_t nameHash, int v)
	{
		m_parameterBlock.SetInt(nameHash, v);
	}
	void Material::SetUInt(size_t nameHash, unsigned int v)
	{
		m_parameterBlock.SetUInt(nameHash, v);
	}
	void Material::SetBool(size_t nameHash, bool v)
	{
		m_parameterBlock.SetBool(nameHash, v);
	}

	void Material::SetFloat(size_t nameHash, float v)
	{
		m_parameterBlock.SetFloat(nameHash, v);
	}
	void Material::SetFloat2(size_t nameHash, const Vector2& v)
	{
		m_parameterBlock.SetFloat2(nameHash, v);
	}
	void Material::SetFloat3(size_t nameHash, const Vector3& v)
	{
		m_parameterBlock.SetFloat3(nameHash, v);
	}
	void Material::SetFloat4(size_t nameHash, const Vector4& v)
	{
		m_parameterBlock.SetFloat4(nameHash, v);
	}
	void Material::SetMatrix(size_t nameHash, const Matrix& m)
	{
		m_parameterBlock.SetMatrix(nameHash, m);
	}
	void Material::SetFloatArray(size_t nameHash, const std::vector<float>& v)
	{
		m_parameterBlock.SetFloatArray(nameHash, v);
	}
	void Material::SetIntArray(size_t nameHash, const std::vector<int>& v)
	{
		m_parameterBlock.SetIntArray(nameHash, v);
	}
	void Material::SetUINTArray(size_t nameHash, const std::vector<UINT>& v)
	{
		m_parameterBlock.SetUINTArray(nameHash, v);
	}
	void Material::SetVector2Array(size_t nameHash, const std::vector<Vector2>& v)
	{
		m_parameterBlock.SetVector2Array(nameHash, v);
	}
	void Material::SetVector3Array(size_t nameHash, const std::vector<Vector3>& v)
	{
		m_parameterBlock.SetVector3Array(nameHash, v);
	}
	void Material::SetVector4Array(size_t nameHash, const std::vector<Vector4>& v)
	{
		m_parameterBlock.SetVector4Array(nameHash, v);
	}
	void Material::SetMatrixArray(size_t nameHash, const std::vector<Matrix>& v)
	{
		m_parameterBlock.SetMatrixArray(nameHash, v);
	}
}