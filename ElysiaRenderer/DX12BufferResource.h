#pragma once
#include "stdafx.h"
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"

namespace ElysiaRenderer
{
	struct DepthBufferCreateDesc
	{
		DepthBufferCreateDesc()
		{
			m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			m_resourceDesc.Width = 0;
			m_resourceDesc.Height = 0;
			m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			m_resourceDesc.DepthOrArraySize = 1;
			m_resourceDesc.MipLevels = 1;
			m_resourceDesc.SampleDesc.Count = 1;
			m_resourceDesc.SampleDesc.Quality = 0;
			m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			m_resourceDesc.Alignment = 0;
		}

		D3D12_RESOURCE_DESC m_resourceDesc{};
		GPUResourceFlags bufferTypeFlags;
	};

	struct BufferCreationDesc
	{
		LPCWSTR m_name;
		size_t m_size = 0;
		size_t m_stride = 0;
		GPUResourceFlags m_viewFlags = GPUResourceFlags::None;
		BufferAccessFlags m_accessFlags = BufferAccessFlags::GPUOnly;
		bool m_isRawAccess = false;
	};

	class DX12BufferResource : public DX12GPUResource
	{
	public:
		DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState)
			: DX12GPUResource(resource, usageState)
		{

		}
		DX12BufferResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState, CComPtr<D3D12MA::Allocation> allocation)
			: DX12GPUResource(resource, usageState)
		{
			m_bufferType = GPUResourceType::Buffer;

			m_allocation = allocation;
			m_resource = resource;
			m_GPUAddress = m_resource->GetGPUVirtualAddress();
		}

		~DX12BufferResource()
		{
			Destory();
		}

		float GetStride() const noexcept
		{
			return m_stride;
		}
		DX12DescriptorHeapHandle GetCBVDescriptor() const noexcept
		{
			return m_CBVDescriptor;
		}
		DX12DescriptorHeapHandle GetSRVDescriptor() const noexcept
		{
			return m_SRVDescriptor;
		}
		DX12DescriptorHeapHandle GetUAVDescriptor() const noexcept
		{
			return m_UAVDescriptor;
		}
		void* GetMappedBuffer() const noexcept
		{
			return m_mappedBuffer;
		}

		void SetStride(float stride)
		{
			m_stride = stride;
		}
		void SetCBVDescriptor(const DX12DescriptorHeapHandle& CBVDescriptor)
		{
			m_CBVDescriptor = CBVDescriptor;
		}
		void SetSRVDescriptor(const DX12DescriptorHeapHandle& SRVDescriptor)
		{
			m_SRVDescriptor = SRVDescriptor;
		}
		void SetUAVDescriptor(const DX12DescriptorHeapHandle& UAVDescriptor)
		{
			m_UAVDescriptor = UAVDescriptor;
		}
		void SetMappedData(const void* bufferData, size_t bufferSize)
		{
			assert(m_mappedBuffer != nullptr && bufferData != nullptr && bufferSize > 0 && m_resourceDesc.Width < bufferSize);
			memcpy_s(m_mappedBuffer, m_resourceDesc.Width, bufferData, bufferSize);
		}

	private:
		size_t m_stride = 0;
		void* m_mappedBuffer = nullptr;
		DX12DescriptorHeapHandle m_CBVDescriptor{};
		DX12DescriptorHeapHandle m_SRVDescriptor{};
		DX12DescriptorHeapHandle m_UAVDescriptor{};
	};
}
