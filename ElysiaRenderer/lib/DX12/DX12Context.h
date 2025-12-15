#pragma once
#include "../Utility/Helper.h"
#include "../Utility/RenderTexture.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;

	class DX12Device;
	class DX12GPUResource;
	class DX12RenderPassDescriptorHeap;

	class DX12Context
	{
	public:
		DX12Context(DX12Device* device, D3D12_COMMAND_LIST_TYPE commandType);
		virtual ~DX12Context();
		
		D3D12_COMMAND_LIST_TYPE GetContextType() const;
		ID3D12GraphicsCommandList4* GetCommandList() const;

		void Reset();
		void Reset(CComPtr<ID3D12PipelineState> pipelineState);
		void AddBarrier(DX12GPUResource& resource, D3D12_RESOURCE_STATES newState, bool isFlush = true);
		void AddBarrier(RenderTexture* resource, D3D12_RESOURCE_STATES newState, bool isFlush = true);

		void FlushBarrier();
		void CopyTextureRegion(DX12GPUResource& destination, ID3D12Resource* source, size_t sourceOffset, 
			SubResourceLayouts subResourceLayouts, UINT numSubResources);
		void CopyBufferRegion(DX12GPUResource& destination, UINT64 destOffset, 
			DX12GPUResource& source, UINT64 sourceOffset, UINT64 numBytes);
		void BindDescriptorHeaps(UINT frameIndex);

	protected:
		DX12Device* m_device = nullptr;
		D3D12_COMMAND_LIST_TYPE m_contextType = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ID3D12GraphicsCommandList4* m_commandList = nullptr;
		std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_currDescriptorHeaps{ nullptr };
		std::array<ID3D12CommandAllocator*, NUM_FRAMES_IN_FLIGHT> m_commandAllocators{ nullptr };
		std::array<D3D12_RESOURCE_BARRIER, MAX_QUEUED_BARRIERS> m_resourceBarriers{};
		uint32_t m_numQueuedBarriers = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE m_currSRVHeapHandle{ 0 };
		DX12RenderPassDescriptorHeap* m_currSRVHeap = nullptr;
	};
}