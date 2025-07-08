#include "DX12Context.h"

namespace ElysiaRenderer
{
	DX12Context::DX12Context(DX12Device* device, D3D12_COMMAND_LIST_TYPE commandType) :
		m_device(device), m_contextType(commandType)
	{
		for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			AssertIfFailed(m_device->GetDevice()->
				CreateCommandAllocator(commandType, IID_PPV_ARGS(&m_commandAllocators[i])));
		}

		AssertIfFailed(m_device->GetDevice()->
			CreateCommandList1(0, commandType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)));
	}
	DX12Context::~DX12Context()
	{
		ElysiaHelper::SafeRelease(m_commandList);

		for (int frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
		{
			ElysiaHelper::SafeRelease(m_commandAllocators[frameIndex]);
		}
	}

	void DX12Context::Reset()
	{
		UINT currFrameID = m_device->GetFrameID();
		m_commandAllocators[currFrameID]->Reset();
		m_commandList->Reset(m_commandAllocators[currFrameID], nullptr);
	}


}