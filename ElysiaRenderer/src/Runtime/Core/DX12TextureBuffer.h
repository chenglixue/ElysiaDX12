#pragma once
#include "DX12GPUResource.h"
#include "DX12DescriptorHeapHandle.h"
#include "DX12BufferResource.h"
#include "TextureUtility.h"

namespace ElysiaCore
{
	class DX12TextureResource : public DX12GPUResource
	{
	public:
		DX12TextureResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState);
		DX12TextureResource(CComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES usageState, CComPtr<D3D12MA::Allocation> allocation);
		DX12TextureResource(DX12TextureResource&& texResource) = default;
		DX12TextureResource(DX12TextureResource& a) = delete;
		DX12TextureResource& operator=(DX12TextureResource& a) = delete;
		DX12TextureResource(const DX12TextureResource& a) = delete;
		DX12TextureResource& operator=(const DX12TextureResource& a) = delete;
		~DX12TextureResource();

		DX12DescriptorHeapHandle GetRTVDescriptor() const
		{
			return m_RTVDescriptor;
		}
		DX12DescriptorHeapHandle GetDSVDescriptor() const
		{
			return m_DSVDescriptor;
		}
		DX12DescriptorHeapHandle GetSRVDescriptor() const
		{
			return m_SRVDescriptor;
		}
		DX12DescriptorHeapHandle GetUAVDescriptor() const
		{
			return m_UAVDescriptor;
		}

		void SetRTVDescriptor(const DX12DescriptorHeapHandle& handle)
		{
			m_RTVDescriptor = handle;
		}
		void SetSRVDescriptor(const DX12DescriptorHeapHandle& handle)
		{
			m_SRVDescriptor = handle;
		}
		void SetDSVDescriptor(const DX12DescriptorHeapHandle& handle)
		{
			m_DSVDescriptor = handle;
		}
		void SetUAVDescriptor(const DX12DescriptorHeapHandle& handle)
		{
			m_UAVDescriptor = handle;
		}

	private:
		DX12DescriptorHeapHandle m_RTVDescriptor{};
		DX12DescriptorHeapHandle m_DSVDescriptor{};
		DX12DescriptorHeapHandle m_SRVDescriptor{};
		DX12DescriptorHeapHandle m_UAVDescriptor{};
	};
}