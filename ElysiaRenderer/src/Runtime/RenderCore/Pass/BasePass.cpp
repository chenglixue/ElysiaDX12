#include "stdafx.h"
#include "BasePass.h"

#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/DX12Shader.h"
#include "Runtime/Core/UploadRingBuffer.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/PSOManager.h"
#include "Runtime/RenderCore/RenderTargetManager.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "Runtime/RenderCore/ShaderVariantManager.h"
#include "Runtime/RenderCore/Pass/RenderPassData.h"

namespace ElysiaRenderer
{
    BasePass::BasePass()
        : m_renderSize(Vector2::Zero)
    {
    }

    BasePass::~BasePass()
    {
        Dispose();
    }

    void BasePass::Setup(const RenderPassData& renderPassData)
    {
        m_renderSize = renderPassData.RenderSize;
        m_pCommand = renderPassData.pCommand;
        m_pDevice = renderPassData.pDevice;
        m_pCameraColorRT = renderPassData.pCameraColorRT;
        m_pCameraDepthRT = renderPassData.pCameraDepthRT;
        m_pSwaiChain = renderPassData.pSwapChain;
        m_pDisplayRT = renderPassData.pDisplayRT;

        Configure();
    }

    void BasePass::Dispose()
    {
    }

    D3D12_GPU_VIRTUAL_ADDRESS BasePass::UploadMaterialConstants(
        UploadRingBuffer* pUploadBuffer,
        UINT8 spaceID,
        Material* pMaterial,
        const ShaderVariantData* pVariantData,
        size_t passID)
    {
        assert(pUploadBuffer);
        assert(pMaterial);
        assert(spaceID < NUM_RESOURCE_SPACES);
        const auto& CBuffer = pVariantData->MergedReflectionData.GetCBuffer(spaceID);

        size_t totalSize = CBuffer.size;
        if (totalSize == 0)
        {
            return 0;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress;
        UINT8* CPUAddress = nullptr;
        if (!pUploadBuffer->AllocateForFrame(m_pDevice->GetFrameID(),
                                             totalSize,
                                             GPUAddress,
                                             CPUAddress))
        {
            assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
            return 0;
        }
        memset(CPUAddress, 0, totalSize);

        for (const auto& memberPair : CBuffer.members)
        {
            auto& member = memberPair.second;
            const MaterialParameterBlock::MaterialParam* pMaterialParam = pMaterial->
                                                                          GetParameterBlock().
                                                                          FindParam(
                                                                              member.Name,
                                                                              passID);
            if (!pMaterialParam)
                continue;

            uint8_t* dest = CPUAddress + member.StartOffset;
            if (IsArrayType(pMaterialParam->type))
            {
                if (!pMaterialParam->value.arrayData.empty())
                {
                    size_t copySize = std::min(
                        pMaterialParam->value.arrayData.size() * sizeof(float),
                        size_t(member.Size));
                    memcpy(dest, pMaterialParam->value.arrayData.data(), copySize);
                }
            }
            else
            {
                size_t copySize = std::min((size_t)member.Size, (size_t)64);
                memcpy(dest, pMaterialParam->value.data.data(), copySize);
            }
        }

        return GPUAddress;
    }

    void BasePass::SetSpaceResource(PassData& passData, UINT8 spaceID)
    {
        if (spaceID != PER_FRAME_SPACE)
        {
            auto pCurrVariantData = passData.pCurrVariantData;
            assert(pCurrVariantData);
            auto pResourceLayout = pCurrVariantData->pMeshResourceLayout.get();
            assert(pResourceLayout);

            if (pResourceLayout->IsValidSpace(spaceID))
            {
                if (pResourceLayout->m_spaces[spaceID] != nullptr)
                {
                    delete pResourceLayout->m_spaces[spaceID];
                    pResourceLayout->m_spaces[spaceID] = nullptr;
                }

                auto GPUAddress = UploadMaterialConstants(
                    BufferManager::GetInstance().GetUploadRingBuffer(),
                    spaceID,
                    m_pMaterial.get(),
                    passData.pCurrVariantData,
                    passData.PassIndex);
                auto newSpace = std::make_unique<PipelineResourceSpace>();
                newSpace->SetDynamicCBV(GPUAddress);
                newSpace->Lock();
                pResourceLayout->SetSpace(spaceID, newSpace.release());

                m_pCommand->SetPipelineResource(spaceID, pResourceLayout->m_spaces[spaceID]);
            }
        }
        else
        {
            auto pCurrVariantData = passData.pCurrVariantData;
            assert(pCurrVariantData);
            auto pResourceLayout = pCurrVariantData->pMeshResourceLayout.get();
            assert(pResourceLayout);

            if (pResourceLayout->IsValidSpace(spaceID))
            {
                pResourceLayout->m_spaces[PER_FRAME_SPACE] = RenderResource::GetInstance().
                    GetPerFrameBindResourceSpace(
                        m_pDevice->GetFrameID());
                m_pCommand->SetPipelineResource(PER_FRAME_SPACE,
                                                passData.pCurrVariantData->pMeshResourceLayout->
                                                         m_spaces[
                                                    PER_FRAME_SPACE]);
            }
        }

    }

    void BasePass::WarmUPCompute()
    {
        m_pCommand->AddBarrier(m_pWarmUPRT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        {
            std::vector<std::wstring> enableKeywords{};

            auto passID = m_warmUpComputePasseID;
            auto& passData = m_pWarmUPMaterial->GetPassData(passID);
            auto VariantManager = passData.pShader->GetVariantManager();
            passData.pCurrVariantData = &VariantManager->GetOrCompileVariantByNames(enableKeywords);

            passData.pPipelineStateObject = PSOManager::GetInstance().GetComputePipelineState(
                m_pDevice,
                m_pWarmUPMaterial.get(),
                passID);

            PipelineInfo pipelineStateData{};
            pipelineStateData.m_pipelineStateObject = m_pWarmUPMaterial->GetPassData(
                                                                           passID)
                                                                       .pPipelineStateObject;
            m_pCommand->SetPipeline(pipelineStateData);

            struct alignas(16)
            {
                UINT g_TargetTexIndex;
            } constantData;
            constexpr UINT constantSize = sizeof(constantData) / 4;

            constantData.g_TargetTexIndex = m_pWarmUPRT->GetResourceHeapIndex();
            m_pCommand->SetPushConstants(PER_MATERIAL_SPACE, &constantData, constantSize);

            auto threadGroupSize = passData.GetKernelThreadGroupSizes();
            m_pCommand->Dispatch(CeilDivide(m_pWarmUPRT->GetWidth(), threadGroupSize.x),
                                 CeilDivide(m_pWarmUPRT->GetHeight(), threadGroupSize.y),
                                 threadGroupSize.z);
        }

        m_pCommand->AddBarrier(m_pWarmUPRT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
}