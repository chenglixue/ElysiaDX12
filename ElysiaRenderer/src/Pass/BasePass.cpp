#include "stdafx.h"
#include "BasePass.h"

#include "lib/Utility/PIXHelper.h"
#include "lib/Utility/RenderTexture.h"
#include "RenderResource.h"
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

	bool BasePass::UploadMaterialConstants(
			UploadRingBuffer* pUploadBuffer,
			UINT spaceID,
			const Material* pMaterial,
			const ShaderVariantData* pVariantData)
	{
		const auto& parameters = pMaterial->GetParameterBlock();
		const auto& CBuffer = pVariantData->MergedReflectionData.GetCBuffer(spaceID);
		
		size_t totalSize = CBuffer.size;
		if (totalSize == 0)
		{
			return false;
		}
		
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
		UINT8* CPUAddress = nullptr;
		if(!pUploadBuffer->Allocate(totalSize, GPUAddress, CPUAddress))
		{
			assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
			return false;
		}
		
		memset(CPUAddress, 0, totalSize);
		
		const MaterialParameterBlock& params = pMaterial->GetParameterBlock();
		
		for(const auto& memberPair : CBuffer.members)
		{
			auto& member =  memberPair.second;
			const MaterialParameterBlock::MaterialParam* pMaterialParam = pMaterial->GetParameterBlock().FindParam(member.Name);
			if(!pMaterialParam) continue;
			
			
		}
	}
}