#include "stdafx.h"
#include "BakePass.h"

#include "PreDrawPass.h"
#include "Editor/UserData.h"
#include "Programs/PIXHelper.h"

#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/DX12UploadContext.h"
#include "Runtime/RenderCore/BakeManager.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/DX12Camera.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/CameraManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderPassResourceManager.h"
#include "Runtime/RenderCore/SceneManager.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"

namespace ElysiaRenderer
{
    BakePass::BakePass()
    {

    }
    BakePass::~BakePass()
    {
        Dispose();
    }
    void BakePass::Dispose()
    {
    }

    void BakePass::Configure()
    {
        m_displayWidth = (UINT)m_displaySize.x;
        m_displayHeight = (UINT)m_displaySize.y;
        m_cameraWidth = std::floor(m_displaySize.x * UserData::GetInstance().taaParameter.sampleRate);
        m_cameraHeight = std::floor(m_displaySize.y * UserData::GetInstance().taaParameter.sampleRate);

        m_subsurfaceScatterData =
        {
            .pPreIntegrateSSSLUT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                static_cast<UINT64>(1024),
                static_cast<UINT64>(1024),
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                true,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::PreIntegrateSSSLUTID)),
            .pNDFLUT = RenderTargetManager::GetInstance().CreateRWRenderTexture(
                static_cast<UINT64>(1024),
                static_cast<UINT64>(1024),
                DXGI_FORMAT_R16_FLOAT,
                true,
                RenderResource::GetInstance().
                GetPropertyName(RenderTextureIDs::IntegrateSSSNDFLUTID))
        };
        RenderPassResourceManager::GetInstance().Create<SubsurfaceScatterData>(&m_subsurfaceScatterData);

        auto skyboxTex = RenderPassResourceManager::GetInstance().Get<ShaderGlobalData>().skyboxTex;
        UINT groupCountX = m_SHCoefficientsTempCount.x = CeilDivide(skyboxTex.GetWidth(), 8);
        UINT groupCountY = m_SHCoefficientsTempCount.y = CeilDivide(skyboxTex.GetHeight(), 8);
        UINT groupCountZ = m_SHCoefficientsTempCount.z = 6;
        auto SHCoefficientsTempCount = groupCountX * groupCountY * groupCountZ;
        if (!m_pSHCoefficientsTempBuffer || m_pSHCoefficientsTempBuffer->GetResourceDesc().Width != AlignU32(
                (UINT)sizeof(SHCoefficientData) * SHCoefficientsTempCount,
                D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
        {
            m_pSHCoefficientsTempBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"SH Coefficients Temp Buffer",
                .stride = sizeof(SHCoefficientData),
                .size = sizeof(SHCoefficientData) * SHCoefficientsTempCount,
                .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
                .accessFlags = BufferAccessFlags::GPUOnly,
                .isRawAccess = false
            });
            m_bIsBakeSHCoefficients = false;
        }

        if (!m_GIData.pSHCoefficientsBuffer)
        {
            m_GIData.pSHCoefficientsBuffer = BufferManager::GetInstance().CreateBuffer(
                BufferCreationDesc
                {
                    .name = L"SH Coefficients Buffer",
                    .stride = sizeof(Vector4),
                    .size = sizeof(Vector4) * 9,
                    .viewFlags = GPUResourceFlags::SRV | GPUResourceFlags::UAV,
                    .accessFlags = BufferAccessFlags::GPUOnly,
                    .isRawAccess = false
                });
            m_bIsBakeSHCoefficients = false;
        }
        RenderPassResourceManager::GetInstance().Create<EnvironmentData>(&m_GIData);

        m_shaderPasses.assign(std::begin(m_PassData), std::end(m_PassData));
        if (!m_pMaterial)
        {
            m_pMaterial = std::make_unique<Material>(m_pDevice, m_shaderPasses);
        }
        UpdatePipeline();
    }

    void BakePass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        for (UINT i = 0; i < BAKE_PASS_COUNT; ++i)
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

    void BakePass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        DoSHCoefficients();
        DoPreIntegrateSSSLUT();
        DoIntegrateSSSNDFLUT();
    }

    void BakePass::DoPreIntegrateSSSLUT()
    {
        if (!BakeManager::GetInstance().ConsumeMasks(EBakeTaskFlags::SSSLut))
            return;

        auto passID = CS_PRE_INTEGRATE_SSS;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto targetRT = m_subsurfaceScatterData.pPreIntegrateSSSLUT;
        m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_PreIntegrateSSSLUTIndex,
                                 targetRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()),
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(targetRT, false);
        }
        m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pCommand->GetCommandList()->Close();
        ID3D12CommandList* ppCommandLists[] = {m_pCommand->GetCommandList()};
        m_pDevice->GetDirectQueue()->ExecuteCommandLists(1, ppCommandLists);
        m_pDevice->WaitForIdle();

        SaveTexToLocal(L"Pre_Integrate_SSS_LUT.png",
                       DXGI_FORMAT_R16G16B16A16_UNORM,
                       m_pDevice->GetDirectQueue(),
                       targetRT->GetResource());

        m_pCommand->Reset();
    }
    void BakePass::DoIntegrateSSSNDFLUT()
    {
        if (!BakeManager::GetInstance().ConsumeMasks(EBakeTaskFlags::SSSNDFLut))
            return;

        auto passID = CS_INTEGRATE_SSS_NDF;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto targetRT = m_subsurfaceScatterData.pNDFLUT;
        m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_IntegrateSSSNDFLUTIndex,
                                 targetRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_TargetSize,
                                   GetScreenSize(targetRT->GetWidth(), targetRT->GetHeight()),
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(targetRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(targetRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
            m_pCommand->AddUAVBarrier(targetRT, false);
        }
        m_pCommand->AddBarrier(targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pCommand->GetCommandList()->Close();
        ID3D12CommandList* ppCommandLists[] = {m_pCommand->GetCommandList()};
        m_pDevice->GetDirectQueue()->ExecuteCommandLists(1, ppCommandLists);
        m_pDevice->WaitForIdle();

        SaveTexToLocal(L"Integrate_SSS_NDF_LUT.png",
                       DXGI_FORMAT_R16_FLOAT,
                       m_pDevice->GetDirectQueue(),
                       targetRT->GetResource());

        m_pCommand->Reset();
    }
    void BakePass::DoSHCoefficients()
    {
        if (m_bIsBakeSHCoefficients)
        {
            return;
        }
        auto& skyboxTex = RenderPassResourceManager::GetInstance().Get<ShaderGlobalData>().skyboxTex;
        if (!TextureManager::GetInstance().GetTexture(skyboxTex)->GetIsReady())
            return;
        DoCalcTempSHCoefficients();
        DoCalcSHCoefficients();
        m_bIsBakeSHCoefficients = true;
    }
    void BakePass::DoCalcTempSHCoefficients()
    {
        auto passID = CS_TEMP_SH_Coefficients;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto targetRT = m_pSHCoefficientsTempBuffer;
        auto& skyboxTex = RenderPassResourceManager::GetInstance().Get<ShaderGlobalData>().skyboxTex;
        m_pCommand->AddBarrier(*targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_SHCoefficientsTempBufferIndex,
                                 targetRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SkyboxSize,
                                   GetScreenSize(skyboxTex.GetWidth(), skyboxTex.GetHeight()),
                                   passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SHCoefficientsTempCount,
                                   m_SHCoefficientsTempCount,
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(skyboxTex.GetWidth(), threadGroupSize.x),
                                 CeilDivide(skyboxTex.GetHeight(), threadGroupSize.y),
                                 6);
            m_pCommand->AddUAVBarrier(targetRT, false);
        }
        m_pCommand->AddBarrier(*targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Bake/") + passName).c_str());
    }
    void BakePass::DoCalcSHCoefficients()
    {
        auto passID = CS_SH_Coefficients;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = passData.pPipelineStateObject;
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        auto targetRT = m_GIData.pSHCoefficientsBuffer;
        m_pCommand->AddBarrier(*targetRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        {
            m_pMaterial->SetUINT(ShaderIDs::g_SHCoefficientsBufferIndex,
                                 targetRT->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetUINT(ShaderIDs::g_SHCoefficientsTempBufferIndex,
                                 m_pSHCoefficientsTempBuffer->GetUAVResourceHeapIndex(),
                                 passID);
            m_pMaterial->SetFloat4(ShaderIDs::g_SHCoefficientsTempCount,
                                   m_SHCoefficientsTempCount,
                                   passID);
            SetSpaceResource(passData, PER_PASS_SPACE);

            m_pCommand->Dispatch(1, 1, 1);
            m_pCommand->AddUAVBarrier(targetRT, false);
        }
        m_pCommand->AddBarrier(*targetRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Bake/") + passName).c_str());
    }


}