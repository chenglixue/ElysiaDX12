#include "stdafx.h"
#include "DX12PipelineState.h"
#include "../Utility/RenderTexture.h"

namespace ElysiaRenderer
{
	DX12PipelineState::DX12PipelineState()
	{

	}

	DX12PipelineState::DX12PipelineState(CComPtr<ID3D12PipelineState> pipelineState)
		: m_pipelineState(pipelineState)
	{

	}

	DX12PipelineState::DX12PipelineState(CComPtr<ID3D12PipelineState> pipelineState, DX12RootSignature* rootSignature)
		: m_pipelineState(pipelineState), m_rootSignature(rootSignature)
	{

	}

	DX12PipelineState::~DX12PipelineState()
	{

	}

	CComPtr<ID3D12PipelineState> DX12PipelineState::GetPipelineState()
	{
		return m_pipelineState;
	}
	DX12RootSignature*& DX12PipelineState::GetRootSignature()
	{
		return m_rootSignature;
	}
	PipelineType DX12PipelineState::GetPipelineType()
	{
		return m_pipelineType;
	}



	DX12GraphicsPipelineState::DX12GraphicsPipelineState()
		: DX12PipelineState()
	{
		m_pipelineType = PipelineType::Graphics;
	}
	DX12GraphicsPipelineState::DX12GraphicsPipelineState(CComPtr<ID3D12PipelineState> pipelineState, DX12RootSignature* rootSignature)
		: DX12PipelineState(pipelineState, rootSignature)
	{
		m_pipelineType = PipelineType::Graphics;
	}
	DX12GraphicsPipelineState::~DX12GraphicsPipelineState()
	{
	}


	DX12ComputePipelineState::DX12ComputePipelineState()
		: DX12PipelineState()
	{
		m_pipelineType = PipelineType::Compute;
	}
	DX12ComputePipelineState::DX12ComputePipelineState(CComPtr<ID3D12PipelineState> pipelineState, DX12RootSignature* rootSignature)
		: DX12PipelineState(pipelineState, rootSignature)
	{
		m_pipelineType = PipelineType::Compute;
	}
	DX12ComputePipelineState::~DX12ComputePipelineState()
	{
	}
}