#include "stdafx.h"
#include "PSOManager.h"

#include "Runtime/Core/DX12Device.h"
#include "Material.h"

namespace ElysiaRenderer
{
	std::unique_ptr<PSOManager> PSOManager::m_instance;
	std::once_flag PSOManager::m_initInstanceFlag;
	
	PSOManager::~PSOManager()
	{
		Destory();
	}

	void PSOManager::Init(ElysiaCore::DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;
	}

	void PSOManager::Destory()
	{

	}
	
	PipelineStateObject* PSOManager::GetGraphicsPipelineState(ElysiaCore::DX12Device* pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& PSODesc, DX12RootSignature* pRootSignature)
	{
		auto emplaceResult = m_graphicsPipelineStates.try_emplace(PSODesc);

		if (emplaceResult.second) 
		{
			CComPtr<ID3D12PipelineState> pipelineState = nullptr;
			ElysiaHelper::ThrowIfFailed(pDevice->GetDevice()->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&pipelineState)));

			auto graphicsPipeline = std::make_unique<DX12GraphicsPipelineState>(pipelineState, pRootSignature);

			std::unique_ptr<PipelineStateObject> pipelineStateObject = std::make_unique<PipelineStateObject>();
			pipelineStateObject->m_pipelineType = PipelineType::Graphics;
			pipelineStateObject->m_pipelineState = std::move(graphicsPipeline);

			emplaceResult.first->second = std::move(pipelineStateObject);
		}

		return emplaceResult.first->second.get();
	}

	PipelineStateObject* PSOManager::GetGraphicsPipelineState(ElysiaCore::DX12Device* pDevice, Material* pMaterial, UINT passIndex,
		const RenderTargetDesc& renderTargetDesc,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology)
	{
		auto& passData = pMaterial->GetPassData(passIndex);

		auto resourceMapping = PipelineResourceMapping();
		auto pDX12RootSignature = std::unique_ptr<DX12RootSignature>(pDevice->CreateRootSignature(
			*passData.pCurrVariantData->pMeshResourceLayout, resourceMapping));
		assert(pDX12RootSignature);
		assert(pDX12RootSignature->GetSignature());
		
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc{};
		PSODesc.pRootSignature = pDX12RootSignature->GetSignature();
		
		auto& stageShaders = passData.pCurrVariantData->StageShaders;
		if (stageShaders.count(ShaderType::Vertex) == 0 || stageShaders.count(ShaderType::Pixel) == 0)
		{
			ThrowRuntimeError("Missing VS/PS stage bytecode for this variant.");
		}
		auto vsBytecode = stageShaders.at(ShaderType::Vertex).bytecode;
		auto psBytecode = stageShaders.at(ShaderType::Pixel).bytecode;
		if (!vsBytecode || !psBytecode)
		{
			ThrowRuntimeError("Null bytecode for VS/PS.");
		}
		PSODesc.VS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = vsBytecode->GetBufferPointer(),
			.BytecodeLength = vsBytecode->GetBufferSize(),
		};
		PSODesc.PS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = psBytecode->GetBufferPointer(),
			.BytecodeLength = psBytecode->GetBufferSize(),
		};
		
		auto inputLayoutData = stageShaders.at(ShaderType::Vertex).ReflectionData;
		std::vector<std::string> localSemanticNames = inputLayoutData.InputElementSemanticNames;
		std::vector<D3D12_INPUT_ELEMENT_DESC> localInputElements;
		localInputElements.reserve(inputLayoutData.InputLayoutElementDescs.size());
		// copy each element and bind pointer into localSemanticNames
		for (size_t i = 0; i < inputLayoutData.InputLayoutElementDescs.size(); ++i)
		{
			D3D12_INPUT_ELEMENT_DESC elem = inputLayoutData.InputLayoutElementDescs[i];
			// point SemanticName to stable c_str() in localSemanticNames
			elem.SemanticName = localSemanticNames[i].c_str();
			localInputElements.push_back(elem);
		}
		D3D12_INPUT_LAYOUT_DESC localInputLayoutDesc;
		localInputLayoutDesc.pInputElementDescs = localInputElements.data();
		localInputLayoutDesc.NumElements = static_cast<UINT>(localInputElements.size());
		PSODesc.InputLayout = localInputLayoutDesc;

		PSODesc.BlendState = passData.BlendDesc;
		PSODesc.RasterizerState = passData.RasterizerDesc;
		PSODesc.DepthStencilState = passData.DepthStencilDesc;
		PSODesc.SampleMask = UINT_MAX;
		PSODesc.PrimitiveTopologyType = topology;
		PSODesc.SampleDesc = DXGI_SAMPLE_DESC
		{
			.Count = 1,
			.Quality = 0
		};
		
		for (UINT i = 0; i < renderTargetDesc.m_numRenderTargets; ++i)
		{
			PSODesc.RTVFormats[i] = renderTargetDesc.m_renderTargetFormats[i];
		}
		PSODesc.NumRenderTargets = renderTargetDesc.m_numRenderTargets;
		PSODesc.DSVFormat = renderTargetDesc.m_depthStencilFormat;

		auto pipelineStateObject = GetGraphicsPipelineState(pDevice, PSODesc, pDX12RootSignature.get());
		if (pipelineStateObject != nullptr)
		{
			pipelineStateObject->m_pipelineResourceMapping = resourceMapping;
			pipelineStateObject->m_rootSignature = std::move(pDX12RootSignature);
		}

		return pipelineStateObject;
	}

	PipelineStateObject* PSOManager::GetComputePipelineState(ElysiaCore::DX12Device* pDevice, Material* pMaterial, UINT passIndex)
	{
		auto& passData = pMaterial->GetPassData(passIndex);

		auto resourceMapping = PipelineResourceMapping();
		auto pDX12RootSignature = std::unique_ptr<DX12RootSignature>(pDevice->CreateRootSignature(*passData.pCurrVariantData->pMeshResourceLayout, resourceMapping));

		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc{};
		PSODesc.CS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = passData.pCurrVariantData->StageShaders.at(ShaderType::Compute).bytecode->GetBufferPointer(),
			.BytecodeLength = passData.pCurrVariantData->StageShaders.at(ShaderType::Compute).bytecode->GetBufferSize(),
		};
		PSODesc.pRootSignature = pDX12RootSignature->GetSignature();

		auto pipelineStateObject = GetComputePipelineState(pDevice, PSODesc, pDX12RootSignature.get());
		if (pipelineStateObject != nullptr)
		{
			pipelineStateObject->m_pipelineResourceMapping = resourceMapping;
			pipelineStateObject->m_rootSignature = std::move(pDX12RootSignature);
		}

		return pipelineStateObject;
	}

	PipelineStateObject* PSOManager::GetComputePipelineState(ElysiaCore::DX12Device* pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC& PSODesc, DX12RootSignature* pRootSignature)
	{
		auto emplaceResult = m_computePipelineStates.try_emplace(PSODesc);

		if (emplaceResult.second)
		{
			CComPtr<ID3D12PipelineState> pipelineState = nullptr;
			ElysiaHelper::ThrowIfFailed(pDevice->GetDevice()->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&pipelineState)));

			auto computePipeline = std::make_unique<DX12ComputePipelineState>(pipelineState, pRootSignature);

			std::unique_ptr<PipelineStateObject> pipelineStateObject = std::make_unique<PipelineStateObject>();
			pipelineStateObject->m_pipelineType = PipelineType::Compute;
			pipelineStateObject->m_pipelineState = std::move(computePipeline);

			emplaceResult.first->second = std::move(pipelineStateObject);
		}

		return emplaceResult.first->second.get();
	}
}