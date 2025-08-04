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

	DX12PipelineState::DX12PipelineState(ID3D12PipelineState* pipelineState, DX12RootSignature* rootSignature)
		: m_pipelineState(pipelineState), m_rootSignature(rootSignature)
	{

	}

	DX12PipelineState::~DX12PipelineState()
	{
		ElysiaHelper::SafeRelease(m_pipelineState);
		if (m_rootSignature != nullptr)
		{
			delete m_rootSignature;
		}
	}





	DX12GraphicsPipelineState::DX12GraphicsPipelineState()
		: DX12PipelineState()
	{
		m_pipelineType = PipleineType::Graphics;
	}
	DX12GraphicsPipelineState::DX12GraphicsPipelineState(ID3D12PipelineState* pipelineState, DX12RootSignature* rootSignature)
		: DX12PipelineState(pipelineState, rootSignature)
	{
		m_pipelineType = PipleineType::Graphics;
	}
	DX12GraphicsPipelineState::~DX12GraphicsPipelineState()
	{
		//ElysiaHelper::SafeRelease(m_pipelineState);
		//ElysiaHelper::SafeRelease(m_depthStencilRT);
	}
}