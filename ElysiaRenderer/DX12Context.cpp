#include "DX12Context.h"
#include "DX12Device.h"

namespace ElysiaRenderer
{
	using namespace ElysiaHelper;
	class DX12Device;

	DX12Context::DX12Context(DX12Device* device, D3D12_COMMAND_LIST_TYPE commandType) :
		m_device(device), m_contextType(commandType)
	{
		for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		{
			AssertIfFailed(m_device->GetDevice()->
				CreateCommandAllocator(commandType, IID_PPV_ARGS(&m_commandAllocators[i])));
		}

		AssertIfFailed(m_device->GetDevice()->
			CreateCommandList1(0, commandType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList)));
	}
	DX12Context::~DX12Context()
	{
		ElysiaHelper::SafeRelease(m_commandList);

		for (int frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; ++frameIndex)
		{
			ElysiaHelper::SafeRelease(m_commandAllocators[frameIndex]);
		}

		/*for (int i = 0; i < m_bundleAllocators.size(); ++i)
		{
			ElysiaHelper::SafeRelease(m_bundleAllocators[i]);
		}*/
	}

	void DX12Context::Reset()
	{
		UINT currFrameID = m_device->GetFrameID();
		m_commandAllocators[currFrameID]->Reset();
		m_commandList->Reset(m_commandAllocators[currFrameID], nullptr);

		if (m_contextType != D3D12_COMMAND_LIST_TYPE_COPY)
		{
			BindDescriptorHeaps(currFrameID);
		}
	}
	void DX12Context::Reset(ID3D12PipelineState* pipelineState)
	{
		UINT currFrameID = m_device->GetFrameID();
		m_commandAllocators[currFrameID]->Reset();
		m_commandList->Reset(m_commandAllocators[currFrameID], pipelineState);

		if (m_contextType != D3D12_COMMAND_LIST_TYPE_COPY)
		{
			BindDescriptorHeaps(currFrameID);
		}
	}

	void DX12Context::AddBarrier(DX12GPUResource& resource, D3D12_RESOURCE_STATES newState)
	{
		if (m_numQueuedBarriers >= MAX_QUEUED_BARRIERS)
		{
			FlushBarrier();
		}

		D3D12_RESOURCE_STATES oldState = resource.GetUsageState();
		if (oldState != newState)
		{
			D3D12_RESOURCE_BARRIER& barrierDesc = m_resourceBarriers[m_numQueuedBarriers];
			m_numQueuedBarriers++;

			// Describes the transition of subresources between different usages
			barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrierDesc.Transition = {resource.GetResource(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, oldState, newState};

			resource.SetUsageState(newState);
		}
	}

	void DX12Context::FlushBarrier()
	{
		if (m_numQueuedBarriers > 0)
		{
			// synchronize multiple accesses to resources.
			m_commandList->ResourceBarrier(m_numQueuedBarriers, m_resourceBarriers.data());
			m_numQueuedBarriers = 0;
		}
	}

	void DX12Context::CopyTextureRegion(DX12GPUResource& dest, DX12GPUResource& source, size_t sourceOffset,
		SubResourceLayouts subResourceLayouts, UINT numSubResources)
	{
		for (UINT currSubResourceIndex = 0; currSubResourceIndex < numSubResources; ++currSubResourceIndex)
		{
			D3D12_TEXTURE_COPY_LOCATION destLocation{};
			destLocation.pResource = dest.GetResource();
			destLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			destLocation.SubresourceIndex = currSubResourceIndex;

			D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
			sourceLocation.pResource = source.GetResource();
			sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			sourceLocation.PlacedFootprint = subResourceLayouts[currSubResourceIndex];
			sourceLocation.PlacedFootprint.Offset += sourceOffset;

			m_commandList->CopyTextureRegion(&destLocation, 0, 0, 0, &sourceLocation, nullptr);
			auto tran = CD3DX12_RESOURCE_BARRIER::Transition(dest.GetResource(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			//m_commandList->ResourceBarrier(1, &tran);
			//AddBarrier(dest, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}


	/// <summary>
	/// bind needed SRV resource and sampler resource for root parameters
	/// </summary>
	/// <param name="frameIndex"></param>
	void DX12Context::BindDescriptorHeaps(UINT frameIndex)
	{
		m_currSRVHeap = &m_device->GetSRVHeap(frameIndex);
		m_currSRVHeap->Reset();

		ID3D12DescriptorHeap* heapsToBind[2];
		heapsToBind[0] = m_device->GetSRVHeap(frameIndex).GetDescriptorHeap();
		heapsToBind[1] = m_device->GetSamplerHeap().GetDescriptorHeap();
		
		m_commandList->SetDescriptorHeaps(2, heapsToBind);
	}

	/*ID3D12GraphicsCommandList4* DX12Context::CreateBundle()
	{
		ID3D12CommandAllocator* newBundleAllocator = nullptr;
		ElysiaHelper::AssertIfFailed(m_device->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE,
			IID_PPV_ARGS(&newBundleAllocator)));

		ID3D12GraphicsCommandList4* newBundle = nullptr;
		ElysiaHelper::AssertIfFailed(m_device->GetDevice()->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, D3D12_COMMAND_LIST_FLAG_NONE,
			IID_PPV_ARGS(&newBundle)));
		m_bundleAllocators.push_back(std::move(newBundleAllocator));
		m_bundles.push_back(std::move(newBundle));

		return m_bundles.back();
	}*/

}