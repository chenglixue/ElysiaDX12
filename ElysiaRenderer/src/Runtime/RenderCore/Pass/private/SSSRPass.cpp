#include "stdafx.h"
#include "../public/SSSRPass.h"

#include "Editor/public/UserData.h"
#include "Programs/public/PIXHelper.h"
#include "Editor/public/IMGUIHelper.h"
#include "Programs/public/Log.h"

#include "Runtime/Core/public/DX12GraphicsContext.h"
#include "Runtime/Core/public/DX12Shader.h"
#include "Runtime/Core/public/DX12Device.h"
#include "Runtime/Core/public/DX12RenderPassDescriptorHeap.h"
#include "Runtime/Core/public/DX12TextureBuffer.h"
#include "Runtime/Core/public/SwapChain.h"

#include "Runtime/RenderCore/public/AutoExposureUtility.h"
#include "Runtime/RenderCore/public/BufferManager.h"
#include "Runtime/RenderCore/public/RenderResource.h"
#include "Runtime/RenderCore/public/RenderTexture.h"
#include "Runtime/RenderCore/public/Material.h"
#include "Runtime/RenderCore/public/PSOManager.h"
#include "Runtime/RenderCore/public/RenderPassResourceManager.h"
#include "Runtime/RenderCore/public/RenderTargetManager.h"
#include "Runtime/RenderCore/public/ShaderVariantManager.h"
#include "Runtime/RenderCore/Pass/public/AutoExposurePass.h"

namespace ElysiaRenderer
{
    static constexpr int SSSR_SPD_GROUP_SIZE = 8;
    static constexpr int SSSR_TILE_CLASSIFY_GROUP_SIZE = 8;
    static constexpr int SSSR_INTERSECT_ARGS_GROUP_SIZE = 1;
    static constexpr int SSSR_INTERSECT_GROUP_SIZE = 8;
    static constexpr int SSSR_COMPOSITE_GROUP_SIZE = 8;

    SSSRPass::SSSRPass()
    {

    }

    SSSRPass::~SSSRPass()
    {
        Dispose();
    }

    void SSSRPass::DestroyGPUBuffers()
    {
        auto destroyBuffer = [](BufferHandle& handle)
        {
            if (handle)
            {
                BufferManager::GetInstance().DestoryBuffer(handle);
                handle.reset();
            }
        };

        destroyBuffer(m_pRayCounterBuffer);
        destroyBuffer(m_pRayListBuffer);
        destroyBuffer(m_pIntersectionIndirectArgsBuffer);
        destroyBuffer(m_pRayCounterReadBackBuffer);
        destroyBuffer(m_pIntersectionArgsReadBackBuffer);
    }

    void SSSRPass::Dispose()
    {
        DestroyGPUBuffers();
        m_pCommandSignature.Reset();
        m_pHIZRT = nullptr;
        m_pIntersectionOutputRT = nullptr;
        m_pTileClassifyDebugRT = nullptr;
    }

    void SSSRPass::Configure()
    {
        DestroyGPUBuffers();

        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_cameraWidth = (UINT)std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_cameraHeight = (UINT)std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);
        if (m_cameraWidth < 1)
            m_cameraWidth = 1;
        if (m_cameraHeight < 1)
            m_cameraHeight = 1;
        m_HIZWidth = m_cameraWidth;
        m_HIZHeight = m_cameraHeight;

        m_HIZMipmapCount = 1;
        {
            const UINT maxDim = std::max(m_HIZWidth, m_HIZHeight);
            if (maxDim > 1)
            {
                m_HIZMipmapCount = static_cast<UINT>(std::floor(std::log2(static_cast<double>(maxDim)))) + 1;
            }
            if (m_HIZMipmapCount < 1)
                m_HIZMipmapCount = 1;
            if (m_HIZMipmapCount > ElysiaHelper::MAX_TEXTURE_SUBRESOURCE_COUNT)
                m_HIZMipmapCount = ElysiaHelper::MAX_TEXTURE_SUBRESOURCE_COUNT;
        }
        
        m_pHIZRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_cameraWidth,
            m_cameraHeight,
            DXGI_FORMAT_R32_FLOAT,
            true,
            m_HIZMipmapCount,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::HIZRTID));

        uint32_t elementSize = 4;
        const uint32_t zeroCounter[4] = {};
        m_pRayCounterBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"SSSR Ray Counter Buffer",
            .stride = elementSize,
            .size = 4ull * elementSize,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .InitData = zeroCounter,
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
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::IntersectionOutputRTID));

        m_pTileClassifyDebugRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
            m_cameraWidth,
            m_cameraHeight,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            true,
            RenderResource::GetInstance().GetPropertyName(RenderTextureIDs::TileClassifyDebugRTID));

        const uint32_t zeroArgs[6] = {};
        m_pIntersectionIndirectArgsBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"SSSR Intersection Args Buffer",
            .stride = elementSize,
            .size = 6ull * elementSize,
            .viewFlags = GPUResourceFlags::UAV | GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::GPUOnly,
            .InitData = zeroArgs,
            .isIndirectBuffer = true,
        });

        m_pRayCounterReadBackBuffer = BufferManager::GetInstance().CreateReadBackBuffer(
            sizeof(int),
            "SSSR Ray Counter Read Back Buffer");

        m_pIntersectionArgsReadBackBuffer = BufferManager::GetInstance().CreateReadBackBuffer(
            sizeof(UINT) * 6ull,
            "SSSR Intersection Args Read Back Buffer");

        m_shaderPasses.assign(std::begin(PassData), std::end(PassData));
        m_shaderPasses[SSSR_COPY_PASS].Environment.SetDefine(L"SSSR_SPD_GROUP_SIZE", SSSR_SPD_GROUP_SIZE);
        m_shaderPasses[SSSR_SPD_PASS].Environment.SetDefine(L"SSSR_SPD_GROUP_SIZE", SSSR_SPD_GROUP_SIZE);
        m_shaderPasses[SSSR_TILE_CLASSIFY_PASS].Environment.SetDefine(L"SSSR_TILE_CLASSIFY_GROUP_SIZE", SSSR_TILE_CLASSIFY_GROUP_SIZE);
        m_shaderPasses[SSSR_INTERSECT_ARGS_PASS].Environment.SetDefine(L"SSSR_INTERSECT_ARGS_GROUP_SIZE", SSSR_INTERSECT_ARGS_GROUP_SIZE);
        m_shaderPasses[SSSR_INTERSECT_ARGS_PASS].Environment.SetDefine(L"SSSR_INTERSECT_GROUP_SIZE", SSSR_INTERSECT_GROUP_SIZE);
        m_shaderPasses[SSSR_INTERSECT_PASS].Environment.SetDefine(L"SSSR_INTERSECT_GROUP_SIZE", SSSR_INTERSECT_GROUP_SIZE);
        m_shaderPasses[SSSR_COMPOSITE_PASS].Environment.SetDefine(L"SSSR_COMPOSITE_GROUP_SIZE", SSSR_COMPOSITE_GROUP_SIZE);
        
        m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
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

        if (!m_pCommandSignature)
        {
            auto& passData = m_pMaterial->GetPassData(SSSR_INTERSECT_PASS);

            D3D12_INDIRECT_ARGUMENT_DESC args[1] = {};
            args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

            D3D12_COMMAND_SIGNATURE_DESC desc = {};
            desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
            desc.NumArgumentDescs = 1;
            desc.pArgumentDescs = args;

            HRESULT hr = m_pDevice->GetDevice()->CreateCommandSignature(&desc,
                                                                       nullptr,
                                                                       IID_PPV_ARGS(&m_pCommandSignature));
            if (FAILED(hr))
            {
                ElysiaHelper::Log::Error("SSSR: CreateCommandSignature failed (0x%08X)", hr);
                m_pCommandSignature.Reset();
            }
        }
    }

    void SSSRPass::Render(FrameContext& context)
    {
        const auto& sssrParam = UserData::GetInstance().sssrParameter;
        const DebugMode debugMode = UserData::GetInstance().debugMode;
        const bool debugActive = IsSSSRDebugActive(debugMode, sssrParam);
        if (!sssrParam.enable && !debugActive)
            return;

        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;
        m_frameIndex = context.frameIndex;
        if (!m_pCamera || !m_pIntersectionOutputRT)
            return;

        DoSPD();

        const bool needIntersect =
            sssrParam.enable || IsSSSRIntersectionDebugActive(debugMode, sssrParam);
        const bool needClassify =
            needIntersect || IsSSSRTileClassifyDebugActive(debugMode, sssrParam);
        if (needClassify)
        {
            DoClearRayCounter();
            DoTileClassify();
        }

        if (needIntersect)
        {
            DoIntersectionArgs();
            DoIntersection();
            // Leave Camera Color alone while the viewport is showing reflection only.
            if (sssrParam.enable && !IsSSSRIntersectionDebugActive(debugMode, sssrParam))
                DoComposite();
        }
    }

    void SSSRPass::BindCommon(UINT passID)
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
    }
    
    void SSSRPass::DoSPD()
    {
        auto CopyCameraDepthToSPD = [&]()
        {
            auto passID = SSSR_COPY_PASS;
            auto& passData = m_pMaterial->GetPassData(passID);
            auto passName = passData.Name.c_str();
            PIXHelper pix(m_pCommand->GetCommandList(), passName);
            
            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);
            SetSpaceResource(passData, PER_FRAME_SPACE);
            
            m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            {
                m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex,
                                      m_pHIZRT->GetUAVResourceHeapIndex(0),
                                      passID);
                m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_HIZWidth, m_HIZHeight),
                                   passID);
                
                SetSpaceResource(passData, PER_PASS_SPACE);
                
                auto threadGroupSize = passData.GetKernelThreadGroupSizes();
                m_pCommand->Dispatch(CeilDivide(m_HIZWidth, threadGroupSize.x),
                                     CeilDivide(m_HIZHeight, threadGroupSize.y),
                                     1);
            }
            m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
        };
        
        CopyCameraDepthToSPD();
        
        auto passID = SSSR_SPD_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);
        
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);
        
        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        
        UINT64 currWidth = m_HIZWidth;
        UINT64 currHeight = m_HIZHeight;
        
        for (UINT i = 1; i < m_HIZMipmapCount; ++i)
        {
            auto lastWidth = currWidth;
            auto lastHeight = currHeight;
            currWidth = MathHelper::Max(UINT64(1), (currWidth + 1) >> 1);
            currHeight = MathHelper::Max(UINT64(1), (currHeight + 1) >> 1);
            
            m_pMaterial->SetUINT(ShaderIDs::g_SourceTexIndex,
                                   m_pHIZRT->GetUAVResourceHeapIndex(i - 1),
                                   passID);
            m_pMaterial->SetUINT(ShaderIDs::g_TargetTexIndex,
                                  m_pHIZRT->GetUAVResourceHeapIndex(i),
                                  passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(currWidth, currHeight),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SourceSize,
                                   GetScreenSize(lastWidth, lastHeight),
                                   passID);
            
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(currWidth, threadGroupSize.x),
                                 CeilDivide(currHeight, threadGroupSize.y),
                                 1);
            m_pCommand->AddUAVBarrier(m_pHIZRT);
        }
        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        
        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

    void SSSRPass::DoClearRayCounter()
    {
        auto passID = SSSR_CLEAR_COUNTER_PASS;
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
        m_pCommand->AddUAVBarrier(m_pRayCounterBuffer, false);
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

        const auto& sssrParam = UserData::GetInstance().sssrParameter;
        const bool writeClassifyDebug =
            IsSSSRTileClassifyDebugActive(UserData::GetInstance().debugMode, sssrParam);

        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pRayListBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(m_pIntersectionOutputRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (writeClassifyDebug)
            m_pCommand->AddBarrier(m_pTileClassifyDebugRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            BindCommon(passID);

            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize, GetScreenSize(m_cameraWidth, m_cameraHeight), passID);
            m_pMaterial->SetFloat(ShaderIDs::g_RoughnessThreshold,
                                  sssrParam.roughnessThreshold,
                                  passID);
            m_pMaterial->SetUINT(ShaderIDs::g_SamplesPerQuad,
                                 ClampSSSRSamplesPerQuad(static_cast<int>(sssrParam.samplesPerQuad)),
                                 passID);
            // Always bind a valid UAV index; the shader skips writes when the view
            // sentinel is SSSR_TILE_CLASSIFY_DEBUG_DISABLED (0xFFFFFFFF).
            m_pMaterial->SetUINT(ShaderIDs::g_TileClassifyDebugTexIndex,
                                 m_pTileClassifyDebugRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_TileClassifyDebugView,
                                 writeClassifyDebug
                                     ? static_cast<UINT>(sssrParam.tileClassifyDebugView)
                                     : 0xFFFFFFFFu,
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
        if (writeClassifyDebug)
            m_pCommand->AddBarrier(m_pTileClassifyDebugRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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
                               D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                               false);
        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

    void SSSRPass::DoIntersection()
    {
        if (!m_pCommandSignature)
            return;

        auto passID = SSSR_INTERSECT_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        const auto& sssrParam = UserData::GetInstance().sssrParameter;

        m_pCommand->AddBarrier(*m_pRayCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(*m_pRayListBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, false);
        m_pCommand->AddBarrier(m_pHIZRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        if (m_pCameraColorRT)
            m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pIntersectionOutputRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            BindCommon(passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_cameraWidth, m_cameraHeight),
                                   passID);
            m_pMaterial->SetUINT(ShaderIDs::g_HIZTexIndex,
                                 m_pHIZRT->GetSRVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_CameraColorTexIndex,
                                 m_pCameraColorRT ? m_pCameraColorRT->GetSRVResourceHeapIndex() : 0u,
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_SobolNoiseTexIndex,
                                RenderPassResourceManager::GetInstance().Get<ShaderGlobalData>().sobolNoiseTex->
                                                 GetSRVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_MaxRayTrace,
                                 ClampSSSRMaxRaySteps(static_cast<int>(sssrParam.maxRaySteps)),
                                 passID);
            m_pMaterial->SetFloat(ShaderIDs::g_Thickness,
                                  sssrParam.thickness < 0.001f ? 0.001f : sssrParam.thickness,
                                  passID);

            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->GetCommandList()->ExecuteIndirect(m_pCommandSignature.Get(),
                                                          1,
                                                          m_pIntersectionIndirectArgsBuffer->GetResource().Get(),
                                                          0,
                                                          nullptr,
                                                          0);
        }
        m_pCommand->AddBarrier(m_pIntersectionOutputRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

    void SSSRPass::DoComposite()
    {
        if (!m_pCameraColorRT)
            return;

        auto passID = SSSR_COMPOSITE_PASS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        const auto& sssrParam = UserData::GetInstance().sssrParameter;

        m_pCommand->AddBarrier(m_pIntersectionOutputRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, false);
        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetMatrix(ShaderIDs::viewMatrix, m_pCamera->GetViewMat(), passID);
            m_pMaterial->SetMatrix(ShaderIDs::projMatrix_I, m_pCamera->GetProjMat().Invert(), passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(m_cameraWidth, m_cameraHeight),
                                   passID);
            m_pMaterial->SetFloat(ShaderIDs::g_Intensity,
                                  sssrParam.intensity < 0.0f ? 0.0f : sssrParam.intensity,
                                  passID);
            m_pMaterial->SetUINT(ShaderIDs::g_IntersectionOutputTexIndex,
                                 m_pIntersectionOutputRT->GetSRVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_CameraColorUAV,
                                 m_pCameraColorRT->GetUAVResourceHeapIndex(),
                                 passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_cameraWidth, threadGroupSize.x),
                                 CeilDivide(m_cameraHeight, threadGroupSize.y),
                                 1);
        }
        m_pCommand->AddUAVBarrier(m_pCameraColorRT, false);
        m_pCommand->AddBarrier(m_pCameraColorRT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("SSSR/") + passName).c_str());
    }

}
