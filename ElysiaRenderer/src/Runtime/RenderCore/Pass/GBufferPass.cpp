#include "stdafx.h"
#include "GBufferPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/DX12Shader.h"

#include "Runtime/Resource/Model/ModelManager.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    using namespace ElysiaModel;

    int GBufferPass::ShaderPassIDs::GBufferPassID = -1;

    size_t GBufferPass::RenderTextureIDs::GBuffer0ID = PropertyToID(L"GBuffer_0");
    size_t GBufferPass::RenderTextureIDs::GBuffer1ID = PropertyToID(L"GBuffer_1");
    size_t GBufferPass::RenderTextureIDs::GBuffer2ID = PropertyToID(L"GBuffer_2");
    size_t GBufferPass::RenderTextureIDs::GBuffer3ID = PropertyToID(L"GBuffer_3");
    size_t GBufferPass::RenderTextureIDs::GBuffer4ID = PropertyToID(L"GBuffer_4");
    size_t GBufferPass::RenderTextureIDs::GBuffer5ID = PropertyToID(L"GBuffer_5");

    size_t GBufferPass::ShaderIDs::screenSize = PropertyToID(L"screenSize");
    size_t GBufferPass::ShaderIDs::viewMatrix = PropertyToID(L"viewMatrix");
    size_t GBufferPass::ShaderIDs::viewMatrix_I = PropertyToID(L"viewMatrix_I");
    size_t GBufferPass::ShaderIDs::projMatrix = PropertyToID(L"projMatrix");
    size_t GBufferPass::ShaderIDs::projMatrix_I = PropertyToID(L"projMatrix_I");
    size_t GBufferPass::ShaderIDs::viewProjMatrix = PropertyToID(L"viewProjMatrix");
    size_t GBufferPass::ShaderIDs::viewProjMatrix_I = PropertyToID(L"viewProjMatrix_I");
    size_t GBufferPass::ShaderIDs::worldMatrix = PropertyToID(L"worldMatrix");
    size_t GBufferPass::ShaderIDs::opacity = PropertyToID(L"opacity");
    size_t GBufferPass::ShaderIDs::cutoff = PropertyToID(L"cutoff");
    size_t GBufferPass::ShaderIDs::baseColorTexIndex = PropertyToID(L"baseColorTexIndex");
    size_t GBufferPass::ShaderIDs::normalTexIndex = PropertyToID(L"normalTexIndex");
    size_t GBufferPass::ShaderIDs::metallicTexIndex = PropertyToID(L"metallicTexIndex");
    size_t GBufferPass::ShaderIDs::roughnessTexIndex = PropertyToID(L"roughnessTexIndex");
    size_t GBufferPass::ShaderIDs::specularTexIndex = PropertyToID(L"specularTexIndex");
    size_t GBufferPass::ShaderIDs::baseColorTint = PropertyToID(L"baseColorTint");
    size_t GBufferPass::ShaderIDs::ambientCubemapTint = PropertyToID(L"ambientCubemapTint");
    size_t GBufferPass::ShaderIDs::normalIntensity = PropertyToID(L"normalIntensity");
    size_t GBufferPass::ShaderIDs::metallicIntensity = PropertyToID(L"metallicIntensity");
    size_t GBufferPass::ShaderIDs::roughnessIntensity = PropertyToID(L"roughnessIntensity");
    size_t GBufferPass::ShaderIDs::ambientCubemapIntensity = PropertyToID(
        L"ambientCubemapIntensity");

    GBufferPass::GBufferPass()
        : BasePass()
    {

    }

    GBufferPass::~GBufferPass()
    {
        Dispose();
    }

    void GBufferPass::Configure()
    {
        CreateRTs();

        m_shaderPasses =
        {
            ShaderPass
            {
                .Name = "GBuffer Pass",
                .FilePath = L"Shaders\\public\\GBuffer.hlsl",
            },
        };
        m_pMaterial = std::move(std::make_unique<Material>(m_pDevice, m_shaderPasses));
        ShaderPassIDs::GBufferPassID = m_pMaterial->FindPassIndex("GBuffer Pass");

        UpdatePipeline();
    }

    void GBufferPass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DrawGBufferPass(context);

        TAAData::Pre_View_M = m_pCamera->GetViewMat();
        TAAData::Pre_View_I_M = m_pCamera->GetViewMat().Invert();
        TAAData::Pre_Proj_M = m_pCamera->GetProjMat();
        TAAData::Pre_Proj_I_M = m_pCamera->GetProjMat().Invert();
        TAAData::Pre_ViewProj_M = m_pCamera->GetViewMat() * m_pCamera->GetProjMat();
        TAAData::Pre_ViewProj_I_M = (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).
            Invert();
    }

    void GBufferPass::Dispose()
    {
        m_GBufferRTs.clear();
    }

    void GBufferPass::CreateRTs()
    {
        m_GBufferRTs.clear();
        // Base Color , ShadingModel
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer0ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Metallic, Specular, Roughness, AO
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer1ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Encode World Tangent, Anisotropy
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R8G8B8A8_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer2ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Encode World Normal, per object data
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R10G10B10A2_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer3ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Emission, opacity
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R10G10B10A2_UNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer4ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }

        // Velocity
        {
            auto pGBufferRT = RenderTargetManager::GetInstance().CreateRenderTexture(
                static_cast<UINT64>(m_renderSize.x),
                static_cast<UINT64>(m_renderSize.y),
                DXGI_FORMAT_R16G16B16A16_SNORM,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::GBuffer5ID));

            m_GBufferRTs.emplace_back(std::move(pGBufferRT));
        }
    }

    std::vector<DX12TextureResource*> GBufferPass::GetGBuffers()
    {
        std::vector<DX12TextureResource*> temp{};
        temp.reserve(m_GBufferRTs.size());
        for (auto& RT : m_GBufferRTs)
        {
            temp.emplace_back(RT->GetTexture());
        }

        return temp;
    }

    void GBufferPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;
        UpdateGBufferPassVariant(ShaderPassIDs::GBufferPassID);
    }

    void GBufferPass::UpdateGBufferPassVariant(UINT passIndex)
    {
        std::vector<std::wstring> enableKeywords{};

        auto& passData = m_pMaterial->GetPassData(passIndex);
        auto VariantManager = passData.pShader->GetVariantManager();
        passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

        RenderTargetDesc RTDesc = RenderTargetDesc
        {
            .m_numRenderTargets = static_cast<UINT8>(m_GBufferRTs.size()),
            .m_depthStencilFormat = m_pCameraDepthRT->GetFormat(),
        };
        for (int i = 0; i < m_GBufferRTs.size(); ++i)
        {
            RTDesc.m_renderTargetFormats[i] = m_GBufferRTs[i]->GetFormat();
        }
        passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
            m_pDevice,
            m_pMaterial.get(),
            passIndex,
            RTDesc);
    }

    void GBufferPass::DrawMesh(ElysiaEngine::FrameContext& context, UINT passIndex)
    {
        auto& passData = m_pMaterial->GetPassData(passIndex);

        struct alignas(16)
        {
            float opacity;
            float cutoff;
            UINT baseColorTexIndex;
            UINT normalTexIndex;

            UINT metallicTexIndex;
            UINT roughnessTexIndex;
            UINT specularTexIndex;
            float metallicIntensity;

            Vector3 baseColorTint;
            float roughnessIntensity;

            float normalIntensity;
        } constantData;
        constexpr UINT constantSize = sizeof(constantData) / 4;

        for (auto& renderItem : context.renderList)
        {
            auto materialData = renderItem.loadedMaterial;

            constantData =
            {
                materialData.opacity,
                0.5,
                renderItem.textureIndices.Albedo,
                renderItem.textureIndices.Normal,

                renderItem.textureIndices.Metallic,
                renderItem.textureIndices.Roughness,
                renderItem.textureIndices.Specular,
                UserData::GetInstance().MetallicIntensity,

                UserData::GetInstance().BaseColorTint,
                UserData::GetInstance().RoughnessIntensity,

                UserData::GetInstance().NormalIntensity,
            };
            m_pCommand->SetPushConstants(PER_MATERIAL_SPACE, &constantData, constantSize);

            m_pMaterial->SetMatrix(ShaderIDs::worldMatrix, renderItem.worldMatrix);

            SetSpaceResource(passData, PER_OBJECT_SPACE);
            m_pCommand->Draw(renderItem.indexCount, renderItem.baseVertex, renderItem.startIndex);
        }
    }

    void GBufferPass::DrawGBufferPass(ElysiaEngine::FrameContext& context)
    {
        auto passID = ShaderPassIDs::GBufferPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            ShaderPassIDs::GBufferPassID).pPipelineStateObject;
        pipelineStateData.m_renderTargets = std::move(GetGBuffers());
        pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);

        for (auto& RT : m_GBufferRTs)
        {
            m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
        }
        m_pCommand->AddBarrier(m_pCameraDepthRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        for (auto& RT : m_GBufferRTs)
        {
            m_pCommand->ClearRenderTarget(RT, Color::Black);
        }
        m_pCommand->ClearDepthStencilTarget(m_pCameraDepthRT, 1.f, 0);
        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if (context.renderList.size())
        {
            m_pCommand->SetIndexBuffer(context.renderList[0].ibView);
            m_pCommand->SetVertexBuffer(0, 1, context.renderList[0].vbView);
        }

        m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                               GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
        m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                               m_pCamera->GetViewMat() * m_pCamera->GetProjMat());
        m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                               (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());
        m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix, TAAData::Pre_View_M, passID);
        m_pMaterial->SetMatrix(ShaderIDs::pre_viewMatrix_I, TAAData::Pre_View_I_M, passID);
        m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix, TAAData::Pre_Proj_M, passID);
        m_pMaterial->SetMatrix(ShaderIDs::pre_projMatrix_I, TAAData::Pre_Proj_I_M, passID);
        m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix,
                               TAAData::Pre_ViewProj_M,
                               passID);
        m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix_I,
                               TAAData::Pre_ViewProj_I_M,
                               passID);

        SetSpaceResource(passData, PER_PASS_SPACE);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        DrawMesh(context, ShaderPassIDs::GBufferPassID);

        for (auto& RT : m_GBufferRTs)
        {
            m_pCommand->AddBarrier(RT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, false);
        }
        m_pCommand->AddBarrier(m_pCameraDepthRT,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                               D3D12_RESOURCE_STATE_DEPTH_READ);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
}