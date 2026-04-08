#include "stdafx.h"
#include "SkyboxPass.h"

#include "GBufferPass.h"
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

#include "Editor/UserData.h"
#include "Runtime/RenderCore/DX12Camera.h"


namespace ElysiaRenderer
{
    SkyboxPass::SkyboxPass()
    {
        BufferCreationDesc vertexBufferDesc =
        {
            .name = L"Skybox Vertex Buffer",
            .stride = sizeof(Vector3),
            .size = sizeof(Vector3) * NumVertices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_vertices
        };
        BufferCreationDesc indexBufferDesc =
        {
            .name = L"Skybox Index Buffer",
            .stride = 0,
            .size = sizeof(UINT16) * NumIndices,
            .viewFlags = GPUResourceFlags::None,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .isRawAccess = false,
            .InitData = m_indices
        };
        m_vertexBuffer = BufferManager::GetInstance().CreateVertexBuffer(vertexBufferDesc);
        m_indexBuffer = BufferManager::GetInstance().CreateIndexBuffer(indexBufferDesc);

        m_vertexView = D3D12_VERTEX_BUFFER_VIEW
        {
            .BufferLocation = m_vertexBuffer->GetGPUAddress(),
            .SizeInBytes = static_cast<UINT>(NumVertices) * m_vertexBuffer->GetStride(),
            .StrideInBytes = m_vertexBuffer->GetStride()
        };
        m_indexView =
        {
            .BufferLocation = m_indexBuffer->GetGPUAddress(),
            .SizeInBytes = NumIndices * ElysiaModel::IndexSize(),
            .Format = ElysiaModel::IndexBufferFormat(),
        };
    }
    SkyboxPass::~SkyboxPass()
    {
        Dispose();
    }
    void SkyboxPass::Dispose()
    {

    }

    void SkyboxPass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_cameraWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_cameraHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
            UpdatePipeline();
        }
    }
    void SkyboxPass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        DrawSkybox();
    }

    void SkyboxPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        std::vector<std::wstring> enableKeywords{};
        auto passID = Draw_SKY_BOX_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto VariantManager = passData.pShader->GetVariantManager();
        auto currVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);
        passData.pCurrVariantData = currVariantData;

        RenderTargetDesc RTDesc = CreateDefaultRenderTargetDesc();
        RTDesc.m_numRenderTargets = 1;
        RTDesc.m_renderTargetFormats = {m_pCameraColorRT->GetFormat()};
        RTDesc.m_depthStencilFormat = m_pCameraDepthRT->GetFormat();
        passData.pPipelineStateObject = PSOManager::GetInstance().GetGraphicsPipelineState(
            m_pDevice,
            m_pMaterial.get(),
            passID,
            RTDesc);
    }

    void SkyboxPass::DrawSkybox()
    {
        if (!m_vertexBuffer->GetIsReady() || !m_indexBuffer->GetIsReady())
        {
            return;
        }

        auto passID = Draw_SKY_BOX_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        pipelineStateData.m_renderTargets = {m_pCameraColorRT->GetTexture()};
        pipelineStateData.m_depthStencilTarget = m_pCameraDepthRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_cameraWidth, m_cameraHeight));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pCommand->SetIndexBuffer(m_indexView);
        m_pCommand->SetVertexBuffer(0, 1, m_vertexView);

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET, false);
        m_pCommand->AddBarrier(
            RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer5ID),
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        {
            Matrix viewNoTranslateMat = m_pCamera->GetViewMat();
            viewNoTranslateMat.Translation(Vector3::Zero); // 抹除位移

            auto preViewNoTranslateMat = GBufferPass::TAAData::Pre_View_M;
            preViewNoTranslateMat.Translation(Vector3::Zero); // 抹除位移
            Matrix nonJitterProjMat = m_pCamera->GetProjMat();

            auto jitterUV = GBufferPass::m_currJitterUV;
            auto jitterMatrixProj = m_pCamera->GetProjMat();
            jitterMatrixProj.m[2][0] += jitterUV.x * 2.f / m_displayWidth * UserData::GetInstance().taaParameter.
                                                                                                    jitterIntensity;
            jitterMatrixProj.m[2][1] -= jitterUV.y * 2.f / m_displayHeight * UserData::GetInstance().taaParameter.
                                                                                                     jitterIntensity;

            m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                                   GetScreenSize(Vector2(m_cameraWidth, m_cameraHeight)));
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, viewNoTranslateMat);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, viewNoTranslateMat.Invert());
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, jitterMatrixProj);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, nonJitterProjMat.Invert());
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   viewNoTranslateMat * nonJitterProjMat);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (viewNoTranslateMat * nonJitterProjMat).Invert());
            m_pMaterial->SetMatrix(ShaderIDs::jitterViewProjMatrix,
                                   viewNoTranslateMat * jitterMatrixProj);
            m_pMaterial->SetMatrix(ShaderIDs::pre_viewProjMatrix,
                                   preViewNoTranslateMat * GBufferPass::TAAData::Pre_Proj_M);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->DrawInstanced(NumIndices, 1, 0, 0, 0);
        }
        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_pCommand->AddBarrier(
            RenderTargetManager::GetInstance().GetRenderTexture(GBufferPass::RenderTextureIDs::GBuffer5ID),
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Skybox/") + passName).c_str());
    }
}