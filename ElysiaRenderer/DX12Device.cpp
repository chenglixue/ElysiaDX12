#include "DX12Device.h"

namespace ElysiaRenderer
{
	DX12Device::DX12Device(HWND windowHandle, UINT2 screenSize)
		: m_screenSize(screenSize)
	{
		InitializeDeviceResources();
		CreateWindowDependentResources(windowHandle, screenSize);
	}

	DX12Device::~DX12Device()
	{

	}

	void DX12Device::InitializeDeviceResources()
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

				SafeRelease(debugController);
			}
#endif
		}
		

		// Create DXGIFactory1
		{
			AssertIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_DXGIFactory)));
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
				AssertIfFailed(adapter->GetDesc1(&adapterDesc));

				// 软件adapter
				if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				// 在不创建Device的情况下，检测adapter是否支持D3D12
				if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
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

				SafeRelease(adapter);
			}

			if (bestAdapterMemory <= 0)
			{
				AssertError("Failed to find an adapter.");
			}

			m_DXGIFactory->EnumAdapters1(bestAdapterIndex, &adapter);

			IDXGIAdapter1* adapter = nullptr;
			UINT bestAdapterIndex = 0;
			size_t bestAdapterMemory = 0;	// 记录最大专用显存
			for (UINT currAdapterIndex = 0; m_DXGIFactory->EnumAdapters1(currAdapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; currAdapterIndex++)
			{
				DXGI_ADAPTER_DESC1 adapterDesc;
				AssertIfFailed(adapter->GetDesc1(&adapterDesc));

				// 软件adapter
				if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				// 在不创建Device的情况下，检测adapter是否支持D3D12
				if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
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

				SafeRelease(adapter);
			}

			if (bestAdapterMemory <= 0)
			{
				AssertError("Failed to find an adapter.");
			}

			m_DXGIFactory->EnumAdapters1(bestAdapterIndex, &adapter);

			// Create Device
			AssertIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)));

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
			m_queueManager = std::make_unique<DX12QueueManager>(m_device);
		}

		// Create Descriptor Heap
		{
			m_RTVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				NUM_RTV_STAGING_DESCRIPTORS);
			m_DSVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
				NUM_RTV_STAGING_DESCRIPTORS);
			m_SRVStagingDescriptorHeap = std::make_unique<DX12StagingDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				NUM_RTV_STAGING_DESCRIPTORS);
		}
	}

	void DX12Device::CreateWindowDependentResources(HWND windowHandle, UINT2 screenSize)
	{
		// Create Swap Chain
		{
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = screenSize.x;
			swapChainDesc.Height = screenSize.y;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.Stereo = false;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SampleDesc.Quality = 0;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.BufferCount = NUM_BACK_BUFFERS;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			swapChainDesc.Flags = 0;
			swapChainDesc.Scaling = DXGI_SCALING_NONE;
			swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

			IDXGISwapChain1* swapChain;
			AssertIfFailed(m_DXGIFactory->CreateSwapChainForHwnd(m_graphicsQueue->GetCommandQueue(), windowHandle, &swapChainDesc, nullptr, nullptr, &swapChain));
			AssertIfFailed(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_swapChain));
			SafeRelease(swapChain);
		}

		// Create Render Target
		{
			for (UINT currBufferIndex = 0; currBufferIndex < NUM_BACK_BUFFERS; currBufferIndex++)
			{
				auto descriptorHandle = m_RTVStagingDescriptorHeap->NewDescriptorHeapHandle();

				ID3D12Resource* backBufferResource = nullptr;
				AssertIfFailed(m_swapChain->GetBuffer(currBufferIndex, IID_PPV_ARGS(&backBufferResource)));

				D3D12_RENDER_TARGET_VIEW_DESC RTVDecs = {};
				RTVDecs.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				RTVDecs.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				RTVDecs.Texture2D.MipSlice = 0;
				RTVDecs.Texture2D.PlaneSlice = 0;
				m_device->CreateRenderTargetView(backBufferResource, &RTVDecs, descriptorHandle.GetCPUHandle());

				m_backBuffers[currBufferIndex] = std::make_unique<DX12TextureBuffer>(
					backBufferResource, D3D12_RESOURCE_STATE_PRESENT, descriptorHandle);
				m_backBuffers[currBufferIndex]->SetResourceDesc(backBufferResource->GetDesc());
			}
		}

		m_frameID = 0;
	}

	void DX12Device::BeginFrame()
	{
		m_frameID = (m_frameID + 1) % NUM_FRAMES_IN_FLIGHT;

		m_queueManager->WaitForFenceCPUBlocking();
	}

	void DX12Device::EndFrame()
	{

	}

	void DX12Device::Present()
	{

	}
}