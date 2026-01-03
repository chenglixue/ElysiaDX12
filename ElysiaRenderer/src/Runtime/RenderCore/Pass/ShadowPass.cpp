#include "stdafx.h"
#include "ShadowPass.h"

#include "Programs/PIXHelper.h"
#include "Programs/SobolSequenceGenerator.h"
#include "Programs/RenderHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/UploadRingBuffer.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/DX12Light.h"
#include "Runtime/RenderCore/DX12Shadow.h"
#include "Runtime/RenderCore/LightManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

#include "GBufferPass.h"
#include "Editor/UserData.h"
#include "Runtime/Resource/Model/ModelManager.h"
#include "Runtime/RenderCore/BufferManager.h"

namespace ElysiaRenderer
{
    int ShadowPass::ShaderPassIDs::ShadowCastPassID = -1;

    size_t ShadowPass::RenderTextureIDs::ShadowRTID = PropertyToID(L"Shadow RT");

    size_t ShadowPass::ShaderIDs::shadowNearZ = PropertyToID(L"shadowNearZ");
    size_t ShadowPass::ShaderIDs::shadowFarZ = PropertyToID(L"shadowFarZ");
    size_t ShadowPass::ShaderIDs::shadowDepthBias = PropertyToID(L"shadowDepthBias");
    size_t ShadowPass::ShaderIDs::shadowSlopeDepthBias = PropertyToID(L"shadowSlopeDepthBias");
    size_t ShadowPass::ShaderIDs::shadowMaxSlopeDepthBias =
        PropertyToID(L"shadowMaxSlopeDepthBias");
    size_t ShadowPass::ShaderIDs::g_sobolSequence = PropertyToID(L"g_sobolSequence");
    size_t ShadowPass::ShaderIDs::worldMatrix = PropertyToID(L"worldMatrix");
    size_t ShadowPass::ShaderIDs::baseColorTexIndex = PropertyToID(L"baseColorTexIndex");
    size_t ShadowPass::ShaderIDs::opacity = PropertyToID(L"opacity");
    size_t ShadowPass::ShaderIDs::cutoff = PropertyToID(L"cutoff");

    ShadowPass::ShadowPass(DX12Camera* pCamera) :
        BasePass(pCamera)
    {
    };
    ShadowPass::~ShadowPass()
    {
        Dispose();
    }
    void ShadowPass::Dispose()
    {

    }

    void ShadowPass::Configure()
    {
        m_pMainLight = LightManager::GetInstance().GetMainLight();

        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "Shadow Cast Pass",
                .FilePath = L"Shaders\\public\\Shadow.hlsl",
            }
        };
        m_pMaterial = std::move(std::make_unique<Material>(m_pDevice, m_shaderPasses));
        ShaderPassIDs::ShadowCastPassID = m_pMaterial->FindPassIndex("Shadow Cast Pass");

        m_sobolSqeuences = Create2DSobolSqeuence(64);
        UpdatePipeline();
    }
    void ShadowPass::Render(ElysiaEngine::FrameContext& context)
    {
        PIXHelper pix(m_pCommand->GetCommandList(), "Shadow Pass");

        m_pMaterial->SetFloat(ShaderIDs::shadowNearZ,
                              LightManager::GetInstance().GetMainShadow()->GetNearZ());
        m_pMaterial->SetFloat(ShaderIDs::shadowFarZ,
                              LightManager::GetInstance().GetMainShadow()->GetFarZ());
        m_pMaterial->SetFloat(ShaderIDs::shadowDepthBias,
                              UserData::GetInstance().shadowDepthBias / 100);
        m_pMaterial->SetFloat(ShaderIDs::shadowSlopeDepthBias,
                              UserData::GetInstance().shadowSlopeDepthBias / 100);
        m_pMaterial->SetFloat(ShaderIDs::shadowMaxSlopeDepthBias,
                              UserData::GetInstance().shadowMaxSlopeDepthBias / 100);
        m_pMaterial->SetVector2Array(ShaderIDs::g_sobolSequence, m_sobolSqeuences);

        DrawShadowPass(context);
    }

    void ShadowPass::UpdatePipeline()
    {
        UpdateShadowPassVariant(ShaderPassIDs::ShadowCastPassID);
    }
    void ShadowPass::UpdateShadowPassVariant(UINT passIndex)
    {
        std::vector<std::wstring> enableKeywords{};

        switch (UserData::GetInstance().shadowQuality)
        {
        case ShadowQuality::Low:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_LOW");
            break;
        }
        case ShadowQuality::Middle:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_MIDDLE");
            break;
        }
        case ShadowQuality::High:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_HIGH");
            break;
        }
        case ShadowQuality::VeryHigh:
        {
            enableKeywords.emplace_back(L"SHADOW_QUALITY_VERYHIGH");
            break;
        }
        }
        switch (UserData::GetInstance().shadowType)
        {
        case ShadowType::Hard:
        {
            enableKeywords.emplace_back(L"HARD_SHADOW");
            break;
        }
        case ShadowType::Soft:
        {
            enableKeywords.emplace_back(L"SOFT_SHADOW");
            break;
        }
        }

        auto& passData = m_pMaterial->GetPassData(passIndex);
        auto VariantManager = passData.pShader->GetVariantManager();
        auto currVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);
        passData.pCurrVariantData = currVariantData;

        RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
        RTDesc.m_numRenderTargets = 0;
        RTDesc.m_depthStencilFormat = LightManager::GetInstance().GetMainShadowRT()->
                                                                  GetFormat();
        passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
            m_pDevice, m_pMaterial.get(), passIndex, RTDesc);
    }

    void ShadowPass::DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex)
    {
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        auto& passData = m_pMaterial->GetPassData(passIndex);
        SetSpaceResource(passData, PER_PASS_SPACE);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        if (context.renderList.size())
        {
            m_pCommand->SetIndexBuffer(context.renderList[0].ibView);
            m_pCommand->SetVertexBuffer(0, 1, context.renderList[0].vbView);
        }

        for (const auto& renderItem : context.renderList)
        {
            auto materialData = renderItem.loadedMaterial;

            m_pMaterial->SetMatrix(ShaderIDs::worldMatrix, renderItem.worldMatrix);
            m_pMaterial->SetUInt(ShaderIDs::baseColorTexIndex, renderItem.textureIndices.Albedo);
            m_pMaterial->SetFloat(ShaderIDs::cutoff, 0.5f);
            m_pMaterial->SetFloat(ShaderIDs::opacity, materialData.opacity);

            SetSpaceResource(passData, PER_OBJECT_SPACE);
            SetSpaceResource(passData, PER_MATERIAL_SPACE);
            m_pCommand->Draw(renderItem.indexCount, renderItem.baseVertex, renderItem.startIndex);
        }
    }
    void ShadowPass::DrawShadowPass(ElysiaEngine::FrameContext& context)
    {
        auto pShadowRT = LightManager::GetInstance().GetMainShadowRT();
        m_pCommand->AddBarrier(pShadowRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_pCommand->ClearDepthStencilTarget(pShadowRT, 1.f, 0);

        if (IsRenderTextureReady({pShadowRT}))
        {
            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
                ShaderPassIDs::ShadowCastPassID).pPipelineStateObject;
            pipelineStateData.m_renderTargets = {};
            pipelineStateData.m_depthStencilTarget = pShadowRT->GetTexture();
            m_pCommand->SetPipeline(pipelineStateData);

            m_pCommand->SetViewport(
                reinterpret_cast<DX12DirectionLight*>(m_pMainLight)->GetMainShadow()->
                                                                     GetViewport());
            m_pCommand->SetScissorRect(
                reinterpret_cast<DX12DirectionLight*>(m_pMainLight)->GetMainShadow()->
                                                                     GetScissorRect());
            DrawMesh(context, ShaderPassIDs::ShadowCastPassID);
        }

        m_pCommand->AddBarrier(pShadowRT, D3D12_RESOURCE_STATE_DEPTH_READ);
    }
}