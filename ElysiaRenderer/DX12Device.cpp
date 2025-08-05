#include "DX12Device.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 616; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }


namespace ElysiaRenderer
{
	DX12Device::DX12Device(HWND windowHandle, ElysiaHelper::UINT2 screenSize)
		: m_screenSize(screenSize)
	{
		InitializeDeviceResources(windowHandle);
		CreateWindowDependentResources();
	}

	DX12Device::~DX12Device()
	{
		WaitForIdle();

		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			DestoryBuffer(std::unique_ptr<DX12TextureUploadBuffer>(std::move(m_uploadContexts[i]->GetTexUploadHeap())));

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
			SafeRelease(pDebug);
		}
#endif // DEBUG

	}

	void DX12Device::InitializeDeviceResources(HWND windowHandle)
	{
		// Enable Debug
		{
			// 仅在debug模式下可用，可以获取更多的调试信息和错误报告
		// 必须在创建D3D12 Device前启用调试层，启用后可以直接删除(因为创建D3D12 Device后，调用该API会在runtime自动删除Device)
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
			IDXGIAdapter1* adapter = nullptr;
			UINT bestAdapterIndex = 0;
			size_t bestAdapterMemory = 0;	// 记录最大专用显存
			for (UINT currAdapterIndex = 0; 
				m_DXGIFactory->EnumAdapters1(currAdapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; 
				currAdapterIndex++)
			{
				DXGI_ADAPTER_DESC1 adapterDesc;
				ElysiaHelper::AssertIfFailed(adapter->GetDesc1(&adapterDesc));

				// soft ware adapter
				if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				// check support D3D12
				if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
				{
					continue;
				}

				// DedicatedVideoMemory:显卡自带的高速显存
				// DedicatedSystemMemory:系统内存中划分给显卡专用的部分
				if (adapterDesc.DedicatedVideoMemory > bestAdapterMemory)
				{
					bestAdapterIndex = currAdapterIndex;
					bestAdapterMemory = adapterDesc.DedicatedVideoMemory;
				}

				ElysiaHelper::SafeRelease(adapter);
			}

			if (bestAdapterMemory <= 0)
			{
				ElysiaHelper::AssertError("Failed to find an adapter.");
			}

			m_DXGIFactory->EnumAdapters1(bestAdapterIndex, &adapter);

			// Create Device
			ElysiaHelper::AssertIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

			// Create Allocator
			{
				D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
				allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
				allocatorDesc.pAdapter = adapter;
				allocatorDesc.pDevice = m_device;

				D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);
			}
		}

		// Create Queue
		{
			m_graphicsQueue = std::make_unique<DX12Queue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
			//m_computeQueue = std::make_unique<DX12Queue>(m_device);
			m_copyQueue = std::make_unique<DX12Queue>(m_device, D3D12_COMMAND_LIST_TYPE_COPY);
		}

		// Create Descriptor Heap
		{
			m_RTVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				NUM_RTV_STAGING_DESCRIPTORS);
			m_SRVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				NUM_RTV_STAGING_DESCRIPTORS);

			for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
			{
				m_SRVRenderPassDescriptorHeaps[currFrameIndex] = std::make_unique<DX12RenderPassDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
					NUM_SRV_RENDER_PASS_USER_DESCRIPTORS);
			}
			
			m_samplerRenderPassDescriptorHeap = std::make_unique<DX12RenderPassDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
				NUM_SAMPLER_DESCRIPTORS);

			for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
			{
				//m_SRVStagingDescriptorHeap
			}
		}

		// Create Swap Chain
		{
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
			swapChainDesc.Width = lround(m_screenSize.x);
			swapChainDesc.Height = lround(m_screenSize.y);
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.Stereo = false;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SampleDesc.Quality = 0;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Flags = 0;
			swapChainDesc.Scaling = DXGI_SCALING_NONE;
			swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

			IDXGISwapChain1* swapChain;
			ElysiaHelper::AssertIfFailed(m_DXGIFactory->CreateSwapChainForHwnd(m_graphicsQueue->GetCommandQueue(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
			ElysiaHelper::AssertIfFailed(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_swapChain));
			ElysiaHelper::SafeRelease(swapChain);
		}

		// Create Upload Context
		{
			VertexBufferCreationDesc vertexBufferCreationDesc{};
			vertexBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			vertexBufferCreationDesc.m_size = 10 * 1024 * 1024;

			TextureBufferCreationDesc textureBufferCreationDesc{};
			textureBufferCreationDesc.bufferAccessFlags = BufferAccessFlags::HostWritable;
			textureBufferCreationDesc.m_size = 40 * 1024 * 1024;

			for (UINT currFrameIndex = 0; currFrameIndex < NUM_FRAMES_IN_FLIGHT; ++currFrameIndex)
			{
				m_uploadContexts[currFrameIndex] = std::make_unique<DX12UploadContext>(
					this, 
					//CreateVertexBuffer(vertexBufferCreationDesc),
					CreateTextureUploadHeap(textureBufferCreationDesc));
			}
		}

		/*for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			m_destructionQueues[i].m_buffers = std::make_unique<std::vector<DX12BufferResource>>();
			m_destructionQueues[i].m_textures = std::make_unique<std::vector<DX12TextureResource>>();
			m_destructionQueues[i].m_contexts = std::make_unique<std::vector<DX12Context>>();
			m_destructionQueues[i].m_pipelineStates = std::make_unique<std::vector<DX12PipelineState>>();
		}*/
		m_frameID = 0;
		mFreeReservedDescriptorIndices.resize(NUM_RESERVED_SRV_DESCRIPTORS - 1);
		std::iota(mFreeReservedDescriptorIndices.begin(), mFreeReservedDescriptorIndices.end(), 1);
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

				D3D12_RENDER_TARGET_VIEW_DESC RTVDecs = {};
				RTVDecs.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				RTVDecs.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				RTVDecs.Texture2D.MipSlice = 0;
				RTVDecs.Texture2D.PlaneSlice = 0;
				m_device->CreateRenderTargetView(backBufferResource, &RTVDecs, currBackBufferRTVHandle.GetCPUHandle());

				m_backBuffers[currBufferIndex] = std::make_unique<DX12TextureResource>(
					backBufferResource, D3D12_RESOURCE_STATE_PRESENT);
				m_backBuffers[currBufferIndex]->SetResourceDesc(backBufferResource->GetDesc());
				m_backBuffers[currBufferIndex]->SetRTVDescriptor(currBackBufferRTVHandle);
			}
		}
	}

	std::unique_ptr<DX12GraphicsContext>		DX12Device::CreateGraphicsContext()
	{
		auto graphicsContext = std::make_unique<DX12GraphicsContext>(this);

		return graphicsContext;
	}
	std::unique_ptr<DX12VertexBuffer>			DX12Device::CreateVertexBuffer(const VertexBufferCreationDesc& bufferCreationDesc)
	{
		/*if (bufferCreationDesc.bufferTypeFlags != BufferTypeFlags::SRV)
		{
			ElysiaHelper::AssertError("Vertex Buffer的 BufferTypeFlags 不匹配");
		}*/
		// CPU-writable/GPU-readable memory
		auto isHostViewable = bufferCreationDesc.bufferAccessFlags == BufferAccessFlags::HostWritable;

		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = isHostViewable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_STATES usageState = isHostViewable ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;

		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc
		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Width = ElysiaHelper::AlignU32(static_cast<uint32_t>(bufferCreationDesc.m_size), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resourceDesc.Alignment = 0;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.SampleDesc = {1, 0};

		D3D12MA::Allocation* allocation = nullptr;
		ID3D12Resource* resource = nullptr;
		ElysiaHelper::ThrowIfFailed(m_allocator->CreateResource(&allocationDesc, &resourceDesc, usageState, nullptr,
			&allocation, IID_PPV_ARGS(&resource)));

		auto vertexBuffer = std::make_unique<DX12VertexBuffer>(resource, usageState, bufferCreationDesc.m_stride, bufferCreationDesc.m_size, allocation);
		
		{
			UINT numElements = static_cast<UINT>(bufferCreationDesc.m_stride > 0 ? bufferCreationDesc.m_size / bufferCreationDesc.m_stride : 1);

			/*D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.Format = bufferCreationDesc.m_isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = numElements;
			SRVDesc.Buffer.StructureByteStride = bufferCreationDesc.m_isRawAccess ? 0 : vertexBuffer->GetVertexBufferView().StrideInBytes;
			SRVDesc.Buffer.StructureByteStride = vertexBuffer->GetVertexBufferView().StrideInBytes;
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;*/

			//vertexBuffer->SetSRVDescriptor(std::move(m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle()));
			/*vertexBuffer->SetDescriptorHeapIndex(m_freeReservedDescriptorIndices.back());
			m_freeReservedDescriptorIndices.pop_back();*/

			//m_device->CreateShaderResourceView(vertexBuffer->GetResource(), &SRVDesc, vertexBuffer->GetSRVDescriptor().GetCPUHandle());
		}

		return vertexBuffer;
	}
	std::unique_ptr<DX12TextureUploadBuffer>	DX12Device::CreateTextureUploadHeap(const TextureBufferCreationDesc& bufferCreationDesc)
	{
		auto isHostViewable = bufferCreationDesc.bufferAccessFlags == BufferAccessFlags::HostWritable;

		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = isHostViewable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_STATES usageState = isHostViewable ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;

		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_desc
		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Width = ElysiaHelper::AlignU32(static_cast<uint32_t>(bufferCreationDesc.m_size), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resourceDesc.Alignment = 0;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.SampleDesc = { 1, 0 };

		D3D12MA::Allocation* allocation = nullptr;
		ID3D12Resource* resource = nullptr;
		ElysiaHelper::ThrowIfFailed(m_allocator->CreateResource(&allocationDesc, &resourceDesc, usageState, nullptr,
			&allocation, IID_PPV_ARGS(&resource)));

		auto texBuffer = std::make_unique<DX12TextureUploadBuffer>(resource, usageState, allocation);

		return texBuffer;
	}
	std::unique_ptr<DX12TextureResource>		DX12Device::CreateTextureFromFile(const TextureCreationDesc& textureCreationDesc)
	{
		auto& texturePath = textureCreationDesc.texturePath;
		bool isSRGB = textureCreationDesc.isSRGB;

		/// Load DDS
		std::unique_ptr<DirectX::ScratchImage> imageData = nullptr;
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
			auto loadResult = DirectX::LoadFromDDSFile(ElysiaHelper::GetAssetFullPath(assetsPath, textureCreationDesc.texturePath).c_str(), DirectX::DDS_FLAGS_NONE, nullptr, *imageData);
			assert(loadResult == S_OK);
		}
		///

		/// grad tex data
		///
		const DirectX::TexMetadata& texMetaData = imageData->GetMetadata();
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

		// 每个Mip图相当于一个子资源
		auto texBuffer = std::make_unique<DX12TextureBuffer>(newTex.get(), texMetaData.mipLevels, texMetaData.arraySize);
		UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];	// 每个子资源的行数
		uint64_t rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];

		m_device->GetCopyableFootprints(&texBuffer->GetDefaultHeap()->GetResourceDesc(), 0, texBuffer->GetNumSubResources(), 0,
			texBuffer->GetSubResourceLayouts().data(), numRows, rowSizesInBytes, &texBuffer->GetTextureDataSize());
		
		texBuffer->InitTexData();

		for (size_t arrayIndex = 0; arrayIndex < texMetaData.arraySize; ++arrayIndex)
		{
			for (size_t mipIndex = 0; mipIndex < texMetaData.mipLevels; ++mipIndex)
			{
				const uint64_t subResourceIndex = mipIndex + (arrayIndex * texMetaData.mipLevels);

				const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourcelayout = texBuffer->GetSubResourceLayouts()[subResourceIndex];
				const uint64_t subResourceHeight = numRows[subResourceIndex];
				// 每行数据的字节数
				const uint64_t subResourcePitch = ElysiaHelper::AlignU32(subResourcelayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
				const uint64_t subResourceDepth = subResourcelayout.Footprint.Depth;
				uint8_t* destSubResourceMemory = texBuffer->GetTexData().get() + subResourcelayout.Offset;

				// sliceIndex是3D纹理的切片索引，2D纹理的切片索引为0
				for (uint64_t sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++)
				{
					const auto subImage = imageData->GetImage(mipIndex, arrayIndex, sliceIndex);
					const uint8_t* sourceSubResourceMemory = subImage->pixels;
					// 拷贝图片每行数据
					for (uint64_t height = 0; height < subResourceHeight; ++height)
					{
						memcpy(destSubResourceMemory, sourceSubResourceMemory, (std::min)(subResourcePitch, subImage->rowPitch));
						destSubResourceMemory += subResourcePitch;
						sourceSubResourceMemory += subImage->rowPitch;
					}
				}
			}
		}

		m_uploadContexts[m_frameID]->AddTextureBufferUpload(std::move(texBuffer));

		return newTex;
	}
	std::unique_ptr<DX12TextureResource>		DX12Device::CreateTexture(TexCreateDesc& desc)
	{
		auto& resourceDesc = desc.m_resouceDesc;
		auto& typeFlag = desc.m_typeFlag;

		bool hasRTV = typeFlag == TexTypeFlags::RTV;
		bool hasSRV = typeFlag == TexTypeFlags::SRV;
		bool hasDSV = typeFlag == TexTypeFlags::DSV;
		bool hasUAV = typeFlag == TexTypeFlags::UAV;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = resourceDesc.Format;
		if (hasDSV)
		{
			clearValue.DepthStencil.Depth = 1.0f;
		}

		/// Create default heap for tex
		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_STATES usageState = D3D12_RESOURCE_STATE_COPY_DEST;
		ID3D12Resource* texResource = nullptr;
		D3D12MA::Allocation* allocation = nullptr;
		m_allocator->CreateResource(&allocationDesc, &resourceDesc, usageState, (!hasRTV && !hasDSV) ? nullptr : &clearValue,
			&allocation, IID_PPV_ARGS(&texResource));
		/// 

		auto newTex = std::make_unique<DX12TextureResource>(texResource, usageState, allocation);

		/// Create SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC SRV{};
		SRV.Format = resourceDesc.Format;
		SRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D && resourceDesc.DepthOrArraySize == 6)
		{
			SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			SRV.TextureCube.MostDetailedMip = 0;
			SRV.TextureCube.MipLevels = (UINT)resourceDesc.MipLevels;
			SRV.TextureCube.ResourceMinLODClamp = 0;
		}
		else
		{
			SRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRV.Texture2D.MostDetailedMip = 0;
			SRV.Texture2D.MipLevels = (UINT)resourceDesc.MipLevels;
			SRV.Texture2D.ResourceMinLODClamp = 0;
		}
		auto SRVHandle = m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle();
		m_device->CreateShaderResourceView(texResource, &SRV, SRVHandle.GetCPUHandle());
		///
		newTex->SetSRVDescriptor(SRVHandle);
		CopyDescriptorFromStageToRenderPass(newTex->GetSRVDescriptor(), )

		return newTex;
	}
	std::unique_ptr<DX12Shader>					DX12Device::CreateShader(ShaderCreateDesc& shaderCreateDesc)
	{
		ID3DBlob* shader = nullptr;

		/// Enable Debug
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		/// Switch Target
		LPCSTR target = nullptr;
		switch (shaderCreateDesc.shaderType)
		{
		case ShaderType::Vertex:
		{
			target = "vs_5_0";
		}
			
			break;
		case ShaderType::Pixel:
		{
			target = "ps_5_0";
		}
			break;
		case ShaderType::Compute:
		{
			target = "cs_5_0";
		}
			break;

		default:
			ElysiaHelper::AssertError("Unimplemented shader type.");
			break;
		}

		WCHAR assetsPath[512];
		ElysiaHelper::GetAssetsPath(assetsPath, _countof(assetsPath));

		//auto shaderFullPath = ElysiaHelper::GetAssetFullPath(assetsPath, shaderCreateDesc.shaderName).c_str();

		auto compleHR = D3DCompileFromFile(ElysiaHelper::GetAssetFullPath(assetsPath, shaderCreateDesc.shaderName).c_str(),
			nullptr, nullptr,
			shaderCreateDesc.entryPoint, target,
			compileFlags, 0, &shader, nullptr);
		ElysiaHelper::ThrowIfFailed(compleHR);

		auto o = std::make_unique<DX12Shader>(std::move(shader));
		return o;
	}
	void										DX12Device::CreateSamplers(DX12RootSignature* rootSignature, D3D12_SHADER_VISIBILITY shaderVisibility)
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
		rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;

		samplerDescs[samplerIndex].Filter = D3D12_FILTER_ANISOTROPIC;
		samplerDescs[samplerIndex].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDescs[samplerIndex].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		rootSignature->InitStaticSamplers(samplerIndex, samplerDescs[samplerIndex], shaderVisibility);
		samplerIndex++;
	}
	void										DX12Device::CreateRootParameters(DX12RootSignature* rootSignature, std::vector<DX12RootParameter*>& rootParamters)
	{
		for (auto i = 0; i < rootParamters.size(); ++i)
		{
			(*rootSignature)[i] = *rootParamters[i];
		}
	}
	std::unique_ptr<DX12RootSignature>			DX12Device::CreateRootSignature(RootSignatureCreatDesc& rootSignatureCreatDesc)
	{
		UINT numRootParamter = rootSignatureCreatDesc.rootParamters.size();
		UINT numSampler = NUM_SAMPLER_DESCRIPTORS;
		auto rootSignature = std::make_unique<DX12RootSignature>(numRootParamter, numSampler);

		CreateSamplers(rootSignature.get());

		CreateRootParameters(rootSignature.get(), rootSignatureCreatDesc.rootParamters);

		rootSignature->Init(m_device, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		
		return rootSignature;
	}
	std::unique_ptr<DX12GraphicsPipelineState>	DX12Device::CreateGraphicsPipelineState(PipelineStateCreateDesc& pipelineStateCreateDesc)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC PSODesc{};
		if (pipelineStateCreateDesc.m_vertexShader != nullptr)
		{
			PSODesc.VS.pShaderBytecode = pipelineStateCreateDesc.m_vertexShader->GetShader()->GetBufferPointer();
			PSODesc.VS.BytecodeLength = pipelineStateCreateDesc.m_vertexShader->GetShader()->GetBufferSize();
		}

		if (pipelineStateCreateDesc.m_pixelShader != nullptr)
		{
			PSODesc.PS.pShaderBytecode = pipelineStateCreateDesc.m_pixelShader->GetShader()->GetBufferPointer();
			PSODesc.PS.BytecodeLength = pipelineStateCreateDesc.m_pixelShader->GetShader()->GetBufferSize();
		}
		
		PSODesc.InputLayout = { pipelineStateCreateDesc.m_inputElementDesc.data(),
			static_cast<UINT>(pipelineStateCreateDesc.m_inputElementDesc.size())};
		PSODesc.pRootSignature = pipelineStateCreateDesc.m_rootSignature->GetSignature();
		PSODesc.RasterizerState = pipelineStateCreateDesc.m_rasterDesc;
		PSODesc.BlendState = pipelineStateCreateDesc.m_blendDesc;
		PSODesc.DepthStencilState = pipelineStateCreateDesc.m_depthStencilDesc;
		PSODesc.DSVFormat = pipelineStateCreateDesc.m_renderTargetDesc.m_depthStencilFormat;
		PSODesc.NodeMask = 0;
		PSODesc.SampleMask = UINT_MAX;
		PSODesc.PrimitiveTopologyType = pipelineStateCreateDesc.m_topology;
		PSODesc.NumRenderTargets = pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets;
		for (UINT i = 0; i < pipelineStateCreateDesc.m_renderTargetDesc.m_numRenderTargets; ++i)
		{
			PSODesc.RTVFormats[i] = pipelineStateCreateDesc.m_renderTargetDesc.m_renderTargetFormats[i];
		}
		PSODesc.SampleDesc = pipelineStateCreateDesc.m_sampleDesc;

		ID3D12PipelineState* pipelineState = nullptr;
		ElysiaHelper::ThrowIfFailed(m_device->CreateGraphicsPipelineState(&PSODesc, IID_PPV_ARGS(&pipelineState)));

		auto graphicsPipeline = std::make_unique<DX12GraphicsPipelineState>(pipelineState, pipelineStateCreateDesc.m_rootSignature);
		return graphicsPipeline;
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
	/// <param name="index"> RenderPass descriptor index in heap </param>
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
		ElysiaHelper::SafeRelease(shader->GetShader());
	}
	void DX12Device::DestoryTexture(std::unique_ptr<DX12TextureResource> texture)
	{
		m_destructionQueues[m_frameID].m_textures.push_back(std::move(texture));
	}

	void DX12Device::ProcessDestruction(UINT frameIndex)
	{
		auto& currFrameDestrctuionQueue = m_destructionQueues[frameIndex];

		for (auto& currBuffer : m_destructionQueues[frameIndex].m_buffers)
		{
			switch (currBuffer->GetBufferType())
			{
				case BufferType::Vertex:
				{
					auto vertexBuffer = dynamic_cast<DX12VertexBuffer*>(currBuffer.get());
					/*if (vertexBuffer->GetSRVDescriptor().IsValid())
					{
						m_SRVStagingDescriptorHeap->FreeDescriptorHeapHandle(vertexBuffer->GetSRVDescriptor());
					}*/
					vertexBuffer->Unmap();
					break;
				}
				case BufferType::Texture:
				{
					auto texUploadHeap = dynamic_cast<DX12TextureUploadBuffer*>(currBuffer.get());
					texUploadHeap->Unmap();
				}
				default:
					ElysiaHelper::AssertError("buffer type none");
					break;
			}

			//ElysiaHelper::SafeRelease(currBuffer.GetResource());
			//ElysiaHelper::SafeRelease(currBuffer.GetAllocation());
		}

		for (auto& currTex : m_destructionQueues[frameIndex].m_textures)
		{
			/*ElysiaHelper::SafeRelease(currTex.GetAllocation());
			ElysiaHelper::SafeRelease(currTex.GetResource());*/
		}

		for (auto& currPipelineState : m_destructionQueues[frameIndex].m_pipelineStates)
		{
			/*ElysiaHelper::SafeRelease(currPipelineState.GetRootSignature());
			ElysiaHelper::SafeRelease(currPipelineState.GetPipelineState());*/
		}

		(currFrameDestrctuionQueue.m_contexts).clear();
		(currFrameDestrctuionQueue.m_buffers).clear();
		(currFrameDestrctuionQueue.m_textures).clear();
		(currFrameDestrctuionQueue.m_pipelineStates).clear();
	}

	void DX12Device::BeginFrame()
	{
		m_frameID = (m_frameID + 1) % NUM_FRAMES_IN_FLIGHT;

		// wait on fences from 2 frames ago
		m_graphicsQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_graphicsQueueFence);
		m_copyQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_copyQueueFence);
		/*m_computeQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_computeQueueFence);
		m_copyQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_copyQueueFence);*/

		ProcessDestruction(m_frameID);

		m_uploadContexts[m_frameID]->Reset();

		m_contextSubmissions[m_frameID].clear();
	}

	void DX12Device::EndFrame()
	{
		m_uploadContexts[m_frameID]->ProcessUploads();
		SubmitContextWork(*m_uploadContexts[m_frameID]);

		m_endOfFrameFences[m_frameID].m_copyQueueFence = m_copyQueue->SingalFence();
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
	}
}