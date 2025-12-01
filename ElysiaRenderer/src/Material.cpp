#include "stdafx.h"
#include "Material.h"

#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12BufferResource.h"
#include "lib/Utility/PipelineResourceUtility.h"
#include "RenderResource.h"
#include "Manager/ShaderVariantManager.h"

namespace ElysiaRenderer
{
	using namespace ElysiaModel;
	
	Material::Material(std::vector<ShaderPass>& shaderPasses)
	{
		Init(shaderPasses);
	}

	Material::Material(std::vector<ShaderPass>& shaderPasses, MeshRender* pMeshRender) :
		m_pMeshRender(pMeshRender)
	{
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
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
					},
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Pixel,
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
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

			newPassData.pShader = GetDevice()->CreateShader(desc);
			newPassData.BlendDesc = GetBlendState(newPassData.pShader->GetRenderStates().at(L"Blend"));
			newPassData.RasterizerDesc = GetRasterizerState(newPassData.pShader->GetRenderStates().at(L"Rasterizer"));
			newPassData.DepthStencilDesc = GetDepthState(newPassData.pShader->GetRenderStates().at(L"Depth"));

			m_passDatas.emplace_back(std::move(newPassData));
		}
	}

	const PassData& Material::GetPassData(UINT passIndex) const noexcept
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

	bool Material::HasMeshRender() const noexcept
	{
		return m_pMeshRender != nullptr;
	}

	void Material::SetPipelineResourceLayout(PipelineResourceLayout* pPipelineResourceLayout)
	{
		if (HasMeshRender())
		{
			for (UINT meshIndex = 0; meshIndex < GetModelImporter()->GetMeshCount(); ++meshIndex)
			{
				auto& meshRenderer = m_pMeshRender[meshIndex];

				for (UINT frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
				{
					auto objectBufferDesc = pPipelineResourceLayout->m_spaces[PER_OBJECT_SPACE]->GetCBVDesc();
					if (meshRenderer.m_objectBuffers[frameIndex] &&
						meshRenderer.m_objectBuffers[frameIndex]->GetResourceDesc().Width != objectBufferDesc.m_size)
					{
						meshRenderer.m_objectBuffers[frameIndex].reset();
						meshRenderer.m_objectBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(objectBufferDesc));
					}

					auto materialBufferDesc = pPipelineResourceLayout->m_spaces[PER_MATERIAL_SPACE]->GetCBVDesc();
					if (meshRenderer.m_materialBuffers[frameIndex] &&
						meshRenderer.m_materialBuffers[frameIndex]->GetResourceDesc().Width != materialBufferDesc.m_size)
					{
						meshRenderer.m_materialBuffers[frameIndex].reset();
						meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
					}
				}
			}
		}

		if (pPipelineResourceLayout->m_spaces[PER_PASS_SPACE] != nullptr)
		{
			
		}
	}

	template<typename T>
	void Material::SetConstantVariable(const std::string& name, T data, UINT passID)
	{
		m_pShader->SetConstantVariable(name, data, passID);
	}
	
	template<typename T>
	void Material::SetConstantVariable(const size_t hash, T data, UINT passID)
	{
		m_pShader->SetConstantVariable(hash, data, passID);
	}
	void Material::ApplyConstantData() 
	{
		m_pShader->ApplyConstantData();
	}

	template void Material::SetConstantVariable<UINT>(const std::string&, UINT, UINT passID);
	template void Material::SetConstantVariable<int>(const std::string&, int, UINT passID);
	template void Material::SetConstantVariable<float>(const std::string&, float, UINT passID);
	template void Material::SetConstantVariable<Vector2>(const std::string&, Vector2, UINT passID);
	template void Material::SetConstantVariable<Vector3>(const std::string&, Vector3, UINT passID);
	template void Material::SetConstantVariable<Vector4>(const std::string&, Vector4, UINT passID);
	template void Material::SetConstantVariable<Matrix>(const std::string&, Matrix, UINT passID);
	template void Material::SetConstantVariable<math::Matrix4>(const std::string&, math::Matrix4, UINT passID);
	template void Material::SetConstantVariable<bool>(const std::string&, bool, UINT passID);
	template void Material::SetConstantVariable<std::vector<Vector2>>(const std::string&, std::vector<Vector2>, UINT passID);
	template void Material::SetConstantVariable<std::vector<Vector3>>(const std::string&, std::vector<Vector3>, UINT passID);
	template void Material::SetConstantVariable<std::vector<Vector4>>(const std::string&, std::vector<Vector4>, UINT passID);
	template void Material::SetConstantVariable<std::vector<UINT>>(const std::string&, std::vector<UINT>, UINT passID);

	template void Material::SetConstantVariable<UINT>(const size_t, UINT, UINT passID);
	template void Material::SetConstantVariable<int>(const size_t, int, UINT passID);
	template void Material::SetConstantVariable<float>(const size_t, float, UINT passID);
	template void Material::SetConstantVariable<Vector2>(const size_t, Vector2, UINT passID);
	template void Material::SetConstantVariable<Vector3>(const size_t, Vector3, UINT passID);
	template void Material::SetConstantVariable<Vector4>(const size_t, Vector4, UINT passID);
	template void Material::SetConstantVariable<Matrix>(const size_t, Matrix, UINT passID);
	template void Material::SetConstantVariable<math::Matrix4>(const size_t, math::Matrix4, UINT passID);
	template void Material::SetConstantVariable<bool>(const size_t, bool, UINT passID);
	template void Material::SetConstantVariable<std::vector<Vector2>>(const size_t, std::vector<Vector2>, UINT passID);
	template void Material::SetConstantVariable<std::vector<Vector3>>(const size_t, std::vector<Vector3>, UINT passID);
	template void Material::SetConstantVariable<std::vector<Vector4>>(const size_t, std::vector<Vector4>, UINT passID);
	template void Material::SetConstantVariable<std::vector<UINT>>(const size_t, std::vector<UINT>, UINT passID);
}