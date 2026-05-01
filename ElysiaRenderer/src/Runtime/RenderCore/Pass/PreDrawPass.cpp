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
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12UploadContext.h"
#include "Runtime/RenderCore/RenderPassResourceManager.h"
#include "Runtime/RenderCore/TextureManager.h"

namespace ElysiaRenderer
{
    PreDrawPass::PreDrawPass()
        : BasePass()
    {
        {
            BufferCreationDesc bufferDesc =
            {
                .name = L"Sobol 256spp256d Buffer",
                .stride = sizeof(int),
                .size = sizeof(int) * 256 * 256,
                .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
                .accessFlags = BufferAccessFlags::GPUOnly,
            };
            m_pSobol256spp256dBuffer = BufferManager::GetInstance().CreateBuffer(bufferDesc);

        }

        {
            BufferCreationDesc bufferDesc =
            {
                .name = L"Scrambling Tile Buffer",
                .stride = sizeof(int),
                .size = sizeof(int) * 128 * 128 * 8,
                .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
                .accessFlags = BufferAccessFlags::GPUOnly,
            };
            m_pScramblingTileBuffer = BufferManager::GetInstance().CreateBuffer(bufferDesc);

        }

        {
            BufferCreationDesc bufferDesc =
            {
                .name = L"Ranking Tile Buffer",
                .stride = sizeof(int),
                .size = sizeof(int) * 128 * 128 * 8,
                .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
                .accessFlags = BufferAccessFlags::GPUOnly,
            };
            m_pRankingTileBuffer = BufferManager::GetInstance().CreateBuffer(bufferDesc);

        }
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
        {
            auto pBufferData = DX12BufferUpload
            {
                .buffer = m_pSobol256spp256dBuffer,
                .pBufferData = std::make_unique<uint8_t[]>(sizeof(int) * 256 * 256),
                .bufferDataSize = sizeof(int) * 256 * 256
            };
            memcpy(pBufferData.pBufferData.get(), g_Sobol_256spp_256d, sizeof(int) * 256 * 256);
            BufferManager::GetInstance().UploadBufferData(m_pDevice->GetUploadContext(), &pBufferData);
        }

        {
            auto pBufferData = DX12BufferUpload
            {
                .buffer = m_pScramblingTileBuffer,
                .pBufferData = std::make_unique<uint8_t[]>(sizeof(int) * 128 * 128 * 8),
                .bufferDataSize = sizeof(int) * 128 * 128 * 8
            };
            memcpy(pBufferData.pBufferData.get(), g_ScramblingTile, sizeof(int) * 128 * 128 * 8);
            BufferManager::GetInstance().UploadBufferData(m_pDevice->GetUploadContext(), &pBufferData);
        }

        {
            auto pBufferData = DX12BufferUpload
            {
                .buffer = m_pRankingTileBuffer,
                .pBufferData = std::make_unique<uint8_t[]>(sizeof(int) * 128 * 128 * 8),
                .bufferDataSize = sizeof(int) * 128 * 128 * 8
            };
            memcpy(pBufferData.pBufferData.get(), g_RankingTile, sizeof(int) * 128 * 128 * 8);
            BufferManager::GetInstance().UploadBufferData(m_pDevice->GetUploadContext(), &pBufferData);
        }

        m_shaderGlobalData =
        {
            .skyboxTex = TextureManager::GetInstance().LoadResidentTexture(L"Tex\\cubemap0.dds"),
            .GGX_E_LUT_Index = TextureManager::GetInstance().LoadResidentTexture(L"Tex\\GGX_E_LUT.dds"),
            .GGX_Eavg_LUT_Index = TextureManager::GetInstance().LoadResidentTexture(L"Tex\\GGX_Eavg_LUT.dds"),
            .blueNoiseTexIndex = TextureManager::GetInstance().LoadResidentTexture(L"Tex\\blue_noise.dds")
        };

        RenderPassResourceManager::GetInstance().Create<ShaderGlobalData>(&m_shaderGlobalData);
    }

    void PreDrawPass::Render(FrameContext& context)
    {
        m_pCamera = context.pCamera;
        auto renderWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        auto renderHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);
        float screenPercentage = (float)renderWidth / (float)m_displaySize.x;

        static std::mt19937 gen(std::random_device{}());
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        auto randomSeed = dist(gen);

        auto& shaderGlobalData = RenderPassResourceManager::GetInstance().Get<ShaderGlobalData>();

        auto GPUAddress = UploadFrameConstant(
            m_pDevice,
            [this, &context, &screenPercentage, &randomSeed, &shaderGlobalData](CBVFrameVariable* dst)
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
                dst->SkyboxTexIndex = shaderGlobalData.skyboxTex.GetResourceHeapIndex();
                dst->GGX_E_LUT_Index = shaderGlobalData.GGX_E_LUT_Index.GetResourceHeapIndex();
                dst->GGX_Eavg_LUT_Index = shaderGlobalData.GGX_Eavg_LUT_Index.GetResourceHeapIndex();
                dst->BlueNoiseTexIndex = shaderGlobalData.blueNoiseTexIndex.GetResourceHeapIndex();
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
                dst->GBuffer6Index = RenderTargetManager::GetInstance()
                                     .GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer6ID)
                                     ->GetResourceHeapIndex();
                dst->AOTexIndex = RenderTargetManager::GetInstance().GetRenderTexture(
                                                                        AOPass::RenderTextureIDs::AORTID)
                                                                    ->GetResourceHeapIndex();
                dst->g_EnableAO = UserData::GetInstance().aoParameter.IsEnableAO;
                dst->g_EnableShadow = UserData::GetInstance().shadowParameter.EnableShadow;
                dst->g_MipBias = std::max(-2.f, std::log2(screenPercentage));
                dst->g_ShadowRadius = UserData::GetInstance().shadowParameter.shadowRadius;
                dst->g_SobolBufferIndex = m_pSobol256spp256dBuffer->GetResourceHeapIndex();
                dst->g_ScramblingTileBufferIndex = m_pScramblingTileBuffer->GetResourceHeapIndex();
                dst->g_RankingTileBufferIndex = m_pRankingTileBuffer->GetResourceHeapIndex();
                dst->g_RandomSeed = randomSeed;
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