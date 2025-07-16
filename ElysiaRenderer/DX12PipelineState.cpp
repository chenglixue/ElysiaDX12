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

	DX12PipelineState::~DX12PipelineState()
	{
		ElysiaHelper::SafeRelease(m_pipelineState);
	}





	DX12GraphicsPipelineState::DX12GraphicsPipelineState()
		: DX12PipelineState()
	{
		m_pipelineType = PipleineType::Graphics;
	}
	DX12GraphicsPipelineState::DX12GraphicsPipelineState(ID3D12PipelineState* pipelineState)
	{
		DX12GraphicsPipelineState();
	}
	DX12GraphicsPipelineState::~DX12GraphicsPipelineState()
	{
	}
}