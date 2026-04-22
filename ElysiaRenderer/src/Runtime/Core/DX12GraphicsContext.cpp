#include "stdafx.h"
#include "DX12GraphicsContext.h"

#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "ContextUtility.h"
#include "DX12TextureBuffer.h"


namespace ElysiaCore
{
    using namespace ElysiaRenderer;

    DX12GraphicsContext::DX12GraphicsContext(DX12Device* device)
        : DX12Context(device, D3D12_COMMAND_LIST_TYPE_DIRECT)
    {

    }

    DX12GraphicsContext::~DX12GraphicsContext()
    {
    }

    void DX12GraphicsContext::SetPushConstants(uint8_t spaceID, const void* data, UINT numValues)
    {
        assert(m_graphicsPipelineStateObject);

        // ��ӳ�����ҵ��� Space ��Ӧ�� RootParameterIndex
        auto& pushMapping = m_graphicsPipelineStateObject->m_pipelineResourceMapping.
                                                           m_PushConstantMappings[spaceID];

        if (pushMapping.has_value())
        {
            UINT rootIndex = pushMapping.value();

            if (m_graphicsPipelineStateObject->m_pipelineType == PipelineType::Compute)
            {
                m_commandList->SetComputeRoot32BitConstants(rootIndex, numValues, data, 0);
            }
            else
            {
                m_commandList->SetGraphicsRoot32BitConstants(rootIndex, numValues, data, 0);
            }
        }
    }

    void DX12GraphicsContext::Discard(RenderTexture* pRT)
    {
        m_commandList->DiscardResource(pRT->GetResource(), nullptr);
    }

    void DX12GraphicsContext::ClearRenderTarget(RenderTexture* renderTarget, Color color)
    {
        auto oldState = renderTarget->GetTexture()->GetUsageState();
        if (oldState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            AddBarrier(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        m_commandList->ClearRenderTargetView(
            renderTarget->GetTexture()->GetRTVDescriptor().GetCPUHandle(),
            color,
            0,
            nullptr);

        if (oldState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            AddBarrier(renderTarget, oldState);
        }
    }

    void DX12GraphicsContext::ClearRenderTarget(const DX12TextureResource& renderTarget,
                                                Color color)
    {
        m_commandList->ClearRenderTargetView(renderTarget.GetRTVDescriptor().GetCPUHandle(),
                                             color,
                                             0,
                                             nullptr);
    }
    void DX12GraphicsContext::ClearDepthStencilTarget(const RenderTexture* renderTarget,
                                                      float depth,
                                                      uint8_t stencil)
    {
        m_commandList->ClearDepthStencilView(
            renderTarget->GetTexture()->GetDSVDescriptor().GetCPUHandle(),
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
            depth,
            stencil,
            0,
            nullptr);
    }

    void DX12GraphicsContext::SetPipeline(PipelineInfo& pipelineBind)
    {
        assert(pipelineBind.m_pipelineStateObject);
        m_graphicsPipelineStateObject = pipelineBind.m_pipelineStateObject;
        auto pipelineState = m_graphicsPipelineStateObject->m_pipelineState.get();
        auto& renderTargets = pipelineBind.m_renderTargets;
        const bool pipelineExpectedBoundExternally = !m_graphicsPipelineStateObject; //imgui

        assert(pipelineState);

        if (!pipelineExpectedBoundExternally)
        {
            m_commandList->SetPipelineState(pipelineState->GetPipelineState().Get());
            if (m_graphicsPipelineStateObject->m_pipelineType == PipelineType::Compute)
            {
                m_commandList->SetComputeRootSignature(
                    m_graphicsPipelineStateObject->m_rootSignature->GetSignature().Get());
            }
            else
            {
                m_commandList->SetGraphicsRootSignature(
                    m_graphicsPipelineStateObject->m_rootSignature->GetSignature().Get());
            }
        }

        if (pipelineExpectedBoundExternally || pipelineState->GetPipelineType() ==
            PipelineType::Graphics)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]
                {};
            D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle{0};
            auto numTarget = renderTargets.size();

            for (size_t i = 0; i < numTarget; ++i)
            {
                renderTargetHandles[i] = renderTargets[i]->GetRTVDescriptor().GetCPUHandle();
            }
            if (pipelineBind.m_depthStencilTarget != nullptr)
            {
                depthStencilHandle = pipelineBind.m_depthStencilTarget->GetDSVDescriptor().
                                                  GetCPUHandle();
            }
            SetRenderTargets(static_cast<UINT>(numTarget), renderTargetHandles, depthStencilHandle);
        }

    }
    void DX12GraphicsContext::SetPipelineResource(uint8_t spaceID,
                                                  PipelineResourceSpace* pipelineBindResourceSpace)
    {
        assert(m_graphicsPipelineStateObject);
        assert(pipelineBindResourceSpace->IsLocked());

        // set root parameters
        // each parameter has one descriptor table
        {
            auto UAVResources = pipelineBindResourceSpace->GetUAVs();
            auto SRVResources = pipelineBindResourceSpace->GetSRVs();

            static const uint32_t maxNumHandlesBinding = 16;
            const UINT numTableHandles = static_cast<UINT>(
                SRVResources.size() + UAVResources.size());
            assert(numTableHandles <= maxNumHandlesBinding);

            static const uint32_t singleDescriptorRangeCopyArray[maxNumHandlesBinding]{
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
            D3D12_CPU_DESCRIPTOR_HANDLE handles[maxNumHandlesBinding]{};
            UINT currentHandleIndex = 0;

            assert(numTableHandles <= maxNumHandlesBinding);

            if (pipelineBindResourceSpace->GetStaticCBV() || pipelineBindResourceSpace->
                HasDynamicCBV())
            {
                auto& rootParameterIndex = m_graphicsPipelineStateObject->m_pipelineResourceMapping.
                                                                          m_CBVMappings[
                    spaceID];
                if (rootParameterIndex.has_value())
                {
                    switch (m_graphicsPipelineStateObject->m_pipelineType)
                    {
                    case PipelineType::Graphics:
                    {
                        if (pipelineBindResourceSpace->HasDynamicCBV())
                        {
                            m_commandList->SetGraphicsRootConstantBufferView(
                                rootParameterIndex.value(),
                                pipelineBindResourceSpace->GetDynamicCBV());
                        }
                        else if (pipelineBindResourceSpace->GetStaticCBV())
                        {
                            m_commandList->SetGraphicsRootConstantBufferView(
                                rootParameterIndex.value(),
                                pipelineBindResourceSpace->GetStaticCBV()->GetGPUAddress());
                        }
                        break;
                    }
                    case PipelineType::Compute:
                    {
                        if (pipelineBindResourceSpace->HasDynamicCBV())
                        {
                            m_commandList->SetComputeRootConstantBufferView(
                                rootParameterIndex.value(),
                                pipelineBindResourceSpace->GetDynamicCBV());
                        }
                        else if (pipelineBindResourceSpace->GetStaticCBV())
                        {
                            m_commandList->SetComputeRootConstantBufferView(
                                rootParameterIndex.value(),
                                pipelineBindResourceSpace->GetStaticCBV()->GetGPUAddress());
                        }
                        break;
                    }
                    default:
                    {
                        assert(false);
                        break;
                    }
                    }
                }
            }

            if (numTableHandles == 0)
            {
                return;
            }

            for (auto& UAV : UAVResources)
            {
                if (UAV->m_resource->GetBufferType() == GPUResourceType::Texture)
                {
                    handles[currentHandleIndex ++] =
                        static_cast<DX12TextureResource*>(UAV->m_resource)->
                        GetUAVDescriptor().GetCPUHandle();
                }
                else
                {
                    handles[currentHandleIndex ++] =
                        static_cast<DX12BufferResource*>(UAV->m_resource)->
                        GetUAVDescriptor().GetCPUHandle();
                }
            }

            for (auto& SRV : SRVResources)
            {
                if (SRV->m_resource->GetBufferType() == GPUResourceType::Texture)
                {
                    handles[currentHandleIndex ++] =
                        static_cast<DX12TextureResource*>(SRV->m_resource)->
                        GetSRVDescriptor().GetCPUHandle();
                }
                else
                {
                    handles[currentHandleIndex ++] =
                        static_cast<DX12BufferResource*>(SRV->m_resource)->
                        GetSRVDescriptor().GetCPUHandle();
                }
            }
            DX12DescriptorHeapHandle blockStart = m_currSRVHeap->AllocateRenderPassDescriptorBlock(
                numTableHandles);
            m_device->CopyDescriptors(1,
                                      &blockStart.GetCPUHandle(),
                                      &numTableHandles,
                                      numTableHandles,
                                      handles,
                                      singleDescriptorRangeCopyArray,
                                      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            auto& tableMapping = m_graphicsPipelineStateObject->m_pipelineResourceMapping.
                                                                m_TableMappings[spaceID];
            assert(tableMapping.has_value());

            switch (m_graphicsPipelineStateObject->m_pipelineType)
            {
            case PipelineType::Graphics:
            {
                m_commandList->SetGraphicsRootDescriptorTable(
                    tableMapping.value(),
                    blockStart.GetGPUHandle());
                break;
            }
            case PipelineType::Compute:
            {
                m_commandList->SetComputeRootDescriptorTable(
                    tableMapping.value(),
                    blockStart.GetGPUHandle());
                break;
            }
            default:
            {
                assert(false);

                break;
            }
            }
        }
    }
    void DX12GraphicsContext::SetRenderTargets(UINT numRenderTargets,
                                               const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle[],
                                               const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
    {
        m_commandList->OMSetRenderTargets(numRenderTargets,
                                          renderTargetHandle,
                                          false,
                                          depthStencilHandle.ptr != 0
                                              ? &depthStencilHandle
                                              : nullptr);
    }
    void DX12GraphicsContext::SetDefaultViewportAndScissor(const ElysiaHelper::UINT2 screenSize)
    {
        D3D12_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(screenSize.x);
        viewport.Height = static_cast<float>(screenSize.y);
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;

        D3D12_RECT scissorRect = {};
        scissorRect.left = 0;
        scissorRect.right = screenSize.x;
        scissorRect.bottom = screenSize.y;
        scissorRect.top = 0;

        SetViewport(viewport);
        SetScissorRect(scissorRect);
    }
    void DX12GraphicsContext::SetViewport(const D3D12_VIEWPORT& viewPort)
    {
        m_commandList->RSSetViewports(1, &viewPort);
    }
    void DX12GraphicsContext::SetScissorRect(const D3D12_RECT& rect)
    {
        m_commandList->RSSetScissorRects(1, &rect);
    }
    void DX12GraphicsContext::SetPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY topology)
    {
        m_commandList->IASetPrimitiveTopology(topology);
    }
    void DX12GraphicsContext::SetVertexBuffer(UINT startIndex,
                                              UINT numVertexBuffer,
                                              D3D12_VERTEX_BUFFER_VIEW& vertexBufferView)
    {
        m_commandList->IASetVertexBuffers(startIndex, numVertexBuffer, &vertexBufferView);
    }
    void DX12GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& indexBufferView)
    {
        m_commandList->IASetIndexBuffer(&indexBufferView);
    }

    void DX12GraphicsContext::DrawFullScreenTriangle()
    {
        SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        m_commandList->IASetIndexBuffer(nullptr);
        Draw(3);
    }
    void DX12GraphicsContext::Draw(UINT vertexCount, UINT vertexStartOffset)
    {
        DrawInstanced(vertexCount, 1, vertexStartOffset, 0);
    }
    void DX12GraphicsContext::Draw(UINT vertexCount,
                                   UINT vertexStartOffset,
                                   UINT startIndexLocation)
    {
        DrawInstanced(vertexCount, 1, startIndexLocation, vertexStartOffset, 0);
    }
    void DX12GraphicsContext::DrawInstanced(UINT vertexCount,
                                            UINT instanceCount,
                                            UINT vertexStartOffset,
                                            UINT startInstanceLocation)
    {
        m_commandList->DrawInstanced(vertexCount,
                                     instanceCount,
                                     vertexStartOffset,
                                     startInstanceLocation);
    }
    void DX12GraphicsContext::DrawInstanced(UINT vertexCount,
                                            UINT instanceCount,
                                            UINT startIndexLocation,
                                            UINT vertexStartOffset,
                                            UINT startInstanceLocation)
    {
        m_commandList->DrawIndexedInstanced(vertexCount,
                                            instanceCount,
                                            startIndexLocation,
                                            vertexStartOffset,
                                            startInstanceLocation);
    }

    void DX12GraphicsContext::Dispatch(UINT groupCountX, UINT groupCountY, UINT groupCountZ)
    {
        m_commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void DX12GraphicsContext::Dispatch1D(UINT threadCountX, UINT groupSizeX)
    {
        Dispatch(GetGroupCount(threadCountX, groupSizeX), 1, 1);
    }

    void DX12GraphicsContext::Dispatch2D(UINT threadCountX,
                                         UINT threadCountY,
                                         UINT groupSizeX,
                                         UINT groupSizeY)
    {
        Dispatch(GetGroupCount(threadCountX, groupSizeX),
                 GetGroupCount(threadCountY, groupSizeY),
                 1);
    }

    void DX12GraphicsContext::Dispatch3D(UINT threadCountX,
                                         UINT threadCountY,
                                         UINT threadCountZ,
                                         UINT groupSizeX,
                                         UINT groupSizeY,
                                         UINT groupSizeZ)
    {
        Dispatch(GetGroupCount(threadCountX, groupSizeX),
                 GetGroupCount(threadCountY, groupSizeY),
                 GetGroupCount(threadCountZ, groupSizeZ));
    }

    void DX12GraphicsContext::CopyTexture(RenderTexture* sourceRT, RenderTexture* destRT)
    {
        auto sourceOldState = sourceRT->GetTexture()->GetUsageState();
        auto destOldState = destRT->GetTexture()->GetUsageState();

        if (sourceOldState != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            AddBarrier(*sourceRT->GetTexture(), D3D12_RESOURCE_STATE_COPY_SOURCE, false);
        }
        if (destOldState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            AddBarrier(*destRT->GetTexture(), D3D12_RESOURCE_STATE_COPY_DEST, false);
        }
        FlushBarrier();

        GetCommandList()->CopyResource(destRT->GetResource(), sourceRT->GetResource());

        // if (sourceRT->GetTexture()->GetUsageState() != sourceOldState)
        // {
        //     AddBarrier(*sourceRT->GetTexture(), sourceOldState, false);
        // }
        // if (destRT->GetTexture()->GetUsageState() != destOldState)
        // {
        //     AddBarrier(*destRT->GetTexture(), destOldState, false);
        // }
        // FlushBarrier();
    }

    void DX12GraphicsContext::CopyTextureRegion(RenderTexture* sourceRT,
                                                RenderTexture* destRT,
                                                UINT64 mipmapLevel)
    {
        auto sourceOldState = sourceRT->GetTexture()->GetUsageState();
        auto destOldState = destRT->GetTexture()->GetUsageState();
        if (sourceOldState != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            AddBarrier(*sourceRT->GetTexture(), D3D12_RESOURCE_STATE_COPY_SOURCE, false);
        }
        if (destOldState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            AddBarrier(*destRT->GetTexture(), D3D12_RESOURCE_STATE_COPY_DEST, false);
        }
        FlushBarrier();

        D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
        srcLocation.pResource = sourceRT->GetResource();
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLocation.SubresourceIndex = sourceRT->GetSubresourceIndex(mipmapLevel);

        D3D12_TEXTURE_COPY_LOCATION destLocation = {};
        destLocation.pResource = destRT->GetResource();
        destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destLocation.SubresourceIndex = destRT->GetSubresourceIndex(mipmapLevel);

        m_commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &srcLocation, nullptr);
    }
}