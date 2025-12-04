#include "stdafx.h"

#include "PipelineResourceSpace.h"
#include "../DX12/DX12BufferResource.h"
#include "../DX12/DX12TextureBuffer.h"


namespace ElysiaRenderer
{
	UINT PipelineResourceSpace::GetIndexOfBindingIndex(const std::vector<PipelineResourceBinding*>& bindResources, UINT bindingIndex)
	{
		const UINT numBinds = static_cast<UINT>(bindResources.size());
		for (UINT currBindIndex = 0; currBindIndex < numBinds; ++currBindIndex)
		{
			if (bindResources.at(currBindIndex)->m_bindingIndex == bindingIndex)
			{
				return currBindIndex;
			}
		}

		return UINT_MAX;
	}
	
	void PipelineResourceSpace::ExpectCBV(UINT registerIndex)
	{
		m_expectedBindings[registerIndex] = ResourceType::CBV;
	}
	void PipelineResourceSpace::ExpectSRV(UINT registerIndex)
	{
		m_expectedBindings[registerIndex] = ResourceType::SRV;
	}
	void PipelineResourceSpace::ExpectUAV(UINT registerIndex)
	{
		m_expectedBindings[registerIndex] = ResourceType::UAV;
	}
	bool PipelineResourceSpace::HasExpectedCBV() const
	{
		return std::any_of(m_expectedBindings.begin(), m_expectedBindings.end(),
			[](const auto& kv) { return kv.second == ResourceType::CBV; });
	}
	bool PipelineResourceSpace::HasDynamicCBV() const noexcept
	{
		return m_hasDynamicCBV;
	}

	DX12BufferResource* PipelineResourceSpace::GetStaticCBV() const
	{
		return m_pStaticCBV;
	}
	D3D12_GPU_VIRTUAL_ADDRESS PipelineResourceSpace::GetDynamicCBV() const
	{
		return m_dynamicCBVAddress;
	}
	std::vector<PipelineResourceBinding*>& PipelineResourceSpace::GetSRVs()
	{
		return m_SRVs;
	}
	std::vector<PipelineResourceBinding*>& PipelineResourceSpace::GetUAVs()
	{
		return m_UAVs;
	}

	void PipelineResourceSpace::SetStaticCBV(DX12BufferResource* CBVResource)
	{
		if (m_isLocked)
		{
			if (CBVResource == nullptr)
			{
				ElysiaHelper::AssertError("Setting unused binding in a locked resource space");
			}
			else
			{
				m_pStaticCBV = CBVResource;
			}
		}
		else
		{
			m_pStaticCBV = CBVResource;
		}
	}
	void PipelineResourceSpace::SetDynamicCBV(D3D12_GPU_VIRTUAL_ADDRESS GPUVA)
	{
		m_dynamicCBVAddress = GPUVA;
		m_hasDynamicCBV = false;
	}
	void PipelineResourceSpace::SetSRV(PipelineResourceBinding* SRVResource)
	{
		UINT currIndex = GetIndexOfBindingIndex(m_SRVs, SRVResource->m_bindingIndex);

		if (m_isLocked)
		{
			if (currIndex == UINT_MAX)
			{
				ElysiaHelper::AssertError("Setting unused binding in a locked resource space");
			}
			else
			{
				m_SRVs[currIndex] = SRVResource;
			}
		}
		else
		{
			if (currIndex == UINT_MAX)
			{
				m_SRVs.emplace_back(SRVResource);

				std::sort(m_SRVs.begin(), m_SRVs.end(), &SortPipelineBindings);
			}
			else
			{
				m_SRVs[currIndex] = SRVResource;
			}
		}
	}
	void PipelineResourceSpace::SetUAV(PipelineResourceBinding* UAVResource)
	{
		UINT currIndex = GetIndexOfBindingIndex(m_UAVs, UAVResource->m_bindingIndex);

		if (m_isLocked)
		{
			if (currIndex == UINT_MAX)
			{
				ElysiaHelper::AssertError("Setting unused binding in a locked resource space");
			}
			else
			{
				m_UAVs[currIndex] = UAVResource;
			}
		}
		else
		{
			if (currIndex == UINT_MAX)
			{
				m_UAVs.emplace_back(UAVResource);

				std::sort(m_UAVs.begin(), m_UAVs.end(), &SortPipelineBindings);
			}
			else
			{
				m_UAVs[currIndex] = UAVResource;
			}
		}
	}

	void PipelineResourceSpace::Lock()
	{
		m_isLocked = true;
	}
	bool PipelineResourceSpace::IsLocked() const
	{
		return m_isLocked;
	}
}