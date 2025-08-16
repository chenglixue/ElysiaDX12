#pragma once
#include "DX12Context.h"
#include "stdafx.h"
#include "DX12TextureResource.h"
#include "DX12PipelineState.h"

namespace ElysiaRenderer
{
	extern class DX12Device;
	using namespace DirectX::SimpleMath;

	class DX12GraphicsContext : public DX12Context
	{
	public:
		DX12GraphicsContext(DX12Device* device);
		~DX12GraphicsContext() override;

		void ClearRenderTarget(const DX12TextureResource& renderTarget, Color color);
		void ClearDepthStencilTarget(const DX12TextureResource& renderTarget, float depth, uint8_t stencil);

		void SetPipeline(PipelineStateData& pipelineStateData, PipelineBindResource& pipelineBindResource);
		void SetRenderTargets(UINT numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle[],
			const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
		void SetDefaultViewportAndScissor(ElysiaHelper::UINT2 screenSize);
		void SetViewport(D3D12_VIEWPORT& viewPort);
		void SetScissorRect(D3D12_RECT& rect);
		void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
		void SetVertexBuffer(UINT startIndex, UINT numVertexBuffer, D3D12_VERTEX_BUFFER_VIEW& vertexBufferView);
		void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& indexBufferView);

		void Draw(UINT vertexCount, UINT vertexStartOffset, UINT startIndexLocation);
		void DrawInstanced(UINT vertexCount, UINT instanceCount, UINT startIndexLocation, UINT vertexStartOffset, UINT startInstanceLocation);

	private:
		DX12GraphicsPipelineState* m_graphicsPipelineState = nullptr;
	};
}