#include "PipelineResourceSpace.h"

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
}