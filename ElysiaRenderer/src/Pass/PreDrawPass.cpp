#include "stdafx.h"
#include "PreDrawPass.h"

#include "GBufferPass.h"
#include "RenderResource.h"
#include "DX12/UploadRingBuffer.h"
#include "Manager/CameraManager.h"
#include "Manager/RenderTargetManager.h"
#include "Utility/RenderHelper.h"

namespace ElysiaRenderer
{
	PreDrawPass::PreDrawPass(DX12Camera* pCamera):
	BasePass(pCamera)
	{
		
	}
	PreDrawPass::~PreDrawPass()
	{
		Dispose();
	}
	void PreDrawPass::Dispose()
	{

	}
	
	void PreDrawPass::Configure()
     {
          
     }
	void PreDrawPass::Execute()
     {
        auto GPUAddress = UploadFrameConstant(m_pDevice,
			[this](CBVFrameVariable* dst)
			{
				dst->cameraPosWS = CameraManager::GetInstance().GetMainCamera()->GetPosition4();
				dst->lightData = std::move(LightManager::GetInstance().GetMainLight()->CreateLightData());
				dst->frameIndex = m_pDevice->GetFrameIndex();
				dst->nearZ = CameraManager::GetInstance().GetMainCamera()->GetNearZ();
				dst->farZ = CameraManager::GetInstance().GetMainCamera()->GetFarZ();
				dst->OpaqueColorIndex = BufferManager::GetInstance().GetCameraColorRT()->GetResourceHeapIndex();
				dst->OpaqueDepthIndex = BufferManager::GetInstance().GetCameraDepthRT()->GetResourceHeapIndex();
				dst->ZBufferParams = GetZBufferParams(CameraManager::GetInstance().GetMainCamera()->GetNearZ(), CameraManager::GetInstance().GetMainCamera()->GetFarZ());
				dst->ShadowTexIndex = LightManager::GetInstance().GetMainShadowRT()->GetResourceHeapIndex();
				dst->shadowMatrix = LightManager::GetInstance().GetMainShadow()->GetShadowMat();
				dst->shadowSize = GetScreenSize(Vector2(LightManager::GetInstance().GetMainShadow()->GetWidth(),
					LightManager::GetInstance().GetMainShadow()->GetHeight()));
				dst->GBuffer0Index = RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBufferPass0ID)->GetResourceHeapIndex();
				dst->GBuffer1Index = RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBufferPass1ID)->GetResourceHeapIndex();
				dst->GBuffer2Index = RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBufferPass2ID)->GetResourceHeapIndex();
				dst->GBuffer3Index = RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBufferPass3ID)->GetResourceHeapIndex();
				dst->GBuffer4Index = RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBufferPass4ID)->GetResourceHeapIndex();
				dst->GBuffer5Index = RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBufferPass5ID)->GetResourceHeapIndex();
			});

		auto frameSpace = RenderResource::GetInstance().GetPerFrameBindResourceSpace(m_pDevice->GetFrameID());
		frameSpace->Reset();
		frameSpace->SetDynamicCBV(GPUAddress);
		frameSpace->Lock();
     }
	void PreDrawPass::Render()
     {
          Execute();
     }
	void PreDrawPass::UpdatePSO()
     {
          
     }
	void PreDrawPass::UpdateVariant()
     {
          
     }
}
