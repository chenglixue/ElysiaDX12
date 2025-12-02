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
						.ShaderName = shaderPasses[passID].FilePath,
						.EntryPoint = shaderPasses[passID].VertexEntryPoint,
					},
					ShaderStageDesc
					{
						.ShaderType = ShaderType::Pixel,
						.ShaderName = shaderPasses[passID].FilePath,
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

	bool Material::HasMeshRender() const noexcept
	{
		return m_pMeshRender != nullptr;
	}

	void Material::CreateMaterialCBuffer(size_t passIndex)
	{
		auto layouts = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData.cbuffers;
		auto& pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer;
		
		if (pMaterialCBuffer != nullptr) pMaterialCBuffer.reset();
		
		pMaterialCBuffer = std::make_unique<MaterialRuntimeCBuffer>();
		
		for (auto&[space, layout] : layouts)
		{
			RuntimeCBuffer runtimeCBuffer{};
			runtimeCBuffer.CPUPtr.resize(layout.size);

			pMaterialCBuffer->CBuffers[space] = runtimeCBuffer;
		}
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
					else if(meshRenderer.m_objectBuffers[frameIndex] == nullptr)
					{
						meshRenderer.m_objectBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(objectBufferDesc));
					}

					auto materialBufferDesc = pPipelineResourceLayout->m_spaces[PER_MATERIAL_SPACE]->GetCBVDesc();
					if (meshRenderer.m_materialBuffers[frameIndex] &&
						meshRenderer.m_materialBuffers[frameIndex]->GetResourceDesc().Width != materialBufferDesc.m_size)
					{
						meshRenderer.m_materialBuffers[frameIndex].reset();
						meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
					}
					else if(meshRenderer.m_materialBuffers[frameIndex] == nullptr)
					{
						meshRenderer.m_materialBuffers[frameIndex] = std::move(GetDevice()->CreateBuffer(materialBufferDesc));
					}
				}
			}
		}

		if (pPipelineResourceLayout->m_spaces[PER_PASS_SPACE] != nullptr)
		{
			const auto& desc = pPipelineResourceLayout->m_spaces[PER_PASS_SPACE]->GetCBVDesc();
			if(m_pPassConstantBuffer != nullptr && m_pPassConstantBuffer->GetResourceDesc().Width != desc.m_size)
			{
				m_pPassConstantBuffer.reset();
				m_pPassConstantBuffer = std::move(GetDevice()->CreateBuffer(desc));
			}
			else if(m_pPassConstantBuffer == nullptr)
			{
				m_pPassConstantBuffer = std::move(GetDevice()->CreateBuffer(desc));
			}
			
		}
		
		if(pPipelineResourceLayout->m_spaces[PER_FRAME_SPACE] != nullptr)
		{
			const auto& desc = pPipelineResourceLayout->m_spaces[PER_FRAME_SPACE]->GetCBVDesc();
			if(m_pFrameConstantBuffer != nullptr && m_pFrameConstantBuffer->GetResourceDesc().Width != desc.m_size)
			{
				m_pFrameConstantBuffer.reset();
				m_pFrameConstantBuffer = std::move(GetDevice()->CreateBuffer(desc));
			}
			else if(m_pFrameConstantBuffer == nullptr)
			{
				m_pFrameConstantBuffer = std::move(GetDevice()->CreateBuffer(desc));
			}
		}
	}
	
	void Material::SetMaterialCBufferGPUPtr(UINT spaceID, UINT8* pMappedBuffer, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		m_passDatas[passIndex].pMaterialCBuffer->CBuffers[spaceID].GPUPtr = pMappedBuffer;
	}

	template<typename T>
	void Material::UpdateCBuffer(RuntimeCBuffer& CBuffer, UINT32 offset, const T& data)
	{
		memcpy(CBuffer.CPUPtr.data() + offset, &data, sizeof(T));
		CBuffer.MakeDirty(offset, sizeof(T));
	}

	void Material::SetFloat(const size_t hashName, const float& value, size_t passIndex)
	{
		assert(passIndex < m_passDatas.size());
		
		auto pMaterialCBuffer = m_passDatas[passIndex].pMaterialCBuffer.get();
		assert(pMaterialCBuffer);

		auto allCBuffers = pMaterialCBuffer->CBuffers;
		for (UINT spaceID = 0; spaceID < allCBuffers.size(); ++spaceID)
		{
			if (allCBuffers.find(spaceID) == allCBuffers.end()) continue;
			auto cbuffer = allCBuffers[spaceID];

			const auto& mergedReflectionData = m_passDatas[passIndex].pCurrVariantData->MergedReflectionData;
			if (mergedReflectionData.HasCBufferMember(spaceID, hashName))
			{
				const auto memberData = mergedReflectionData.FindCBufferMember(spaceID, hashName);
				UpdateCBuffer<float>(cbuffer, memberData.StartOffset, value);
			}
			
			
		}
	}

	void Material::Flush()
	{
		for (auto& passData : m_passDatas)
		{
			for (auto&[spaceID, CBuffer] : passData.pMaterialCBuffer->CBuffers)
			{
				if (!CBuffer.HasDirtyRange()) continue;

				UINT32 size = CBuffer.DirtyEnd - CBuffer.DirtyBegin;

				memcpy(CBuffer.GPUPtr + CBuffer.DirtyBegin, CBuffer.CPUPtr.data() + CBuffer.DirtyBegin, size);

				CBuffer.ClearDirty();
			}	
		}
		
	}

}