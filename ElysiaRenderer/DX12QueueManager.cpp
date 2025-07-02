#include "DX12QueueManager.h"
#include "Helper.h"
#include <iostream>

namespace ElysiaRenderer
{
	DX12QueueManager::DX12QueueManager(ID3D12Device* device)
	{
		m_graphicsQueue = new DX12Queue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_computeQueue = new DX12Queue(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		m_copyQueue = new DX12Queue(device, D3D12_COMMAND_LIST_TYPE_COPY);
	}
	DX12QueueManager::~DX12QueueManager()
	{
		delete m_graphicsQueue;
		delete m_computeQueue;
		delete m_copyQueue;
	}

	DX12Queue* DX12QueueManager::GetQueue(D3D12_COMMAND_LIST_TYPE queueType)
	{
		switch (queueType)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			return m_graphicsQueue;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			return m_computeQueue;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			return m_copyQueue;

		default:
			ElysiaHelper::ThrowRuntimeError("Bad command type lookup in queue manager.");
		}

		return nullptr;
	}

	bool DX12QueueManager::IsFenceComplete(uint64_t fenceValue)
	{
		auto waitQueue = GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
		return waitQueue->IsFenceCompleted(fenceValue);
	}
	void DX12QueueManager::WaitForFenceCPUBlocking(uint64_t fenceValue)
	{
		auto waitQueue = GetQueue((D3D12_COMMAND_LIST_TYPE)(fenceValue >> 56));
		return waitQueue->WaitForFenceCPUBlocking(fenceValue);
	}
	void DX12QueueManager::WaitForAllIdle()
	{
		m_graphicsQueue->WaitForIdle();
		m_computeQueue->WaitForIdle();
		m_copyQueue->WaitForIdle();
	}
}