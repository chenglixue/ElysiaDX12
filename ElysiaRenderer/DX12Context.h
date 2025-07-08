#pragma once
#include "stdafx.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
	class DX12Context
	{
	public:
		DX12Context(DX12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
		virtual ~DX12Context();
		
		D3D12_COMMAND_LIST_TYPE GetContextType() const 
		{
			return m_contextType; 
		}
		ID3D12GraphicsCommandList4* GetCommandList() const 
		{ 
			return m_commandList; 
		}

		void Reset();

	protected:
		DX12Device* m_device;
		D3D12_COMMAND_LIST_TYPE m_contextType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ID3D12GraphicsCommandList4* m_commandList = nullptr;
		std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_currentDescriptorHeaps{ nullptr };
		std::array<ID3D12CommandAllocator*, NUM_FRAMES_IN_FLIGHT> m_commandAllocators{ nullptr };
		std::array<D3D12_RESOURCE_BARRIER, MAX_QUEUED_BARRIERS> m_resourceBarriers{};
		uint32_t m_numQueuedBarriers = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE mCurrentSRVHeapHandle{ 0 };
	};
} // namespace ElysiaRenderer