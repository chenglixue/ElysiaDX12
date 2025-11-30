#include "stdafx.h"
#include "DX12Device.h"

#include "D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "DX12GraphicsContext.h"
#include "DX12UploadContext.h"
#include "DX12BufferResource.h"
#include "DX12TextureBuffer.h"
#include "DX12Context.h"
#include "DX12StagingDescriptorHeap.h"
#include "DX12Queue.h"
#include "DX12Shader.h"
#include "AMD/LPM/FreesyncHDR.h"
#include "AMD\libs\AGS\amd_ags.h"

// #include "src/Parameter/UserData.h"
#include "lib/Event/Messager.h"
#include "../Utility/RenderTexture.h"
#include "Utility/ShaderCompileOptions.h"
#include "src/Manager/ShaderVariantManager.h"

namespace ElysiaRenderer
{

	std::unique_ptr<DX12Device> g_device = nullptr;

	DX12Device::DX12Device(HWND windowHandle, ElysiaHelper::UINT2 screenSize)
		:	m_screenSize(screenSize),
			m_hWnd(windowHandle)
	{
		InitializeDeviceResources(windowHandle);
		CreateWindowDependentResources();
	}
	   
	DX12Device::~DX12Device()
	{
		WaitForIdle();

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			DestoryBuffer(std::unique_ptr<DX12BufferResource>(std::move(m_uploadContexts[i]->GetTexUploadHeap())));
		}

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			ProcessDestruction(i);
		}

		
		m_RTVStagingDescriptorHeap = nullptr;
		m_DSVStagingDescriptorHeap = nullptr;
		m_CBVRenderPassDescriptorHeap = nullptr;
		m_samplerRenderPassDescriptorHeap = nullptr;
		m_UAVRenderPassDescriptorHeap = nullptr;

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			m_SRVRenderPassDescriptorHeaps[i] = nullptr;
			m_uploadContexts[i] = nullptr;
		}

		m_graphicsQueue = nullptr;
		m_computeQueue = nullptr;
		m_copyQueue = nullptr;

		ElysiaHelper::SafeRelease(m_device);
		ElysiaHelper::SafeRelease(m_DXGIFactory);
		ElysiaHelper::SafeRelease(m_swapChain);
		ElysiaHelper::SafeRelease(m_allocator);

#ifdef DEBUG
		IDXGIDebug1* pDebug = nullptr;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
		{
			pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
			ElysiaHelper::SafeRelease(pDebug);
		}
#endif // DEBUG

	}

	void DX12Device::InitializeDeviceResources(HWND windowHandle)
	{
		// Enable Debug
		{
			// ����debugģʽ�¿��ã����Ի�ȡ����ĵ�����Ϣ�ʹ��󱨸�
		// �����ڴ���D3D12 Deviceǰ���õ��Բ㣬���ú����ֱ��ɾ��(��Ϊ����D3D12 Device�󣬵��ø�API����runtime�Զ�ɾ��Device)
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nf-d3d12sdklayers-id3d12debug-enabledebuglayer
#if defined(_DEBUG)
			ID3D12Debug* debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				ElysiaHelper::SafeRelease(debugController);
			}
#endif
		}

		// Create DXGIFactory1
		{
			ElysiaHelper::AssertIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_DXGIFactory)));
		}

		// Get Adapter & Create Device & Create Allocator
		{
			// Create Adapter
			m_adapter = nullptr;
			UINT bestAdapterIndex = 0;
			size_t bestAdapterMemory = 0;	// ��¼���ר���Դ�
			for (UINT currAdapterIndex = 0; 
				m_DXGIFactory->EnumAdapters1(currAdapterIndex, &m_adapter) != DXGI_ERROR_NOT_FOUND;
				currAdapterIndex++)
			{
				DXGI_ADAPTER_DESC1 adapterDesc;
				ElysiaHelper::AssertIfFailed(m_adapter->GetDesc1(&adapterDesc));

				// soft ware adapter
				if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				DXGI_ADAPTER_DESC AdapterDesc;
				m_adapter->GetDesc(&AdapterDesc);
				const bool bAMDGPU = (AdapterDesc.VendorId == 0x1002);

				if (bAMDGPU)
				{
					AGSReturnCode result = agsInitialize(AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH), nullptr, &m_agsContext, &m_agsGPUInfo);
					if (result == AGS_SUCCESS)
					{
						AGSDX12DeviceCreationParams creationParams = {};
						creationParams.pAdapter = m_adapter;
						creationParams.iid = __uuidof(m_device);
						creationParams.FeatureLevel = D3D_FEATURE_LEVEL_12_0;

						AGSDX12ExtensionParams extensionParams = {};
						AGSDX12ReturnedParams returnedParams = {};

						// Create AGS Device
						//
						AGSReturnCode rc = agsDriverExtensionsDX12_CreateDevice(m_agsContext, &creationParams, &extensionParams, &returnedParams);
						if (rc == AGS_SUCCESS)
						{
							m_device = dynamic_cast<ID3D12Device5*>(returnedParams.pDevice);
						}
						else
						{
							Trace("Warning: AGS CreateDevice() failed w/ code=%d", rc);
						}
					}
					else
					{
						Trace("Warning: agsInitialize() failed w/ code=%d", result);
					}
				}

				// check support D3D12
				if (FAILED(D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_2, _uuidof(ID3D12Device), nullptr)))
				{
					continue;
				}

				// DedicatedVideoMemory:�Կ��Դ��ĸ����Դ�
				// DedicatedSystemMemory:ϵͳ�ڴ��л��ָ��Կ�ר�õĲ���
				if (adapterDesc.DedicatedVideoMemory > bestAdapterMemory)
				{
					bestAdapterIndex = currAdapterIndex;
					bestAdapterMemory = adapterDesc.DedicatedVideoMemory;
				}

				ElysiaHelper::SafeRelease(m_adapter);
			}

			if (bestAdapterMemory <= 0)
			{
				ElysiaHelper::AssertError("Failed to find an adapter.");
			}

			m_DXGIFactory->EnumAdapters1(bestAdapterIndex, &m_adapter);

			// Create Device
			ElysiaHelper::AssertIfFailed(D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)));

			// Create Allocator
			{
				D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
				allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
				allocatorDesc.pAdapter = m_adapter;
				allocatorDesc.pDevice = m_device;

				D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);
			}
		}

		CAULDRON_DX12::fsHdrInit(GetAGSContext(), GetAGSGPUInfo(), m_hWnd, m_adapter);
		m_format = fsHdrGetFormat(DISPLAYMODE_SDR);

		// Create Queue
		{
			m_graphicsQueue = std::make_unique<DX12Queue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
			m_computeQueue = std::make_unique<DX12Queue>(m_device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
			m_copyQueue = std::make_unique<DX12Queue>(m_device, D3D12_COMMAND_LIST_TYPE_COPY);
		}

		// Create Descriptor Heap
		{
			m_RTVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				NUM_RTV_STAGING_DESCRIPTORS);
			m_SRVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				NUM_SRV_STAGING_DESCRIPTORS);
			m_DSVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				NUM_DSV_STAGING_DESCRIPTORS);

			for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
			{
				m_SRVRenderPassDescriptorHeaps[currFrameIndex] = std::make_unique<DX12RenderPassDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
					NUM_RESERVED_SRV_DESCRIPTORS, NUM_SRV_RENDER_PASS_USER_DESCRIPTORS);

				m_ImguiDescriptors[currFrameIndex] = m_SRVRenderPassDescriptorHeaps[currFrameIndex]->GetReservedDescriptor(IMGUI_RESERVED_DESCRIPTOR_INDEX);
			}
			
			m_samplerRenderPassDescriptorHeap = std::make_unique<DX12RenderPassDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
				0, NUM_SAMPLER_DESCRIPTORS);
		}

		// Create Swap Chain
		{
			m_descSwapChain;
			m_descSwapChain.Width = lround(m_screenSize.x);
			m_descSwapChain.Height = lround(m_screenSize.y);
			m_descSwapChain.Format = m_format;
			m_descSwapChain.Stereo = false;
			m_descSwapChain.SampleDesc.Count = 1;
			m_descSwapChain.SampleDesc.Quality = 0;
			m_descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			m_descSwapChain.BufferCount = NUM_BACK_BUFFERS;
			m_descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			m_descSwapChain.Flags = 0;
			m_descSwapChain.Scaling = DXGI_SCALING_NONE;
			m_descSwapChain.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

			ThrowIfFailed(m_DXGIFactory->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING, &m_bTearingSupport, sizeof(m_bTearingSupport)));

			m_descSwapChain.Flags = m_bTearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

			IDXGISwapChain1* swapChain;
			ElysiaHelper::AssertIfFailed(m_DXGIFactory->CreateSwapChainForHwnd(m_graphicsQueue->GetCommandQueue(), windowHandle, &m_descSwapChain, nullptr, nullptr, &swapChain));
			ThrowIfFailed(m_DXGIFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));
			ElysiaHelper::AssertIfFailed(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_swapChain));
			ElysiaHelper::SafeRelease(swapChain);
		}

		// Create Upload Context
		{
			BufferCreationDesc uploadBufferDesc{};
			uploadBufferDesc.m_accessFlags = BufferAccessFlags::HostWritable;
			uploadBufferDesc.m_size = 40 * 4096 * 4096;

			BufferCreationDesc uploadTextureDesc{};
			uploadTextureDesc.m_accessFlags = BufferAccessFlags::HostWritable;
			uploadTextureDesc.m_size = 40 * 4096 * 4096;

			for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
			{
				m_uploadContexts[currFrameIndex] = std::make_unique<DX12UploadContext>(
					this, 
					CreateBuffer(uploadBufferDesc),
					CreateBuffer(uploadTextureDesc));
			}
		}

		CreateSamplers();


		/*for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			m_destructionQueues[i].m_buffers = std::make_unique<std::vector<DX12BufferResource>>();
			m_destructionQueues[i].m_textures = std::make_unique<std::vector<DX12TextureResource>>();
			m_destructionQueues[i].m_contexts = std::make_unique<std::vector<DX12Context>>();
			m_destructionQueues[i].m_graphicsPipelineStates = std::make_unique<std::vector<DX12PipelineState>>();
		}*/
		m_frameID = 0;
		m_frameIndex = 0;
		m_freeReservedDescriptorIndices.resize(NUM_RESERVED_SRV_DESCRIPTORS - 1);
		std::iota(m_freeReservedDescriptorIndices.begin(), m_freeReservedDescriptorIndices.end(), 1);
		if (m_displayMode == CAULDRON_DX12::DisplayMode::DISPLAYMODE_SDR)
		{
			m_format = ConvertIntoGammaFormat(m_format);
		}
	}
	void DX12Device::CreateWindowDependentResources()
	{
		// Create Render Target
		{
			for (UINT currBufferIndex = 0; currBufferIndex < NUM_BACK_BUFFERS; currBufferIndex++)
			{
				auto currBackBufferRTVHandle = m_RTVStagingDescriptorHeap->NewDescriptorHeapHandle();

				ID3D12Resource* backBufferResource = nullptr;
				ElysiaHelper::AssertIfFailed(m_swapChain->GetBuffer(currBufferIndex, IID_PPV_ARGS(&backBufferResource)));
				backBufferResource->SetName(L"Camera Color Buffer");

				D3D12_RENDER_TARGET_VIEW_DESC RTVDecs = {};
				RTVDecs.Format = m_format;
				RTVDecs.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				RTVDecs.Texture2D.MipSlice = 0;
				RTVDecs.Texture2D.PlaneSlice = 0;
				m_device->CreateRenderTargetView(backBufferResource, &RTVDecs, currBackBufferRTVHandle.GetCPUHandle());

				m_backBuffers[currBufferIndex] = std::make_unique<DX12TextureResource>(
					backBufferResource, D3D12_RESOURCE_STATE_PRESENT);
				m_backBuffers[currBufferIndex]->SetResourceDesc(backBufferResource->GetDesc());
				m_backBuffers[currBufferIndex]->SetRTVDescriptor(currBackBufferRTVHandle);
				backBufferResource->Release();
			}
		}
	}
	void DX12Device::OnCreateWindowSizeDependentResources(uint32_t dwWidth, uint32_t dwHeight, bool bVSyncOn, DisplayMode displayMode, bool disableLocalDimming)
	{
		// check whether the requested mode is supported and fall back to SDR if not supported
		bool bIsModeSupported = IsModeSupported(displayMode);
		if (bIsModeSupported == false)
		{
			//assert(!"FS HDR display mode not supported");
			displayMode = DISPLAYMODE_SDR;
		}

		if ((displayMode == CAULDRON_DX12::DisplayMode::DISPLAYMODE_HDR10_2084 ||
				displayMode == CAULDRON_DX12::DisplayMode::DISPLAYMODE_HDR10_SCRGB)
				&&
				(displayMode == CAULDRON_DX12::DisplayMode::DISPLAYMODE_FSHDR_Gamma22 ||
					displayMode == CAULDRON_DX12::DisplayMode::DISPLAYMODE_FSHDR_SCRGB))
		{
			ThrowIfFailed(
				m_swapChain->ResizeBuffers(
					NUM_BACK_BUFFERS,
					lround(m_screenSize.x),
					lround(m_screenSize.y),
					DXGI_FORMAT_B8G8R8A8_UNORM,
					m_descSwapChain.Flags)
			);

			ThrowIfFailed(m_swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
		}

		m_displayMode = displayMode;
		m_format = fsHdrGetFormat(displayMode);
		m_bVSyncOn = bVSyncOn;

		for (size_t i = 0; i < m_backBuffers.size(); ++i)
		{
			m_backBuffers[i]->GetResource().Release();
		}

		 ThrowIfFailed(
		 		m_swapChain->ResizeBuffers(
		 			NUM_BACK_BUFFERS,
		 			lround(m_screenSize.x),
		 			lround(m_screenSize.y),
		 			m_format,
		 			m_descSwapChain.Flags)
		 	);
		fsHdrSetDisplayMode(displayMode, disableLocalDimming, m_swapChain);

		// if SDR, convert add gamma for the swapchain format so blending is correct
		if (m_displayMode == DISPLAYMODE_SDR)
		{
			m_format = ConvertIntoGammaFormat(m_format);
		}

		CreateWindowDependentResources();
	}

	std::unique_ptr<DX12GraphicsContext>		DX12Device::CreateGraphicsContext()
	{
		auto graphicsContext = std::make_unique<DX12GraphicsContext>(this);

		return graphicsContext;
	}
	std::unique_ptr<DX12BufferResource>			DX12Device::CreateBuffer(const BufferCreationDesc& bufferCreationDesc)
	{
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Width = AlignU32(static_cast<UINT>(bufferCreationDesc.m_size), 256);
		resourceDesc.Height = 1;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		UINT numElements = static_cast<UINT>(bufferCreationDesc.m_stride > 0 ? bufferCreationDesc.m_size / bufferCreationDesc.m_stride : 1);
		bool isHostVisible = ((bufferCreationDesc.m_accessFlags & BufferAccessFlags::HostWritable) == BufferAccessFlags::HostWritable);
		bool isHasCBV = ((bufferCreationDesc.m_viewFlags & GPUResourceFlags::CBV) == GPUResourceFlags::CBV);
		bool isHasSRV = ((bufferCreationDesc.m_viewFlags & GPUResourceFlags::SRV) == GPUResourceFlags::SRV);
		bool isHasUAV = ((bufferCreationDesc.m_viewFlags & GPUResourceFlags::UAV) == GPUResourceFlags::UAV);

		D3D12_RESOURCE_STATES usageState = isHostVisible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
		
		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = isHostVisible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

		CComPtr<D3D12MA::Allocation> pAllocation = nullptr;
		CComPtr<ID3D12Resource> pResource = nullptr;
		ElysiaHelper::ThrowIfFailed(m_allocator->CreateResource(&allocationDesc, &resourceDesc, usageState, nullptr,
			&pAllocation, IID_PPV_ARGS(&pResource)));
		//pResource->SetName(bufferCreationDesc.m_name);
		 
		auto pNewBuffer = std::make_unique<DX12BufferResource>(pResource, usageState, pAllocation);

		if (isHasCBV)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc{};
			CBVDesc.SizeInBytes = static_cast<UINT>(pNewBuffer->GetResourceDesc().Width);
			CBVDesc.BufferLocation = pNewBuffer->GetGPUAddress();

			pNewBuffer->SetCBVDescriptor(m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle());
			m_device->CreateConstantBufferView(&CBVDesc, pNewBuffer->GetCBVDescriptor().GetCPUHandle());
		}

		if (isHasSRV)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SRVDesc.Format = bufferCreationDesc.m_isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = static_cast<UINT>(bufferCreationDesc.m_isRawAccess ? bufferCreationDesc.m_size / 4 : numElements);
			SRVDesc.Buffer.StructureByteStride = bufferCreationDesc.m_isRawAccess ? 0 : static_cast<UINT>(pNewBuffer->GetStride());
			SRVDesc.Buffer.Flags = bufferCreationDesc.m_isRawAccess ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

			pNewBuffer->SetSRVDescriptor(m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle());
			m_device->CreateShaderResourceView(pNewBuffer->GetResource(), &SRVDesc, pNewBuffer->GetSRVDescriptor().GetCPUHandle());

			pNewBuffer->SetResourceHeapIndex(m_freeReservedDescriptorIndices.back());
			m_freeReservedDescriptorIndices.pop_back();

			CopyDescriptorFromStageToRenderPass(pNewBuffer->GetSRVDescriptor(), pNewBuffer->GetResourceHeapIndex());
		}

		if (isHasUAV)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};

			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			UAVDesc.Format = bufferCreationDesc.m_isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			UAVDesc.Buffer.CounterOffsetInBytes = 0;
			UAVDesc.Buffer.FirstElement = 0;
			UAVDesc.Buffer.NumElements = static_cast<UINT>(bufferCreationDesc.m_isRawAccess ? bufferCreationDesc.m_size / 4 : numElements);
			UAVDesc.Buffer.StructureByteStride = bufferCreationDesc.m_isRawAccess ? 0 : static_cast<UINT>(pNewBuffer->GetStride());
			UAVDesc.Buffer.Flags = bufferCreationDesc.m_isRawAccess ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

			pNewBuffer->SetUAVDescriptor(m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle());

			m_device->CreateUnorderedAccessView(pNewBuffer->GetResource(), nullptr, &UAVDesc, pNewBuffer->GetUAVDescriptor().GetCPUHandle());
		}

		if (isHostVisible)
		{
			pNewBuffer->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&pNewBuffer->m_mappedBuffer));
		}

		return pNewBuffer;
	}
	
	std::unique_ptr<DX12TextureResource>		DX12Device::CreateTextureFromFile(const TextureCreationDesc& textureCreationDesc)
	{
		auto& texturePath = textureCreationDesc.texturePath;
		bool isSRGB = textureCreationDesc.isSRGB;

		const std::wstring extension = GetFileExtension(texturePath.c_str());
		/// Load DDS
		std::unique_ptr<DirectX::ScratchImage> imageData = std::make_unique<DirectX::ScratchImage>();
		if(extension == L"DDS" || extension == L"dds")
		{
			auto s2ws = [](const std::string& s)
			{
				//yoink https://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode
				int32_t len = 0;
				int32_t slength = (int32_t)s.length() + 1;
				len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
				wchar_t* buf = new wchar_t[len];
				MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
				std::wstring r(buf);
				delete[] buf;
				return r;
			};

			WCHAR assetsPath[512];
			ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

			imageData = std::make_unique<DirectX::ScratchImage>();
			auto loadResult = DirectX::LoadFromDDSFile((assetsPath + texturePath).c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData);
			if (loadResult != S_OK)
			{
				std::cout << WstringToString(textureCreationDesc.texturePath) + " not found" << std::endl;
				return nullptr;
			}
		}
		else
		{
			DirectX::ScratchImage tempImage;
			auto loadResult = DirectX::LoadFromWICFile(texturePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, tempImage);
			if (loadResult != S_OK)
			{
				std::cout << WstringToString(textureCreationDesc.texturePath) + " not found" << std::endl;
				return nullptr;
			}
			ThrowIfFailed(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, *imageData, false));
		}
		///

		/// grad tex data
		///
		const auto& texMetaData = imageData->GetMetadata();
		auto texFormat = isSRGB ? DirectX::MakeSRGB(texMetaData.format) : texMetaData.format;
		bool is3DTex = texMetaData.dimension == DirectX::TEX_DIMENSION_TEXTURE3D;
		///
		
		/// Create tex desc && tex resource
		D3D12_RESOURCE_DESC texDesc{};
		texDesc.Width = texMetaData.width;
		texDesc.Height = static_cast<UINT>(texMetaData.height);
		texDesc.Dimension = is3DTex ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Format = texFormat;
		texDesc.MipLevels = static_cast<UINT16>(texMetaData.mipLevels);
		texDesc.Alignment = 0;
		texDesc.DepthOrArraySize = static_cast<UINT16>(is3DTex ? texMetaData.depth : texMetaData.arraySize);
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		TexCreateDesc createDesc{};
		createDesc.m_resouceDesc = std::move(texDesc);
		createDesc.m_typeFlag = TexTypeFlags::SRV;

		auto newTex = std::move(CreateTexture(createDesc));
		///

		// ÿ��Mipͼ�൱��һ������Դ
		auto textureUpload = std::make_unique<DX12TextureUpload>();
		textureUpload->m_textureBuffer = newTex.get();
		textureUpload->m_numSubResources = static_cast<UINT>(texMetaData.mipLevels * texMetaData.arraySize);

		UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];	// ÿ������Դ������
		uint64_t rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];

		auto resourceDesc = textureUpload->m_textureBuffer->GetResourceDesc();
		m_device->GetCopyableFootprints(&resourceDesc, 0, textureUpload->m_numSubResources, 0,
			textureUpload->m_subResourceLayouts.data(), numRows, rowSizesInBytes, &textureUpload->m_textureDataSize);
		
		textureUpload->m_pTextureData = std::make_unique<uint8_t[]>(textureUpload->m_textureDataSize);

		for (size_t arrayIndex = 0; arrayIndex < texMetaData.arraySize; ++arrayIndex)
		{
			for (size_t mipIndex = 0; mipIndex < texMetaData.mipLevels; ++mipIndex)
			{
				const uint64_t subResourceIndex = mipIndex + (arrayIndex * texMetaData.mipLevels);

				const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourcelayout = textureUpload->m_subResourceLayouts[subResourceIndex];
				const uint64_t subResourceHeight = numRows[subResourceIndex];
				// ÿ�����ݵ��ֽ���
				const uint64_t subResourcePitch = ElysiaHelper::AlignU32(subResourcelayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
				const uint64_t subResourceDepth = subResourcelayout.Footprint.Depth;
				uint8_t* destSubResourceMemory = textureUpload->m_pTextureData.get() + subResourcelayout.Offset;

				// sliceIndex��3D�������Ƭ������2D�������Ƭ����Ϊ0
				for (uint64_t sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++)
				{
					const auto subImage = imageData->GetImage(mipIndex, arrayIndex, sliceIndex);
					const uint8_t* sourceSubResourceMemory = subImage->pixels;
					// ����ͼƬÿ������
					for (uint64_t height = 0; height < subResourceHeight; ++height)
					{
						memcpy(destSubResourceMemory, sourceSubResourceMemory, (std::min)(subResourcePitch, subImage->rowPitch));
						destSubResourceMemory += subResourcePitch;
						sourceSubResourceMemory += subImage->rowPitch;
					}
				}
			}
		}

		m_uploadContexts[m_frameID]->AddTextureToUploads(std::move(textureUpload));

		return newTex;
	}
	std::unique_ptr<DX12TextureResource>		DX12Device::CreateTexture(const TexCreateDesc& desc)
	{
		auto resourceDesc = desc.m_resouceDesc;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		auto typeFlag = desc.m_typeFlag;

		bool hasRTV = (typeFlag & TexTypeFlags::RTV) == TexTypeFlags::RTV;
		bool hasSRV = (typeFlag & TexTypeFlags::SRV) == TexTypeFlags::SRV;
		bool hasDSV = (typeFlag & TexTypeFlags::DSV) == TexTypeFlags::DSV;
		bool hasUAV = (typeFlag & TexTypeFlags::UAV) == TexTypeFlags::UAV;

		DXGI_FORMAT resourceFormat = resourceDesc.Format;
		DXGI_FORMAT shaderResourceViewFormat = resourceDesc.Format;
		//D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_COPY_DEST;
		D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_COMMON;

		if (hasRTV)
		{
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			usageState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		if (hasDSV)
		{
			switch(desc.m_resouceDesc.Format)
			{
				case DXGI_FORMAT_D16_UNORM:
				{
					resourceFormat = DXGI_FORMAT_R16_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R16_UNORM;
					break;
				}
				case DXGI_FORMAT_D24_UNORM_S8_UINT:
				{
					resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
					break;
				}
				case DXGI_FORMAT_D32_FLOAT:
				{
					resourceFormat = DXGI_FORMAT_R32_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT;
					break;
				}
				case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
				{
					resourceFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
					shaderResourceViewFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
					break;
				}
				default:
				{
					ElysiaHelper::AssertError("Bad depth stencil format.");
					break;
				}
			}

			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			//usageState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			usageState = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}

		if (hasUAV)
		{
			resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			usageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		resourceDesc.Format = resourceFormat;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = desc.m_resouceDesc.Format;
		if (hasDSV)
		{
			clearValue.DepthStencil.Depth = 1.0f;
			clearValue.DepthStencil.Stencil = 0;
		}
		if (hasRTV)
		{
			float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
			memcpy(clearValue.Color, clearColor, sizeof(clearValue.Color));
		}

		/// Create default heap for tex
		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		CComPtr<ID3D12Resource> texResource = nullptr;
		CComPtr<D3D12MA::Allocation> allocation = nullptr;
		ElysiaHelper::ThrowIfFailed(m_allocator->CreateResource(&allocationDesc, &resourceDesc, usageState, (!hasRTV && !hasDSV) ? nullptr : &clearValue,
			&allocation, IID_PPV_ARGS(&texResource)));
		texResource->SetName(desc.m_name.c_str());
		/// 

		auto newTex = std::make_unique<DX12TextureResource>(texResource, usageState, allocation);

		if (hasSRV)
		{
			auto SRVHandle = m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle();

			if (hasDSV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC SRV{};
				SRV.Format = shaderResourceViewFormat;
				SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				SRV.Texture2D.MostDetailedMip = 0;
				SRV.Texture2D.MipLevels = 1;
				SRV.Texture2D.ResourceMinLODClamp = 0;
				SRV.Texture2D.PlaneSlice = 0;

				m_device->CreateShaderResourceView(newTex->GetResource(), &SRV, SRVHandle.GetCPUHandle());
			}
			else
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC* srvDescPointer = nullptr;
				D3D12_SHADER_RESOURCE_VIEW_DESC SRV = {};

				bool isCubeMap = resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && resourceDesc.DepthOrArraySize == 6;
				if (isCubeMap)
				{
					SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					SRV.TextureCube.MostDetailedMip = 0;
					SRV.TextureCube.MipLevels = (UINT)resourceDesc.MipLevels;
					SRV.TextureCube.ResourceMinLODClamp = 0;
					srvDescPointer = &SRV;
				}

				m_device->CreateShaderResourceView(newTex->GetResource(), srvDescPointer, SRVHandle.GetCPUHandle());
			}
			
			///
			newTex->SetSRVDescriptor(SRVHandle);
			newTex->SetResourceHeapIndex(m_freeReservedDescriptorIndices.back());
			m_freeReservedDescriptorIndices.pop_back();

			CopyDescriptorFromStageToRenderPass(newTex->GetSRVDescriptor(), newTex->GetResourceHeapIndex());
		}

		if (hasRTV)
		{
			auto RTVHandle = m_RTVStagingDescriptorHeap->NewDescriptorHeapHandle();

			newTex->SetRTVDescriptor(RTVHandle);
			m_device->CreateRenderTargetView(newTex->GetResource(), nullptr, newTex->GetRTVDescriptor().GetCPUHandle());

		}

		if (hasDSV)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.Format = desc.m_resouceDesc.Format;
			dsvDesc.Texture2D.MipSlice = 0;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			auto newDSVHandle = m_DSVStagingDescriptorHeap->NewDescriptorHeapHandle();
			newTex->SetDSVDescriptor(newDSVHandle);
			m_device->CreateDepthStencilView(newTex->GetResource(), &dsvDesc, newTex->GetDSVDescriptor().GetCPUHandle());
		}

		if (hasUAV)
		{
			auto newUAVHandle = m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle();
			newTex->SetUAVDescriptor(newUAVHandle);
			m_device->CreateUnorderedAccessView(newTex->GetResource(), nullptr, nullptr, newTex->GetUAVDescriptor().GetCPUHandle());
		}

		newTex->SetIsReady(hasRTV || hasDSV);

		return newTex;
	}

	std::unique_ptr<DX12Shader>					DX12Device::CreateShader(ShaderCreateDesc& shaderCreateDesc, const std::vector<std::wstring>& enabledKeywords)
	{
		/// Enable Debug
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_STRICTNESS;
#else
		UINT compileFlags = 0;
#endif

		//
		// Get x64 path
		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

		ElysiaHelper::ShaderCompileOptions compileOptions;
		
#if defined(_DEBUG)
		compileOptions.EnableDebug(true);
		compileOptions.SetOptLevel(0);
#else
		compileOptions.EnableDebug(false);
		compileOptions.SetOptLevel(3);
#endif
		
		CComPtr<IDxcUtils> pUtils;
		CComPtr<IDxcCompiler3> pCompiler;
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));
		
		//
		// Open source file.  
		//
		CComPtr<IDxcBlobEncoding> pSource;
		auto path = ElysiaHelper::GetAssetFullPath(assetsPath, shaderCreateDesc.stages.begin()->ShaderName.c_str());
		ThrowIfFailed(pUtils->LoadFile(ElysiaHelper::GetAssetFullPath(assetsPath, shaderCreateDesc.stages.begin()->ShaderName.c_str()).c_str(), nullptr, &pSource));
		DxcBuffer Source
		{
			.Ptr = pSource->GetBufferPointer(),
			.Size = pSource->GetBufferSize(),
			.Encoding = DXC_CP_ACP	// Assume BOM says UTF8 or UTF16 or this is ANSI text.
		};

		std::wstring hlslWString = StringToWstring((const char*)Source.Ptr);
		auto pragmaInfo = ParseShaderPragmas(hlslWString);

		auto pKeywordSpace = std::make_unique<ShaderKeywordSpace>();
		for (auto& group : pragmaInfo.KeywordGroups)
		{
			for (auto& key : group.Keywords)
			{
				if (!key.empty())
				{
					pKeywordSpace->AddKeyword(key);
				}
			}
		}

		auto variantMgr = std::make_unique<ShaderVariantManager>(pKeywordSpace.get());
		variantMgr->SetCompileCallback(
			[&](const ShaderKeywordSet& set)
			{
				return CompileVariantAllStages(compileOptions, shaderCreateDesc, Source, set, pKeywordSpace.get());
			});

		auto variants = variantMgr->BuildAllVariants(pragmaInfo);
		variantMgr->InitializeFromCompiled(variants);

		

		auto o = std::make_unique<DX12Shader>(std::move(variantMgr));

		return o;
	}
	void										DX12Device::CreateSamplers(D3D12_SHADER_VISIBILITY shaderVisibility)
	{
		D3D12_SAMPLER_DESC samplerDescs[NUM_SAMPLER_DESCRIPTORS]{};
		for (size_t i = 0; i < NUM_SAMPLER_DESCRIPTORS; ++i)
		{
			samplerDescs[0].BorderColor[0] = samplerDescs[0].BorderColor[1] = samplerDescs[0].BorderColor[2] = samplerDescs[0].BorderColor[3] = 0.0f;
			samplerDescs[i].MipLODBias = 0;
			samplerDescs[i].MaxAnisotropy = 16;
			samplerDescs[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
			samplerDescs[i].MinLOD = 0;
			samplerDescs[i].MaxLOD = D3D12_FLOAT32_MAX;
		}
		UINT samplerIndex = 0;
		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		//rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		auto samplerDescriptorBlock = m_samplerRenderPassDescriptorHeap->AllocateRenderPassDescriptorBlock(NUM_SAMPLER_DESCRIPTORS);
		D3D12_CPU_DESCRIPTOR_HANDLE currentSamplerDescriptor = samplerDescriptorBlock.GetCPUHandle();

		for (uint32_t samplerIndex = 0; samplerIndex < NUM_SAMPLER_DESCRIPTORS; samplerIndex++)
		{
			m_device->CreateSampler(&samplerDescs[samplerIndex], currentSamplerDescriptor);
			currentSamplerDescriptor.ptr += m_samplerRenderPassDescriptorHeap->GetDescriptorSingleSize();
		}
	}
	void										DX12Device::CreateRootParameters(DX12RootSignature* rootSignature, std::vector<DX12RootParameter*>& rootParamters)
	{
		for (auto i = 0; i < rootParamters.size(); ++i)
		{
			(*rootSignature)[i] = *rootParamters[i];
		}
	}
	DX12RootSignature*							DX12Device::CreateRootSignature(const PipelineResourceLayout& resourceLayout, PipelineResourceMapping& resourceMapping)
	{
		std::vector<DX12RootParameter*> rootParameters{};
		std::array<std::vector<D3D12_DESCRIPTOR_RANGE1>, NUM_RESOURCE_SPACES> desciptorRanges;

		for (UINT currSpaceID = 0; currSpaceID < NUM_RESOURCE_SPACES; ++currSpaceID)
		{
			auto currSpace = resourceLayout.m_spaces[currSpaceID];
			std::vector<D3D12_DESCRIPTOR_RANGE1>& currDescriptorRange = desciptorRanges[currSpaceID];

			if (currSpace)
			{
				const auto CBV = currSpace->GetCBV();
				auto SRVs = currSpace->GetSRVs();
				auto UAVs = currSpace->GetUAVs();

				if (CBV)
				{
					DX12RootParameter* rootParameter = new DX12RootParameter();
					rootParameter->InitAsConstantBufferView(0, D3D12_SHADER_VISIBILITY_ALL, currSpaceID);

					resourceMapping.m_CBVMappings[currSpaceID] = static_cast<UINT>(rootParameters.size());
					rootParameters.emplace_back(std::move(rootParameter));
				}

				if (SRVs.empty() && UAVs.empty())
				{
					continue;
				}

				for (auto& uav : UAVs)
				{
					D3D12_DESCRIPTOR_RANGE1 range{};
					range.BaseShaderRegister = uav->m_bindingIndex;
					range.NumDescriptors = 1;
					range.OffsetInDescriptorsFromTableStart = static_cast<uint32_t>(currDescriptorRange.size());
					range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
					range.RegisterSpace = currSpaceID;
					range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

					currDescriptorRange.push_back(range);
				}

				// all of SRV Resource has one DESCRIPTOR RANGE which only has one descriptor
				for (auto& SRV : SRVs)
				{
					D3D12_DESCRIPTOR_RANGE1 pDescriptorRange{};
					pDescriptorRange.BaseShaderRegister = SRV->m_bindingIndex;
					pDescriptorRange.NumDescriptors = 1;
					pDescriptorRange.OffsetInDescriptorsFromTableStart = static_cast<UINT>(currDescriptorRange.size());
					pDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					pDescriptorRange.RegisterSpace = currSpaceID;
					pDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

					currDescriptorRange.emplace_back(pDescriptorRange);
				}

				DX12RootParameter* rootParameter = new DX12RootParameter();
				rootParameter->InitAsDescriptorTable(static_cast<UINT>(currDescriptorRange.size()), currDescriptorRange.data(), D3D12_SHADER_VISIBILITY_ALL);

				resourceMapping.m_TableMappings[currSpaceID] = static_cast<UINT>(rootParameters.size());
				rootParameters.emplace_back(std::move(rootParameter));
			}
		}

		UINT numRootParamter = static_cast<UINT>(rootParameters.size());
		//UINT numSampler = NUM_SAMPLER_DESCRIPTORS;
		UINT numSampler = 0;
		DX12RootSignature* rootSignature = new DX12RootSignature(numRootParamter, numSampler);


		CreateRootParameters(rootSignature, rootParameters);

		rootSignature->Init(m_device, 
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | 
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | 
			D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);
		
		return rootSignature;
	}

	void DX12Device::CopyDescriptors(uint32_t numDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* destDescriptorRangeStarts, const uint32_t* destDescriptorRangeSizes,
		uint32_t numSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* srcDescriptorRangeStarts, const uint32_t* srcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType)
	{
		m_device->CopyDescriptors(numDestDescriptorRanges, destDescriptorRangeStarts, destDescriptorRangeSizes, numSrcDescriptorRanges, srcDescriptorRangeStarts, srcDescriptorRangeSizes, descriptorType);
	}
	/// <summary>
	/// 
	/// </summary>
	/// <param name="SRVHandle"> stage SRV Handle in buffer or tex </param>
	/// <param name="index"> resource index in render pass heap </param> 
	void DX12Device::CopyDescriptorFromStageToRenderPass(DX12DescriptorHeapHandle SRVHandle, UINT index)
	{
		for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
		{
			auto targetDescriptor = m_SRVRenderPassDescriptorHeaps[currFrameIndex]->GetReservedDescriptor(index);
			m_device->CopyDescriptorsSimple(1, targetDescriptor.GetCPUHandle(), SRVHandle.GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}
	ContextSubmissionResult DX12Device::SubmitContextWork(DX12Context& context)
	{
		uint64_t fenceResult = 0;

		switch (context.GetContextType())
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			fenceResult = m_graphicsQueue->ExecuteCommandList(context.GetCommandList());
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			fenceResult = m_computeQueue->ExecuteCommandList(context.GetCommandList());
			break;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			fenceResult = m_copyQueue->ExecuteCommandList(context.GetCommandList());
			break;
		default:
			ElysiaHelper::AssertError("Unsupported submission type.");
		}

		ContextSubmissionResult submissionResult;
		submissionResult.frameID = m_frameID;
		submissionResult.submissionIndex = static_cast<UINT>(m_contextSubmissions[m_frameID].size());

		m_contextSubmissions[m_frameID].push_back(std::make_pair(fenceResult, context.GetContextType()));

		return submissionResult;
	}

	void DX12Device::DestoryContext(std::unique_ptr<DX12Context> context)
	{
		m_destructionQueues[m_frameID].m_contexts.push_back(std::move(context));
	}
	void DX12Device::DestoryBuffer(std::unique_ptr<DX12BufferResource> buffer)
	{
		m_destructionQueues[m_frameID].m_buffers.push_back(std::move(buffer));
	}
	void DX12Device::DestoryPipelineState(std::unique_ptr<DX12PipelineState> pipelineState)
	{
		m_destructionQueues[m_frameID].m_pipelineStates.push_back(std::move(pipelineState));
	}
	void DX12Device::DestoryShader(std::unique_ptr<DX12Shader> shader)
	{
		//ElysiaHelper::SafeRelease(shader->GetShader());
	}
	void DX12Device::DestoryTexture(std::unique_ptr<DX12TextureResource> texture)
	{
		m_destructionQueues[m_frameID].m_textures.push_back(std::move(texture));
	}

	void DX12Device::ProcessDestruction(UINT frameIndex)
	{
		auto& currFrameDestrctuionQueue = m_destructionQueues[frameIndex];

		(currFrameDestrctuionQueue.m_contexts).clear();
		(currFrameDestrctuionQueue.m_buffers).clear();
		(currFrameDestrctuionQueue.m_textures).clear();
		(currFrameDestrctuionQueue.m_pipelineStates).clear();
	}

	void DX12Device::BeginFrame()
	{
		m_frameIndex++;
		m_frameID = (m_frameID + 1) % NUM_FRAMES_IN_FLIGHT;

		// wait on fences from 2 frames ago
		m_graphicsQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_graphicsQueueFence);
		m_copyQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_copyQueueFence);
		m_computeQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_computeQueueFence);

		ProcessDestruction(m_frameID);

		m_uploadContexts[m_frameID]->ResolveProcessedUploads();
		m_uploadContexts[m_frameID]->Reset();

		m_contextSubmissions[m_frameID].clear();
	}

	void DX12Device::EndFrame()
	{
		m_uploadContexts[m_frameID]->ProcessUploads();
		SubmitContextWork(*m_uploadContexts[m_frameID]);

		m_endOfFrameFences[m_frameID].m_copyQueueFence = m_copyQueue->SingalFence();
		m_endOfFrameFences[m_frameID].m_computeQueueFence = m_computeQueue->SingalFence();
	}

	void DX12Device::Present()
	{
		m_swapChain->Present(0, 0);
		m_endOfFrameFences[m_frameID].m_graphicsQueueFence = m_graphicsQueue->SingalFence();
	}

	void DX12Device::WaitForIdle()
	{
		m_graphicsQueue->WaitForIdle();
		m_copyQueue->WaitForIdle();
		m_computeQueue->WaitForIdle();
	}

	bool DX12Device::IsModeSupported(DisplayMode displayMode)
	{
		std::vector<DisplayMode> displayModesAvailable;
		EnumerateDisplayModes(&displayModesAvailable);
		return  std::find(displayModesAvailable.begin(), displayModesAvailable.end(), displayMode) != displayModesAvailable.end();
	}
	
	void DX12Device::EnumerateDisplayModes(std::vector<CAULDRON_DX12::DisplayMode> *pModes, std::vector<const char *> *pNames)
	{
		fsHdrEnumerateDisplayModes(pModes);

		if (pNames != NULL)
		{
			pNames->clear();
			for (DisplayMode mode : *pModes)
				pNames->push_back(fsHdrGetDisplayModeString(mode));
		}
	}

	ShaderReflectionData DX12Device::ReflectShaderStage(CComPtr<IDxcResult> pResults, CComPtr<IDxcUtils> pUtils)
	{
		ShaderReflectionData o{};

		//
		// Get separate reflection.
		//
		CComPtr<IDxcBlob> pReflectionData;
		CComPtr< ID3D12ShaderReflection > pReflection;
		ThrowIfFailed(pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr));
		if (pReflectionData != nullptr)
		{
			// Optionally, save reflection blob for later here.

			// Create reflection interface.
			const DxcBuffer ReflectionData
			{
				.Ptr = pReflectionData->GetBufferPointer(),
				.Size = pReflectionData->GetBufferSize(),
				.Encoding = DXC_CP_ACP,
			};

			pUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

			// Use reflection interface here.
			D3D12_SHADER_DESC pShaderDesc{};
			pReflection->GetDesc(&pShaderDesc);
			
			// Set ConstantBuffer layout & constant buffer member
			{
				std::unordered_map<std::string, ShaderReflectionData::ShaderVariable> shaderVariables{};
				for (UINT i = 0; i < pShaderDesc.BoundResources; ++i)
				{
					D3D12_SHADER_INPUT_BIND_DESC resourceDesc{};
					pReflection->GetResourceBindingDesc(i, &resourceDesc);

					if (resourceDesc.Type == D3D_SIT_CBUFFER)
					{
						ID3D12ShaderReflectionConstantBuffer* pConstantBuffer = pReflection->GetConstantBufferByIndex(i);
						D3D12_SHADER_BUFFER_DESC constantBufferDesc{};
						pConstantBuffer->GetDesc(&constantBufferDesc);

						auto variableName = constantBufferDesc.Name;
						D3D_SHADER_INPUT_TYPE resourceType = resourceDesc.Type;
						auto spaceID = resourceDesc.Space;
						auto registerPos = resourceDesc.BindPoint;

						ShaderReflectionData::ShaderVariable temp 
						{
							.type = ShaderReflectionData::ShaderVariable::Type::ConstantBuffer,
							.registerPos = registerPos,
							.spaceID = spaceID,
							.name = variableName,
							.size = constantBufferDesc.Size
						};
						shaderVariables.insert({variableName, temp});

						for (UINT memberIndex = 0; memberIndex < constantBufferDesc.Variables; ++memberIndex)
						{
							auto memberVariable = pConstantBuffer->GetVariableByIndex(memberIndex);
							D3D12_SHADER_VARIABLE_DESC variableDesc{};
							memberVariable->GetDesc(&variableDesc);

							ShaderReflectionData::ShaderConstantVariableDesc constantVariableDesc{};
							constantVariableDesc.SpaceID = spaceID;
							constantVariableDesc.StartOffset = variableDesc.StartOffset;
							constantVariableDesc.Size = variableDesc.Size;
							//constantVariableDesc.pData = new char[constantVariableDesc.Size];
							constantVariableDesc.pData = std::vector<char>(constantVariableDesc.Size);

							shaderVariables[variableName].members.insert({variableDesc.Name, constantVariableDesc});
						}
					}
				}
				o.cbuffers = std::move(shaderVariables);
			}

			// Get Vertex layout
			{
				std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc(pShaderDesc.InputParameters);
				std::vector <std::string > inputElementSemanticNames{ pShaderDesc.InputParameters };

				for (UINT32 parameterIndex = 0; parameterIndex < pShaderDesc.InputParameters; ++parameterIndex)
				{
					D3D12_SIGNATURE_PARAMETER_DESC signatureParameterDesc{};
					pReflection->GetInputParameterDesc(parameterIndex, &signatureParameterDesc);

					inputElementSemanticNames[parameterIndex] = signatureParameterDesc.SemanticName;
				}
				o.InputElementSemanticNames = std::move(inputElementSemanticNames);

				for (UINT32 parameterIndex = 0; parameterIndex < pShaderDesc.InputParameters; ++parameterIndex)
				{
					D3D12_SIGNATURE_PARAMETER_DESC signatureParameterDesc{};
					pReflection->GetInputParameterDesc(parameterIndex, &signatureParameterDesc);

					inputElementDesc[parameterIndex] = D3D12_INPUT_ELEMENT_DESC
					{
						.SemanticName = o.InputElementSemanticNames[parameterIndex].c_str(),
						.SemanticIndex = signatureParameterDesc.SemanticIndex,
						.Format = MaskToFormat(signatureParameterDesc.Mask),
						.InputSlot = 0u,
						.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
						.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
										// There doesn't seem to be a obvious way to 
										// automate this currently, which might be a issue when instanced rendering is used
						.InstanceDataStepRate = 0u
					};
				}

				o.InputLayoutElementDescs = std::move(inputElementDesc);
				o.InputLayoutDesc = 
				{
					.pInputElementDescs = o.InputLayoutElementDescs.data(),
					.NumElements = static_cast<UINT32>(o.InputLayoutElementDescs.size()),
				};
			}
		}
	}
	
	const ShaderBytecode DX12Device::CompileShaderStage(
			const std::wstring& path,
			const std::wstring& entry,
			const std::wstring& target,
			const std::vector<LPCWSTR>& args,
			const DxcBuffer& sourceBuffer)
	{
		
		// 
		// Create compiler and utils.
		//
		CComPtr<IDxcUtils> pUtils;
		CComPtr<IDxcCompiler3> pCompiler;
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

		//
		// Create default include handler
		//
		CComPtr<IDxcIncludeHandler> pIncludeHandler;
		ThrowIfFailed(pUtils->CreateDefaultIncludeHandler(&pIncludeHandler));

		auto pszArgs = args;
		//
		// Compile it with specified arguments.
		//
		CComPtr<IDxcResult> pResults;
		auto hr = pCompiler->Compile(
			&sourceBuffer,                // Source buffer.
			pszArgs.data(),         // Array of pointers to arguments.
			(UINT)pszArgs.size(),      // Number of arguments.
			pIncludeHandler,        // User-provided interface to handle #include directives (optional).
			IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
		);
		if (FAILED(hr))
		{
			ThrowRuntimeError(std::string("Failed to compile shader with path : ") + WstringToString(path + entry));
		}

		//
		// Print errors if present.
		//
		CComPtr<IDxcBlobUtf8> pErrors = nullptr;
		pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
		// Note that d3dcompiler would return null if no errors or warnings are present.
		// IDxcCompiler3::Compile will always return an error buffer, but its length
		// will be zero if there are no warnings or errors.
		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
			wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

		//
		// Quit if the compilation failed.
		//
		HRESULT hrStatus;
		pResults->GetStatus(&hrStatus);
		if (FAILED(hrStatus))
		{
			wprintf(L"Compilation Failed\n");
		}

		//
		// Save shader binary.
		//
		CComPtr<IDxcBlob> pShader = nullptr;
		CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
		pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), nullptr);
		if (pShader != nullptr)
		{
			FILE* fp = NULL;

			/*_wfopen_s(&fp, pShaderName->GetStringPointer(), L"wb");
			fwrite(pShader->GetBufferPointer(), pShader->GetBufferSize(), 1, fp);
			fclose(fp);*/

			std::cout << "Shader compiled successfully. Size: " << pShader->GetBufferSize() << " bytes" << std::endl;
			std::cout << "Shader compiled successfully. Adress: " << pShader->GetBufferPointer() << std::endl;
		}

		//
		// Save pdb.
		//
		CComPtr<IDxcBlob> pPDB = nullptr;
		CComPtr<IDxcBlobUtf16> pPDBName = nullptr;
		pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);
		if(pPDB != nullptr && pPDBName != nullptr)
		{
			FILE* fp = NULL;

			// Note that if you don't specify -Fd, a pdb name will be automatically generated.
			// Use this file name to save the pdb so that PIX can find it quickly.
			_wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb");
			fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
			fclose(fp);
		}

		//
		// Print hash.
		//
		CComPtr<IDxcBlob> pHash = nullptr;
		pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
		if (pHash != nullptr && pHash->GetBufferSize() >= 16)
		{
			wprintf(L"Hash: ");
			DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
			for (int i = 0; i < _countof(pHashBuf->HashDigest); i++)
				wprintf(L"%.2x", pHashBuf->HashDigest[i]);
			wprintf(L"\n");
		}

		//
		// Demonstrate getting the hash from the PDB blob using the IDxcUtils::GetPDBContents API
		//
		CComPtr<IDxcBlob> pHashDigestBlob = nullptr;
		CComPtr<IDxcBlob> pDebugDxilContainer = nullptr;
		if (SUCCEEDED(pUtils->GetPDBContents(pPDB, &pHashDigestBlob, &pDebugDxilContainer)))
		{
			// This API returns the raw hash digest, rather than a DxcShaderHash structure.
			// This will be the same as the DxcShaderHash::HashDigest returned from
			// IDxcResult::GetOutput(DXC_OUT_SHADER_HASH, ...).
			wprintf(L"Hash from PDB: ");
			const BYTE* pHashDigest = (const BYTE*)pHashDigestBlob->GetBufferPointer();
			assert(pHashDigestBlob->GetBufferSize() == 16); // hash digest is always 16 bytes.
			for (int i = 0; i < pHashDigestBlob->GetBufferSize(); i++)
				wprintf(L"%.2x", pHashDigest[i]);
			wprintf(L"\n");

			// The pDebugDxilContainer blob will contain a DxilContainer formatted
			// binary, but with different parts than the pShader blob retrieved
			// earlier.
			// The parts in this container will vary depending on debug options and
			// the compiler version.
			// This blob is not meant to be directly interpreted by an application.
		}

		auto reflectionData = ReflectShaderStage(pResults, pUtils);

		ShaderBytecode o
		{
			.bytecode = pShader,
			.entry = entry,
			.target = target,
			.ReflectionData = std::move(reflectionData)
		};
	}
	
	ShaderVariantData DX12Device::CompileVariantAllStages(
			const ShaderCompileOptions& compileOptions,
			const ShaderCreateDesc& desc,
			const DxcBuffer& source,
			const ShaderKeywordSet& keywordSet,
			const ShaderKeywordSpace* keywordSpace)
	{
		ShaderVariantData o{};
		o.KeywordSet = keywordSet;

		for(auto& stage : desc.stages)
		{
			/// Switch Target
			std::wstring target;
			switch (stage.ShaderType)
			{
				case ShaderType::Vertex:
					{
						target = L"vs_6_6";
					}
			
				break;
				case ShaderType::Pixel:
					{
						target = L"ps_6_6";
					}
				break;
				case ShaderType::Compute:
					{
						target = L"cs_6_6";
					}
				break;

				default:
					ElysiaHelper::AssertError("Unimplemented shader type.");
				break;
			}

			//
			// Get x64 path
			WCHAR assetsPath[512];
			ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

			std::cout << std::filesystem::path(stage.ShaderName).string() << std::endl;
			std::cout << std::filesystem::path(stage.EntryPoint).string() << std::endl;
			std::cout << std::filesystem::path(target).string() << std::endl;
			auto temp = std::filesystem::path(assetsPath).wstring();
			temp += L"\\Shaders";

			LPCWSTR pdbName = std::wstring(stage.ShaderName + stage.EntryPoint + std::wstring(L".pdb")).c_str();
			LPCWSTR binName = std::wstring(stage.ShaderName + stage.EntryPoint + std::wstring(L".bin")).c_str();

			auto newCompileOptions = compileOptions;
			newCompileOptions.SetShaderPath(stage.ShaderName);
			newCompileOptions.SetEntry(stage.EntryPoint);
			newCompileOptions.SetTarget(target);
			newCompileOptions.AddIncludeDir(temp);
			newCompileOptions.AddIncludeDir(temp + L"\\public");
			newCompileOptions.AddIncludeDir(temp + L"\\private");

			for (size_t i = 0; i < keywordSpace->Count(); i++)
			{
				if(keywordSet.Bits().test(i))
				{
					auto name = keywordSpace->GetName((int)i);
					newCompileOptions.AddMacro(name);
				}
			}
			auto pszArgs = newCompileOptions.BuildArguments();

			o.StageShaders[stage.ShaderType] = CompileShaderStage(stage.ShaderName, stage.EntryPoint, stage.Target, pszArgs, source);
			o.MergedReflectionData.Merge(o.StageShaders[stage.ShaderType].ReflectionData);
		}
		
		return o;
	}
}
