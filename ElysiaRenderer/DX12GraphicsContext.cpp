#include "DX12GraphicsContext.h"
#include "DX12RootSignature.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
	extern class DX12Device;

	DX12GraphicsContext::DX12GraphicsContext(DX12Device* device) : 
		DX12Context(device, D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		 
	}

	DX12GraphicsContext::~DX12GraphicsContext()
	{
	}

	void DX12GraphicsContext::ClearRenderTarget(const DX12TextureResource& renderTarget, Color color)
	{
		m_commandList->ClearRenderTargetView(renderTarget.GetRTVDescriptor().GetCPUHandle(),
			color, 0, nullptr);
	}
	void DX12GraphicsContext::ClearDepthStencilTarget(const DX12TextureResource& renderTarget, float depth, uint8_t stencil)
	{
		m_commandList->ClearDepthStencilView(renderTarget.GetDSVDescriptor().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			depth, stencil, 0, nullptr);
	}

	void DX12GraphicsContext::SetPipeline(PipelineStateData& pipelineStateData, PipelineBindResource& pipelineBindResource)
	{
		auto pipelineState = pipelineStateData.m_pipelineState;
		auto& renderTargets = pipelineStateData.m_renderTargets;
		auto& SRVResources = pipelineBindResource.m_SRVResources;

		if (pipelineState->GetPipelineType() != PipleineType::Graphics)
		{
			ElysiaHelper::AssertError("Pipeline not graphics");
			return;
		}

		m_graphicsPipelineState = dynamic_cast<DX12GraphicsPipelineState*>(pipelineState);

		m_commandList->SetPipelineState(pipelineState->GetPipelineState());
		m_commandList->SetGraphicsRootSignature(pipelineState->GetRootSignature()->GetSignature());

		// set root parameters
		// each parameter has one descriptor table
		{
			static const uint32_t maxNumHandlesBinding = 16;
			static const uint32_t singleDescriptorRangeCopyArray[maxNumHandlesBinding]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1 };

			D3D12_CPU_DESCRIPTOR_HANDLE handles[maxNumHandlesBinding]{};
			UINT currentHandleIndex = 0;

			auto rootParameters = pipelineState->GetRootSignature()->GetDX12RootParameters();
			auto currRootParameter = rootParameters;
			UINT currRootParameterIndex = 0;
			for (; currRootParameter < rootParameters + pipelineState->GetRootSignature()->GetNumRootParams(); ++currRootParameter)
			{
				switch (currRootParameter->GetType())
				{
					case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					{
						UINT spaceID = currRootParameter->GetSpaceID();
						switch (spaceID)
						{
							case ElysiaHelper::PER_OBJECT_SPACE:
							{
								
								break;
							}

							case ElysiaHelper::PER_PASS_SPACE:
							{
								for (auto& SRVResource : SRVResources[spaceID])
								{
									switch (SRVResource->GetBufferType())
									{
									case BufferType::Texture:
									{
										handles[currentHandleIndex++] = static_cast<DX12TextureResource*>(SRVResource.get())->GetSRVDescriptor().GetCPUHandle();
										break;
									}

									default:
										ElysiaHelper::ThrowRuntimeError("SRVResource type is none");
										break;
									}
								}

								UINT numTableHandles = SRVResources[spaceID].size();
								auto blockStart = m_currSRVHeap->AllocateRenderPassDescriptorBlock(numTableHandles);
								m_device->CopyDescriptors(1, &blockStart.GetCPUHandle(), &numTableHandles, numTableHandles, handles, singleDescriptorRangeCopyArray, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

								m_commandList->SetGraphicsRootDescriptorTable(currRootParameterIndex, blockStart.GetGPUHandle());

								break;
							}
						}
						

						break;
					}

					case D3D12_ROOT_PARAMETER_TYPE_CBV:
					{
						auto spaceID = currRootParameter->GetSpaceID();
						auto CBVSize = pipelineBindResource.CBVSizes[spaceID];
						auto CBVIndex = pipelineBindResource.CBVIndexs[spaceID];
						if (pipelineBindResource.m_CBVResource[spaceID][CBVIndex])
						{
							m_commandList->SetGraphicsRootConstantBufferView(currentHandleIndex++, pipelineBindResource.m_CBVResource[spaceID][CBVIndex]->GetGPUAddress());
						}
						break;

						
						break;
					}
				}

				currRootParameterIndex++;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle{ 0 };
		auto numTarget = renderTargets.size();
		for (size_t i = 0; i < numTarget; ++i)
		{
			renderTargetHandles[i] = renderTargets[i]->GetRTVDescriptor().GetCPUHandle();
		}
		if (pipelineStateData.m_depthStencilTarget != nullptr)
		{
			depthStencilHandle = pipelineStateData.m_depthStencilTarget->GetDSVDescriptor().GetCPUHandle();
		}
		SetRenderTargets(static_cast<UINT>(numTarget), renderTargetHandles, depthStencilHandle);
	}
	void DX12GraphicsContext::SetRenderTargets(UINT numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle[],
		const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
	{
		m_commandList->OMSetRenderTargets(numRenderTargets, renderTargetHandle, FALSE, 
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
	void DX12GraphicsContext::Draw(UINT vertexCount, UINT vertexStartOffset, UINT startIndexLocation)
	{
		DrawInstanced(vertexCount, 1, startIndexLocation, vertexStartOffset, 0);
	}
	void DX12GraphicsContext::DrawInstanced(UINT vertexCount, UINT instanceCount, UINT startIndexLocation, UINT vertexStartOffset, UINT startInstanceLocation)
	{
		m_commandList->DrawIndexedInstanced(vertexCount, instanceCount, startIndexLocation, vertexStartOffset, startInstanceLocation);
	}
}