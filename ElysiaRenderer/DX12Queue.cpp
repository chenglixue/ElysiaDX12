#include "DX12Queue.h"
#include "Helper.h"

namespace ElysiaRenderer
{
	DX12Queue::DX12Queue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType)
	{
		m_commandQueue = nullptr;
		m_queueType = commandType;

		m_fence = nullptr;
		m_nextFenceValue = 1;
		m_lastCompletedFenceValue = 0;
		m_fenceEventHandle = 0;

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = m_queueType;
		queueDesc.NodeMask = 0;
		ElysiaHelper::AssertIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

		ElysiaHelper::AssertIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

		m_fence->Signal(m_lastCompletedFenceValue);

		m_fenceEventHandle = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);

		assert(m_fenceEventHandle != INVALID_HANDLE_VALUE);
	}

	DX12Queue::~DX12Queue()
	{
		ElysiaHelper::SafeRelease(m_commandQueue);
		ElysiaHelper::SafeRelease(m_fence);

		CloseHandle(m_fenceEventHandle);
	}

	uint64_t DX12Queue::PollCurrentFenceValue()
	{
		m_lastCompletedFenceValue = (std::max)(m_lastCompletedFenceValue, m_fence->GetCompletedValue());
		return m_lastCompletedFenceValue;
	}
	bool DX12Queue::IsFenceCompleted(uint64_t fenceValue)
	{
		if (m_lastCompletedFenceValue < fenceValue)
		{
			PollCurrentFenceValue();
		}

		return m_lastCompletedFenceValue >= fenceValue;
	}

	void DX12Queue::InsertWait(uint64_t fenceValue)
	{
		m_commandQueue->Wait(m_fence, fenceValue);
	}
	void DX12Queue::InsertWaitForQueueFence(DX12Queue* otherQueue, uint64_t fenceValue)
	{
		m_commandQueue->Wait(otherQueue->GetFence(), fenceValue);
	}
	void DX12Queue::InsertWaitForQueue(DX12Queue* otherQueue)
	{
		m_commandQueue->Wait(otherQueue->GetFence(), otherQueue->GetNextFenceValue() - 1);
	}

	// 判断m_lastCompletedFenceValue是否>=fenceValue，是则说明已经同步，无需等待；否则，还需等待GPU，同步之后，使得m_lastCompletedFenceValue = fenceValue,用于下次判断
	void DX12Queue::WaitForFenceCPUBlocking(uint64_t fenceValue)
	{
		if (IsFenceCompleted(fenceValue))
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lockGuard(m_eventMutex);

			m_fence->SetEventOnCompletion(fenceValue, m_fenceEventHandle);
			WaitForSingleObjectEx(m_fenceEventHandle, INFINITE, false);
			m_lastCompletedFenceValue = fenceValue;
		}
	}

	uint64_t DX12Queue::ExecuteCommandList(ID3D12CommandList* commandList)
	{
		ElysiaHelper::AssertIfFailed(static_cast<ID3D12GraphicsCommandList*>(commandList)->Close());
		m_commandQueue->ExecuteCommandLists(1, &commandList);
		
		return SingalFence();
	}

	uint64_t DX12Queue::SingalFence()
	{
		std::lock_guard<std::mutex> lockGuard(m_fenceMutex);

		m_commandQueue->Signal(m_fence, m_nextFenceValue);

		return m_nextFenceValue++;
	}
}