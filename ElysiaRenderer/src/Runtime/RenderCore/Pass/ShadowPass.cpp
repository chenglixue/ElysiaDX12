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
    ShadowPass::ShadowPass()
        : BasePass()
    {
        m_meshDatas.reserve(Max_RenderItem_Count);
        m_indirectCommands.reserve(Max_RenderItem_Count);
        m_pIndirectDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
        {
            .name = L"Shadow Indirect Buffer",
            .stride = sizeof(IndirectCommand),
            .size = sizeof(IndirectCommand) * Max_RenderItem_Count,
            .viewFlags = GPUResourceFlags::SRV,
            .accessFlags = BufferAccessFlags::HostWritable,
            .isRawAccess = true,
            .InitData = nullptr,
            .isAccelerationStructure = false,
            .isIndirectBuffer = true
        });
    }
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
        LightManager::GetInstance().GetMainLight()->CreateMainShadow(20, DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

        m_sobolSqeuences = Create2DSobolSqeuence(64);
        UpdatePipeline();
    }
    void ShadowPass::Render(ElysiaEngine::FrameContext& context)
    {
        m_pCamera = context.pCamera;
        m_pGPUTimer = context.pGPUTimer;

        if (!UserData::GetInstance().shadowParameter.EnableShadow)
            return;

        if (context.renderList.empty())
            return;
        UploadMeshData(context.renderList);
        if (!m_pMeshDataBuffer)
            return;
        DrawShadowPass(context);
    }

    void ShadowPass::UpdatePipeline()
    {
        if (!m_pMaterial)
            return;

        UpdateShadowPassVariant(ShaderPassIDs::ShadowCastPassID);
    }
    void ShadowPass::UpdateShadowPassVariant(UINT passIndex)
    {
        std::vector<std::wstring> enableKeywords{};

        switch (UserData::GetInstance().shadowParameter.shadowQuality)
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
        switch (UserData::GetInstance().shadowParameter.shadowType)
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
            m_pDevice,
            m_pMaterial.get(),
            passIndex,
            RTDesc);

        if (!m_pCommandSignature)
        {
            // 对应 IndirectCommand::pushConstants
            D3D12_INDIRECT_ARGUMENT_DESC args[2] = {};
            args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
            args[0].Constant.RootParameterIndex = PER_MATERIAL_SPACE - 1;
            args[0].Constant.DestOffsetIn32BitValues = 0;
            args[0].Constant.Num32BitValuesToSet = 2; // 两个 UINT

            // 对应 IndirectCommand::drawArguments
            args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

            D3D12_COMMAND_SIGNATURE_DESC desc = {};
            desc.ByteStride = sizeof(IndirectCommand);
            desc.NumArgumentDescs = 2;
            desc.pArgumentDescs = args;

            m_pDevice->GetDevice()->CreateCommandSignature(&desc,
                                                           passData.pRootSignature->GetSignature().Get(),
                                                           IID_PPV_ARGS(&m_pCommandSignature));
        }
    }

    void ShadowPass::DrawMesh(ElysiaEngine::FrameContext& context, PassData& passData)
    {
        m_indirectCommands.clear();
        UINT renderItemIndex = 0;
        UINT meshDataBufferIndex = m_pMeshDataBuffer->GetResourceHeapIndex();
        for (auto& renderItem : context.renderList)
        {
            IndirectCommand indirectCommand{};
            indirectCommand.pushConstants =
            {
                .meshDataBufferIndex = meshDataBufferIndex,
                .meshDataIndex = renderItemIndex
            };
            indirectCommand.drawArguments = D3D12_DRAW_INDEXED_ARGUMENTS
            {
                .IndexCountPerInstance = renderItem.indexCount,
                .InstanceCount = 1,
                .StartIndexLocation = renderItem.startIndex,
                .BaseVertexLocation = int(renderItem.baseVertex),
                .StartInstanceLocation = 0,
            };
            m_indirectCommands.emplace_back(indirectCommand);
            renderItemIndex ++;
        }
        memcpy(m_pIndirectDataBuffer->GetMappedBuffer(),
               m_indirectCommands.data(),
               sizeof(IndirectCommand) * Max_RenderItem_Count);

        m_pCommand->GetCommandList()->ExecuteIndirect(m_pCommandSignature.Get(),
                                                      // 执行多少次命令
                                                      renderItemIndex,
                                                      m_pIndirectDataBuffer->GetResource().Get(),
                                                      // 从 Buffer 的开头开始
                                                      0,
                                                      // 如果没有 CountBuffer，则固定执行指定的次数
                                                      nullptr,
                                                      0);

    }
    void ShadowPass::DrawShadowPass(ElysiaEngine::FrameContext& context)
    {
        auto passID = ShaderPassIDs::ShadowCastPassID;
        auto& passData = m_pMaterial->GetPassData(passID);
        auto passName = passData.Name.c_str();
        PIXHelper pix(m_pCommand->GetCommandList(), passName);

        auto pShadowRT = LightManager::GetInstance().GetMainShadowRT();
        PipelineInfo pipelineStateData{};
        pipelineStateData.m_pipelineStateObject = m_pMaterial->GetPassData(
            passID).pPipelineStateObject;
        pipelineStateData.m_renderTargets = {};
        pipelineStateData.m_depthStencilTarget = pShadowRT->GetTexture();
        m_pCommand->SetPipeline(pipelineStateData);
        SetSpaceResource(passData, PER_FRAME_SPACE);

        m_pCommand->AddBarrier(pShadowRT, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_pCommand->ClearDepthStencilTarget(pShadowRT, 1.f, 0);
        m_pCommand->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pCommand->SetViewport(
            reinterpret_cast<DX12DirectionLight*>(m_pMainLight)->GetMainShadow()->
                                                                 GetViewport());
        m_pCommand->SetScissorRect(
            reinterpret_cast<DX12DirectionLight*>(m_pMainLight)->GetMainShadow()->
                                                                 GetScissorRect());
        if (context.renderList.size())
        {
            m_pCommand->SetIndexBuffer(BufferManager::GetInstance().GetGlobalIndexBufferView());
            m_pCommand->SetVertexBuffer(0,
                                        1,
                                        BufferManager::GetInstance().GetGlobalVertexBufferView());
        }

        m_pMaterial->SetFloat(ShaderIDs::shadowNearZ,
                              LightManager::GetInstance().GetMainShadow()->GetNearZ());
        m_pMaterial->SetFloat(ShaderIDs::shadowFarZ,
                              LightManager::GetInstance().GetMainShadow()->GetFarZ());
        m_pMaterial->SetFloat(ShaderIDs::shadowDepthBias,
                              UserData::GetInstance().shadowParameter.shadowDepthBias / 100);
        m_pMaterial->SetFloat(ShaderIDs::shadowSlopeDepthBias,
                              UserData::GetInstance().shadowParameter.shadowSlopeDepthBias / 100);
        m_pMaterial->SetFloat(ShaderIDs::shadowMaxSlopeDepthBias,
                              UserData::GetInstance().shadowParameter.shadowMaxSlopeDepthBias / 100);
        m_pMaterial->SetVector2Array(ShaderIDs::g_sobolSequence, m_sobolSqeuences);
        SetSpaceResource(passData, PER_PASS_SPACE);

        DrawMesh(context, passData);

        m_pCommand->AddBarrier(pShadowRT, D3D12_RESOURCE_STATE_DEPTH_READ);

        m_pGPUTimer->GetTimeStamp(m_pCommand->GetCommandList(), (std::string("Shadow/") + passName).c_str());
    }

    void ShadowPass::UploadMeshData(const std::vector<RenderItem>& renderItems)
    {
        const auto renderItemCount = renderItems.size();
        UINT bufferSize = Max_RenderItem_Count * sizeof(MeshData);

        m_meshDatas.clear();

        for (UINT64 i = 0; i < renderItemCount; i ++)
        {
            const auto& materialData = renderItems[i].loadedMaterial;
            const auto& textureIndices = renderItems[i].textureIndices;
            auto meshData = MeshData
            {
                .world_M = renderItems[i].worldMatrix,

                .opacity = materialData.opacity,
                .cutoff = 0.5,
                .baseColorTexIndex = textureIndices.Albedo,
                .vertexOffset = renderItems[i].baseVertex,

                .indexOffset = renderItems[i].startIndex,
                .pad = UINT3(0, 0, 0)
            };
            m_meshDatas.emplace_back(meshData);
        }

        if (!m_pMeshDataBuffer)
        {
            m_pMeshDataBuffer = BufferManager::GetInstance().CreateBuffer(BufferCreationDesc
            {
                .name = L"Shadow Mesh Data Buffer",
                .stride = sizeof(MeshData),
                .size = bufferSize,
                .viewFlags = GPUResourceFlags::SRV,
                .accessFlags = BufferAccessFlags::HostWritable,
                .isRawAccess = false,
                .InitData = m_meshDatas.data()
            });
        }
        else
        {
            memcpy(m_pMeshDataBuffer->GetMappedBuffer(), m_meshDatas.data(), bufferSize);
        }
    }
}