#include "stdafx.h"
#include "BasePass.h"

#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/DX12GraphicsContext.h"
#include "Runtime/Core/UploadRingBuffer.h"

#include "Runtime/RenderCore/RenderResource.h"
#include "Runtime/RenderCore/Material.h"
#include "Runtime/RenderCore/BufferManager.h"
#include "Runtime/RenderCore/Pass/RenderPassData.h"

namespace ElysiaRenderer
{
    BasePass::BasePass()
    {

    }

    BasePass::BasePass(DX12Camera* pCamera) :
        m_renderSize(Vector2::Zero),
        m_pCamera(pCamera)
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
        if (!pUploadBuffer->AllocateForFrame(m_pDevice->GetFrameID(), totalSize, GPUAddress, CPUAddress))
        {
            assert(false && "UploadRingBuffer is full! Call Reset() at beginning of frame.");
            return 0;
        }
        memset(CPUAddress, 0, totalSize);

        for (const auto& memberPair : CBuffer.members)
        {
            auto& member = memberPair.second;
            const MaterialParameterBlock::MaterialParam* pMaterialParam = pMaterial->GetParameterBlock().FindParam(
                member.Name, passID);
            if (!pMaterialParam)
                continue;

            uint8_t* dest = CPUAddress + member.StartOffset;
            if (IsArrayType(pMaterialParam->type))
            {
                if (!pMaterialParam->value.arrayData.empty())
                {
                    size_t copySize = std::min(pMaterialParam->value.arrayData.size() * sizeof(float),
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

                auto GPUAddress = UploadMaterialConstants(BufferManager::GetInstance().GetUploadRingBuffer(), spaceID,
                                                          m_pMaterial.get(), passData.pCurrVariantData,
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
                pResourceLayout->m_spaces[PER_FRAME_SPACE] = RenderResource::GetInstance().GetPerFrameBindResourceSpace(
                    m_pDevice->GetFrameID());
                m_pCommand->SetPipelineResource(PER_FRAME_SPACE,
                                                passData.pCurrVariantData->pMeshResourceLayout->m_spaces[
                                                    PER_FRAME_SPACE]);
            }
        }

    }
}