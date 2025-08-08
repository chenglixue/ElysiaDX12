#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12RenderPassDescriptorHeap.h"


namespace ElysiaRenderer
{
	class DX12Device;

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

		//ID3D12GraphicsCommandList4* CreateBundle();

		void Reset();
		void Reset(ID3D12PipelineState* pipelineState);
		void AddBarrier(DX12GPUResource& resource, D3D12_RESOURCE_STATES newState);
		void FlushBarrier();
		void CopyTextureRegion(DX12GPUResource& destination, DX12GPUResource& source, size_t sourceOffset, SubResourceLayouts subResourceLayouts, UINT numSubResources);
		void BindDescriptorHeaps(UINT frameIndex);

	protected:
		DX12Device* m_device = nullptr;
		D3D12_COMMAND_LIST_TYPE m_contextType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ID3D12GraphicsCommandList4* m_commandList = nullptr;
		//std::vector<ID3D12GraphicsCommandList4*> m_bundles;
		std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_currDescriptorHeaps{ nullptr };
		std::array<ID3D12CommandAllocator*, NUM_FRAMES_IN_FLIGHT> m_commandAllocators{ nullptr };
		//std::vector<ID3D12CommandAllocator*>  m_bundleAllocators;
		std::array<D3D12_RESOURCE_BARRIER, MAX_QUEUED_BARRIERS> m_resourceBarriers{};
		uint32_t m_numQueuedBarriers = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE m_currSRVHeapHandle{ 0 };
		DX12RenderPassDescriptorHeap* m_currSRVHeap = nullptr;
	};
}