#include "stdafx.h"
#include "BasePass.h"

#include "lib/Utility/PIXHelper.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"
#include "src/Material.h"
#include "DX12/UploadRingBuffer.h"
#include "Manager/TextureManager.h"
#include "lib/Utility/Common.h"

namespace ElysiaRenderer
{
	BasePass::BasePass(DX12Camera* pCamera) :
		m_renderSize(Vector2::Zero),
		m_pCamera(pCamera),
		m_shaderPasses(),
		m_pMaterial(),
		m_PipelineStateObjects()
	{

	}

	BasePass::~BasePass()
	{
		Dispose();
	}

	void BasePass::Setup(const RenderPassData& renderPassData)
	{
		m_renderSize = renderPassData.RenderSize;
		m_pCommand = renderPassData.pCommand;
		m_pDevice = renderPassData.pDevice;

		Configure();
	}

	void BasePass::Dispose()
	{
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
				case MaterialParameterBlock::UInt:
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
				case MaterialParameterBlock::FloatArray:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(float) * pMaterialParam->value.arrayData.size());
					break;
				}
				case MaterialParameterBlock::IntArray:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(int) * pMaterialParam->value.arrayData.size());
					break;
				}
				case MaterialParameterBlock::UIntArray:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(UINT) * pMaterialParam->value.arrayData.size());
					break;
				}
				case MaterialParameterBlock::Float2Array:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(float) * pMaterialParam->value.arrayData.size());
					break;
				}
				case MaterialParameterBlock::Float3Array:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(float) * pMaterialParam->value.arrayData.size());
					break;
				}
				case MaterialParameterBlock::Float4Array:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(float) * pMaterialParam->value.arrayData.size());
					break;
				}
				case MaterialParameterBlock::MatrixArray:
				{
					memcpy(dest, pMaterialParam->value.arrayData.data(), sizeof(float) * pMaterialParam->value.arrayData.size());
					break;
				}
			}
		}

		return GPUAddress;
	}

	void BasePass::SetSpaceResource(PassData& passData, UINT8 spaceID)
	{
		if(spaceID != PER_FRAME_SPACE)
		{
			auto pCurrVariantData = passData.pCurrVariantData;
			assert(pCurrVariantData);
			auto pResourceLayout = pCurrVariantData->pMeshResourceLayout.get();
			assert(pResourceLayout);

			if (pResourceLayout->IsValidSpace(spaceID))
			{
				if (pResourceLayout->m_spaces[spaceID] != nullptr)
				{
					delete pResourceLayout->m_spaces[spaceID];
					pResourceLayout->m_spaces[spaceID] = nullptr;
				}
			
				auto GPUAddress = UploadMaterialConstants(m_pDevice->GetGlobalUploadBuffer(), spaceID,  m_pMaterial.get(), passData.pCurrVariantData);
				auto newSpace = std::make_unique<PipelineResourceSpace>();
				newSpace->SetDynamicCBV(GPUAddress);
				newSpace->Lock();
				pResourceLayout->SetSpace(spaceID, newSpace.release());

				m_pCommand->SetPipelineResource(spaceID, pResourceLayout->m_spaces[spaceID]);
			}
		}
		else
		{
			auto pCurrVariantData = passData.pCurrVariantData;
			assert(pCurrVariantData);
			auto pResourceLayout = pCurrVariantData->pMeshResourceLayout.get();
			assert(pResourceLayout);
			
			if (pResourceLayout->IsValidSpace(spaceID))
			{
				pResourceLayout->m_spaces[PER_FRAME_SPACE] = RenderResource::GetInstance().GetPerFrameBindResourceSpace();
				m_pCommand->SetPipelineResource(PER_FRAME_SPACE, passData.pCurrVariantData->pMeshResourceLayout->m_spaces[PER_FRAME_SPACE]);
			}
		}
		
	}
}