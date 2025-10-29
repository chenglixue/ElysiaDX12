#include "DX12GraphicsContext.h"
#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "RenderTexture.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	DX12GraphicsContext::DX12GraphicsContext(DX12Device* device) : 
		DX12Context(device, D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		 
	}

	DX12GraphicsContext::~DX12GraphicsContext()
	{
	}

	void DX12GraphicsContext::ClearRenderTarget(const RenderTexture& renderTarget, Color color)
	{
		m_commandList->ClearRenderTargetView(renderTarget.GetTexture()->GetRTVDescriptor().GetCPUHandle(),
			color, 0, nullptr);
	}

	void DX12GraphicsContext::ClearRenderTarget(const DX12TextureResource& renderTarget, Color color)
	{
		m_commandList->ClearRenderTargetView(renderTarget.GetRTVDescriptor().GetCPUHandle(),
			color, 0, nullptr);
	}
	void DX12GraphicsContext::ClearDepthStencilTarget(const RenderTexture& renderTarget, float depth, uint8_t stencil)
	{
		m_commandList->ClearDepthStencilView(renderTarget.GetTexture()->GetDSVDescriptor().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			depth, stencil, 0, nullptr);
	}

	void DX12GraphicsContext::SetPipeline(PipelineInfo& pipelineBind)
	{
		m_graphicsPipelineStateObject = pipelineBind.m_pipelineStateObject;
		auto pipelineState = m_graphicsPipelineStateObject->m_pipelineState.get();
		auto& renderTargets = pipelineBind.m_renderTargets;
		const bool pipelineExpectedBoundExternally = !m_graphicsPipelineStateObject; //imgui

		if (!pipelineExpectedBoundExternally)
		{
			if (m_graphicsPipelineStateObject->m_pipelineType == PipelineType::Compute)
			{

			}
			else
			{
				m_commandList->SetPipelineState(pipelineState->GetPipelineState());
				m_commandList->SetGraphicsRootSignature(pipelineState->GetRootSignature()->GetSignature());
			}
		}

		if (pipelineState->GetPipelineType() != PipelineType::Graphics)
		{
			ElysiaHelper::AssertError("Pipeline not graphics");
			return;
		}

		if (pipelineExpectedBoundExternally || pipelineState->GetPipelineType() == PipelineType::Graphics)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
			D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle{ 0 };
			auto numTarget = renderTargets.size();

			for (size_t i = 0; i < numTarget; ++i)
			{
				renderTargetHandles[i] = renderTargets[i]->GetRTVDescriptor().GetCPUHandle();
			}
			if (pipelineBind.m_depthStencilTarget != nullptr)
			{
				depthStencilHandle = pipelineBind.m_depthStencilTarget->GetDSVDescriptor().GetCPUHandle();
			}
			SetRenderTargets(static_cast<UINT>(numTarget), renderTargetHandles, depthStencilHandle);
		}
		
	}
	void DX12GraphicsContext::SetPipelineResource(uint8_t spaceID, PipelineResourceSpace* pipelineBindResourceSpace)
	{
		assert(m_graphicsPipelineStateObject);
		assert(pipelineBindResourceSpace->IsLocked());

		// set root parameters
		// each parameter has one descriptor table
		{
			auto SRVResources = pipelineBindResourceSpace->GetSRVs();
			auto CBVResource = pipelineBindResourceSpace->GetCBV();

			static const uint32_t maxNumHandlesBinding = 16;
			const UINT numTableHandles = static_cast<UINT>(SRVResources.size());
			assert(numTableHandles <= maxNumHandlesBinding);

			static const uint32_t singleDescriptorRangeCopyArray[maxNumHandlesBinding]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1 };
			D3D12_CPU_DESCRIPTOR_HANDLE handles[maxNumHandlesBinding]{};
			UINT currentHandleIndex = 0;

			if (CBVResource)
			{
				auto& rootParameterIndex = m_graphicsPipelineStateObject->m_pipelineResourceMapping.m_CBVMappings[spaceID];
				assert(rootParameterIndex.has_value());

				switch (m_graphicsPipelineStateObject->m_pipelineType)
				{
					case PipelineType::Graphics:
					{
						m_commandList->SetGraphicsRootConstantBufferView(rootParameterIndex.value(), CBVResource->GetGPUAddress());
						break;
					}
				}
			}

			if (numTableHandles == 0)
			{
				return;
			}

			for (auto& SRV : SRVResources)
			{
				if (SRV->m_resource->GetBufferType() == GPUResourceType::Texture)
				{
					handles[currentHandleIndex++] = static_cast<DX12TextureResource*>(SRV->m_resource)->GetSRVDescriptor().GetCPUHandle();
				}
				else
				{
					//handles[currentHandleIndex++] = static_cast<DX12BufferResource*>(SRV->m_resource)->GetSRVDescriptor().GetCPUHandle();
				}
			}
			DX12DescriptorHeapHandle blockStart = m_currSRVHeap->AllocateRenderPassDescriptorBlock(numTableHandles);
			m_device->CopyDescriptors(1, &blockStart.GetCPUHandle(), &numTableHandles, numTableHandles, handles, singleDescriptorRangeCopyArray, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			auto& tableMapping = m_graphicsPipelineStateObject->m_pipelineResourceMapping.m_TableMappings[spaceID];
			assert(tableMapping.has_value());

			switch (m_graphicsPipelineStateObject->m_pipelineType)
			{
				case PipelineType::Graphics:
				{
					m_commandList->SetGraphicsRootDescriptorTable(tableMapping.value(), blockStart.GetGPUHandle());
					break;
				}
			}
		}
	}
	void DX12GraphicsContext::SetRenderTargets(UINT numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle[],
		const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
	{
		m_commandList->OMSetRenderTargets(numRenderTargets, renderTargetHandle, false,
			depthStencilHandle.ptr != 0 ? &depthStencilHandle : nullptr);
	}
	void DX12GraphicsContext::SetDefaultViewportAndScissor(ElysiaHelper::UINT2 screenSize)
	{
		D3D12_VIEWPORT viewport = {};
		viewport.Width			= static_cast<float>(screenSize.x);
		viewport.Height			= static_cast<float>(screenSize.y);
		viewport.TopLeftX		= 0;
		viewport.TopLeftY		= 0;
		viewport.MinDepth		= 0;
		viewport.MaxDepth		= 1;

		D3D12_RECT scissorRect = {};
		scissorRect.left = 0;
		scissorRect.right = screenSize.x;
		scissorRect.bottom = screenSize.y;
		scissorRect.top = 0;

		SetViewport(viewport);
		SetScissorRect(scissorRect);
	}
	void DX12GraphicsContext::SetViewport(D3D12_VIEWPORT& viewPort)
	{
		m_commandList->RSSetViewports(1, &viewPort);
	}
	void DX12GraphicsContext::SetScissorRect(D3D12_RECT& rect)
	{
		m_commandList->RSSetScissorRects(1, &rect);
	}
	void DX12GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
	{
		m_commandList->IASetPrimitiveTopology(topology);
	}
	void DX12GraphicsContext::SetVertexBuffer(UINT startIndex, UINT numVertexBuffer, D3D12_VERTEX_BUFFER_VIEW& vertexBufferView)
	{
		m_commandList->IASetVertexBuffers(startIndex, numVertexBuffer, &vertexBufferView);
	}
	void DX12GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& indexBufferView)
	{
		m_commandList->IASetIndexBuffer(&indexBufferView);
	}

	void DX12GraphicsContext::Draw(UINT vertexCount, UINT vertexStartOffset)
	{
		DrawInstanced(vertexCount, 1, vertexStartOffset, 0);
	}
	void DX12GraphicsContext::Draw(UINT vertexCount, UINT vertexStartOffset, UINT startIndexLocation)
	{
		DrawInstanced(vertexCount, 1, startIndexLocation, vertexStartOffset, 0);
	}
	void DX12GraphicsContext::DrawInstanced(UINT vertexCount, UINT instanceCount, UINT vertexStartOffset, UINT startInstanceLocation)
	{
		m_commandList->DrawInstanced(vertexCount, instanceCount, vertexStartOffset, startInstanceLocation);
	}
	void DX12GraphicsContext::DrawInstanced(UINT vertexCount, UINT instanceCount, UINT startIndexLocation, UINT vertexStartOffset, UINT startInstanceLocation)
	{
		m_commandList->DrawIndexedInstanced(vertexCount, instanceCount, startIndexLocation, vertexStartOffset, startInstanceLocation);
	}
}