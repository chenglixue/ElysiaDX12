#include "stdafx.h"
#include "PSOManager.h"

#include "lib/DX12/DX12Device.h"
#include "Material.h"

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

	PipelineStateObject* PSOManager::GetGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& PSODesc, DX12RootSignature* pRootSignature)
	{
		auto emplaceResult = m_graphicsPipelineStates.try_emplace(PSODesc);

		if (emplaceResult.second) 
		{
			CComPtr<ID3D12PipelineState> pipelineState = nullptr;
			ElysiaHelper::ThrowIfFailed(GetDevice()->GetDevice()->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&pipelineState)));

			auto graphicsPipeline = std::make_unique<DX12GraphicsPipelineState>(pipelineState, pRootSignature);

			std::unique_ptr<PipelineStateObject> pipelineStateObject = std::make_unique<PipelineStateObject>();
			pipelineStateObject->m_pipelineType = PipelineType::Graphics;
			pipelineStateObject->m_pipelineState = std::move(graphicsPipeline);

			emplaceResult.first->second = std::move(pipelineStateObject);
		}

		return emplaceResult.first->second.get();
	}

	PipelineStateObject* PSOManager::GetGraphicsPipelineState(Material* pMaterial, UINT passIndex,
		const RenderTargetDesc& renderTargetDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
	{
		auto& passData = pMaterial->GetPassData(passIndex);

		auto resourceMapping = PipelineResourceMapping();
		auto pDX12RootSignature = std::unique_ptr<DX12RootSignature>(GetDevice()->CreateRootSignature(*passData.MeshResourceLayouts, resourceMapping));

		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc{};

		PSODesc.pRootSignature = pDX12RootSignature->GetSignature();
		PSODesc.VS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = passData.pVSShader->GetShader()->GetBufferPointer(),
			.BytecodeLength = passData.pVSShader->GetShader()->GetBufferSize(),
		};
		PSODesc.PS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = passData.pPSShader->GetShader()->GetBufferPointer(),
			.BytecodeLength = passData.pPSShader->GetShader()->GetBufferSize(),
		};

		PSODesc.BlendState = passData.BlendDesc;
		PSODesc.RasterizerState = passData.RasterizerDesc;
		PSODesc.DepthStencilState = passData.DepthStencilDesc;
		PSODesc.SampleMask = UINT_MAX;
		PSODesc.InputLayout = passData.pVSShader->GetInputElementDesc();
		PSODesc.PrimitiveTopologyType = topology;
		PSODesc.SampleDesc = DXGI_SAMPLE_DESC
		{
			.Count = 1,
			.Quality = 0
		};
		
		auto desc = CreateDefaultRenderTargetDesc();
		for (UINT i = 0; i < renderTargetDesc.m_numRenderTargets; ++i)
		{
			PSODesc.RTVFormats[i] = desc.m_renderTargetFormats[i];
		}
		PSODesc.NumRenderTargets = desc.m_numRenderTargets;
		PSODesc.DSVFormat = desc.m_depthStencilFormat;

		for (UINT i = 0; i < renderTargetDesc.m_numRenderTargets; ++i)
		{
			PSODesc.RTVFormats[i] = renderTargetDesc.m_renderTargetFormats[i];
		}
		PSODesc.DSVFormat = renderTargetDesc.m_depthStencilFormat;
		PSODesc.NumRenderTargets = renderTargetDesc.m_numRenderTargets;

		auto pipelineStateObject = GetGraphicsPipelineState(PSODesc, pDX12RootSignature.get());
		if (pipelineStateObject != nullptr)
		{
			pipelineStateObject->m_pipelineResourceMapping = resourceMapping;
			pipelineStateObject->m_rootSignature = std::move(pDX12RootSignature);
		}

		return pipelineStateObject;
	}

	PipelineStateObject* PSOManager::GetComputePipelineState(Material* pMaterial, UINT passIndex)
	{
		auto& passData = pMaterial->GetPassData(passIndex);

		auto resourceMapping = PipelineResourceMapping();
		auto pDX12RootSignature = std::unique_ptr<DX12RootSignature>(GetDevice()->CreateRootSignature(*passData.MeshResourceLayouts, resourceMapping));

		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc{};
		PSODesc.CS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = passData.pCSShader->GetShader()->GetBufferPointer(),
			.BytecodeLength = passData.pCSShader->GetShader()->GetBufferSize(),
		};
		PSODesc.pRootSignature = pDX12RootSignature->GetSignature();

		auto pipelineStateObject = GetComputePipelineState(PSODesc, pDX12RootSignature.get());
		if (pipelineStateObject != nullptr)
		{
			pipelineStateObject->m_pipelineResourceMapping = resourceMapping;
			pipelineStateObject->m_rootSignature = std::move(pDX12RootSignature);
		}

		return pipelineStateObject;
	}

	PipelineStateObject* PSOManager::GetComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC& PSODesc, DX12RootSignature* pRootSignature)
	{
		auto emplaceResult = m_computePipelineStates.try_emplace(PSODesc);

		if (emplaceResult.second)
		{
			CComPtr<ID3D12PipelineState> pipelineState = nullptr;
			ElysiaHelper::ThrowIfFailed(GetDevice()->GetDevice()->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&pipelineState)));

			auto computePipeline = std::make_unique<DX12ComputePipelineState>(pipelineState, pRootSignature);

			std::unique_ptr<PipelineStateObject> pipelineStateObject = std::make_unique<PipelineStateObject>();
			pipelineStateObject->m_pipelineType = PipelineType::Compute;
			pipelineStateObject->m_pipelineState = std::move(computePipeline);

			emplaceResult.first->second = std::move(pipelineStateObject);
		}

		return emplaceResult.first->second.get();
	}
}