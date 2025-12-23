#pragma once
#include "DX12GPUResource.h"
#include "PipelineResourceSpace.h"
#include "Programs/Helper.h"

namespace ElysiaCore
{
	using namespace ElysiaHelper;

	class ElysiaCore::DX12GPUResource;
	class ElysiaCore::PipelineResourceSpace;

	struct PipelineResourceMapping
	{
		// space id : root paramter index
		std::array<std::optional<UINT>, NUM_RESOURCE_SPACES> m_CBVMappings{};
		std::array<std::optional<UINT>, NUM_RESOURCE_SPACES> m_TableMappings{};
	};

	struct PipelineResourceBinding
	{
		UINT m_bindingIndex = 0;
		DX12GPUResource* m_resource = nullptr;
	};

	struct PipelineResourceLayout
	{
		std::array<PipelineResourceSpace*, NUM_RESOURCE_SPACES> m_spaces{  };
		
		void SetSpace(UINT8 spaceID, PipelineResourceSpace* pSpace)
		{
			assert(spaceID < NUM_RESOURCE_SPACES && pSpace);
			
			m_spaces[spaceID] = pSpace;
		}
		
		bool IsValidSpace(size_t spaceID)
		{
			assert(spaceID < NUM_RESOURCE_SPACES);
			return m_spaces[spaceID];
		}
	};

	inline bool SortPipelineBindings(PipelineResourceBinding* a, PipelineResourceBinding* b)
	{
		return (*a).m_bindingIndex < (*b).m_bindingIndex;
	}
}