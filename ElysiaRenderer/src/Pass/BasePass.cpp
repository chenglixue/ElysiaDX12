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