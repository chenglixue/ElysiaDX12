#include "stdafx.h"
#include "BufferManager.h"

#include "Runtime/Core/DX12BufferResource.h"
#include "Runtime/Core/DX12TextureBuffer.h"
#include "Runtime/Core/DX12StagingDescriptorHeap.h"

#include "Runtime/Resource/Model/LoadedModel.h"
#include "Runtime/Core/UploadRingBuffer.h"
#include "Runtime/Core/DX12UploadContext.h"
#include "Runtime/Core/DX12Device.h"
#include "Runtime/Core/BufferUtility.h"
#include "Runtime/Engine/FrameContext.h"


namespace ElysiaRenderer
{
	std::unique_ptr<BufferManager> BufferManager::m_instance;
	std::once_flag BufferManager::m_initInstanceFlag;

	BufferManager::BufferManager() = default;
	BufferManager::~BufferManager()
	{
	}

	void BufferManager::Init(ElysiaCore::DX12Device* pDevice)
	{
		assert(pDevice);
		m_pDevice = pDevice;

		D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
		allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
		allocatorDesc.pDevice = m_pDevice->GetDevice();
		allocatorDesc.pAdapter = m_pDevice->GetAdapter();
		D3D12MA::CreateAllocator(&allocatorDesc, &m_pAllocator);
		
		m_pUploadBuffer = std::make_unique<UploadRingBuffer>(m_pDevice, m_pAllocator, 1024 * 1024 * 1024, L"Global Upload Buffer");
	}

	void BufferManager::Destory()
	{
		ElysiaHelper::SafeRelease(m_pAllocator);
		for (auto& buffer : m_bufferPools)
		{
			if (buffer)
			{
				buffer.reset();
			}
		}
	}

	void BufferManager::Update(const ElysiaEngine::FrameContext& context) 
	{
		m_frameID = context.frameID;
		m_frameIndex = context.frameIndex;
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

	BufferHandle BufferManager::CreateBuffer(const BufferCreationDesc& bufferCreationDesc)
	{
		std::lock_guard<std::mutex> lock(m_createMutex);
		
		auto alignSize = AlignU32((UINT)bufferCreationDesc.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		
		UINT32 index;
		if (!m_freeBufferIndices.empty())
		{
			index = m_freeBufferIndices.front();
			m_freeBufferIndices.pop();
			if(m_bufferPools[index] && m_bufferPools[index]->IsFree() && index < m_bufferPools.size() && m_bufferPools[index]->GetResourceDesc().Width < alignSize)
			{
				if(m_bufferPools[index]->ReInit(m_pDevice, bufferCreationDesc))
				{
					return m_bufferPools[index];
				}
			}
		}
		else
		{
			index = static_cast<UINT32>(m_bufferPools.size());
			m_bufferPools.emplace_back();
		}

		UINT numElements = static_cast<UINT>(bufferCreationDesc.stride > 0 ? bufferCreationDesc.size / bufferCreationDesc.stride : 1);
		bool isHostVisible = ((bufferCreationDesc.accessFlags & BufferAccessFlags::HostWritable) == BufferAccessFlags::HostWritable);
		bool isHasCBV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::CBV) == GPUResourceFlags::CBV);
		bool isHasSRV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::SRV) == GPUResourceFlags::SRV);
		bool isHasUAV = ((bufferCreationDesc.viewFlags & GPUResourceFlags::UAV) == GPUResourceFlags::UAV);
		
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignSize);

		D3D12_RESOURCE_STATES resourceState = isHostVisible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
		D3D12MA::ALLOCATION_DESC allocationDesc{};
		allocationDesc.HeapType = isHostVisible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
		
		CComPtr<D3D12MA::Allocation> pAllocation = nullptr;
		CComPtr<ID3D12Resource> pResource = nullptr;
		ElysiaHelper::ThrowIfFailed(m_pAllocator->CreateResource(&allocationDesc, &resourceDesc, resourceState, nullptr,
			&pAllocation, IID_PPV_ARGS(&pResource)));
		if (bufferCreationDesc.name.c_str())
		{
			pResource->SetName(bufferCreationDesc.name.c_str());
		}

		auto pNewBuffer = std::make_shared<DX12BufferResource>(pResource, resourceState, pAllocation);
		pNewBuffer->SetStride(bufferCreationDesc.stride);
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

		if (index < m_bufferPools.size())
		{
			m_bufferPools[index] = pNewBuffer;
		}
		else
		{
			m_bufferPools.emplace_back(pNewBuffer);
		}

		return pNewBuffer;
	}
	void BufferManager::DestoryBuffer(const BufferHandle handle)
	{
		if(!handle) return;
		
		UINT64 deleteFrameIndex = m_frameIndex + NUM_FRAMES_IN_FLIGHT;
		m_grbageQueue.push_back({deleteFrameIndex, handle});
	}
	void BufferManager::Release(BufferHandle handle)
	{
		if (!handle) return;

		UINT32 index = handle->GetIndex();
		if (index < m_bufferPools.size() && m_bufferPools[index] == handle)
		{
			m_freeBufferIndices.push(index);

			m_bufferPools[index]->Reset();
		}
	}
	void BufferManager::ProcessGarbage(uint64_t currentFrameIndex)
	{
		std::lock_guard<std::mutex> lock(m_garbageMutex);
		
		auto it = m_grbageQueue.begin();
		while(it != m_grbageQueue.end())
		{
			if(it->first <= currentFrameIndex)
			{
				it = m_grbageQueue.erase(it);
			}
			else
			{
				it++;
			}
		}
	}

	void BufferManager::UploadBufferData(DX12UploadContext* uploadContext, std::vector<DX12BufferUpload*>& bufferUploads)
	{
		const auto numBufferUploads = static_cast<UINT>(bufferUploads.size());
		size_t bufferUploadHeapOffset = 0;
		UINT numBuffersProcessed = 0;

		if (numBufferUploads <=0) return;

		size_t totalSize = 0;
		for (const auto& upload : bufferUploads)
		{
			totalSize += AlignU32(upload->bufferDataSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
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
				memcpy(cpuAddress + bufferUploadHeapOffset, bufferUpload->pBufferData.get(), bufferUpload->bufferDataSize);
				D3D12_SUBRESOURCE_DATA subData = 
				{
						bufferUpload->pBufferData.get(), 
				static_cast<UINT>(bufferUpload->bufferDataSize), 
				static_cast<UINT>(bufferUpload->bufferDataSize)
				};
				// uploadContext->CopyBufferRegion(*bufferUpload->buffer, 0, m_pUploadBuffer->GetResource(), 
				// 	gpuAddress - m_pUploadBuffer->GetResource()->GetGPUVirtualAddress(), bufferUpload->bufferDataSize);
				UpdateSubresources(uploadContext->GetCommandList(), bufferUpload->buffer->GetResource(), m_pUploadBuffer->GetResource(),
				gpuAddress - m_pUploadBuffer->GetResource()->GetGPUVirtualAddress() + bufferUploadHeapOffset, 0, 1, &subData);
				uploadContext->AddBarrier(*bufferUpload->buffer, D3D12_RESOURCE_STATE_COMMON, false);
				bufferUploadHeapOffset = AlignU32(bufferUploadHeapOffset + bufferUpload->bufferDataSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				uploadContext->AddBufferProcess(bufferUpload);
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

		if (numTextureUploads <=0) return;
		
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
				
				memcpy(cpuAddress + texUploadHeapOffset, textureUpload->pTextureData.get(), textureUpload->textureDataSize);
				uploadContext->CopyTextureRegion(*textureUpload->pTextureBuffer, m_pUploadBuffer->GetResource(), 
					gpuAddress - m_pUploadBuffer->GetResource()->GetGPUVirtualAddress() + texUploadHeapOffset,
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

	BufferHandle BufferManager::CreateVertexBuffer(const ElysiaModel::LoadedModel& model)
	{
		ElysiaCore::BufferCreationDesc bufferCreationDesc = 
		{
			.name = StringToWstring(model.name + " Vertex Buffer"),
			.stride = sizeof(ElysiaModel::MeshVertex),
			.size = model.vertices.size() * sizeof(ElysiaModel::MeshVertex),
			.viewFlags = GPUResourceFlags::None,
			.accessFlags = BufferAccessFlags::GPUOnly,
			.isRawAccess = true
		};
		auto bufferHandle = std::move(CreateBuffer(bufferCreationDesc));
		assert(bufferHandle);

		auto pBufferUpload = new DX12BufferUpload();
		pBufferUpload->buffer = bufferHandle;
		pBufferUpload->bufferDataSize = bufferCreationDesc.size;
		pBufferUpload->pBufferData = std::make_unique<uint8_t[]>(bufferCreationDesc.size);

		memcpy(pBufferUpload->pBufferData.get(), model.vertices.data(), bufferCreationDesc.size);
		m_pDevice->GetUploadContext()->AddBufferToUploads(std::move(pBufferUpload));

		return bufferHandle;
	}
	BufferHandle BufferManager::CreateIndexBuffer(const ElysiaModel::LoadedModel& model)
	{
		ElysiaCore::BufferCreationDesc bufferCreationDesc
		{
			.name = StringToWstring(model.name + " Index Buffer"),
			.stride = 0,
			.size = model.indices.size() * sizeof(UINT16),
			.viewFlags = GPUResourceFlags::None,
			.accessFlags = BufferAccessFlags::GPUOnly,
			.isRawAccess = true
		};
		auto bufferHandle = std::move(CreateBuffer(bufferCreationDesc));

		auto pBufferUpload = new DX12BufferUpload();
		pBufferUpload->buffer = bufferHandle;
		pBufferUpload->pBufferData = std::make_unique<uint8_t[]>(bufferCreationDesc.size);
		pBufferUpload->bufferDataSize = bufferCreationDesc.size;
		memcpy(pBufferUpload->pBufferData.get(), model.indices.data(), bufferCreationDesc.size);

		m_pDevice->GetUploadContext()->AddBufferToUploads(std::move(pBufferUpload));

		return bufferHandle;
	}
}