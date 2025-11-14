#pragma once
#include "DX12Shader.h"
#include "DX12PipelineState.h"
#include "DX12RootSignature.h"
#include "TextureUtility.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	class DX12TextureResource;
	class DX12RenderPassDescriptorHeap;
	class DX12DescriptorHeapHandle;
	class DX12Context;
	class DX12UploadContext;
	class DX12GraphicsContext;
	class DX12BufferResource;
	class DX12StagingDescriptorHeap;
	class DX12Queue;

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

		ID3D12Device5*			GetDevice()
		{
			return m_device;
		}
		IDXGIFactory7*			GetDXGIFactory()
		{
			return m_DXGIFactory;
		}
		IDXGISwapChain4*		GetSwapChain()
		{
			return m_swapChain;
		}
		D3D12MA::Allocator*		GetAllocator()
		{
			return m_allocator;
		}
		UINT					GetFrameID() const
		{
			return m_frameID;
		}
		UINT					GetFrameIndex() const
		{
			return m_frameIndex;
		}
		const Vector4			GetScreenSize() const
		{
			return std::move(Vector4(static_cast<float>(m_screenSize.x), static_cast<float>(m_screenSize.y),
				1.f / static_cast<float>(m_screenSize.x), 1.f / static_cast<float>(m_screenSize.y)));
		}
		DX12TextureResource&	GetCurrBackBuffer()
		{
			return *m_backBuffers[m_swapChain->GetCurrentBackBufferIndex()];
		}
		DX12RenderPassDescriptorHeap& GetSRVHeap(UINT frameIndex)
		{
			return *m_SRVRenderPassDescriptorHeaps[frameIndex];
		}
		DX12RenderPassDescriptorHeap& GetSamplerHeap()
		{
			return *m_samplerRenderPassDescriptorHeap;
		}
		DX12DescriptorHeapHandle& GetImguiDescriptor(uint32_t index)
		{ 
			return m_ImguiDescriptors[index];
		}
		DX12UploadContext* GetUploadContext() const noexcept
		{
			return m_uploadContexts[m_frameID].get();
		}

		std::unique_ptr<DX12GraphicsContext>		CreateGraphicsContext();
		std::unique_ptr<DX12BufferResource>			CreateBuffer(const BufferCreationDesc& bufferCreationDesc);
		std::unique_ptr<DX12TextureResource>		CreateTextureFromFile(const TextureCreationDesc& textureCreationDesc);
		std::unique_ptr<DX12TextureResource>		CreateTexture(const TexCreateDesc& desc);
		std::unique_ptr<DX12Shader>					CreateShader(ShaderCreateDesc& shaderCreateDesc);
		void										CreateSamplers(D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void										CreateRootParameters(DX12RootSignature* rootSignature, std::vector<DX12RootParameter*>& rootParamters);
		DX12RootSignature*							CreateRootSignature(const PipelineResourceLayout& resourceLayout, PipelineResourceMapping& resourceMapping);

		void DestoryShader(std::unique_ptr<DX12Shader> shader);
		void DestoryBuffer(std::unique_ptr<DX12BufferResource> buffer);
		void DestoryPipelineState(std::unique_ptr<DX12PipelineState> pipelineState);
		void DestoryContext(std::unique_ptr<DX12Context> context);
		void DestoryTexture(std::unique_ptr<DX12TextureResource> texture);

		void CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
			uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);
		void CopyDescriptorFromStageToRenderPass(DX12DescriptorHeapHandle SRVHandle, UINT index);
		ContextSubmissionResult SubmitContextWork(DX12Context& context);

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
			std::vector<std::unique_ptr<DX12BufferResource>> m_buffers;
			std::vector<std::unique_ptr<DX12TextureResource>> m_textures;
			std::vector<std::unique_ptr<DX12Context>> m_contexts;
			std::vector<std::unique_ptr<DX12PipelineState>> m_pipelineStates;
		};

		void InitializeDeviceResources(HWND windowHandle);
		void CreateWindowDependentResources();
		void ProcessDestruction(UINT frameIndex);

		ElysiaHelper::UINT2 m_screenSize;
		UINT m_frameIndex;
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
		std::array<std::unique_ptr<DX12RenderPassDescriptorHeap>, NUM_FRAMES_IN_FLIGHT> m_SRVRenderPassDescriptorHeaps;
		std::unique_ptr<DX12RenderPassDescriptorHeap> m_CBVRenderPassDescriptorHeap;
		std::unique_ptr<DX12RenderPassDescriptorHeap> m_samplerRenderPassDescriptorHeap;
		std::unique_ptr<DX12RenderPassDescriptorHeap> m_UAVRenderPassDescriptorHeap;
		std::array<DX12DescriptorHeapHandle, NUM_FRAMES_IN_FLIGHT> m_ImguiDescriptors;
		std::vector<UINT> m_freeReservedDescriptorIndices;
		std::array<std::unique_ptr<DX12UploadContext>, NUM_FRAMES_IN_FLIGHT> m_uploadContexts;
		std::array<std::unique_ptr<DX12TextureResource>, NUM_BACK_BUFFERS> m_backBuffers;
		std::array<EndOfFrameFences, NUM_FRAMES_IN_FLIGHT> m_endOfFrameFences;
		std::array<std::vector<std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE>>, NUM_FRAMES_IN_FLIGHT> m_contextSubmissions;
		std::array<DestructionQueue, NUM_FRAMES_IN_FLIGHT> m_destructionQueues;
	};

	extern std::unique_ptr<DX12Device> g_device;

	inline DX12Device* GetDevice()
	{
		if (g_device == nullptr)
		{
			ThrowRuntimeError("null device");
		}
		return g_device.get();
	}
}