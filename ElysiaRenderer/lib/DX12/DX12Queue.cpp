#include "stdafx.h"
#include "DX12Queue.h"
#include "../Utility/Helper.h"

namespace ElysiaRenderer
{
	DX12Queue::DX12Queue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType)	:
		m_queueType(commandType),
		m_nextFenceValue(1),
		m_lastCompletedFenceValue(0)
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = m_queueType;
		queueDesc.NodeMask = 0;
		ElysiaHelper::AssertIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

		ElysiaHelper::AssertIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

		m_fence->Signal(m_lastCompletedFenceValue);

		m_fenceEventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

		assert(m_fenceEventHandle != INVALID_HANDLE_VALUE);
	}

	DX12Queue::~DX12Queue()
	{
		if(m_fenceEventHandle)
		{
			CloseHandle(m_fenceEventHandle);
			m_fenceEventHandle = nullptr;
		}
	}

	bool DX12Queue::IsFenceCompleted(uint64_t fenceValue)
	{
		uint64_t completed = m_lastCompletedFenceValue.load(std::memory_order_acquire);
		if (completed >= fenceValue) return true;
		
		// Only query GPU if needed
		completed = m_fence->GetCompletedValue();
		m_lastCompletedFenceValue.store(completed, std::memory_order_release);
		return completed >= fenceValue;
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
		if (IsFenceCompleted(fenceValue)) return;

		// Use single shared event (manual reset cleared after wait)
		m_fence->SetEventOnCompletion(fenceValue, m_fenceEventHandle);
		WaitForSingleObject(m_fenceEventHandle, INFINITE);
		ResetEvent(m_fenceEventHandle);  // 必须手动重置！

		m_lastCompletedFenceValue.store(fenceValue, std::memory_order_release);
	}
	
	void DX12Queue::WaitForIdle() 
	{
		// m_nextFenceValue - 1:每次singal后，m_nextFenceValue++
		WaitForFenceCPUBlocking(m_nextFenceValue.load(std::memory_order_acquire) - 1); 
	}

	uint64_t DX12Queue::ExecuteCommandList(ID3D12CommandList* commandList)
	{
		ElysiaHelper::AssertIfFailed(static_cast<ID3D12GraphicsCommandList*>(commandList)->Close());
		m_commandQueue->ExecuteCommandLists(1, &commandList);
		
		return SingalFence();
	}

	uint64_t DX12Queue::SingalFence()
	{
		uint64_t value = m_nextFenceValue.fetch_add(1, std::memory_order_relaxed);

		m_commandQueue->Signal(m_fence, value);

		return value;
	}
}