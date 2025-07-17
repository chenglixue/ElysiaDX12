#pragma once
#include "stdafx.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12Queue.h"
#include "DX12QueueManager.h"
#include "DX12StagingDescriptorHeap.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "DX12VertexBuffer.h"
#include "DX12ConstantBuffer.h"
#include "DX12BufferResource.h"
#include "DX12TextureResource.h"
#include "Definition.h"
#include "DX12Context.h"
#include "DX12GraphicsContext.h"
#include "DX12Shader.h"
#include "DX12RootSignature.h"
#include "DX12PipelineState.h"

namespace ElysiaRenderer
{
	using namespace DirectX::SimpleMath;

	struct ContextSubmissionResult
	{
		UINT frameID = 0;
		UINT submissionIndex = 0;
	};

	class DX12Device
	{
	public:
		DX12Device(HWND windowHandle, ElysiaHelper::UINT2 screenSize);
		~DX12Device();

		ID3D12Device5* GetDevice()
		{
			return m_device;
		}
		IDXGIFactory7* GetDXGIFactory()
		{
			return m_DXGIFactory;
		}
		IDXGISwapChain4* GetSwapChain()
		{
			return m_swapChain;
		}
		D3D12MA::Allocator* GetAllocator()
		{
			return m_allocator;
		}
		UINT GetFrameID() const
		{
			return m_frameID;
		}
		ElysiaHelper::UINT2 GetScreenSize() const
		{
			return m_screenSize;
		}
		DX12TextureResource& GetCurrBackBuffer()
		{
			return *m_backBuffers[m_swapChain->GetCurrentBackBufferIndex()];
		}

		std::unique_ptr<DX12GraphicsContext> CreateGraphicsContext();
		std::unique_ptr<DX12VertexBuffer>	CreateVertexBuffer(const BufferCreationDesc& bufferCreationDesc);
		std::unique_ptr<DX12Shader> CreateShader(ShaderCreateDesc& shaderCreateDesc);
		std::unique_ptr<DX12RootSignature> CreateRootSignature();
		std::unique_ptr<DX12GraphicsPipelineState> CreateGraphicsPipelineState(PipelineStateCreateDesc& pipelineStateCreateDesc);

		void DestoryContext(std::unique_ptr<DX12Context> context);
		void DestoryBuffer(std::unique_ptr<DX12GPUResource> buffer);
		void DestoryPipelineState(std::unique_ptr<DX12PipelineState> pipelineState);
		void DestoryShader(std::unique_ptr<DX12Shader> shader);

		ContextSubmissionResult SubmitContextWork(DX12Context* context);

		void WaitForIdle();

		void BeginFrame();
		void EndFrame();
		void Present();

	private:
		// 记录DX12Queu每次singal后的m_nextFenceValue
		struct EndOfFrameFences
		{
			uint64_t m_graphicsQueueFence = 0;
			uint64_t m_computeQueueFence = 0;
			uint64_t m_copyQueueFence = 0;
		};

		struct DestructionQueue
		{
			std::vector<std::unique_ptr<DX12GPUResource>> m_buffers;
			std::vector<std::unique_ptr<DX12TextureResource>> m_textures;
			std::vector<std::unique_ptr<DX12Context>> m_contexts;
			std::vector<std::unique_ptr<DX12PipelineState>> m_pipelineStates;
		};

		void InitializeDeviceResources(HWND windowHandle);
		void CreateWindowDependentResources();
		void ProcessDestruction(UINT frameIndex);

		ElysiaHelper::UINT2 m_screenSize;
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
		std::vector<UINT> m_freeReservedDescriptorIndices;
		std::array<std::unique_ptr<DX12TextureResource>, NUM_BACK_BUFFERS> m_backBuffers;
		std::array<EndOfFrameFences, NUM_FRAMES_IN_FLIGHT> m_endOfFrameFences;
		std::array<std::vector<std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE>>, NUM_FRAMES_IN_FLIGHT> m_contextSubmissions;
		std::array<DestructionQueue, NUM_FRAMES_IN_FLIGHT> m_destructionQueues;
	};
}