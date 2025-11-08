#include "stdafx.h"
#include "PSOManager.h"

#include "DX12Device.h"
#include "RenderMaterial.h"

namespace ElysiaRenderer
{
	std::unique_ptr<PSOManager> g_pPSOManager = nullptr;

	PSOManager::~PSOManager()
	{
		Destory();
	}

	void PSOManager::Init()
	{

	}

	void PSOManager::Destory()
	{

	}

	PipelineStateObject* PSOManager::GetGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& PSODesc)
	{
		auto emplaceResult = m_pipelineStates.try_emplace(PSODesc);

		if (emplaceResult.second) 
		{
			CComPtr<ID3D12PipelineState> pipelineState = nullptr;
			ElysiaHelper::ThrowIfFailed(GetDevice()->GetDevice()->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&pipelineState)));

			auto graphicsPipeline = std::make_unique<DX12GraphicsPipelineState>(pipelineState, PSODesc.pRootSignature);

			std::unique_ptr<PipelineStateObject> pipelineStateObject = std::make_unique<PipelineStateObject>();
			pipelineStateObject->m_pipelineType = PipelineType::Graphics;
			pipelineStateObject->m_pipelineState = std::move(graphicsPipeline);

			return pipelineStateObject.get();
		}

		return nullptr;
	}

	PipelineStateObject* PSOManager::GetGraphicsPipelineState(RenderMaterial* pMaterial, UINT passIndex,
		const RenderTargetDesc& renderTargetDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
	{
		auto& passData = pMaterial->GetPassData(passIndex);

		auto resourceMapping = PipelineResourceMapping();
		auto pDX12RootSignature = std::make_unique<DX12RootSignature>(GetDevice()->CreateRootSignature(*passData.MeshResourceLayouts, resourceMapping));

		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc
		{
			.pRootSignature = pDX12RootSignature->GetSignature(),
			.VS = D3D12_SHADER_BYTECODE
			{
				.pShaderBytecode = passData.pVSShader->GetShader()->GetBufferPointer(),
				.BytecodeLength = passData.pVSShader->GetShader()->GetBufferSize(),
			},
			.PS = D3D12_SHADER_BYTECODE
			{
				.pShaderBytecode = passData.pPSShader->GetShader()->GetBufferPointer(),
				.BytecodeLength = passData.pPSShader->GetShader()->GetBufferSize(),
			},

			.BlendState = passData.BlendDesc,
			.SampleMask = UINT_MAX,
			.RasterizerState = passData.RasterizerDesc,
			.DepthStencilState = passData.DepthStencilDesc,
			.InputLayout = passData.pVSShader->GetInputElementDesc(),
			.PrimitiveTopologyType = topology,
			.NumRenderTargets = renderTargetDesc.m_numRenderTargets,
			.DSVFormat = renderTargetDesc.m_depthStencilFormat,
			.SampleDesc = DXGI_SAMPLE_DESC
			{
				.Count = 1,
				.Quality = 0
			},

			
		};
		for (UINT i = 0; i < renderTargetDesc.m_numRenderTargets; ++i)
		{
			PSODesc.RTVFormats[i] = renderTargetDesc.m_renderTargetFormats[i];
		}

		auto pipelineStateObject = GetGraphicsPipelineState(PSODesc);
		if (pipelineStateObject != nullptr)
		{
			pipelineStateObject->m_pipelineResourceMapping = resourceMapping;
			pipelineStateObject->m_rootSignature = std::move(pDX12RootSignature);
		}

		return pipelineStateObject;
	}
}