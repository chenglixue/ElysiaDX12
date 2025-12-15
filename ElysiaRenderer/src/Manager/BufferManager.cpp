#include "stdafx.h"
#include "BufferManager.h"

#include "RenderTargetManager.h"
#include "DX12/DX12StagingDescriptorHeap.h"
#include "DX12/DX12UploadContext.h"
#include "DX12/UploadRingBuffer.h"
#include "lib/DX12/DX12Device.h"
#include "lib/DX12/DX12TextureBuffer.h"
#include "lib/DX12/DX12BufferResource.h"
#include "Parameter/UserData.h"

namespace ElysiaRenderer
{
	std::unique_ptr<BufferManager> BufferManager::m_instance;
	std::once_flag BufferManager::m_initInstanceFlag;
	
	BufferManager::~BufferManager()
	{
		Destory();
	}

	void BufferManager::Init(DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;

		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
		allocatorDesc.pDevice = m_pDevice->GetDevice();
		allocatorDesc.pAdapter = m_pDevice->GetAdapter();
		D3D12MA::CreateAllocator(&allocatorDesc, &m_pAllocator);
		
		m_pUploadBuffer = std::make_unique<UploadRingBuffer>(m_pDevice, m_pAllocator, 256 * 1024 * 1024, L"Global Upload Buffer");
		
		
		
		if (!UserData::GetInstance().IsUseHDR)
		{
			m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_pDevice->GetScreenSize().x),
				static_cast<UINT64>(m_pDevice->GetScreenSize().y),
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
				true,
				"Camera Color RT");
		}
		else
		{
			switch (UserData::GetInstance().HDRLevel)
			{
				case HDRQuality::Low: 
				{
					m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_pDevice->GetScreenSize().x),
						static_cast<UINT64>(m_pDevice->GetScreenSize().y),
						DXGI_FORMAT_R11G11B10_FLOAT,
						true,
						"Camera Color RT"); 
					break;  
				} 
				case HDRQuality::High:
				{ 
					m_pCameraColorRT = RenderTargetManager::GetInstance().CreateRWRenderTexture(static_cast<UINT64>(m_pDevice->GetScreenSize().x),
						static_cast<UINT64>(m_pDevice->GetScreenSize().y),
						DXGI_FORMAT_R16G16B16A16_FLOAT,
						true,
						"Camera Color RT");
					break;
				}
				default:
				{ 
					ThrowRuntimeError("Invalid choose");
					break;
				}
			}
		}
		m_pCameraDepthRT = RenderTargetManager::GetInstance().CreateRenderTexture(
			static_cast<UINT64>(m_pDevice->GetScreenSize().x),
			static_cast<UINT64>(m_pDevice->GetScreenSize().y),
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			true,
			"Camera Depth RT");
	}

	void BufferManager::Destory()
	{
		ElysiaHelper::SafeRelease(m_pAllocator);
		for (auto& buffer : m_buffers)
		{
			if (buffer)
			{
				buffer.reset();
			}
		}
	}

	void BufferManager::Update() 
	{
		
	}

	D3D12MA::Allocator* BufferManager::GetAllocator() const noexcept
	{
		assert(m_pAllocator);
		return m_pAllocator;
	}
	UploadRingBuffer* BufferManager::GetUploadRingBuffer() const noexcept
	{
		assert(m_pUploadBuffer);
		return m_pUploadBuffer.get();
	}

	BufferManager::BufferHandle BufferManager::CreateBuffer(const BufferCreationDesc& bufferCreationDesc)
	{
		UINT32 index;
		if (!m_freeBufferSlots.empty())
		{
			index = m_freeBufferSlots.front();
			m_freeBufferSlots.pop();
		}
		else
		{
			index = static_cast<UINT32>(m_buffers.size());
			m_buffers.emplace_back();
		}

		UINT numElements = static_cast<UINT>(bufferCreationDesc.stride > 0 ? bufferCreationDesc.size / bufferCreationDesc.stride : 1);
		bool isHostVisible = ((bufferCreationDesc.accessFlags & BufferAccessFlags::HostWritable) == BufferAccessFlags::HostWritable);
		bool isHasCBV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::CBV) == GPUResourceFlags::CBV);
		bool isHasSRV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::SRV) == GPUResourceFlags::SRV);
		bool isHasUAV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::UAV) == GPUResourceFlags::UAV);

		auto alignSize = AlignU32(bufferCreationDesc.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignSize);

		D3D12_RESOURCE_STATES resourceState =D3D12_RESOURCE_STATE_GENERIC_READ;
		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
		
		CComPtr<D3D12MA::Allocation> pAllocation = nullptr;
		CComPtr<ID3D12Resource> pResource = nullptr;
		ElysiaHelper::ThrowIfFailed(m_pAllocator->CreateResource(&allocationDesc, &resourceDesc, resourceState, nullptr,
			&pAllocation, IID_PPV_ARGS(&pResource)));
		if (bufferCreationDesc.name)
		{
			pResource->SetName(bufferCreationDesc.name);
		}

		auto pNewBuffer = std::make_unique<DX12BufferResource>(pResource, resourceState, pAllocation);
		pNewBuffer->SetIndex(index);
		if (isHostVisible)
		{
			pNewBuffer->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&pNewBuffer->m_mappedBuffer));
		}

		if (isHasCBV)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc{};
			CBVDesc.SizeInBytes = static_cast<UINT>(pNewBuffer->GetResourceDesc().Width);
			CBVDesc.BufferLocation = pNewBuffer->GetGPUAddress();

			pNewBuffer->SetCBVDescriptor(m_pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle());
			m_pDevice->GetDevice()->CreateConstantBufferView(&CBVDesc, pNewBuffer->GetCBVDescriptor().GetCPUHandle());
		}

		if (isHasSRV)
		{ 
			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SRVDesc.Format = bufferCreationDesc.isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			SRVDesc.Buffer.FirstElement = 0;
			SRVDesc.Buffer.NumElements = static_cast<UINT>(bufferCreationDesc.isRawAccess ? bufferCreationDesc.size / 4 : numElements);
			SRVDesc.Buffer.StructureByteStride = bufferCreationDesc.stride > 0 ? static_cast<UINT>(pNewBuffer->GetStride()) : 0;
			SRVDesc.Buffer.Flags = bufferCreationDesc.isRawAccess ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

			pNewBuffer->SetSRVDescriptor(m_pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle());
			m_pDevice->GetDevice()->CreateShaderResourceView(pNewBuffer->GetResource(), &SRVDesc, pNewBuffer->GetSRVDescriptor().GetCPUHandle());

			pNewBuffer->SetResourceHeapIndex(m_pDevice->m_freeReservedDescriptorIndices.back());
			m_pDevice->m_freeReservedDescriptorIndices.pop_back();

			m_pDevice->CopyDescriptorFromStageToRenderPass(pNewBuffer->GetSRVDescriptor(), pNewBuffer->GetResourceHeapIndex());
		}

		if (isHasUAV)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};

			UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			UAVDesc.Format = bufferCreationDesc.isRawAccess ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
			UAVDesc.Buffer.CounterOffsetInBytes = 0;
			UAVDesc.Buffer.FirstElement = 0;
			UAVDesc.Buffer.NumElements = static_cast<UINT>(bufferCreationDesc.isRawAccess ? bufferCreationDesc.size / 4 : numElements);
			UAVDesc.Buffer.StructureByteStride = bufferCreationDesc.isRawAccess ? 0 : static_cast<UINT>(pNewBuffer->GetStride());
			UAVDesc.Buffer.Flags = bufferCreationDesc.isRawAccess ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

			pNewBuffer->SetUAVDescriptor(m_pDevice->GetSRVStageHeap()->NewDescriptorHeapHandle());

			m_pDevice->GetDevice()->CreateUnorderedAccessView(pNewBuffer->GetResource(), nullptr, &UAVDesc, pNewBuffer->GetUAVDescriptor().GetCPUHandle());
		}

		if (index < m_buffers.size())
		{
			m_buffers[index] = std::move(pNewBuffer);
		}
		else
		{
			m_buffers.emplace_back(std::move(pNewBuffer));
		}

		return pNewBuffer;
	}
	void BufferManager::Release(BufferHandle handle)
	{
		if (!handle) return;

		UINT32 index = handle->GetIndex();
		if (index < m_buffers.size() && m_buffers[index] == handle)
		{
			m_freeBufferSlots.push(index);

			m_buffers[index].reset();
		}
	}

	void BufferManager::UploadBufferData(DX12UploadContext* uploadContext, std::vector<DX12BufferUpload*>& bufferUploads)
	{
		const auto numBufferUploads = static_cast<UINT>(bufferUploads.size());
		size_t bufferUploadHeapOffset = 0;
		UINT numBuffersProcessed = 0;

		size_t totalSize = 0;
		for (const auto& upload : bufferUploads)
		{
			totalSize += AlignUp(upload->bufferDataSize, (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}

		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
		UINT8* cpuAddress;
		if (m_pUploadBuffer->AllocateForFrame(m_pDevice->GetFrameID(), totalSize, gpuAddress, cpuAddress))
		{
			for (numBuffersProcessed; numBuffersProcessed < numBufferUploads; numBuffersProcessed++)
			{
				auto bufferUpload = bufferUploads[numBuffersProcessed];
				if (bufferUploadHeapOffset + bufferUpload->bufferDataSize > totalSize)
				{
					break;
				}

				uploadContext->AddBarrier(*bufferUpload->buffer, D3D12_RESOURCE_STATE_COPY_DEST, false);
				memcpy(cpuAddress + bufferUploadHeapOffset, bufferUpload->pBufferData, bufferUpload->bufferDataSize);
				D3D12_SUBRESOURCE_DATA subData = 
				{
						bufferUpload->pBufferData, 
				static_cast<UINT>(bufferUpload->bufferDataSize), 
				static_cast<UINT>(bufferUpload->bufferDataSize)
				};
				// uploadContext->CopyBufferRegion(*bufferUpload->buffer, 0, m_pUploadBuffer->GetResource(), 
				// 	gpuAddress - m_pUploadBuffer->GetResource()->GetGPUVirtualAddress(), bufferUpload->bufferDataSize);
				UpdateSubresources(uploadContext->GetCommandList(), bufferUpload->buffer->GetResource(), m_pUploadBuffer->GetResource(),
				gpuAddress - m_pUploadBuffer->GetResource()->GetGPUVirtualAddress() + bufferUploadHeapOffset, 0, 1, &subData);
				uploadContext->AddBarrier(*bufferUpload->buffer, D3D12_RESOURCE_STATE_COMMON, false);
				bufferUploadHeapOffset += bufferUpload->bufferDataSize;
				bufferUploadHeapOffset = AlignUp(bufferUploadHeapOffset, (size_t)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				uploadContext->AddBufferProcess(bufferUpload);
				
				numBuffersProcessed++;
			}
			uploadContext->FlushBarrier();
			
			if(numBuffersProcessed > 0)
			{
				bufferUploads.erase(bufferUploads.begin(), bufferUploads.begin() + numBuffersProcessed);
			}
		}
		
	}
	
	void BufferManager::UploadTextureData(DX12UploadContext* uploadContext, std::vector<DX12TextureUpload*>& textureUploads)
	{
		const auto numTextureUploads = static_cast<UINT>(textureUploads.size());
		size_t texUploadHeapOffset = 0;
		UINT numTexsProcessed = 0;
		
		size_t totalSize = 0;
		for (const auto& upload : textureUploads)
		{
			totalSize += AlignU64(upload->textureDataSize, 512);
		}
		
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
		UINT8* cpuAddress;
		if (m_pUploadBuffer->AllocateForFrame(m_pDevice->GetFrameID(), totalSize, gpuAddress, cpuAddress))
		{
			for (numTexsProcessed; numTexsProcessed < numTextureUploads; ++numTexsProcessed)
			{
				auto textureUpload = textureUploads[numTexsProcessed];
				if (texUploadHeapOffset + textureUpload->textureDataSize > totalSize)
				{
					break;
				}
				
				memcpy(cpuAddress + texUploadHeapOffset, textureUpload->pTextureData, textureUpload->textureDataSize);
				uploadContext->CopyTextureRegion(*textureUpload->pTextureBuffer, m_pUploadBuffer->GetResource(), texUploadHeapOffset,
					textureUpload->subResourceLayouts, textureUpload->numSubResources);
				
				texUploadHeapOffset += textureUpload->textureDataSize;
				texUploadHeapOffset = AlignU64(texUploadHeapOffset, 512);
				
				uploadContext->AddTextureProcess(textureUpload);
			}
			
			if (numTexsProcessed > 0)
			{
				textureUploads.erase(textureUploads.begin(), textureUploads.begin() + numTexsProcessed);
			}
		}
	}
	

	RenderTexture* BufferManager::GetCameraDepthRT() const noexcept
	{
		return m_pCameraDepthRT;
	}

	RenderTexture* BufferManager::GetCameraColorRT() const noexcept
	{
		return m_pCameraColorRT;
	}

	BufferManager::BufferHandle BufferManager::GetVertexBuffer() const noexcept
	{
		return m_pVertexBuffer;
	}

	BufferManager::BufferHandle BufferManager::GetIndexBuffer() const noexcept
	{
		return m_pIndexBuffer;
	}

	const D3D12_INDEX_BUFFER_VIEW& BufferManager::GetIndexBufferView() const noexcept
	{
		return m_indexBufferView;
	}

	const D3D12_VERTEX_BUFFER_VIEW& BufferManager::GetVertexBufferView() const noexcept
	{
		return m_vertexBufferView;
	}

	void BufferManager::AddVertexBuffer(BufferCreationDesc desc)
	{
		m_pVertexBuffer = std::move(BufferManager::GetInstance().CreateBuffer(desc));
	}

	void BufferManager::AddIndexBuffer(BufferCreationDesc desc)
	{
		m_pIndexBuffer = std::move(BufferManager::GetInstance().CreateBuffer(desc));
	}

	void BufferManager::SetVertexBufferView(const D3D12_VERTEX_BUFFER_VIEW& view)
	{
		m_vertexBufferView = view;
	}

	void BufferManager::SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW& view)
	{
		m_indexBufferView = view;
	}

}