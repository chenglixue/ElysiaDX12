#pragma once
#include "../Utility/Helper.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12Queue;

	class DX12QueueManager
	{
	public:
		DX12QueueManager(ID3D12Device* device);
		~DX12QueueManager();

		DX12Queue* GetGraphicsQueue();
		DX12Queue* GetComputeQueue();
		DX12Queue* GetCopyQueue();

		DX12Queue* GetQueue(D3D12_COMMAND_LIST_TYPE queueType);

		bool IsFenceComplete(uint64_t fenceValue);
		void WaitForFenceCPUBlocking(uint64_t fenceValue);
		void WaitForAllIdle();
	
	private:
		std::unique_ptr<DX12Queue> m_graphicsQueue;
		std::unique_ptr<DX12Queue> m_computeQueue;
		std::unique_ptr<DX12Queue> m_copyQueue;
	};
}
