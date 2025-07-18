#include "DX12Device.h"

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
		for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			ProcessDestruction(i);
		}

		ElysiaHelper::SafeRelease(m_device);
		ElysiaHelper::SafeRelease(m_DXGIFactory);
		ElysiaHelper::SafeRelease(m_swapChain);
		ElysiaHelper::SafeRelease(m_allocator);
	}

	void DX12Device::InitializeDeviceResources(HWND windowHandle)
	{
		// Enable Debug
		{
			// 仅在debug模式下可用，可以获取更多的调试信息和错误报告
		// 必须在创建D3D12 Device前启用调试层，启用后可以直接删除(因为创建D3D12 Device后，调用该API会在runtime自动删除Device)
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12sdklayers/nf-d3d12sdklayers-id3d12debug-enabledebuglayer
#if defined(_DEBUG)
			ID3D12Debug* debugController = nullptr;
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
			for (UINT currAdapterIndex = 0; m_DXGIFactory->EnumAdapters1(currAdapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; currAdapterIndex++)
			{
				DXGI_ADAPTER_DESC1 adapterDesc;
				ElysiaHelper::AssertIfFailed(adapter->GetDesc1(&adapterDesc));

				// 软件adapter
				if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				// 在不创建Device的情况下，检测adapter是否支持D3D12
				if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
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
			/*m_computeQueue = std::make_unique<DX12Queue>(m_device);
			m_copyQueue = std::make_unique<DX12Queue>(m_device);*/
		}

		// Create Descriptor Heap
		{
			m_RTVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				NUM_RTV_STAGING_DESCRIPTORS);
			/*m_DSVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				NUM_RTV_STAGING_DESCRIPTORS);*/
			m_SRVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				NUM_RTV_STAGING_DESCRIPTORS);

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
		m_frameID = 0;

		// -1 for IMGUI Descriptor
		m_freeReservedDescriptorIndices.resize(NUM_RESERVED_SRV_DESCRIPTORS - 1);
		std::iota(m_freeReservedDescriptorIndices.begin(), m_freeReservedDescriptorIndices.end(), 1);
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
	void DX12Device::CreateSamplers()
	{

	}

	std::unique_ptr<DX12GraphicsContext>	DX12Device::CreateGraphicsContext()
	{
		auto graphicsContext = std::make_unique<DX12GraphicsContext>(this);

		return graphicsContext;
	}
	std::unique_ptr<DX12VertexBuffer>		DX12Device::CreateVertexBuffer(const BufferCreationDesc& bufferCreationDesc)
	{
		if (bufferCreationDesc.bufferTypeFlags != BufferTypeFlags::SRV)
		{
			ElysiaHelper::AssertError("Vertex Buffer的 BufferTypeFlags 不匹配");
			return nullptr;
		}
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

			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			//SRVDesc.Format = bufferCreationDesc.m_isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = numElements;
			//SRVDesc.Buffer.StructureByteStride = bufferCreationDesc.m_isRawAccess ? 0 : vertexBuffer->GetVertexBufferView().StrideInBytes;
			SRVDesc.Buffer.StructureByteStride = vertexBuffer->GetVertexBufferView().StrideInBytes;
			SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

			//vertexBuffer->SetSRVDescriptor(std::move(m_SRVStagingDescriptorHeap->NewDescriptorHeapHandle()));
			/*vertexBuffer->SetDescriptorHeapIndex(m_freeReservedDescriptorIndices.back());
			m_freeReservedDescriptorIndices.pop_back();*/

			//m_device->CreateShaderResourceView(vertexBuffer->GetResource(), &SRVDesc, vertexBuffer->GetSRVDescriptor().GetCPUHandle());
		}

		return vertexBuffer;
	}
	std::unique_ptr<DX12Shader>				DX12Device::CreateShader(ShaderCreateDesc& shaderCreateDesc)
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

		std::unique_ptr<DX12Shader> o = std::make_unique<DX12Shader>(std::move(shader));
		return o;
	}
	std::unique_ptr<DX12RootSignature>		DX12Device::CreateRootSignature(RootSignatureCreatDesc& rootSignatureCreatDesc)
	{
		auto rootSignature = std::make_unique<DX12RootSignature>();

		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		//samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		rootSignature->InitStaticSamplers(0, samplerDesc);

		rootSignature->Init(m_device, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		
		return rootSignature;
	}
	std::unique_ptr<DX12GraphicsPipelineState>		DX12Device::CreateGraphicsPipelineState(PipelineStateCreateDesc& pipelineStateCreateDesc)
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

		auto graphicsPipeline = std::make_unique<DX12GraphicsPipelineState>(pipelineState, PSODesc.pRootSignature);
		return graphicsPipeline;
	}


	ContextSubmissionResult DX12Device::SubmitContextWork(DX12Context* context)
	{
		uint64_t fenceResult = 0;

		switch (context->GetContextType())
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			fenceResult = m_graphicsQueue->ExecuteCommandList(context->GetCommandList());
			break;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			fenceResult = m_computeQueue->ExecuteCommandList(context->GetCommandList());
			break;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			fenceResult = m_copyQueue->ExecuteCommandList(context->GetCommandList());
			break;
		default:
			ElysiaHelper::AssertError("Unsupported submission type.");
		}

		ContextSubmissionResult submissionResult;
		submissionResult.frameID = m_frameID;
		submissionResult.submissionIndex = static_cast<UINT>(m_contextSubmissions[m_frameID].size());

		m_contextSubmissions[m_frameID].push_back(std::make_pair(fenceResult, context->GetContextType()));

		return submissionResult;
	}

	void DX12Device::DestoryContext(std::unique_ptr<DX12Context> context)
	{
		m_destructionQueues[m_frameID].m_contexts.push_back(std::move(context));
	}
	void DX12Device::DestoryBuffer(std::unique_ptr<DX12GPUResource> buffer)
	{
		m_destructionQueues[m_frameID].m_buffers.push_back(std::move(buffer));
	}
	void DX12Device::DestoryPipelineState(std::unique_ptr<DX12PipelineState> pipelineState)
	{
		m_destructionQueues[m_frameID].m_pipelineStates.push_back(std::move(pipelineState));
	}
	void DX12Device::DestoryShader(std::unique_ptr<DX12Shader> shader)
	{
		auto tempShader = shader->GetShader();
		ElysiaHelper::SafeRelease(tempShader);
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
					if (vertexBuffer->GetMappedBuffer() != nullptr)
					{
						vertexBuffer->GetResource()->Unmap(0, nullptr);
					}
					break;
				}
				default:
					ElysiaHelper::AssertError("buffer type none");
					break;
			}

			auto resource = currBuffer->GetResource();
			auto allocation = currBuffer->GetAllocation();
			ElysiaHelper::SafeRelease(resource);
			ElysiaHelper::SafeRelease(allocation);
		}

		for (auto& currPipelineState : m_destructionQueues[frameIndex].m_pipelineStates)
		{
			auto signature = currPipelineState->GetRootSignature();
			auto pipelineState = currPipelineState->GetRootSignature();
			ElysiaHelper::SafeRelease(signature);
			ElysiaHelper::SafeRelease(pipelineState);
		}

		currFrameDestrctuionQueue.m_contexts.clear();
		currFrameDestrctuionQueue.m_buffers.clear();
		currFrameDestrctuionQueue.m_pipelineStates.clear();
	}

	void DX12Device::BeginFrame()
	{
		m_frameID = (m_frameID + 1) % NUM_FRAMES_IN_FLIGHT;

		// wait on fences from 2 frames ago
		m_graphicsQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_graphicsQueueFence);
		/*m_computeQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_computeQueueFence);
		m_copyQueue->WaitForFenceCPUBlocking(m_endOfFrameFences[m_frameID].m_copyQueueFence);*/

		ProcessDestruction(m_frameID);

		m_contextSubmissions[m_frameID].clear();
	}

	void DX12Device::EndFrame()
	{

	}

	void DX12Device::Present()
	{
		m_swapChain->Present(0, 0);
		m_endOfFrameFences[m_frameID].m_graphicsQueueFence = m_graphicsQueue->SingalFence();
	}

	void DX12Device::WaitForIdle()
	{
		m_graphicsQueue->WaitForIdle();
	}
}