#include "DX12GraphicsContext.h"
#include "DX12RootSignature.h"


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

	void DX12GraphicsContext::SetPipeline(PipelineStateData& pipelineStateData)
	{
		auto pipelineState = pipelineStateData.m_pipelineState;
		auto& renderTargets = pipelineStateData.m_renderTargets;

		if (pipelineState->GetPipelineType() != PipleineType::Graphics)
		{
			ElysiaHelper::AssertError("Pipeline not graphics");
			return;
		}

		m_graphicsPipelineState = dynamic_cast<DX12GraphicsPipelineState*>(pipelineState);

		m_commandList->SetPipelineState(pipelineState->GetPipelineState());
		m_commandList->SetGraphicsRootSignature(pipelineState->GetRootSignature()->GetSignature());



		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandles[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle{ 0 };
		auto numTarget = renderTargets.size();
		for (size_t i = 0; i < numTarget; ++i)
		{
			renderTargetHandles[i] = renderTargets[i]->GetRTVDescriptor().GetCPUHandle();
		}
		if (m_graphicsPipelineState->GetDepthStencilRT() != nullptr)
		{
			depthStencilHandle = m_graphicsPipelineState->GetDepthStencilRT()->GetRTVDescriptor().GetCPUHandle();
		}
		SetRenderTargets(static_cast<UINT>(numTarget), renderTargetHandles, depthStencilHandle);
	}
	void DX12GraphicsContext::SetRenderTargets(UINT numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle[],
		const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
	{
		m_commandList->OMSetRenderTargets(numRenderTargets, renderTargetHandle, FALSE, 
			depthStencilHandle.ptr != 0 ? &depthStencilHandle : NULL);
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
	void DX12GraphicsContext::Draw(UINT vertexCount, UINT vertexStartOffset)
	{
		DrawInstanced(vertexCount, 1, vertexStartOffset, 0);
	}
	void DX12GraphicsContext::DrawInstanced(UINT vertexCount, UINT instanceCount, UINT vertexStartOffset, UINT startInstanceLocation)
	{
		m_commandList->DrawInstanced(vertexCount, instanceCount, vertexStartOffset, startInstanceLocation);
	}
}