#pragma once
#include "Programs/Helper.h"

namespace ElysiaCore
{
	class DX12Queue
	{
	public:
		DX12Queue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
		~DX12Queue();

		ID3D12CommandQueue* GetCommandQueue()
		{
			return m_commandQueue;
		}
		ID3D12Fence* GetFence()
		{
			return m_fence;
		}
		uint64_t GetNextFenceValue()
		{
			return m_nextFenceValue;
		}
		uint64_t GetLastCompletedFenceValue()
		{
			return m_lastCompletedFenceValue;
		}

		uint64_t ExecuteCommandList(ID3D12CommandList* commandList);
		uint64_t SingalFence();

		bool IsFenceCompleted(uint64_t fenceValue);
		uint64_t PollCurrentFenceValue();

		// GPU Wait
		void InsertWait(uint64_t fenceValue);
		void InsertWaitForQueueFence(DX12Queue* otherQueue, uint64_t fenceValue);
		void InsertWaitForQueue(DX12Queue* otherQueue);

		// CPU Wait
		void WaitForFenceCPUBlocking(uint64_t fenceValue);
		void WaitForIdle() 
		{
			// m_nextFenceValue - 1:每次singal后，m_nextFenceValue++
			WaitForFenceCPUBlocking(m_nextFenceValue - 1); 
		}

	private:
		ID3D12CommandQueue* m_commandQueue;
		D3D12_COMMAND_LIST_TYPE m_queueType;

		ID3D12Fence* m_fence;
		uint64_t m_nextFenceValue;
		uint64_t m_lastCompletedFenceValue;
		std::mutex m_fenceMutex;
		std::mutex m_eventMutex;

		HANDLE m_fenceEventHandle;
	};
}