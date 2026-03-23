#include "stdafx.h"
#include "SkyboxPass.h"

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

        m_pCommand->SetDefaultViewportAndScissor(ElysiaHelper::UINT2(m_renderSize));
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        m_pCommand->SetIndexBuffer(m_indexView);
        m_pCommand->SetVertexBuffer(0, 1, m_vertexView);

        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        {
            Matrix viewNoTranslate = m_pCamera->GetViewMat();
            viewNoTranslate.Translation(Vector3::Zero); // 抹除位移
            Matrix viewProjInv = (viewNoTranslate * m_pCamera->GetProjMat()).Invert();
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   m_pCamera->GetViewMat() * viewProjInv);

            m_pMaterial->SetFloat4(ShaderIDs::screenSize,
                                   GetScreenSize(Vector2(m_renderSize.x, m_renderSize.y)));
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat());
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I, m_pCamera->GetViewMat().Invert());
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat());
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert());
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   viewNoTranslate * m_pCamera->GetProjMat());
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert());
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->DrawInstanced(NumIndices, 1, 0, 0, 0);
        }
        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), passName);
    }
}