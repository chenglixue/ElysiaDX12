#include "DX12QueueManager.h"
#include "DX12Queue.h"

namespace ElysiaRenderer
{
	DX12QueueManager::DX12QueueManager(ID3D12Device* device)
	{
		m_graphicsQueue = std::make_unique<DX12Queue>(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_computeQueue = std::make_unique<DX12Queue>(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		m_copyQueue = std::make_unique<DX12Queue>(device, D3D12_COMMAND_LIST_TYPE_COPY);
	}
	DX12QueueManager::~DX12QueueManager()
	{
		
	}

	DX12Queue* DX12QueueManager::GetGraphicsQueue()
	{
		return m_graphicsQueue.get();
	}
	DX12Queue* DX12QueueManager::GetComputeQueue()
	{
		return m_computeQueue.get();
	}
	DX12Queue* DX12QueueManager::GetCopyQueue()
	{
		return m_copyQueue.get();
	}

	DX12Queue* DX12QueueManager::GetQueue(D3D12_COMMAND_LIST_TYPE queueType)
	{
		switch (queueType)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			return m_graphicsQueue.get();
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			return m_computeQueue.get();
		case D3D12_COMMAND_LIST_TYPE_COPY:
			return m_copyQueue.get();

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