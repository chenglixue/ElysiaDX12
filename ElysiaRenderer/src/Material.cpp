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
	
	D3D12_GPU_VIRTUAL_ADDRESS  UploadMaterialConstants(
			UploadRingBuffer* pUploadBuffer,
			UINT8 spaceID,
			Material* pMaterial,
			const ShaderVariantData* pVariantData)
	{
		assert(pUploadBuffer);
		assert(pMaterial);
		assert(spaceID < NUM_RESOURCE_SPACES);
		const auto& CBuffer = pVariantData->MergedReflectionData.GetCBuffer(spaceID);
		
		size_t totalSize = CBuffer.size;
		if (totalSize == 0)
		{
			return 0;
		}
		
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
		UINT8* CPUAddress = nullptr;
		if(!pUploadBuffer->Allocate(totalSize, GPUAddress, CPUAddress))
		{
			assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
			return 0;
		}
		memset(CPUAddress, 0, totalSize);
		
		for(const auto& memberPair : CBuffer.members)
		{
			auto& member =  memberPair.second;
			const MaterialParameterBlock::MaterialParam* pMaterialParam = pMaterial->GetParameterBlock().FindParam(member.Name);
			if(!pMaterialParam) continue;
			
			uint8_t* dest = CPUAddress + member.StartOffset;
			switch (pMaterialParam->type)
			{
			case MaterialParameterBlock::INT:
				{
					*reinterpret_cast<int*>(dest) = static_cast<int>(pMaterialParam->value.data[0]);
					break;
				}
			case MaterialParameterBlock::UINT:
				{
					*reinterpret_cast<UINT*>(dest) = static_cast<unsigned int>(pMaterialParam->value.data[0]);
					break;
				}
			case MaterialParameterBlock::BOOL:
				{
					*reinterpret_cast<UINT*>(dest) = static_cast<unsigned int>(pMaterialParam->value.data[0]);
					break;
				}
			case MaterialParameterBlock::FLOAT:
				{
					*reinterpret_cast<float*>(dest) = (pMaterialParam->value.data[0]);
					break;
				}
			case MaterialParameterBlock::FLOAT2:
				{
					auto& v = *reinterpret_cast<Vector2*>(dest);
					v.x = pMaterialParam->value.data[0];
					v.y = pMaterialParam->value.data[1];
					break;
				}
			case MaterialParameterBlock::FLOAT3:
				{
					auto& v = *reinterpret_cast<Vector3*>(dest);
					v.x = pMaterialParam->value.data[0];
					v.y = pMaterialParam->value.data[1];
					v.z = pMaterialParam->value.data[2];
					break;
				}
			case MaterialParameterBlock::FLOAT4:
				{
					auto& v = *reinterpret_cast<Vector4*>(dest);
					v.x = pMaterialParam->value.data[0];
					v.y = pMaterialParam->value.data[1];
					v.z = pMaterialParam->value.data[2];
					v.w = pMaterialParam->value.data[3];
					break;
				}
			case MaterialParameterBlock::MATRIX4X4:
				{
					memcpy(dest, pMaterialParam->value.data.data(), 64);
					break;
				}
			}
		}

		return GPUAddress;
	}
}