#pragma once
#include "ShaderUtility.h"
#include "DX12DescriptorHeapHandle.h"
#include "Programs/Helper.h"
#include "ThirdParty/FreesyncHDR.h"

namespace ElysiaCore
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
	class DX12RootParameter;
	class DX12RootSignature;
	class DX12PipelineState;
	class DX12Shader;
	class ShaderKeywordSpace;
}

namespace ElysiaCore
{
	struct ContextSubmissionResult
	{
		UINT frameID = 0;
		UINT submissionIndex = 0;
	};

	using namespace CAULDRON_DX12;
	using namespace ElysiaHelper;

	class DX12Device
	{
	public:
		std::vector<UINT> m_freeReservedDescriptorIndices;
		std::unique_ptr<DX12StagingDescriptorHeap> m_RTVStagingDescriptorHeap;
		std::unique_ptr<DX12StagingDescriptorHeap> m_DSVStagingDescriptorHeap;
		std::unique_ptr<DX12StagingDescriptorHeap> m_SRVStagingDescriptorHeap;
		std::array<std::unique_ptr<DX12RenderPassDescriptorHeap>, NUM_FRAMES_IN_FLIGHT> m_SRVRenderPassDescriptorHeaps;
		std::unique_ptr<DX12RenderPassDescriptorHeap> m_samplerRenderPassDescriptorHeap;
		std::array<DX12DescriptorHeapHandle, NUM_FRAMES_IN_FLIGHT> m_ImguiDescriptors;

	public:
		DX12Device();
		~DX12Device();
		void OnCreate(std::wstring appName, bool bCPUValidationEnabled, bool bGpuValidationEnabled);
		void OnDestroy();

		UINT GetFrameID() const noexcept {return m_frameID;} 
		ID3D12Device*			GetDevice()
		{
			return m_pDevice;
		}
		IDXGIAdapter*			GetAdapter() const noexcept
		{
			assert(m_pAdapter != nullptr);
			return m_pAdapter;
		}
		DX12StagingDescriptorHeap* GetSRVStageHeap() const noexcept
		{
			return m_SRVStagingDescriptorHeap.get();
		}
		DX12StagingDescriptorHeap* GetRTVStageHeap() const noexcept
		{
			return m_RTVStagingDescriptorHeap.get();
		}
		DX12StagingDescriptorHeap* GetDSVStageHeap() const noexcept
		{
			return m_DSVStagingDescriptorHeap.get();
		}
		DX12RenderPassDescriptorHeap& GetSRVRenderHeap(UINT frameIndex)
		{
			return *m_SRVRenderPassDescriptorHeaps[frameIndex];
		}
		DX12RenderPassDescriptorHeap& GetSamplerHeap() const noexcept
		{
			return *m_samplerRenderPassDescriptorHeap;
		}
		DX12DescriptorHeapHandle& GetImguiDescriptor(uint32_t index) { return m_ImguiDescriptors[index]; }
		DX12UploadContext* GetUploadContext() const noexcept{ return m_uploadContexts[m_frameID].get(); }
		AGSContext* GetAGSContext() { return m_agsContext; }
		AGSGPUInfo* GetAGSGPUInfo() { return &m_agsGPUInfo; }
		ID3D12CommandQueue* GetDirectQueue() const noexcept;

		std::unique_ptr<DX12GraphicsContext>		CreateGraphicsContext();
		std::unique_ptr<DX12Shader>					CreateShader(ShaderCreateDesc& shaderCreateDesc);
		void										CreateSamplers(D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL);
		void										CreateRootParameters(DX12RootSignature* rootSignature, std::vector<DX12RootParameter*>& rootParamters);
		DX12RootSignature*							CreateRootSignature(const PipelineResourceLayout& resourceLayout, PipelineResourceMapping& resourceMapping);

		void DestoryBuffer(std::unique_ptr<DX12BufferResource> buffer, UINT frameID);
		void DestoryPipelineState(std::unique_ptr<DX12PipelineState> pipelineState, UINT frameID);
		void DestoryContext(std::unique_ptr<DX12Context> context, UINT frameID);
		void DestoryTexture(std::unique_ptr<DX12TextureResource> texture, UINT frameID);

		void CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
			uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType);
		void CopyDescriptorFromStageToRenderPass(DX12DescriptorHeapHandle SRVHandle, UINT index);
		ContextSubmissionResult SubmitContextWork(DX12Context& context);

		void WaitForIdle();

		void BeginFrame(UINT frameID);
		void EndFrame();
		void Present();

		void GetDeviceInfo(std::string *deviceGPUName, std::string *driverVersion);
		bool IsFp16Supported() { return m_fp16Supported; }
		bool IsRT10Supported() { return m_rt10Supported; }
		bool IsRT11Supported() { return m_rt11Supported; }
		bool IsVRSTier1Supported() { return m_vrs1Supported; }
		bool IsVRSTier2Supported() { return m_vrs2Supported; }
		bool IsBarycentricsSupported() { return m_barycentricsSupported; }
	private:
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

		void InitializeDeviceResources();
		void ProcessDestruction(UINT frameIndex);

		ShaderReflectionData ReflectShaderStage(CComPtr<IDxcResult> pResults, CComPtr<IDxcUtils> pUtils);
		ShaderBytecode CompileShaderStage(
			const std::wstring& path,
			const std::wstring& entry,
			const std::wstring& target,
			const std::vector<LPCWSTR>&,
			const DxcBuffer& sourceBuffer);
		ShaderVariantData CompileVariantAllStages(
			const ShaderCompileOptions& baseOptions,
			const ShaderCreateDesc& desc,
			const DxcBuffer& source,
			const ShaderKeywordSet& keywordSet,
			const ShaderKeywordSpace* keywordSpace);

		HWND m_hWnd;
		ElysiaHelper::UINT2 m_screenSize = Vector2::Zero;
		UINT m_frameID = 0;
		
		AGSContext* m_agsContext = nullptr;
		AGSGPUInfo  m_agsGPUInfo = {};
		BOOL m_bTearingSupport = false;
		DisplayMode m_displayMode = DISPLAYMODE_SDR;
		bool m_bVSyncOn = false;

		bool                  m_fp16Supported = false;
		bool                  m_rt10Supported = false;
		bool                  m_rt11Supported = false;
		bool                  m_vrs1Supported = false;
		bool                  m_vrs2Supported = false;
		bool                  m_barycentricsSupported = false;

		ID3D12Device* m_pDevice = nullptr;
		IDXGIAdapter* m_pAdapter = nullptr;
		std::unique_ptr<DX12Queue> m_graphicsQueue;
		std::unique_ptr<DX12Queue> m_computeQueue;
		std::unique_ptr<DX12Queue> m_copyQueue;

		std::array<EndOfFrameFences, NUM_FRAMES_IN_FLIGHT> m_endOfFrameFences;
		std::array<std::unique_ptr<DX12UploadContext>, NUM_FRAMES_IN_FLIGHT> m_uploadContexts;
		std::array<std::vector<std::pair<uint64_t, D3D12_COMMAND_LIST_TYPE>>, NUM_FRAMES_IN_FLIGHT> m_contextSubmissions;
		std::array<DestructionQueue, NUM_FRAMES_IN_FLIGHT> m_destructionQueues;
	};
}