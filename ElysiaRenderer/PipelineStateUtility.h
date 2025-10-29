#pragma once
#include "Helper.h"
#include "PipelineResourceSpace.h"

namespace ElysiaRenderer
{
	class DX12Shader;
	class DX12PipelineState;
	class DX12RootSignature;
	class DX12TextureResource;

	enum class PipelineType : uint8_t
	{
		None = 0,
		Graphics = 1,
		Compute = 2
	};

	struct RenderTargetDesc
	{
		std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> m_renderTargetFormats{ DXGI_FORMAT_UNKNOWN };
		uint8_t m_numRenderTargets = 0;
		DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_UNKNOWN;
	};

	struct PipelineStateCreateDesc
	{
		DX12Shader* m_vertexShader;
		DX12Shader* m_pixelShader;
		D3D12_RASTERIZER_DESC m_rasterDesc{};
		D3D12_BLEND_DESC m_blendDesc{};
		D3D12_DEPTH_STENCIL_DESC m_depthStencilDesc{};
		DXGI_SAMPLE_DESC m_sampleDesc{};
		D3D12_PRIMITIVE_TOPOLOGY_TYPE m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		RenderTargetDesc m_renderTargetDesc{};
		std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElementDesc;
		PipelineType m_pipelineStateType = PipelineType::Graphics;
	};

	struct PipelineStateObject
	{
		std::unique_ptr<DX12PipelineState> m_pipelineState = nullptr;
		PipelineResourceMapping m_pipelineResourceMapping;
		std::unique_ptr<DX12RootSignature> m_rootSignature = nullptr;
		PipelineType m_pipelineType = PipelineType::Graphics;
	};

	struct PipelineInfo
	{
		PipelineStateObject* m_pipelineStateObject = nullptr;
		std::vector<DX12TextureResource*> m_renderTargets{};
		DX12TextureResource* m_depthStencilTarget = nullptr;
	};



	inline RenderTargetDesc CreateDefaultRenderTargetDesc()
	{
		RenderTargetDesc desc{};
		desc.m_renderTargetFormats.fill(DXGI_FORMAT_UNKNOWN);
		desc.m_numRenderTargets = 1;
		desc.m_depthStencilFormat = DXGI_FORMAT_UNKNOWN;
		return desc;
	}

	inline PipelineStateCreateDesc CreateDefaultPipelineStateCreateDesc()
	{
		PipelineStateCreateDesc desc{};
		/*desc.m_rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
		desc.m_rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.m_rasterDesc.FrontCounterClockwise = FALSE;
		desc.m_rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.m_rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.m_rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.m_rasterDesc.DepthClipEnable = TRUE;
		desc.m_rasterDesc.MultisampleEnable = FALSE;
		desc.m_rasterDesc.AntialiasedLineEnable = FALSE;
		desc.m_rasterDesc.ForcedSampleCount = 0;
		desc.m_rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;*/

		/*desc.m_blendDesc.AlphaToCoverageEnable = false;
		desc.m_blendDesc.IndependentBlendEnable = false;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			FALSE, FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			desc.m_blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		}*/

		/*desc.m_depthStencilDesc.DepthEnable = false;
		desc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.m_depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.m_depthStencilDesc.StencilEnable = false;
		desc.m_depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		desc.m_depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		desc.m_depthStencilDesc.FrontFace = defaultStencilOp;
		desc.m_depthStencilDesc.BackFace = defaultStencilOp;*/

		desc.m_sampleDesc.Count = 1;
		desc.m_sampleDesc.Quality = 0;

		desc.m_renderTargetDesc = CreateDefaultRenderTargetDesc();

		return desc;
	}
}