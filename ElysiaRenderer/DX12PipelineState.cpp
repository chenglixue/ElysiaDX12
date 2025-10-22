#include "DX12PipelineState.h"

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
		/*if (m_rootSignature != nullptr)
		{
			delete m_rootSignature;
			m_rootSignature = nullptr;
		}*/
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
}