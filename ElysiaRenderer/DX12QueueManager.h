#pragma once
#include "stdafx.h"
#include "DX12Queue.h"

namespace ElysiaRenderer
{
	class DX12QueueManager
	{
	public:
		DX12QueueManager(ID3D12Device* device);
		~DX12QueueManager();

		DX12Queue* GetGraphicsQueue()
		{
			return m_graphicsQueue;
		}
		DX12Queue* GetComputeQueue()
		{
			return m_computeQueue;
		}
		DX12Queue* GetCopyQueue()
		{
			return m_copyQueue;
		}

		DX12Queue* GetQueue(D3D12_COMMAND_LIST_TYPE queueType);

		bool IsFenceComplete(uint64_t fenceValue);
		void WaitForFenceCPUBlocking(uint64_t fenceValue);
		void WaitForAllIdle();
	
	private:
		DX12Queue* m_graphicsQueue;
		DX12Queue* m_computeQueue;
		DX12Queue* m_copyQueue;
	};
}
