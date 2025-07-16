#include "DX12Context.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
	class DX12Device;

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
	void DX12Context::Reset(ID3D12PipelineState* pipelineState)
	{
		UINT currFrameID = m_device->GetFrameID();
		m_commandAllocators[currFrameID]->Reset();
		m_commandList->Reset(m_commandAllocators[currFrameID], pipelineState);
	}

	void DX12Context::AddBarrier(DX12GPUResource& resource, D3D12_RESOURCE_STATES newState)
	{
		if (m_numQueuedBarriers >= MAX_QUEUED_BARRIERS)
		{
			FlushBarrier();
		}

		D3D12_RESOURCE_STATES oldState = resource.GetUsageState();
		if (oldState != newState)
		{
			D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarriers[m_numQueuedBarriers];
			m_numQueuedBarriers++;

			// Describes the transition of subresources between different usages
			barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrierDesc.Transition = {resource.GetResource(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, oldState, newState};

			resource.SetUsageState(newState);
		}
	}

	void DX12Context::FlushBarrier()
	{
		if (m_numQueuedBarriers > 0)
		{
			// synchronize multiple accesses to resources.
			m_commandList->ResourceBarrier(m_numQueuedBarriers, m_resourceBarriers.data());
			m_numQueuedBarriers = 0;
		}
	}
}