#include "stdafx.h"

#include "DX12DescriptorHeap.h"

namespace ElysiaCore
{
	DX12DescriptorHeap::DX12DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor,
		bool isReferenceShader)
	{
		m_heapType = heapType;
		m_maxDescriptors = numDescriptor;
		m_isReferenceShader = isReferenceShader;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.Type = heapType;
		heapDesc.NumDescriptors = numDescriptor;
		heapDesc.Flags = isReferenceShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		ElysiaHelper::ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

		m_descriptorHeapCPUStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

		if (isReferenceShader)
		{
			m_descriptorHeapGPUStart = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		m_descriptorSize = device->GetDescriptorHandleIncrementSize(heapType);
	}
	DX12DescriptorHeap::~DX12DescriptorHeap()
	{
		ElysiaHelper::SafeRelease(m_descriptorHeap);
	}
}