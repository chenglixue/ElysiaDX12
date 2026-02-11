#include "stdafx.h"
#include "PreDrawPass.h"

#include "AOPass.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/UploadRingBuffer.h"

#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/DX12Light.h"
#include "Runtime/RenderCore/DX12Shadow.h"
#include "Runtime/RenderCore/LightManager.h"
#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/RenderTexture.h"

#include "Programs/RenderHelper.h"

#include "GBufferPass.h"
#include "Editor/UserData.h"
#include "Runtime/RenderCore/TextureManager.h"

namespace ElysiaRenderer
{
    PreDrawPass::PreDrawPass()
        : BasePass()
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

    void PreDrawPass::Render(FrameContext& context)
    {
        m_pCamera = context.pCamera;

        auto GPUAddress = UploadFrameConstant(
            m_pDevice,
            [this, context](CBVFrameVariable* dst)
            {
                *dst = RenderResource::GetInstance().GetCBVFrameVariable();
                dst->cameraPosWS = CameraManager::GetInstance().GetMainCamera()->GetPosition4();
                dst->lightData = std::move(
                    LightManager::GetInstance().GetMainLight()->CreateLightData());
                dst->frameIndex = context.frameIndex;
                dst->nearZ = CameraManager::GetInstance().GetMainCamera()->GetNearZ();
                dst->farZ = CameraManager::GetInstance().GetMainCamera()->GetFarZ();
                dst->ZBufferParams = GetZBufferParams(
                    CameraManager::GetInstance().GetMainCamera()->GetNearZ(),
                    CameraManager::GetInstance().GetMainCamera()->GetFarZ());
                dst->shadowMatrix = LightManager::GetInstance().GetMainShadow()->GetShadowMat();
                dst->shadowSize = GetScreenSize(Vector2(
                    LightManager::GetInstance().GetMainShadow()->GetWidth(),
                    LightManager::GetInstance().GetMainShadow()->GetHeight()));

                dst->OpaqueColorIndex = m_pCameraColorRT->GetResourceHeapIndex();
                dst->OpaqueDepthIndex = m_pCameraDepthRT->GetResourceHeapIndex();
                dst->SkyboxTexIndex =
                    TextureManager::GetInstance().LoadResidentTexture(L"Tex\\cubemap0.dds").
                                                  GetResourceHeapIndex();
                dst->GGX_E_LUT_Index =
                    TextureManager::GetInstance().LoadResidentTexture(L"Tex\\GGX_E_LUT.dds").
                                                  GetResourceHeapIndex();
                dst->GGX_Eavg_LUT_Index =
                    TextureManager::GetInstance().LoadResidentTexture(L"Tex\\GGX_Eavg_LUT.dds").
                                                  GetResourceHeapIndex();
                dst->BlueNoiseTexIndex =
                    TextureManager::GetInstance().LoadResidentTexture(L"Tex\\blue_noise.dds").
                                                  GetResourceHeapIndex();
                dst->ShadowTexIndex = LightManager::GetInstance().GetMainShadowRT()->
                                                                  GetResourceHeapIndex();
                dst->GBuffer0Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer0ID)
                                     ->GetResourceHeapIndex();
                dst->GBuffer1Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer1ID)
                                     ->GetResourceHeapIndex();
                dst->GBuffer2Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer2ID)
                                     ->GetResourceHeapIndex();
                dst->GBuffer3Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer3ID)
                                     ->GetResourceHeapIndex();
                dst->GBuffer4Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer4ID)
                                     ->GetResourceHeapIndex();
                dst->GBuffer5Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer5ID)
                                     ->GetResourceHeapIndex();
                dst->AOTexIndex = RenderTargetManager::GetInstance().GetRenderTexture(
                                                                        AOPass::RenderTextureIDs::AORTID)
                                                                    ->GetResourceHeapIndex();
                dst->g_EnableAO = UserData::GetInstance().aoParameter.IsEnableAO;
                dst->g_EnableShadow = UserData::GetInstance().EnableShadow;
            });

        auto frameSpace = RenderResource::GetInstance().GetPerFrameBindResourceSpace(
            m_pDevice->GetFrameID());
        frameSpace->Reset();
        frameSpace->SetDynamicCBV(GPUAddress);
        frameSpace->Lock();
    }

    void PreDrawPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

    }
} // namespace ElysiaRenderer