#include "stdafx.h"
#include "PSOManager.h"

#include "DX12Device.h"

namespace ElysiaRenderer
{
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

	ID3D12PipelineState* PSOManager::GetPipelineState(D3D12_GRAPHICS_PIPELINE_STATE_DESC const& stateDesc)
	{
		auto emplaceResult = m_pipelineStates.try_emplace(stateDesc);

		if (emplaceResult.second) 
		{
			ThrowIfFailed(GetDevice()->GetDevice()->CreateGraphicsPipelineState(&stateDesc, IID_PPV_ARGS(&emplaceResult.first->second)));
		}

		return emplaceResult.first->second;
	}
}