#pragma once
#include "stdafx.h"
//#include "DX12Shader.h"

namespace ElysiaRenderer
{
	extern class DX12RootSignature;
	extern class DX12Shader;

	enum class PipleineType : uint8_t
	{
		Graphics = 0,
		Compute = 1
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
		DX12RootSignature* m_rootSignature;
		D3D12_RASTERIZER_DESC m_rasterDesc{};
		D3D12_BLEND_DESC m_blendDesc{};
		D3D12_DEPTH_STENCIL_DESC m_depthStencilDesc{};
		DXGI_SAMPLE_DESC m_sampleDesc{};
		D3D12_PRIMITIVE_TOPOLOGY_TYPE m_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		RenderTargetDesc m_renderTargetDesc{};
		std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElementDesc;
		PipleineType m_pipelineStateType = PipleineType::Graphics;
	};

	class DX12PipelineState
	{
	public:
		DX12PipelineState();
		DX12PipelineState(ID3D12PipelineState* pipelineState);
		virtual ~DX12PipelineState();

		ID3D12PipelineState* GetPipelineState()
		{
			return m_pipelineState;
		}

	private:
		ID3D12PipelineState* m_pipelineState;
	};

	class DX12GraphicsPipelineState : public DX12PipelineState
	{
	public:
		DX12GraphicsPipelineState();
		DX12GraphicsPipelineState(ID3D12PipelineState* pipelineState);
		~DX12GraphicsPipelineState() override;

		PipleineType GetPipelineType()
		{
			return m_pipelineType;
		}

	private:
		PipleineType m_pipelineType;
	};

	inline static PipelineStateCreateDesc CreateDefaultPipelineStateCreateDesc()
	{
		PipelineStateCreateDesc desc{};
		desc.m_rasterDesc.CullMode = D3D12_CULL_MODE_BACK;
		desc.m_rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.m_rasterDesc.FrontCounterClockwise = false;
		desc.m_rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.m_rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.m_rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.m_rasterDesc.DepthClipEnable = true;
		desc.m_rasterDesc.MultisampleEnable = false;
		desc.m_rasterDesc.AntialiasedLineEnable = false;
		desc.m_rasterDesc.ForcedSampleCount = 0;
		desc.m_rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		desc.m_blendDesc.AlphaToCoverageEnable = false;
		desc.m_blendDesc.IndependentBlendEnable = false;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			false, false,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			desc.m_blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;
		}

		desc.m_depthStencilDesc.DepthEnable = true;
		desc.m_depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.m_depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.m_depthStencilDesc.StencilEnable = false;
		desc.m_depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		desc.m_depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		desc.m_depthStencilDesc.FrontFace = defaultStencilOp;
		desc.m_depthStencilDesc.BackFace = defaultStencilOp;

		desc.m_sampleDesc.Count = 1;
		desc.m_sampleDesc.Quality = 0;

		return desc;
	}
}