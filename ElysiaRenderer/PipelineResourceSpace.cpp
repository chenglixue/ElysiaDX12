#include "stdafx.h"

#include "PipelineResourceSpace.h"
#include "DX12BufferResource.h"
#include "DX12TextureBuffer.h"


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

	DX12BufferResource* PipelineResourceSpace::GetCBV()
	{
		return m_CBV;
	}
	std::vector<PipelineResourceBinding*>& PipelineResourceSpace::GetSRVs()
	{
		return m_SRVs;
	}
	std::vector<PipelineResourceBinding*>& PipelineResourceSpace::GetUAVs()
	{
		return m_UAVs;
	}


	void PipelineResourceSpace::SetCBV(DX12BufferResource* CBVResource)
	{
		if (m_isLocked)
		{
			if (CBVResource == nullptr)
			{
				ElysiaHelper::AssertError("Setting unused binding in a locked resource space");
			}
			else
			{
				m_CBV = CBVResource;
			}
		}
		else
		{
			m_CBV = CBVResource;
		}
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