#include "stdafx.h"
#include "SSSRPass.h"

#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"
#include "Editor/IMGUIHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/SwapChain.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    SSSRPass::SSSRPass()
    {

    }

    SSSRPass::~SSSRPass()
    {
        Dispose();
    }

    void SSSRPass::Dispose()
    {

    }

    void SSSRPass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_cameraWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_cameraHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);

        uint32_t elementSize = 4;
        m_pRayCounterBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"SSSR Ray Counter Buffer",
            .stride = elementSize,
            .size = 4ull * elementSize,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pRayListBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"SSSR Ray List Buffer",
            .stride = elementSize,
            .size = m_cameraWidth * m_cameraHeight * elementSize,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pIntersectionOutputRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_cameraWidth,
            m_cameraHeight,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            true,
            L"SSSR Intersection Output RT");

        m_pIntersectionIndirectArgsBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"SSSR Intersection Args Buffer",
            .stride = elementSize,
            .size = 6ull * elementSize,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
        });

        m_pRayCounterReadBackBuffer = BufferManager::GetInstance().CreateReadBackBuffer(
            sizeof(int),
            "SSSR Ray Counter Read Back Buffer");

        m_pIntersectionArgsReadBackBuffer = BufferManager::GetInstance().CreateReadBackBuffer(
            sizeof(UINT) * 6ull,
            "SSSR Intersection Args Read Back Buffer");

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }

    void SSSRPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < SSSR_PASS_COUNT; ++i)
        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = PassID(i);
            auto& passData = m_pMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject =
                PSOManager::GetInstance().GetComputePipelineState(
                    m_pDevice,
                    m_pMaterial.get(),
                    passID);
        }
    }

    void SSSRPass::Render(FrameContext& context)
    {
        if (!UserData::GetInstance().bloomParameter.enable)
            return;
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_frameIndex = context.frameIndex;

        DoTileClassify();
        DoIntersectionArgs();
    }

    void SSSRPass::DoClearRayCounter()
    {
        auto passID = SSSR_CLEAR_RAY_COUNTER_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_RayCounterBufferIndex,
                                 m_pRayCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);

            SetSpaceResource(passData, PER_PASS_SPACE);
            m_pCommand->Dispatch(1, 1, 1);
        }
        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

    void SSSRPass::DoTileClassify()
    {
        auto passID = SSSR_TILE_CLASSIFY_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pRayListBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(m_pIntersectionOutputRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix_I,
                                   m_pCamera->GetViewMat().Invert(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix, m_pCamera->GetProjMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I,
                                   m_pCamera->GetProjMat().Invert(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix,
                                   m_pCamera->GetViewMat() * m_pCamera->GetProjMat(),
                                   passID);
            m_pMaterial->SetMatrix(ShaderIDs::viewProjMatrix_I,
                                   (m_pCamera->GetViewMat() * m_pCamera->GetProjMat()).Invert(),
                                   passID);

            m_pMaterial->SetUINT(ShaderIDs::g_RayCounterBufferIndex,
                                 m_pRayCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_RayListBufferIndex,
                                 m_pRayListBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_IntersectionOutputTexIndex,
                                 m_pIntersectionOutputRT->GetUAVResourceHeapIndex(),
                                 passID);

            m_pMaterial->SetFloat4(ShaderIDs::g_DestSize, GetScreenSize(m_cameraWidth, m_cameraHeight), passID);
            m_pMaterial->SetFloat(ShaderIDs::g_RoughnessThreshold,
                                  UserData::GetInstance().sssrParameter.roughnessThreshold,
                                  passID);
            m_pMaterial->SetUINT(ShaderIDs::g_SamplesPerQuad,
                                 UserData::GetInstance().sssrParameter.samplesPerQuad,
                                 passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_cameraWidth, threadGroupSize.x),
                                 CeilDivide(m_cameraHeight, threadGroupSize.y),
                                 1);
        }
        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(*m_pRayListBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pIntersectionOutputRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_pCommand->GetCommandList()->CopyResource(m_pRayCounterReadBackBuffer->GetResource().Get(),
                                                   m_pRayCounterBuffer->GetResource().Get());
        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        DoTileClassifyDebug();

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

    void SSSRPass::DoTileClassifyDebug()
    {
        uint32_t* pMappedData = nullptr;
        D3D12_RANGE readRange{0, sizeof(uint32_t)};

        HRESULT hr = m_pRayCounterReadBackBuffer->GetResource()->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&pMappedData));

        int m_rayCounter = 0;
        if (SUCCEEDED(hr))
        {
            m_rayCounter = *pMappedData;
            ElysiaHelper::Log::Info("SSSR Total Ray Counter: %i", m_rayCounter);

            D3D12_RANGE writeRange{0, 0};
            m_pRayCounterReadBackBuffer->GetResource()->Unmap(0, &writeRange);
        }
    }

    void SSSRPass::DoIntersectionArgs()
    {
        auto passID = SSSR_INTERSECT_ARGS_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(*m_pIntersectionIndirectArgsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_RayCounterBufferIndex,
                                 m_pRayCounterBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_IntersectionArgsBufferIndex,
                                 m_pIntersectionIndirectArgsBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->Dispatch(1, 1, 1);
        }
        m_pCommand->AddBarrier(*m_pIntersectionIndirectArgsBuffer,
                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                               false);
        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_pCommand->GetCommandList()->CopyResource(m_pRayCounterReadBackBuffer->GetResource().Get(),
                                                   m_pRayCounterBuffer->GetResource().Get());
        uint32_t* pMappedData = nullptr;
        D3D12_RANGE readRange{0, sizeof(UINT) * 6ull};

        HRESULT hr = m_pRayCounterReadBackBuffer->GetResource()->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&pMappedData));

        uint32_t* rayCounter;
        if (SUCCEEDED(hr))
        {
            rayCounter = pMappedData;
            ElysiaHelper::Log::Info("SSSR Intersection Args: %i", rayCounter[0]);

            D3D12_RANGE writeRange{0, 0};
            m_pRayCounterReadBackBuffer->GetResource()->Unmap(0, &writeRange);
        }

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

}