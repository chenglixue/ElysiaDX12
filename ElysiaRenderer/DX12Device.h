#pragma once
#include "stdafx.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12Queue.h"
#include "DX12StagingDescriptorHeap.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "DX12TextureBuffer.h"
#include "DX12VertexBuffer.h"
#include "DX12ConstantBuffer.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
	using namespace DirectX::SimpleMath;

	constexpr uint32_t NUM_BACK_BUFFERS = 3;
	constexpr uint32_t NUM_FRAMES_IN_FLIGHT = 2;
	constexpr uint32_t NUM_RTV_STAGING_DESCRIPTORS = 256;
	constexpr uint32_t NUM_DSV_STAGING_DESCRIPTORS = 32;
	constexpr uint32_t NUM_SRV_STAGING_DESCRIPTORS = 4096;

	class DX12Device
	{
	public:
		DX12Device(HWND windowHandle, UINT2 screenSize);
		~DX12Device();

	private:
		struct EndOfFrameFences
		{
			uint64_t m_graphicsQueueFence = 0;
			uint64_t m_computeQueueFence = 0;
			uint64_t m_copyQueueFence = 0;
		};

		struct DestructionQueue
		{
			std::vector<std::unique_ptr<DX12VertexBuffer>> m_vertexBuffer;
			std::vector<std::unique_ptr<DX12ConstantBuffer>> m_constantBuffer;
			std::vector<std::unique_ptr<DX12TextureBuffer>> m_textureBuffer;
		};

		void InitializeDeviceResources();
		void CreateWindowDependentResources(HWND windowHandle, UINT2 screenSize);

		UINT2 m_screenSize;
		UINT m_frameID;

		ID3D12Device5* m_device = nullptr;
		IDXGIFactory7* m_DXGIFactory = nullptr;
		IDXGISwapChain4* m_swapChain = nullptr;
		D3D12MA::Allocator* m_allocator = nullptr;
		std::unique_ptr<DX12Queue> m_graphicsQueue;
		std::unique_ptr<DX12Queue> m_computeQueue;
		std::unique_ptr<DX12Queue> m_copyQueue;
		std::unique_ptr<DX12StagingDescriptorHeap> m_RTVStagingDescriptorHeap;
		std::unique_ptr<DX12StagingDescriptorHeap> m_DSVStagingDescriptorHeap;
		std::unique_ptr<DX12StagingDescriptorHeap> m_SRVStagingDescriptorHeap;
		std::unique_ptr<DX12RenderPassDescriptorHeap> m_renderPassDescriptorHeap;
		std::array<std::unique_ptr<DX12TextureBuffer>, NUM_BACK_BUFFERS> m_backBuffers;
		std::array<std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE>, NUM_FRAMES_IN_FLIGHT> m_contextSubmissions;
		std::array<DestructionQueue, NUM_FRAMES_IN_FLIGHT> m_destructionQueues;
	};
}