#pragma once
#include "DX12Context.h"
#include "DX12PipelineState.h"

namespace ElysiaCore
{
    class DX12Device;
    class PipelineResourceSpace;
}

namespace ElysiaRenderer
{
    class RenderTexture;
}

namespace ElysiaCore
{
    class DX12GraphicsContext : public DX12Context
    {
    public:
        DX12GraphicsContext(DX12Device* device);
        ~DX12GraphicsContext() override;

        void ClearRenderTarget(RenderTexture* renderTarget, Color color);
        void ClearRenderTarget(const DX12TextureResource& renderTarget, Color color);
        void ClearDepthStencilTarget(const RenderTexture* renderTarget, float depth, uint8_t stencil);

        void SetPipeline(PipelineInfo& pipelineStateData);
        void SetPipelineResource(uint8_t spaceID, PipelineResourceSpace* pipelineBindResource);
        void SetRenderTargets(UINT numRenderTargets, const D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle[],
                              const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
        void SetDefaultViewportAndScissor(const ElysiaHelper::UINT2 screenSize);
        void SetViewport(const D3D12_VIEWPORT& viewPort);
        void SetScissorRect(const D3D12_RECT& rect);
        void SetPrimitiveTopology(const D3D12_PRIMITIVE_TOPOLOGY topology);
        void SetVertexBuffer(UINT startIndex, UINT numVertexBuffer, D3D12_VERTEX_BUFFER_VIEW& vertexBufferView);
        void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& indexBufferView);

        void DrawFullScreenTriangle();
        void Draw(UINT vertexCount, UINT vertexStartOffset = 0);
        void Draw(UINT vertexCount, UINT vertexStartOffset, UINT startIndexLocation);
        void DrawInstanced(UINT vertexCount, UINT instanceCount, UINT vertexStartOffset, UINT startInstanceLocation);
        void DrawInstanced(UINT vertexCount, UINT instanceCount, UINT startIndexLocation, UINT vertexStartOffset,
                           UINT startInstanceLocation);

        void Dispatch(UINT groupCountX, UINT groupCountY, UINT groupCountZ);
        void Dispatch1D(UINT threadCountX, UINT groupSizeX);
        void Dispatch2D(UINT threadCountX, UINT threadCountY, UINT groupSizeX, UINT groupSizeY);
        void Dispatch3D(UINT threadCountX, UINT threadCountY, UINT threadCountZ, UINT groupSizeX, UINT groupSizeY,
                        UINT groupSizeZ);

        void CopyTexture(RenderTexture* sourceRT, RenderTexture* destRT);
        void CopyTextureRegion(RenderTexture* sourceRT, RenderTexture* destRT, UINT64 mipmapLevel = 0);

    private:
        PipelineStateObject* m_graphicsPipelineStateObject = nullptr;
    };
}