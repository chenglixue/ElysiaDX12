#include "stdafx.h"
#include "DX12ComputeContext.h"

#include "DX12Device.h"
#include "DX12RootSignature.h"
#include "DX12RenderPassDescriptorHeap.h"
#include "Runtime/RenderCore/RenderTexture.h"
#include "DX12PipelineState.h"
#include "ContextUtility.h"

namespace ElysiaCore
{
	DX12ComputeContext::DX12ComputeContext(DX12Device* pDevice) : 
		DX12Context(pDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE)
	{

	}

	DX12ComputeContext::~DX12ComputeContext()
	{

	}

	void DX12ComputeContext::SetPipeline(PipelineInfo& pipelineStateData)
	{
		assert(pipelineStateData.m_pipelineStateObject && 
			pipelineStateData.m_pipelineStateObject->m_pipelineType == PipelineType::Compute);

		m_pCurrentPipeline = pipelineStateData.m_pipelineStateObject;
		auto pipelineState = m_pCurrentPipeline->m_pipelineState.get();

		m_commandList->SetPipelineState(pipelineState->GetPipelineState());
		m_commandList->SetComputeRootSignature(pipelineState->GetRootSignature()->GetSignature());
	}

	void DX12ComputeContext::SetPipelineResource(uint8_t spaceID, PipelineResourceSpace* pipelineBindResourceSpace)
	{
		assert(m_pCurrentPipeline);
		assert(pipelineBindResourceSpace->IsLocked());

		auto SRVResources = pipelineBindResourceSpace->GetSRVs();
		auto CBVResource = pipelineBindResourceSpace->GetStaticCBV();

		static const uint32_t maxNumHandlesBinding = 16;
		const UINT numTableHandles = static_cast<UINT>(SRVResources.size());
		assert(numTableHandles <= maxNumHandlesBinding);

		static const uint32_t singleDescriptorRangeCopyArray[maxNumHandlesBinding]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1 };
		D3D12_CPU_DESCRIPTOR_HANDLE handles[maxNumHandlesBinding]{};
		UINT currentHandleIndex = 0;

		assert(numTableHandles <= maxNumHandlesBinding);

		if (CBVResource)
		{
			auto& rootParameterIndex = m_pCurrentPipeline->m_pipelineResourceMapping.m_CBVMappings[spaceID];
			assert(rootParameterIndex.has_value());

			m_commandList->SetComputeRootConstantBufferView(rootParameterIndex.value(), CBVResource->GetGPUAddress());
		}

		if (numTableHandles == 0)
		{
			return;
		}

		for (auto& SRV : SRVResources)
		{
			if (SRV->m_resource->GetBufferType() == GPUResourceType::Texture)
			{
				handles[currentHandleIndex++] = static_cast<DX12TextureResource*>(SRV->m_resource)->GetSRVDescriptor().GetCPUHandle();
			}
			else
			{
				handles[currentHandleIndex++] = static_cast<DX12BufferResource*>(SRV->m_resource)->GetSRVDescriptor().GetCPUHandle();
			}
		}
		DX12DescriptorHeapHandle blockStart = m_currSRVHeap->AllocateRenderPassDescriptorBlock(numTableHandles);
		m_device->CopyDescriptors(1, &blockStart.GetCPUHandle(), &numTableHandles, numTableHandles, handles, singleDescriptorRangeCopyArray, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		auto& tableMapping = m_pCurrentPipeline->m_pipelineResourceMapping.m_TableMappings[spaceID];
		assert(tableMapping.has_value());

		m_commandList->SetComputeRootDescriptorTable(tableMapping.value(), blockStart.GetGPUHandle());
	}

	void DX12ComputeContext::Dispatch(size_t groupCountX, size_t groupCountY, size_t groupCountZ)
	{
		m_commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
	}

	void DX12ComputeContext::Dispatch1D(size_t threadCountX, size_t groupSizeX)
	{
		Dispatch(GetGroupCount(threadCountX, groupSizeX), 1, 1);
	}

	void DX12ComputeContext::Dispatch2D(size_t threadCountX, size_t threadCountY, size_t groupSizeX, size_t groupSizeY)
	{
		Dispatch(GetGroupCount(threadCountX, groupSizeX), GetGroupCount(threadCountY, groupSizeY), 1);
	}

	void DX12ComputeContext::Dispatch3D(size_t threadCountX, size_t threadCountY, size_t threadCountZ, size_t groupSizeX, size_t groupSizeY, size_t groupSizeZ)
	{
		Dispatch(GetGroupCount(threadCountX, groupSizeX), GetGroupCount(threadCountY, groupSizeY), GetGroupCount(threadCountZ, groupSizeZ));
	}
}