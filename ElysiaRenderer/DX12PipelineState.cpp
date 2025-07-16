#include "DX12PipelineState.h"

namespace ElysiaRenderer
{
	DX12PipelineState::DX12PipelineState()
	{

	}

	DX12PipelineState::DX12PipelineState(ID3D12PipelineState* pipelineState)
		: m_pipelineState(pipelineState)
	{

	}

	DX12PipelineState::DX12PipelineState(ID3D12PipelineState* pipelineState, ID3D12RootSignature* rootSignature)
		: m_pipelineState(pipelineState), m_rootSignature(rootSignature)
	{

	}

	DX12PipelineState::~DX12PipelineState()
	{
		ElysiaHelper::SafeRelease(m_pipelineState);
	}





	DX12GraphicsPipelineState::DX12GraphicsPipelineState()
		: DX12PipelineState()
	{
		m_pipelineType = PipleineType::Graphics;
	}
	DX12GraphicsPipelineState::DX12GraphicsPipelineState(ID3D12PipelineState* pipelineState, ID3D12RootSignature* rootSignature)
		: DX12PipelineState(pipelineState, rootSignature)
	{
		m_pipelineType = PipleineType::Graphics;
	}
	DX12GraphicsPipelineState::~DX12GraphicsPipelineState()
	{
	}
}